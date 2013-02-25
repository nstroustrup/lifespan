#ifndef NS_WORM_DETECTION_CONSTANTS_H
#define NS_WORM_DETECTION_CONSTANTS_H

#undef ALLOW_ALL_SPINE_PERMUTATIONS

#include "ns_ex.h"
#include <string>

///Various statistics can be calculated on detected objects.  These are used
///as features used by classifiers to identify nematodes
typedef enum{	  ns_stat_pixel_area,								//0
				  ns_stat_bitmap_width,								//1
				  ns_stat_bitmap_height,							//2
				  ns_stat_bitmap_diagonal,							//3
				  ns_stat_spine_length,								//4
				  ns_stat_distance_between_ends,					//5
				  ns_stat_average_width,							//6
				  ns_stat_max_width,								//7
				  ns_stat_min_width,								//8
				  ns_stat_width_variance,							//9
				  ns_stat_width_at_center,							//10
				  ns_stat_width_at_end_0,							//11
				  ns_stat_width_at_end_1,							//12
				  ns_stat_spine_length_to_area,						//13
				  ns_stat_spine_length_to_bitmap_width_ratio,		//14
				  ns_stat_spine_length_to_bitmap_height_ratio,		//15
				  ns_stat_spine_length_to_bitmap_diagonal_ratio,	//16
				  ns_stat_spine_length_to_max_width_ratio,			//17
				  ns_stat_spine_length_to_average_width,			//18
				  ns_stat_end_width_to_middle_width_ratio_0,		//19
				  ns_stat_end_width_to_middle_width_ratio_1,		//20
				  ns_stat_end_width_ratio,							//21
				  ns_stat_average_curvature,						//22
				  ns_stat_max_curvature,							//23
				  ns_stat_total_curvature,							//24
				  ns_stat_curvature_variance,						//25
				  ns_stat_curvature_x_intercept,					//26
				
				  ns_stat_relative_intensity_average,				//27
				  ns_stat_relative_intensity_variance,						//28
				  ns_stat_relative_intensity_skew,							//29
				  ns_stat_relative_intensity_max,							//30
				  ns_stat_relative_intensity_spine_average,					//31
				  ns_stat_relative_intensity_spine_variance,				//32
				  ns_stat_relative_intensity_roughness_1,					//33
				  ns_stat_relative_intensity_roughness_2,					//34
				  ns_stat_relative_intensity_dark_pixel_average,			//35
				  ns_stat_relative_intensity_dark_pixel_area,				//36
				  ns_stat_relative_intensity_of_neighborhood,				//37
				  ns_stat_relative_intensity_distance_from_neighborhood,	//38
				  ns_stat_relative_intensity_containing_image_region_average,//39

				  ns_stat_relative_intensity_normalized_average,				//40				
				  ns_stat_relative_intensity_normalized_max,					//41
				  ns_stat_relative_intensity_normalized_spine_average,		//42

				  ns_stat_absolute_intensity_average,				//43
				  ns_stat_absolute_intensity_variance,						//44
				  ns_stat_absolute_intensity_skew,							//45
				  ns_stat_absolute_intensity_max,							//46
				  ns_stat_absolute_intensity_spine_average,					//47
				  ns_stat_absolute_intensity_spine_variance,				//48
				  ns_stat_absolute_intensity_roughness_1,					//49
				  ns_stat_absolute_intensity_roughness_2,					//50
				  ns_stat_absolute_intensity_dark_pixel_average,			//51
				  ns_stat_absolute_intensity_dark_pixel_area,				//52
				  ns_stat_absolute_intensity_of_neighborhood,				//53
				  ns_stat_absolute_intensity_distance_from_neighborhood,	//54
				  ns_stat_absolute_intensity_containing_image_region_average,//55

				  ns_stat_absolute_intensity_normalized_average,				//56				
				  ns_stat_absolute_intensity_normalized_max,					//57
				  ns_stat_absolute_intensity_normalized_spine_average,		//58
				  ns_stat_edge_length,										//59
				  ns_stat_edge_to_area_ratio,								//60
				  
				  ns_stat_intensity_profile_edge,						//61
				  ns_stat_intensity_profile_center,						//62
				  ns_stat_intensity_profile_max,						//63
				  ns_stat_intensity_profile_variance,					//64
				  ns_stat_number_of_stats							//
	} 
	ns_detected_worm_classifier;

///Various important constants used during worm detection.
///Several specified values are only used if SVM machine learning
///is disabled.

struct ns_worm_detection_constant{
	typedef enum{
		connected_object_area_cutoff, 
		maximum_worm_region_area, 
		minimum_worm_region_area,
		maximum_region_diagonal,
		worm_end_node_margin,
		spine_visualization_resize_factor,
		spine_smoothing_radius,
		spine_visualization_output_resolution,
		tiff_compression_intensity_crop_value,
		maximum_dimension_size_for_small_images,
		minimum_worm_hole_size,
		nearest_neighbor_maximum_distance,
		fast_movement_lower_distance_threshold,
		number_of_constants} constant_type;

	typedef enum{
		overlap_required_to_interpolate_threshold_region_as_worm,  //percentage overlap, x out of 10
		allowed_drift_distance_for_objects_during_interpolation,  //in pixels
		number_of_time_points_to_use_for_interpolation,
		number_of_time_points_required_to_infer_an_object_during_interpolation,
		overlap_required_to_infer_an_object_during_interpolation, //percentage overlap, x out of 10
		allowed_drift_distance_for_objects_during_static_mask_creation,
		proportion_of_early_time_points_present_required_during_static_mask_creation,//percentage overlap, x out of 10
		proportion_of_middle_time_points_present_required_during_static_mask_creation,//percentage overlap, x out of 10
		proportion_of_late_time_points_present_required_during_static_mask_creation, //percentage overlap, x out of 10
		maximum_number_of_objects_per_image,
		maximum_number_of_actual_worms_per_image,
		maximum_number_of_putative_worms_per_image,
		maximum_number_of_worms_per_multiple_worm_cluster
				} resolution_independant_constant_type;
};

template<class T>
struct ns_worm_detection_constant_set{
	inline const unsigned int & operator[](const T & d) const{return constants[(unsigned int)d];}
	inline unsigned int & operator[](const T & d) {return constants[(unsigned int)d];}

	private:
	unsigned int constants[ns_worm_detection_constant::number_of_constants];
};

class ns_worm_detection_constants{

public:

	///loads all constants into memory.  Should be called once per excecutable (though multiple calls cause no harm)
	static void init();

	//the largest length ratio between two worms in a cluster (ie one worm can't be more than 1.2 times longer
	//than another
	inline static float maximum_worm_cluster_length_ratio(){return (float)1.3;}

	inline static unsigned char max_number_of_segments_to_disambiguate(){return 10;}

	static inline const unsigned long get(const ns_worm_detection_constant::constant_type & d,const float & resolution){
		if (resolution <= 1201)
			return ns_worm_detection_constants::const_1200[d];
		else return ns_worm_detection_constants::const_3200[d];
	}

	static inline const unsigned long get(const ns_worm_detection_constant::resolution_independant_constant_type & d){
		return ns_worm_detection_constants::const_resolution_independant[d];
	}
	enum{ spine_smoothing_radius_1200 = 16,
		  spine_smoothing_radius_3200 = 1,
		  percent_distance_to_retract_spine_from_ends = 8};
	private:
	static ns_worm_detection_constant_set<ns_worm_detection_constant::constant_type> const_1200,const_3200;
	static ns_worm_detection_constant_set<ns_worm_detection_constant::resolution_independant_constant_type> const_resolution_independant;
};

///Given a feature id, returns the human-readable name of the feature
std::string ns_classifier_label(const ns_detected_worm_classifier & c);
///Given a featre enumeration, returns a one-word abbreviation of the feature, suitable for matlab variable names, etc.
std::string ns_classifier_abbreviation(const ns_detected_worm_classifier & c);


#endif
