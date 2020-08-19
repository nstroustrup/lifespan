#pragma once
#include "ns_posture_analysis_models.h"
#include "ns_image_server_results_storage.h"
#include "ns_machine_analysis_data_loader.h"
typedef enum { ns_lifespan, ns_thermotolerance, ns_quiecent, ns_v2 } ns_parameter_set_range;


struct ns_model_building_specification{

	typedef enum { ns_standard, ns_strict_ordering, ns_simultaneous_movement_cessation_and_expansion } ns_cross_replicate_estimator_type;
	typedef enum { ns_static, ns_empirical,ns_empirical_without_weights} ns_state_transition_type;
	static std::string state_transition_type_to_string(const ns_state_transition_type& e) {
		switch (e) {
		case ns_static:
			return "static";
		case ns_empirical:
			return "empirical";
		case ns_empirical_without_weights:
			return "empirical_no_weights";
		}
		throw ns_ex("Unknown estimator type!");

	}
	static std::string estimator_type_to_short_string(const ns_cross_replicate_estimator_type & e) {
		switch (e) {
		case ns_standard:
			return "flexible";
		case ns_strict_ordering:
			return "strict";
		case ns_simultaneous_movement_cessation_and_expansion:
			return "simultaneous";
		}
		throw ns_ex("Unknown estimator type!");

	}
	static std::string estimator_type_to_long_string(const ns_cross_replicate_estimator_type & e) {
		switch (e) {
		case ns_standard:
			return " using a flexible state ordering scheme"; 
		case ns_strict_ordering:
			return " using a restricted state ordering scheme"; 
		case ns_simultaneous_movement_cessation_and_expansion:
			return " requiring synchronous movement cessation and expansion "; 
		}
		throw ns_ex("Unknown estimator type!");
	}
	ns_cross_replicate_estimator_type cross_replicate_estimator_type;
	ns_state_transition_type state_transition_type;
	std::string name;
	std::vector<std::string> model_features_to_use;
};

struct ns_cross_validation_subject {
	ns_cross_validation_subject() :observations(0) {}
	ns_hmm_observation_set* observations;

	std::vector< ns_model_building_specification > specification;
	std::string genotype, subject;
	typedef enum { ns_all_data, ns_genotype_specific, ns_experiment_specific, ns_hmm_user_specified_specific, ns_hmm_user_specified_experiment_specific } ns_cross_replicate_type;
	ns_cross_replicate_type cross_replicate_type;
};
void ns_run_hmm_cross_validation(std::string& results_text, ns_image_server_results_subject sub, ns_machine_analysis_data_loader& movement_results, const std::map < std::string, ns_cross_validation_subject>& models_to_fit, ns_sql& sql);

void ns_identify_best_threshold_parameteters(std::string& results_text, const ns_parameter_set_range& range,  ns_image_server_results_subject sub, ns_sql& sql);