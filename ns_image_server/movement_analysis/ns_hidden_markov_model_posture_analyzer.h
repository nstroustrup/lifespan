#ifndef NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H
#define NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H

#include "ns_ex.h"
#include "ns_hidden_markov_model.h"
#include "ns_death_time_annotation.h"
#include "ns_time_path_posture_movement_solution.h"
#include "ns_posture_analysis_models.h"
#include <limits.h>



class ns_time_path_movement_markov_solver : public ns_analyzed_image_time_path_death_time_estimator{
public:
	ns_time_path_movement_markov_solver(const ns_emperical_posture_quantification_value_estimator & e):estimator(e){}
	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path * path, std::ostream * debug_output_=0)const{return estimate_posture_movement_states(2,path,0,debug_output_);}
	ns_time_path_posture_movement_solution operator() (ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::ostream * debug_output=0)const{return estimate_posture_movement_states(2,path,path,debug_output);}

	ns_time_path_posture_movement_solution estimate_posture_movement_states(int software_value,const ns_analyzed_image_time_path * source_path, ns_analyzed_image_time_path * output_path = 0,std::ostream * debug_output=0) const;
	ns_time_path_posture_movement_solution estimate_posture_movement_states(const std::vector<double> & movement_ratio, const std::vector<double> & tm, bool output_loglikelihood_series, ns_sequential_hidden_markov_solution & solution,std::ostream * debug_output=0) const;
	const ns_emperical_posture_quantification_value_estimator & estimator;
	std::string software_version_number() const { return "2"; }
	unsigned long latest_possible_death_time(const ns_analyzed_image_time_path * path,
		const unsigned long last_observation_time) const{return  last_observation_time; }
};

#endif
