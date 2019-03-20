#ifndef NS_POSTURE_ANALYSIS_MODELS
#define NS_POSTURE_ANALYSIS_MODELS
#include "ns_ex.h"
#include "ns_get_double.h"
#include <algorithm>
#include <iostream>
#include "ns_analyzed_image_time_path_element_measurements.h"

class ns_analyzed_image_time_path;
struct ns_hmm_emission {
	ns_analyzed_image_time_path_element_measurements measurement;
	unsigned long emission_time; //*NOT USED IN TRAINING...only for debugging and data visualization*/
	ns_stationary_path_id path_id;
};
struct ns_hmm_emission_normalization_stats {
	ns_analyzed_image_time_path_element_measurements path_mean, path_variance;
	ns_death_time_annotation source;
};
class ns_emission_probabiliy_model;

struct ns_emperical_posture_quantification_value_estimator{
	~ns_emperical_posture_quantification_value_estimator();
	friend class ns_time_path_movement_markov_solver;
	bool add_observation(const std::string &software_version, const ns_death_time_annotation & properties, const ns_analyzed_image_time_path * path);
	void build_estimator_from_observations(std::string & output);

	void probability_for_each_state(const ns_analyzed_image_time_path_element_measurements & e,std::vector<double> & p) const;
	void read_observation_data(std::istream & in);
	void write_observation_data(std::ostream & out,const std::string & experiment_name = "") const;
	
	void read(std::istream & i);
	void write(std::ostream & o)const;
	static ns_emperical_posture_quantification_value_estimator dummy();
	ns_emperical_posture_quantification_value_estimator();
	ns_emperical_posture_quantification_value_estimator(const ns_emperical_posture_quantification_value_estimator&); 
	ns_emperical_posture_quantification_value_estimator& operator=(const ns_emperical_posture_quantification_value_estimator&); 
	bool state_specified_by_model(const ns_hmm_movement_state s) const;
	//useful for debugging
	void provide_measurements_and_sub_probabilities(const ns_hmm_movement_state & state, const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & measurement, std::vector<double> & sub_probabilitiy ) const;
	void provide_sub_probability_names(std::vector<std::string> & names) const;
	unsigned long number_of_sub_probabilities() const;
	bool state_defined(const ns_hmm_movement_state & m) const;
private:
	void write_visualization(std::ostream & o,const std::string & experiment_name="") const;
	std::map<ns_hmm_movement_state, ns_emission_probabiliy_model *> emission_probability_models;
	std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> > observed_values;
	std::map<ns_stationary_path_id, ns_hmm_emission_normalization_stats > normalization_stats;
};

struct ns_threshold_movement_posture_analyzer_parameters{
	double stationary_cutoff,
		posture_cutoff,
		death_time_expansion_cutoff;
	unsigned long permanance_time_required_in_seconds;

	unsigned long death_time_expansion_time_kernel_in_seconds;
	bool use_v1_movement_score;
	std::string version_flag;
	static ns_threshold_movement_posture_analyzer_parameters default_parameters(const unsigned long experiment_duration_in_seconds);
	void read(std::istream & i);
	void write(std::ostream & o)const;
};

struct ns_posture_analysis_model{
	typedef enum{ns_not_specified,ns_threshold,ns_hidden_markov,ns_unknown} ns_posture_analysis_method;
	static ns_posture_analysis_method method_from_string(const std::string & s){
		if (s.size() == 0)
			return ns_not_specified;
		if (s == "thresh")
			return ns_threshold;
		if (s == "hm" || s == "hmm")
			return ns_hidden_markov;
		return ns_unknown;
	}
	ns_emperical_posture_quantification_value_estimator hmm_posture_estimator;
	
	ns_threshold_movement_posture_analyzer_parameters threshold_parameters;

	static ns_posture_analysis_model dummy();
	ns_posture_analysis_method posture_analysis_method;
	std::string name;
};
#endif
