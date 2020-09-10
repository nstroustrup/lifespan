#ifndef NS_THRESHOLD_MOVEMENT_POSTURE_ANALYZER
#define NS_THRESHOLD_MOVEMENT_POSTURE_ANALYZER
#include "ns_time_path_posture_movement_solution.h"

class ns_threshold_movement_posture_analyzer : public ns_analyzed_image_time_path_death_time_estimator{
public:
	ns_threshold_movement_posture_analyzer(const ns_threshold_movement_posture_analyzer_parameters & p):parameters(p){}
	
	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path * path, std::ostream * debug_output=0) const;
	ns_time_path_posture_movement_solution operator() (ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::ostream * debug_output=0)const;
	ns_time_path_posture_movement_solution operator() (const ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries, ns_analyzed_image_time_path_death_time_estimator_reusable_memory & mem, std::ostream * debug_output = 0)const {
		return (*this)(path, debug_output);
	}
	unsigned long latest_possible_death_time(const ns_analyzed_image_time_path * path,const unsigned long last_observation_time) const;

	static std::string current_software_version() { return "2.2"; }
	std::string current_software_version_number() const { return current_software_version(); }
	std::string model_software_version_number() const { return parameters.version_flag; }
	const std::string& model_description() const { return parameters.model_description_text; }
private:
	ns_time_path_posture_movement_solution run(const ns_analyzed_image_time_path * path, std::ostream * debug_output=0) const;
	const ns_threshold_movement_posture_analyzer_parameters parameters;
};
#endif
