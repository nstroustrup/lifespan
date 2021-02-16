#pragma once
#include "ns_ex.h"
#include <iostream>
#include "ns_vector.h"
#include <ostream>
struct ns_analyzed_image_time_path_element_measurements {
	static std::string measurement_format_version() { return "2.3"; }
	//This is the quantification used to identify death times in old versions of the lifespan machine
	inline const double & death_time_posture_analysis_measure_v1() const { return denoised_movement_score; }
	inline double & death_time_posture_analysis_measure_v1() { return denoised_movement_score; }

	//This is the quantification used to identify death times in the current version
	inline const double & death_time_posture_analysis_measure_v2_cropped() const { return spatial_averaged_movement_sum_cropped[1]; }
	inline double & death_time_posture_analysis_measure_v2_cropped() { return spatial_averaged_movement_sum_cropped[1]; }

	inline const double & death_time_posture_analysis_measure_v2_uncropped() const { return spatial_averaged_movement_sum_uncropped; }
	inline double & death_time_posture_analysis_measure_v2_uncropped() { return spatial_averaged_movement_sum_uncropped; }

	inline void rescale_baseline_intensities(double scale_factor);

	ns_64_bit movement_sum,
		total_foreground_area,
		total_intensity_within_foreground,
		total_stabilized_area,
		total_region_area,
		total_intensity_within_region,
		total_intensity_within_stabilized_denoised,
		total_intensity_within_stabilized,
		total_alternate_worm_area,
		total_intensity_within_alternate_worm;

	ns_s64_bit change_in_total_foreground_intensity,
		change_in_total_region_intensity,
		change_in_total_stabilized_intensity_1x,
		change_in_total_stabilized_intensity_2x,
		change_in_total_stabilized_intensity_4x;

	ns_s64_bit 	change_in_total_outside_stabilized_intensity_1x,
		change_in_total_outside_stabilized_intensity_2x,
		change_in_total_outside_stabilized_intensity_4x;

	double
		movement_score,
		denoised_movement_score,
		spatial_averaged_movement_sum_uncropped,
		spatial_averaged_movement_sum_cropped[3],
		spatial_averaged_movement_sum_cropped_4x,
		spatial_averaged_scaled_movement_sum_cropped,
		spatial_averaged_scaled_movement_sum_cropped_4x,
		intensity_normalization_scale_factor;

	//double mean_intensity_within_foreground() const { return total_intensity_within_foreground / (double)total_foreground_area; }
	double mean_intensity_within_region() const { return total_intensity_within_region / (double)total_region_area; }
	double mean_intensity_within_stabilized() const { return total_intensity_within_stabilized / (double)total_stabilized_area; }
	double mean_intensity_within_alternate_worm() const { return total_intensity_within_alternate_worm / (double)total_alternate_worm_area; }

	mutable std::vector<double> posture_quantification_extra_debug_fields;

	void zero();

	void square();
	void square_root();

	ns_analyzed_image_time_path_element_measurements() { zero(); }
	static void write_header(std::ostream & out);
	void write(std::ostream & out, const ns_vector_2d & registration_offset, const ns_vector_2d & intensity_center_of_mass,const bool & saturated_offset) const;
	void read(std::istream & in, ns_vector_2d & registration_offset, ns_vector_2d& intensity_center_of_mass, bool & saturated_offset);
};

ns_analyzed_image_time_path_element_measurements operator+(const ns_analyzed_image_time_path_element_measurements & a, const ns_analyzed_image_time_path_element_measurements & b);
ns_analyzed_image_time_path_element_measurements operator-(const ns_analyzed_image_time_path_element_measurements & a, const ns_analyzed_image_time_path_element_measurements & b);
ns_analyzed_image_time_path_element_measurements operator/(const ns_analyzed_image_time_path_element_measurements & a, const int & d);
