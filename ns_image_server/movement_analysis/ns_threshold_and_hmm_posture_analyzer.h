#pragma once

#include "ns_threshold_movement_posture_analyzer.h"
#include "ns_hidden_markov_model_posture_analyzer.h"


class ns_threshold_and_hmm_posture_analyzer : public ns_analyzed_image_time_path_death_time_estimator{
public:
	ns_threshold_and_hmm_posture_analyzer(const ns_posture_analysis_model& p) : model(&p) {}

	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path* path, std::ostream* debug_output_ = 0)const {
		ns_analyzed_image_time_path_death_time_estimator_reusable_memory mem;
		return estimate_posture_movement_states(2, path, mem, 0, debug_output_);
	}
	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path* path, const bool fill_in_loglikelihood_timeseries, ns_analyzed_image_time_path_death_time_estimator_reusable_memory & mem, std::ostream* debug_output_ = 0)const {
		return estimate_posture_movement_states(2, path, mem, 0, debug_output_);
	}
	ns_time_path_posture_movement_solution operator() (ns_analyzed_image_time_path* path, const bool fill_in_loglikelihood_timeseries, std::ostream* debug_output = 0)const {
		ns_analyzed_image_time_path_death_time_estimator_reusable_memory mem;
		return estimate_posture_movement_states(2, path, mem, path, debug_output);
	}

	ns_time_path_posture_movement_solution estimate_posture_movement_states(int software_value, const ns_analyzed_image_time_path* source_path, ns_analyzed_image_time_path_death_time_estimator_reusable_memory& mem, ns_analyzed_image_time_path* output_path = 0, std::ostream * debug_output = 0) const;

	unsigned long latest_possible_death_time(const ns_analyzed_image_time_path* path, const unsigned long last_observation_time) const;
	std::string model_software_version_number() const { return model->threshold_parameters.version_flag; }
	std::string current_software_version_number() const { return "2.2"; }
	const std::string & model_description() const { return model->threshold_parameters.model_description_text; }
private:
	ns_time_path_posture_movement_solution run(const ns_analyzed_image_time_path* path, std::ostream* debug_output = 0) const;
	const ns_posture_analysis_model * model;
};