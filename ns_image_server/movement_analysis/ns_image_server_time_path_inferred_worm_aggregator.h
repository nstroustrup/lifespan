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
				  region_interpolated_id;
};
class ns_image_server_time_path_inferred_worm_aggregator{
public:
	//creates new region interpolated images for locations where worms are inferred to exist.
	//stores their location in the collage image to the context_image_position_in_region_vis_image member 
	//of each inferred element in the solution.
	void create_images_for_solution(const unsigned long region_info_id,ns_time_path_solution & s,ns_sql & sql){
		std::vector<ns_time_path_element *> inferred_elements;
		std::map<unsigned long,ns_image_server_time_path_inferred_worm_aggregator_image_info> region_images_by_time;
		{
			sql << "SELECT capture_time,id,"
				<< ns_processing_step_db_column_name(ns_unprocessed) << "," 
				<< ns_processing_step_db_column_name(ns_process_spatial) << ","
				<< ns_processing_step_db_column_name(ns_process_threshold) << ","
				<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
				<< " FROM sample_region_images WHERE region_info_id = " 
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
				region_images_by_time[in.time] = in;
			}
		}
		
		ns_image_standard unprocessed_image,
							spatial_image,
							thresholded_image;
		unprocessed_image.use_more_memory_to_avoid_reallocations();
		spatial_image.use_more_memory_to_avoid_reallocations();
		thresholded_image.use_more_memory_to_avoid_reallocations();
		double counter(0);
		std::cerr << "Caching images for path gaps and prefixes...";
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
			if (inferred_elements.empty()){
				std::map<unsigned long,ns_image_server_time_path_inferred_worm_aggregator_image_info>::iterator p(region_images_by_time.find(s.timepoints[t].time));
				if (p == region_images_by_time.end())
					throw ns_ex("Could not find timepoint ") << s.timepoints[t].time << " in the database";
				if (p->second.region_interpolated_id != 0){
					ns_image_server_image im;
					im.load_from_db(p->second.region_interpolated_id,&sql);
					image_server.image_storage.delete_from_storage(im,ns_delete_long_term,&sql);
					sql << "UPDATE sample_region_images SET "
						<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
						<< " = 0, worm_interpolation_results_id = 0 WHERE id = " << p->second.id;
					sql.send_query();
				}
				else{
					sql << "UPDATE sample_region_images SET worm_interpolation_results_id = 0 WHERE id = " << p->second.id;
					sql.send_query();
				}
				continue;
			}
			std::map<unsigned long,ns_image_server_time_path_inferred_worm_aggregator_image_info>::iterator p(region_images_by_time.find(s.timepoints[t].time));
			if (p == region_images_by_time.end())
				throw ns_ex("Could not find timepoint ") << s.timepoints[t].time << " in the database";

			//load images
			ns_image_server_image im;
			im.load_from_db(p->second.raw_image_id,&sql);
			image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(&unprocessed_image,1024);
			im.load_from_db(p->second.spatial_id,&sql);
			image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(&spatial_image,1024);
			im.load_from_db(p->second.threshold_id,&sql);
			image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(&thresholded_image,1024);
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
			ns_image_server_captured_image_region reg_im;
			reg_im.load_from_db(p->second.id,&sql);

			const ns_image_standard & worm_collage(results.generate_region_collage(unprocessed_image,spatial_image,thresholded_image));
			std::vector<ns_vector_2i> positions;
			results.worm_collage.info().image_locations_in_collage((unsigned long)worms.size(),positions);
			for (unsigned int i = 0; i < positions.size(); i++){
				inferred_elements[i]->context_image_position_in_region_vis_image = positions[i];
			}
			
			reg_im.register_worm_detection(&results,true,sql);

			bool had_to_use_volatile_storage;
			ns_image_server_image region_bitmap = reg_im.create_storage_for_processed_image(ns_process_region_interpolation_vis,ns_tiff,&sql);
			ns_image_storage_reciever_handle<ns_8_bit> region_bitmap_o = image_server.image_storage.request_storage(
														region_bitmap,
														ns_tiff, 1024,&sql,
													had_to_use_volatile_storage,
													false,
													false);
			worm_collage.pump(region_bitmap_o.output_stream(),1024);
			
		}
	}

};

#endif

