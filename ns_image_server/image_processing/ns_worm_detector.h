#ifndef NS_WORM_DETECTOR
#define NS_WORM_DETECTOR
#include "ns_identify_contiguous_bitmap_regions.h"
#include "ns_detected_object.h"



class ns_object_to_segment_cluster_converter{
public:
///Generates a Delauny triangular mesh for each region present in regions
	void use_delauny_method(std::vector<ns_detected_object *> objects, const bool multiple_worm_disambiguation=true,const std::string & debug_spine_output_filename = "");
	void use_zhang_thinning(std::vector<ns_detected_object *> objects, const bool multiple_worm_disambiguation=true);
};


template<class whole_image>
class ns_worm_detector{
public:
	ns_image_worm_detection_results * run(const ns_64_bit region_info_id,const unsigned long capture_time,const whole_image & unprocessed_image,const whole_image & thresholded_image, const whole_image & spatial_median_image, const ns_image_standard * static_mask, const unsigned int & minimum_region_area, const unsigned int &maximum_region_area, const unsigned int &maximum_region_diagonal, const ns_svm_model_specification & model, const unsigned long maximum_number_of_objects, ns_sql * sql_for_debug_output,const std::string & spine_debug_filename_output="", const ns_detected_worm_info::ns_visualization_type visualization_type=ns_detected_worm_info::ns_vis_raster,ns_whole_image_region_stats image_region_stats = ns_whole_image_region_stats::null(),const bool do_multiple_worm_disambiguation=true){
		ns_detected_object_manager object_manager;
		//std::cerr << "Identifying objects";
		ns_identify_contiguous_bitmap_regions(thresholded_image,object_manager.objects);
		unsigned long init_object_count((unsigned long)object_manager.objects.size());
		object_manager.constrain_region_area(minimum_region_area,maximum_region_area,maximum_region_diagonal);


		image_server_const.add_subtext_to_current_event(ns_to_string(object_manager.objects.size()) + "/" + ns_to_string(init_object_count) + ns_to_string(" objects fall within size limits.\n") , sql_for_debug_output);

		if (object_manager.objects.size() > ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_objects_per_image))
			throw ns_ex("ns_worm_detector::run()::") << (unsigned int)object_manager.objects.size() << " objects identified, more than the specificed " << ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_objects_per_image) << " cutoff.  Contamination/condensation suspected.";

		ns_image_worm_detection_results * tmp(run(region_info_id,capture_time,object_manager,unprocessed_image,spatial_median_image,static_mask,model,maximum_number_of_objects,sql_for_debug_output,spine_debug_filename_output,visualization_type,image_region_stats));
		tmp->generate_region_collage(unprocessed_image,spatial_median_image,thresholded_image);
		return tmp;
	}

	///detect all worms in the image, and return information about each worm and non-worm image.
	template<class whole_im>
	ns_image_worm_detection_results * run(const ns_64_bit region_info_id,const unsigned long capture_time,ns_detected_object_manager & detected_objects, const whole_im & unprocessed_image,const whole_im & spatial_median_image, const ns_image_standard * static_mask, const ns_svm_model_specification & model, const unsigned long maximum_number_of_objects, ns_sql * sql_for_debug_output, const std::string & debug_filename_output="", const ns_detected_worm_info::ns_visualization_type visualization_type=ns_detected_worm_info::ns_vis_raster, ns_whole_image_region_stats whole_image_region_stats = ns_whole_image_region_stats::null(),const bool do_multiple_worm_disambiguation=true){
		const unsigned long start_time = ns_current_time();
		const std::vector<ns_detected_object *>::size_type s(detected_objects.objects.size());
		hande_static_mask(detected_objects,spatial_median_image, static_mask);

		image_server_const.add_subtext_to_current_event(ns_to_string(s - detected_objects.objects.size()) + "objects were removed by the static mask.\n", sql_for_debug_output);
	
		detected_objects.convert_bitmaps_into_node_graphs(spatial_median_image.properties().resolution, debug_filename_output);
		detected_objects.calculate_segment_topologies_from_node_graphs(do_multiple_worm_disambiguation);
		ns_image_worm_detection_results * results = 0;
		results = new ns_image_worm_detection_results();

		try{
			results->region_info_id = region_info_id;
			results->capture_time = capture_time;
	//		cerr << "Generating region results\n";
			//cerr << regions.size() << " regions.\n";
			results->process_segment_cluster_solutions(detected_objects.objects,spatial_median_image,unprocessed_image,visualization_type,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_putative_worms_per_image), sql_for_debug_output);
			results->set_whole_image_region_stats(whole_image_region_stats);
			if (whole_image_region_stats==ns_whole_image_region_stats::null())
				image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("ns_worm_detector::run()::Warning: No whole image region stats were specified"));
			results->calculate_image_region_stats();
			results->sort_putative_worms(model);

			for (unsigned int i = 0; i < detected_objects.interpolated_objects.size(); i++)
				results->add_interpolated_worm_area(detected_objects.interpolated_objects[i]);

			if (results->number_of_actual_worms() > maximum_number_of_objects)
				throw ns_ex("ns_worm_detector::run()::") << results->number_of_actual_worms() << " worms detected in image, more than the specified " << maximum_number_of_objects << " maximum.";
			image_server_const.add_subtext_to_current_event(ns_to_string(results->number_of_actual_worms()) + " worms were detected\n", sql_for_debug_output);
	
			return results;
		}
		catch(...){
			delete results;
			throw;
		}
	}

private:



	void hande_static_mask(ns_detected_object_manager & detected_objects, const ns_image_standard & grayscale_image, const ns_image_standard * static_mask){
		if (static_mask != 0){
			if (!(static_mask->properties() == grayscale_image.properties()))
				throw ns_ex("ns_detected_object_identifier::Static mask size does not match grayscale image size ( ")
					<< static_mask->properties().width << "x" << static_mask->properties().height << " vs "
					<< grayscale_image.properties().width << "x" << grayscale_image.properties().height << ")";
			detected_objects.remove_objects_found_in_static_mask(*static_mask);
		}
	}

};


#endif
