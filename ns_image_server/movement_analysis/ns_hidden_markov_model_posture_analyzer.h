#ifndef NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H
#define NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H

#include "ns_ex.h"
#include "ns_death_time_annotation.h"
#include "ns_time_path_posture_movement_solution.h"
#include "ns_posture_analysis_models.h"
#include <limits.h>
#include "ns_region_metadata.h"

struct ns_hmm_movement_analysis_optimizatiom_stats_event_annotation {
	ns_death_time_annotation_time_interval by_hand, machine;
	bool by_hand_identified, machine_identified;
};

struct ns_hmm_movement_optimization_stats_record_path_element {
	ns_hmm_movement_state state;
	double total_probability;
	std::vector<double> sub_probabilities;
	std::vector<double> sub_measurements;
};
struct ns_hmm_movement_optimization_stats_record_path {
	double log_likelihood;
	//state,log-likelihood of path up until that state
	std::vector<ns_hmm_movement_optimization_stats_record_path_element> path;
};
struct ns_hmm_movement_analysis_optimizatiom_stats_record {
	enum { number_of_states = 5 };
	static const ns_movement_event states[number_of_states];
	typedef std::map<ns_movement_event, ns_hmm_movement_analysis_optimizatiom_stats_event_annotation> ns_record_list;
	ns_record_list measurements;
	ns_stationary_path_id id;
	ns_death_time_annotation properties;
	
	ns_hmm_movement_optimization_stats_record_path by_hand_state_info, machine_state_info;
	std::vector<double> state_info_times;
	std::vector<std::string> state_info_variable_names;

};
struct ns_hmm_movement_analysis_optimizatiom_stats {
	std::vector<ns_hmm_movement_analysis_optimizatiom_stats_record> animals;

	static void write_error_header(std::ostream & o);
	void write_error_data(std::ostream & o, const std::string & cross_validation_info, const unsigned long & cross_validation_replicate_id, const std::map<ns_64_bit,  ns_region_metadata> & metadata_cache) const;

	void write_hmm_path_header(std::ostream & o) const;
	void write_hmm_path_data(std::ostream & o, const std::map<ns_64_bit, ns_region_metadata> & metadata_cache) const;

};

class ns_hmm_solver {
public:

	typedef std::pair<ns_hmm_movement_state, unsigned long> ns_hmm_state_transition_time_path_index;

	ns_time_path_posture_movement_solution movement_state_solution;

	void solve(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator,
				std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2);
	static void probability_of_path_solution(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const ns_time_path_posture_movement_solution & solution, ns_hmm_movement_optimization_stats_record_path  & state_info, bool generate_path_info);


	//run the viterbi algorithm using the specified indicies of the path
	static double run_viterbi(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const std::vector<unsigned long> path_indices,
		std::vector<ns_hmm_state_transition_time_path_index > &movement_transitions,
		std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2);

	void build_movement_state_solution_from_movement_transitions(const std::vector<unsigned long> path_indices, const std::vector<ns_hmm_state_transition_time_path_index > & movement_transitions);

private:
	//m[i][j] is the log probabilitiy that an individual in state i transitions to state j.
	static void build_state_transition_matrix(const ns_emperical_posture_quantification_value_estimator & estimator, std::vector<std::vector<double> > & m);

	static ns_hmm_movement_state most_probable_state(const std::vector<double> & d);

};

class ns_time_path_movement_markov_solver : public ns_analyzed_image_time_path_death_time_estimator{
public:
	ns_time_path_movement_markov_solver(const ns_emperical_posture_quantification_value_estimator & e):estimator(e){}
	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path * path, std::ostream * debug_output_ = 0)const {
		std::vector<double > tmp_storage_1;
		std::vector<unsigned long > tmp_storage_2;
		return estimate_posture_movement_states(2, path, tmp_storage_1, tmp_storage_2,0,debug_output_);
	}
	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2, std::ostream * debug_output_=0)const{
		return estimate_posture_movement_states(2,path,tmp_storage_1,tmp_storage_2,0,debug_output_);
	}
	ns_time_path_posture_movement_solution operator() (ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::ostream * debug_output=0)const{
		std::vector<double > tmp_storage_1;
		std::vector<unsigned long > tmp_storage_2;
		return estimate_posture_movement_states(2,path,tmp_storage_1,tmp_storage_2,path,debug_output);
	}

	ns_time_path_posture_movement_solution estimate_posture_movement_states(int software_value,const ns_analyzed_image_time_path * source_path, std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2, ns_analyzed_image_time_path * output_path = 0,std::ostream * debug_output=0) const;
	const ns_emperical_posture_quantification_value_estimator & estimator;
	std::string software_version_number() const { return "2.1"; }
	unsigned long latest_possible_death_time(const ns_analyzed_image_time_path * path,
		const unsigned long last_observation_time) const{return  last_observation_time; }
};

#endif
