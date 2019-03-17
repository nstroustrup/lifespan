#ifndef NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H
#define NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H

#include "ns_ex.h"
#include "ns_death_time_annotation.h"
#include "ns_time_path_posture_movement_solution.h"
#include "ns_posture_analysis_models.h"
#include <limits.h>



class ns_hmm_solver {
public:

	typedef std::pair<ns_hmm_movement_state, unsigned long> ns_hmm_state_transition_time_path_index;

	ns_time_path_posture_movement_solution movement_state_solution;

	void solve(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator);
	static double probability_of_path_solution(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const ns_time_path_posture_movement_solution & solution, std::vector<double> & log_probabilities);


	//run the viterbi algorithm using the specified indicies of the path
	static double run_viterbi(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const std::vector<unsigned long> path_indices,
		std::vector<ns_hmm_state_transition_time_path_index > &movement_transitions);

	void build_movement_state_solution_from_movement_transitions(const std::vector<unsigned long> path_indices, const std::vector<ns_hmm_state_transition_time_path_index > & movement_transitions);

private:
	//m[i][j] is the log probabilitiy that an individual in state i transitions to state j.
	static void build_state_transition_matrix(std::vector<std::vector<double> > & m);

	static ns_hmm_movement_state most_probable_state(const std::vector<double> & d);

};

class ns_time_path_movement_markov_solver : public ns_analyzed_image_time_path_death_time_estimator{
public:
	ns_time_path_movement_markov_solver(const ns_emperical_posture_quantification_value_estimator & e):estimator(e){}
	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path * path, std::ostream * debug_output_=0)const{return estimate_posture_movement_states(2,path,0,debug_output_);}
	ns_time_path_posture_movement_solution operator() (ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::ostream * debug_output=0)const{return estimate_posture_movement_states(2,path,path,debug_output);}

	ns_time_path_posture_movement_solution estimate_posture_movement_states(int software_value,const ns_analyzed_image_time_path * source_path, ns_analyzed_image_time_path * output_path = 0,std::ostream * debug_output=0) const;
	const ns_emperical_posture_quantification_value_estimator & estimator;
	std::string software_version_number() const { return "2.1"; }
	unsigned long latest_possible_death_time(const ns_analyzed_image_time_path * path,
		const unsigned long last_observation_time) const{return  last_observation_time; }
};

#endif
