#ifndef NS_TIME_PATH_POSTURE_MOVEMENT_SOLUTION
#define NS_TIME_PATH_POSTURE_MOVEMENT_SOLUTION
#include "ns_death_time_annotation.h"

struct ns_movement_state_observation_boundaries{
	long start_index,
				  end_index; //one past the last valid point in the interval
	bool skipped;
	unsigned long longest_observation_gap_within_interval;
};
struct ns_time_path_posture_movement_solution{
	ns_movement_state_observation_boundaries moving,
		slowing,
		dead,
		expanding;
	std::string reason_for_animal_to_be_censored;
	double loglikelihood_of_solution;
};
struct ns_movement_state_time_interval_indicies{
	ns_movement_state_time_interval_indicies():period_start_index(0),period_end_index(0),
		 interval_occurs_after_observation_interval(false),
		 interval_occurs_before_observation_interval(false){}

	ns_movement_state_time_interval_indicies(long start,long end):period_start_index(start),period_end_index(end),
		 interval_occurs_after_observation_interval(false),
		 interval_occurs_before_observation_interval(false){}
	long period_start_index,
		 period_end_index;
	bool interval_occurs_after_observation_interval,
		 interval_occurs_before_observation_interval;
};
struct ns_movement_state_observation_boundary_interval{
	ns_movement_state_observation_boundary_interval():skipped(false){}
	
	ns_movement_state_time_interval_indicies entrance_interval,
											exit_interval;
	unsigned long longest_observation_gap_within_interval;
	bool skipped;
};

class ns_analyzed_image_time_path;

class ns_analyzed_image_time_path_death_time_estimator{
public:
	virtual ~ns_analyzed_image_time_path_death_time_estimator() {}; // has derived classes that are passed around cast as the base class: should have virtual destructor.
	virtual ns_time_path_posture_movement_solution operator() (const ns_analyzed_image_time_path * path, std::ostream * debug_output=0)const=0;
	virtual ns_time_path_posture_movement_solution operator() (ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::ostream * debug_output=0)const=0;
	virtual ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries, std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2, std::ostream * debug_output_ = 0)const = 0;
	virtual unsigned long latest_possible_death_time(const ns_analyzed_image_time_path * path,const unsigned long last_observation_time) const = 0;
	virtual std::string software_version_number() const =0 ;
	std::string name;
};

#endif
