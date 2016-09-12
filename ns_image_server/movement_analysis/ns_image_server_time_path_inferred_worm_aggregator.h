#ifndef NS_INFERRED_WORM_AGGREGATOR
#define NS_INFERRED_WORM_AGGREGATOR

#include "ns_image_server_images.h"
#include "ns_time_path_solver.h"
#include "ns_detected_worm_info.h"
#include <map>
#include <iostream>

struct ns_image_server_time_path_inferred_worm_aggregator_image_info{
	unsigned long time;
	ns_64_bit	id,
				raw_image_id,
				  spatial_id,
				  threshold_id,
				  region_interpolated_id,
				  worm_detection_results_id,
				  worm_interpolated_results_id;
	bool problem,censored;
	//the lifespan machine can occasionally duplicate a time point.
	//we need to keep those up to date as well so they remain identical
	//and don't cause book keeping problems.

	//the duplication appears to occur by a sample image being masked twice simulaneously
	//by two image processing servers.  This should be fixable--probably a race condition
	//in the job processing code.
	std::vector<ns_image_server_time_path_inferred_worm_aggregator_image_info> duplicates_of_this_time_point;

	void remove_inferred_element_from_db(ns_sql & sql){
		if (region_interpolated_id != 0){
			ns_image_server_image im;
			im.load_from_db(region_interpolated_id,&sql);
			image_server.image_storage.delete_from_storage(im,ns_delete_long_term,&sql);
			sql << "UPDATE sample_region_images SET "
				<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
				<< " = 0, worm_interpolation_results_id = 0 WHERE id = " << id;
			sql.send_query();

		}
		else{
			sql << "UPDATE sample_region_images SET worm_interpolation_results_id = 0 WHERE id = " << id;
			sql.send_query();
		}
		sql << "DELETE FROM worm_detection_results WHERE id = " << worm_interpolated_results_id;
		sql.send_query();
	}
	ns_image_server_time_path_inferred_worm_aggregator_image_info():problem(false),censored(false){}
};
class ns_image_server_time_path_inferred_worm_aggregator{
public:
	//creates new region interpolated images for locations where worms are inferred to exist.
	//stores their location in the collage image to the context_image_position_in_region_vis_image member 
	//of each inferred element in the solution.

	//returns false if corrupt images were found and the time path solution needs to be rebuilt
	bool create_images_for_solution(const ns_64_bit region_info_id,ns_time_path_solution & s,ns_sql & sql){
		std::vector<ns_time_path_element *> inferred_elements;
		std::map<unsigned long,ns_image_server_time_path_inferred_worm_aggregator_image_info> region_images_by_time;
		{
			sql << "SELECT capture_time,id,"
				<< ns_processing_step_db_column_name(ns_unprocessed) << "," 
				<< ns_processing_step_db_column_name(ns_process_spatial) << ","
				<< ns_processing_step_db_column_name(ns_process_threshold) << ","
				<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
				<< ", problem, censored, worm_detection_results_id,worm_interpolation_results_id FROM sample_region_images WHERE region_info_id = " 
				<< region_info_id;

			ns_sql_result res;
			sql.get_rows(res);
			for (unsigned int i = 0; i < res.size(); i++){
				ns_image_server_time_path_inferred_worm_aggregator_image_info in;
				in.time = atol(res[i][0].c_str());
				in.id =  ns_atoi64(res[i][1].c_str());
				in.raw_image_id = ns_atoi64(res[i][2].c_str());
				in.spatial_id = ns_atoi64(res[i][3].c_str());
				in.threshold_id = ns_atoi64(res[i][4].c_str());
				in.region_interpolated_id = ns_atoi64(res[i][5].c_str());
				in.problem = ns_atoi64(res[i][6].c_str()) > 0;
				in.censored = ns_atoi64(res[i][7].c_str()) > 0;
				in.worm_detection_results_id = ns_atoi64(res[i][8].c_str()) ;
				in.worm_interpolated_results_id = ns_atoi64(res[i][9].c_str()); 
				std::map<unsigned long,ns_image_server_time_path_inferred_worm_aggregator_image_info>::iterator p;
				p = region_images_by_time.find(in.time);
				if (p == region_images_by_time.end())
					region_images_by_time[in.time] = in;
				else{
					if (in.id == p->second.id)
						throw ns_ex("A duplicate sample_region_image **with the same region_id** was found!");
					//there's a duplicate sample_region_image at this time point.
					p->second.duplicates_of_this_time_point.push_back(in);
				}
			}
		}
		
		ns_image_standard unprocessed_image,
							spatial_image,
							thresholded_image;
		unprocessed_image.use_more_memory_to_avoid_reallocations();
		spatial_image.use_more_memory_to_avoid_reallocations();
		thresholded_image.use_more_memory_to_avoid_reallocations();
		double counter(0);
		//std::cerr << "Caching images for path gaps and prefixes...";
		bool needs_to_be_rebuilt(false);

		for (unsigned int t = 0; t < s.timepoints.size(); t++){
			if (s.timepoints.size()/20 < counter){
				counter = 0;
				std::cerr << (t*100)/s.timepoints.size() << "%...";
			}
			counter++;
			inferred_elements.resize(0);
			for (unsigned int i = 0; i < s.timepoints[t].elements.size(); i++)
				if (s.timepoints[t].elements[i].inferred_animal_location)
					inferred_elements.push_back(&s.timepoints[t].elements[i]);
			
			std::map<unsigned long,ns_image_server_time_path_inferred_worm_aggregator_image_info>::iterator current_region_image(region_images_by_time.find(s.timepoints[t].time));
				
			if (current_region_image == region_images_by_time.end())
				throw ns_ex("Could not find timepoint ") << s.timepoints[t].time << " in the database";

			if (inferred_elements.empty()){
				current_region_image->second.remove_inferred_element_from_db(sql);
				for (unsigned int i = 0; i < current_region_image->second.duplicates_of_this_time_point.size(); i++)
					current_region_image->second.duplicates_of_this_time_point[i].remove_inferred_element_from_db(sql);
				continue;
			}

			//load images
			ns_image_server_image im;
			ns_ex ex;
			bool unprocessed_problem(false),processed_problem(false);
			try{
				im.load_from_db(current_region_image->second.raw_image_id,&sql);

				image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(&unprocessed_image,1024);
			}
			catch(ns_ex & ex_){
				ex = ex_;
				unprocessed_problem = true;
			}
			try{
				im.load_from_db(current_region_image->second.spatial_id,&sql);
				image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(&spatial_image,1024);
				im.load_from_db(current_region_image->second.threshold_id,&sql);
				image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(&thresholded_image,1024);
				
			}
			catch(ns_ex & ex_){
				processed_problem = true;
				ex = ex_;
			}
			//if there's an error, flag it, remove the problematic inferred animals from the solution, and remove any existing data from the db.
			if (unprocessed_problem || processed_problem){
				ns_64_bit error_id = image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_ex("Skipping and flagging a corrupt or missing image:") << ex.text());
				needs_to_be_rebuilt = true;
				if (unprocessed_problem){
					sql << "UPDATE sample_region_images SET problem=" << error_id << " WHERE id = " << current_region_image->second.id;
					sql.send_query();
				}
				if (processed_problem){
					sql << "UPDATE sample_region_images SET " << ns_processing_step_db_column_name(ns_process_spatial) << " = 0,"
							<< ns_processing_step_db_column_name(ns_process_threshold) << " = 0,"
							<< ns_processing_step_db_column_name(ns_process_lossy_stretch) << " = 0,"
							<< ns_processing_step_db_column_name(ns_process_worm_detection) << " = 0,"
							<< ns_processing_step_db_column_name(ns_process_region_vis) << " = 0,"
							<< ns_processing_step_db_column_name(ns_process_worm_detection_labels) << " = 0,"
							<< ns_processing_step_db_column_name(ns_process_interpolated_vis) << " = 0,"
							<<" worm_detection_results_id=0, worm_interpolation_results_id=0 "
							<< "WHERE id = " <<  current_region_image->second.id;
					sql.send_query();
					sql << "DELETE FROM worm_detection_results WHERE id = " << current_region_image->second.worm_detection_results_id 
						<< " OR id = " << current_region_image->second.worm_interpolated_results_id;
					sql.send_query();
				}
				
				s.remove_inferred_animal_locations(t,true);
			
				current_region_image->second.remove_inferred_element_from_db(sql);
				for (unsigned int i = 0; i < current_region_image->second.duplicates_of_this_time_point.size(); i++)
					current_region_image->second.duplicates_of_this_time_point[i].remove_inferred_element_from_db(sql);
				
				continue;

			}
			ns_image_worm_detection_results results;
			results.id = 0;
			
			ns_image_properties prop(unprocessed_image.properties());
			prop.components = 1;
			std::vector<ns_detected_worm_info> & worms(results.replace_actual_worms_access());
			worms.resize(inferred_elements.size());
			
			//ns_font & f(font_server.default_font());
			//f.set_height(18);

			for (unsigned int i = 0; i < inferred_elements.size(); i++){
				ns_detected_worm_info & worm(worms[i]);
				
				worm.region_size = inferred_elements[i]->region_size;
				worm.region_position_in_source_image = inferred_elements[i]->region_position;
				worm.context_image_size = inferred_elements[i]->context_image_size;
				worm.context_position_in_source_image = inferred_elements[i]->context_image_position;
				worm.movement_state = ns_movement_not_calculated;
	//			f.draw_grayscale(worm.region_position_in_source_image.x + worm.region_size.x/2,worm.region_position_in_source_image.y + worm.region_size.y/2,
		//						 255,std::string("w") + ns_to_string(t),spatial_image);

				//copy images over for new inferred detected worm
				prop.height = worm.region_size.y;
				prop.width = worm.region_size.x;
				worm.bitmap().prepare_to_recieve_image(prop);
				for (unsigned int y = 0; y < worm.region_size.y; y++)
					for (unsigned int x = 0; x < worm.region_size.x; x++)
						worm.bitmap()[y][x] = thresholded_image[y+worm.region_position_in_source_image.y][x+worm.region_position_in_source_image.x]; 
			}
			results.replace_actual_worms();
			const ns_image_standard & worm_collage(results.generate_region_collage(unprocessed_image,spatial_image,thresholded_image));

			std::vector<ns_vector_2i> positions;
			results.worm_collage.info().image_locations_in_collage((unsigned long)worms.size(),positions);
			for (unsigned int i = 0; i < positions.size(); i++){
				inferred_elements[i]->context_image_position_in_region_vis_image = positions[i];
			}
			
			ns_image_server_captured_image_region reg_im;
			reg_im.load_from_db(current_region_image->second.id,&sql);
			reg_im.register_worm_detection(&results,true,sql);

			bool had_to_use_volatile_storage;
			ns_image_server_image region_bitmap = reg_im.create_storage_for_processed_image(ns_process_region_interpolation_vis,ns_tiff,&sql);
			unsigned long write_attempts = 0;
			while (true) {
				try {
					ns_image_storage_reciever_handle<ns_8_bit> region_bitmap_o = image_server.image_storage.request_storage(
						region_bitmap,
						ns_tiff, 1024, &sql,
						had_to_use_volatile_storage,
						false,
						false);
					worm_collage.pump(region_bitmap_o.output_stream(), 1024);
					break;
				}
				catch (ns_ex & ex) {
					image_server.register_server_event(ex, &sql);
					if (write_attempts == 4)
						throw ex;
					write_attempts++;
				}
			}
			for (unsigned int i = 0; i < current_region_image->second.duplicates_of_this_time_point.size(); i++){
				ns_image_server_captured_image_region reg_im2;
				reg_im2.load_from_db(current_region_image->second.duplicates_of_this_time_point[i].id,&sql);
				reg_im2.register_worm_detection(&results,true,sql);

				sql << "UPDATE " << ns_processing_step_db_table_name(ns_process_region_interpolation_vis) << " SET " 
								 << ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
								 << " = " << region_bitmap.id << " WHERE id = " << reg_im2.region_images_id;
				sql.send_query();

			}
		}
		return needs_to_be_rebuilt;
	}

};

#endif

