#ifndef NS_POSTURE_ANALYSIS_MODELS
#define NS_POSTURE_ANALYSIS_MODELS
#include "ns_ex.h"
#include "ns_get_double.h"
#include <algorithm>
#include <iostream>
#include "ns_analyzed_image_time_path_element_measurements.h"
#include <set>

class ns_analyzed_image_time_path;
struct ns_hmm_emission {
	ns_analyzed_image_time_path_element_measurements measurement;
	unsigned long emission_time; //*NOT USED IN TRAINING...only for debugging and data visualization*/
	ns_stationary_path_id path_id;
	ns_64_bit region_id;
	unsigned long device_id;
};
struct ns_hmm_emission_normalization_stats {
	ns_analyzed_image_time_path_element_measurements path_mean, path_variance;
	ns_death_time_annotation source;
};
class ns_emission_probabiliy_model;

class ns_emperical_posture_quantification_value_estimator{
public:
	typedef enum { ns_all_states, ns_no_post_expansion_contraction, ns_no_expansion_while_alive, no_expansion_while_alive_nor_contraction,ns_no_expansion_nor_contraction,ns_number_of_state_settings} ns_states_permitted;
	static std::string state_permissions_to_string(const ns_states_permitted & s);
	static ns_states_permitted state_permissions_from_string(const std::string & s);
	~ns_emperical_posture_quantification_value_estimator();
	friend class ns_time_path_movement_markov_solver;
	bool add_observation(const std::string &software_version, const ns_death_time_annotation & properties, const ns_analyzed_image_time_path * path, const unsigned long device_id );
	void build_estimator_from_observations(std::string & output, const ns_states_permitted & states_permitted);

	void probability_for_each_state(const ns_analyzed_image_time_path_element_measurements & e,std::vector<double> & p) const;
	void read_observation_data(std::istream & in);
	void write_observation_data(std::ostream & out,const std::string & experiment_name = "") const;
	
	void read(std::istream & i);
	void write(std::ostream & o)const;
	static ns_emperical_posture_quantification_value_estimator dummy();
	ns_emperical_posture_quantification_value_estimator();
	ns_emperical_posture_quantification_value_estimator(const ns_emperical_posture_quantification_value_estimator&); 
	ns_emperical_posture_quantification_value_estimator& operator=(const ns_emperical_posture_quantification_value_estimator&); 


	void output_debug_info(const ns_analyzed_image_time_path_element_measurements & e, std::ostream & o) const;
	bool state_specified_by_model(const ns_hmm_movement_state s) const;
	//useful for debugging
	void provide_measurements_and_sub_probabilities(const ns_hmm_movement_state & state, const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & measurement, std::vector<double> & sub_probabilitiy ) const;
	void provide_sub_probability_names(std::vector<std::string> & names) const;
	unsigned long number_of_sub_probabilities() const;
	bool state_defined(const ns_hmm_movement_state & m) const;
	typedef std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> > ns_hmm_observed_values_list;
	ns_hmm_observed_values_list observed_values;
	std::map<ns_stationary_path_id, ns_hmm_emission_normalization_stats > normalization_stats;
	const ns_states_permitted & states_permitted() const { return states_permitted_int; }
	void defined_states(std::set<ns_hmm_movement_state> & s) const{ 
		for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); p++)
			s.emplace(p->first);
	}
private:
	void write_visualization(std::ostream & o,const std::string & experiment_name="") const;
	std::map<ns_hmm_movement_state, ns_emission_probabiliy_model *> emission_probability_models;
	ns_states_permitted states_permitted_int;
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
