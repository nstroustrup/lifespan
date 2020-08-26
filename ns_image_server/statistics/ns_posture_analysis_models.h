#ifndef NS_POSTURE_ANALYSIS_MODELS
#define NS_POSTURE_ANALYSIS_MODELS
#include "ns_ex.h"
#include "ns_get_double.h"
#include <algorithm>
#include <iostream>
#include "ns_analyzed_image_time_path_element_measurements.h"
#include "ns_death_time_annotation.h"
#include <set>

class ns_analyzed_image_time_path;
template<class data_t>
struct ns_hmm_labeled_data {
	ns_hmm_labeled_data() :genotype(0),region_name(0),device_name(0),experiment_id(0) {}
	data_t measurement;
	ns_stationary_path_id path_id;
	ns_64_bit region_info_id;
	ns_64_bit experiment_id;
	const std::string* database_name;
	const std::string* region_name;
	const std::string * device_name;

	/*Used to stratify data and generate genotype-specific hmm models*/
	const std::string* genotype;

	/*NOT USED IN TRAINING...only for debugging and data visualization*/
	unsigned long emission_time; 
};
typedef ns_hmm_labeled_data< ns_analyzed_image_time_path_element_measurements> ns_hmm_emission;
typedef ns_hmm_labeled_data< double > ns_hmm_duration;

struct ns_hmm_emission_normalization_stats {
	ns_analyzed_image_time_path_element_measurements path_mean, path_variance;
	ns_death_time_annotation source;
	const std::string* region_name, * device_name;
};



typedef enum { ns_all_states_permitted, ns_no_post_expansion_contraction, ns_no_expansion_while_alive, no_expansion_while_alive_nor_contraction, ns_no_expansion_nor_contraction, ns_require_movement_expansion_synchronicity, ns_number_of_state_settings } ns_hmm_states_permitted;
class ns_hmm_observation_set {
public:
	ns_hmm_observation_set() :volatile_number_of_individuals_fully_annotated(0), volatile_number_of_individuals_observed(0) {}
	typedef std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> > ns_hmm_observed_values_list;
	typedef std::map<ns_hmm_state_transition, std::vector<ns_hmm_duration> > ns_hmm_state_duration_list;
	ns_hmm_observed_values_list obs;
	ns_hmm_state_duration_list state_durations;
	void read_emissions(std::istream& in);
	void write_emissions(std::ostream& out, const std::string& experiment_name = "") const;
	void write_durations(std::ostream& out, const std::string& experiment_name = "") const;
	bool add_observation(const std::string& software_version, const ns_death_time_annotation& properties, const ns_analyzed_image_time_path* path, const std::string* database_name, const ns_64_bit& experiment_id, const std::string* plate_name, const std::string* device_name, const std::string * genotype);
	void clean_up_data_prior_to_model_fitting();
	std::map<ns_stationary_path_id, ns_hmm_emission_normalization_stats > normalization_stats;
	std::set<std::string> volatile_string_storage;

	unsigned long volatile_number_of_individuals_fully_annotated;
	unsigned long volatile_number_of_individuals_observed;
};

class ns_probability_model_holder;
class ns_probability_model_generator;
class ns_emperical_posture_quantification_value_estimator{
public:

	typedef enum { ns_static, ns_static_mod,ns_empirical,ns_empirical_without_weights } ns_hmm_states_transition_types;
	static std::string state_permissions_to_string(const ns_hmm_states_permitted& s);
	static ns_hmm_states_permitted state_permissions_from_string(const std::string & s);
	~ns_emperical_posture_quantification_value_estimator();
	friend class ns_time_path_movement_markov_solver;
	bool build_estimator_from_observations(const ns_hmm_observation_set & observation_set, const ns_probability_model_generator* generator,const ns_hmm_states_permitted& states_permitted_, const ns_hmm_states_transition_types & transition_type_, std::string & output);

	void log_probability_for_each_state(const ns_analyzed_image_time_path_element_measurements & e,std::vector<double> & p) const;

	//log probability of an animal moving between states after duration_in_seconds seconds.
	//logprob[i][j] is the probability of moving from state i to state j.
	//weight matrix is used to modify log_probabilities. The value should be the log of a weight between 0 and 1 .
	void state_transition_log_probabilities(const double& duration_in_seconds, const std::vector < std::vector<double> >& weight_matrix, std::vector < std::vector<double> >& log_prob) const;

	void read(std::istream & i);
	void write(std::ostream & o)const;
	static ns_emperical_posture_quantification_value_estimator dummy();
	ns_emperical_posture_quantification_value_estimator();
	ns_emperical_posture_quantification_value_estimator(const ns_emperical_posture_quantification_value_estimator&); 
	ns_emperical_posture_quantification_value_estimator& operator=(const ns_emperical_posture_quantification_value_estimator&); 

	void validate_model_settings(ns_sql& sql) const;

	void output_debug_info(const ns_analyzed_image_time_path_element_measurements & e, std::ostream & o) const;
	bool state_specified_by_model(const ns_hmm_movement_state s) const;
	//useful for debugging
	void provide_measurements_and_log_sub_probabilities(const ns_hmm_movement_state & state, const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & measurement, std::vector<double> & sub_probabilitiy ) const;
	void provide_sub_probability_names(std::vector<std::string> & names) const;
	unsigned long number_of_sub_probabilities() const;
	bool state_defined(const ns_hmm_movement_state & m) const;
	const ns_hmm_states_permitted & states_permitted() const { return states_permitted_int; }
	const ns_hmm_states_transition_types & states_transitions() const { return state_transition_type; }
	void defined_states(std::set<ns_hmm_movement_state>& s) const;
	std::string software_version_when_built;
	friend bool operator==(const ns_emperical_posture_quantification_value_estimator & a, const ns_emperical_posture_quantification_value_estimator & b);
private:
	void write_visualization(std::ostream & o,const std::string & experiment_name="") const;
	ns_probability_model_holder * emission_probability_models;
	ns_hmm_states_permitted states_permitted_int;
	ns_hmm_states_transition_types state_transition_type;
	int flags_to_int() const;
	void update_flags_from_int(int flag_int);
};
bool operator==(const ns_emperical_posture_quantification_value_estimator & a, const ns_emperical_posture_quantification_value_estimator & b);

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
	typedef enum{ns_not_specified,ns_threshold,ns_hidden_markov,ns_threshold_and_hmm,ns_unknown} ns_posture_analysis_method;
	static ns_posture_analysis_method method_from_string(const std::string & s){
		if (s.empty())
			return ns_not_specified;
		if (s == "thresh")
			return ns_threshold;
		if (s == "hm" || s == "hmm")
			return ns_hidden_markov;
		if (s == "thr_hm")
			return ns_threshold_and_hmm;
		return ns_unknown;
	}
	static std::string method_to_string(const ns_posture_analysis_method & m) {
		switch (m) {
		case ns_not_specified: return "";
		case ns_threshold: return "thresh";
		case ns_hidden_markov: return "hmm";
		case ns_threshold_and_hmm: return "thr_hm";
		case ns_unknown: return "?";
		default:throw ns_ex("ns_posture_analysis_model::method_to_string()::Unknown method.");
		}
	}
	std::string software_version_when_built() const{
		switch (posture_analysis_method) {
		case ns_not_specified: return "N/A";
		case ns_threshold: return threshold_parameters.version_flag;
		case ns_hidden_markov:
		case ns_threshold_and_hmm:  return hmm_posture_estimator.software_version_when_built;
		case ns_unknown: return "?";
		default:throw ns_ex("ns_posture_analysis_model::method_to_string()::Unknown method.");
		}
	}
	ns_emperical_posture_quantification_value_estimator hmm_posture_estimator;
	
	ns_threshold_movement_posture_analyzer_parameters threshold_parameters;

	static ns_posture_analysis_model dummy();
	ns_posture_analysis_method posture_analysis_method;
	std::string name;
};
#endif
