#ifndef NS_TIME_PATH_SOLVER_PARAMETERS
#define NS_TIME_PATH_SOLVER_PARAMETERS

struct ns_time_path_solver_parameters{
	unsigned long short_capture_interval_in_seconds,number_of_consecutive_sample_captures,number_of_samples_per_device;
	double maximum_object_detection_density_in_events_per_hour() const{return 1.0/(short_capture_interval_in_seconds/60.0/60.0*(double)number_of_samples_per_device);}

	unsigned long min_stationary_object_path_fragment_duration_in_seconds;
	unsigned long stationary_object_path_fragment_window_length_in_seconds;
	unsigned long stationary_object_path_fragment_max_movement_distance;
	unsigned long maximum_time_gap_between_joined_path_fragments;
	unsigned long maximum_time_overlap_between_joined_path_fragments;
	unsigned long maximum_distance_betweeen_joined_path_fragments;
	unsigned long min_final_stationary_path_duration_in_minutes;

	double maximum_fraction_duplicated_points_between_joined_path_fragments,
		   maximum_path_fragment_displacement_per_hour,
		   max_average_final_path_average_timepoint_displacement,
		   maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment,
		   maximum_fraction_of_median_gap_allowed_in_low_density_paths;

	static ns_time_path_solver_parameters default_parameters(const unsigned long experiment_length_in_seconds,
															const unsigned long short_capture_interval_in_seconds_,
															const unsigned long number_of_consecutive_sample_captures_,
															const unsigned long number_of_samples_per_device_);

	static ns_time_path_solver_parameters default_parameters(const ns_64_bit sample_region_image_info_id, ns_sql & sql,bool create_default_parameter_file_if_needed=false,bool load_from_disk_if_possible=true);

};


struct ns_time_path_limits {
	ns_time_path_limits() {}
	ns_time_path_limits(const ns_death_time_annotation_time_interval & before, const ns_death_time_annotation_time_interval & end,
		const ns_death_time_annotation_time_interval & first_plate, const ns_death_time_annotation_time_interval & last_plate) :
		interval_before_first_observation(before),
		interval_after_last_observation(end),
		first_obsevation_of_plate(first_plate),
		last_obsevation_of_plate(last_plate) {}
	ns_death_time_annotation_time_interval interval_before_first_observation,
		interval_after_last_observation,
		first_obsevation_of_plate,
		last_obsevation_of_plate;
};

#endif