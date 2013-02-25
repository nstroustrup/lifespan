#include "ns_worm_detection_constants.h"
using namespace std;

void ns_worm_detection_constants::init(){
	const_1200[ns_worm_detection_constant::connected_object_area_cutoff]=1000;
	const_1200[ns_worm_detection_constant::maximum_worm_region_area]=10000;
	const_1200[ns_worm_detection_constant::minimum_worm_region_area]=40;
	const_1200[ns_worm_detection_constant::maximum_region_diagonal]=140;
	const_1200[ns_worm_detection_constant::worm_end_node_margin]=4;
	const_1200[ns_worm_detection_constant::spine_visualization_resize_factor]=8;
	const_1200[ns_worm_detection_constant::spine_smoothing_radius]=8;
	const_1200[ns_worm_detection_constant::spine_visualization_output_resolution]=1;
	const_1200[ns_worm_detection_constant::maximum_dimension_size_for_small_images] = 1200;
	const_1200[ns_worm_detection_constant::tiff_compression_intensity_crop_value]= 0;
	const_1200[ns_worm_detection_constant::fast_movement_lower_distance_threshold] = 7;
	const_1200[ns_worm_detection_constant::minimum_worm_hole_size] =20; //???made up number
	const_3200[ns_worm_detection_constant::nearest_neighbor_maximum_distance] = 50;  //??Made up number


	const_3200[ns_worm_detection_constant::connected_object_area_cutoff]=9000;
	const_3200[ns_worm_detection_constant::maximum_worm_region_area]=10000;
	const_3200[ns_worm_detection_constant::minimum_worm_region_area]=360;
	const_3200[ns_worm_detection_constant::maximum_region_diagonal]=420;
	const_3200[ns_worm_detection_constant::worm_end_node_margin]=0;
	const_3200[ns_worm_detection_constant::spine_visualization_resize_factor]=2;
	const_3200[ns_worm_detection_constant::spine_smoothing_radius]=5;
	const_3200[ns_worm_detection_constant::spine_visualization_output_resolution]=2;
	const_3200[ns_worm_detection_constant::maximum_dimension_size_for_small_images] = 8000;
	const_3200[ns_worm_detection_constant::tiff_compression_intensity_crop_value]= 0;
	const_3200[ns_worm_detection_constant::fast_movement_lower_distance_threshold] = 30;
	const_3200[ns_worm_detection_constant::minimum_worm_hole_size] = 110;
	const_3200[ns_worm_detection_constant::nearest_neighbor_maximum_distance] = 150;

	const_resolution_independant[ns_worm_detection_constant::overlap_required_to_interpolate_threshold_region_as_worm] = 9;

	const_resolution_independant[ns_worm_detection_constant::allowed_drift_distance_for_objects_during_interpolation] = 22;
	const_resolution_independant[ns_worm_detection_constant::number_of_time_points_to_use_for_interpolation] = 8;
	const_resolution_independant[ns_worm_detection_constant::number_of_time_points_required_to_infer_an_object_during_interpolation] = 8;
	const_resolution_independant[ns_worm_detection_constant::overlap_required_to_infer_an_object_during_interpolation] = 3;

	const_resolution_independant[ns_worm_detection_constant::maximum_number_of_objects_per_image] = 1500;
	const_resolution_independant[ns_worm_detection_constant::maximum_number_of_putative_worms_per_image] = 4000;
	const_resolution_independant[ns_worm_detection_constant::maximum_number_of_actual_worms_per_image] = 300;
	const_resolution_independant[ns_worm_detection_constant::allowed_drift_distance_for_objects_during_static_mask_creation] = 22;
	const_resolution_independant[ns_worm_detection_constant::proportion_of_early_time_points_present_required_during_static_mask_creation] = 7;
	const_resolution_independant[ns_worm_detection_constant::proportion_of_middle_time_points_present_required_during_static_mask_creation] = 5;
	const_resolution_independant[ns_worm_detection_constant::proportion_of_late_time_points_present_required_during_static_mask_creation] = 5;
	const_resolution_independant[ns_worm_detection_constant::maximum_number_of_worms_per_multiple_worm_cluster] = 2;
}


ns_worm_detection_constant_set<ns_worm_detection_constant::constant_type> ns_worm_detection_constants::const_1200;
ns_worm_detection_constant_set<ns_worm_detection_constant::constant_type> ns_worm_detection_constants::const_3200;
ns_worm_detection_constant_set<ns_worm_detection_constant::resolution_independant_constant_type> ns_worm_detection_constants::const_resolution_independant;


std::string ns_classifier_label(const ns_detected_worm_classifier & c){
	switch(c){
		case ns_stat_pixel_area:								return "Pixel Area";
		case ns_stat_bitmap_width:								return "Rectangular Width";
		case ns_stat_bitmap_height:								return "Rectangular Height";
		case ns_stat_bitmap_diagonal:							return "Rectangular Diagonal";
		case ns_stat_spine_length:								return "Spine Length";
		case ns_stat_average_width:								return "Average Width";
		case ns_stat_max_width:									return "Maximum Width";
		case ns_stat_min_width:									return "Minimum Width";
		case ns_stat_width_variance:							return "Variance in Width";
		case ns_stat_width_at_center:							return "Width at Center";
		case ns_stat_width_at_end_0:							return "Width at Front";
		case ns_stat_width_at_end_1:							return "Width at Rear";
		case ns_stat_spine_length_to_area:						return "SPLength / Pixel Area";
		case ns_stat_spine_length_to_bitmap_width_ratio:		return "SPLength / Rect Width";
		case ns_stat_spine_length_to_bitmap_height_ratio:		return "SPLength / Rect Height";
		case ns_stat_spine_length_to_bitmap_diagonal_ratio:		return "SSPLength / Rect Diagonal";
		case ns_stat_spine_length_to_max_width_ratio:			return "SPLength / Maximum Width";
		case ns_stat_spine_length_to_average_width:				return "SPLength / Average Width";
		case ns_stat_end_width_to_middle_width_ratio_0:			return "SPLength / Width at Front";
		case ns_stat_end_width_to_middle_width_ratio_1:			return "SPLength / Width at Rear";
		case ns_stat_end_width_ratio:							return "Ratio of end widths";
		case ns_stat_average_curvature:							return "Average Curvature";
		case ns_stat_max_curvature:								return "Maxiumum Curvature";
		case ns_stat_total_curvature:							return "Total Curvature";
		case ns_stat_curvature_x_intercept:						return "Curvature X Intercepts";
		case ns_stat_curvature_variance:						return "Curvature Variance";
		case ns_stat_distance_between_ends:						return "Distance between ends";
		case ns_stat_absolute_intensity_average:						return "Absolute Intensity";
		case ns_stat_absolute_intensity_variance:						return "Absolute Intensity Variance";
		case ns_stat_absolute_intensity_skew:							return "Absolute Intensity Skew";
		case ns_stat_absolute_intensity_roughness_1:					return "Absolute Intensity Roughness (Entropy)";
		case ns_stat_absolute_intensity_roughness_2:					return "Absolute Intensity Roughness (Spatial Variance)";
		case ns_stat_absolute_intensity_max:							return "Absolute Intensity Maximum ";
		case ns_stat_absolute_intensity_spine_average:					return "Absolute Intensity Average Along Spine";
		case ns_stat_absolute_intensity_of_neighborhood:				return "Absolute Intensity Average in Neighborhood";
		case ns_stat_absolute_intensity_dark_pixel_average:				return "Absolute Intensity darkest 20% of pixels";
		case ns_stat_absolute_intensity_dark_pixel_area:				return "Absolute Intensity darkest 20% area";
		case ns_stat_absolute_intensity_distance_from_neighborhood:		return "Absolute Intensity Dist from Nbrhd";
		case ns_stat_absolute_intensity_spine_variance:					return "Absolute Intensity Variance Along Spine";
		case ns_stat_absolute_intensity_containing_image_region_average:return "Absolute Intensity of Region";
		case ns_stat_absolute_intensity_normalized_average:				return "Absolute Intensity Normalized Average";
		case ns_stat_absolute_intensity_normalized_max:					return "Absolute Intensity Normalized Max";
		case ns_stat_absolute_intensity_normalized_spine_average:		return "Absolute Intensity Normalized Avg Along Spine";
		case ns_stat_relative_intensity_average:						return "Relative Intensity";
		case ns_stat_relative_intensity_variance:						return "Relative Intensity Variance";
		case ns_stat_relative_intensity_skew:							return "Relative Intensity Skew";
		case ns_stat_relative_intensity_roughness_1:					return "Relative Intensity Roughness (Entropy)";
		case ns_stat_relative_intensity_roughness_2:					return "Relative Intensity Roughness (Spatial Variance)";
		case ns_stat_relative_intensity_max:							return "Relative Intensity Maximum ";
		case ns_stat_relative_intensity_spine_average:					return "Relative Intensity Average Along Spine";
		case ns_stat_relative_intensity_of_neighborhood:				return "Relative Intensity Average in Neighborhood";
		case ns_stat_relative_intensity_dark_pixel_average:				return "Relative Intensity darkest 20% of pixels";
		case ns_stat_relative_intensity_dark_pixel_area:				return "Relative Intensity darkest 20% area";
		case ns_stat_relative_intensity_distance_from_neighborhood:		return "Relative Intensity Dist from Nbrhd";
		case ns_stat_relative_intensity_spine_variance:					return "Relative Intensity Variance Along Spine";
		case ns_stat_relative_intensity_containing_image_region_average:return "Relative Intensity of Region";
		case ns_stat_relative_intensity_normalized_average:				return "Relative Intensity Normalized Average";
		case ns_stat_relative_intensity_normalized_max:					return "Relative Intensity Normalized Max";
		case ns_stat_relative_intensity_normalized_spine_average:		return "Relative Intensity Normalized Avg Along Spine";
		case ns_stat_edge_length:										return "Edge Area";
		case ns_stat_edge_to_area_ratio:								return "Ratio of Edge Area to Object Area";
		case ns_stat_intensity_profile_edge:							return "Intensity Profile at Edge";
		case ns_stat_intensity_profile_center:							return "Intensity Profile at Center";
		case ns_stat_intensity_profile_max:								return "Intensity Profile maximum";
		case ns_stat_intensity_profile_variance:						return "Intensity Profile Variance";
		case ns_stat_number_of_stats:									return "Number of Statistics used";
		default: throw ns_ex("Unknown worm classifier") << (unsigned int)c;
	}
}

std::string ns_classifier_abbreviation(const ns_detected_worm_classifier & c){
	switch(c){
		case ns_stat_pixel_area:								return "pixel_area";
		case ns_stat_bitmap_width:								return "bitmap_width";
		case ns_stat_bitmap_height:								return "bitmap_height";
		case ns_stat_bitmap_diagonal:							return "bitmap_diagonal";
		case ns_stat_spine_length:								return "spine_length";
		case ns_stat_distance_between_ends:						return "distance_between_ends";
		case ns_stat_average_width:								return "width_average";
		case ns_stat_max_width:									return "width_max";
		case ns_stat_min_width:									return "width_min";
		case ns_stat_width_variance:							return "width_variance";
		case ns_stat_width_at_center:							return "width_at_center";
		case ns_stat_width_at_end_0:							return "width_at_end_0";
		case ns_stat_width_at_end_1:							return "width_at_end_1";
		case ns_stat_spine_length_to_area:						return "spine_length_to_area";
		case ns_stat_spine_length_to_bitmap_width_ratio:		return "spine_length_to_bitmap_width_ratio";
		case ns_stat_spine_length_to_bitmap_height_ratio:		return "spine_length_to_bitmap_height_ratio";
		case ns_stat_spine_length_to_bitmap_diagonal_ratio:		return "spine_length_to_bitmap_diagonal_ratio";
		case ns_stat_spine_length_to_max_width_ratio:			return "spine_length_to_max_width_ratio";
		case ns_stat_spine_length_to_average_width:				return "spine_length_to_average_width";
		case ns_stat_end_width_to_middle_width_ratio_0:			return "end_width_to_middle_width_ratio_0";
		case ns_stat_end_width_to_middle_width_ratio_1:			return "end_width_to_middle_width_ratio_1";
		case ns_stat_end_width_ratio:							return "end_width_ratio";
		case ns_stat_average_curvature:							return "curvature_average";
		case ns_stat_max_curvature:								return "curvature_maximum";
		case ns_stat_total_curvature:							return "curvature_total";
		case ns_stat_curvature_variance:						return "curvature_variance";	
		case ns_stat_curvature_x_intercept:						return "curvature_x_intercepts";
		case ns_stat_absolute_intensity_average:							return "intensity_abs_average";
		case ns_stat_absolute_intensity_max:								return "intensity_abs_maximum";
		case ns_stat_absolute_intensity_variance:						return "intensity_abs_variance";
		case ns_stat_absolute_intensity_skew:							return "intensity_abs_skew";
		case ns_stat_absolute_intensity_roughness_1:						return "intensity_abs_roughness_entropy";
		case ns_stat_absolute_intensity_roughness_2:						return "intensity_abs_roughness_spatial_var";
		case ns_stat_absolute_intensity_of_neighborhood:					return "intensity_abs_neighborhood";
		case ns_stat_absolute_intensity_distance_from_neighborhood:		return "intensity_abs_dist_from_neighborhood";
		case ns_stat_absolute_intensity_dark_pixel_average:			return "intensity_abs_dark_avg";
		case ns_stat_absolute_intensity_dark_pixel_area:				return "intensity_abs_dark_area";
		case ns_stat_absolute_intensity_spine_average:					return "intensity_abs_spine_average";
		case ns_stat_absolute_intensity_spine_variance:					return "intensity_abs_spine_variance";
		case ns_stat_absolute_intensity_containing_image_region_average:	return "intensity_abs_containing_image_average";
		case ns_stat_absolute_intensity_normalized_average:				return "intensity_abs_normalized_average";
		case ns_stat_absolute_intensity_normalized_max:					return "intensity_abs_normalized_max";
		case ns_stat_absolute_intensity_normalized_spine_average:		return "intensity_abs_normalized_spine_average";
		case ns_stat_relative_intensity_average:							return "intensity_rel_average";
		case ns_stat_relative_intensity_max:								return "intensity_rel_maximum";
		case ns_stat_relative_intensity_variance:						return "intensity_rel_variance";
		case ns_stat_relative_intensity_skew:							return "intensity_rel_skew";
		case ns_stat_relative_intensity_roughness_1:						return "intensity_rel_roughness_entropy";
		case ns_stat_relative_intensity_roughness_2:						return "intensity_rel_roughness_spatial_var";
		case ns_stat_relative_intensity_of_neighborhood:					return "intensity_rel_neighborhood";
		case ns_stat_relative_intensity_distance_from_neighborhood:		return "intensity_rel_dist_from_neighborhood";
		case ns_stat_relative_intensity_dark_pixel_average:			return "intensity_rel_dark_avg";
		case ns_stat_relative_intensity_dark_pixel_area:				return "intensity_rel_dark_area";
		case ns_stat_relative_intensity_spine_average:					return "intensity_rel_spine_average";
		case ns_stat_relative_intensity_spine_variance:					return "intensity_rel_spine_variance";
		case ns_stat_relative_intensity_containing_image_region_average:	return "intensity_rel_containing_image_average";
		case ns_stat_relative_intensity_normalized_average:				return "intensity_rel_normalized_average";
		case ns_stat_relative_intensity_normalized_max:					return "intensity_rel_normalized_max";
		case ns_stat_relative_intensity_normalized_spine_average:		return "intensity_rel_normalized_spine_average";
		case ns_stat_edge_length:										return "edge_area";
		case ns_stat_edge_to_area_ratio:								return "edge_object_area_ratio";
		case ns_stat_intensity_profile_edge:							return "intensity_profile_edge";
		case ns_stat_intensity_profile_center:							return "intensity_profile_center";
		case ns_stat_intensity_profile_max:								return "intensity_profile_max";
		case ns_stat_intensity_profile_variance:						return "intensity_profile_variance";
		case ns_stat_number_of_stats:							return "number_of_stats";
		default: throw ns_ex("Unknown worm classifier") << (unsigned int)c;
	}
}
