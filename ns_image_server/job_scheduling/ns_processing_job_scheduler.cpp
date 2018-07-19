#include "ns_processing_job_scheduler.h"
#ifndef NS_ONLY_IMAGE_ACQUISITION
#include "ns_image_processing_pipeline.h"
#endif
#include "ns_processing_job_processor.h"

using namespace std;


struct ns_image_processing_pipline_holder{
public:
#ifndef NS_ONLY_IMAGE_ACQUISITION
	ns_image_processing_pipline_holder(const unsigned long pipeline_chunk_size):pipeline(pipeline_chunk_size){}

	ns_image_processing_pipeline pipeline;
	#endif
};

std::string ns_maintenance_task_to_string(const ns_maintenance_task & task){
	switch(task){
		case ns_maintenance_rebuild_sample_time_table:
			return "Rebuild sample times table";
		case ns_maintenance_rebuild_worm_movement_table:
			return "Rebuild worm movement table";
		case ns_maintenance_update_processing_job_queue:
			return "Add new jobs to queue";
		case ns_maintenance_rebuild_movement_data:
			return "Build Movement Data";
		case ns_maintenance_rebuild_movement_from_stored_image_quantification:
			return "Rebuild Movement Data From Stored Image Quantification";
		case ns_maintenance_rebuild_movement_from_stored_images:
			return "Rebuild Movement Data From Stored Images";
		case ns_maintenance_generate_movement_posture_visualization:
			return "Generate Movement Posture Visualization";
		case ns_maintenance_recalculate_censoring:
			return "Recalculate Censoring";
		case ns_maintenance_generate_movement_posture_aligned_visualization:
			return "Generate Movement Posture Aligned Visualization";
		case ns_maintenance_generate_sample_regions_from_mask:
			return "Generate Sample Regions from Mask";
		case ns_maintenance_delete_images_from_database:
			return "Delete Images from the Database";
		case ns_maintenance_delete_files_from_disk_request:
			return "Submit Request for files to be delelted from disk";
		case ns_maintenance_delete_files_from_disk_action:
			return "Delete files from disk";
		case ns_maintenance_check_for_file_errors:
			return "Check for missing or broken files";
		case ns_maintenance_determine_disk_usage:
			return "Determine disk usage for files";
		case ns_maintenance_generate_animal_storyboard:
			return "Generate Animal Storyboard";
		case ns_maintenance_generate_animal_storyboard_subimage:
			return "Process Animal Storyboard Sub-Image";
		case ns_maintenance_compress_stored_images:
			return "Compress Stored Images";
		case ns_maintenance_generate_subregion_mask:
			return "Generate Subregion Mask";
		case ns_maintenance_rerun_image_registration:
			return "Re-Run Image Registration";
		case ns_maintenance_recalc_image_stats:
			return "Recalculate Image Statistics";
		case ns_maintenance_recalc_worm_morphology_statistics:
			return "Compile worm morphology statistics";
		case ns_maintenance_last_task: throw ns_ex("ns_maintenance_task_to_string::last_task does not have a std::string representation");
		default:
			throw ns_ex("ns_maintenance_task_to_string::Unknown Maintenance task");
	}
}

bool ns_processing_job_scheduler::run_a_job(ns_processing_job & job,ns_sql & sql){
	//if we can't talk to the long term storage we're bound to fail, so don't try.

	ns_image_server_push_job_scheduler push_scheduler;

	//refresh flag labels from db
	ns_death_time_annotation_flag::get_flags_from_db(sql);

	if (job.maintenance_task == ns_maintenance_update_processing_job_queue){
		image_server.register_server_event(ns_image_server_event("Updating job queue"),&sql);
		image_server.update_processing_status("Updating Queue", 0, 0, &sql);
		push_scheduler.report_job_as_finished(job,sql);
		push_scheduler.discover_new_jobs(sql);
		sql << "DELETE from processing_jobs WHERE id = " << job.id;
		sql.send_query();
		sql << "DELETE from processing_job_queue WHERE id = " << job.queue_entry_id;
		sql.send_query();
		return true;
	}

	ns_acquire_for_scope<ns_processing_job_processor> processor(
		#ifndef NS_ONLY_IMAGE_ACQUISITION
		ns_processing_job_processor_factory::generate(job,image_server,&this->pipeline->pipeline));
		#else
		ns_processing_job_processor_factory::generate(job,image_server,0));
		#endif
	try{
		std::string rejection_reason;
		if (!processor().job_is_still_relevant(sql,rejection_reason)){
			image_server.register_server_event(ns_image_server_event("Encountered a processing job queue that had already been performed or invalidated: ") << rejection_reason << "[" << job.description() << "]",&sql);
			push_scheduler.report_job_as_finished(job,sql);
			if (processor().delete_job_after_processing())
				processor().delete_job(sql);
			processor.release();
			sql.send_query("COMMIT");
			return true;
		}
		if(idle_timer_running)
			image_server.register_job_duration(ns_performance_statistics_analyzer::ns_idle,idle_timer.stop());
		idle_timer_running = false;
		ns_high_precision_timer tp;
		tp.start();

		//mark the subject as busy to prevent multiple jobs running simultaneously on the same data
		processor().mark_subject_as_busy(true,sql);
		sql.send_query("COMMIT");

		//update UI to show job is being performed, if requested.
		if (processor().flag_job_as_being_processed_before_processing())
			processor().flag_job_as_being_processed(sql);
		sql.send_query("COMMIT");

		image_server.update_processing_status("Processing Job", job.id,job.queue_entry_id, &sql);

		ns_64_bit problem_id;
		try {
			problem_id = processor().run_job(sql);
		}
		catch (ns_ex & e) {
			ns_ex ex("Processing job did not handle its own exception: ");
			ex << e.text();
			throw e;
		}
		if (problem_id != 0)
			processor().mark_subject_as_problem(problem_id, sql);

		processor().mark_subject_as_busy(false,sql);

		if (problem_id == 0)
			push_scheduler.report_job_as_finished(job,sql);

		sql.send_query("COMMIT");

		if (problem_id == 0) {
			processor().handle_concequences_of_job_completion(sql);

			if (processor().delete_job_after_processing())
				processor().delete_job(sql);
			sql.send_query("COMMIT");

			image_server.update_processing_status("Finished Job", job.id, job.queue_entry_id, &sql);
		}

		processor.release();
		if (problem_id == 0)
			image_server.register_job_duration(ns_performance_statistics_analyzer::ns_running_a_job,tp.stop());


		#ifndef NS_ONLY_IMAGE_ACQUISITION
		//don't let old, unused data accumulate.
		ns_image_fast_registration_profile_cache::external_source_type source;
		source.image_storage = &image_server.image_storage;
		source.sql = &sql;
		image_server.image_registration_profile_cache.remove_old_images(10 * 60, source);

		if (job.maintenance_task == ns_maintenance_generate_animal_storyboard ||
			job.maintenance_task == ns_maintenance_generate_animal_storyboard_subimage)
			image_server.clean_up_storyboard_cache(false, sql);
		else
			image_server.clean_up_storyboard_cache(true, sql);
		#endif
		idle_timer_running = true;
		idle_timer.start();

		return true;
	}
	catch(ns_ex & ex){
		//we have found an error, handle it by registering it in the
		//host_event log, and annotate the current job (and any associated images)
		//with a reference to the error that occurred.
		sql.clear_query();
		image_server.update_processing_status("Handling Error", job.id, job.queue_entry_id, &sql);

		processor().mark_subject_as_busy(false,sql);


		ns_64_bit error_id(push_scheduler.report_job_as_problem(job,ex,sql));
		sql.send_query("COMMIT");



		//there are a variety of problems that could cause an exception to be thrown.
		//only mark the image itself as problematic if the error doesn't come from
		//any of the environmental problems that can crop up.
		bool problem_with_long_term_storage(!image_server.image_storage.long_term_storage_was_recently_writeable());
		if (!problem_with_long_term_storage &&
			ex.type() != ns_network_io &&
			ex.type() != ns_sql_fatal &&
			ex.type() != ns_memory_allocation &&
			ex.type() != ns_cache)
			processor().mark_subject_as_problem(error_id,sql);

		//image_server.performance_statistics.cancel_outstanding_jobs();
		if (ex.type() == ns_memory_allocation)
			throw;	//memory allocation errors can cause big, long-term problems, thus we need to pass
					//them downwards to be handled.
		else
		processor.release();
	}
	catch(std::exception & e){
		ns_ex ex(e);

		sql.clear_query();
		image_server.update_processing_status("Handling Error", job.id, job.queue_entry_id, &sql);

		processor().mark_subject_as_busy(false,sql);
		//we have found an error, handle it by registering it in the
		//host_event log, and annotate the current job (and any associated images)
		//with a reference to the error that occurred.
		ns_64_bit error_id(push_scheduler.report_job_as_problem(job,ex,sql));
		sql.send_query("COMMIT");


		processor().mark_subject_as_problem(error_id,sql);

	//	image_server.performance_statistics.cancel_outstanding_jobs();

		processor.release();
		if (ex.type() == ns_memory_allocation)
			throw; //memory allocation errors can cause big, long-term problems, thus we need to pass
					//them downwards to be handled.
	}
	return true;
}

void ns_processing_job_scheduler::clear_heap(){
	#ifndef NS_ONLY_IMAGE_ACQUISITION
	pipeline->pipeline.clear_heap();
	#endif
	}

void ns_processing_job_scheduler::init_pipeline(){

	#ifndef NS_ONLY_IMAGE_ACQUISITION
	pipeline = new ns_image_processing_pipline_holder(ns_pipeline_chunk_size);  //note that, if openjpeg2000 is being used, the chunk size sets the tile height of the openjpeg2000 images.
	#else
	pipeline = 0;
	#endif
}
void ns_processing_job_scheduler::destruct_pipeline(){
	if (pipeline != 0){
		delete pipeline;
		pipeline = 0;
	}
}

struct ns_get_file_deletion_arguments{
	//delete captured images whose small image has been calculated and mask applied
	static std::string captured_file_specification(const std::string & table_name){
			return table_name + ".image_id != 0 AND "
					+ table_name + ".small_image_id != 0 AND "
					+ table_name + ".mask_applied != 0 AND " + table_name + ".never_delete_image = 0 AND " + table_name + ".problem = 0 AND " + table_name + ".currently_being_processed=0 ";
	}
	//delete small captured images where the original large copy still eixsts.
	static std::string small_captured_file_specification(const std::string & table_name){
			return table_name + ".small_image_id != 0 AND "
				   + table_name + ".image_id != 0 AND " + table_name + ".problem = 0 AND " + table_name + ".currently_being_processed=0 ";
	}
	//delete captured images that have been censored
	static std::string censored_file_specification(const std::string & table_name){
			return table_name + ".censored!=0 AND " + table_name + ".never_delete_image = 0 AND " + table_name + ".problem = 0 AND " + table_name + ".currently_being_processed=0 ";
	}
};

void ns_identify_region_files_to_delete(const ns_64_bit & region_id,const bool entire_region, const std::vector<char> & operations, ns_sql & sql,std::vector<ns_file_location_specification> & files){

	//if we are deleting the raw data, delete the entire region including all processed images
	if (entire_region) {
		files.push_back(image_server_const.image_storage.get_base_path_for_region(region_id, &sql));
		return;
	}
	//if we're deleting only certain image processing tasks, just do that.
	for (unsigned long i = 0; i < (unsigned long)operations.size(); i++) {
		if (operations[i])
			files.push_back(image_server_const.image_storage.get_path_for_region(region_id, &sql, (ns_processing_task)i));
	}
	//delete metadata associated with that region
	if (operations[(int)ns_process_worm_detection]) {
		if (!operations[(int)ns_process_region_vis])
			files.push_back(image_server_const.image_storage.get_path_for_region(region_id, &sql, ns_process_region_vis));
		if (!operations[(int)ns_process_region_interpolation_vis])
			files.push_back(image_server_const.image_storage.get_path_for_region(region_id, &sql, ns_process_region_interpolation_vis));
		files.push_back(image_server_const.image_storage.get_file_specification_for_path_data(image_server_const.image_storage.get_base_path_for_region(region_id, &sql)));
		files.push_back(image_server_const.image_storage.get_file_specification_for_movement_data(region_id, "time_path_movement_image_analysis_quantification.csv", &sql));
		files.push_back(image_server_const.image_storage.get_file_specification_for_movement_data(region_id, "time_path_solution_data.csv", &sql));
		files.push_back(image_server_const.image_storage.get_detection_data_path_for_region(region_id, &sql));

		ns_image_server_results_subject spec;
		spec.region_id = region_id;

		ns_image_server_results_file f = image_server_const.results_storage.time_path_image_analysis_quantification(spec, "detailed", false, sql, false, false);
		files.push_back(image_server_const.image_storage.convert_results_file_to_location(&f));
		f = image_server_const.results_storage.time_path_image_analysis_quantification(spec, "detailed", false, sql, false, true);
		files.push_back(image_server_const.image_storage.convert_results_file_to_location(&f));
	}
}

void ns_get_experiment_cleanup_subjects(const ns_64_bit experiment_id, ns_sql_result & regions, std::vector<char> & operations, ns_sql & sql) {
/*	sql << "SELECT s.id from capture_samples as s WHERE s.experiment_id = " << experiment_id;
	ns_sql_result samples;
	sql.get_rows(samples);*/
	sql << "SELECT r.id from capture_samples as s, sample_region_image_info as r WHERE s.experiment_id = " << experiment_id << " AND r.sample_id = s.id";
	sql.get_rows(regions);
	operations.resize(0);
	operations.resize((int)ns_process_last_task_marker, 0);
	for (unsigned int i = 0; i < (int)ns_process_last_task_marker; i++) {
		if (i != (int)ns_unprocessed &&
			i != (int)ns_process_apply_mask &&
			i != (int)ns_process_thumbnail &&
			i != (int)ns_process_add_to_training_set &&
			i != (int)ns_process_analyze_mask &&
			i != (int)ns_process_compile_video &&
			i != (int)ns_process_heat_map &&
			i != (int)ns_process_static_mask)
			operations[i] = 1;
	}
}


void ns_handle_image_metadata_delete_action(ns_processing_job & job,ns_sql & sql){

	//we only want one node submitting deletion requests, because they are long queries
	//and it's possible to crash a standard mysql sever by submitting too many simultaneously
	int number_of_cycles(0);
	while (true) {
		ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("constants", &sql, true, __FILE__, __LINE__));
		std::string lock_holder_time_string(image_server.get_cluster_constant_value("image_metadata_deletion_lock", ns_to_string(0), &sql));
		
		std::string current_time_string;
		sql << "SELECT UNIX_TIMESTAMP(NOW())";
		current_time_string = sql.get_value();

		const unsigned long lock_holder_time(atol(lock_holder_time_string.c_str()));
		const unsigned long current_time(atol(current_time_string.c_str()));

		//if nobody has the lock, or the lock has expired after 5 minutes
		const bool expired_lock(lock_holder_time!= 0 && (current_time > lock_holder_time && current_time - lock_holder_time > 10 * 60));
		const bool invalid_lock((current_time < lock_holder_time && lock_holder_time - current_time > 10 * 60));
		if (lock_holder_time == 0 || expired_lock || invalid_lock) {
			image_server.set_cluster_constant_value("image_metadata_deletion_lock", ns_to_string(current_time), &sql);
			lock.release(__FILE__, __LINE__);
			if (expired_lock)
				image_server.add_subtext_to_current_event("Seizing an expired lock.", &sql);
			if (invalid_lock)
				image_server.add_subtext_to_current_event("Invalidating a lock apparently claimed in the future.", &sql);
			break;
		}
		else {
			lock.release(__FILE__, __LINE__);
			if (number_of_cycles==0)
				image_server.add_subtext_to_current_event("Waiting for another node to finish deletion task...",&sql);
			else image_server.add_subtext_to_current_event(".", &sql);
			number_of_cycles++;
			ns_thread::sleep(image_server_const.dispatcher_refresh_interval());
		}
	}
	try {
		if (number_of_cycles>0)
			image_server.add_subtext_to_current_event("Running.\n", &sql);
		vector<ns_64_bit> regions_to_delete;
		vector<ns_64_bit> samples_to_delete;
		ns_64_bit experiment_to_delete(0);
		bool autocommit_state = sql.autocommit_state();
		sql.set_autocommit(false);
		sql.send_query("BEGIN");
		std::vector<char> operations;
		operations.reserve(job.operations.size());
		operations.insert(operations.end(), job.operations.begin(), job.operations.end());
		try {
			if (job.image_id != 0 &&		//older versions of the software do not respect the "ns_delete_everything_but_raw_data" flag.
											//to avoid the situation where old versions will delete all data for an experiment,
											//jobs with this flag set also have image_id set to an arbitrary value
											//which will cause older versions of the software to simply try to delete that single image
											//rather than an entire experiment.
											//So here, we check that experiment_id = 0, to identify legit image deletion tasks
				job.experiment_id == 0) {
				sql << "DELETE FROM images WHERE id = " << job.image_id;
				sql.send_query();
			}
			else if (job.region_id != 0)
				regions_to_delete.push_back(job.region_id);
			else if (job.sample_id != 0) {
				if (operations[ns_unprocessed]) {
					if (job.maintenance_flag == ns_processing_job::ns_only_delete_processed_captured_images) {
						sql << "DELETE images FROM images, captured_images WHERE captured_images.sample_id = " << job.sample_id
							<< " AND captured_images.image_id = images.id "
							<< " AND " << ns_get_file_deletion_arguments::captured_file_specification("captured_images");
						sql.send_query();
						sql << "UPDATE captured_images SET image_id = 0 WHERE sample_id=" << job.sample_id
							<< " AND " << ns_get_file_deletion_arguments::captured_file_specification("captured_images");
						sql.send_query();
					}
					else if (job.maintenance_flag == ns_processing_job::ns_delete_censored_images) {
						sql << "DELETE images FROM images, captured_images WHERE captured_images.sample_id = " << job.sample_id
							<< " AND captured_images.image_id = images.id "
							<< " AND " << ns_get_file_deletion_arguments::censored_file_specification("captured_images");
						sql.send_query();
						sql << "UPDATE captured_images SET image_id = 0 WHERE sample_id=" << job.sample_id
							<< " AND " << ns_get_file_deletion_arguments::censored_file_specification("captured_images");
						sql.send_query();
					}
					else if (operations[ns_process_thumbnail]) {
						sql << "DELETE images FROM images, captured_images WHERE captured_images.sample_id = " << job.sample_id
							<< " AND captured_images.small_image_id = images.id "
							<< " AND " << ns_get_file_deletion_arguments::small_captured_file_specification("captured_images");
						sql.send_query();
						sql << "UPDATE captured_images SET small_image_id = 0 WHERE sample_id=" << job.sample_id
							<< " AND " << ns_get_file_deletion_arguments::small_captured_file_specification("captured_images");
						sql.send_query();
					}
					else throw ns_ex("Requesting to delete unprocessed sample images with no flag specified");
				}
				else {
					for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++)
						if (operations[task]) throw ns_ex("Full sample image deletion specified with operations flagged!");
					samples_to_delete.push_back(job.sample_id);
					//check for malformed request
				}
			}
			else if (job.experiment_id != 0) {
				//delete all data!
				if (job.maintenance_flag != ns_processing_job::ns_delete_everything_but_raw_data) {
					experiment_to_delete = job.experiment_id;
					sql << "SELECT id FROM capture_samples WHERE experiment_id = " << experiment_to_delete;
					ns_sql_result res;
					sql.get_rows(res);
					for (unsigned long i = 0; i < res.size(); i++)
						samples_to_delete.push_back(ns_atoi64(res[i][0].c_str()));
				}
				//delete everything but unprocessed data
				else {
					ns_sql_result regions;
					//note that this /changes/ the options specificied to those required of a cleanup.
					//so that later in this function we'll delete files according to those options.
					ns_get_experiment_cleanup_subjects(job.experiment_id, regions, operations, sql);
					for (unsigned long i = 0; i < regions.size(); i++)
						regions_to_delete.push_back(ns_atoi64(regions[i][0].c_str()));
				}
			}
			for (unsigned long i = 0; i < samples_to_delete.size(); i++) {
				sql << "SELECT id FROM sample_region_image_info WHERE sample_id = " << samples_to_delete[i];
				ns_sql_result res;
				sql.get_rows(res);
				for (unsigned int i = 0; i < res.size(); i++)
					regions_to_delete.push_back(ns_atoi64(res[i][0].c_str()));
			}
			for (unsigned long i = 0; i < regions_to_delete.size(); i++) {
				//unless the job is flagged as deleting the entire sample region, just delete individual images
				if (job.maintenance_flag != ns_processing_job::ns_delete_entire_sample_region) {
					for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++) {
						if (!operations[task]) continue;
						const string db_table(ns_processing_step_db_table_name(task));
						//if the data is processed for each time point in the image, delete each one
						if (db_table == "sample_region_images") {
							sql << "DELETE images FROM images,sample_region_images WHERE sample_region_images.region_info_id = " << regions_to_delete[i] <<
								" AND images.id = sample_region_images." << ns_processing_step_db_column_name(task);
							//	cerr << sql.query() << "\n";
							sql.send_query();
							sql << "UPDATE sample_region_images SET " << ns_processing_step_db_column_name(task) << " = 0 WHERE region_info_id = " << regions_to_delete[i];
							sql.send_query();
						}
						//if the data is processed once for the entire region, delete it.
						else {
							sql << "DELETE images FROM images, sample_region_image_info WHERE sample_region_image_info.id = " << regions_to_delete[i] <<
								" AND images.id = sample_region_image_info." << ns_processing_step_db_column_name(task);
							sql.send_query();
							sql << "UPDATE sample_region_image_info SET " << ns_processing_step_db_column_name(task) << " = 0 WHERE id = " << regions_to_delete[i];
							sql.send_query();
						}
					}
				}
				//if the job is flagged to delete the entire region, do it.
				else {
					//check for malformed request
					for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++)
						if (operations[task]) throw ns_ex("Full region image deletion specified with operations flagged!");

					//delete processed images
					for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++) {
						const string db_table(ns_processing_step_db_table_name(task));
						//if the data is processed for each time point in the image, delete each one
						if (db_table == "sample_region_images") {
							sql << "DELETE images FROM images,sample_region_images WHERE sample_region_images.region_info_id = " << regions_to_delete[i] <<
								" AND images.id = sample_region_images." << ns_processing_step_db_column_name(task);
							//	cerr << sql.query() << "\n";
							sql.send_query();
						}
						//if the data is processed once for the entire region, delete it.
						else {
							sql << "DELETE images FROM images, sample_region_image_info WHERE sample_region_image_info.id = " << regions_to_delete[i] <<
								" AND images.id = sample_region_image_info." << ns_processing_step_db_column_name(task);
							sql.send_query();
						}
					}
					//delete associated movement data
					sql << "DELETE worm_detection_results FROM worm_detection_results, sample_region_images WHERE "
						<< "sample_region_images.region_info_id = " << regions_to_delete[i]
						<< " AND worm_detection_results_id = worm_detection_results.id";
					sql.send_query();
					sql << "DELETE worm_detection_results FROM worm_detection_results, sample_region_images WHERE "
						<< "sample_region_images.region_info_id = " << regions_to_delete[i]
						<< " AND worm_interpolation_results_id = worm_detection_results.id";
					sql.send_query();
					sql << "DELETE worm_movement FROM worm_movement,sample_region_images WHERE "
						<< "sample_region_images.region_info_id = " << regions_to_delete[i]
						<< " AND worm_movement.id = sample_region_images.worm_movement_id";
					sql.send_query();

					sql << "DELETE i FROM image_statistics as i,sample_region_images as r "
						<< "WHERE r.region_info_id = " << regions_to_delete[i] << " AND r.image_statistics_id = i.id";
					sql.send_query();
					//delete movement info
					sql << "DELETE FROM worm_movement WHERE region_info_id = " << regions_to_delete[i];
					sql.send_query();
					//delete time points
					sql << "DELETE from sample_region_images WHERE region_info_id = " << regions_to_delete[i];
					sql.send_query();
					sql << "DELETE FROM sample_region_image_info WHERE id = " << regions_to_delete[i];
					sql.send_query();
				}
			}
			for (unsigned long i = 0; i < samples_to_delete.size(); i++) {
				//delete masks associated with each sample
				sql << "SELECT mask_id FROM capture_samples WHERE id = " << samples_to_delete[i];
				ns_sql_result res;
				sql.get_rows(res);
				for (unsigned long j = 0; j < res.size(); j++) {
					sql << "DELETE images FROM images, image_masks WHERE image_masks.id = " << res[j][0]
						<< " AND images.id = image_masks.image_id";
					sql.send_query();
					sql << "DELETE FROM image_mask_regions WHERE mask_id = " << res[j][0];
					sql.send_query();
					sql << "DELETE FROM image_masks WHERE id = " << res[j][0];
					sql.send_query();
				}

				sql << "DELETE i FROM image_statistics as i,captured_images as c "
					<< "WHERE c.sample_id = " << samples_to_delete[i] << " AND c.image_statistics_id = i.id";
				sql.send_query();


				//	//delete sample time relationships
				//	sql << "DELETE FROM sample_time_relationships WHERE sample_id = " << samples_to_delete[i];
				//	sql.send_query();
				sql << "DELETE images FROM images, captured_images WHERE captured_images.sample_id = " << samples_to_delete[i]
					<< " AND captured_images.image_id = images.id";
				sql.send_query();
				sql << "DELETE FROM captured_images WHERE sample_id = " << samples_to_delete[i];
				sql.send_query();
				sql << "DELETE FROM capture_schedule WHERE sample_id = " << samples_to_delete[i];
				sql.send_query();
				sql << "DELETE FROM capture_samples WHERE id = " << samples_to_delete[i];
				sql.send_query();
			}
			if (experiment_to_delete != 0) {
				sql << "DELETE FROM experiments WHERE id = " << experiment_to_delete;
				sql.send_query();
			}
			//	throw ns_ex("YOIKS!");
			sql.send_query("COMMIT");
		}
		catch (...) {
			sql.clear_query();
			sql.send_query("ROLLBACK");
			sql.set_autocommit(autocommit_state);
			throw;
		}
		sql.set_autocommit(autocommit_state);
	}
	catch (...) {
		image_server.set_cluster_constant_value("image_metadata_deletion_lock", "0", &sql);
		throw;
	}
	image_server.set_cluster_constant_value("image_metadata_deletion_lock", "0", &sql);
}


void ns_handle_file_delete_request(ns_processing_job & job, ns_sql & sql){
	vector<ns_file_location_specification> files;

	if (job.image_id != 0 &&					//older versions of the software do not respect the "ns_delete_everything_but_raw_data" flag.
													//to avoid the situation where old versions will delete all data for an experiment,
													//jobs with this flag set also have image_id set to an arbitrary value
													//which will cause older versions of the software to simply try to delete that single image
													//rather than an entire experiment.
													//So here, we check that experiment_id = 0, to identify legit image deletion tasks
		job.experiment_id == 0) {
		ns_image_server_image im;
		im.load_from_db(job.image_id,&sql);
		files.push_back(image_server_const.image_storage.get_file_specification_for_image(im,&sql));
	}
	else if (job.region_id != 0){
		bool specific_job_specified(false);
		for (unsigned int i = 0; i < job.operations.size(); i++){
			if (job.operations[i]) specific_job_specified = true;
		}
		if (job.maintenance_flag == ns_processing_job::ns_delete_censored_images){
			throw ns_ex("ns_handle_file_delete_request()::ns_delete_censored_images::Not implemented for regions yet!");
			/*	sql << "SELECT image_id FROM captured_images WHERE sample_id=" << job.sample_id
					<< " AND censored!=0 AND never_delete_image = 0 ORDER BY capture_time";
				ns_sql_result res;
				sql.get_rows(res);
				for (unsigned int i = 0; i < res.size(); i++){
					ns_image_server_image im;
					im.id = atol(res[i][0].c_str());
					files.push_back(image_server_const.image_storage.get_file_specification_for_image(im,sql));
				}*/
		}
		else
			ns_identify_region_files_to_delete(job.region_id, !specific_job_specified, job.operations, sql, files);
	}
	else if (job.sample_id != 0){
		//we want to delete just the raw, unmasked images
		if (job.operations[ns_unprocessed]){
			//often we only want to delete a subset of captured images.
			if (job.maintenance_flag == ns_processing_job::ns_only_delete_processed_captured_images){
				sql << "SELECT ci.image_id FROM captured_images as ci WHERE ci.sample_id=" << job.sample_id
					<< " AND " << ns_get_file_deletion_arguments::captured_file_specification("ci") << " ORDER BY ci.capture_time";
				ns_sql_result res;
				sql.get_rows(res);
				for (unsigned int i = 0; i < res.size(); i++){
					ns_image_server_image im;
					im.id = ns_atoi64(res[i][0].c_str());
					files.push_back(image_server_const.image_storage.get_file_specification_for_image(im,&sql));
				}
			}
			else if (job.maintenance_flag == ns_processing_job::ns_delete_censored_images){
				sql << "SELECT ci.image_id FROM captured_images as ci WHERE ci.sample_id=" << job.sample_id
					<< " AND " <<  ns_get_file_deletion_arguments::censored_file_specification("ci") << " ORDER BY capture_time";
				ns_sql_result res;
				sql.get_rows(res);
				for (unsigned int i = 0; i < res.size(); i++){
					ns_image_server_image im;
					im.id = ns_atoi64(res[i][0].c_str());
					files.push_back(image_server_const.image_storage.get_file_specification_for_image(im,&sql));
				}
			}
			else if (job.operations[ns_process_thumbnail]){
				sql << "SELECT ci.small_image_id FROM captured_images as ci WHERE ci.sample_id=" << job.sample_id
					<< " AND " << ns_get_file_deletion_arguments::small_captured_file_specification("ci") << " ORDER BY ci.capture_time";
				ns_sql_result res;
				sql.get_rows(res);
				for (unsigned int i = 0; i < res.size(); i++){
					ns_image_server_image im;
					im.id = ns_atoi64(res[i][0].c_str());
					files.push_back(image_server_const.image_storage.get_file_specification_for_image(im,&sql));
				}
			}
			else throw ns_ex("Requesting to delete unprocessed sample images with no flag specified");
		}
		//we want to delete the entire sample
		else
			files.push_back(image_server_const.image_storage.get_path_for_sample(job.sample_id,&sql));
	}
	else if (job.experiment_id != 0) {
		if (job.maintenance_flag == ns_processing_job::ns_delete_everything_but_raw_data) {
			std::vector<char> operations;
			ns_sql_result regions;
			ns_get_experiment_cleanup_subjects(job.experiment_id, regions, operations, sql);
			//delete images and storyboard for each region
			for (unsigned int i = 0; i < regions.size(); i++) {
				const ns_64_bit region_id(ns_atoi64(regions[i][0].c_str()));
				ns_identify_region_files_to_delete(region_id, false, operations, sql, files);
				files.push_back(image_server.image_storage.get_storyboard_path(0, region_id, 0, "", ns_xml,sql, true));
			}
			//delete experiment storyboards
			files.push_back(image_server.image_storage.get_storyboard_path(job.experiment_id, 0, 0, "", ns_xml, sql, true));
			//delete all region movement data
			if (regions.size() != 0)
				files.push_back(image_server_const.image_storage.get_file_specification_for_movement_data(ns_atoi64(regions[0][0].c_str()),"", &sql));

		}
		//delete all data stored on disk for the experiment!
		else
			files.push_back(image_server_const.image_storage.get_path_for_experiment(job.experiment_id, &sql));

	}
	else throw ns_ex("ns_handle_file_delete_request()::Unknown delete job type");
	if (files.size() == 0){
		image_server_const.register_server_event(
			ns_image_server_event("ns_handle_file_delete_request()::Deletion job request specification produced no files or "
								  "directories to be deleted."),&sql);
			sql << "DELETE FROM processing_jobs WHERE id = " << job.id;
			sql.send_query();

		return;
	}
	ns_64_bit job_id = image_server_const.image_storage.create_file_deletion_job(job.id,sql);
	for (unsigned int i = 0; i < files.size(); i++)
		image_server_const.image_storage.submit_file_deletion_request(job_id,files[i],sql);
}


ns_processing_job ns_handle_file_delete_action(ns_processing_job & job, ns_sql & sql){

	vector<ns_file_location_specification> specs;
	ns_64_bit parent_job_id(0);
	image_server_const.image_storage.get_file_deletion_requests(job.delete_file_job_id,parent_job_id,specs,sql);

	//after handling the request, we will want to delete the metadata of deleted images.  Load this from the parent job.
	ns_processing_job parent_job;
	sql << parent_job.provide_query_stub() << " FROM processing_jobs WHERE id = " << parent_job_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_handle_file_delete_action::Could not load parent job from database");
	parent_job.load_from_result(res[0]);
	try{
		sql << "UPDATE processing_jobs SET currently_under_processing=" << image_server_const.host_id() << "  WHERE id=" << parent_job.id;
		sql.send_query();
		ns_image_server_event ev("ns_image_processing_pipeline::Processing file deletion job for ");
		if (parent_job.region_id != 0) {
			sql << "SELECT e.name, s.name, r.name FROM experiments as e, capture_samples as s, sample_region_image_info as r WHERE r.id = " << parent_job.region_id << " AND r.sample_id = s.id AND s.experiment_id = e.id";
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() != 0) {
				parent_job.experiment_name = res[0][0];
				parent_job.sample_name = res[0][1];
				parent_job.region_name = res[0][2];
				ev << parent_job.experiment_name << "::" << parent_job.sample_name << "::" << parent_job.region_name;
			}
		}
		else if (parent_job.sample_id != 0) {
			sql << "SELECT e.name, s.name FROM experiments as e, capture_samples as s WHERE s.id = " << parent_job.sample_id << " AND s.experiment_id = e.id";
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() != 0) {
				parent_job.experiment_name = res[0][0];
				parent_job.sample_name = res[0][1];

				ev << parent_job.experiment_name << "::" << parent_job.sample_name;
			}
		}
		else if (parent_job.image_id != 0 &&		//older versions of the software do not respect the "ns_delete_everything_but_raw_data" flag.
											//to avoid the situation where old versions will delete all data for an experiment,
											//jobs with this flag set also have image_id set to an arbitrary value
											//which will cause older versions of the software to simply try to delete that single image
											//rather than an entire experiment.
											//So here, we check that experiment_id = 0, to identify legit image deletion tasks
			parent_job.experiment_id == 0) {
			ev << "::Image id " << parent_job.image_id;
		}
		else if (parent_job.experiment_id != 0) {
			sql << "SELECT e.name FROM experiments as e WHERE e.id = " << parent_job.experiment_id;
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() != 0) {
				parent_job.experiment_name = res[0][0];

				ev << parent_job.experiment_name;
			}
		}
		else throw ns_ex("Could not identify job type!");
		image_server_const.register_server_event(ev,&sql);

		for (unsigned int i = 0; i < specs.size(); i++)
			image_server_const.image_storage.delete_file_specification(specs[i],ns_delete_both_volatile_and_long_term);
		ns_handle_image_metadata_delete_action(parent_job,sql);
		image_server_const.image_storage.delete_file_deletion_job(job.delete_file_job_id,sql);
		sql << "DELETE from processing_jobs WHERE id=" << job.id;
		sql.send_query();
		sql << "DELETE from processing_jobs WHERE id=" << parent_job.id;
		sql.send_query();
	}
	catch(ns_ex & ex){
		ns_64_bit event_id = image_server_const.register_server_event(ex,&sql);
		sql << "UPDATE processing_jobs SET currently_under_processing=0,problem=" << event_id << " WHERE id=" << parent_job.id;
		sql.send_query();
		sql << "DELETE from processing_jobs WHERE id=" << job.id;
		sql.send_query();
	}
	return parent_job;
}
