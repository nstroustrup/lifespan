#include "ns_image_processing_pipeline.h"
#include "ns_image_server.h"
#include "ns_resampler.h"
#include "ns_image_server_automated_job_scheduler.h"
#include "ns_heat_map_interpolation.h"
#include "ns_movement_visualization_generator.h"
#include "ns_hand_annotation_loader.h"
#include "ns_image_registration.h"
#include "ns_simple_cache.h"
#include "ns_gaussian_pyramid.h"
using namespace std;

void ns_check_for_file_errors(ns_processing_job & job, ns_sql & sql){
	if (job.image_id != 0){
		ns_image_server_image im;
		im.load_from_db(job.image_id,&sql);
		if (!image_server_const.image_storage.image_exists(im,&sql,true)){
			const ns_64_bit event_id(image_server_const.register_server_event(ns_image_server_event("Image cannot be found on disk:") << im.path << DIR_CHAR_STR << im.filename,&sql));
			im.mark_as_problem(&sql,event_id);
		}
	}
	else if (job.region_id != 0){
		sql << "SELECT id FROM sample_region_images WHERE region_info_id = " << job.region_id;
		ns_sql_result res;
		sql.get_rows(res);
		unsigned long step(res.size()/10);
		for (unsigned long i = 0; i < res.size(); i++){
			if (i%step == 0)
				image_server_const.add_subtext_to_current_event(ns_to_string(10 * (i / step)) + "%...", &sql);
			ns_image_server_captured_image_region reg;
			try{
				reg.load_from_db(ns_atoi64(res[i][0].c_str()),&sql);
				if (reg.region_detection_results_id != 0){
					ns_image_worm_detection_results results;
					results.detection_results_id = reg.region_detection_results_id;
					try{
						results.load_from_db(false,false,sql,true);
					}
					catch(ns_ex & ex){
						image_server_const.add_subtext_to_current_event(ex.text()+ "\n", &sql);
					}
				}
				if (reg.region_interpolation_results_id != 0){
					ns_image_worm_detection_results results;
					results.detection_results_id = reg.region_interpolation_results_id;
					try{
						results.load_from_db(false,true,sql,true);
					}
					catch(ns_ex & ex){
						image_server_const.add_subtext_to_current_event(ex.text() + "\n", &sql);
					}
				}
				ns_image_stream_static_offset_buffer<ns_8_bit> buf(ns_image_stream_buffer_properties(10000,1));
				for (unsigned long j = 0; j < reg.op_images_.size(); j++){
					bool changed = false;
					if (reg.op_images_[j]!=0){
						ns_image_server_image im;
						try{
							im.load_from_db(reg.op_images_[j],&sql);
					
							ns_image_storage_source_handle<ns_8_bit> h(image_server_const.image_storage.request_from_storage(im,&sql));
							unsigned long internal_state;
							long w(h.input_stream().properties().width*h.input_stream().properties().components);
							if (w > buf.properties().width)
								buf.resize(ns_image_stream_buffer_properties(w,1));
							h.input_stream().send_lines(buf,1,internal_state);
							try{
								h.clear();
							}catch(...){}
						}
						catch(ns_ex & ex){
							reg.op_images_[j] = 0;
							changed = true;
						}
					}
					if (changed){
						reg.update_all_processed_image_records(sql);
						if (reg.op_images_[0] == 0){
							sql << "UPDATE sample_region_images SET problem=1 WHERE id = " << reg.region_images_id;
							sql.send_query();
						}
					}
				}
			}
			catch(ns_ex & ex){
				image_server_const.add_subtext_to_current_event(ex.text() + "\n", &sql);
				continue;
			}

		}
		//throw ns_ex("Checking for region file errors has not been implemented yet");
	}
	else if (job.sample_id != 0){
		sql << "SELECT name,experiment_id FROM capture_samples WHERE id = " << job.sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_check_for_file_errors()::Could not load sample ") << job.sample_id << " from db!";
		const string sample_name(res[0][0]);
		unsigned long experiment_id = ns_atoi64( res[0][1].c_str());
		sql << "SELECT name FROM experiments WHERE id = " << experiment_id;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_check_for_file_errors()::Could not load experiment ") << experiment_id << " from db!";
		const string experiment_name(res[0][0]);

		sql << "SELECT id,capture_time, image_id, small_image_id FROM captured_images WHERE problem = 0 AND currently_being_processed=0 AND sample_id = " << job.sample_id;
		sql.get_rows(res);
		bool found_problem(false);
		for (unsigned int i = 0; i < res.size(); i++){
			bool problem(false);
			bool large_image_exists(false);
			ns_image_server_image im;
			im.id = ns_atoi64(res[i][2].c_str());
			if (im.id != 0){
				im.load_from_db(im.id,&sql);
				if (!image_server_const.image_storage.image_exists(im,&sql,true)){
					problem = true;
					found_problem = true;
					const ns_64_bit event_id(image_server_const.register_server_event(ns_image_server_event("Large capture image cannot be found on disk:") << experiment_name << "::" << sample_name << "::" << ns_format_time_string(atol(res[i][1].c_str())),&sql));
					im.mark_as_problem(&sql,event_id);
				}
				else large_image_exists = true;
			}
			im.id = ns_atoi64(res[i][3].c_str());
			bool clear_small_image(false);
			if (im.id != 0){
				if (!image_server_const.image_storage.image_exists(im,&sql,true)){
					if (!large_image_exists){
						const ns_64_bit event_id(image_server_const.register_server_event(ns_image_server_event("Small capture image cannot be found on disk:") << experiment_name << "::" << sample_name << "::" << ns_format_time_string(atol(res[i][1].c_str())),&sql));
						im.mark_as_problem(&sql,event_id);
					}
					else{
						sql<< "DELETE FROM images WHERE id = " << im.id;
						sql.send_query();
						clear_small_image = true;
					}
					problem = true;
				}
			}
			if (problem){
				sql << "UPDATE captured_images SET problem = 1 WHERE id = " << res[i][0];
				sql.send_query();
			}
		}
		sql.send_query("COMMIT");
			
		if (found_problem)
			ns_image_server_automated_job_scheduler::identify_experiments_needing_captured_image_protection(sql,job.sample_id);
	}
	else throw ns_ex("ns_check_for_file_errors()::No subject specified for file checking");
}

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
				files.push_back(image_server.image_storage.get_storyboard_path(0, region_id, 0, "", sql, true));
			}
			//delete experiment storyboards
			files.push_back(image_server.image_storage.get_storyboard_path(job.experiment_id, 0, 0, "", sql, true));
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
		unsigned long event_id = image_server_const.register_server_event(ex,&sql);
		sql << "UPDATE processing_jobs SET currently_under_processing=0,problem=" << event_id << " WHERE id=" << parent_job.id;
		sql.send_query();	
		sql << "DELETE from processing_jobs WHERE id=" << job.id;
		sql.send_query();
	}
	return parent_job;
}

void ns_handle_image_metadata_delete_action(ns_processing_job & job,ns_sql & sql){
	vector<unsigned long> regions_to_delete;
	vector<unsigned long> samples_to_delete;
	unsigned long experiment_to_delete(0);
	bool autocommit_state = sql.autocommit_state();
	sql.set_autocommit(false);
	sql.send_query("BEGIN");
	std::vector<char> operations;
	operations.reserve(job.operations.size());
	operations.insert(operations.end(), job.operations.begin(), job.operations.end());
	try{
		if (job.image_id != 0 &&		//older versions of the software do not respect the "ns_delete_everything_but_raw_data" flag.  
										//to avoid the situation where old versions will delete all data for an experiment,
										//jobs with this flag set also have image_id set to an arbitrary value
										//which will cause older versions of the software to simply try to delete that single image
										//rather than an entire experiment.
										//So here, we check that experiment_id = 0, to identify legit image deletion tasks
			job.experiment_id == 0){  
			sql << "DELETE FROM images WHERE id = " << job.image_id;
			sql.send_query();
		}
		else if (job.region_id != 0)
			regions_to_delete.push_back(job.region_id);
		else if (job.sample_id != 0){
			if (operations[ns_unprocessed]){
				if (job.maintenance_flag == ns_processing_job::ns_only_delete_processed_captured_images){
					sql << "DELETE images FROM images, captured_images WHERE captured_images.sample_id = " << job.sample_id
						<< " AND captured_images.image_id = images.id "
						<< " AND " << ns_get_file_deletion_arguments::captured_file_specification("captured_images");
					sql.send_query();
					sql << "UPDATE captured_images SET image_id = 0 WHERE sample_id=" << job.sample_id
						<< " AND " << ns_get_file_deletion_arguments::captured_file_specification("captured_images");
					sql.send_query();
				}
				else if (job.maintenance_flag == ns_processing_job::ns_delete_censored_images){
					sql << "DELETE images FROM images, captured_images WHERE captured_images.sample_id = " << job.sample_id
							<< " AND captured_images.image_id = images.id "
							<< " AND " << ns_get_file_deletion_arguments::censored_file_specification("captured_images");
					sql.send_query();
					sql << "UPDATE captured_images SET image_id = 0 WHERE sample_id=" << job.sample_id
						<< " AND " <<  ns_get_file_deletion_arguments::censored_file_specification("captured_images");
					sql.send_query();
				}
				else if (operations[ns_process_thumbnail]){
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
			else{
				for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++)
					if (operations[task]) throw ns_ex("Full sample image deletion specified with operations flagged!");
				samples_to_delete.push_back(job.sample_id);
				//check for malformed request
			}
		}
		else if (job.experiment_id != 0){
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
		for (unsigned long i = 0; i < samples_to_delete.size(); i++){
			sql << "SELECT id FROM sample_region_image_info WHERE sample_id = " << samples_to_delete[i];
			ns_sql_result res;
			sql.get_rows(res);
			for (unsigned int i = 0; i < res.size(); i++)
				regions_to_delete.push_back(ns_atoi64(res[i][0].c_str()));
		}
		for (unsigned long i = 0; i < regions_to_delete.size(); i++){
			//unless the job is flagged as deleting the entire sample region, just delete individual images
			if (job.maintenance_flag != ns_processing_job::ns_delete_entire_sample_region){
				for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++){
					if (!operations[task]) continue;
					const string db_table(ns_processing_step_db_table_name(task));
					//if the data is processed for each time point in the image, delete each one
					if (db_table ==  "sample_region_images"){
						sql << "DELETE images FROM images,sample_region_images WHERE sample_region_images.region_info_id = " << regions_to_delete[i] << 
							" AND images.id = sample_region_images." << ns_processing_step_db_column_name(task);
					//	cerr << sql.query() << "\n";
						sql.send_query();
						sql << "UPDATE sample_region_images SET " << ns_processing_step_db_column_name(task) << " = 0 WHERE region_info_id = " << regions_to_delete[i];
						sql.send_query();
					}
					//if the data is processed once for the entire region, delete it.
					else{
						sql << "DELETE images FROM images, sample_region_image_info WHERE sample_region_image_info.id = " << regions_to_delete[i] << 
								" AND images.id = sample_region_image_info." << ns_processing_step_db_column_name(task);
						sql.send_query();
						sql << "UPDATE sample_region_image_info SET " << ns_processing_step_db_column_name(task) << " = 0 WHERE id = " << regions_to_delete[i];
						sql.send_query();
					}
				}
			}
			//if the job is flagged to delete the entire region, do it.
			else{
				//check for malformed request
				for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++)
					if (operations[task]) throw ns_ex("Full region image deletion specified with operations flagged!");

				//delete processed images
				for (unsigned int task = 0; task < (unsigned int)ns_process_last_task_marker; task++){
					const string db_table(ns_processing_step_db_table_name(task));
					//if the data is processed for each time point in the image, delete each one
					if (db_table ==  "sample_region_images"){
						sql << "DELETE images FROM images,sample_region_images WHERE sample_region_images.region_info_id = " << regions_to_delete[i] << 
							" AND images.id = sample_region_images." << ns_processing_step_db_column_name(task);
					//	cerr << sql.query() << "\n";
						sql.send_query();
					}
					//if the data is processed once for the entire region, delete it.
					else{
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
		for (unsigned long i = 0; i < samples_to_delete.size(); i++){
			//delete masks associated with each sample
			sql << "SELECT mask_id FROM capture_samples WHERE id = " << samples_to_delete[i];
			ns_sql_result res;
			sql.get_rows(res);
			for (unsigned long j = 0; j < res.size(); j++){
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


			//delete sample time relationships
			sql << "DELETE FROM sample_time_relationships WHERE sample_id = " << samples_to_delete[i];
			sql.send_query();
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
		if (experiment_to_delete != 0){
			sql << "DELETE FROM experiments WHERE id = " << experiment_to_delete;
			sql.send_query();
		}
	//	throw ns_ex("YOIKS!");
		sql.send_query("COMMIT");
	}
	catch(...){
		sql.clear_query();
		sql.send_query("ROLLBACK");
		sql.set_autocommit(autocommit_state);
		throw;
	}
	sql.set_autocommit(autocommit_state);
}


///specifies the database entry id for the precomputed image corresponding to the specified processing step
bool ns_precomputed_processing_step_images::specify_image_id(const ns_processing_task & i, const unsigned int id,ns_sql & sql){
	exists[i] = (id != 0);
	if (id == 0) return false;
	
	images[i].id = id;

	if (!image_server_const.image_storage.image_exists(images[i],&sql)){
		image_server_const.register_server_event(ns_image_server_event("ns_precomputed_processing_step_images::") << ns_processing_task_to_string(i) << " ("<< images[i].filename << ") could not be loaded from disk.",&sql);
		images[i].id = 0;
		exists[i] = 0;
		return false;
	}
	return true;
}



void recalculate_dependencies(const ns_processing_task to_recalc, vector<char> & operations,ns_precomputed_processing_step_images & precomputed_images){
	if(!precomputed_images.has_been_calculated(to_recalc)) return;
	precomputed_images.remove_preprocessed_image(to_recalc);
	for (unsigned int k = 0; k < operations.size(); k++){
		if (ns_image_processing_pipeline::preprocessed_step_required(to_recalc,(ns_processing_task)k))
			recalculate_dependencies((ns_processing_task)k,operations,precomputed_images);
	}
}


bool attempt_to_preload(const ns_processing_task & task,ns_precomputed_processing_step_images & precomputed_images, vector<char> &operations, ns_image_standard &input,ns_sql & sql){
	bool computation_needed(true);
	if (!precomputed_images.is_provided(task))
		return false;

	try{
		ns_image_processing_pipeline::register_event(task,input.properties(),ns_image_server_event(),true,sql);
		precomputed_images.load_image<ns_image_standard,ns_8_bit>(task,input,sql);
		return true;
	}
	catch(ns_ex & ex){
		//if we wanted to load a precomputed image from disk, but failed, we need to recalculate it.
		image_server_const.register_server_event(ex,&sql);

		//we also need to precompute any images that depend on it.
		precomputed_images.remove_preprocessed_image(task);
		recalculate_dependencies(ns_process_spatial,operations,precomputed_images);
		return false;
	}
}

	///loads all precomputed images based on the ids specified using specifiy_image_id()
void ns_precomputed_processing_step_images::load_from_db(ns_sql & sql){
	if (loaded) return;
	for (unsigned int i = 0; i < images.size(); i++){
		if (images[i].id == 0) continue;
		images[i].load_from_db(images[i].id,&sql);
	}
	loaded=true;
}


///given the source image, run() runs the processing steps specified in operations.  operations is a bit map with each entry corresponding to the step
///referred to by its ns_processing_task enum value.  Operations flagged as "1" are performed, operations flagged as "0" are skipped.
void ns_image_processing_pipeline::process_region(const ns_image_server_captured_image_region & region_im, const vector<char> operations, ns_sql & sql, const ns_svm_model_specification & model, const ns_lifespan_curve_cache_entry & death_annotations){
	vector<char> ops = operations;
	ns_image_server_captured_image_region region_image(region_im);
	
	region_image.load_from_db(region_image.region_images_id,&sql);
	ns_image_server_captured_image_region source_image = region_image;

	//used to group of sub-processing steps in the event log
	//allows complex statistics on processing speed to be calculated.
	ns_image_server_event parent_event("ns_image_processing_pipeline::Processing image region ");
	parent_event << source_image.filename(&sql) << "\nActions: " << ns_create_operation_summary(operations);
	parent_event.specifiy_event_subject(region_image);
	parent_event.specify_parent_server_event(image_server_const.register_server_event(parent_event,&sql));
	parent_event.clear_text();

	//Check the databse to see if any of the image steps have been precomputed.
	//We can load them from disk rather than having to re-compute them.
	ns_precomputed_processing_step_images precomputed_images;
	analyze_operations(region_image,ops,precomputed_images,sql);

	const bool allow_use_of_volatile_storage(false);
	const bool report_file_activity_to_db(false);
	bool had_to_use_volatile_storage;

	string hd_compression_rate = image_server_const.get_cluster_constant_value("jp2k_hd_compression_rate", ns_to_string(NS_DEFAULT_JP2K_HD_COMPRESSION), &sql);
	float hd_compression_rate_f = atof(hd_compression_rate.c_str());
	if (hd_compression_rate_f <= 0)
		throw ns_ex("Invalid compression rate specified in jp2k_hd_compression_rate cluster constant: ") << hd_compression_rate_f;

	ns_high_precision_timer tm;
	try{
		vector<char> operations = ops;
		precomputed_images.load_from_db(sql);
	/*	if (operations[ns_process_movement_paths_visualition_with_mortality_overlay] && 
			precomputed_images.has_been_calculated(ns_process_movement_paths_visualization)){
			for (unsigned int i = 0; i < (int) ns_process_movement_paths_visualization; i++)
				ops[i] = 0;
		}*/

		//only one frame out of ten needs to have a training set generated for it.  
		//So we can skip analysis all together if a) nothing has to be done except for the training set
		// and b) this isn't the 1/10 that needs to be completed.
		bool this_frame_should_generate_a_training_set_image(false);
		if (operations[ns_process_add_to_training_set]){	

				sql << "SELECT id FROM sample_region_images WHERE region_info_id = " 
					<< region_image.region_info_id << " AND censored = 0 "
					<< " ORDER BY capture_time ASC";
				ns_sql_result res;
				sql.get_rows(res);
				if (res.empty())
					throw ns_ex("Could not load region information from db!");
				const unsigned long offset = res.size()/10;
				if (res.size() > 1){
					unsigned int cur_point_pos;
					for (cur_point_pos = 0; cur_point_pos < res.size(); cur_point_pos++)
						if (ns_atoi64(res[cur_point_pos][0].c_str()) == region_image.region_images_id)
							break;
					if (cur_point_pos%offset==0)
						this_frame_should_generate_a_training_set_image = true;
					else this_frame_should_generate_a_training_set_image = false;
				}
				if (!this_frame_should_generate_a_training_set_image){
					bool other_images_required(false);
					for (unsigned int i = ns_process_spatial; i < ns_process_worm_detection_labels; i++){
						if (i == ns_process_add_to_training_set)
							continue;
						if (operations[i] && !precomputed_images.has_been_calculated((ns_processing_task)i)){
							other_images_required = true;
							break;
						}
					}
					if (!other_images_required){
						sql << "UPDATE sample_region_images SET make_training_set_image_from_frame = 0," <<
								ns_processing_step_db_column_name(ns_process_add_to_training_set) << "=1 WHERE id = " << region_image.region_images_id;
						sql.send_query();
					
						return;
					}
				}
			}
		try{
			//if we're doing worm detection on the image, we're going to need the spatial average image.
			//So we must grab it from the pre-computed images.
			bool unprocessed_loaded(false);
			
			if (operations[ns_process_spatial]) {

				if (!attempt_to_preload(ns_process_spatial, precomputed_images, operations, spatial_average, sql)) {
					ns_image_server_image unprocessed_image(region_image.request_processed_image(ns_unprocessed, sql));
					ns_image_storage_source_handle<ns_8_bit> unprocessed_image_file(image_server_const.image_storage.request_from_storage(unprocessed_image, &sql));
					unprocessed_image_file.input_stream().pump(unprocessed, _image_chunk_size);
					unprocessed_loaded = true;

					register_event(ns_process_spatial, unprocessed_image_file.input_stream().properties(), parent_event, false, sql);

					tm.start();

					///internal binding used to link processing steps
					///spatial averager -> next step in pipeline
					ns_image_stream_binding<ns_spatial_median_calculator<ns_component, true>, ns_image_whole<ns_component> >
						spatial_calc(spatial_averager, spatial_average, _image_chunk_size);

					unprocessed.pump(spatial_calc, _image_chunk_size);

					ns_crop_lower_intensity<ns_component>(spatial_average, (ns_component)ns_worm_detection_constants::get(ns_worm_detection_constant::tiff_compression_intensity_crop_value, spatial_average.properties().resolution));


					//output a spatially averaged copy to disk.
					//if set to tiff, do not compress spatial images: uses 3x more disk space for negliable improvement in image quality (and attendant increase in movement dection accuracy)
					const ns_image_type spatial_image_type = ns_jp2k;
					ns_image_server_image output_image = region_image.create_storage_for_processed_image(ns_process_spatial, spatial_image_type, &sql);

				
						ns_image_storage_reciever_handle<ns_component> r = image_server_const.image_storage.request_storage(
							output_image,
							spatial_image_type, (spatial_image_type==ns_jp2k)? hd_compression_rate_f:1.0, _image_chunk_size, &sql,
							had_to_use_volatile_storage,
							report_file_activity_to_db,
							allow_use_of_volatile_storage);
						spatial_average.pump(r.output_stream(), _image_chunk_size);

					//	r.output_stream().init(ns_image_properties(0, 0, 0));
					
					output_image.mark_as_finished_processing(&sql);

					image_server.register_job_duration(ns_process_spatial, tm.stop());
				}
			}

			if (!unprocessed_loaded && (precomputed_images.worm_detection_needs_to_be_performed || operations[ns_process_compress_unprocessed])) {
				ns_image_server_image unprocessed_image(region_image.request_processed_image(ns_unprocessed, sql));
				ns_image_storage_source_handle<ns_8_bit> unprocessed_image_file(image_server_const.image_storage.request_from_storage(unprocessed_image, &sql));
				unprocessed_image_file.input_stream().pump(unprocessed, _image_chunk_size);
				unprocessed_loaded = true;
			}
			if (operations[ns_process_compress_unprocessed]) {
				
				ns_image_server_image unprocessed_image(region_image.request_processed_image(ns_unprocessed, sql));
			
				ns_image_server_image old_im(unprocessed_image);
				ns_image_type t(ns_get_image_type(unprocessed_image.filename));
				if (t != ns_jp2k) {
					unprocessed_image.filename = ns_dir::extract_filename_without_extension(unprocessed_image.filename);
					unprocessed_image.filename += ".jp2";

					string compression_rate = image_server_const.get_cluster_constant_value("jp2k_compression_rate", ns_to_string(NS_DEFAULT_JP2K_COMPRESSION), &sql);
					float compression_rate_f = atof(compression_rate.c_str());
					if (compression_rate_f <= 0)
						throw ns_ex("Invalid compression rate specified in jp2k_compression_rate cluster constant: ") << compression_rate_f;
					bool b;
					ns_image_storage_reciever_handle<ns_8_bit> im_dest(image_server_const.image_storage.request_storage(
						unprocessed_image, ns_jp2k, compression_rate_f, 1024, &sql, b, false, false));
					unprocessed.pump(im_dest.output_stream(), 1024);
					unprocessed_image.save_to_db(unprocessed_image.id, &sql);
					image_server_const.image_storage.delete_from_storage(old_im, ns_delete_long_term, &sql);
				}
			}

			//lossy stretch of dynamic range
			if (operations[ns_process_lossy_stretch]){
				if (!attempt_to_preload(ns_process_lossy_stretch,precomputed_images,operations,dynamic_stretch,sql)){
				
					register_event(ns_process_lossy_stretch,dynamic_stretch.properties(),parent_event,false,sql);

					tm.start();
					spatial_average.pump(dynamic_stretch,_image_chunk_size);
				
				

					ns_process_dynamic_stretch(dynamic_stretch);
				
		
					//output a small, compressed copy for visualization			
					ns_image_server_image output_image =  region_image.create_storage_for_processed_image(ns_process_lossy_stretch,ns_jpeg,&sql);
					ns_image_storage_reciever_handle<ns_component> r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, _image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																allow_use_of_volatile_storage);
					dynamic_stretch.pump(r.output_stream(),_image_chunk_size);
					output_image.mark_as_finished_processing(&sql);

					image_server.register_job_duration(ns_process_lossy_stretch,tm.stop());
				}
			}
			ns_image_standard paths_visualization;
			if (operations[ns_process_movement_paths_visualization]){
				if (!attempt_to_preload(ns_process_movement_paths_visualization,precomputed_images,operations,paths_visualization,sql)){
					if (dynamic_stretch.properties().width == 0 || dynamic_stretch.properties().height == 0){
						if (!attempt_to_preload(ns_process_lossy_stretch,precomputed_images,operations,dynamic_stretch,sql))
							throw ns_ex("Could not load or recreate the movement paths visualization!");
					}
					//throw ns_ex("Depreciated!");
					//dynamic_stretch.load_from_db(region_image.region_images_id,sql);
			
					register_event(ns_process_movement_paths_visualization,dynamic_stretch.properties(),parent_event,false,sql);
	
					ns_movement_visualization_generator gen;
					const ns_death_time_annotation_compiler & compiler(death_annotations.get_region_data(ns_death_time_annotation_set::ns_all_annotations,region_image.region_info_id,sql));
					ns_death_time_annotation_compiler::ns_region_list::const_iterator p(compiler.regions.find(region_image.region_info_id));
					if (p == compiler.regions.end())
						throw ns_ex("Could not find region ") << region_image.region_info_id << " in db!";
					//extract annotations for just this timepoint
					ns_death_time_annotation_set annotations;
					gen.create_time_path_analysis_visualization(region_image, p->second,dynamic_stretch, paths_visualization,sql);
	
					ns_image_server_image out_im(region_image.create_storage_for_processed_image(ns_process_movement_paths_visualization,ns_tiff,&sql));
					ns_image_storage_reciever_handle<ns_8_bit> out_im_f(image_server_const.image_storage.request_storage(
														out_im,
														ns_tiff, 1.0,1024,&sql,
																	had_to_use_volatile_storage,
																	report_file_activity_to_db,
																	allow_use_of_volatile_storage));
					paths_visualization.pump(out_im_f.output_stream(),1024);
			
					out_im.mark_as_finished_processing(&sql);
				}
		
			}
			if (operations[ns_process_movement_paths_visualition_with_mortality_overlay]){
				region_image.load_from_db(region_image.region_images_id,&sql);

				register_event(ns_process_movement_paths_visualition_with_mortality_overlay,dynamic_stretch.properties(),parent_event,false,sql);
		/*
				ns_image_server_image vis_image(region_image.request_processed_image(ns_process_movement_paths_visualization,sql));
				ns_image_storage_source_handle<ns_8_bit> image_file(image_server_const.image_storage.request_from_storage(vis_image,&sql));

				image_file.input_stream().pump(underlay,_image_chunk_size);*/
				ns_region_metadata metadata;
				metadata.load_from_db(region_image.region_info_id,"",sql);
				overlay_graph(region_image.region_info_id,paths_visualization,region_image.capture_time,metadata,death_annotations,sql);


				ns_image_server_image out_im(region_image.create_storage_for_processed_image(ns_process_movement_paths_visualition_with_mortality_overlay,ns_jpeg,&sql));
				ns_image_storage_reciever_handle<ns_8_bit> out_im_f(image_server_const.image_storage.request_storage(
													out_im,ns_jpeg,
													NS_DEFAULT_JPEG_COMPRESSION, 1024,&sql,
													had_to_use_volatile_storage,
													report_file_activity_to_db,
													allow_use_of_volatile_storage));
				paths_visualization.pump(out_im_f.output_stream(),1024);
			
				out_im.mark_as_finished_processing(&sql);
			}

			//apply threshold	

			if (operations[ns_process_threshold]){
				if (!attempt_to_preload(ns_process_threshold,precomputed_images,operations,thresholded,sql)){
			

				//cerr << "Thresholding source resolution = " << temporary_image.properties().resolution << "\n";
				register_event(ns_process_threshold,temporary_image.properties(),parent_event,false,sql);	

				ns_threshold_manager<ns_component,ns_image_server_captured_image_region> threshold_manager;
			
				tm.start();

				threshold_manager.run( region_image,spatial_average,thresholded,sql,_image_chunk_size);
			
				ns_image_server_image output_image =  region_image.create_storage_for_processed_image(ns_process_threshold,ns_tiff,&sql);
				ns_image_storage_reciever_handle<ns_component> r = image_server_const.image_storage.request_storage(
															output_image,
															ns_tiff, 1.0, _image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																allow_use_of_volatile_storage);
				thresholded.pump(r.output_stream(),_image_chunk_size);
				output_image.mark_as_finished_processing(&sql);
			
				image_server.register_job_duration(ns_process_threshold,tm.stop());
				}
			}


			//detect worms
			//We are guarenteed
			//1)The thresholded image has already been loaded into thresholded
			//2)The spatial average has been previously loaded into spatial_average


			if (precomputed_images.worm_detection_needs_to_be_performed){	
			
				register_event(ns_process_worm_detection,spatial_average.properties(),parent_event,false,sql);
				image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Detecting Worms...") << ns_ts_minor_event,&sql);
			
			
				tm.start();
			
		
				ns_worm_detector<ns_image_standard> worm_detector;
	
				ns_detected_worm_info::ns_visualization_type vis_type = (operations[ns_process_accept_vis] || operations[ns_process_reject_vis]) ? ns_detected_worm_info::ns_vis_raster : ns_detected_worm_info::ns_vis_none;
			
				ns_whole_image_region_stats image_stats;
				image_stats.absolute_intensity_stats.calculate(unprocessed,false);
				image_stats.relative_intensity_stats.calculate(spatial_average,true);
				region_image.load_from_db(region_image.region_images_id,&sql);
				if (region_image.capture_time == 0)
					throw ns_ex("No capture time specified!");
				sql << "SELECT maximum_number_of_worms_per_plate FROM sample_region_image_info WHERE id = " << region_image.region_info_id;
				ns_sql_result res;
				sql.get_rows(res);
				if (res.size() == 0)
					throw ns_ex("Could not load worm count maximum from database!");

				//get potentially cached static image mask.  this caching was built to be threadsafe
				ns_image_server_image static_mask_image = region_image.request_processed_image(ns_process_static_mask, sql);
				ns_acquire_for_scope<ns_image_worm_detection_results> detected_worms;
				{
					ns_image_storage_handler::cache_t::const_handle_t static_mask;
					ns_image_cache_data_source data_source;
					data_source.handler = &image_server_const.image_storage; 
					data_source.sql = & sql;
					if (static_mask_image.id != 0) {
						image_server_const.add_subtext_to_current_event("Using a static mask.\n", &sql);
						image_server.image_storage.cache.get_for_read(static_mask_image, static_mask, data_source);
					}



					const unsigned long worm_count_max(atol(res[0][0].c_str()));
					detected_worms.attach(worm_detector.run(region_image.region_info_id, region_image.capture_time,
						unprocessed,
						thresholded,
						spatial_average,
						((static_mask_image.id != 0)?&(static_mask().image):0),
						ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area, spatial_average.properties().resolution),
						ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area, spatial_average.properties().resolution),
						ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal, spatial_average.properties().resolution),
						model,
						worm_count_max,
						&sql,
						"",
						vis_type,
						image_stats
					));
				}//implicitly release static mask back to cache.

				image_server.register_job_duration(ns_process_worm_detection,tm.stop());
				//save worm information
				detected_worms().save(region_image, false, sql, true);


				ns_image_stream_color_converter<ns_component, ns_image_stream_static_offset_buffer<ns_component> >color_converter(_image_chunk_size);
				color_converter.set_output_components(3);
				ns_image_stream_binding<ns_image_stream_color_converter<ns_component, ns_image_stream_static_offset_buffer<ns_component> >,
										ns_image_whole<ns_component> > to_color(color_converter,temporary_image,_image_chunk_size);

				//draw without labels
				if (operations[ns_process_worm_detection]){
					spatial_average.pump(to_color,_image_chunk_size);
					detected_worms().create_visualization(spatial_average.properties().height/75,6,temporary_image,region_image.display_label(),true,false,false);
					ns_image_whole<ns_component> small_im;
					temporary_image.resample(get_small_dimensions(temporary_image.properties()),small_im);
					

					ns_image_server_image output_image = region_image.create_storage_for_processed_image(ns_process_worm_detection,ns_jpeg,&sql);
					ns_image_storage_reciever_handle<ns_component>  r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, _image_chunk_size,&sql,
															had_to_use_volatile_storage,
															report_file_activity_to_db,
															allow_use_of_volatile_storage);
					small_im.pump(r.output_stream(),_image_chunk_size);
					output_image.mark_as_finished_processing(&sql);
	
				}
				//draw with labels
				if (operations[ns_process_worm_detection_labels]){
					spatial_average.pump(to_color,_image_chunk_size);
					detected_worms().create_visualization(spatial_average.properties().height/75,6,temporary_image,region_image.display_label(),true,true,true);	
			
					ns_image_server_image d_vis = region_image.create_storage_for_processed_image(ns_process_worm_detection_labels,ns_jpeg,&sql);
					ns_image_storage_reciever_handle<ns_component> d_vis_o = image_server_const.image_storage.request_storage(
																d_vis,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION,  _image_chunk_size,&sql,
															had_to_use_volatile_storage,
															report_file_activity_to_db,
															allow_use_of_volatile_storage);
					temporary_image.pump(d_vis_o.output_stream(),_image_chunk_size);	
					d_vis.mark_as_finished_processing(&sql);
	
				}
				//if worm detection has been performed in a way that will save that information to the db,
				//we must also save the region bitmap.
				if (operations[ns_process_worm_detection] || operations[ns_process_worm_detection_labels]){
					ns_image_whole<ns_component> comp_out;
					//save region bitmap
					register_event(ns_process_region_vis,comp_out.properties(),parent_event,false,sql);
					const ns_image_standard & worm_collage(detected_worms().generate_region_collage(unprocessed,spatial_average,thresholded));
				
					ns_image_server_image region_bitmap = region_image.create_storage_for_processed_image(ns_process_region_vis,ns_tiff,&sql);
					ns_image_storage_reciever_handle<ns_component> region_bitmap_o = image_server_const.image_storage.request_storage(
																region_bitmap,
																ns_tiff, 1.0,_image_chunk_size,&sql,
															had_to_use_volatile_storage,
															report_file_activity_to_db,
															allow_use_of_volatile_storage);
					worm_collage.pump(region_bitmap_o.output_stream(),_image_chunk_size);
					region_bitmap.mark_as_finished_processing(&sql);
				}
				//cerr << "Saving spine vis...\n";
				//save accepted worm spine visualization
				if (operations[ns_process_accept_vis]){
					ns_image_whole<ns_component> comp_out;
					register_event(ns_process_accept_vis,parent_event,false,sql);
					detected_worms().create_spine_visualizations(comp_out);
				
					ns_image_server_image a_vis = region_image.create_storage_for_processed_image(ns_process_accept_vis,ns_tiff,&sql);
					ns_image_storage_reciever_handle<ns_component> a_vis_o = image_server_const.image_storage.request_storage(
																a_vis,
																ns_tiff, 1.0, _image_chunk_size,&sql,
															had_to_use_volatile_storage,
															report_file_activity_to_db,
															allow_use_of_volatile_storage);
					comp_out.pump(a_vis_o.output_stream(),_image_chunk_size);
				
					a_vis.mark_as_finished_processing(&sql);
				}

				//save accepted reject spine visualization
				if (operations[ns_process_reject_vis]){
					ns_image_whole<ns_component> comp_out;
					register_event(ns_process_reject_vis,comp_out.properties(),parent_event,false,sql);
					detected_worms().create_reject_spine_visualizations(comp_out);
					
					ns_image_server_image r_vis = region_image.create_storage_for_processed_image(ns_process_reject_vis,ns_tiff,&sql);
					ns_image_storage_reciever_handle<ns_component> r_vis_o = image_server_const.image_storage.request_storage(
																r_vis,
																ns_tiff, 1.0, _image_chunk_size,&sql,
															had_to_use_volatile_storage,
															report_file_activity_to_db,
															allow_use_of_volatile_storage);
					comp_out.pump(r_vis_o.output_stream(),_image_chunk_size);	
					r_vis.mark_as_finished_processing(&sql);
				}
			

				//bool NS_OUTPUT_ONLY_DETECTED_WORMS = true;
				if (operations[ns_process_add_to_training_set]){	
					try{
						
						register_event(ns_process_add_to_training_set,temporary_image.properties(),parent_event,false,sql);

						if (this_frame_should_generate_a_training_set_image){
							sql << "UPDATE sample_region_images SET make_training_set_image_from_frame = 1 WHERE id = " << region_image.region_images_id;
							sql.send_query();
							unsigned long spec_max = ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_objects_per_image);
							if (detected_worms().number_of_putative_worms() > spec_max)
								image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Training set image object count (") << detected_worms().number_of_putative_worms() << ") exceeds specified maximum (" << spec_max << ")",&sql);
							else{
								ns_image_standard ti;
								ns_worm_training_set_image::generate(detected_worms(),ti);
								ns_image_server_image a_worm_im = region_image.create_storage_for_processed_image(ns_process_add_to_training_set,ns_tiff,&sql);
									ns_image_storage_reciever_handle<ns_component> a_worm = image_server_const.image_storage.request_storage(
																		a_worm_im,
																		ns_tiff, 1.0, _image_chunk_size,&sql,
																	had_to_use_volatile_storage,
																	report_file_activity_to_db,
																	allow_use_of_volatile_storage);
							
								ti.pump(a_worm.output_stream(),_image_chunk_size);

								ofstream * metadata_out(image_server_const.image_storage.request_metadata_output(a_worm_im,ns_csv,false,&sql));
								try{
									detected_worms().output_feature_statistics(*metadata_out);
									metadata_out->close();
									delete metadata_out;
								}
								catch(...){
									metadata_out->close();
									delete metadata_out;
									throw;
								}
							}
						}
						else{
							
							sql << "UPDATE sample_region_images SET make_training_set_image_from_frame = 0," <<
								 ns_processing_step_db_column_name(ns_process_add_to_training_set) << "=1 WHERE id = " << region_image.region_images_id;
							sql.send_query();
						}
		//				region_image.mark_as_added_to_training_set(sql);
					}
					catch(ns_ex & ex){
						ns_ex ex2;
						ex2 << "Could not create training set: " << ex.text();
						image_server_const.register_server_event(ex2,&sql);
					}
				}
				detected_worms.release();
			}

			//This option overlays the lifespan curve on the specified image
			if (operations[ns_process_worm_detection_with_graph]){
				if (!attempt_to_preload(ns_process_worm_detection,precomputed_images,operations,temporary_image,sql))
					throw ns_ex("Could not load worm detection for ns_process_worm_detection_with_graph.");

				register_event(ns_process_worm_detection_with_graph,temporary_image.properties(),parent_event,false,sql);
	
				ns_region_metadata metadata;
				metadata.load_from_db(region_image.region_info_id,"",sql);
				overlay_graph(region_image.region_info_id,temporary_image,region_image.capture_time,metadata,death_annotations,sql);
				ns_image_server_image output_image;

				output_image = region_image.create_storage_for_processed_image(ns_process_worm_detection_with_graph,ns_jpeg,&sql);
				ns_image_storage_reciever_handle<ns_component>  r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION,_image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																allow_use_of_volatile_storage);
				temporary_image.pump(r.output_stream(),_image_chunk_size);
				output_image.mark_as_finished_processing(&sql);
			}
			if (operations[ns_process_movement_coloring_with_graph]){
				if (!attempt_to_preload(ns_process_movement_coloring,precomputed_images,operations,temporary_image,sql))
					throw ns_ex("Could not load worm coloring for ns_process_movement_coloring_with_graph.");
				register_event(ns_process_movement_coloring_with_graph,temporary_image.properties(),parent_event,false,sql);
	
				ns_region_metadata metadata;
				metadata.load_from_db(region_image.region_info_id,"",sql);
				overlay_graph(region_image.region_info_id,temporary_image,region_image.capture_time,metadata,death_annotations,sql);
				
				ns_image_server_image output_image;
				output_image = region_image.create_storage_for_processed_image(ns_process_movement_coloring_with_graph,ns_jpeg,&sql);
				ns_image_storage_reciever_handle<ns_component>  r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, _image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																allow_use_of_volatile_storage);
				temporary_image.pump(r.output_stream(),_image_chunk_size);
				output_image.mark_as_finished_processing(&sql);
			
			}
		
			if(operations[ns_process_movement_coloring_with_survival]){
				if (!attempt_to_preload(ns_process_movement_coloring,precomputed_images,operations,temporary_image,sql))
					throw ns_ex("Could not load worm coloring for ns_process_movement_coloring_with_survival");
			
				register_event(ns_process_movement_coloring_with_survival,temporary_image.properties(),parent_event,false,sql);
	
				ns_region_metadata metadata;
				metadata.load_from_db(region_image.region_info_id,"",sql);
				overlay_graph(region_image.region_info_id,temporary_image,region_image.capture_time,metadata,death_annotations,sql);
	
				ns_image_server_image output_image;
				output_image = region_image.create_storage_for_processed_image(ns_process_movement_coloring_with_survival,ns_jpeg,&sql);
				ns_image_storage_reciever_handle<ns_component>  r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION,_image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																allow_use_of_volatile_storage);
				temporary_image.pump(r.output_stream(),_image_chunk_size);
				output_image.mark_as_finished_processing(&sql);
			
			}

			if (last_task >= ns_process_last_task_marker)
				throw ns_ex("ns_image_processing_pipeline::Attempting to pump off the end of the pipeline!");
		}
		catch(ns_ex & ex){
		//	image_server_const.performance_statistics.cancel_outstanding_jobs();
			throw ex;
		}
	}
	catch(...){
		sql.clear_query();
		source_image.delete_processed_image(last_task,ns_delete_both_volatile_and_long_term,&sql);
		source_image.mark_as_finished_processing(&sql);
		throw;
	}
}
float ns_image_processing_pipeline::process_mask(ns_image_server_image & source_image, const ns_64_bit mask_id, ns_sql & sql){	
	
	image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Processing Mask for image_id ") << source_image.id,&sql);
	source_image.load_from_db(source_image.id,&sql);
	string new_filename = ns_dir::extract_filename_without_extension(source_image.filename);
	new_filename += "_processed.jpg";
	//create image for visualization
	sql << "SELECT visualization_image_id,resize_factor FROM image_masks WHERE id = '" << mask_id <<"'";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_processing_job_scheduler::Could not load specified mask: ") << mask_id << "\n";
	unsigned int resize_factor = atol(res[0][1].c_str());
	//cerr << "Resize factor = " << resize_factor;
	ns_image_server_image visualization_image;
	//create new image for visualization if necissary
	if (res[0][0] == "0"){
		sql << "INSERT INTO images SET filename = '" << sql.escape_string(new_filename) << "', path='" << sql.escape_string(source_image.path) << "', `partition`='" << source_image.partition << "', host_id= " << image_server_const.host_id() << ", creation_time = " << ns_current_time();
		//cerr << sql.query() << "\n";
		visualization_image.id = sql.send_query_get_id();
		sql << "UPDATE image_masks SET visualization_image_id='" << visualization_image.id << "' WHERE id='" << mask_id << "'";
		sql.send_query();
	}
	else{
		visualization_image.id = ns_atoi64(res[0][0].c_str());
		sql << "UPDATE images SET filename = '" << sql.escape_string(new_filename) << "', path='" << sql.escape_string(source_image.path) << "', `partition`='" << source_image.partition << "', host_id= " << image_server_const.host_id() << ", creation_time = " << ns_current_time()
				<< " WHERE id=" << 	visualization_image.id;
	//	cerr << sql.query() << "\n";
		sql.send_query();
		//delete old copy of the file
		image_server_const.image_storage.delete_from_storage(visualization_image,ns_delete_both_volatile_and_long_term,&sql);
	}
	sql.send_query("COMMIT");
	source_image.processed_output_storage = &visualization_image;
	const float mask_resolution(analyze_mask(source_image,resize_factor,mask_id,sql));
	return mask_resolution*resize_factor;
}

struct ns_mask_region_sorter_element{
	unsigned long mask_region_id,
				  mask_value;
	ns_vector_2i average,
				 pos,
				 size;
	string name;
	bool operator()(const ns_mask_region_sorter_element &l, const ns_mask_region_sorter_element & r){
		if (l.average.y == r.average.y) return (l.average.x < l.average.x);
		return l.average.y < r.average.y;
	}
};
void ns_image_processing_pipeline::generate_sample_regions_from_mask(ns_64_bit sample_id, const float capture_sample_image_resolution_in_dpi,ns_sql & sql){
	
	image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Analyzing Regions for sample ") << sample_id,&sql);
	sql << "SELECT mask_id, image_resolution_dpi FROM capture_samples WHERE id = " << sample_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_processing_pipeline::generate_sample_regions_from_mask()::Could not find sample id ") << sample_id << " in database";
	if (res[0][0] == "0")
		throw ns_ex("ns_image_processing_pipeline::generate_sample_regions_from_mask()::sample id ") << sample_id << " has no mask specified";
	string mask_id = res[0][0];
	const float db_resolution_spec (atof(res[0][1].c_str()));
	if (db_resolution_spec != capture_sample_image_resolution_in_dpi)
		throw ns_ex("The database specification of the sample resolution: ") << db_resolution_spec << " does not match that of the mask: " << capture_sample_image_resolution_in_dpi;
	
	sql << "SELECT id, mask_value, x_min, y_min, x_max, y_max, x_average, y_average FROM image_mask_regions WHERE mask_id = " << mask_id << " ORDER BY mask_value ASC";
	sql.get_rows(res);
	vector<ns_mask_region_sorter_element> mask_regions(res.size());
	std::map<unsigned long, ns_mask_region_sorter_element *> mask_finder;

	//sort regions top to boytom based on their positions in the mask
	for (unsigned long i = 0; i < res.size(); i++){
		mask_regions[i].mask_region_id = ns_atoi64(res[i][0].c_str());
		mask_regions[i].mask_value = atol(res[i][1].c_str());
		mask_regions[i].pos.x = atol(res[i][2].c_str());
		mask_regions[i].pos.y = atol(res[i][3].c_str());
		mask_regions[i].size.x = atol(res[i][4].c_str()) - mask_regions[i].pos.x;
		mask_regions[i].size.y = atol(res[i][5].c_str()) - mask_regions[i].pos.y;
		mask_regions[i].average.x = atol(res[i][6].c_str());
		mask_regions[i].average.y = atol(res[i][7].c_str());
		mask_finder[mask_regions[i].mask_region_id] = &mask_regions[i];
	}
	std::sort(mask_regions.begin(),mask_regions.end(),ns_mask_region_sorter_element());
	for (unsigned long i = 0; i < res.size(); i++){
		mask_regions[i].name = ns_to_string(i);
	}
	
	//check to see if any existing regions match the current mask
	//if there is one pre-existing, use it.  Otherwise, create a new one.
	for (unsigned long i = 0; i < mask_regions.size(); i++){
		sql << "SELECT id FROM sample_region_image_info WHERE mask_id = " <<mask_id << " AND mask_region_id = " << mask_regions[i].mask_region_id;
		ns_sql_result res2;
		sql.get_rows(res2);
		//if the region already exists, don't change its info, as this would change filenames of images 
		//associated with that region
		if (res2.size() > 1)
			throw ns_ex("ns_image_processing_pipeline::generate_sample_regions_from_mask()::Multiple regions match a single mask area!");
		if (res2.size() == 0){
			sql << "INSERT INTO sample_region_image_info SET "
				<< "mask_region_id = " << mask_regions[i].mask_region_id 
				<< ", mask_id = " << mask_id << ","
				<< "sample_id = " << sample_id 
				<< ", name = '" << mask_regions[i].name << "'"
				<< ", position_in_sample_x = '" << mask_regions[i].pos.x/capture_sample_image_resolution_in_dpi << "'"
				<< ", position_in_sample_y = '" << mask_regions[i].pos.y/capture_sample_image_resolution_in_dpi << "'"
				<< ", size_x = '" << mask_regions[i].size.x/capture_sample_image_resolution_in_dpi << "'"
				<< ", size_y = '" << mask_regions[i].size.y/capture_sample_image_resolution_in_dpi << "'"
				<< ", details = '',"
				"reason_censored = '',"
				"strain_condition_1 = '',"
				"strain_condition_2 = '',"
				"strain_condition_3 = '',"
				"culturing_temperature = '',"
				"experiment_temperature = '',"
				"food_source = '',"
				"environmental_conditions = '',"
				"posture_analysis_model = '',"
				"posture_analysis_method = '',"
				"worm_detection_model = '',"
				"position_analysis_model = '',"
				"strain=''";

			sql.send_query();
		}
	}
	//look for regions that do not match any mask region, perhaps left over from previous masks
	sql << "SELECT id, mask_region_id FROM sample_region_image_info WHERE sample_id = " << sample_id;
	sql.get_rows(res);
	for (unsigned int i = 0; i < res.size(); i++){
		unsigned long region_id = ns_atoi64(res[i][0].c_str());
		if (mask_finder.find(region_id) == mask_finder.end()){
			ns_processing_job job;
			job.region_id = region_id;
			ns_handle_image_metadata_delete_action(job,sql);
		}
	}
	sql.send_query("COMMIT");
}
	
	///analyze_mask assumes the specified image is a mask containing region information.  Each region is represented by a different
	///color.  analyze_mask calculates the number of regions specified in the mask, calculates their statistics (center of mass, etc)
	///and makes a visualzation of the regions to allow easy verification of mask correctness
float ns_image_processing_pipeline::analyze_mask(ns_image_server_image & image, const unsigned int resize_factor, const ns_64_bit mask_id, ns_sql & sql){

	image.load_from_db(image.id,&sql);
	image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Analyzing Mask ") << image.filename,&sql);
	//cerr << "requesting mask";
	//acquire the image
	ns_image_storage_source_handle<ns_component> mask = image_server_const.image_storage.request_from_storage(image,&sql);
	//cerr << "done.";
	mask_analyzer.set_resize_factor(resize_factor);
	const float mask_resolution(mask.input_stream().properties().resolution);
	//analyze mask
	if (image.processed_output_storage == 0) //visualization not requested
		mask.input_stream().pump(mask_analyzer,_image_chunk_size);
	else{
		//make visualization
		ns_image_whole<ns_component> visualization;
		mask_analyzer.register_visualization_output(visualization);
		mask.input_stream().pump(mask_analyzer,_image_chunk_size);

		ns_image_whole<ns_component> *out = &visualization;
		ns_image_whole<ns_component> visualization_small;
		if (visualization.properties().width > 800){
			float r = 800.0f/visualization.properties().width;
			ns_image_properties nprop=visualization.properties();
			nprop.width=(unsigned long)(nprop.width*r);
			nprop.height=(unsigned long)(nprop.height*r);
			visualization.resample(nprop,visualization_small);
			out = &visualization_small;
		}

		ns_image_server_image output_image = image.create_storage_for_processed_image(ns_process_analyze_mask,ns_tiff,&sql);
		bool had_to_use_local_storage;
		ns_image_storage_reciever_handle<ns_component> visualization_output = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION,_image_chunk_size,&sql,
																had_to_use_local_storage,
																false,
																false);
		out->pump(visualization_output.output_stream(),_image_chunk_size);
		image.processed_output_storage->mark_as_finished_processing(&sql);
		output_image.mark_as_finished_processing(&sql);
	}

	//store mask regions database
	mask_analyzer.mask_info().save_to_db(mask_id,sql);

	sql.send_query("COMMIT");
	return mask_resolution;
}

	///used for debugging; frees all memory stored on the heap.
void ns_image_processing_pipeline::clear_heap(){
	ns_image_properties null(0,0,0);
	mask_splitter.clear_heap();
	spatial_averager.init(null);

	dynamic_stretch.init(null);
	spatial_average.init(null);
	temporary_image.init(null);

	threshold_applier.init(null);
	mask_analyzer.init(null);

	temporary_image.clear();	
	spatial_average.clear();
	dynamic_stretch.clear();
}


//this is almost always done elsewhere, as part of a movement analysis job!
void ns_image_processing_pipeline::calculate_static_mask_and_heat_map(const vector<char> operations, ns_image_server_captured_image_region & region_image, ns_sql & sql){
	throw ns_ex("Not implemented: run as a movement analysis job");
	region_image.load_from_db(region_image.region_images_id,&sql);

	ns_worm_multi_frame_interpolation mfi;
	//mfi.clear_previous_interpolation_results(region_image.region_info_id,sql);

	bool calculate_heat_map = operations[(int)ns_process_heat_map] != 0;
	
	bool generate_static_mask = operations[(int)ns_process_static_mask] != 0;

	

		ns_image_standard heat_map,
						  static_mask;
		
		bool had_to_use_local_storage;
	if (generate_static_mask){
		//delete all previously computedprocessing results
	
		//try to load the heat map from disk.
		if (!calculate_heat_map){
			try{
				ns_image_server_image heat_map_im(region_image.request_processed_image(ns_process_heat_map,sql));
				
				ns_image_storage_source_handle<ns_component> heat_map_source(image_server_const.image_storage.request_from_storage(heat_map_im,&sql));
				heat_map_source.input_stream().pump(heat_map,512);
			}
			//if loading from disk fails, make sure to calculate the heat map from scratch.
			catch(ns_ex & ex){
				image_server_const.add_subtext_to_current_event(ex.text() + "\n", &sql);
				calculate_heat_map = true;
			}
		}
	}
	
	ns_high_precision_timer tm;
	//calculate heat map
	if (calculate_heat_map){
		sql << "SELECT number_of_frames_used_to_mask_stationary_objects FROM sample_region_image_info WHERE id = " << region_image.region_info_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_image_processing_pipeline::calculate_static_mask_and_heat_map::Could not load region information for region ") << region_image.region_info_id;
		const unsigned long number_of_frames_used_to_mask_stationary_objects(atol(res[0][0].c_str()));

		register_event(ns_process_heat_map,ns_image_server_event(),false,sql);
		tm.start();
		mfi.load_all_region_worms(region_image.region_info_id,sql,false);
		mfi.generate_heat_map(heat_map,number_of_frames_used_to_mask_stationary_objects,sql);
		//save the heat map to disk
		ns_image_server_image a_vis = region_image.create_storage_for_processed_image(ns_process_heat_map,ns_tiff,&sql);
		ns_image_storage_reciever_handle<ns_component> a_vis_o = image_server_const.image_storage.request_storage(
													a_vis,
													ns_tiff, 1.0, _image_chunk_size,&sql,had_to_use_local_storage,false,false);
		heat_map.pump(a_vis_o.output_stream(),_image_chunk_size);
		a_vis.mark_as_finished_processing(&sql);
		image_server.register_job_duration(ns_process_heat_map,tm.stop());
	
	}
	if (generate_static_mask){
		
		register_event(ns_process_static_mask,ns_image_server_event(),false,sql);
		tm.start();
		ns_worm_multi_frame_interpolation::generate_static_mask_from_heatmap(heat_map,static_mask);

		//save the static mask to disk
		ns_image_server_image b_vis = region_image.create_storage_for_processed_image(ns_process_static_mask,ns_tiff,&sql);
		ns_image_storage_reciever_handle<ns_component> b_vis_o = image_server_const.image_storage.request_storage(
												b_vis,
												ns_tiff, 1.0, _image_chunk_size,&sql,had_to_use_local_storage,false,false);
		static_mask.pump(b_vis_o.output_stream(),_image_chunk_size);
		b_vis.mark_as_finished_processing(&sql);
		image_server.register_job_duration(ns_process_static_mask,tm.stop());

	}
	sql << "UPDATE sample_region_images SET currently_under_processing=0 WHERE region_info_id = " << region_image.region_info_id;
	sql.send_query();
	sql << "UPDATE worm_movement SET problem=0,calculated=0 WHERE region_info_id = " << region_image.region_info_id;
	sql.send_query();
	sql.send_query("COMMIT");

	if (generate_static_mask){
		ns_image_server_automated_job_scheduler::register_static_mask_completion(region_image.region_images_id,sql);
	}
}

///Creates a time-lapse video of the specified region.  A video is made for each of the specified processing steps.
#ifndef NS_NO_XVID
void ns_image_processing_pipeline::compile_video(ns_image_server_captured_image_region & region_image, const vector<char> operations,const ns_video_region_specification & region_spec, ns_sql & sql){
	unsigned long start_time = ns_current_time();
	//load the region image to get region information
	region_image.load_from_db(region_image.region_images_id,&sql);
	

	//create a sample image to get output path information
	ns_file_location_specification spec(image_server_const.image_storage.get_path_for_experiment(region_image.experiment_id,&sql));
	string output_path = image_server_const.image_storage.get_absolute_path_for_video(spec,false);
	
	string relative_path(image_server_const.image_storage.get_relative_path_for_video(spec,false));

	ns_dir::create_directory_recursive(output_path);
	sql << "SELECT apply_vertical_image_registration FROM capture_samples WHERE id=" << region_image.sample_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_processing_pipeline::compile_video::Could not load sample information for specified job.");

	const bool apply_vertical_registration(res[0][0] != "0");

	//go through and create all requested movies
	for (unsigned int i = 0; i < (unsigned int)ns_process_last_task_marker; i++){		
		string relative_path_f(relative_path);
		if (!operations[i])
			continue;
		if (i == ns_process_compile_video || 
			i == ns_process_add_to_training_set || 
			i == ns_process_analyze_mask || 
			i == ns_process_compile_video || 
			i == ns_process_apply_mask)
			continue;
		//cerr << "Compiling " << ns_processing_task_to_string((ns_processing_task)i) << " (" << i << ")";
		string process_output_path = output_path;
		if (ns_processing_step_directory_d((ns_processing_task)i).size() != 0){
			process_output_path += string(DIR_CHAR_STR);
			process_output_path +=ns_processing_step_directory_d((ns_processing_task)i);

			relative_path_f += string(DIR_CHAR_STR);
			relative_path_f +=ns_processing_step_directory_d((ns_processing_task)i);

			ns_dir::create_directory_recursive(process_output_path);
		}

		try{
			sql << "SELECT images.path, images.filename, sample_region_images.capture_time  FROM images, sample_region_images WHERE images.id = sample_region_images.";
			sql << ns_processing_step_db_column_name(i);
			sql << " AND sample_region_images.region_info_id = " << region_image.region_info_id;
			if(region_spec.start_time != 0)
				sql << " AND sample_region_images.capture_time >= " << region_spec.start_time;
			if(region_spec.stop_time != 0)
				sql << " AND sample_region_images.capture_time <= " << region_spec.stop_time;
			sql << " AND sample_region_images.problem = 0 AND sample_region_images.censored = 0";
			sql << " ORDER BY sample_region_images.capture_time ASC";
			res.resize(0);
			sql.get_rows(res);

			if (res.size() == 0){
				image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Processing step ") << ns_processing_task_to_string((ns_processing_task)i) << " yielded no images.",&sql);
				continue;
			}
			vector<string> filenames;
			filenames.reserve(res.size());

			string o_filename = region_image.experiment_name + "=" + region_image.sample_name + "=" + region_image.region_name + "=" + ns_processing_step_directory_d((ns_processing_task)i);
			if (region_spec.is_specified()){
				o_filename += "=(";
				o_filename += ns_to_string(region_spec.position_x) + "-" + ns_to_string(region_spec.width) + ",";
				o_filename += ns_to_string(region_spec.position_y) + "-" + ns_to_string(region_spec.height) + ")";
			}

			string output_basename = process_output_path + DIR_CHAR_STR + o_filename;
			ns_dir::convert_slashes(output_basename);


			ns_image_server_event ev("ns_image_processing_pipeline::Compiling video ");
			ev << region_image.experiment_name << "::" << ns_processing_task_to_string((ns_processing_task)i) << "::" 
				<< "(" << (unsigned int)res.size() << " Frames)::"<< output_basename;
			ev.specifiy_event_subject(region_image);
			ev.specify_processing_job_operation(ns_process_compile_video);
			ns_64_bit event_id = image_server_const.register_server_event(ev,&sql);


			ns_video_region_specification reg(region_spec);
			//labels already provided
			if ((i == (int)ns_process_worm_detection || i == (int)ns_process_worm_detection_labels) && reg.timestamp_type == ns_video_region_specification::ns_date_timestamp)
				reg.timestamp_type = ns_video_region_specification::ns_no_timestamp;
			std::vector<ns_vector_2i> registration_offsets;
			//registration_offsets.resize(1,ns_vector_2i(0,0));
			ns_image_processing_pipeline::make_video(region_image.experiment_id,res,reg,registration_offsets,output_basename,sql);
			
				
			ns_image_server_image video_info;
			video_info.path = relative_path_f;
			video_info.partition = image_server_const.image_storage.get_partition_for_experiment(region_image.experiment_id,&sql,true);
			video_info.host_id = image_server_const.host_id();
			video_info.filename = o_filename;

			video_info.save_to_db(0,&sql);
			sql << "UPDATE sample_region_image_info SET op" << i << "_video_id = " << video_info.id << " WHERE id = " << region_image.region_info_id;
			sql.send_query();
		}
		catch(ns_ex & ex){
			image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::compile_video()::") << ex.text(),&sql);
		}
	}
}


///Creates a time-lapse video of the specified sample.  A video is made for each of the specified processing steps.
void ns_image_processing_pipeline::compile_video(ns_image_server_captured_image & sample_image, const vector<char> operations, const ns_video_region_specification & region_spec, ns_sql & sql){
	unsigned long start_time = ns_current_time();
	//load the region image to get region information
	sample_image.load_from_db(sample_image.captured_images_id,&sql);

	ns_file_location_specification spec(image_server_const.image_storage.get_path_for_experiment(sample_image.experiment_id,&sql));
	string output_path = image_server_const.image_storage.get_absolute_path_for_video(spec,true);
	string relative_path = image_server_const.image_storage.get_relative_path_for_video(spec,true);
	ns_dir::create_directory_recursive(output_path);

	//go through and create all requested movies
	for (unsigned int i = 0; i < (unsigned int)ns_process_last_task_marker; i++){			
		if (!operations[i])
			continue;
		if (i == ns_process_compile_video || 
			i == ns_process_add_to_training_set || 
			i == ns_process_analyze_mask || 
			i == ns_process_compile_video || 
			i == ns_process_apply_mask)
			continue;
		//cerr << "Compiling " << ns_processing_task_to_string((ns_processing_task)i) << " (" << i << ")";
		ns_sql_result res;
		try{
			//get the filename for each file in the sample
			
			sql << "SELECT si.path, si.filename, i.path,i.filename, ci.capture_time  FROM captured_images AS ci "
				<< "LEFT JOIN images AS si ON (ci.small_image_id = si.id) "
				<< "LEFT JOIN images AS i ON (ci.image_id = i.id) "
				<< "WHERE "
				<< "ci.sample_id = " << sample_image.sample_id;
			if(region_spec.start_time != 0)
				sql << " AND ci.capture_time >= " << region_spec.start_time;
			if(region_spec.stop_time != 0)
				sql << " AND ci.capture_time <= " << region_spec.stop_time;
			sql << " AND ci.problem = 0 AND ci.censored = 0";
			sql << " ORDER BY ci.capture_time ASC";
			res.resize(0);
			sql.get_rows(res);

			ns_sql_result files_to_compile(res.size(),std::vector<std::string>(3));
			unsigned long height(0);
			for (unsigned int j = 0; j < res.size(); j++){
				//try to use small copies if available
				if (res[j][0] != ""){
					files_to_compile[height][0] = res[j][0];
					files_to_compile[height][1] = res[j][1];
				}
				//otherwise try to use the large version
				else{
					if(res[j][2] == "")
						continue;
					files_to_compile[height][0] = res[j][2];
					files_to_compile[height][1] = res[j][3];
				}
				files_to_compile[height][2] = res[j][4];
				height++;	
			}
			files_to_compile.resize(height);

			if (files_to_compile.size() == 0){
				image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Sample yielded no images."),&sql);
				continue;
			}
			

			string o_filename = sample_image.experiment_name + "=" + sample_image.sample_name;
			
			if (region_spec.is_specified()){
				o_filename += "=(";
				o_filename += ns_to_string(region_spec.position_x) + "-" + ns_to_string(region_spec.width) + ",";
				o_filename += ns_to_string(region_spec.position_y) + "-" + ns_to_string(region_spec.height) + ")";
			}


			string output_basename = output_path + DIR_CHAR_STR + o_filename;
			ns_dir::convert_slashes(output_basename);


			ns_image_server_event ev("ns_image_processing_pipeline::Compiling video ");
			ev << sample_image.experiment_name << "::" 
				<< "(" << (unsigned int)files_to_compile.size() << " Frames)::"<< output_basename;
			ev.specifiy_event_subject(sample_image);
			ev.specify_processing_job_operation(ns_process_compile_video);

			unsigned long event_id = image_server_const.register_server_event(ev,&sql);


			//calculate registration information if required
			vector<ns_vector_2i> registration_offsets;
			make_video(sample_image.experiment_id,files_to_compile,region_spec,registration_offsets,output_basename,sql);
			
			ns_image_server_image video_info;
			video_info.path = relative_path;
			video_info.partition = image_server_const.image_storage.get_partition_for_experiment(sample_image.experiment_id,&sql,true);
			video_info.host_id = image_server_const.host_id();
			video_info.filename = o_filename;

			video_info.save_to_db(0,&sql);
			sql << "UPDATE capture_samples SET op" << i << "_video_id = " << video_info.id << " WHERE id = " << sample_image.sample_id;
			sql.send_query();
		}
		catch(ns_ex & ex){
			image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::compile_video()::") << ns_processing_task_to_string((ns_processing_task)i) << "::" << ex.text(),&sql);
		}
	}
}
///Creates a time-lapse video of the specified sample.  A video is made for each of the specified processing steps.
void ns_image_processing_pipeline::compile_video_experiment(ns_image_server_captured_image & sample_image, const vector<char> operations, ns_sql & sql){
}
#endif

void ns_image_processing_pipeline::wrap_m4v_stream(const string & m4v_filename, const string & output_basename, const long number_of_frames,const bool for_ppt,ns_sql & sql){
	//we have now produced a raw mpeg4 stream.
	//compile it into mp4 movies at four different frame_rates
	for (unsigned int j = 0; j < 5; j++){
		
		string output,
			   error_output;
		
		string fps;
		switch(j){
			case 0: fps="0.5"; break;
			case 1: fps="1"; break;
			case 2: fps="5"; break;
			case 3: fps="10"; break;
			case 4: fps="30"; break;
		}
		cout << fps << "fps...";
		string vid_filename = output_basename + "=" + fps + "fps";
	//if (!for_ppt)
			vid_filename += ".mp4";
	//	else vid_filename += ".wmv";
		ns_dir::delete_file(vid_filename);
		string param;
		//if (!for_ppt)
		param = image_server_const.video_compilation_parameters(m4v_filename,vid_filename,number_of_frames,fps,sql);
		//else param = "-i " + m4v_filename + " -vcodec wmv2 -sameq -r " + fps + " " + vid_filename;
		ns_external_execute exec;
		//if (!for_ppt)	
		//cerr << "Running " << image_server_const.video_compiler_filename() + " " + param << "\n";
		ns_external_execute_options opt;
		opt.binary = true;
		exec.run(image_server_const.video_compiler_filename(), param, opt);
		//else exec.run(image_server_const.video_ppt_compiler_filename(), param, false,true);
	
		exec.release_io();
		exec.wait_for_termination();

		#ifdef _WIN32 
		//x264 likes to mess with the console window title, so we have to set it back
		image_server_const.set_console_window_title();
		#endif
	}
	cout << "\n";
}
#ifndef NS_NO_XVID
void ns_image_processing_pipeline::make_video(const unsigned long experiment_id, const vector< vector<string> > path_and_filenames, const ns_video_region_specification & region_spec, const vector<ns_vector_2i> registration_offsets, const string &output_basename, ns_sql & sql){
	
	ns_xvid_encoder xvid;	
	vector<string> filenames;
	vector<string> labels;
	filenames.reserve(path_and_filenames.size());
	labels.reserve(path_and_filenames.size());
	if (path_and_filenames.size() > 0 && region_spec.timestamp_type != ns_video_region_specification::ns_no_timestamp && path_and_filenames[0].size() < 3)
		throw ns_ex("ns_image_processing_pipeline::make_vide()::Timestamp requested, but no capture times provided.");

	for (unsigned int j = 0; j < path_and_filenames.size(); j++){
		string fn = image_server_const.image_storage.get_absolute_path_for_video_image(experiment_id,path_and_filenames[j][0],path_and_filenames[j][1],sql); 
		ns_dir::convert_slashes(fn);
		if (ns_dir::file_exists(fn))
			filenames.push_back(fn);
		else image_server_const.register_server_event(ns_image_server_event("ns_xvid::Could not load frame from ") << fn,&sql);
		switch(region_spec.timestamp_type){
			case ns_video_region_specification::ns_no_timestamp: break;
			case ns_video_region_specification::ns_date_timestamp:
				labels.push_back(ns_format_time_string_for_human(atol(path_and_filenames[j][2].c_str()))); break;
			case ns_video_region_specification::ns_age_timestamp:
				labels.push_back(ns_to_string_short((atol(path_and_filenames[j][2].c_str())-region_spec.time_at_which_population_had_zero_age)/60.0/60.0/24.0,2)); break;
				break;
		default: throw ns_ex("Unfamiliar label request!");
		}
	}

	if (filenames.size() == 0){
		image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::Processing step yielded images, but none could be opened."),&sql);
	}

	ns_xvid_parameters param = ns_xvid_encoder::default_parameters();

	param.max_dimention=1200;
	param.ARG_MAXKEYINTERVAL=12;
	param.ARG_FRAMERATE=5;
	
	string output_filename =  output_basename + ".m4v";

	xvid.run(filenames,param,output_filename,region_spec,labels,registration_offsets);
	wrap_m4v_stream(output_filename,output_basename,filenames.size(),false,sql);
		
}
#endif
	
///Takes the image and applies the appropriate mask to make a series of region images.  The resulting images are saved to disk
///and annotated in the database.

void ns_image_processing_pipeline::resize_sample_image(ns_image_server_captured_image & captured_image, ns_sql & sql){
	captured_image.load_from_db(captured_image.captured_images_id,&sql);
	ns_image_server_event ev("ns_image_processing_pipeline::Creating resized capture image ");
	ev << captured_image.experiment_name << "::" << captured_image.sample_name << "::" << captured_image.capture_time;
	image_server_const.register_server_event(ev,&sql);
	ns_high_precision_timer timer;
	timer.start();
//	image_server_const.performance_statistics.starting_job(ns_process_thumbnail);
	try{
		captured_image.load_from_db(captured_image.captured_images_id,&sql);

		ns_image_server_image small_image(captured_image.make_small_image_storage(&sql));
		bool had_to_use_volatile_storage;
		ns_image_storage_reciever_handle<ns_8_bit> small_image_output(image_server_const.image_storage.request_storage(small_image,ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, 1024,&sql,had_to_use_volatile_storage,false,false));
		

		ns_resampler<ns_8_bit> resampler(_image_chunk_size);
		resampler.set_maximum_dimentions(ns_image_server_captured_image::small_image_maximum_dimensions());
		
		ns_image_stream_binding< ns_resampler<ns_8_bit>, ns_image_storage_reciever<ns_8_bit> > 
			resampler_binding(resampler,small_image_output.output_stream(),_image_chunk_size);
		
		ns_image_server_image source_image;
		source_image.id = captured_image.capture_images_image_id;
		ns_image_storage_source_handle<ns_component> source(image_server_const.image_storage.request_from_storage(source_image,&sql));
		source.input_stream().pump(resampler_binding,_image_chunk_size);

		
		small_image.save_to_db(0,&sql);
		sql << "UPDATE captured_images SET small_image_id = " << small_image.id << " WHERE id = " << captured_image.captured_images_id;
		sql.send_query();

		image_server.register_job_duration(ns_process_thumbnail, timer.stop());
	}
	catch(...){
		//image_server_const.performance_statistics.cancel_outstanding_jobs();
		throw;
	}
}


void ns_image_processing_pipeline::resize_region_image(ns_image_server_captured_image_region & region_image,ns_sql & sql){
	region_image.load_from_db(region_image.region_images_id,&sql);
	//ns_image_server_event ev("ns_image_processing_pipeline::Creating resized region image ");

	//ev << region_image.experiment_name << "::" << region_image.sample_name << "::" << region_image.region_name << ":: " << region_image.capture_time;
	//image_server_const.register_server_event(ev,&sql);

	ns_image_server_image small_image(region_image.create_storage_for_processed_image(ns_process_thumbnail,ns_jpeg,&sql));
	
	bool had_to_use_volatile_storage;
	ns_image_storage_reciever_handle<ns_8_bit> small_image_output(image_server_const.image_storage.request_storage(small_image,ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, 1024,&sql,had_to_use_volatile_storage,false,false));
	

	ns_resampler<ns_8_bit> resampler(_image_chunk_size);
	resampler.set_maximum_dimentions(ns_image_server_captured_image_region::small_image_maximum_dimensions());
	
	ns_image_stream_binding< ns_resampler<ns_8_bit>, ns_image_storage_reciever<ns_8_bit> > 
		resampler_binding(resampler,small_image_output.output_stream(),_image_chunk_size);
	
	ns_image_server_image source_image;
	source_image.id = region_image.region_images_image_id;
	ns_image_storage_source_handle<ns_component> source(image_server_const.image_storage.request_from_storage(source_image,&sql));
	source.input_stream().pump(resampler_binding,_image_chunk_size);

	small_image.mark_as_finished_processing(&sql);

}
void ns_image_processing_pipeline::apply_mask(ns_image_server_captured_image & captured_image, 
		vector<ns_image_server_captured_image_region> & output_regions, ns_sql & sql){
	
	output_regions.resize(0);	
	ns_high_precision_timer timer;
	timer.start();
	
	try {
		ns_image_server_event ev("ns_image_processing_pipeline::Applying Mask");
		ev << " on " << captured_image.filename(&sql);
		ev.specifiy_event_subject(captured_image);
		ev.specify_processing_job_operation(ns_process_apply_mask);
		ns_64_bit event_id = image_server_const.register_server_event(ev, &sql);

		captured_image.load_from_db(captured_image.captured_images_id, &sql);
		bool delete_captured_image(false);
		{
			sql << "SELECT delete_captured_images_after_mask FROM experiments WHERE id = " << captured_image.experiment_id;
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() != 1)
				throw ns_ex("ns_image_processing_pipeline::apply_mask()::Could not load experiment data from db (") << captured_image.experiment_id << ")";
			delete_captured_image = res[0][0] != "0";
			if (delete_captured_image) {
				sql << "SELECT first_frames_are_protected FROM capture_samples WHERE id=" << captured_image.sample_id;
				sql.get_rows(res);

				if (res.size() == 0)
					throw ns_ex("ns_image_processing_pipeline::apply_mask()::Could not find sample for specified captured image!") << captured_image.sample_id;

				if (res[0][0] == "0") {
					delete_captured_image = false;
					image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::apply_mask()::Since no images in this sample have been protected from deletion, this capture image will not be deleted."), &sql);
				}
				if (delete_captured_image && captured_image.capture_images_small_image_id == 0) {
					image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::apply_mask()::Attempting to create a small resized copy of the image before masking"), &sql);
					try {
						resize_sample_image(captured_image, sql);
					}
					catch (ns_ex & ex_) {
						delete_captured_image = false;
						throw ns_ex("While masking::") << ex_.text() << ex_.type();
					}
					catch (...) {
						delete_captured_image = false;
						throw;
					}
				}
				if (captured_image.never_delete_image) {
					delete_captured_image = false;
					image_server_const.register_server_event(ns_image_server_event("ns_image_processing_pipeline::apply_mask()::Because this capture image is marked \"Never Delete\", it will not be deleted."), &sql);
				}
			}
		}


		unsigned long start_time = ns_current_time();
		//get mask info from db
		sql << "SELECT capture_samples.mask_id, image_masks.image_id, capture_samples.apply_vertical_image_registration,image_masks.resize_factor FROM capture_samples, image_masks WHERE image_masks.id = capture_samples.mask_id AND capture_samples.id = " << captured_image.sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_image_processing_pipeline::Specified sample does not exist in database during mask application.");
		if (res[0][0] == "" || res[0][0] == "0")
			throw ns_ex("ns_image_processing_pipeline::Specified sample '") << captured_image.sample_name << "'(" << captured_image.sample_id << ") does have mask set.";
		unsigned long mask_id = ns_atoi64(res[0][0].c_str()),
			mask_image_id = ns_atoi64(res[0][1].c_str()),
			apply_vertical_image_registration = atol(res[0][2].c_str());
		//	if (apply_vertical_image_registration)
		//		cerr << "Vertical registration requested\n";
		//	else "Vertical registration not requested\n";
		unsigned long resize_factor = atol(res[0][3].c_str());

		//cerr << "Using Resize Factor " << resize_factor << "\n";
		mask_splitter.set_resize_factor(resize_factor);


		//load captured image that will be masked
		//ns_image_whole<ns_component> source_im;
		ns_acquire_for_scope<ns_registration_disk_buffer> new_image_buffer;
		ns_image_server_image source_image;
		source_image.id = captured_image.capture_images_image_id;



		ns_vector_2i offset = ns_vector_2i(0, 0);
		ns_image_fast_registration_profile_cache::const_handle_t requested_image, reference_image;
		ns_image_fast_registration_profile_data_source profile_data_source;
		profile_data_source.sql = &sql;
		profile_data_source.image_storage = &image_server_const.image_storage;

		if (apply_vertical_image_registration) {
			image_server_const.add_subtext_to_current_event(ns_image_server_event("Loading images..."), &sql);
			
			get_reference_image(captured_image, reference_image, profile_data_source);
			ns_image_server_image im;
			im.id = captured_image.capture_images_image_id;
			image_server.image_registration_profile_cache.get_unlinked_singleton(im, requested_image, profile_data_source);

			image_server_const.add_subtext_to_current_event(ns_image_server_event("Aligning sample image to reference image..."), &sql);
			offset = get_vertical_registration(reference_image, requested_image, profile_data_source);
			reference_image.release();
			cerr << offset << " ";

			sql << "UPDATE captured_images SET registration_horizontal_offset='" << offset.x << "', registration_vertical_offset='" << offset.y << "', registration_offset_calculated=1 WHERE id = " << captured_image.captured_images_id;
			sql.send_query();
		}
		else {
			ns_image_server_image im;
			im.id = captured_image.capture_images_image_id;	
			image_server.image_registration_profile_cache.get_unlinked_singleton(im, requested_image, profile_data_source);
		}

		mask_splitter.mask_info()->load_from_db(mask_id, sql);

		ns_image_server_image mask_image_info;
		if (!mask_image_info.load_from_db(mask_image_id, &sql))
			throw ns_ex("ns_image_processing_pipeline::Mask ") << mask_id << " has no image specified when applying mask.";

	
		//obtain output streams for split regions.
		sql << "SELECT sample_region_image_info.id, sample_region_image_info.name, image_mask_regions.id, image_mask_regions.mask_value FROM image_mask_regions, sample_region_image_info WHERE sample_region_image_info.mask_region_id = image_mask_regions.id AND image_mask_regions.mask_id= '" << mask_id << "'";
		sql.get_rows(res);
		//the record in the sample_region_image table for each new masked region

		//the record in the images table for each masked region
		vector<ns_image_server_image> output_images;
		unsigned long mask_info_size = mask_splitter.mask_info()->size();
		for (unsigned int i = 0; i < res.size(); i++) {
			unsigned int	mask_region_info_id = ns_atoi64(res[i][0].c_str());
			unsigned int 	mask_region_id = ns_atoi64(res[i][2].c_str());
			int				mask_region_value = ns_atoi64(res[i][3].c_str()); // if it could accidentally be < 0 (see below) it ought to be an int
			string			mask_region_name = res[i][1];

			if (mask_region_value < 0 || mask_region_value > mask_info_size)
				throw ns_ex("ns_image_processing_pipeline::Invalid mask value specified in mask.");
			output_regions.resize(output_regions.size() + 1);
			ns_image_server_captured_image_region & new_region_image(output_regions[output_regions.size() - 1]);

			sql.send_query("BEGIN");
			//see if the region has already been created;
			sql << "SELECT id, image_id FROM sample_region_images WHERE region_info_id = '" << mask_region_info_id << "' AND capture_time = '" << captured_image.capture_time << "'";
			ns_sql_result out;
			sql.get_rows(out);
			ns_image_server_image output_image;
			output_image.id = 0;
			//if a record in the sample_region_images table exists, and if so, whether a record in the images table exists as well.
			if (out.size() != 0) {
				//delete the previous file
				output_image.id = ns_atoi64(out[0][1].c_str());
				//	if (output_image.id != 0)
				//			image_server_const.image_storage.delete_from_storage(output_image,sql);
					//we can use the old image; its filename must be correct.
					//we can also use the existing region_image record.
				new_region_image.region_images_id = ns_atoi64(out[0][0].c_str());
				new_region_image.region_images_image_id = output_image.id;
				new_region_image.region_info_id = mask_region_info_id;
				new_region_image.capture_time = captured_image.capture_time;
				new_region_image.mask_color = mask_region_value;
				sql << "UPDATE sample_region_images SET last_modified=" << ns_current_time() <<
					", capture_sample_image_id = " << captured_image.captured_images_id << ", currently_under_processing=1, vertical_image_registration_applied = " << apply_vertical_image_registration << " WHERE id=" << new_region_image.region_images_id;
				sql.send_query();
				//XXX NOTE: Here we might want to delete all previously calculated processing jobs from the record

			}
			//create new record in the sample region images table
			else {

				sql << "INSERT INTO sample_region_images SET region_info_id = '" << mask_region_info_id <<
					"', capture_time = '" << captured_image.capture_time << "', last_modified=" << ns_current_time() << ", " <<
					" capture_sample_image_id = " << captured_image.captured_images_id << ", currently_under_processing=1, vertical_image_registration_applied = " << apply_vertical_image_registration << " ";

				new_region_image.region_images_id = sql.send_query_get_id();
				sql.send_query("COMMIT");
			}
			new_region_image.region_info_id = mask_region_info_id;
			new_region_image.captured_images_id = captured_image.captured_images_id;
			new_region_image.sample_id = captured_image.sample_id;
			new_region_image.experiment_id = captured_image.experiment_id;
			new_region_image.capture_images_image_id = captured_image.capture_images_image_id;
			new_region_image.capture_time = captured_image.capture_time;
			new_region_image.sample_name = captured_image.sample_name;
			new_region_image.device_name = captured_image.device_name;
			new_region_image.experiment_name = captured_image.experiment_name;
			new_region_image.mask_color = mask_region_value;

			//if a record in the images table hasn't been made, make it.
			if (output_image.id == 0) {
				//create a new image for the region_image image
				output_image.partition = image_server_const.image_storage.get_partition_for_experiment(new_region_image.experiment_id, &sql);
				output_image.path = new_region_image.directory(&sql);
				output_image.filename = new_region_image.filename(&sql) + ".tif";
				sql << "INSERT INTO images SET filename = '" << sql.escape_string(output_image.filename) << "', path = '" << sql.escape_string(output_image.path)
					<< "', creation_time=" << captured_image.capture_time << ", host_id = " << image_server_const.host_id() << ", `partition` = '" << output_image.partition << "' ";
				output_image.id = sql.send_query_get_id();
				new_region_image.region_images_image_id = output_image.id;
				sql << "UPDATE sample_region_images SET image_id = " << output_image.id << " WHERE id= " << new_region_image.region_images_id;
				sql.send_query();
				sql.send_query("COMMIT");
			}
			//get storage for the output image.
			bool had_to_use_volatile_storage;
			(*mask_splitter.mask_info())[mask_region_value]->reciever = image_server_const.image_storage.request_storage(output_image, ns_tiff, 1.0, _image_chunk_size, &sql, had_to_use_volatile_storage, false, false);
			(*mask_splitter.mask_info())[mask_region_value]->reciever_provided = true;
			output_images.push_back(output_image);
		}
		sql.send_query("COMMIT");

		//No opportunity for deadlocks because we are only reading.
		ns_simple_image_cache::const_handle_t mask_image;
		ns_image_cache_data_source data_source;
		data_source.handler = &image_server_const.image_storage;
		data_source.sql = &sql;
		image_server.image_storage.cache.get_for_read(mask_image_info, mask_image, data_source);


		mask_splitter.specify_mask(mask_image().image);
		//if the whole image is shifted down in relation to the reference image
		//we need to shift the mask up to compensate!
		mask_splitter.specify_registration_offset(offset*-1);
		//we collect image statistics, such as average intensity, from the images
		ns_image_statistics sample_image_statistics;
		mask_splitter.specificy_sample_image_statistics(sample_image_statistics);

		try {
			ns_image_storage_source_handle<ns_8_bit> full_res_source = requested_image().full_res_image(profile_data_source);
			full_res_source.input_stream().pump(mask_splitter, _image_chunk_size);

			full_res_source.clear();
			requested_image().delete_cached_file(profile_data_source);
			requested_image.release();

			//mark all regions as processed.
			for (unsigned int i = 0; i < output_regions.size(); i++) {
				(*mask_splitter.mask_info())[output_regions[i].mask_color]->image_stats.calculate_statistics_from_histogram();
				ns_64_bit image_stats_db_id(0);
				(*mask_splitter.mask_info())[output_regions[i].mask_color]->image_stats.submit_to_db(image_stats_db_id, sql, true, false);
				sql << "UPDATE sample_region_images SET currently_under_processing=0, image_statistics_id=" << image_stats_db_id << " WHERE id= " << output_regions[i].region_images_id;
				sql.send_query();
			}


			if (delete_captured_image) {
				image_server_const.image_storage.delete_from_storage(captured_image, ns_delete_long_term, &sql);
				sql << "UPDATE captured_images SET image_id = 0 WHERE id = " << captured_image.captured_images_id;
				sql.send_query();
				sql << "DELETE FROM images WHERE id = " << captured_image.capture_images_image_id;
				sql.send_query();
			}

		}
		catch (ns_ex & ex) {
			sql.clear_query();
			//if the image doesn't exist or is corrupted, mark the record as "problem".
			if (ex.type() == ns_file_io) {
				ns_64_bit event_id = image_server_const.register_server_event(ex, &sql);
				captured_image.mark_as_problem(&sql, event_id);
				for (unsigned int i = 0; i < output_regions.size(); i++) {
					sql << "DELETE images FROM images, sample_region_images WHERE sample_region_images.id= " << output_regions[i].region_images_id << " AND sample_region_images.image_id = images.id";
					sql.send_query();
					sql << "DELETE from sample_region_images WHERE id= " << output_regions[i].region_images_id;
					sql.send_query();
				}
				sql.send_query("COMMIT");
				return;
			}
			else throw ex;
		}

		//make region thumbnails
		for (unsigned int i = 0; i < output_regions.size(); i++) {
			try {
				resize_region_image(output_regions[i], sql);
			}
			catch (ns_ex & ex) {
				ns_64_bit ev(image_server_const.register_server_event(ex, &sql));
				output_regions[i].mark_as_problem(&sql, ev);
			}
		}


		//once the files are written, mark them as so in the db
		for (unsigned int i = 0; i < output_images.size(); i++)
			output_images[i].mark_as_finished_processing(&sql);

		sample_image_statistics.calculate_statistics_from_histogram();
		ns_64_bit sample_stats_db_id(0);
		sample_image_statistics.submit_to_db(sample_stats_db_id, sql);
		sql << "UPDATE captured_images SET mask_applied=1, image_statistics_id=" << sample_stats_db_id << " WHERE id= " << captured_image.captured_images_id;
		sql.send_query();
		sql.send_query("COMMIT");

		unsigned long stop_time = ns_current_time();
		ev.specify_processing_duration(stop_time - start_time);
		//		image_server_const.update_registered_server_event(event_id,ev);
		ns_64_bit t(timer.stop());
		cerr << "total time:" << (t / 1000 / 10)/100.0;
		image_server.register_job_duration(ns_process_apply_mask, t);
	}
	catch (...) {
		throw;
	}	
}

void ns_image_processing_pipeline::register_event(const ns_processing_task & task, const ns_image_properties & properties, const ns_image_server_event & source_event,const bool precomputed,ns_sql & sql){
	ns_image_server_event ev(source_event);
	ev.specify_event_subject_dimentions(properties);
	register_event(task,ev,precomputed,sql);
}

void ns_image_processing_pipeline::register_event(const ns_processing_task & task, const ns_image_server_event & source_event,const bool precomputed,ns_sql & sql){
	ns_image_server_event output("ns_image_processing_pipeline::");
	if (precomputed)
		output << "Loading Precomputed ";
	else output << "Computing ";
	output << ns_processing_task_to_string(task) << ns_ts_minor_event;
	output.log = false;
	image_server_const.register_server_event(output,&sql);

	ns_image_server_event log_to_db = source_event;
	log_to_db.specify_processing_job_operation(task);
	log_to_db << ns_ts_minor_event;
	image_server_const.register_server_event(log_to_db,&sql);
}

bool ns_image_processing_pipeline::detection_calculation_required(const ns_processing_task & s){
	return  s == ns_process_worm_detection ||
			s == ns_process_worm_detection_labels || 
			s == ns_process_region_vis || 
			s == ns_process_accept_vis || 
			s == ns_process_reject_vis || 
			s == ns_process_add_to_training_set;
}

bool ns_image_processing_pipeline::preprocessed_step_required(const ns_processing_task & might_be_needed, const ns_processing_task & task_to_perform){
	const ns_processing_task & s(might_be_needed);
	switch(task_to_perform){
		case ns_process_apply_mask: 
		case ns_process_compress_unprocessed:
		case ns_process_thumbnail:
		case ns_process_compile_video:
		case ns_process_analyze_mask: 
		case ns_process_heat_map:
		case ns_process_static_mask:
		case ns_process_last_task_marker:
			return false;
		
		case ns_unprocessed: return false;
		case ns_process_spatial: return s==ns_unprocessed;
		case ns_process_lossy_stretch: return s==ns_process_spatial;

		case ns_process_threshold: return s==ns_process_spatial;

		case ns_process_worm_detection:
		case ns_process_worm_detection_labels: 
		case ns_process_region_vis: 
		case ns_process_region_interpolation_vis: 
		case ns_process_accept_vis: 
		case ns_process_reject_vis: 
		case ns_process_add_to_training_set:
				return s==ns_unprocessed || s==ns_process_spatial || s == ns_process_threshold;

		case ns_process_worm_detection_with_graph: return s ==ns_process_worm_detection;
	
		case ns_process_interpolated_vis: return false;

		case ns_process_movement_coloring:
		case ns_process_movement_mapping:
		case ns_process_posture_vis: return s==ns_process_lossy_stretch;

		case ns_process_movement_coloring_with_graph:
		case ns_process_movement_coloring_with_survival: return s == ns_process_movement_coloring;

		case ns_process_movement_paths_visualition_with_mortality_overlay:
			 return s == ns_process_movement_paths_visualization;

		case ns_process_movement_paths_visualization:
			 return s == ns_process_lossy_stretch;

		case ns_process_movement_posture_visualization: 
		case ns_process_movement_posture_aligned_visualization:
		case ns_process_unprocessed_backup:
				return false;

		default: throw ns_ex("Unkown Processing Step:") << ns_processing_task_to_string(might_be_needed);
	}
}


void cancel_dependencies(const ns_processing_task to_cancel, vector<char> & operations,ns_precomputed_processing_step_images & precomputed_images){
	if(!precomputed_images.has_been_calculated(to_cancel)) return;
	operations[to_cancel] = 0;
	for (unsigned int k = 0; k < operations.size(); k++){
		if (ns_image_processing_pipeline::preprocessed_step_required(to_cancel,(ns_processing_task)k))
			cancel_dependencies((ns_processing_task)k,operations,precomputed_images);
	}
}

bool identify_missing_dependencies(const ns_processing_task & task_required, vector<char> & operations,vector<char> & dependencies_checked,ns_precomputed_processing_step_images & precomputed_images){


	//the current task is required, mark it so.
	operations[(unsigned long)task_required] = 1;
	//if the current task has not been precomputed, we'll need to recompute both it and anything that depends on it.
	bool current_task_is_missing = !precomputed_images.has_been_calculated(task_required);

	//if we already have checked downstream dependencies, we don't need to again.
	if (dependencies_checked[(unsigned long)task_required])
		return current_task_is_missing;
	dependencies_checked[(unsigned long)task_required] = 1;

	//search for downstream task dependencies, to make sure they get marked as required in operations[]
	bool dependency_is_missing(false);
	for (unsigned int i = (unsigned int)ns_process_spatial; i < operations.size(); i++){
		if (ns_image_processing_pipeline::preprocessed_step_required((ns_processing_task)i,task_required)){
			dependency_is_missing = dependency_is_missing || identify_missing_dependencies((ns_processing_task) i,operations,dependencies_checked,precomputed_images);
		}
	}
	//if any downstream dependencies have not yet been calculated, we'll need to recompute this one.
	//thus, we ignore the previously computed copy.
	if (dependency_is_missing)
		precomputed_images.remove_preprocessed_image(task_required);
	//upstream tasks will need to be recomputed if this or downstream dependencies need to be recomputed
	return current_task_is_missing || dependency_is_missing;
}

void ns_image_processing_pipeline::reason_through_precomputed_dependencies(vector<char> & operations,ns_precomputed_processing_step_images & precomputed_images){

	//calculate operation dependencies
	vector<char> dependencies_checked(operations.size(),0);
	for (long i = (long)operations.size()-1; i >= (long)ns_process_spatial; --i){
		if (operations[i]) identify_missing_dependencies((ns_processing_task)i,operations,dependencies_checked,precomputed_images);
		
	}

	//decide whether worm detection needs to be done
	for (unsigned int i = (unsigned int)ns_process_spatial; i < operations.size(); i++){
		if (operations[i] &&
			detection_calculation_required((ns_processing_task)i) &&
			!precomputed_images.is_provided((ns_processing_task)i))
			precomputed_images.worm_detection_needs_to_be_performed = true;
	}

	//movement color images cannot be processed in this step, so we have to ignore any requests that require it
	if (!precomputed_images.is_provided(ns_process_movement_coloring)){
		for (unsigned int i = (unsigned int)ns_process_spatial; i < operations.size(); i++){
			if (operations[i] &&
				preprocessed_step_required(ns_process_movement_coloring,(ns_processing_task)i)){
				cancel_dependencies((ns_processing_task)i,operations,precomputed_images);
			}
		}
	}
};

void ns_lifespan_curve_cache_entry::clean()const{
	if (region_raw_data_cache.size() > 1)
		region_raw_data_cache.clear();
}
const ns_death_time_annotation_compiler & ns_lifespan_curve_cache_entry::get_region_data(const ns_death_time_annotation_set::ns_annotation_type_to_load & a,const unsigned long id,ns_sql & sql) const{
	//check to see if local data is up to date
	sql << "SELECT latest_movement_rebuild_timestamp FROM sample_region_image_info WHERE id = " << id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.empty())
		throw ns_ex("ns_lifespan_curve_cache_entry::get_region_data()::Could not find region ") << id << " in db";
	unsigned long region_timestamp(atol(res[0][0].c_str()));
	ns_region_raw_cache::iterator p = region_raw_data_cache.find(id);
	bool rebuild(false);
	if (p == region_raw_data_cache.end()){
		region_raw_data_cache.clear(); //only keep one cached copy lying around.  these things use a lot of memory!
		p = region_raw_data_cache.insert(ns_region_raw_cache::value_type(id,ns_lifespan_curve_cache_entry_data())).first;
		p->second.region_compilation_timestamp = region_timestamp;
		rebuild = true;
	}
	if (p->second.region_compilation_timestamp < region_timestamp)
		rebuild = true;
	if (rebuild){
		ns_machine_analysis_data_loader machine_loader;
		machine_loader.load(a,id,0,0,sql);
		for (unsigned int i = 0; i < machine_loader.samples.size(); i++){
			for (unsigned int j = 0; j < machine_loader.samples[i].regions.size(); j++){
				p->second.compiler.add(machine_loader.samples[i].regions[j].death_time_annotation_set,machine_loader.samples[i].regions[j].metadata);
				//hand annotations are already loaded
				ns_hand_annotation_loader hand;
				hand.load_region_annotations(ns_death_time_annotation_set::ns_censoring_data,
					machine_loader.samples[i].regions[j].metadata.region_id,
					machine_loader.samples[i].regions[j].metadata.experiment_id,
					machine_loader.samples[i].regions[j].metadata.experiment_name,
					machine_loader.samples[i].regions[j].metadata,
					sql);
		//		cached_risk_timeseries_metadata = machine_loader.samples[i].regions[j].metadata;
				p->second.compiler.add(hand.annotations);
			}
		}

		//this->cached_strain_risk_timeseries = 
	}

	return p->second.compiler;
}

const ns_lifespan_curve_cache_entry & ns_lifespan_curve_cache::get_experiment_data(const ns_64_bit id, ns_sql & sql){
	//first we check all the regions to make sure all the data is loaded.
	sql << "SELECT r.id, r.latest_movement_rebuild_timestamp, r.latest_by_hand_annotation_timestamp "
			"FROM sample_region_image_info as r, capture_samples as s "
			"WHERE r.sample_id = s.id AND s.experiment_id = " << id;
	ns_sql_result res;
	sql.get_rows(res);
	bool reload_experiment_data(false);
	for (unsigned int i = 0; i < res.size(); i++){
		const unsigned long region_id(ns_atoi64(res[i][0].c_str()));
		const unsigned long latest_machine_timestamp(atol(res[i][1].c_str()));
		const unsigned long latest_hand_timestamp(atol(res[i][2].c_str()));
		ns_lifespan_region_timestamp_cache::iterator p = timestamp_cache.find(region_id);
		if (p==timestamp_cache.end() || 
			p->second.latest_movement_rebuild_timestamp != latest_machine_timestamp|| 
			p->second.latest_by_hand_annotation_timestamp != latest_hand_timestamp){
			reload_experiment_data = true;
			timestamp_cache[region_id].latest_movement_rebuild_timestamp = latest_machine_timestamp;
			timestamp_cache[region_id].latest_by_hand_annotation_timestamp = latest_hand_timestamp;
		}
	}
	ns_lifespan_curve_cache_storage::iterator p(data_cache.find(id));
	if (p == data_cache.end()){
		reload_experiment_data = true;
		p = data_cache.insert(ns_lifespan_curve_cache_storage::value_type(id,ns_lifespan_curve_cache_entry())).first;
	}
	if (reload_experiment_data){
		
		//experiment_set.generate_common_time_set(p->second.survival_curves_on_common_time);
		//p->second.survival_curves_on_common_time.generate_survival_statistics();
		return p->second;
	}
	else return p->second;
}


void ns_image_processing_pipeline::overlay_graph(const ns_64_bit region_id,ns_image_whole<ns_component> & image, unsigned long start_time, 
	const ns_region_metadata & m, const ns_lifespan_curve_cache_entry & lifespan_curve, ns_sql & sql){

	ns_image_properties lifespan_curve_image_prop,metadata_overlay_prop;
	lifespan_curve_image_prop.width  = (unsigned int)(image.properties().width*(1-1/sqrt(2.0f)));
	lifespan_curve_image_prop.height = (lifespan_curve_image_prop.width/3)*2;
	lifespan_curve_image_prop.components = 3;		

	metadata_overlay_prop = lifespan_curve_image_prop;
	metadata_overlay_prop.height/=3;
	metadata_overlay_prop.width/=2;

	const unsigned int dw(image.properties().width),
						dh(image.properties().height);
	if (image.properties().width < lifespan_curve_image_prop.width || image.properties().height < lifespan_curve_image_prop.height)
		throw ns_ex("ns_image_processing_pipeline::Attempting to insert (") << lifespan_curve_image_prop.width << "," << lifespan_curve_image_prop.height 
																			<< ") graph into (" << dw << "," << dh << " image.";
	if (image.properties().components != 3)
		throw ns_ex("ns_image_processing_pipeline::Attempting to insert a graph into a grayscale image.");
	ns_image_standard lifespan_curve_graph;
	bool optimize_for_small_graph = lifespan_curve_image_prop.height < 800;

	ns_graph graph;
	ns_movement_visualization_generator vis_gen;
	ns_image_standard metadata_overlay;
	metadata_overlay.prepare_to_recieve_image(metadata_overlay_prop);
	
	if (lifespan_curve.cached_risk_timeseries_metadata.region_id != m.region_id){
		const ns_death_time_annotation_compiler & compiler(lifespan_curve.get_region_data(ns_death_time_annotation_set::ns_all_annotations,region_id,sql));
		ns_lifespan_experiment_set set;
		compiler.generate_survival_curve_set(set,ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,true,false);
	//	set.generate_survival_statistics();
		set.generage_aggregate_risk_timeseries(m,lifespan_curve.cached_plate_risk_timeseries,lifespan_curve.cached_risk_timeseries_time);
		lifespan_curve.cached_risk_timeseries_metadata = m;
	}
	if (lifespan_curve.cached_strain_risk_timeseries_metadata.device_regression_match_description() != m.device_regression_match_description()){
			lifespan_curve.cached_strain_risk_timeseries_metadata = m;
			ns_machine_analysis_data_loader machine_loader;
			ns_lifespan_experiment_set experiment_set;
			machine_loader.load_just_survival(experiment_set,0,0,m.experiment_id,sql,false,true);
			experiment_set.generage_aggregate_risk_timeseries(m,lifespan_curve.cached_strain_risk_timeseries,lifespan_curve.cached_strain_risk_timeseries_time);
		lifespan_curve.cached_strain_risk_timeseries_metadata = m;
		
	}
	

	vis_gen.create_survival_curve_for_capture_time	(start_time,m,lifespan_curve.cached_plate_risk_timeseries,lifespan_curve.cached_strain_risk_timeseries,
													 lifespan_curve.cached_risk_timeseries_time,lifespan_curve.cached_strain_risk_timeseries_time,"Survival",true,optimize_for_small_graph,metadata_overlay,graph);													
	lifespan_curve_graph.init(lifespan_curve_image_prop);
	graph.draw(lifespan_curve_graph);

	const unsigned int w(lifespan_curve_graph.properties().width),
			h(lifespan_curve_graph.properties().height);

	for (unsigned int y = 0; y < h; y++){
		for (unsigned int x = 0; x < w; x++){
			ns_color_8 v(lifespan_curve_graph[y][3*x],
				 		 lifespan_curve_graph[y][3*x+1],
						 lifespan_curve_graph[y][3*x+2]);
				double sh = 1;
				if (v == ns_color_8(0,0,0))
					sh = .75;
				int _x = dw-w+x, // if these could accidentally be < 0 (see below) they should be signed
					_y = dh-h+y;
				if (_x < 0 || _x >= image.properties().width)
					throw ns_ex("ns_image_processing_pipeline::Graph overlay problem (X)");
				if (_y < 0 || _y >= image.properties().height)
					throw ns_ex("ns_image_processing_pipeline::Graph overlay problem (Y)");
				for (unsigned int c = 0; c < 3; c++)
					image[_y][3*_x+c] = (ns_8_bit)((1.0-sh)*image[_y][3*_x+c] + (sh)*lifespan_curve_graph[y][3*x+c]);
		}
	}
	for (unsigned int y = 0; y < metadata_overlay.properties().height; y++){
		for (unsigned int x = 0; x < metadata_overlay.properties().width; x++){
			ns_color_8 v(metadata_overlay[y][3*x],
				 		metadata_overlay[y][3*x+1],
						metadata_overlay[y][3*x+2]);
			double sh = 1;
			if (v == ns_color_8(0,0,0))
				sh = .75;
			int _x = dw-metadata_overlay.properties().width-w+x, // if these could accidentally be < 0 (see below) they should be signed
				_y = dh-metadata_overlay.properties().height+y;
			if (_x < 0 || _x >= image.properties().width)
				throw ns_ex("ns_image_processing_pipeline::Graph overlay problem (X)");
			if (_y < 0 || _y >= image.properties().height)
				throw ns_ex("ns_image_processing_pipeline::Graph overlay problem (Y)");
			for (unsigned int c = 0; c < 3; c++)
				image[_y][3*_x+c] = (ns_8_bit)((1.0-sh)*image[_y][3*_x+c] + (sh)*metadata_overlay[y][3*x+c]);
		}
	}
}


///Confirms that the requetsed operations are self consistant.
void ns_image_processing_pipeline::analyze_operations(const vector<char> & operations){
	first_task = ns_process_last_task_marker;
	last_task = (ns_processing_task)0;
	for (unsigned int i = 0; i < operations.size(); i++)
		if (operations[i]){
			first_task = (ns_processing_task)i;
			break;
		}
	if (first_task == operations.size())
		throw ns_ex("ns_image_processing_pipeline::Requesting a pipline operation with no tasks specified.");

	for (int i = (int)operations.size()-1; i >= 0; i--)
		if (operations[i]){
			last_task = (ns_processing_task)(i);
			break;
		}
	if (last_task >= ns_process_last_task_marker)
		throw ns_ex("ns_image_processing_pipeline::Requesting operations off the end of the pipeline.");

	//we don't allow jobs that contain operations on individual regions to also execute movement jobs.
	//detect jobs that include both movement and region jobs, and truncate them at the last non-movement job
	//specified
	if (first_task < ns_process_movement_coloring && 
		last_task >= ns_process_movement_coloring){	
			for (int i = (int)ns_process_movement_coloring-1; i >= 0; i--){
				if (operations[i]){
					last_task = (ns_processing_task)i;
					break;
				}
			}
	}

	//ignore requests to do nothing to the mask.
	if (first_task == ns_unprocessed)
		first_task = ns_process_apply_mask;

	if (first_task > last_task)
		throw ns_ex("ns_image_processing_pipeline::You have requested to perform operations in an invalid order.  Requesting this should be impossible!  Good work!");

	if (first_task <= ns_process_apply_mask && last_task > ns_process_apply_mask)
		throw ns_ex("ns_image_processing_pipeline::Cannot apply a mask to a region that has already been masked.");
}



//Confirmst that the specified operations are possible to calculate, and locates any steps that can be loaded from disk rather than re-computed.
void ns_image_processing_pipeline::analyze_operations(const ns_image_server_captured_image_region & region_image,vector<char> & operations, ns_precomputed_processing_step_images & precomputed_images, ns_sql & sql){
	
	//Look to see if any of the processing steps have already been computed
	sql << "SELECT ";
	for (unsigned int i = (unsigned int)ns_process_spatial; i < (unsigned int)ns_process_last_task_marker; i++){
		sql << ns_processing_step_db_column_name(i);
		if (ns_processing_step_db_column_name(i).size() == 0)
			sql << "0";
		if (i != (unsigned int)ns_process_last_task_marker-1) sql << ",";
	}
	sql << " FROM sample_region_images WHERE id=" << region_image.region_images_id;
	ns_sql_result res;
	sql.get_rows(res);

	if (res.size() == 0)
		throw ns_ex("ns_image_processing_pipeline::Could not load precomputed image information from database.");

	
	for (unsigned int i = (unsigned int)ns_process_spatial; i < (unsigned int)ns_process_last_task_marker; i++){
		if (i == ns_process_add_to_training_set ||
			i == ns_process_region_vis ||
			i == ns_process_thumbnail)
			continue;
		precomputed_images.specify_image_id((ns_processing_task)i, ns_atoi64(res[0][i-2].c_str()),sql);
	}

	reason_through_precomputed_dependencies(operations,precomputed_images);

	analyze_operations(operations);
}


bool ns_image_processing_pipeline::check_for_precalculated_registration(const ns_image_server_captured_image & captured_image, ns_vector_2i & registration_offset, ns_sql & sql){
	sql << "SELECT registration_offset_calculated, registration_vertical_offset, registration_horizontal_offset FROM captured_images WHERE id = " << captured_image.captured_images_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_processing_pipeline::apply_mask::Could not load captured image sample image registration information");
	if(res[0][0] != "0"){
		registration_offset.y = atoi(res[0][1].c_str());
		registration_offset.x = atoi(res[0][2].c_str());
		return true;
	}
	return false;
}
	ns_vector_2i registration_offset;
	//delete_profile_after_use = false;
	/*if (check_for_precalculated_registration(captured_image,registration_offset,sql)){
	//	cerr << "Using existing vertical registration\n";
		return registration_offset;
	}*/

void ns_image_processing_pipeline::get_reference_image(const ns_image_server_captured_image & captured_image, 
													   ns_image_fast_registration_profile_cache::const_handle_t & reference_image,
													   ns_image_fast_registration_profile_cache::external_source_type & external_source) {
	//load the reference image to which the masked image must be vertically registered
	bool registration_image_loaded = false;
	unsigned long attempts = 0;
	ns_image_server_image reference_image_db_record;
	*external_source.sql << "SELECT image_id, id, never_delete_image FROM captured_images WHERE sample_id = " << captured_image.sample_id << " AND never_delete_image = 1 AND problem = 0 AND censored = 0 ORDER BY capture_time ASC LIMIT 5";
	ns_sql_result res;
	external_source.sql->get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_processing_pipeline::run_vertical_registration()::Could not perform registration because no reference images exist for the sample") << captured_image.experiment_name << "::" << captured_image.sample_name;
	reference_image_db_record.id = 0;
	bool reference_image_loaded(false);

	for (unsigned int i = 0; !reference_image_loaded && i < res.size(); i++) {
		if (res[i][0] == "0")
			continue;
		ns_image_server_image im;
		im.id = ns_atoi64(res[i][0].c_str());
	
		try {
			image_server.image_registration_profile_cache.get_for_read(im, reference_image, external_source);
			reference_image_loaded = true;
		}
		catch (ns_ex & ex) {
			image_server_const.register_server_event(ex, external_source.sql);
			//could do something here to allow more reference images to be used, but this is potentially
			//troublesome and will be avoided for now
		}
	}
	if (!reference_image_loaded)
		throw ns_ex("ns_image_processing_pipeline::run_vertical_registration()::Could not perform registration because all of the first few raw images could not be loaded for sample ") << captured_image.experiment_name << "::" << captured_image.sample_name << " "
		<< "Only images that are flagged in captured_images as problem=0, censored=0, never_delete=1 can be used as references for image registration";
}

ns_vector_2i ns_image_processing_pipeline::get_vertical_registration(
			ns_image_fast_registration_profile_cache::const_handle_t & requested_image,
			ns_image_fast_registration_profile_cache::const_handle_t & reference_image,
			ns_image_fast_registration_profile_cache::external_source_type & external_source) {

	ns_calc_best_alignment_fast aligner(ns_vector_2i(1000,1000), ns_vector_2i(0,0), ns_vector_2i(0, 0));
	aligner.minimize_memory_use(true);
	bool saturated_offset;
	ns_vector_2d fine_offset = aligner(ns_vector_2d(0, 0), ns_vector_2d(1000, 1000), reference_image().pyramid, requested_image().pyramid, saturated_offset);

	ns_vector_2i offset(round(fine_offset.x*ns_image_fast_registration_profile::ns_registration_downsample_factor),
		round(fine_offset.y*ns_image_fast_registration_profile::ns_registration_downsample_factor));

	return offset;
}

void ns_shift_image_by_offset(ns_image_standard & image, ns_vector_2i offset){
		offset = offset*-1;
		ns_vector_2i image_size(image.properties().width,image.properties().height);
		ns_vector_2i new_topleft(offset), 
						new_bottomright(image_size + offset);
		if (offset.x < 0){
			new_topleft.x = 0;
		}
		if (offset.y < 0){
			new_topleft.y = 0;
		}
		if (new_bottomright.x >= image_size.x)
			new_bottomright.x = image_size.x;
		if (new_bottomright.y >= image_size.y)
			new_bottomright.y = image_size.y;
		new_bottomright = new_bottomright + ns_vector_2i(-1,-1);
		//copy pixels over to new location
		long y_bounds[3] = { new_topleft.y,new_bottomright.y,1};
		long x_bounds[3] = { new_topleft.x,new_bottomright.x,1};
		ns_swap<long> swap;
		if (offset.y > 0){
			swap(y_bounds[0],y_bounds[1]);
			y_bounds[2]=-1;
		}
		if (offset.x > 0){
			swap(x_bounds[0],x_bounds[1]);
			x_bounds[2]=-1;
		}
			
		y_bounds[1]+=y_bounds[2];
		x_bounds[1]+=x_bounds[2];
		for (long y = y_bounds[0]; y != y_bounds[1]; y+=y_bounds[2]){
			for (long x = x_bounds[0]; x != x_bounds[1]; x+=x_bounds[2]){
				image[y][x] = image[y-offset.y][x-offset.x];
			}
		}
		//zero out all the border
		for (long y = 0; y < new_topleft.y; y++)
			for (long x = 0; x < image_size.x; x++)
				image[y][x] = 0;
			
		for (int y = new_topleft.y; y < new_bottomright.y; y++){
			for (long x = 0; x < new_topleft.x; x++)
				image[y][x] = 0;
			for (long x = new_bottomright.x+1; x < image_size.x; x++)
				image[y][x] = 0;
		}
		for (long y = new_bottomright.y+1; y < image_size.y; y++)
			for (long x = 0; x < image_size.x; x++)
				image[y][x] = 0;
	}


void ns_rerun_image_registration(const ns_64_bit region_id, ns_sql & sql){
	sql << "SELECT id, image_id, " << ns_processing_step_db_column_name(ns_process_unprocessed_backup) << ", capture_time,"
		 << ns_processing_step_db_column_name(ns_process_spatial) << ","
		 << ns_processing_step_db_column_name(ns_process_lossy_stretch) << ","
		 <<  ns_processing_step_db_column_name(ns_process_threshold) << " "
		"FROM sample_region_images WHERE region_info_id = " << region_id << " AND censored=0 ORDER BY capture_time ASC";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_rerun_image_registration::The requested region, ") << region_id << ", contains no uncensored images.";

	long i = 0;

	ns_image_fast_registration_profile_cache::const_handle_t reference_profile;
	ns_image_fast_registration_profile_data_source profile_data_source;
	profile_data_source.sql = &sql;
	profile_data_source.image_storage = &image_server_const.image_storage;

	ns_image_server_image reference_image_record;
	reference_image_record.id = 0;
	while (reference_image_record.id == 0) {
		if (i >= res.size())
			throw ns_ex("ns_rerun_image_registration::Could not find a non-censored reference image to use for alignment.");
		try {
			reference_image_record.id = ns_atoi64(res[i][1].c_str());
			image_server.image_registration_profile_cache.get_for_read(reference_image_record, reference_profile, profile_data_source);
			reference_profile.release();
		}
		catch (...) { 
			reference_image_record.id = 0;
		}
		i++;
	}
	if (reference_image_record.id == 0)
		throw ns_ex("Could not find a suitable reference image");
	ns_image_standard buffer;
	buffer.use_more_memory_to_avoid_reallocations();
	for (;i<res.size();i++){
		try{
			ns_image_server_captured_image_region region_image;
			region_image.region_images_id = ns_atoi64(res[i][0].c_str());

			ns_64_bit unprocessed_id = ns_atoi64(res[i][1].c_str());
			ns_64_bit backup_unprocessed_id = ns_atoi64(res[i][2].c_str());
			unsigned long capture_time = atol(res[i][3].c_str());

			ns_image_server_image image_record;
			if (backup_unprocessed_id != 0){
				image_record.id = backup_unprocessed_id;
			}else image_record.id = unprocessed_id;

			ns_image_fast_registration_profile_cache::const_handle_t image_profile;
			image_server.image_registration_profile_cache.get_unlinked_singleton(image_record,image_profile , profile_data_source);

			ns_calc_best_alignment_fast aligner(ns_vector_2i(1000, 1000), ns_vector_2i(0, 0), ns_vector_2i(0, 0));
			bool saturated_offset;
			ns_vector_2d fine_offset = aligner(ns_vector_2d(0, 0), ns_vector_2d(1000, 1000), reference_profile().pyramid, image_profile().pyramid, saturated_offset);
			reference_profile.release();
			ns_vector_2i offset(round(fine_offset.x), round(fine_offset.y));

			//ns_vector_2i offset(-500,500);
			image_server_const.add_subtext_to_current_event(ns_to_string((100*i)/res.size()) + "%: " + ns_format_time_string_for_human(capture_time) = "\n",&sql);
			
			//if registration is required and a backup does not already exist, create a backup.
			if (offset.squared() <= 1)
				continue;
			image_server_const.add_subtext_to_current_event(std::string("Fixing offset : ") +ns_to_string(offset.x)+", " + ns_to_string(offset.y) + "..." + ns_processing_task_to_string(ns_unprocessed)= "...",&sql);
			if (backup_unprocessed_id == 0){
				//cerr << "Creating backup...";
				ns_image_server_image backup_image = region_image.create_storage_for_processed_image(ns_process_unprocessed_backup,ns_tiff,&sql);
				bool had_to_use_volatile_storage;
				ns_image_storage_reciever_handle<ns_8_bit> out_im(image_server_const.image_storage.request_storage(backup_image,ns_tiff,1.0,1024,&sql,had_to_use_volatile_storage,false,false));
				ns_image_storage_source_handle<ns_8_bit> full_res(image_profile().full_res_image(profile_data_source));
				full_res.input_stream().pump(out_im.output_stream(),1024);
				out_im.clear();
			}
			ns_image_storage_source_handle<ns_8_bit> full_res(image_profile().full_res_image(profile_data_source));
			full_res.input_stream().pump(buffer, 1024);
			image_profile().delete_cached_file(profile_data_source);
			image_profile.release();
			ns_shift_image_by_offset(buffer,offset);

			ns_image_server_image dest_im;
			dest_im.id = unprocessed_id;
			bool had_to_use_volatile_storage;
			ns_image_storage_reciever_handle<ns_8_bit> out_im(image_server_const.image_storage.request_storage(dest_im,ns_tiff,1.0,1024,&sql,had_to_use_volatile_storage,false,false));
			//now fix the registration of all derived images.
			buffer.pump(out_im.output_stream(),1024);
			out_im.clear();
			ns_processing_task tasks[3] = { ns_process_spatial, ns_process_lossy_stretch,ns_process_threshold};
			for (unsigned int j = 0; j < 3; j++){
				try {
					ns_image_server_image task_image;
					task_image.id = ns_atoi64(res[i][4 + j].c_str());
					if (task_image.id == 0)
						continue;
					cout << ns_processing_task_to_string(tasks[j]) << "...";
					ns_image_storage_source_handle<ns_8_bit> source(image_server_const.image_storage.request_from_storage(task_image, &sql));
					source.input_stream().pump(buffer, 1024);
					source.clear();
					ns_shift_image_by_offset(buffer, offset);
					ns_image_type type = ns_tiff;
					float compression = 1.0;
					if (tasks[j] == ns_process_lossy_stretch) {
						type = ns_jpeg;
						compression = .8;
					}
					ns_image_storage_reciever_handle<ns_8_bit> out_im(image_server_const.image_storage.request_storage(task_image,type, compression,1024,&sql,had_to_use_volatile_storage,false,false));
					buffer.pump(out_im.output_stream(),1024);
				}
				catch(ns_ex & ex){
					cout << ex.text() << "\n";}
			}

		}
		catch(ns_ex & ex){
			cout << ex.text() << "\n";
		}
	}
}

ns_image_properties ns_image_processing_pipeline::get_small_dimensions(const ns_image_properties & prop){
	float max_dimension = (float)ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_dimension_size_for_small_images,prop.resolution);
	
	float ds;
	if (prop.height < prop.width)
		ds = max_dimension/prop.width;
	else ds = max_dimension/prop.height;
	if (ds > 1) ds = 1;
	return ns_image_properties((unsigned long)(prop.height*ds),(unsigned long)(prop.width*ds),prop.components,prop.resolution*ds);
}


