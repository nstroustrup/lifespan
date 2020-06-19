#pragma once
#include "ns_posture_analysis_models.h"
#include "ns_image_server_results_storage.h"
#include "ns_machine_analysis_data_loader.h"
typedef enum { ns_lifespan, ns_thermotolerance, ns_quiecent, ns_v2 } ns_parameter_set_range;


struct ns_cross_replicate_specification{
	ns_cross_replicate_specification() :observations(0){}
	ns_hmm_observation_set* observations;

	std::string genotype, subject;

	typedef enum { ns_standard, ns_strict_ordering, ns_simultaneous_movement_cessation_and_expansion } ns_cross_replicate_estimator_type;
	ns_cross_replicate_estimator_type cross_replicate_estimator_type;


	typedef enum { ns_all_data, ns_genotype_specific, ns_experiment_specific, ns_genotype_experiment_specific } ns_cross_replicate_type;
	ns_cross_replicate_type cross_replicate_type;
};
void ns_run_hmm_cross_validation(std::string& results_text, ns_image_server_results_subject sub, ns_machine_analysis_data_loader& movement_results, const std::map < std::string, ns_cross_replicate_specification>& models_to_fit, ns_sql& sql);

void ns_identify_best_threshold_parameteters(std::string& results_text, const ns_parameter_set_range& range,  ns_image_server_results_subject sub, ns_sql& sql);