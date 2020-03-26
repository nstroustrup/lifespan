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

#include "ns_processing_job_scheduler.h"
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

	else if (job.region_id != 0) {

		sql << "SELECT id FROM sample_region_images WHERE region_info_id = " << job.region_id;
		ns_sql_result res;
		sql.get_rows(res);
		std::set<ns_64_bit> sample_region_image_ids;
		for (unsigned long i = 0; i < res.size(); i++)
			sample_region_image_ids.insert(ns_atoi64(res[i][0].c_str()));

		//first find any problem worm detection results
		sql << ns_image_worm_detection_results::sql_select_stub() << ", sample_region_images as reg,sample_region_image_info as r WHERE reg.region_info_id = r.id AND r.id = " << job.region_id << " AND e.id = reg.worm_detection_results_id";
		ns_sql_result detection_results_information;
		sql.get_rows(detection_results_information);
		const unsigned long number_of_detection_results(detection_results_information.size());
		sql << ns_image_worm_detection_results::sql_select_stub() << ", sample_region_images as reg,sample_region_image_info as r WHERE reg.region_info_id = r.id AND r.id = " << job.region_id << " AND e.id = reg.worm_interpolation_results_id";
		ns_sql_result interpolated_detection_results_information;
		sql.get_rows(interpolated_detection_results_information);

		//concatenate normal and interpolated records so we can do this in one pass
		detection_results_information.reserve(number_of_detection_results + interpolated_detection_results_information.size());
		detection_results_information.insert(detection_results_information.end(), interpolated_detection_results_information.begin(), interpolated_detection_results_information.end());

		ns_image_worm_detection_results results;
		std::set<ns_64_bit> result_ids_to_delete,
			references_to_clear,
			interpolated_references_to_clear,
			detection_results_found,
			interpolated_results_found;
		for (unsigned long i = 0; i < detection_results_information.size(); i++) {
			const bool interpolated(i >= number_of_detection_results);
			ns_64_bit results_id = ns_atoi64(detection_results_information[i][0].c_str());
			if (interpolated)
				interpolated_results_found.insert(detection_results_found.begin(), results_id);
			else
				detection_results_found.insert(detection_results_found.begin(), results_id);
			ns_64_bit source_image_id = ns_atoi64(detection_results_information[i][1].c_str());
			try {
				results.load_from_db_internal(false, interpolated, detection_results_information[i], sql, true);
				std::set<ns_64_bit>::iterator p(sample_region_image_ids.find(results.source_image_id));
				if (p == sample_region_image_ids.end())
					result_ids_to_delete.insert(results.detection_results_id);
			}
			catch (...) {
				result_ids_to_delete.insert(results_id);
				if (interpolated)
					interpolated_references_to_clear.insert(source_image_id);
				else
					references_to_clear.insert(source_image_id);
			}
		}
		//now go through and delete any errors.
		if (!result_ids_to_delete.empty()) {
			sql << "DELETE FROM worm_detection_results WHERE id IN (";
			for (std::set<ns_64_bit>::iterator p = result_ids_to_delete.begin(); p != result_ids_to_delete.end(); p++)
				sql << *p << ",";
			sql << "0)";
			sql.send_query();
		}
		if (!references_to_clear.empty()) {
			sql << "UPDATE sample_region_images SET worm_detection_results_id = 0 WHERE id IN (";
			for (std::set<ns_64_bit>::iterator p = references_to_clear.begin(); p != references_to_clear.end(); p++)
				sql << *p << ",";
			sql << "0)";
			sql.send_query();
			references_to_clear.clear();
		}

		if (!interpolated_references_to_clear.empty()) {
			sql << "UPDATE sample_region_images SET worm_interpolation_results_id = 0 WHERE id IN (";
			for (std::set<ns_64_bit>::iterator p = interpolated_references_to_clear.begin(); p != interpolated_references_to_clear.end(); p++)
				sql << *p << ",";
			sql << "0)";
			sql.send_query();
			interpolated_references_to_clear.clear();
		}

		//now we check
		//1) for any images with missing detection or interpolated detection references
		//2) each image to see it exists on disk
		//3) each image to see if it has been replaced by a compressed copy
		sql << ns_image_server_captured_image_region::sql_stub(&sql) << " AND r.id = " << job.region_id;
		ns_sql_result region_results;
		sql.get_rows(region_results);

		unsigned long step(region_results.size() / 10);
		for (unsigned long i = 0; i < region_results.size(); i++) {
			if (i%step == 0)
				image_server_const.add_subtext_to_current_event(ns_to_string(10 * (i / step)) + "%...", &sql);
			ns_image_server_captured_image_region reg;
			try {
				reg.load_from_db_internal(region_results[i]);

				//check for missing detection or interpolated detection references
				if (reg.region_detection_results_id != 0) {
					std::set<ns_64_bit>::iterator p = detection_results_found.find(reg.region_detection_results_id);
					if (p == detection_results_found.end())
						references_to_clear.insert(references_to_clear.begin(), reg.region_images_id);
				}
				if (reg.region_interpolation_results_id != 0) {
					std::set<ns_64_bit>::iterator p = interpolated_results_found.find(reg.region_interpolation_results_id);
					if (p == interpolated_results_found.end())
						interpolated_references_to_clear.insert(interpolated_references_to_clear.begin(), reg.region_images_id);
				}

				//check for missing or compressed images
				ns_image_stream_static_offset_buffer<ns_8_bit> buf(ns_image_stream_buffer_properties(10000, 1));
				for (unsigned long j = 0; j < reg.op_images_.size(); j++) {
					if (i == (int)ns_process_add_to_training_set)
						continue;
					bool changed = false;
					if (reg.op_images_[j] != 0) {
						ns_image_server_image im_base;

						im_base.load_from_db(reg.op_images_[j], &sql);
						try {
							//first iteration try file
							//second iteration look for jpeg2000 conversion
							for (int k = 0; k < 2; k++) {
								const bool tif_failed_try_jp2(k == 1);
								ns_image_server_image im = im_base;

								if (tif_failed_try_jp2)
									im.filename += ".jp2";

								try {
									ns_ojp2k_initialization::verbose_output = false;
									ns_jpeg_library_user::verbose_output = false;
									ns_image_storage_source_handle<ns_8_bit> h(image_server_const.image_storage.request_from_storage(im, &sql));
									//unsigned long internal_state;
									long w(h.input_stream().properties().width*h.input_stream().properties().components);
									if (w > buf.properties().width)
										buf.resize(ns_image_stream_buffer_properties(w, 1));
								//	h.input_stream().send_lines(buf, 1, internal_state);
									try {
										ns_ojp2k_initialization::verbose_output = false;
										ns_jpeg_library_user::verbose_output = false;
										h.clear();
									}
									catch (...) {}
									if (tif_failed_try_jp2) 	//if we have found a jp2k compressed copy, update the database with the new filename.
										im.save_to_db(im.id, &sql, false);
									//We have successfully validated the file.
									break;
								}
								catch (...) {
									if (tif_failed_try_jp2) throw;
								}
							}
						}
						catch (ns_ex & ex) {
							reg.op_images_[j] = 0;
							changed = true;
						}
					}
					if (changed) {
						reg.update_all_processed_image_records(sql);
						if (reg.op_images_[0] == 0) {
							sql << "UPDATE sample_region_images SET problem=1 WHERE id = " << reg.region_images_id;
							sql.send_query();
						}
					}
				}
			}
			catch (ns_ex & ex) {
				image_server_const.add_subtext_to_current_event(ex.text() + "\n", &sql);
				continue;
			}

		}
		if (!references_to_clear.empty()) {
			sql << "UPDATE sample_region_images SET worm_detection_results_id = 0 WHERE id IN (";
			for (std::set<ns_64_bit>::iterator p = references_to_clear.begin(); p != references_to_clear.end(); p++)
				sql << *p << ",";
			sql << "0)";
			sql.send_query();
			references_to_clear.clear();
		}

		if (!interpolated_references_to_clear.empty()) {
			sql << "UPDATE sample_region_images SET worm_interpolation_results_id = 0 WHERE id IN (";
			for (std::set<ns_64_bit>::iterator p = interpolated_references_to_clear.begin(); p != interpolated_references_to_clear.end(); p++)
				sql << *p << ",";
			sql << "0)";
			sql.send_query();
			interpolated_references_to_clear.clear();
		}
		ns_ojp2k_initialization::verbose_output = true;
		ns_jpeg_library_user::verbose_output = true;
	}
	else if (job.sample_id != 0){
		sql << "SELECT name,experiment_id FROM capture_samples WHERE id = " << job.sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_check_for_file_errors()::Could not load sample ") << job.sample_id << " from db!";
		const string sample_name(res[0][0]);
		ns_64_bit experiment_id = ns_atoi64( res[0][1].c_str());
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
					//look for jp2k
					im.filename += ".jp2";
					if (image_server_const.image_storage.image_exists(im, &sql, true)) {
						im.save_to_db(im.id, &sql, false);
						large_image_exists = true;
					}
					else {
						problem = true;
						found_problem = true;
						const ns_64_bit event_id(image_server_const.register_server_event(ns_image_server_event("Large capture image cannot be found on disk:") << experiment_name << "::" << sample_name << "::" << ns_format_time_string(atol(res[i][1].c_str())), &sql));
						im.mark_as_problem(&sql, event_id);
					}
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

///specifies the database entry id for the precomputed image corresponding to the specified processing step
bool ns_precomputed_processing_step_images::specify_image_id(const ns_processing_task & i, const ns_64_bit id,ns_sql & sql){
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


void ns_get_worm_detection_model_for_job(ns_64_bit region_id, ns_worm_detection_model_cache::const_handle_t & model, ns_sql & sql) {
	std::string model_name;
	
	sql << "SELECT worm_detection_model FROM sample_region_image_info WHERE "
		<< "sample_region_image_info.id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_processing_job_scheduler::The specified job refers to a region with an invalid sample reference: ") << region_id;
	model_name = res[0][0];
	
	//else throw ns_ex("ns_processing_job_scheduler::Not enough information specified to get model filename for job");

	if (model_name == "")
		throw ns_ex("ns_processing_job_scheduler::The specified job refers to a sample with no model file specified");

	image_server.get_worm_detection_model(model_name, model);
	if (!model.is_valid())
		throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Attempting to run task on region without specifying model");
}
///given the source image, run() runs the processing steps specified in operations.  operations is a bit map with each entry corresponding to the step
///referred to by its ns_processing_task enum value.  Operations flagged as "1" are performed, operations flagged as "0" are skipped.
void ns_image_processing_pipeline::process_region(const ns_image_server_captured_image_region & region_im, const vector<char> operations, ns_sql & sql){
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

	ns_image_storage_handler::ns_volatile_storage_behavior volatile_behavior(ns_image_storage_handler::ns_forbid_volatile);
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

		//only one frame out of ten needs to be annotated by hand.
		//we need to generate the remainder as well, so training sets can be generated by storyboards.
		
		bool this_frame_should_generate_a_training_set_image(true);
		/*if (operations[ns_process_add_to_training_set]){

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
				if (0&&!this_frame_should_generate_a_training_set_image){
					sql << "UPDATE sample_region_images SET make_training_set_image_from_frame = 0," <<
						ns_processing_step_db_column_name(ns_process_add_to_training_set) << "=1 WHERE id = " << region_image.region_images_id;
					sql.send_query();
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
					

						return;
					}
				}
			}*/
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
							volatile_behavior);
						spatial_average.pump(r.output_stream(), _image_chunk_size);

					//	r.output_stream().init(ns_image_properties(0, 0, 0));

					output_image.mark_as_finished_processing(&sql);

					image_server.register_job_duration(ns_process_spatial, tm.stop());
				}
			}

			if (!unprocessed_loaded && (precomputed_images.worm_detection_needs_to_be_redone_now || operations[ns_process_compress_unprocessed])) {
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
					ns_add_image_suffix(unprocessed_image.filename, ns_jp2k);

					string compression_rate = image_server_const.get_cluster_constant_value("jp2k_compression_rate", ns_to_string(NS_DEFAULT_JP2K_COMPRESSION), &sql);
					float compression_rate_f = atof(compression_rate.c_str());
					if (compression_rate_f <= 0)
						throw ns_ex("Invalid compression rate specified in jp2k_compression_rate cluster constant: ") << compression_rate_f;
					bool b;
					ns_image_storage_reciever_handle<ns_8_bit> im_dest(image_server_const.image_storage.request_storage(
						unprocessed_image, ns_jp2k, compression_rate_f, 1024, &sql, b, false, volatile_behavior));
					unprocessed.pump(im_dest.output_stream(), 1024);
					unprocessed_image.save_to_db(unprocessed_image.id, &sql);
					image_server_const.image_storage.delete_from_storage(old_im, ns_delete_long_term, &sql);
				}
			}

			//lossy stretch of dynamic range
			if (operations[ns_process_lossy_stretch] || operations[ns_process_movement_paths_visualization]){
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
																volatile_behavior);
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
					

					register_event(ns_process_movement_paths_visualization,dynamic_stretch.properties(),parent_event,false,sql);

					//first load data for whole experiment
					ns_annotation_region_data_id annotation_data(ns_death_time_annotation_set::ns_all_annotations, region_image.experiment_id);
					ns_experiment_surival_data_cache::const_handle_t annotation_handle;
					image_server.survival_data_cache.get_for_read(annotation_data, annotation_handle,sql);
					
					ns_death_time_annotation_compiler::ns_region_list::const_iterator p(annotation_handle().get_annotations_for_region(region_image.region_info_id));
					
					//now generate image
					ns_movement_visualization_generator gen;
					gen.create_time_path_analysis_visualization(region_image, p->second,dynamic_stretch, paths_visualization,sql);

					ns_image_server_image out_im(region_image.create_storage_for_processed_image(ns_process_movement_paths_visualization,ns_tiff,&sql));
					ns_image_storage_reciever_handle<ns_8_bit> out_im_f(image_server_const.image_storage.request_storage(
														out_im,
														ns_tiff, 1.0,1024,&sql,
																	had_to_use_volatile_storage,
																	report_file_activity_to_db,
																	volatile_behavior));
					paths_visualization.pump(out_im_f.output_stream(),1024);

					out_im.mark_as_finished_processing(&sql);
				}

			}
			if (operations[ns_process_movement_paths_visualition_with_mortality_overlay]){
				region_image.load_from_db(region_image.region_images_id,&sql);

				register_event(ns_process_movement_paths_visualition_with_mortality_overlay,dynamic_stretch.properties(),parent_event,false,sql);
				ns_annotation_region_data_id annotation_data(ns_death_time_annotation_set::ns_all_annotations, region_image.experiment_id);
				ns_experiment_surival_data_cache::const_handle_t annotation_handle;
				image_server.survival_data_cache.get_for_read(annotation_data, annotation_handle, sql);

				//compiler.generate_survival_statistics();
				overlay_graph(region_image.region_info_id,paths_visualization,region_image.capture_time,annotation_handle(),sql);


				ns_image_server_image out_im(region_image.create_storage_for_processed_image(ns_process_movement_paths_visualition_with_mortality_overlay,ns_jpeg,&sql));
				ns_image_storage_reciever_handle<ns_8_bit> out_im_f(image_server_const.image_storage.request_storage(
													out_im,ns_jpeg,
													NS_DEFAULT_JPEG_COMPRESSION, 1024,&sql,
													had_to_use_volatile_storage,
													report_file_activity_to_db,
													volatile_behavior));
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
																volatile_behavior);
				thresholded.pump(r.output_stream(),_image_chunk_size);
				output_image.mark_as_finished_processing(&sql);

				image_server.register_job_duration(ns_process_threshold,tm.stop());
				}
			}


			//detect worms
			//We are guarenteed
			//1)The thresholded image has already been loaded into thresholded
			//2)The spatial average has been previously loaded into spatial_average


			if (precomputed_images.worm_detection_needs_to_be_redone_now){

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
				sql << "SELECT maximum_number_of_worms_per_plate, last_worm_detection_model_used FROM sample_region_image_info WHERE id = " << region_image.region_info_id;
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
					ns_worm_detection_model_cache::const_handle_t model;
					ns_get_worm_detection_model_for_job(region_image.region_info_id, model, sql);

					const unsigned long worm_count_max(atol(res[0][0].c_str()));
					detected_worms.attach(worm_detector.run(region_image.region_info_id, region_image.capture_time,
						unprocessed,
						thresholded,
						spatial_average,
						((static_mask_image.id != 0)?&(static_mask().image):0),
						ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area, spatial_average.properties().resolution),
						ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area, spatial_average.properties().resolution),
						ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal, spatial_average.properties().resolution),
						model().model_specification,
						worm_count_max,
						&sql,
						"",
						vis_type,
						image_stats
					));

					if (res[0][1] != model().model_specification.model_name) {
						sql << "UPDATE sample_region_image_info SET last_worm_detection_model_used = '" << model().model_specification.model_name << "' WHERE id = " << region_image.region_info_id;
						sql.send_query();
					}

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
															volatile_behavior);
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
															volatile_behavior);
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
															volatile_behavior);
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
															volatile_behavior);
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
															volatile_behavior);
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
																	volatile_behavior);

								ti.pump(a_worm.output_stream(),_image_chunk_size);
								a_worm_im = region_image.create_storage_for_processed_image(ns_process_add_to_training_set, ns_xml, &sql);
								ns_acquire_for_scope<ns_ostream> xmp_out(image_server_const.image_storage.request_metadata_output(a_worm_im, ns_xml,false, &sql));
								xmp_out()() << ti.properties().description;
								xmp_out.release();
								a_worm_im = region_image.create_storage_for_processed_image(ns_process_add_to_training_set, ns_csv_gz, &sql);
								ns_acquire_for_scope<ns_ostream> metadata_out(image_server_const.image_storage.request_metadata_output(a_worm_im,ns_csv_gz,true,&sql));
								detected_worms().output_feature_statistics(metadata_out()());
								metadata_out.release();
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

				ns_annotation_region_data_id annotation_data(ns_death_time_annotation_set::ns_all_annotations, region_image.experiment_id);
				ns_experiment_surival_data_cache::const_handle_t annotation_handle;
				image_server.survival_data_cache.get_for_read(annotation_data, annotation_handle, sql);

				overlay_graph(region_image.region_info_id,temporary_image,region_image.capture_time,annotation_handle(),sql);
				ns_image_server_image output_image;

				output_image = region_image.create_storage_for_processed_image(ns_process_worm_detection_with_graph,ns_jpeg,&sql);
				ns_image_storage_reciever_handle<ns_component>  r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION,_image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																volatile_behavior);
				temporary_image.pump(r.output_stream(),_image_chunk_size);
				output_image.mark_as_finished_processing(&sql);
			}
			if (operations[ns_process_movement_coloring_with_graph]){
				if (!attempt_to_preload(ns_process_movement_coloring,precomputed_images,operations,temporary_image,sql))
					throw ns_ex("Could not load worm coloring for ns_process_movement_coloring_with_graph.");
				register_event(ns_process_movement_coloring_with_graph,temporary_image.properties(),parent_event,false,sql);

				ns_annotation_region_data_id annotation_data(ns_death_time_annotation_set::ns_all_annotations, region_image.experiment_id);
				ns_experiment_surival_data_cache::const_handle_t annotation_handle;
				image_server.survival_data_cache.get_for_read(annotation_data, annotation_handle, sql);

				overlay_graph(region_image.region_info_id,temporary_image,region_image.capture_time,annotation_handle(),sql);

				ns_image_server_image output_image;
				output_image = region_image.create_storage_for_processed_image(ns_process_movement_coloring_with_graph,ns_jpeg,&sql);
				ns_image_storage_reciever_handle<ns_component>  r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, _image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																volatile_behavior);
				temporary_image.pump(r.output_stream(),_image_chunk_size);
				output_image.mark_as_finished_processing(&sql);

			}

			if(operations[ns_process_movement_coloring_with_survival]){
				if (!attempt_to_preload(ns_process_movement_coloring,precomputed_images,operations,temporary_image,sql))
					throw ns_ex("Could not load worm coloring for ns_process_movement_coloring_with_survival");

				register_event(ns_process_movement_coloring_with_survival,temporary_image.properties(),parent_event,false,sql);

				ns_annotation_region_data_id annotation_data(ns_death_time_annotation_set::ns_all_annotations, region_image.experiment_id);
				ns_experiment_surival_data_cache::const_handle_t annotation_handle;
				image_server.survival_data_cache.get_for_read(annotation_data, annotation_handle, sql);

				overlay_graph(region_image.region_info_id,temporary_image,region_image.capture_time,annotation_handle(),sql);

				ns_image_server_image output_image;
				output_image = region_image.create_storage_for_processed_image(ns_process_movement_coloring_with_survival,ns_jpeg,&sql);
				ns_image_storage_reciever_handle<ns_component>  r = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION,_image_chunk_size,&sql,
																had_to_use_volatile_storage,
																report_file_activity_to_db,
																volatile_behavior);
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
	ns_64_bit mask_region_id;
	unsigned long  mask_value;
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
	std::map<ns_64_bit, ns_mask_region_sorter_element *> mask_finder;

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

	std::string default_posture_analysis_model = image_server.get_cluster_constant_value("default_posture_analysis_model", "2017_07_20=wildtype_v2", &sql);
	std::string default_posture_analysis_method = image_server.get_cluster_constant_value("default_posture_analysis_method", "thresh", &sql);
	std::string default_worm_detection_model = image_server.get_cluster_constant_value("default_worm_detection_model", "mixed_genotype_high_24_plus_fscore_features", &sql);
	std::string default_position_analysis_model = image_server.get_cluster_constant_value("default_position_analysis_model", "", &sql);
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
				<< ", position_in_sample_x = '" << mask_regions[i].pos.x / capture_sample_image_resolution_in_dpi << "'"
				<< ", position_in_sample_y = '" << mask_regions[i].pos.y / capture_sample_image_resolution_in_dpi << "'"
				<< ", size_x = '" << mask_regions[i].size.x / capture_sample_image_resolution_in_dpi << "'"
				<< ", size_y = '" << mask_regions[i].size.y / capture_sample_image_resolution_in_dpi << "'"
				<< ", details = '',"
				"reason_censored = '',"
				"strain_condition_1 = '',"
				"strain_condition_2 = '',"
				"strain_condition_3 = '',"
				"culturing_temperature = '',"
				"experiment_temperature = '',"
				"food_source = '',"
				"environmental_conditions = '',"
				"posture_analysis_model = '" << default_posture_analysis_model << "',"
				"posture_analysis_method = '" << default_posture_analysis_method << "',"
				"worm_detection_model = '" << default_worm_detection_model << "',"
				"position_analysis_model = '" << default_position_analysis_model << "',"
				"time_series_denoising_flag = " << ns_to_string((int)ns_time_series_denoising_parameters::default_strategy()) <<","
				"strain='',"
				"last_posture_analysis_model_used=''"
				"last_posture_analysis_method_used=''"
				"last_worm_detection_model_used=''";

			sql.send_query();
		}
	}
	//look for regions that do not match any mask region, perhaps left over from previous masks
	sql << "SELECT id, mask_region_id FROM sample_region_image_info WHERE sample_id = " << sample_id;
	sql.get_rows(res);
	for (unsigned int i = 0; i < res.size(); i++){
		ns_64_bit region_id = ns_atoi64(res[i][0].c_str());
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
		ns_image_server_image output_image = image.create_storage_for_processed_image(ns_process_analyze_mask,ns_jpeg,&sql);
		bool had_to_use_local_storage;
		ns_image_storage_reciever_handle<ns_component> visualization_output = image_server_const.image_storage.request_storage(
																output_image,
																ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION,_image_chunk_size,&sql,
																had_to_use_local_storage,
																false,
			ns_image_storage_handler::ns_forbid_volatile);

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
													ns_tiff, 1.0, _image_chunk_size,&sql,had_to_use_local_storage,false, ns_image_storage_handler::ns_forbid_volatile);
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
												ns_tiff, 1.0, _image_chunk_size,&sql,had_to_use_local_storage,false, ns_image_storage_handler::ns_forbid_volatile);
		static_mask.pump(b_vis_o.output_stream(),_image_chunk_size);
		b_vis.mark_as_finished_processing(&sql);
		image_server.register_job_duration(ns_process_static_mask,tm.stop());

	}
	sql << "UPDATE sample_region_images SET currently_under_processing=0 WHERE region_info_id = " << region_image.region_info_id;
	sql.send_query();
	//sql << "UPDATE worm_movement SET problem=0,calculated=0 WHERE region_info_id = " << region_image.region_info_id;
	//sql.send_query();
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

			bool grayscale = false;
			if (i == (int)ns_process_thumbnail || i == (int)ns_unprocessed || i == (int)ns_process_threshold || i == (int)ns_process_spatial || i == (int)ns_process_lossy_stretch)
				grayscale = true;

			std::vector<ns_vector_2i> registration_offsets;
			//registration_offsets.resize(1,ns_vector_2i(0,0));
			ns_image_processing_pipeline::make_video(region_image.experiment_id, grayscale,res,reg,registration_offsets,output_basename,sql);


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
				if (res[j][0] != "" && !region_spec.output_at_high_definition){
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

			ns_64_bit event_id = image_server_const.register_server_event(ev,&sql);

			bool grayscale = false;
			if (i == (int)ns_process_thumbnail || i == (int)ns_unprocessed || i == (int)ns_process_threshold || i == (int)ns_process_spatial || i == (int)ns_process_lossy_stretch)
				grayscale = true;

			//calculate registration information if required
			vector<ns_vector_2i> registration_offsets;
			make_video(sample_image.experiment_id,grayscale, files_to_compile,region_spec,registration_offsets,output_basename,sql);

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
void ns_image_processing_pipeline::make_video(const ns_64_bit experiment_id, bool grayscale, const vector< vector<string> > path_and_filenames, const ns_video_region_specification & region_spec, const vector<ns_vector_2i> registration_offsets, const string &output_basename, ns_sql & sql){

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

	if (region_spec.output_at_high_definition) {
		//1440p
		param.max_height = 1440;
		param.max_width = 2560;
	}
	else {
		//1080p
		param.max_height = 1920;
		param.max_width = 1080;
	}

	param.ARG_MAXKEYINTERVAL=12;
	param.ARG_FRAMERATE=10;
	param.ARG_GREYSCALE = grayscale ? 1 : 0;


	string output_filename =  output_basename + ".m4v";

	xvid.run(filenames,param,output_filename,region_spec,labels,registration_offsets);
	wrap_m4v_stream(output_filename,output_basename,filenames.size(),false,sql);

}
#endif

///Takes the image and applies the appropriate mask to make a series of region images.  The resulting images are saved to disk
///and annotated in the database.

void ns_image_processing_pipeline::resize_sample_image(ns_image_server_captured_image & captured_image, ns_sql & sql){
	captured_image.load_from_db(captured_image.captured_images_id,&sql);
	//ns_image_server_event ev("ns_image_processing_pipeline::Creating resized capture image ");
	//ev << captured_image.experiment_name << "::" << captured_image.sample_name << "::" << captured_image.capture_time;
	//image_server_const.register_server_event(ev,&sql);
	ns_high_precision_timer timer;
	timer.start();
//	image_server_const.performance_statistics.starting_job(ns_process_thumbnail);
	try{
		captured_image.load_from_db(captured_image.captured_images_id,&sql);

		ns_image_server_image small_image(captured_image.make_small_image_storage(&sql));
		bool had_to_use_volatile_storage;
		ns_image_storage_reciever_handle<ns_8_bit> small_image_output(image_server_const.image_storage.request_storage(small_image,ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, 1024,&sql,had_to_use_volatile_storage,false, ns_image_storage_handler::ns_forbid_volatile));


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
	ns_image_storage_reciever_handle<ns_8_bit> small_image_output(image_server_const.image_storage.request_storage(small_image,ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, 1024,&sql,had_to_use_volatile_storage,false, ns_image_storage_handler::ns_forbid_volatile));


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


	captured_image.load_from_db(captured_image.captured_images_id, &sql);
	ns_image_server_event ev("ns_image_processing_pipeline::Applying Mask");
	ev << " on " << captured_image.experiment_name << "::" << captured_image.sample_name;
	ev.specifiy_event_subject(captured_image);
	//	ev.specify_processing_job_operation(ns_process_apply_mask);
	ns_64_bit event_id = image_server_const.register_server_event(ev, &sql);
	image_server.add_subtext_to_current_event((std::string("Processing image collected at ") + ns_format_time_string_for_human(captured_image.capture_time) + "\n").c_str(), &sql);
	bool delete_captured_image(false);
	ns_image_type output_file_type;
	float hd_compression_rate_f = 0;
	

	{
		sql << "SELECT delete_captured_images_after_mask, compression_type  FROM experiments WHERE id = " << captured_image.experiment_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() != 1)
			throw ns_ex("ns_image_processing_pipeline::apply_mask()::Could not load experiment data from db (") << captured_image.experiment_id << ")";
		delete_captured_image = res[0][0] != "0";
		if (res[0][1] == "lzw") {
			output_file_type = ns_tiff;
			hd_compression_rate_f = 1.0;
		}
		else if (res[0][1] == "" || res[0][1] == "jp2k")  //jp2k by default!
			output_file_type = ns_jp2k;
		else throw ns_ex("Unknown compression type specified for experiment images:") << res[0][1];

		sql << "SELECT first_frames_are_protected FROM capture_samples WHERE id=" << captured_image.sample_id;
		sql.get_rows(res);

		if (res.size() == 0)
			throw ns_ex("Could not find sample for specified captured image!") << captured_image.sample_id;

		if (res[0][0] == "0") {
			delete_captured_image = false;
			image_server_const.add_subtext_to_current_event("Since no images in this sample have been protected from deletion, this capture image will not be deleted.\n", &sql);
		}
		
		

		if (delete_captured_image) {
			if (captured_image.capture_images_small_image_id == 0) {
				image_server_const.add_subtext_to_current_event("Creating small thumbnail record of the captured image...\n", &sql);
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
				image_server_const.add_subtext_to_current_event("Because this capture image is marked \"Never Delete\", it will not be deleted.\n", &sql);
			}
		}
	
		if (output_file_type == ns_jp2k) {
			std::string hd_compression_rate = image_server_const.get_cluster_constant_value("jp2k_hd_compression_rate", ns_to_string(NS_DEFAULT_JP2K_HD_COMPRESSION), &sql);
			hd_compression_rate_f = atof(hd_compression_rate.c_str());
			if (hd_compression_rate_f <= 0)
				throw ns_ex("Invalid compression rate specified in jp2k_hd_compression_rate cluster constant: ") << hd_compression_rate_f;
		}

	}
	


	unsigned long start_time = ns_current_time();
	//get mask info from db
	sql << "SELECT capture_samples.mask_id, image_masks.image_id, capture_samples.apply_vertical_image_registration,image_masks.resize_factor FROM capture_samples, image_masks WHERE image_masks.id = capture_samples.mask_id AND capture_samples.id = " << captured_image.sample_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0) {

		sql << "SELECT capture_samples.mask_id FROM capture_samples WHERE capture_samples.id = " << captured_image.sample_id;
		ns_sql_result res2;
		sql.get_rows(res2);
		if (res2.size() == 0)
			throw ns_ex("ns_image_processing_pipeline::Specified sample, ") << captured_image.sample_name << "'(" << captured_image.sample_id << ") does not exist in database during mask application.";

		sql << "SELECT capture_samples.mask_id FROM image_masks WHERE image_masks.id = " << res2[0][0];
		ns_sql_result res3;
		sql.get_rows(res3);
		if (res3.size() == 0)
			throw ns_ex("ns_image_processing_pipeline::The mask specified for sample ") << captured_image.sample_name << "'(" << captured_image.sample_id << ") with the id" << res2[0][0] << " is missing from the database.";
		throw ns_ex("ns_image_processing_pipeline::An unspecified error has occurred when trying to locate the mask db record for '") << captured_image.sample_name << "'(" << captured_image.sample_id << ")";
	}
	if (res[0][0] == "" || res[0][0] == "0")
		throw ns_ex("ns_image_processing_pipeline::Specified sample '") << captured_image.sample_name << "'(" << captured_image.sample_id << ") does have mask set.";
	ns_64_bit mask_id = ns_atoi64(res[0][0].c_str()),
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
		image_server_const.add_subtext_to_current_event("Loading images...\n", &sql);

		get_reference_image(captured_image, reference_image, profile_data_source);
		ns_image_server_image im;
		im.id = captured_image.capture_images_image_id;
		image_server.image_registration_profile_cache.get_unlinked_singleton(im, requested_image, profile_data_source);

		image_server_const.add_subtext_to_current_event("Aligning sample image to reference image...", &sql);
		offset = get_vertical_registration(reference_image, requested_image, profile_data_source);
		reference_image.release();
		image_server_const.add_subtext_to_current_event((std::string("Found an offset of (") + ns_to_string(offset.x) + "," + ns_to_string(offset.y) + ")...").c_str(), &sql);

		sql << "UPDATE captured_images SET registration_horizontal_offset='" << offset.x << "', registration_vertical_offset='" << offset.y << "', registration_offset_calculated=1 WHERE id = " << captured_image.captured_images_id;
		sql.send_query();
	}
	else {
		ns_image_server_image im;
		im.id = captured_image.capture_images_image_id;
		image_server.image_registration_profile_cache.get_unlinked_singleton(im, requested_image, profile_data_source);
	}

	//upload statistics on the captured image to the db
	sql << "SELECT image_statistics_id FROM captured_images WHERE id = " << captured_image.captured_images_id;
	
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("Could not load captured_image from db");
	{
		ns_64_bit stats_id(ns_atoi64(res[0][0].c_str()));
		ns_image_statistics image_statistics;
		image_statistics.size.x = requested_image().properties.width;
		image_statistics.size.y = requested_image().properties.height;
		const ns_histogram<unsigned int, ns_8_bit> & hist(requested_image().histogram);
		image_statistics.histogram.resize(hist.size());
		for (unsigned int i = 0; i < hist.size(); i++)
			image_statistics.histogram[i] = hist[i];
		image_statistics.calculate_statistics_from_histogram();
		image_statistics.submit_to_db(stats_id, &sql);
		sql << "UPDATE captured_images SET image_statistics_id= " << stats_id << " WHERE id = " << captured_image.captured_images_id;
		sql.send_query();
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
		ns_64_bit	mask_region_info_id = ns_atoi64(res[i][0].c_str());
		ns_64_bit 	mask_region_id = ns_atoi64(res[i][2].c_str());
		int				mask_region_value = atol(res[i][3].c_str()); // if it could accidentally be < 0 (see below) it ought to be an int
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

			
			
			output_image.filename = new_region_image.filename(&sql);
			ns_add_image_suffix(output_image.filename, output_file_type);

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
		(*mask_splitter.mask_info())[mask_region_value]->reciever = image_server_const.image_storage.request_storage(output_image, output_file_type, hd_compression_rate_f, _image_chunk_size, &sql, had_to_use_volatile_storage, false, ns_image_storage_handler::ns_forbid_volatile);
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
			(*mask_splitter.mask_info())[output_regions[i].mask_color]->image_stats.submit_to_db(image_stats_db_id, &sql, true, false);
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

		image_server_const.add_subtext_to_current_event("Done.\n", &sql);
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
	sample_image_statistics.submit_to_db(sample_stats_db_id, &sql);
	sql << "UPDATE captured_images SET mask_applied=1, image_statistics_id=" << sample_stats_db_id << " WHERE id= " << captured_image.captured_images_id;
	sql.send_query();
	sql.send_query("COMMIT");

	unsigned long stop_time = ns_current_time();
	ev.specify_processing_duration(stop_time - start_time);
	//		image_server_const.update_registered_server_event(event_id,ev);
	ns_64_bit t(timer.stop());
	//	cerr << "total time:" << (t / 1000 / 10)/100.0;
	image_server.register_job_duration(ns_process_apply_mask, t);

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

bool ns_image_processing_pipeline::worm_detection_needs_to_be_redone(const ns_processing_task & s){
	return  s == ns_process_worm_detection ||
		s == ns_process_worm_detection_labels ||
		s == ns_process_region_vis ||
		s == ns_process_accept_vis ||
		s == ns_process_reject_vis ||
		s == ns_process_add_to_training_set;
}

bool ns_image_processing_pipeline::worm_detection_needs_to_be_loadable(const ns_processing_task & s) {
	return preprocessed_step_required(ns_process_worm_detection, s);
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
		case ns_process_posture_vis: return s== ns_process_worm_detection;

		case ns_process_movement_coloring_with_graph:
		case ns_process_movement_coloring_with_survival: return s == ns_process_movement_coloring;

		case ns_process_movement_paths_visualition_with_mortality_overlay:
			 return s == ns_process_movement_paths_visualization;

		case ns_process_movement_paths_visualization:
			 return s == ns_process_worm_detection;

		case ns_process_movement_posture_visualization:
		case ns_process_movement_plate_and_individual_visualization:
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
			worm_detection_needs_to_be_redone((ns_processing_task)i) &&
			!precomputed_images.is_provided((ns_processing_task)i))
			precomputed_images.worm_detection_needs_to_be_redone_now = true;
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


bool ns_lifespan_curve_cache_entry_data::check_to_see_if_cached_is_most_recent(const ns_64_bit & id, ns_sql & sql) {
	sql << "SELECT latest_movement_rebuild_timestamp, latest_by_hand_annotation_timestamp FROM sample_region_image_info WHERE id = " << id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.empty())
		throw ns_ex("ns_lifespan_curve_cache_entry::get_region_data()::Could not find region ") << id << " in db";
	const unsigned long rebuild_timestamp(atol(res[0][0].c_str()));
	const unsigned long annotation_timestamp(atol(res[0][1].c_str()));
	const unsigned long region_timestamp = (rebuild_timestamp > annotation_timestamp) ? rebuild_timestamp : annotation_timestamp;

	return this->region_compilation_timestamp >= region_timestamp;
}

void ns_lifespan_curve_cache_entry_data::load(const ns_annotation_region_data_id & id, ns_sql & sql) {
	//check to see if local data is up to date
	sql << "SELECT latest_movement_rebuild_timestamp, latest_by_hand_annotation_timestamp FROM sample_region_image_info WHERE id = " << id.id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.empty())
		throw ns_ex("ns_lifespan_curve_cache_entry::get_region_data()::Could not find region ") << id.id << " in db";
	const unsigned long rebuild_timestamp(atol(res[0][0].c_str()));
	const unsigned long annotation_timestamp(atol(res[0][1].c_str()));
	const unsigned long region_timestamp = (rebuild_timestamp > annotation_timestamp) ? rebuild_timestamp : annotation_timestamp;

	
	region_compilation_timestamp = region_timestamp;

	ns_machine_analysis_data_loader machine_loader;
	machine_loader.load(id.annotations,metadata.region_id,0,0,sql);
	for (unsigned int i = 0; i < machine_loader.samples.size(); i++){
		for (unsigned int j = 0; j < machine_loader.samples[i].regions.size(); j++){
			compiler.add(machine_loader.samples[i].regions[j]->death_time_annotation_set,machine_loader.samples[i].regions[j]->metadata);
			//hand annotations are already loaded
			ns_hand_annotation_loader hand;
			hand.load_region_annotations(ns_death_time_annotation_set::ns_censoring_data,
				machine_loader.samples[i].regions[j]->metadata.region_id,
				machine_loader.samples[i].regions[j]->metadata.experiment_id,
				machine_loader.samples[i].regions[j]->metadata.experiment_name,
				machine_loader.samples[i].regions[j]->metadata,
				sql);
			compiler.add(hand.annotations);
		}
	}
}

const ns_lifespan_curve_cache_entry_data & ns_lifespan_curve_cache_entry::get_region_entry(const ns_64_bit & region_id) const {
	ns_region_raw_cache::const_iterator region_annotations = region_raw_data_cache.find(region_id);
	if (region_annotations == region_raw_data_cache.end())
		throw ns_ex("ns_lifespan_curve_cache_entry::could not find region ") << region_id << " in region_raw_data_cache.";
	return region_annotations->second;

}
ns_death_time_annotation_compiler::ns_region_list::const_iterator ns_lifespan_curve_cache_entry::get_annotations_for_region(const ns_64_bit & region_id) const {
	const ns_death_time_annotation_compiler & compiler(get_region_entry(region_id).compiler);
	ns_death_time_annotation_compiler::ns_region_list::const_iterator p(compiler.regions.find(region_id));
	if (p == compiler.regions.end())
		throw ns_ex("Could not find region ") << region_id << " in death time annotation compiler.";
	return p;
}
//get_region_data
void ns_lifespan_curve_cache_entry::load_from_external_source(const ns_annotation_region_data_id & id, ns_sql & sql) {
	//first we check all the regions to make sure all the data is loaded.
	sql << "SELECT r.id, r.latest_movement_rebuild_timestamp, r.latest_by_hand_annotation_timestamp "
		"FROM sample_region_image_info as r, capture_samples as s "
		"WHERE  r.censored=0 AND r.sample_id = s.id AND s.experiment_id = " << id.id;
	ns_sql_result res;
	sql.get_rows(res);
	bool reload_experiment_data(false);
	for (unsigned int i = 0; i < res.size(); i++) {
		const ns_64_bit region_id(ns_atoi64(res[i][0].c_str()));
		const unsigned long latest_machine_timestamp(atol(res[i][1].c_str()));
		const unsigned long latest_hand_timestamp(atol(res[i][2].c_str()));
		ns_region_raw_cache::iterator p = region_raw_data_cache.find(region_id);
		if (p == region_raw_data_cache.end()) {
			p = region_raw_data_cache.insert(ns_region_raw_cache::value_type(region_id, ns_lifespan_curve_cache_entry_data())).first;
			reload_experiment_data = true;
		}
		else if (
			p->second.latest_movement_rebuild_timestamp != latest_machine_timestamp ||
			p->second.latest_by_hand_annotation_timestamp != latest_hand_timestamp) {
			reload_experiment_data = true;
		}

		if (reload_experiment_data) {
			p->second.metadata.load_from_db(region_id, "", sql);

			p->second.latest_movement_rebuild_timestamp = latest_machine_timestamp;
			p->second.latest_by_hand_annotation_timestamp = latest_hand_timestamp;
			p->second.load(id, sql);

			ns_lifespan_experiment_set set;
			p->second.compiler.generate_survival_curve_set(set, ns_death_time_annotation::ns_machine_annotations_if_no_by_hand, true, false);
			

			set.generate_aggregate_risk_timeseries(p->second.metadata, true, "",0,p->second.best_guess_survival, p->second.movement_survival, p->second.death_associated_expansion_survival, p->second.risk_timeseries_time,false);

			//here we should identify all possible strain-level risk time series by identifying all values of 
			//p->second.metadata
			//and then running the following code for each;
			/*
				lifespan_curve.cached_strain_risk_timeseries_metadata = m;
				ns_machine_analysis_data_loader machine_loader;
				ns_lifespan_experiment_set experiment_set;
				machine_loader.load_just_survival(experiment_set, 0, 0, m.experiment_id, sql, false, true);
				experiment_set.generage_aggregate_risk_timeseries(m, lifespan_curve.cached_strain_risk_timeseries, lifespan_curve.cached_strain_risk_timeseries_time);
				lifespan_curve.cached_strain_risk_timeseries_metadata = m;
			*/
		}
	}
}


void ns_image_processing_pipeline::overlay_graph(const ns_64_bit region_id,ns_image_whole<ns_component> & image, unsigned long start_time,
	const ns_lifespan_curve_cache_entry & lifespan_curve, ns_sql & sql){

	ns_image_properties lifespan_curve_image_prop,metadata_overlay_prop;
	lifespan_curve_image_prop.width  = (unsigned int)(image.properties().width*(1-1/sqrt(2.0f)));
	lifespan_curve_image_prop.height = (lifespan_curve_image_prop.width/3)*2;
	lifespan_curve_image_prop.components = 3;

	metadata_overlay_prop = lifespan_curve_image_prop;
	metadata_overlay_prop.height/=3;
	metadata_overlay_prop.width/=3;

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
	const ns_lifespan_curve_cache_entry_data & plate_data = lifespan_curve.get_region_entry(region_id);

	vis_gen.create_survival_curve_for_capture_time	(start_time,plate_data.metadata, plate_data.movement_survival,lifespan_curve.cached_strain_movement_survival,
																	plate_data.risk_timeseries_time,lifespan_curve.cached_strain_risk_timeseries_time,"Survival",true,optimize_for_small_graph,metadata_overlay,graph);
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
	*external_source.sql << "SELECT image_id, id, never_delete_image FROM captured_images WHERE sample_id = " << captured_image.sample_id << " AND never_delete_image = 1 AND problem = 0 AND censored = 0 ORDER BY capture_time DESC LIMIT 5";
	ns_sql_result res;
	external_source.sql->get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_processing_pipeline::run_vertical_registration()::Could not perform registration because no reference images exist for the sample") << captured_image.experiment_name << "::" << captured_image.sample_name
				<< ns_do_not_flag_images_as_problem;
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
		<< "Only images that are flagged in captured_images as problem=0, censored=0, never_delete=1 can be used as references for image registration"
		<< ns_do_not_flag_images_as_problem;
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
				ns_image_storage_reciever_handle<ns_8_bit> out_im(image_server_const.image_storage.request_storage(backup_image,ns_tiff,1.0,1024,&sql,had_to_use_volatile_storage,false, ns_image_storage_handler::ns_forbid_volatile));
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
			ns_image_storage_reciever_handle<ns_8_bit> out_im(image_server_const.image_storage.request_storage(dest_im,ns_tiff,1.0,1024,&sql,had_to_use_volatile_storage,false, ns_image_storage_handler::ns_forbid_volatile));
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
					ns_image_storage_reciever_handle<ns_8_bit> out_im(image_server_const.image_storage.request_storage(task_image,type, compression,1024,&sql,had_to_use_volatile_storage,false, ns_image_storage_handler::ns_forbid_volatile));
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


