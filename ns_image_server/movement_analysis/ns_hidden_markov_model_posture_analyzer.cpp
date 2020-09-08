#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_normal_distribution.h"
#include "ns_posture_analysis_models.h"
#include "ns_threshold_and_hmm_posture_analyzer.h"
#include "ns_gmm.h"
#include "ns_probability_model_measurement_accessor.h"
#include <iomanip>



#define number_of_gmm_dimensions  2

double inline ns_catch_infinity(const double & d){
	return (d==-std::numeric_limits<double>::infinity())?-20:d;
}


double inline ns_truncate_positive(const double & d){
	return (d<=0)?(std::numeric_limits<double>::epsilon()):(d);
};

double inline ns_truncate_negatives(const double & d){
	return (d>=0?d:0);
}


void ns_hmm_solver::solve(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator,
	std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2) {
	bool found_start_time(false);
	unsigned long start_time_i(0);
	const unsigned long first_stationary_timepoint = path.first_stationary_timepoint();
	long first_stationary_timepoint_index = -1;
	std::vector<unsigned long> path_indices;
	for (unsigned int i = start_time_i; i < path.element_count(); i++) {
		if (!path.element(i).excluded && !path.element(i).censored) {
			if (!found_start_time) {
				start_time_i = i;
				found_start_time = true;
			}
			path_indices.push_back(i);
			if (i >= first_stationary_timepoint && first_stationary_timepoint_index == -1)
				first_stationary_timepoint_index = path_indices.size()-1;

		}
	}
	if (first_stationary_timepoint_index == -1)
		first_stationary_timepoint_index = 0;
	std::vector<ns_hmm_state_transition_time_path_index > movement_transitions;
	movement_state_solution = ns_time_path_posture_movement_solution();
	movement_state_solution.loglikelihood_of_solution = run_viterbi(path, estimator, path_indices, movement_transitions, tmp_storage_1, tmp_storage_2);
	build_movement_state_solution_from_movement_transitions(first_stationary_timepoint_index,path_indices, movement_transitions);
}

 void ns_hmm_solver::probability_of_path_solution(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const ns_time_path_posture_movement_solution & solution, ns_hmm_movement_optimization_stats_record_path & state_info, bool generate_path_info) {
	std::vector < ns_hmm_movement_state > movement_states(path.element_count(), ns_hmm_unknown_state);
	if (solution.moving.skipped && solution.slowing.skipped && solution.dead.skipped)
		throw ns_ex("ns_hmm_solver::probability_of_path_solution()::Encountered an empty solution");
	if (!solution.moving.skipped) {
		for (unsigned int i = 0; i <= solution.moving.start_index; i++)
			movement_states[i] = ns_hmm_missing;
		if (estimator.state_defined(ns_hmm_moving_vigorously)) {
			for (unsigned int i = solution.moving.start_index; i <= solution.moving.end_index; i++)
				movement_states[i] = ns_hmm_moving_vigorously;
		}
		else {
			for (unsigned int i = solution.moving.start_index; i <= solution.moving.end_index; i++) {
				//std::cout << "weakly from " << solution.moving.start_index << " to " << solution.moving.end_index << "\n";;
				movement_states[i] = ns_hmm_moving_weakly;
			}
		}
	}
	if (!solution.slowing.skipped) {
		if (solution.moving.skipped) {
			for (unsigned int i = 0; i <= solution.slowing.start_index; i++)
				movement_states[i] = ns_hmm_missing;
		}
		for (unsigned int i = solution.slowing.start_index; i <= solution.slowing.end_index; i++) {
			if (!solution.post_expansion_contracting.skipped && i >= solution.post_expansion_contracting.start_index && i <= solution.post_expansion_contracting.end_index)
				movement_states[i] = ns_hmm_contracting_post_expansion;
			else if (!solution.expanding.skipped && i >= solution.expanding.start_index && i <= solution.expanding.end_index)
				movement_states[i] = ns_hmm_moving_weakly_expanding;
			else if (!solution.expanding.skipped && i > solution.expanding.end_index)
				movement_states[i] = ns_hmm_moving_weakly_post_expansion;
			else movement_states[i] = ns_hmm_moving_weakly;
		}
	}
	if (!solution.dead.skipped) {
		if (solution.moving.skipped && solution.slowing.skipped) {
			for (unsigned int i = 0; i <= solution.dead.start_index; i++)
				movement_states[i] = ns_hmm_missing;
		}
		for (unsigned int i = solution.dead.start_index; i <= solution.dead.end_index; i++) {
			if (!solution.post_expansion_contracting.skipped && i >= solution.post_expansion_contracting.start_index && i <= solution.post_expansion_contracting.end_index)
				movement_states[i] = ns_hmm_contracting_post_expansion;
			else if (!solution.expanding.skipped && i >= solution.expanding.start_index && i <= solution.expanding.end_index)
				movement_states[i] = ns_hmm_not_moving_expanding;
			else if (!solution.expanding.skipped && i < solution.expanding.start_index) {
				if (estimator.state_defined(ns_hmm_not_moving_alive))
					movement_states[i] = ns_hmm_not_moving_alive;
				else  movement_states[i] = ns_hmm_not_moving_dead;
			}
			else movement_states[i] = ns_hmm_not_moving_dead;
		}
	}
	//find first not excluded state
	unsigned long start_i(0);
	bool start_i_found = false;
	for (int i = 0; i < path.element_count(); i++) {
		if (path.element(i).excluded || path.element(i).censored)
			continue;
		if (movement_states[i] == ns_hmm_unknown_state)
			throw ns_ex("Error generating hmm states");
		if (!start_i_found) {
			start_i = i;
			start_i_found = true;
		}
	}
	if (!start_i_found)
		throw ns_ex("No unexcluded states!");

	std::vector<double> emission_log_probabilities;
	std::vector<std::vector<double> > log_transition_weight_probability;
	std::vector<std::vector<double> > log_state_transition_probabilitiy;
	build_state_transition_weight_matrix(estimator, log_transition_weight_probability);
	for (unsigned int i = 0; i < log_transition_weight_probability.size(); i++)
		for (unsigned int j = 0; j < log_transition_weight_probability[i].size(); j++)
			log_transition_weight_probability[i][j] = log(log_transition_weight_probability[i][j]);
	
	//get first one to fill in unknowns

	ns_hmm_movement_state previous_state = movement_states[0];
	double cur_p = 0, log_likelihood(0);
	unsigned long cur_pi(0);
	if (generate_path_info) 
		state_info.path.resize(movement_states.size());
	for (unsigned int i = start_i; i < path.element_count(); i++) {
		if (path.element(i).excluded || path.element(i).censored) {
			if (generate_path_info) {
				state_info.path[i].state = ns_hmm_unknown_state;
				state_info.path[i].total_log_probability = 0;
				state_info.path[i].sub_measurements.resize(estimator.number_of_sub_probabilities(), 0);
				state_info.path[i].log_sub_probabilities.resize(estimator.number_of_sub_probabilities(), 0);
			}
			continue;
		}
	
		if (movement_states[i] == ns_hmm_unknown_state)
			throw ns_ex("ns_hmm_solver::probability_of_path_solution()::encountered an unknown state");
		estimator.log_probability_for_each_state(path.element(i).measurements, emission_log_probabilities);
		double state_transition_logp(0);
		if (i > start_i) {
			estimator.state_transition_log_probabilities((double)path.element(i).absolute_time - (double)path.element(i - 1).absolute_time,
				log_transition_weight_probability, log_state_transition_probabilitiy);
			state_transition_logp = log_state_transition_probabilitiy[previous_state][movement_states[i]];
		}
		cur_p = state_transition_logp +emission_log_probabilities[movement_states[i]];

		log_likelihood += cur_p;
		if (generate_path_info) {
			state_info.path[i].state = movement_states[i];
			state_info.path[i].total_log_probability = cur_p;
		}
	
		if (generate_path_info)
			estimator.provide_measurements_and_log_sub_probabilities(movement_states[i], path.element(i).measurements, state_info.path[i].sub_measurements, state_info.path[i].log_sub_probabilities);
	
		previous_state = movement_states[i];
	}
	state_info.log_likelihood = log_likelihood;
}


//run the viterbi algorithm using the specified indicies of the path
void ns_forbid_requested_hmm_states(const ns_emperical_posture_quantification_value_estimator & e, const unsigned long offset, std::vector<char > & forbidden) {

	switch (e.states_permitted()) {
		case ns_all_states_permitted: break;
		case no_expansion_while_alive_nor_contraction:
			forbidden[offset + ns_hmm_contracting_post_expansion] = 1; //deliberate readthrough
		case ns_no_expansion_while_alive:
			forbidden[offset + ns_hmm_moving_weakly_expanding] = 1;
			forbidden[offset + ns_hmm_moving_weakly_post_expansion] = 1; break;
		case ns_no_expansion_nor_contraction:
			forbidden[offset + ns_hmm_moving_weakly_expanding] = 1;
			forbidden[offset + ns_hmm_moving_weakly_post_expansion] = 1;
			forbidden[offset + ns_hmm_not_moving_expanding] = 1;	//deliberate case readthrough
		case ns_no_post_expansion_contraction:
			forbidden[offset + ns_hmm_contracting_post_expansion] = 1; break;
		case ns_require_movement_expansion_synchronicity:
			forbidden[offset + ns_hmm_moving_weakly_expanding] = 1;
			forbidden[offset + ns_hmm_moving_weakly_post_expansion] = 1; 
			forbidden[offset + ns_hmm_not_moving_alive] = 1; break;
		default: throw ns_ex("Invalid state permission flag: ") << (int)e.states_permitted();
	}
}

double ns_hmm_solver::run_viterbi(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const std::vector<unsigned long> &path_indices,
	std::vector<ns_hmm_state_transition_time_path_index > &movement_transitions, std::vector<double > & probabilitiy_of_path, std::vector<unsigned long > & previous_state) {
	const int number_of_states((int)ns_hmm_unknown_state);

	//this time-independent matrix is multiplied against the time-dependent state transitions
	//to forbid certain transitions, penalize others, etc.
	std::vector<std::vector<double> > state_transition_weight_matrix;
	build_state_transition_weight_matrix(estimator, state_transition_weight_matrix);
	//std::ofstream out("c:\\server\\state_transition_dot.txt");
	//output_state_transition_matrix(state_transition_matrix, out);
	//out.close();
	for (unsigned int i = 0; i < state_transition_weight_matrix.size(); i++)
		for (unsigned int j = 0; j < state_transition_weight_matrix[i].size(); j++)
			state_transition_weight_matrix[i][j] = log(state_transition_weight_matrix[i][j]);

	std::set<ns_hmm_movement_state> defined_states;
	estimator.defined_states(defined_states);
	const bool allow_weakly = defined_states.find(ns_hmm_moving_weakly) != defined_states.end();

	std::vector<char > path_forbidden;

	unsigned long first_appearance_id = path.first_stationary_timepoint();

	std::vector<unsigned long>optimal_path_state(path_indices.size(), 0);
	double optimal_path_log_probability, cumulative_renormalization_factor(0);
	//std::cerr << "Start:\n"
	{
		unsigned long fbdone = 0, nobs(path_indices.size());
		unsigned long mstat(number_of_states);
		unsigned long lrnrm;
		probabilitiy_of_path.resize(0);
		probabilitiy_of_path.resize(nobs*mstat, 0);
		std::vector<double> renormalization_factors(nobs,0);
		previous_state.resize(0);
		previous_state.resize(nobs*mstat, 0);
		path_forbidden.resize(0);
		path_forbidden.resize(nobs*mstat, 0);
		
		std::vector<double> emission_log_probabilities;
		std::vector<std::vector<double> > state_transition_probabilities, last_measurement_transition_probability;
		estimator.log_probability_for_each_state(path.element(path_indices[0]).measurements, emission_log_probabilities);
		path_forbidden[ns_hmm_moving_weakly_post_expansion] = 1; //do not allow animals to start as moving weakly post-expansion.  They can only start weakly /pre-expansion/ if the expansion was not observed!
		path_forbidden[ns_hmm_contracting_post_expansion] = 1; //do not allow animals to start as contracting post-expansion.  They can only start weakly /pre-expansion/ if the expansion was not observed!
		if (!allow_weakly) path_forbidden[ns_hmm_moving_weakly] = 1;
		ns_forbid_requested_hmm_states(estimator, 0, path_forbidden);

		//find the most likely final state of the path
		long last_valid_measurement;
		for (last_valid_measurement = nobs - 1; last_valid_measurement >= 0; last_valid_measurement--) {
			if (!path.element(last_valid_measurement).excluded)
				break;
		}
		if (last_valid_measurement == 0)
			throw ns_ex("Found entirely excluded path!");

		
		if (first_appearance_id == 0) //if the animal is present in the first frame, disallow "missing"
			emission_log_probabilities[ns_hmm_missing] = 0;
		long i, j, t;
		double max_p, max_prev_i;
		for (i = 0; i < mstat; i++) probabilitiy_of_path[i] = emission_log_probabilities[i];

		for (t = 1; t < nobs; t++) {
			bool missing_disalowed = t >= path_indices[first_appearance_id];
			if (path.element(t).excluded) {
				//skip, staying in the same state
				for (j = 0; j < mstat; j++) {
					probabilitiy_of_path[mstat*t + j] = probabilitiy_of_path[mstat*(t - 1) + j];
					previous_state[t*mstat + j] = previous_state[(t - 1)*mstat + j];
					path_forbidden[mstat*t + j] = path_forbidden[mstat*(t - 1) + j];
				}
				continue;
			}
			//emission probabilities
			estimator.log_probability_for_each_state(path.element(path_indices[t]).measurements, emission_log_probabilities);

			//state transition probabilities
			const double duration = (double)path.element(t).absolute_time - (double)path.element(t - 1).absolute_time;
			estimator.state_transition_log_probabilities(duration, state_transition_weight_matrix, state_transition_probabilities);
			if (t==last_valid_measurement)
				estimator.state_transition_log_probabilities(duration, state_transition_weight_matrix, last_measurement_transition_probability);
			
			//identify situations where no state produces a non-zero probability
			//skip these points, staying in the same state.
			bool nonzero_state_probability_found = false;
			for (unsigned int w = 0; w < emission_log_probabilities.size(); w++)
				if (std::isfinite(emission_log_probabilities[w]))
					nonzero_state_probability_found = true;
			if (!nonzero_state_probability_found) {
				//	std::cout << "Found impossible measurement: \n";
				for (j = 0; j < mstat; j++) {
					probabilitiy_of_path[mstat*t + j] = probabilitiy_of_path[mstat*(t - 1) + j];
					previous_state[t*mstat + j] = previous_state[(t - 1)*mstat + j];
					path_forbidden[t*mstat + j] = path_forbidden[(t - 1)*mstat + j];
				}
				continue;
			}
			
			double max_pp = -DBL_MAX; //most likely path to reach any state at this time

			for (j = 0; j < mstat; j++) { 
				max_p = -DBL_MAX;	//most likely path to reach state j  at this time
				max_prev_i = 0;
				bool found_valid = false;
				for (i = 0; i < mstat; i++) {
					//was a bug here, state_transition_matrix[i][j]==0, causing problems all allong?  
					if (!std::isfinite(state_transition_probabilities[i][j])  || !std::isfinite(emission_log_probabilities[j]) || path_forbidden[mstat*(t - 1) + i])
						continue;
					//calculate probability of moving from state i at time t-1 to state j now
					double cur = probabilitiy_of_path[mstat*(t - 1)+i] + state_transition_probabilities[i][j] + emission_log_probabilities[j];
					//if this is the last measurement, we also must include
					//any penalty for /staying in this state/ at thte end of the observation period.
					if (t == last_valid_measurement)
						cur += last_measurement_transition_probability[j][ns_hmm_end_of_observation];
					if (!std::isnan(cur) && std::isfinite(cur) && (!found_valid || cur > max_p)) {
						max_p = cur;
						max_prev_i = i;
						found_valid = true;
					}
				}
				//don't allow animals to be missing after their first observed start time.
				//this avoids an error where the algorithm decides worms never appear
				if (!found_valid ||  missing_disalowed && j == ns_hmm_missing)
					path_forbidden[mstat*t + j] = 1;
				ns_forbid_requested_hmm_states(estimator, mstat*t, path_forbidden);

				probabilitiy_of_path[mstat*t+j] = max_p;
				previous_state[t*mstat +j] = max_prev_i;

				if (!std::isnan(max_p) && std::isfinite(max_p) && max_p > max_pp)
					max_pp = max_p;
			}
			if (!allow_weakly) path_forbidden[mstat*t + ns_hmm_moving_weakly] = 1;

			const double renom_lim(-1e100);
			if (max_pp < renom_lim) {
				renormalization_factors[t] = -max_pp;
				for (j = 0; j < mstat; j++) {
					probabilitiy_of_path[mstat*t + j] -= max_pp;
				}
			}

		}
		*optimal_path_state.rbegin() = 0;
		optimal_path_log_probability = -DBL_MAX;
		cumulative_renormalization_factor = 0;

		bool found_valid = false;

		//find the most likely final state of the path
		for (j = 0; j < mstat; j++) {
			if (path_forbidden[mstat* last_valid_measurement + j])
				continue;
			if (!found_valid || probabilitiy_of_path[mstat*last_valid_measurement + j] > optimal_path_log_probability) {
				optimal_path_log_probability = probabilitiy_of_path[mstat* last_valid_measurement + j];
				cumulative_renormalization_factor = renormalization_factors[last_valid_measurement];
				*optimal_path_state.rbegin() = j;
				found_valid = true;
			}
		}

		//now work backwards and recrete the path that got us there.
		for (t=last_valid_measurement-1; t >= 1; t--) {
			if (path.element(t).excluded) {
				optimal_path_state[t - 1] = optimal_path_state[t];
				continue;
			}
			optimal_path_state[t - 1] = previous_state[mstat*t + optimal_path_state[t]];
			optimal_path_log_probability += probabilitiy_of_path[mstat*t + optimal_path_state[t]];
			cumulative_renormalization_factor += renormalization_factors[t];
		}
		if (!path.element(0).excluded) {
			optimal_path_log_probability += probabilitiy_of_path[mstat * t + optimal_path_state[0]];
			cumulative_renormalization_factor += renormalization_factors[0];
		}

	
			//std::cout << "\n";
		//now find transition of times between states
		if (optimal_path_state.empty())
			throw ns_ex("Empty path state!");
		//now find transition of times between states
		movement_transitions.resize(0);
		movement_transitions.push_back(ns_hmm_state_transition_time_path_index((ns_hmm_movement_state)optimal_path_state[0], 0));
		for (unsigned int i = 1; i < optimal_path_state.size(); i++) {
			const ns_hmm_movement_state s = (ns_hmm_movement_state)optimal_path_state[i];
			if (s != movement_transitions.rbegin()->first)
				movement_transitions.push_back(ns_hmm_state_transition_time_path_index(s, i));
		}
		if (0 && movement_transitions.size() == 1) {
			std::ofstream out("c:\\server\\dbg.csv");
			out << "t,R";
			for (unsigned int j = 0; j < mstat; j++)
				out << "," << "cumulative p " << ns_hmm_movement_state_to_string((ns_hmm_movement_state)j);
			for (unsigned int j = 0; j < mstat; j++)
				out << "," << "p(t) " << ns_hmm_movement_state_to_string((ns_hmm_movement_state)j);
			for (unsigned int j = 0; j < mstat; j++)
				out << "," << "s(t) " << ns_hmm_movement_state_to_string((ns_hmm_movement_state)j);
			for (unsigned int j = 0; j < mstat; j++)
				out << "," << "f(t) " << ns_hmm_movement_state_to_string((ns_hmm_movement_state)j);
			out << "\n";
			for (unsigned int t = 0; t < nobs; t++) {
				out << t << "," << renormalization_factors[t];
				for (unsigned int j = 0; j < mstat; j++)
					out << "," << probabilitiy_of_path[mstat*t + j];
				estimator.log_probability_for_each_state(path.element(path_indices[t]).measurements, emission_log_probabilities);
				for (unsigned int j = 0; j < mstat; j++)
					out << "," << emission_log_probabilities[j];
				for (unsigned int j = 0; j < mstat; j++)
					out << "," << ns_hmm_movement_state_to_string((ns_hmm_movement_state)previous_state[mstat*t + j]);
				for (unsigned int j = 0; j < mstat; j++)
					out << "," << (path_forbidden[mstat*t + j]?"+":"-");
				out << "\n";
			}
			out.close();
			std::cerr << "Singleton produced\n";

		}
	}
	//if (movement_transitions.size() == 1)
	//	std::cout << "YIKES";

	return optimal_path_log_probability- cumulative_renormalization_factor;
}

void ns_hmm_solver::build_movement_state_solution_from_movement_transitions(const unsigned long first_stationary_path_index,const std::vector<unsigned long> path_indices, const std::vector<ns_hmm_state_transition_time_path_index > & movement_transitions){
	if (first_stationary_path_index >= path_indices.size())
		throw ns_ex("ns_hmm_solver::build_movement_state_solution_from_movement_transitions()::Invalid first stationary path index");
	movement_state_solution.moving.start_index = path_indices[first_stationary_path_index];
	ns_movement_state m = ns_movement_fast;

	//set all states skipped by default.
	movement_state_solution.dead.end_index = movement_state_solution.dead.start_index = 0;
	movement_state_solution.dead.skipped = true;
	movement_state_solution.dead.longest_observation_gap_within_interval = 0;
	
	movement_state_solution.expanding = movement_state_solution.post_expansion_contracting = movement_state_solution.post_expansion_contracting = movement_state_solution.slowing = movement_state_solution.dead;

	int expanding_state = 0, contracting_state = 0;
	movement_state_solution.moving.skipped = false;
	movement_state_solution.moving.start_index = first_stationary_path_index;
	movement_state_solution.moving.end_index = *path_indices.rbegin();


	//go through each of the state transitions and annotate what has happened in the solution.
	for (unsigned int i = 0; i < movement_transitions.size(); i++) {
		//look for movement transitions
		if (m == ns_movement_fast && (movement_transitions[i].first != ns_hmm_missing && movement_transitions[i].first != ns_hmm_moving_vigorously)) {
			if (i != 0 && movement_transitions[i-1].first != ns_hmm_missing) { //if we had a period of fast movement, mark it as not being skipped.
				movement_state_solution.moving.skipped = false;
				movement_state_solution.moving.end_index = path_indices[movement_transitions[i].second];
			}
			else 
				movement_state_solution.moving.skipped = true;
			
			m = ns_movement_slow;
			movement_state_solution.slowing.start_index = path_indices[movement_transitions[i].second];
			movement_state_solution.slowing.end_index = *path_indices.rbegin();
			movement_state_solution.slowing.skipped = (movement_state_solution.slowing.start_index == movement_state_solution.slowing.end_index);
		}
		if (m == ns_movement_slow && (
			movement_transitions[i].first != ns_hmm_moving_weakly &&
			movement_transitions[i].first != ns_hmm_moving_weakly_expanding &&
			movement_transitions[i].first != ns_hmm_moving_weakly_post_expansion)) {
			if (path_indices[movement_transitions[i].second] != movement_state_solution.slowing.start_index)
				movement_state_solution.slowing.end_index = path_indices[movement_transitions[i].second];
			else {
				movement_state_solution.slowing.end_index = movement_state_solution.slowing.start_index = 0;
				movement_state_solution.slowing.skipped = true;
			}
			m = ns_movement_stationary;
				movement_state_solution.dead.start_index = path_indices[movement_transitions[i].second];
				movement_state_solution.dead.end_index = *path_indices.rbegin();
			movement_state_solution.dead.skipped = movement_state_solution.dead.start_index == movement_state_solution.dead.end_index;
		}
		//now we handle expansions
		switch (expanding_state) {
		case 0:
			if (movement_transitions[i].first == ns_hmm_moving_weakly_expanding ||
				movement_transitions[i].first == ns_hmm_not_moving_expanding) {
				movement_state_solution.expanding.start_index = path_indices[movement_transitions[i].second];
				movement_state_solution.expanding.end_index = *path_indices.rbegin();
				movement_state_solution.expanding.skipped = movement_state_solution.expanding.start_index == movement_state_solution.expanding.end_index;
				expanding_state = 1;
			}
			else if (movement_transitions[i].first == ns_hmm_moving_weakly_post_expansion) {
				//std::cerr << "Encountered an invalid post expansion state.";
				ns_ex ex("Encountered an invalid post expansion state:");
				for (unsigned int j = 0; j < movement_transitions.size(); j++) {
					ex << "\n" << movement_transitions[j].second << ":" << ns_hmm_movement_state_to_string(movement_transitions[j].first);
				}
				throw ex;
			}
			break;
		case 1:
			if (movement_transitions[i].first != ns_hmm_moving_weakly_expanding &&
				movement_transitions[i].first != ns_hmm_not_moving_expanding) {
				movement_state_solution.expanding.end_index = path_indices[movement_transitions[i].second];
				expanding_state = 2;
				if (movement_state_solution.expanding.end_index == movement_state_solution.expanding.start_index)
					std::cout << "invisble expansion transition";

			}
			break;
		case 2: if (movement_transitions[i].first != ns_hmm_moving_weakly_post_expansion &&
			movement_transitions[i].first != ns_hmm_not_moving_dead &&
			movement_transitions[i].first != ns_hmm_contracting_post_expansion)
			throw ns_ex("Unexpected post-expansion state!");
		}

		//now we handle contractions
		switch (contracting_state) {
		case 0:
			if (movement_transitions[i].first == ns_hmm_contracting_post_expansion) {
				//if (expanding_state == 0)
				//	throw ns_ex("Encountered a contracting animal that had not first expanded.");
				movement_state_solution.post_expansion_contracting.start_index = path_indices[movement_transitions[i].second];
				movement_state_solution.post_expansion_contracting.end_index = *path_indices.rbegin();
				movement_state_solution.post_expansion_contracting.skipped = movement_state_solution.post_expansion_contracting.start_index == movement_state_solution.post_expansion_contracting.end_index;
				contracting_state = 1;
			}
			
			break;
		case 1:
			if (movement_transitions[i].first != ns_hmm_contracting_post_expansion) {
				movement_state_solution.post_expansion_contracting.end_index = path_indices[movement_transitions[i].second];
				expanding_state = 2;
				if (movement_state_solution.post_expansion_contracting.end_index == movement_state_solution.post_expansion_contracting.start_index)
					std::cout << "invisble contraction transition";

			}
			break;
		case 2: if (movement_transitions[i].first != ns_hmm_not_moving_dead)
			throw ns_ex("Failure to die after contraction");
		}
	}
	//zero out for neatness.  These values shouldn't be used, but leaving them set can be confusing during debugging.
	if (movement_state_solution.moving.skipped)
		movement_state_solution.moving.start_index = movement_state_solution.moving.end_index = 0;
	if (movement_state_solution.slowing.skipped)
		movement_state_solution.slowing.start_index = movement_state_solution.slowing.end_index = 0;
	if (movement_state_solution.dead.skipped)
		movement_state_solution.dead.start_index = movement_state_solution.dead.end_index = 0;
	if (movement_state_solution.expanding.skipped)
		movement_state_solution.expanding.start_index = movement_state_solution.expanding.end_index = 0;
	if (movement_state_solution.post_expansion_contracting.skipped)
		movement_state_solution.post_expansion_contracting.start_index = movement_state_solution.post_expansion_contracting.end_index = 0;

	//if (movement_transitions.size() == 1)
	//	std::cerr << "Singleton transition encountered\n";
}


void ns_hmm_solver::output_state_transition_matrix(const std::string & title, std::vector<std::vector<double> >& m, std::ostream& o) {
	o << std::setprecision(5);
	o << "digraph " << title << " {\n";
	o << "forcelabels=true;\n";
	std::vector<char> node_has_neighbors(m.size(),0);
	for (unsigned int i = 0; i < m.size(); i++) {
		for (unsigned int j = 0; j < m[i].size(); j++) {
			if (i == j) continue;
			if (m[i][j] != 0 || m[j][i] != 0) {
				node_has_neighbors[i] = true;
				break;
			}
		}
	}
	for (unsigned int i = 0; i < m.size(); i++) {
		if (!node_has_neighbors[i])
			continue;
		o << "a_" << i << " [label=\"" << ns_hmm_movement_state_to_string((ns_hmm_movement_state)i) << "\"];\n";
	}
	
	for (unsigned int i = 0; i < m.size(); i++) {
		if (!node_has_neighbors[i])
			continue;
		for (unsigned int j = 0; j < m[i].size(); j++)
			if (m[i][j] != 0)
				o << "a_" << i << " -> " << "a_" << j << " [label=\"" << std::fixed << m[i][j] << "\"];\n";
	}
	o << "}";
}


void ns_emperical_posture_quantification_value_estimator::state_transition_log_probabilities(const double& duration_in_seconds, const std::vector < std::vector<double> >& log_weight_matrix, std::vector<std::vector<double>>& log_prob) const{	ns_hmm_duration duration;
	duration.measurement = duration_in_seconds;
	log_prob.resize((int)ns_hmm_unknown_state);
	const bool using_static_probs(state_transition_type == ns_static || state_transition_type == ns_static_mod);
	//set up default matrix
	if ((log_weight_matrix.size() != 0 || using_static_probs) && log_weight_matrix.size() != log_prob.size())
		throw ns_ex("Invalid weight matrix");
	for (unsigned int i = 0; i < (unsigned int)ns_hmm_unknown_state; i++) {
		log_prob[i].resize((int)ns_hmm_unknown_state);
		if ((log_weight_matrix.size() != 0 || using_static_probs )&& log_weight_matrix[i].size() != log_prob[i].size())
			throw ns_ex("Invalid weight matrix");
		if (!using_static_probs) {
			for (unsigned int j = 0; j < (unsigned int)ns_hmm_unknown_state; j++)
				log_prob[i][j] = -INFINITY;
		}
		else {
			for (unsigned int j = 0; j < (unsigned int)ns_hmm_unknown_state; j++)
				log_prob[i][j] = log_weight_matrix[i][j];
		}
	}
	if (state_transition_type == ns_static || state_transition_type == ns_static_mod)
		return;

	for (auto p = emission_probability_models->state_transition_models.begin(); p != emission_probability_models->state_transition_models.end(); p++) {

		if (p->first.first == p->first.second) // Self transitions are the probability of /not/ exiting the current state to any other state...we calculate this later.
			continue; 
		//transition probability is the probability of transitioning to that state at the specified time, multipled by the probability of entering that state ever.
		double external_weight = 0;
		if (log_weight_matrix.size() != 0)
			external_weight = log_weight_matrix[(int)p->first.first][(int)p->first.second];
		double prob = log(p->second->point_emission_likelihood(duration)), 
			model_weight = 0;
		if (state_transition_type != ns_empirical_without_weights)
			model_weight = log(p->second->model_weight());		//this is analogous to the gillespie algorithm.  if an event happens, scale the probability by the fraction of animals that make that transition.

		log_prob[(int)p->first.first][(int)p->first.second] = prob+ model_weight + external_weight;
	}
	//now we handle self transition probabilities;
	for (unsigned int i = 0; i < log_prob.size(); i++) {
		double total_exit_prob = 0;
		for (unsigned int j = 0; j < log_prob.size(); j++) {
			if (i == j) continue;
			if (std::isfinite(log_prob[i][j]))
				total_exit_prob += exp(log_prob[i][j]);
		}
		double external_weight = 0;
		if (log_weight_matrix.size() != 0)
			external_weight = log_weight_matrix[(int)i][(int)i];

		if (total_exit_prob > 1)
			throw ns_ex("Invalid exit probability");
		log_prob[i][i] = log(1 - total_exit_prob) + external_weight;
	}
}


//m[i][j] is the bais for or against an individual in state i transitioning to state j.
//the actual probabilities are calculated from empiric transition probabilities multiplied by these weights
void ns_hmm_solver::build_state_transition_weight_matrix(const ns_emperical_posture_quantification_value_estimator & estimator, std::vector<std::vector<double> > & m)  {
	m.resize((int)ns_hmm_unknown_state);

	const double penalized_transition = 1;

	//Note that any state transition that is not explicitly defined later in this function will be zero by default,
	//and be forbidden
	for (unsigned int i = 0; i < (int)ns_hmm_unknown_state; i++) {
		m[i].resize(0);
		m[i].resize((int)ns_hmm_unknown_state, 0);
		switch (estimator.states_transitions()) {
		case ns_emperical_posture_quantification_value_estimator::ns_static:
			m[i][i] = 1000;	break;
		case ns_emperical_posture_quantification_value_estimator::ns_static_mod:
			m[i][i] = 1;	break;
		default: m[i][i] = 1; break;
		}
	}

	std::set<ns_hmm_movement_state> defined_states;
	estimator.defined_states(defined_states);
	const bool allow_weakly = defined_states.find(ns_hmm_moving_weakly) != defined_states.end();
//	ns_all_states_permitted, ns_no_post_expansion_contraction, ns_no_expansion_while_alive, no_expansion_while_alive_nor_contraction, ns_no_expansion_nor_contraction, ns_require_movement_expansion_synchronicity, ns_number_of_state_settings



	const bool allow_expansion = estimator.states_permitted() != ns_no_expansion_nor_contraction;
	const bool all_expansion_while_alive = (estimator.states_permitted() != ns_no_expansion_while_alive && 
											estimator.states_permitted() != ns_require_movement_expansion_synchronicity &&
											estimator.states_permitted() != no_expansion_while_alive_nor_contraction &&
										 allow_expansion && allow_weakly);
	const bool allow_contraction = allow_expansion && estimator.states_permitted() != ns_no_post_expansion_contraction;

	const bool allow_not_moving_alive = estimator.states_permitted() != ns_require_movement_expansion_synchronicity;


	//if there are any loops anywhere here, the approach will not function
	//because we set all the transition probabilities equal and this works
	//only because the all state transitions are irreversable.
	m[ns_hmm_missing][ns_hmm_moving_vigorously] = 1;
	if (allow_weakly) m[ns_hmm_missing][ns_hmm_moving_weakly] = 1;
	if (all_expansion_while_alive) m[ns_hmm_missing][ns_hmm_moving_weakly_expanding] = 1;
	if (allow_expansion) {
		if (allow_not_moving_alive)
			m[ns_hmm_missing][ns_hmm_not_moving_alive] = 1;
		m[ns_hmm_missing][ns_hmm_not_moving_expanding] = 1;
	}
	m[ns_hmm_missing][ns_hmm_not_moving_dead] = penalized_transition;	//we penalize any path that skips death time expansion
	if (allow_weakly) m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly] = 1;
	if (all_expansion_while_alive) m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly_expanding] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly] = 1;
	if (allow_expansion) {
		if (allow_not_moving_alive)
			m[ns_hmm_moving_vigorously][ns_hmm_not_moving_alive] = 1;
		m[ns_hmm_moving_vigorously][ns_hmm_not_moving_expanding] = 1;
	}
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_dead] = penalized_transition;
	m[ns_hmm_moving_vigorously][ns_hmm_end_of_observation] = penalized_transition;

	if (all_expansion_while_alive) m[ns_hmm_moving_weakly][ns_hmm_moving_weakly_expanding] = 1;
	if (allow_weakly){
		m[ns_hmm_moving_weakly][ns_hmm_not_moving_dead] = allow_expansion ? penalized_transition : 1;
		m[ns_hmm_moving_vigorously][ns_hmm_end_of_observation] = penalized_transition;
		if (allow_expansion) {
			if (allow_not_moving_alive)
				m[ns_hmm_moving_weakly][ns_hmm_not_moving_alive] = 1;
			m[ns_hmm_moving_weakly][ns_hmm_not_moving_expanding] = 1;
		}
		if (allow_contraction)
			m[ns_hmm_moving_weakly][ns_hmm_contracting_post_expansion] = 1;


	}

	if (allow_weakly && all_expansion_while_alive) {
		m[ns_hmm_moving_weakly_expanding][ns_hmm_moving_weakly_post_expansion] = 1;
		m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_expanding] = 1;
		if (allow_contraction) {
			m[ns_hmm_moving_weakly_expanding][ns_hmm_contracting_post_expansion] = 1;
			m[ns_hmm_moving_weakly_post_expansion][ns_hmm_contracting_post_expansion] = 1;
		}
		m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_dead] = 1;
		m[ns_hmm_moving_weakly_expanding][ns_hmm_end_of_observation] = penalized_transition;
		
		m[ns_hmm_moving_weakly_post_expansion][ns_hmm_not_moving_dead] = 1;
		m[ns_hmm_moving_weakly_post_expansion][ns_hmm_end_of_observation] = penalized_transition;
	}

	if (allow_expansion) {
		m[ns_hmm_not_moving_expanding][ns_hmm_not_moving_dead] = 1;
		m[ns_hmm_not_moving_expanding][ns_hmm_contracting_post_expansion] = 1;
		m[ns_hmm_not_moving_expanding][ns_hmm_end_of_observation] = penalized_transition;
		if (allow_contraction) {
			m[ns_hmm_contracting_post_expansion][ns_hmm_not_moving_dead] = 1;
			m[ns_hmm_contracting_post_expansion][ns_hmm_end_of_observation] = 1;
			if (allow_not_moving_alive)
				m[ns_hmm_not_moving_alive][ns_hmm_contracting_post_expansion] = 1;
		}
		if (allow_not_moving_alive) {
			m[ns_hmm_not_moving_alive][ns_hmm_not_moving_expanding] = 1;
			m[ns_hmm_not_moving_alive][ns_hmm_not_moving_dead] = penalized_transition;
			m[ns_hmm_not_moving_alive][ns_hmm_end_of_observation] = penalized_transition;
		}
	}
	m[ns_hmm_not_moving_dead][ns_hmm_end_of_observation] = 1;

	//identify and remove states not considered by this estimator
	for (unsigned int s = 0; s < (int)ns_hmm_unknown_state; s++) {
		if (!estimator.state_specified_by_model((ns_hmm_movement_state)s)) {
			for (unsigned int i = 0; i < (int)ns_hmm_unknown_state; i++) {
				m[s][i] = 0;
				m[i][s] = 0;
			}
		}
	}

	//normalize all transition probabilities
	for (unsigned int i = 0; i < (int)ns_hmm_unknown_state; i++) {
		unsigned int sum = 0;
		for (unsigned int j = 0; j < (int)ns_hmm_unknown_state; j++)
			sum += m[i][j];
		if (sum != 0)
		for (unsigned int j = 0; j < (int)ns_hmm_unknown_state; j++)
			m[i][j] = m[i][j] / (double)sum;
	}
}

ns_hmm_movement_state ns_hmm_solver::most_probable_state(const std::vector<double> & d) {
	ns_hmm_movement_state s((ns_hmm_movement_state)0);
	double p(d[0]);
	for (unsigned int i = 1; i < d.size(); i++)
		if (d[i] > p) {
			s = (ns_hmm_movement_state)i;
			p = d[i];
		}
	return s;
}

/*
	//forward backward
	unsigned long fbdone = 0, nobs(path_indices.size());
	unsigned long mstat(number_of_states);
	unsigned long lrnrm;
	std::vector<std::vector<double> > alpha(nobs, std::vector<double>(mstat, 0)),
		beta(nobs, std::vector<double>(mstat, 0)),
		pstate(nobs, std::vector<double>(mstat, 0));
	std::vector<unsigned long> arnrm(nobs, 0), brnrm(nobs, 0);

	double BIG(1.e20), BIGI(1. / BIG), lhood;
	{

		std::vector<double> emission_log_probabilities;
		estimator.probability_for_each_state(path->element(path_indices[0]).measurements, emission_log_probabilities);
		int i, j, t;
		double sum, asum, bsum;
		for (i = 0; i < mstat; i++) alpha[0][i] = emission_log_probabilities[i];
		arnrm[0] = 0;
		for (t = 1; t < nobs; t++) {
			if (path->element(t).excluded)
				continue;
			estimator.probability_for_each_state(path->element(path_indices[t]).measurements, emission_log_probabilities);
			asum = 0;
			for (j = 0; j < mstat; j++) {
				sum = 0.;
				for (i = 0; i < mstat; i++) sum += alpha[t - 1][i] * a[i][j] * emission_log_probabilities[j];  //probability of moving from state i at t-1 to state j at t
				alpha[t][j] = sum;
				asum += sum;
			}
			arnrm[t] = arnrm[t - 1];
			if (asum < BIGI) {
				++arnrm[t];
				for (j = 0; j < mstat; j++) alpha[t][j] *= BIG;
			}
		}
		for (i = 0; i < mstat; i++) beta[nobs - 1][i] = 1.;
		brnrm[nobs - 1] = 0;
		for (t = nobs - 2; t >= 0; t--) {

			estimator.probability_for_each_state(path->element(path_indices[t + 1]).measurements, emission_log_probabilities);
			bsum = 0.;
			for (i = 0; i < mstat; i++) {
				sum = 0.;
				for (j = 0; j < mstat; j++) sum += a[i][j] * emission_log_probabilities[j] * beta[t + 1][j];
				beta[t][i] = sum;
				bsum += sum;
			}
			brnrm[t] = brnrm[t + 1];
			if (bsum < BIGI) {
				++brnrm[t];
				for (j = 0; j < mstat; j++) beta[t][j] *= BIG;
			}
		}
		lhood = 0.;
		for (i = 0; i < mstat; i++) lhood += alpha[0][i] * beta[0][i];
		lrnrm = arnrm[0] + brnrm[0];
		while (lhood < BIGI) { lhood *= BIG; lrnrm++; }
		for (t = 0; t < nobs; t++) {
			sum = 0.;
			for (i = 0; i < mstat; i++) sum += (pstate[t][i] = alpha[t][i] * beta[t][i]);
			for (i = 0; i < mstat; i++) pstate[t][i] /= sum;
		}
		fbdone = 1;
		solution.loglikelihood_of_solution = log(lhood) + lrnrm * log(BIGI);
	}
}
*/

std::string ns_time_path_movement_markov_solver::model_software_version_number() const {
	return estimator.software_version_when_built;
}

ns_time_path_posture_movement_solution ns_time_path_movement_markov_solver::estimate_posture_movement_states(int software_version,const ns_analyzed_image_time_path * path, std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2, ns_analyzed_image_time_path * output_path, std::ostream * debug_output)const{
	if (estimator.software_version_when_built != current_software_version_number())
		throw ns_ex("The specified HMM model was built with an outdated version, ") << estimator.software_version_when_built << ", whereas this software was compiled as version " << current_software_version_number();
	ns_hmm_solver solver;
	solver.solve(*path, estimator, tmp_storage_1, tmp_storage_2);

	return solver.movement_state_solution;
}



template<class measurement_accessor_t>
unsigned long ns_emission_probabiliy_gaussian_diagonal_covariance_model<measurement_accessor_t>::training_data_buffer_size = 0;

template<class measurement_accessor_t>
double* ns_emission_probabiliy_gaussian_diagonal_covariance_model<measurement_accessor_t>::training_data_buffer = 0;

template<class measurement_accessor_t>
ns_lock ns_emission_probabiliy_gaussian_diagonal_covariance_model<measurement_accessor_t>::training_data_buffer_lock("tbl");


template<class measurement_accessor_t>
std::vector<double> ns_state_transition_probability_model<measurement_accessor_t>::training_data_buffer;

template<class measurement_accessor_t>
ns_lock ns_state_transition_probability_model<measurement_accessor_t>::training_data_buffer_lock("tbl2");

bool operator<(const ns_gmm_sorter& a, const ns_gmm_sorter& b) {
	return a.weight < b.weight;
}
inline bool ns_double_equal(const double& a, const double& b) {
	return a == b || fabs(a / b - 1) < .0001;
}
bool operator==(const GMM& a, const GMM& b) {
	if (a.GetDimNum() != b.GetDimNum()) {
		std::cerr << "GMM dimensions do not match\n";
		return false;
	}
	if (a.GetMixNum() != b.GetMixNum()) {
		std::cerr << "GMM gaussian counts do not match\n";
		return false;
	}
	for (int i = 0; i < a.GetMixNum(); i++) {
		if (!ns_double_equal(a.Prior(i), b.Prior(i))) {
			std::cerr << "Prior " << i << " doesn't match\n";
			double q = fabs(a.Prior(i) / b.Prior(i) - 1);
			std::cerr << q << "\n";
			return false;
		}
		const double* am = a.Mean(i),
			* bm = b.Mean(i);
		for (int j = 0; j < a.GetDimNum(); j++) {
			if (!ns_double_equal(am[j], bm[j])) {
				std::cerr << "Mean " << i << ", " << j << " doesn't match\n";
				return false;
			}
		}
		am = a.Variance(i);
		bm = b.Variance(i);
		for (int j = 0; j < a.GetDimNum(); j++) {
			if (!ns_double_equal(am[j], bm[j])) {
				std::cerr << "Variance " << i << ", " << j << " doesn't match\n";
				return false;
			}
		}
	}
	return true;
}
bool operator==(const ns_emperical_posture_quantification_value_estimator & a, const ns_emperical_posture_quantification_value_estimator & b) {
	if (a.emission_probability_models->state_emission_models.size() != b.emission_probability_models->state_emission_models.size()) {
		std::cerr << "Wrong number of state emission models\n";
		return false;
	}if (a.emission_probability_models->state_transition_models.size() != b.emission_probability_models->state_transition_models.size()) {
		std::cerr << "Wrong number of state transition models\n";
		return false;
	}
	if (a.states_permitted_int != b.states_permitted_int) {
		std::cerr << "State permission mismatch!";
		return false;
	}
	if (a.state_transition_type != b.state_transition_type) {
		std::cerr << "State transition mismatch!";
		return false;
	}
	for (auto p = a.emission_probability_models->state_emission_models.begin(); p != a.emission_probability_models->state_emission_models.end(); ++p) {
		auto q = b.emission_probability_models->state_emission_models.find(p->first);
		if (q == b.emission_probability_models->state_emission_models.end()) {
			std::cerr << "Missing state emission model\n";
			return false;
		}
		if (!(p->second->equals(q->second)))
			return false;
	}for (auto p = a.emission_probability_models->state_transition_models.begin(); p != a.emission_probability_models->state_transition_models.end(); ++p) {
		auto q = b.emission_probability_models->state_transition_models.find(p->first);
		if (q == b.emission_probability_models->state_transition_models.end()) {
			std::cerr << "Missing state transition model\n";
			return false;
		}
		if (!(p->second->equals(q->second)))
			return false;
	}
	return true;

}



void ns_emperical_posture_quantification_value_estimator::provide_measurements_and_log_sub_probabilities(const ns_hmm_movement_state & state, const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & measurement, std::vector<double> & sub_probabilitiy) const {
	bool undefined_state(false);
	auto p = emission_probability_models->state_emission_models.find(state);
	
	if (p == emission_probability_models->state_emission_models.end()) {
		//if we are debugging a state for which the emission model isn't trained, output N/A.
		if (emission_probability_models->state_emission_models.empty())
			throw ns_ex("No emission models!");
		p = emission_probability_models->state_emission_models.begin();
		undefined_state = true;
	}
	ns_hmm_emission em;
	em.measurement = e;
	p->second->log_sub_probabilities(em, measurement, sub_probabilitiy);


	//if we are debugging a state for which the emission model isn't trained, output N/A.
	if (undefined_state) {
		for (unsigned int i = 0; i < sub_probabilitiy.size(); i++)
			sub_probabilitiy[i] = -1;
	}
}
void ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names(std::vector<std::string> & names) const {
	if (emission_probability_models->state_emission_models.empty())
		throw ns_ex("ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names()::Cannot find any probability model");
	emission_probability_models->state_emission_models.begin()->second->sub_probability_names(names);
}
unsigned long ns_emperical_posture_quantification_value_estimator::number_of_sub_probabilities() const {
	if (emission_probability_models->state_emission_models.empty())
		throw ns_ex("ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names()::Cannot find any probability model");
	return emission_probability_models->state_emission_models.begin()->second->number_of_sub_probabilities();
}

bool ns_emperical_posture_quantification_value_estimator::state_defined(const ns_hmm_movement_state & m) const {
	return emission_probability_models->state_emission_models.find(m) != emission_probability_models->state_emission_models.end();

}
void ns_emperical_posture_quantification_value_estimator::defined_states(std::set<ns_hmm_movement_state>& s) const {
	for (auto p = emission_probability_models->state_emission_models.begin(); p != emission_probability_models->state_emission_models.end(); p++)
		s.emplace(p->first);
}


void ns_emperical_posture_quantification_value_estimator::log_probability_for_each_state(const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & d) const {
	d.resize(0);
	d.resize((int)ns_hmm_unknown_state,-INFINITY);
	ns_hmm_emission em;
	em.measurement = e;
	for (auto p = emission_probability_models->state_emission_models.begin(); p != emission_probability_models->state_emission_models.end(); p++) {
		const double tmp(p->second->point_emission_log_probability(em));
		d[p->first] = tmp;
	}
}

void ns_emperical_posture_quantification_value_estimator::validate_model_settings(ns_sql& sql) const {
	//any state not specified in the estimator will be removed from the transition matrix in build_transition_matrix().
	if (!state_specified_by_model(ns_hmm_not_moving_alive))
		image_server_const.register_server_event(ns_image_server_event("This model file does not have a not-moving-but-alive state specified. This means that animal's movement cessation and death-associated expansion are forced to be synchronous."), &sql);

	if (!state_specified_by_model(ns_hmm_moving_weakly) || !state_specified_by_model(ns_hmm_not_moving_dead))
		throw ns_ex("This model file does not have a moving weakly or dead state!");
}
void ns_emperical_posture_quantification_value_estimator::output_debug_info(const ns_analyzed_image_time_path_element_measurements & e, std::ostream & o) const {

	for (auto p = emission_probability_models->state_emission_models.begin(); p != emission_probability_models->state_emission_models.end(); p++) {
		std::vector<std::string> names;
		std::vector<double> measurements;
		std::vector<double> probabilities;
		p->second->sub_probability_names(names);
		ns_hmm_emission em;
		em.measurement = e;
		p->second->log_sub_probabilities(em, measurements, probabilities);
		for (unsigned int i = 0; i < names.size(); i++) {
			o << p->first << ": " << names[i] << ": " << measurements[i] << ": " << probabilities[i] << "\n";
		}
	}
}
	

void ns_hmm_observation_set::write_emissions(std::ostream & out, const std::string & experiment_name) const {
	out << "device_name,region_name,detection_set_id,group_id,path_id,data_type,time,hmm_movement_state,";
	ns_analyzed_image_time_path_element_measurements::write_header(out);
	out << "\n";
	//first write normalization stats
	out.precision(30);
	for (std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> >::const_iterator p = obs.begin(); p != obs.end(); p++) {
		for (unsigned int i = 0; i < p->second.size(); i++) {
			out << *(p->second[i].device_name) << ","
				<< *(p->second[i].region_name) << ","
				<< p->second[i].path_id.detection_set_id << ","
				<< p->second[i].path_id.group_id << ","
				<< p->second[i].path_id.path_id << ","
				<< "d," 
				<< p->second[i].emission_time << "," 
				<< ns_hmm_movement_state_to_string(p->first) << ",";
			p->second[i].measurement.write(out, ns_vector_2i(0, 0), false);
			out << "\n";
		}
	}
	for (std::map<ns_stationary_path_id, ns_hmm_emission_normalization_stats >::const_iterator p = normalization_stats.begin(); p != normalization_stats.end(); p++) {
		out << *(p->second.device_name) << ","
			<< *(p->second.region_name) << ","
		    << p->first.detection_set_id << ","
			<< p->first.group_id << ","
			<< p->first.path_id << ",";
		out << "m,0,all,";
		p->second.path_mean.write(out, ns_vector_2d(0, 0), false);
		out << "\n";
		out << *(p->second.device_name) << ","
			<< *(p->second.region_name) << "," 
			<< p->first.detection_set_id << ","
			<< p->first.group_id << ","
			<< p->first.path_id << ",";
		out << "v,0,all";
		p->second.path_variance.write(out, ns_vector_2d(0, 0), false);
		out << "\n";
		out << *(p->second.device_name) << ","
			<< *(p->second.region_name) << "," 
			<< p->first.detection_set_id << ","
			<< p->first.group_id << ","
			<< p->first.path_id << ",";
		out << "a,0,all," << p->second.source.to_string() << "\n";
	}
}

void ns_hmm_observation_set::write_durations(std::ostream& out, const std::string& experiment_name) const {
	out << "device_name,region_name,detection_set_id,group_id,path_id,data_type,time,hmm_movement_state_source,hmm_movement_state_destination,duration";
	out << "\n";
	//first write normalization stats
	out.precision(30);
	for (std::map<ns_hmm_state_transition, std::vector<ns_hmm_duration> >::const_iterator p = state_durations.begin(); p != state_durations.end(); p++) {
		for (unsigned int i = 0; i < p->second.size(); i++) {
			out << *(p->second[i].device_name) << ","
				<< *(p->second[i].region_name) << ","
				<< p->second[i].path_id.detection_set_id << ","
				<< p->second[i].path_id.group_id << ","
				<< p->second[i].path_id.path_id << ","
				<< "d,"
				<< p->second[i].emission_time << ","
				<< ns_hmm_movement_state_to_string(p->first.first) << ","
				<< ns_hmm_movement_state_to_string(p->first.second) << ","
				<< p->second[i].measurement;
			out << "\n";
		}
	}
}


ns_emperical_posture_quantification_value_estimator::ns_emperical_posture_quantification_value_estimator():emission_probability_models(new ns_probability_model_holder){}
ns_emperical_posture_quantification_value_estimator::ns_emperical_posture_quantification_value_estimator(const ns_emperical_posture_quantification_value_estimator& a) {
	emission_probability_models = new ns_probability_model_holder;
	for (auto p = a.emission_probability_models->state_emission_models.begin(); p != a.emission_probability_models->state_emission_models.end(); p++)
		emission_probability_models->state_emission_models.insert(emission_probability_models->state_emission_models.begin(), std::map<ns_hmm_movement_state, ns_hmm_probability_model<ns_measurement_accessor>*>::value_type(p->first, p->second->clone()));
	for (auto p = a.emission_probability_models->state_transition_models.begin(); p != a.emission_probability_models->state_transition_models.end(); p++)
		emission_probability_models->state_transition_models.insert(emission_probability_models->state_transition_models.begin(), std::map<ns_hmm_state_transition, ns_hmm_probability_model<ns_duration_accessor>*>::value_type(p->first, p->second->clone()));

}
ns_emperical_posture_quantification_value_estimator& ns_emperical_posture_quantification_value_estimator::operator=(const ns_emperical_posture_quantification_value_estimator& a) {
	this->emission_probability_models->state_emission_models.clear();
	this->emission_probability_models->state_transition_models.clear();
	for (auto p = a.emission_probability_models->state_emission_models.begin(); p != a.emission_probability_models->state_emission_models.end(); p++)
		emission_probability_models->state_emission_models.insert(emission_probability_models->state_emission_models.begin(), std::map<ns_hmm_movement_state, ns_hmm_probability_model<ns_measurement_accessor>*>::value_type(p->first, p->second->clone()));
	for (auto p = a.emission_probability_models->state_transition_models.begin(); p != a.emission_probability_models->state_transition_models.end(); p++)
		emission_probability_models->state_transition_models.insert(emission_probability_models->state_transition_models.begin(), std::map<ns_hmm_state_transition, ns_hmm_probability_model<ns_duration_accessor>*>::value_type(p->first, p->second->clone()));
	return *this;
}

ns_emperical_posture_quantification_value_estimator::~ns_emperical_posture_quantification_value_estimator() {
	delete emission_probability_models;
}

void ns_emperical_posture_quantification_value_estimator::read(std::istream& i) {
	std::string tmp;
	software_version_when_built = model_description_text = "";
	while (true) {
		getline(i, tmp, '\n');
		if (i.fail())
			throw ns_ex("ns_emperical_posture_quantification_value_estimator::read()::Model file on disk appears to be empty.");
		if (tmp.size() > 0 && tmp[0] == '#')
			model_description_text += tmp.substr(1) + '\n';
		else //this is the header line, which we discard.  The data starts on the next line.
			break;
	}
	ns_get_string get_string;
	std::string software_version_for_model;
	ns_hmm_emission_probability_model_organizer organizer;
	while (true) {
		ns_emission_probabiliy_gaussian_diagonal_covariance_model< ns_measurement_accessor >* model = new ns_emission_probabiliy_gaussian_diagonal_covariance_model< ns_measurement_accessor >; //first load data into a model with the maximum number of dimensions;
		try {
			std::string state_string;
			ns_64_bit data;
			if (!model->read(i, state_string, software_version_for_model, data, &organizer))
				break;
			if (software_version_when_built.empty())
				software_version_when_built = software_version_for_model;
			else if (software_version_when_built != software_version_for_model)
				throw ns_ex("Software version mismatch within model");
			update_flags_from_int(data);

			if (state_string.empty() || i.fail()) {
				if (emission_probability_models->state_emission_models.size() < 1)
					throw ns_ex("ns_emperical_posture_quantification_value_estimator()::The estimator did not contain enough data.");
			}
			bool state_not_transition = ns_string_is_a_state_not_a_transition(state_string);
			if (state_not_transition) {
				const ns_hmm_movement_state state = ns_hmm_movement_state_from_string(state_string);
				auto p2 = emission_probability_models->state_emission_models.find(state);
				if (p2 == emission_probability_models->state_emission_models.end())
					p2 = emission_probability_models->state_emission_models.insert(emission_probability_models->state_emission_models.end(),
						std::map < ns_hmm_movement_state, ns_hmm_probability_model<ns_measurement_accessor>*>::value_type(state, model));
				else throw ns_ex("Duplicae state emission model found in file: ") << state_string;
			}
			else {
				//copy gmm data into the appropriate structure
				auto trans_model = new ns_state_transition_probability_model<ns_duration_accessor>;
				try {
					trans_model->convert_from_gmm_file_format(*model);
					delete model;
					model = 0;

					ns_hmm_state_transition trans = ns_hmm_state_transition_from_string(state_string);
					auto p2 = emission_probability_models->state_transition_models.find(trans);
					if (p2 == emission_probability_models->state_transition_models.end())
						p2 = emission_probability_models->state_transition_models.insert(emission_probability_models->state_transition_models.end(),
							std::map < ns_hmm_state_transition, ns_hmm_probability_model<ns_duration_accessor>*>::value_type(trans, trans_model));
					else throw ns_ex("Duplicate state transition found in file: ") << state_string;
				}
				catch (...) {
					delete trans_model;
					throw;
				}
			}
		}
		catch (...) {
			ns_safe_delete(model);
			throw;
		}
	}
}

int ns_emperical_posture_quantification_value_estimator::flags_to_int()const {
	int a = states_permitted_int;
	int b = state_transition_type;
	return (a & 0xFF) | ((b & 0xFF) << 8);
}

void ns_emperical_posture_quantification_value_estimator::update_flags_from_int(int flag_int) {
	states_permitted_int = (ns_hmm_states_permitted)(flag_int& 0xFF);
	state_transition_type = (ns_hmm_states_transition_types)((flag_int >> 8) & 0xFF);
}

void ns_emperical_posture_quantification_value_estimator::write( std::ostream & o)const {
	if (model_description_text.size() > 0) {
		o << "#";
		for (unsigned int i = 0; i < model_description_text.size(); i++) {
			if (model_description_text[i] == '\n' && i+1 != model_description_text.size()) 
				o << "\n#";
			else o << model_description_text[i];
		}
		if (*model_description_text.rbegin() != '\n')
			o << "\n";
	}
	if (emission_probability_models->state_emission_models.size() != 0)
		emission_probability_models->state_emission_models.begin()->second->write_header(o);
	o << "\n";
	for (auto p = emission_probability_models->state_emission_models.begin(); p != emission_probability_models->state_emission_models.end(); p++) {
		p->second->write(ns_hmm_movement_state_to_string(p->first), ns_time_path_movement_markov_solver::current_software_version(),flags_to_int(),o);
		o << "\n";
	}
	for (auto p = emission_probability_models->state_transition_models.begin(); p != emission_probability_models->state_transition_models.end(); p++) {
		p->second->write(ns_hmm_state_transition_to_string(p->first), ns_time_path_movement_markov_solver::current_software_version(), flags_to_int(), o);
		o << "\n";
	}
}

void ns_hmm_observation_set::read_emissions(std::istream & in){
	std::string tmp, tmp2;
	getline(in, tmp, '\n');
	//read normalization stats
	ns_vector_2d tmp_d;
	bool tmp_b;
	while (true) {
		getline(in, tmp, ','); //device_name
		if (in.fail())
			break;
		auto device_name = volatile_string_storage.find(tmp);
		if (device_name == volatile_string_storage.end())
			device_name = volatile_string_storage.emplace(tmp).first;

		getline(in, tmp, ','); //plate name
		auto plate_name = volatile_string_storage.find(tmp);
		if (plate_name == volatile_string_storage.end())
			plate_name = volatile_string_storage.emplace(tmp).first;

		ns_stationary_path_id id;
		id.detection_set_id = ns_atoi64(tmp.c_str());
		getline(in, tmp, ',');
		id.group_id = atoi(tmp.c_str());
		getline(in, tmp, ',');
		id.path_id = atoi(tmp.c_str());

		getline(in, tmp, ',');	//what is the data type of this entry?
		if (tmp == "m" || tmp == "v" || tmp == "a") { //reading in a normalization stat
			ns_hmm_emission_normalization_stats & stats = normalization_stats[id];
			stats.device_name = &(*device_name);
			stats.region_name = &(*plate_name);

			getline(in, tmp2, ',');	//discard time column
			getline(in, tmp2, ',');	//discard hmm movement state column

			if (tmp == "m")
				stats.path_mean.read(in, tmp_d, tmp_b);
			else if (tmp == "v")
				stats.path_variance.read(in, tmp_d, tmp_b);
			else if (tmp == "a") {
				getline(in,tmp, '\n');
				stats.source.from_string(tmp);
			}
		}
		else if (tmp == "d") { //reading in a data point
			getline(in, tmp, ','); //state
			if (in.fail())
				break;
			getline(in, tmp2, ','); //discard time column
			ns_hmm_movement_state state(ns_hmm_movement_state_from_string(tmp));
			std::vector<ns_hmm_emission> & observations = obs[state];
			observations.resize(observations.size() + 1);
			ns_hmm_emission & e(*observations.rbegin());
			e.path_id = id;
			e.measurement.read(in, tmp_d, tmp_b);
			e.device_name = &(*device_name);
			e.region_name = &(*plate_name);
		}
		else throw ns_ex("Malformed file");
		if (in.fail())
			throw ns_ex("Malformed file");
	}
}

void ns_hmm_observation_set::clean_up_data_prior_to_model_fitting() {
	//if the user hasn't explicitly labeled moving weakly post expansion,
	//use the moving weakly pre expansion as a proxy.
	

		//we mix together expansion annotations with and without animal movement.
		//this removes any bias the machine has as to whether expansion happens before or after the final movement.
		auto p_expansion_moving = obs.find(ns_hmm_moving_weakly_expanding);
		auto p_expansion_not_moving = obs.find(ns_hmm_not_moving_expanding);
		if (p_expansion_moving != obs.end() && p_expansion_not_moving != obs.end()) {
			p_expansion_moving->second.insert(p_expansion_moving->second.end(), p_expansion_not_moving->second.begin(), p_expansion_not_moving->second.end());
			p_expansion_not_moving->second = p_expansion_moving->second;
		}
	
}

bool ns_emperical_posture_quantification_value_estimator::build_estimator_from_observations(const std::string & description, const ns_hmm_observation_set& observation_set, const ns_probability_model_generator* generator, 
	const ns_hmm_states_permitted& states_permitted_, const ns_hmm_states_transition_types & transition_type_, std::string& output){

	model_description_text.resize(0);
	model_description_text += description;
	model_description_text += "Generated from annotations of " + ns_to_string(observation_set.volatile_number_of_individuals_fully_annotated) + " individuals\n";

	states_permitted_int = states_permitted_;
	state_transition_type = transition_type_;
	software_version_when_built = ns_time_path_movement_markov_solver::current_software_version();

	//a list of observations for each state.
	//because we mix and match these, we need to have a vector of vectors.
	std::map<ns_hmm_movement_state, std::vector<const std::vector<ns_hmm_emission>* > > observations_sorted_by_state;
	for (auto p = observation_set.obs.begin(); p != observation_set.obs.end(); p++)
		observations_sorted_by_state[p->first].push_back(&p->second);


	bool add_pre_expansion_weakly_to_post_expansion_weakly(false);
	if (states_permitted_ != ns_no_expansion_while_alive && states_permitted_ != no_expansion_while_alive_nor_contraction) {
		auto p = observation_set.obs.find(ns_hmm_moving_weakly_post_expansion);
		if (p == observation_set.obs.end() || p->second.size() < 100) {
			add_pre_expansion_weakly_to_post_expansion_weakly = true;
		}
	}
	if (add_pre_expansion_weakly_to_post_expansion_weakly) {
		auto p = observation_set.obs.find(ns_hmm_moving_weakly);
		if (p != observation_set.obs.end())
			observations_sorted_by_state[ns_hmm_moving_weakly_post_expansion].push_back(&(p->second));
	}

	//count states and build probability models for each one as needed
	std::vector<unsigned long > state_counts((int)(ns_hmm_unknown_state), 0);
	const unsigned long minimum_number_of_observations = 100;
	for (auto q = observations_sorted_by_state.begin(); q != observations_sorted_by_state.end(); q++) {
		unsigned long& state_count = state_counts[(int)q->first];
		for (auto p = q->second.begin(); p != q->second.end(); p++) 
			state_count += (*p)->size();
		if (state_count < minimum_number_of_observations)
			continue;	//skip states with to few observations.
		auto p2 = emission_probability_models->state_emission_models.find(q->first);
		if (p2 == emission_probability_models->state_emission_models.end()) {
			p2 = emission_probability_models->state_emission_models.insert(emission_probability_models->state_emission_models.end(),
				std::map < ns_hmm_movement_state, ns_hmm_probability_model<ns_measurement_accessor>*>::value_type(q->first, 0));
			p2->second = (*generator)();
		}

		p2->second->build_from_data(q->second,observation_set.volatile_number_of_individuals_fully_annotated);
	}
	//now do state transitions
	//first we calculate the total number of transitions from one state to a different state, which we'll use to normalize the transition probabilities
	std::vector<unsigned long> state_transition_count(ns_hmm_unknown_state, 0);
	for (auto q = observation_set.state_durations.begin(); q != observation_set.state_durations.end(); q++) {
		if (q->first.first != q->first.second)
			state_transition_count[(int)q->first.first] += q->second.size();
	}

	for (auto q = observation_set.state_durations.begin(); q != observation_set.state_durations.end(); q++) {
		auto p2 = emission_probability_models->state_transition_models.find(q->first);
		if (p2 == emission_probability_models->state_transition_models.end()) {
			p2 = emission_probability_models->state_transition_models.insert(emission_probability_models->state_transition_models.end(),
				std::map < ns_hmm_state_transition, ns_hmm_probability_model<ns_duration_accessor>*>::value_type(q->first, 0));
			auto model = new ns_state_transition_probability_model<ns_duration_accessor>;
			p2->second = model;
		}
		std::vector<const std::vector<ns_hmm_duration>* > data(1);
		data[0] = &q->second;

		p2->second->build_from_data(data, state_transition_count[(int)q->first.first]);
	}
	std::vector<ns_hmm_movement_state> required_states;
	required_states.reserve(3);
	required_states.push_back(ns_hmm_missing);
	required_states.push_back(ns_hmm_not_moving_dead);
	if (states_permitted_ != ns_require_movement_expansion_synchronicity)
		required_states.push_back(ns_hmm_moving_weakly);

	ns_ex ex;
	//output+= "By hand annotation entries per HMM state:\n";
	//for (unsigned int i = 0; i < state_counts.size(); i++) 
	//	output+= ns_to_string(state_counts[i]) + "\t" + ns_hmm_movement_state_to_string((ns_hmm_movement_state)i) +"\n";
	for (unsigned int i = 0; i < state_counts.size(); i++) {
		if (state_counts[i] < minimum_number_of_observations) {
			for (unsigned int j = 0; j < required_states.size(); j++) {
				if (required_states[j] == i) {
					if (!ex.text().empty())
						ex << "\n";
					else ex << "Too few by-hand annotations were provided to fit an HMM model (type=" << (int)states_permitted_ << "). ";
					ex << "Only  " << state_counts[i] << " annotations were provided for the state " << ns_hmm_movement_state_to_string(required_states[j]) << " (" << state_counts[required_states[j]] << ")).";
					continue;
				}
			}
			//if (state_counts[i] == 0)
			//	output += "Warning: No annotations were made for state " + ns_hmm_movement_state_to_string((ns_hmm_movement_state)i) + ".  It will not be considered in the model.\n";
		}
	}

	if (states_permitted_ != ns_no_expansion_nor_contraction &&
		states_permitted_ != ns_require_movement_expansion_synchronicity &&
		state_counts[ns_hmm_not_moving_expanding] >= minimum_number_of_observations && state_counts[ns_hmm_not_moving_alive] < minimum_number_of_observations)
		throw ns_ex("In order to detect movement cessation and death-time expansion as distinct events, observations are required for the time in-between these events.  Only ") << state_counts[ns_hmm_not_moving_alive] << " observations were provided, so a model cannot be built.";
	
	//sometimes the number of by hand annotations for states allows models to be built for only a subset of states.
	//forbid unusual combinations of states, so we don't generate weird models where worms can only enter certain states that don't make sense in combination
	if (emission_probability_models->state_emission_models.find(ns_hmm_moving_weakly) == emission_probability_models->state_emission_models.end() &&
		(emission_probability_models->state_emission_models.find(ns_hmm_moving_weakly_expanding) != emission_probability_models->state_emission_models.end()
			|| emission_probability_models->state_emission_models.find(ns_hmm_moving_weakly_post_expansion) != emission_probability_models->state_emission_models.end())) {
		emission_probability_models->state_emission_models.erase(ns_hmm_moving_weakly_expanding);
		emission_probability_models->state_emission_models.erase(ns_hmm_moving_weakly_post_expansion);
		std::cout << "Excluding expansion while weakly moving states because no annotations are provided for weakly moving.\n";
	}

	if ((emission_probability_models->state_emission_models.find(ns_hmm_moving_weakly_post_expansion) != emission_probability_models->state_emission_models.end() ||
		emission_probability_models->state_emission_models.find(ns_hmm_contracting_post_expansion) != emission_probability_models->state_emission_models.end()) &&
		emission_probability_models->state_emission_models.find(ns_hmm_not_moving_expanding) == emission_probability_models->state_emission_models.end()) {
		emission_probability_models->state_emission_models.erase(ns_hmm_moving_weakly_post_expansion);
		emission_probability_models->state_emission_models.erase(ns_hmm_contracting_post_expansion);
		std::cout << "Post-expansion states are defined but expansion is not.\n";
	}

	if (observation_set.state_durations.size() == 0) {
		std::cout << "Too few state transitions were annotated\n";
		return false;
	}

	if (!ex.text().empty())
		throw ex;
	return true;
}

std::string ns_emperical_posture_quantification_value_estimator::state_permissions_to_string(const ns_hmm_states_permitted & s) {
	switch(s){
		case ns_all_states_permitted: return "all_states_allowed";
		case ns_no_post_expansion_contraction: return "no_contraction";
		case ns_no_expansion_while_alive: return "no_expansion_while_alive";
		case no_expansion_while_alive_nor_contraction: return "no_expansion_while_alive_nor_contraction";
		case ns_no_expansion_nor_contraction: return "no_expansion";
		case ns_number_of_state_settings:
		default: throw ns_ex("ns_emperical_posture_quantification_value_estimator::state_permissions_to_string()::Unknown permission: ") << (int)s;
	} 
}
ns_hmm_states_permitted ns_emperical_posture_quantification_value_estimator::state_permissions_from_string(const std::string & s) {
	for (unsigned int i = 0; i < (int)ns_number_of_state_settings; i++)
		if (s == state_permissions_to_string((ns_hmm_states_permitted)i))
			return (ns_hmm_states_permitted)i;
	throw ns_ex("Unknown state permission string: ") << s;
}


class ns_source_state_matches {
public:
	ns_source_state_matches(const ns_hmm_movement_state& state) :state_(state) {}
	bool operator()(const std::pair<ns_hmm_state_transition,double> & t) const {
		return t.first.first == state_;
	}
	ns_hmm_movement_state state_;
}; 
class ns_destination_state_matches {
public:
	ns_destination_state_matches(const ns_hmm_movement_state& state) :state_(state) {}
	bool operator()(const std::pair<ns_hmm_state_transition, double> & t) const {
		return t.first.second == state_;
	}
	ns_hmm_movement_state state_;
};
class ns_source_or_destination_state_matches {
public:
	ns_source_or_destination_state_matches(const ns_hmm_movement_state& state) :state_(state) {}
	bool operator()(const std::pair<ns_hmm_state_transition, double> & t) const {
		return t.first.first == state_ || t.first.second == state_;
	}
	ns_hmm_movement_state state_;
};

void ns_emperical_posture_quantification_value_estimator::write_visualization(std::ostream & o, const std::string & experiment_name)const{
}

bool ns_hmm_observation_set::add_observation(const ns_death_time_annotation& properties, const ns_analyzed_image_time_path* path, const std::string* database_name, const ns_64_bit& experiment_id, const std::string* plate_name, const std::string* device_name, const std::string * genotype_){
	//only consider paths with death times annotated.
	if (!path->by_hand_movement_cessation_time().fully_unbounded() &&
		!path->by_hand_death_associated_expansion_time().fully_unbounded()) {
		ns_analyzed_image_time_path_element_measurements path_mean, path_mean_square, path_variance;
		path_mean.zero();
		path_variance.zero();
		unsigned long n(0);
		for (unsigned int i = 0; i < path->element_count(); i++) {
			if (path->element(i).excluded || path->element(i).censored)
				continue;

			path_mean = path_mean + path->element(i).measurements;
			ns_analyzed_image_time_path_element_measurements a(path->element(i).measurements);
			a.square();
			if (a.total_intensity_within_region < path->element(i).measurements.total_intensity_within_region)
				throw ns_ex("ns_emperical_posture_quantification_value_estimator::add_by_hand_data_to_sample_set()::Integer Overflow!");
			path_mean_square = path_mean_square + a;
			n++;
		}
		if (n != 0) {
			path_mean = path_mean / n;
			path_mean_square = path_mean_square / n;
			ns_analyzed_image_time_path_element_measurements a(path_mean);
			a.square();
			// Var(x) = E(x^2) - (E(x))^2
			path_variance = path_mean_square - a;
		}
		unsigned long detect_contraction_state = 0;
		double min_ix4 = 0;
		int expansion_contraction_state = 0;
		bool animal_contracted = false;
		for (unsigned int i = 0; i < path->element_count(); i++) {
			if (path->element(i).excluded || path->element(i).censored)
				continue;
			if (path->by_hand_hmm_movement_state(path->element(i).absolute_time) == ns_hmm_contracting_post_expansion) {
				animal_contracted = true;
				//std::cout << "Animal contracted\n";
				break;
			}
		}
		ns_hmm_movement_state previous_state = (path->element_count() > 0) ?
			path->by_hand_hmm_movement_state(path->element(0).absolute_time) : ns_hmm_missing;
		unsigned long previous_state_start = path->element_count() > 0 ? path->element(0).absolute_time : 0;

		std::map<ns_hmm_state_transition, double > current_worm_state_durations;



		unsigned long last_valid_time(0);
		for (unsigned int i = 0; i < path->element_count(); i++) {
			if (path->element(i).excluded || path->element(i).censored)
				continue;
			ns_hmm_movement_state by_hand_movement_state(path->by_hand_hmm_movement_state(path->element(i).absolute_time));
			last_valid_time = path->element(i).absolute_time;
			//quantify how long an animal remains in each state before transitioning to another.
			if (by_hand_movement_state != previous_state) {
				current_worm_state_durations[ns_hmm_state_transition(previous_state, by_hand_movement_state)] = path->element(i).absolute_time - previous_state_start;
				previous_state_start = path->element(i).absolute_time;
				previous_state = by_hand_movement_state;
			}


			//we don't enter in as evidence measurements taken after expansion has stopped but before contraction has begun.
			if (animal_contracted) {
				if (expansion_contraction_state == 0 && (
					by_hand_movement_state == ns_hmm_moving_weakly_expanding || by_hand_movement_state == ns_hmm_not_moving_expanding)) {
					expansion_contraction_state = 1;
				}
				else if (expansion_contraction_state == 1) {
					if (by_hand_movement_state != ns_hmm_moving_weakly_expanding && by_hand_movement_state != ns_hmm_not_moving_expanding) {
						if (by_hand_movement_state == ns_hmm_contracting_post_expansion)
							expansion_contraction_state = 3;
						else {
							expansion_contraction_state = 2;
							continue;
						}
					}
				}
				else if (expansion_contraction_state == 2) {
					if (by_hand_movement_state == ns_hmm_contracting_post_expansion)
						expansion_contraction_state = 3;
					else continue;
				}

			}

			if (by_hand_movement_state == ns_hmm_unknown_state)
				throw ns_ex("Unknown state encountered!");
			/*
			//*automatically annotate death time contraction after expansion
			switch (detect_contraction_state){
			case 0:
				if (by_hand_movement_state == ns_hmm_not_moving_expanding)
					detect_contraction_state = 1;
				break;
			case 1:
				if (by_hand_movement_state == ns_hmm_not_moving_dead)
					detect_contraction_state = 2;
				break;
			case 2:
				//once the animal starts shrinking, mark contraction as possible
				if (path->element(i).measurements.change_in_total_stabilized_intensity_4x <= -100) {
					detect_contraction_state = 3;
					min_ix4 = path->element(i).measurements.change_in_total_stabilized_intensity_4x;
				}
				break;
			case 3:
				//as long as the animal's shrinking is accelerating, mark the animal as contracting
				if (path->element(i).measurements.change_in_total_stabilized_intensity_4x <= min_ix4)
					min_ix4 = path->element(i).measurements.change_in_total_stabilized_intensity_4x;

				if (path->element(i).measurements.change_in_total_stabilized_intensity_4x <= min_ix4/3) {
					by_hand_movement_state = ns_hmm_contracting_post_expansion;
				}
				else
					detect_contraction_state = 4;		//we are done

			}*/
			//if (s == ns_hmm_missing)
			//	continue;				//do not learn this state
			std::vector<ns_hmm_emission>& v(obs[by_hand_movement_state]);
			v.resize(v.size() + 1);
			ns_hmm_emission& e = *v.rbegin();
			e.measurement = path->element(i).measurements;
			e.path_id = properties.stationary_path_id;
			e.emission_time = path->element(i).absolute_time;
			e.region_name = plate_name;
			e.device_name = device_name;
			e.region_info_id = properties.region_info_id;
			e.database_name = database_name;
			e.experiment_id = experiment_id;
			e.genotype = genotype_;
			ns_hmm_emission_normalization_stats& stats = normalization_stats[e.path_id];
			stats.path_mean = path_mean;
			stats.path_variance = path_variance;
			stats.source = properties;
			stats.region_name = plate_name;
			stats.device_name = device_name;
		}
		if (last_valid_time != 0)
			current_worm_state_durations[ns_hmm_state_transition(previous_state, ns_hmm_end_of_observation)] = last_valid_time - previous_state_start;

		//identify any skipped intervals, and mark them with duration zero.
		auto moving_weakly_entrance = std::find_if(current_worm_state_durations.begin(), current_worm_state_durations.end(), ns_source_state_matches(ns_hmm_moving_weakly));
		auto not_moving_alive = std::find_if(current_worm_state_durations.begin(), current_worm_state_durations.end(), ns_source_state_matches(ns_hmm_not_moving_alive));
		auto not_moving_expanding = std::find_if(current_worm_state_durations.begin(), current_worm_state_durations.end(), ns_source_state_matches(ns_hmm_not_moving_expanding));
		auto contracting_any = std::find_if(current_worm_state_durations.begin(), current_worm_state_durations.end(), ns_source_or_destination_state_matches(ns_hmm_contracting_post_expansion));
		auto contracting_start = std::find_if(current_worm_state_durations.begin(), current_worm_state_durations.end(), ns_source_state_matches(ns_hmm_contracting_post_expansion));
		auto not_moving_dead = std::find_if(current_worm_state_durations.begin(), current_worm_state_durations.end(), ns_source_or_destination_state_matches(ns_hmm_not_moving_dead));
		auto not_found = current_worm_state_durations.end();

		if (moving_weakly_entrance == not_found) {
			if (not_moving_alive != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_moving_weakly, ns_hmm_not_moving_alive)] = 0;
			else if (not_moving_expanding != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_moving_weakly, ns_hmm_not_moving_expanding)] = 0;
			else if (contracting_any != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_moving_weakly, ns_hmm_contracting_post_expansion)] = 0;
			else if (not_moving_dead != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_moving_weakly, ns_hmm_not_moving_dead)] = 0;
		}
		if (not_moving_alive == not_found) {
			if (not_moving_expanding != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_not_moving_alive, ns_hmm_not_moving_expanding)] = 0;
			else if (contracting_any != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_not_moving_alive, ns_hmm_contracting_post_expansion)] = 0;
			else if (not_moving_dead != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_not_moving_alive, ns_hmm_not_moving_dead)] = 0;
		}
		if (not_moving_expanding != not_found && contracting_start == not_found) {
			if (not_moving_dead != not_found)
				current_worm_state_durations[ns_hmm_state_transition(ns_hmm_contracting_post_expansion, ns_hmm_not_moving_dead)] = 0;
		}

		//now we have a list of all state transitions for the current path, add them to the observation list.
		for (auto p = current_worm_state_durations.begin(); p != current_worm_state_durations.end(); p++) {
			std::vector<ns_hmm_duration>& v(state_durations[p->first]);
			v.resize(v.size() + 1);
			ns_hmm_duration& e = *v.rbegin();
			e.path_id = properties.stationary_path_id;
			e.emission_time = 0;
			e.region_name = plate_name;
			e.device_name = device_name;
			e.region_info_id = properties.region_info_id;
			e.database_name = database_name;
			e.experiment_id = experiment_id;
			e.genotype = genotype_;
			e.measurement = p->second;

			//keep a list of how long animals stayed within a state before transitioning to any other state
			std::vector<ns_hmm_duration>& self_v(state_durations[ns_hmm_state_transition(p->first.first, p->first.first)]);
			self_v.resize(self_v.size() + 1);
			ns_hmm_duration& self_e = *self_v.rbegin();
			self_e = e;

		}

		if (this->obs.size() > 1 && state_durations.size() == 0)
			std::cerr << "Empty duration!";

		volatile_number_of_individuals_fully_annotated++;
		volatile_number_of_individuals_observed++;
		return true;
	}
	volatile_number_of_individuals_observed++;
	return false;
}

bool ns_emperical_posture_quantification_value_estimator::state_specified_by_model(const ns_hmm_movement_state s) const {
	if (s == ns_hmm_end_of_observation)
		return true;
	auto p = emission_probability_models->state_emission_models.find(s);
	return p != emission_probability_models->state_emission_models.end();
}

ns_posture_analysis_model ns_posture_analysis_model::dummy(){
		ns_posture_analysis_model m;
		m.posture_analysis_method = ns_posture_analysis_model::ns_threshold;
		m.threshold_parameters.stationary_cutoff = 0;
		m.threshold_parameters.permanance_time_required_in_seconds = 0;
		m.threshold_parameters.version_flag = ns_threshold_movement_posture_analyzer::current_software_version();

		return m;
	}

ns_emperical_posture_quantification_value_estimator ns_emperical_posture_quantification_value_estimator::dummy(){
	ns_emperical_posture_quantification_value_estimator e;
	
	return e;
}


ns_threshold_movement_posture_analyzer_parameters ns_threshold_movement_posture_analyzer_parameters::default_parameters(const unsigned long experiment_duration_in_seconds){
	ns_threshold_movement_posture_analyzer_parameters p;
//	p.stationary_cutoff = 0.014375;
//	p.posture_cutoff = .2;
		
	p.posture_cutoff = 6;
	p.stationary_cutoff = 6;

	p.use_v1_movement_score = true;

	//for really short experiments (i.e. heat shock), lower the duration for which animals must be stationary 
	//before being annotated as such.
	
	if (experiment_duration_in_seconds < 5*24*60*60)
		p.permanance_time_required_in_seconds = 3*60*60;  //no permanance for really short experiments. we assume the worms are being killed rapidly

	else p.permanance_time_required_in_seconds= 4*60*60;  //half a day
	return p;
}
void ns_threshold_movement_posture_analyzer_parameters::read(std::istream & i){
	model_description_text = "";
	std::string tmp;
	while (true) {
		getline(i, tmp, '\n');
		if (i.fail())
			throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Model file on disk appears to be empty.");
		if (tmp.size() > 0 && tmp[0] == '#')
			model_description_text += tmp.substr(1) + '\n';
		else //this is the posture cutoff line, which we'll need to parse.
			break;
	}
	auto p = tmp.find(',');
	if (i.fail() || p == tmp.npos || tmp.substr(0,p) != "posture_cutoff")
		throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error.  First specification should be posture cutoff; instead it is ") << tmp.substr(0,p);
	posture_cutoff = atof(tmp.substr(p + 1).c_str());

	//getline(i,tmp,',');
	//if (tmp != "posture_cutoff" || i.fail())
	//i >> posture_cutoff;
	//getline(i,tmp,'\n');
	getline(i,tmp,',');
	if (tmp != "stationary_cutoff" || i.fail())
		throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 2");
	i >> stationary_cutoff;
	getline(i,tmp,'\n');
	getline(i,tmp,',');
	if (tmp != "hold_time_seconds" && tmp != "hold_time" || i.fail())
		throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 3");
	i >> permanance_time_required_in_seconds;
	getline(i, tmp, '\n');
	getline(i, tmp, ',');
	if (i.fail()) {
		use_v1_movement_score = true;
		return;
	}
	 if (tmp != "software_version")
		throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 4");
	 i >> tmp;
	 version_flag.resize(0);
	 version_flag.reserve(tmp.size());
	 for (unsigned int i = 0; i < tmp.size(); i++)
	   if (!isspace(tmp[i]))
	     version_flag+=tmp[i];

	 use_v1_movement_score = version_flag == "1";
	 if (use_v1_movement_score)
		 return;
	 /*getline(i, tmp, '\n');
	 getline(i, tmp, ',');
	 if (tmp != "death_time_expansion_cutoff" || i.fail())
		 throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 5");
	 i >> death_time_expansion_cutoff;
	 getline(i, tmp, '\n');
	 getline(i, tmp, ',');
	 if (tmp != "death_time_expansion_time_kernel" || i.fail())
		 throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 6");
	 i >> death_time_expansion_time_kernel_in_seconds;*/
}
void ns_threshold_movement_posture_analyzer_parameters::write(std::ostream & o)const{
	if (model_description_text.size() > 0) {
		o << "#";
		for (unsigned int i = 0; i < model_description_text.size(); i++) {
			if (model_description_text[i] == '\n')
				o << "\n#";
			else o << model_description_text[i];
		}
		if (*model_description_text.rbegin() != '\n')
			o << "\n";
	}
	o << "posture_cutoff, " << posture_cutoff << "\n"
		"stationary_cutoff, " << stationary_cutoff << "\n"
		"hold_time_seconds, " << permanance_time_required_in_seconds << "\n"
		"software_version, " << version_flag << "\n";
}





unsigned long ns_threshold_and_hmm_posture_analyzer::latest_possible_death_time(const ns_analyzed_image_time_path* path, const unsigned long last_observation_time) const {
	return  last_observation_time - model->threshold_parameters.permanance_time_required_in_seconds;
}
ns_time_path_posture_movement_solution ns_threshold_and_hmm_posture_analyzer::estimate_posture_movement_states(int software_version, const ns_analyzed_image_time_path* path, std::vector<double >& tmp_storage_1, std::vector<unsigned long >& tmp_storage_2, ns_analyzed_image_time_path* output_path, std::ostream* debug_output)const {

	ns_hmm_solver hmm_solver;
	hmm_solver.solve(*path, model->hmm_posture_estimator, tmp_storage_1, tmp_storage_2);

	ns_threshold_movement_posture_analyzer threshold_solver(model->threshold_parameters);
	ns_time_path_posture_movement_solution threshold_solution = threshold_solver(path, debug_output);
	
	ns_time_path_posture_movement_solution blended_solution =hmm_solver.movement_state_solution;
	blended_solution.dead = threshold_solution.dead;
	blended_solution.moving = threshold_solution.moving;
	blended_solution.slowing = threshold_solution.slowing;

	return blended_solution;
}
