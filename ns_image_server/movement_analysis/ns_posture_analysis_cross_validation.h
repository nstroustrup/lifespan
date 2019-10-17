#pragma once
#include "ns_posture_analysis_models.h"
#include "ns_image_server_results_storage.h"
#include "ns_machine_analysis_data_loader.h"

typedef enum { ns_lifespan, ns_thermotolerance, ns_quiecent, ns_v2 } ns_parameter_set_range;
void ns_run_hmm_cross_validation(std::string& results_text, ns_image_server_results_subject sub, ns_machine_analysis_data_loader& results,const std::map < std::string, ns_emperical_posture_quantification_value_estimator>& observations_sorted_by_genotype, ns_sql& sql);

void ns_identify_best_threshold_parameteters(std::string& results_text, const ns_parameter_set_range& range,  ns_image_server_results_subject sub, ns_sql& sql);