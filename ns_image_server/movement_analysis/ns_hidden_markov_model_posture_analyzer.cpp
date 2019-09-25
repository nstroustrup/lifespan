#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_normal_distribution.h"
#include "ns_posture_analysis_models.h"
#include "ns_threshold_and_hmm_posture_analyzer.h"
#include "GMM.h"
	

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
	ns_time_path_posture_movement_solution solution;
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
	movement_state_solution.loglikelihood_of_solution = run_viterbi(path, estimator, path_indices, movement_transitions, tmp_storage_1, tmp_storage_2);
	build_movement_state_solution_from_movement_transitions(first_stationary_timepoint_index,path_indices, movement_transitions);
}

 void ns_hmm_solver::probability_of_path_solution(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const ns_time_path_posture_movement_solution & solution, ns_hmm_movement_optimization_stats_record_path & state_info, bool generate_path_info) {
	std::vector < ns_hmm_movement_state > movement_states(path.element_count(), ns_hmm_unknown_state);
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
	for (int i = 0; i < movement_states.size(); i++) {
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

	std::vector<double> emission_probabilities;
	std::vector<std::vector<double> > transition_probability;
	build_state_transition_matrix(estimator,transition_probability);
	
	//get first one to fill in unknowns

	ns_hmm_movement_state previous_state = movement_states[0];
	double cur_p = 0, log_likelihood(0);
	unsigned long cur_pi(0);
	if (generate_path_info) 
		state_info.path.resize(movement_states.size());
	for (unsigned int i = start_i; i < movement_states.size(); i++) {
		if (path.element(i).excluded || path.element(i).censored) {
			if (generate_path_info) {
				state_info.path[i].state = ns_hmm_unknown_state;
				state_info.path[i].total_probability = 0;
				state_info.path[i].sub_measurements.resize(estimator.number_of_sub_probabilities(), 0);
				state_info.path[i].sub_probabilities.resize(estimator.number_of_sub_probabilities(), 1);
			}
			continue;
		}
	
		if (movement_states[i] == ns_hmm_unknown_state)
			throw ns_ex("ns_hmm_solver::probability_of_path_solution()::encountered an unknown state");
		estimator.probability_for_each_state(path.element(i).measurements, emission_probabilities);
		cur_p = log(transition_probability[previous_state][movement_states[i]] *
			emission_probabilities[movement_states[i]]);

		log_likelihood += cur_p;
		if (generate_path_info) {
			state_info.path[i].state = movement_states[i];
			state_info.path[i].total_probability = cur_p;
		}
	
		if (generate_path_info)
			estimator.provide_measurements_and_sub_probabilities(movement_states[i], path.element(i).measurements, state_info.path[i].sub_measurements, state_info.path[i].sub_probabilities);
	
		previous_state = movement_states[i];
	}
	state_info.log_likelihood = log_likelihood;
}


//run the viterbi algorithm using the specified indicies of the path
void ns_forbid_requested_hmm_states(const ns_emperical_posture_quantification_value_estimator & e, const unsigned long offset, std::vector<char > & forbidden) {

	switch (e.states_permitted()) {
		case ns_emperical_posture_quantification_value_estimator::ns_all_states: break;
		case ns_emperical_posture_quantification_value_estimator::no_expansion_while_alive_nor_contraction:
			forbidden[offset + ns_hmm_contracting_post_expansion] = 1; //deliberate readthrough
		case ns_emperical_posture_quantification_value_estimator::ns_no_expansion_while_alive:
			forbidden[offset + ns_hmm_moving_weakly_expanding] = 1;
			forbidden[offset + ns_hmm_moving_weakly_post_expansion] = 1; break;
		case ns_emperical_posture_quantification_value_estimator::ns_no_expansion_nor_contraction:
			forbidden[offset + ns_hmm_moving_weakly_expanding] = 1;
			forbidden[offset + ns_hmm_moving_weakly_post_expansion] = 1;
			forbidden[offset + ns_hmm_not_moving_expanding] = 1;	//deliberate case readthrough
		case ns_emperical_posture_quantification_value_estimator::ns_no_post_expansion_contraction:
			forbidden[offset + ns_hmm_contracting_post_expansion] = 1; break;
	}
}

double ns_hmm_solver::run_viterbi(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const std::vector<unsigned long> path_indices,
	std::vector<ns_hmm_state_transition_time_path_index > &movement_transitions, std::vector<double > & probabilitiy_of_path, std::vector<unsigned long > & previous_state) {
	const int number_of_states((int)ns_hmm_unknown_state);
	std::vector<std::vector<double> > a;
	build_state_transition_matrix(estimator,a);

	std::set<ns_hmm_movement_state> defined_states;
	estimator.defined_states(defined_states);
	const bool allow_weakly = defined_states.find(ns_hmm_moving_weakly) != defined_states.end();

	std::vector<char > path_forbidden;

	unsigned long first_appearance_id = path.first_stationary_timepoint();

	std::vector<unsigned long>optimal_path_state(path_indices.size(), 0);
	double optimal_path_log_probability;
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
		path_forbidden.resize(nobs*mstat, 0);
		
		std::vector<double> emission_log_probabilities;
		estimator.probability_for_each_state(path.element(path_indices[0]).measurements, emission_log_probabilities);
		path_forbidden[ns_hmm_moving_weakly_post_expansion] = 1; //do not allow animals to start as moving weakly post-expansion.  They can only start weakly /pre-expansion/ if the expansion was not observed!
		path_forbidden[ns_hmm_contracting_post_expansion] = 1; //do not allow animals to start as contracting post-expansion.  They can only start weakly /pre-expansion/ if the expansion was not observed!
		if (!allow_weakly) path_forbidden[ns_hmm_moving_weakly] = 1;
		ns_forbid_requested_hmm_states(estimator, 0, path_forbidden);
		
		if (first_appearance_id == 0) //if the animal is present in the first frame, disallow "missing"
			emission_log_probabilities[ns_hmm_missing] = 0;
		long i, j, t;
		double max_p, max_prev_i;
		for (i = 0; i < mstat; i++) probabilitiy_of_path[i] = log(emission_log_probabilities[i]);
		for (t = 1; t < nobs; t++) {
		//	std::cout << t << " ";
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
			estimator.probability_for_each_state(path.element(path_indices[t]).measurements, emission_log_probabilities);

			
			//identify situations where no state produces a non-zero probability
			//skip these points, staying in the same state.
			bool nonzero_state_probability_found = false;
			for (unsigned int w = 0; w < emission_log_probabilities.size(); w++)
				if (emission_log_probabilities[w] != 0)
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
					if (a[i][j] == 0 || emission_log_probabilities[j] == 0 || path_forbidden[mstat*(t - 1) + i])
						continue;
					//calculate probability of moving from state i at time t-1 to state j now
					const double cur = probabilitiy_of_path[mstat*(t - 1)+i] + log(a[i][j] * emission_log_probabilities[j]);
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

		bool found_valid = false;
		//find the last valid frame and find the most likely final state of the path
		for (t = nobs - 1; t >= 0; t--) {
			if (path.element(t).excluded)
				continue;
			for (j = 0; j < mstat; j++) {
				if (path_forbidden[mstat*(nobs - 1) + j])
					continue;
				if (!found_valid || probabilitiy_of_path[mstat*(nobs - 1) + j] > optimal_path_log_probability) {
					optimal_path_log_probability = probabilitiy_of_path[mstat*(nobs - 1) + j];
					*optimal_path_state.rbegin() = j;
					found_valid = true;
				}
			}
			break;
		}
		if (t == 0)
			throw ns_ex("Found entirely excluded path!");
		//now work backwards and recrete the path that got us there.
		for (; t >= 1; t--) {
			if (path.element(t).excluded) {
				optimal_path_state[t - 1] = optimal_path_state[t];
				continue;
			}
			optimal_path_state[t - 1] = previous_state[mstat*t + optimal_path_state[t]];
			optimal_path_log_probability += probabilitiy_of_path[mstat*t + optimal_path_state[t]];
		}
	
		//std::cout << "\n";
	//now find transition of times between states
	if (optimal_path_state.empty())
		throw ns_ex("Empty path state!");
	//now find transition of times between states
	movement_transitions.push_back(ns_hmm_state_transition_time_path_index((ns_hmm_movement_state)optimal_path_state[0], 0));
	for (unsigned int i = 1; i < optimal_path_state.size(); i++) {
		const ns_hmm_movement_state s = (ns_hmm_movement_state)optimal_path_state[i];
		if (s != movement_transitions.rbegin()->first)
			movement_transitions.push_back(ns_hmm_state_transition_time_path_index(s, i));
	}
	if (0 && movement_transitions.size() >= 2 && movement_transitions[0].first == ns_hmm_missing && movement_transitions[1].first == ns_hmm_moving_weakly_post_expansion) {
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
			estimator.probability_for_each_state(path.element(path_indices[t]).measurements, emission_log_probabilities);
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

	return optimal_path_log_probability;
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
			if (i != 0 && movement_transitions[i-1].first != ns_hmm_missing) {
				movement_state_solution.moving.skipped = false;
				movement_state_solution.moving.end_index = path_indices[movement_transitions[i].second];
			}
			else movement_state_solution.moving.skipped = true;
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
			else movement_state_solution.slowing.skipped = true;
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
				movement_state_solution.expanding.skipped = false;
				movement_state_solution.expanding.start_index = path_indices[movement_transitions[i].second];
				movement_state_solution.expanding.end_index = *path_indices.rbegin();
				movement_state_solution.expanding.skipped = movement_state_solution.expanding.start_index == movement_state_solution.expanding.end_index;
				expanding_state = 1;
			}
			else if (movement_transitions[i].first == ns_hmm_moving_weakly_post_expansion ||
				movement_transitions[i].first == ns_hmm_contracting_post_expansion) {
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
				if (expanding_state == 0)
					throw ns_ex("Encountered a contracting animal that had not first expanded.");
				movement_state_solution.post_expansion_contracting.skipped = false;
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
		case 2: if (movement_transitions[i].first == ns_hmm_contracting_post_expansion)
			throw ns_ex("Re-entry into contraction state.");
		}
	}

	//if (movement_transitions.size() == 1)
	//	std::cerr << "Singleton transition encountered\n";
}

//m[i][j] is the log probabilitiy that an individual in state i transitions to state j.
void ns_hmm_solver::build_state_transition_matrix(const ns_emperical_posture_quantification_value_estimator & estimator, std::vector<std::vector<double> > & m) {
	m.resize((int)ns_hmm_unknown_state);

	const double penalized_transition = 1e-5;

	for (unsigned int i = 0; i < (int)ns_hmm_unknown_state; i++) {
		m[i].resize(0);
		m[i].resize((int)ns_hmm_unknown_state, 0);
		m[i][i] = 1000;	//make staying in the same state more probable than switching, to discourage short stays in each state
	}

	std::set<ns_hmm_movement_state> defined_states;
	estimator.defined_states(defined_states);
	const bool allow_weakly = defined_states.find(ns_hmm_moving_weakly) != defined_states.end();

	const bool allow_expansion = estimator.states_permitted() != ns_emperical_posture_quantification_value_estimator::ns_no_expansion_nor_contraction;
	const bool all_expansion_while_alive = (estimator.states_permitted() != ns_emperical_posture_quantification_value_estimator::ns_no_expansion_while_alive &&
										 allow_expansion && allow_weakly);
	const bool allow_contraction = allow_expansion && estimator.states_permitted() != ns_emperical_posture_quantification_value_estimator::ns_no_post_expansion_contraction;



	//if there are any loops anywhere here, the approach will not function
	//because we set all the transition probabilities equal and this works
	//only because the all state transitions are irreversable.
	m[ns_hmm_missing][ns_hmm_moving_vigorously] = 1;
	if (allow_weakly) m[ns_hmm_missing][ns_hmm_moving_weakly] = 1;
	if (all_expansion_while_alive) m[ns_hmm_missing][ns_hmm_moving_weakly_expanding] = 1;
	if (allow_expansion) {
		m[ns_hmm_missing][ns_hmm_not_moving_alive] = 1;
		m[ns_hmm_missing][ns_hmm_not_moving_expanding] = 1;
	}
	m[ns_hmm_missing][ns_hmm_not_moving_dead] = penalized_transition;	//we penalize any path that skips death time expansion
	if (allow_weakly) m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly] = 1;
	if (all_expansion_while_alive) m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly_expanding] = 1;

	if (allow_expansion) {
		m[ns_hmm_moving_vigorously][ns_hmm_not_moving_alive] = 1;
		m[ns_hmm_moving_vigorously][ns_hmm_not_moving_expanding] = 1;
	}
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_dead] = penalized_transition;

	if (all_expansion_while_alive) m[ns_hmm_moving_weakly][ns_hmm_moving_weakly_expanding] = 1;
	if (allow_expansion && allow_weakly) {
		m[ns_hmm_moving_weakly][ns_hmm_not_moving_alive] = 1;
		m[ns_hmm_moving_weakly][ns_hmm_not_moving_expanding] = 1;
	}
	if (allow_weakly)
	m[ns_hmm_moving_weakly][ns_hmm_not_moving_dead] = allow_expansion? penalized_transition : 1;

	if (all_expansion_while_alive) {
		m[ns_hmm_moving_weakly_expanding][ns_hmm_moving_weakly_post_expansion] = 1;
		m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_expanding] = 1;
		if (allow_contraction)
		m[ns_hmm_moving_weakly_expanding][ns_hmm_contracting_post_expansion] = 1;
		m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_dead] = 1;

		m[ns_hmm_moving_weakly_post_expansion][ns_hmm_not_moving_dead] = 1;
	}

	if (allow_expansion) {
		m[ns_hmm_not_moving_expanding][ns_hmm_not_moving_dead] = 1;
		m[ns_hmm_not_moving_expanding][ns_hmm_contracting_post_expansion] = 1;
		m[ns_hmm_not_moving_expanding][ns_hmm_not_moving_dead] = 1;
		if (allow_contraction)
		m[ns_hmm_contracting_post_expansion][ns_hmm_not_moving_dead] = 1;
		m[ns_hmm_not_moving_alive][ns_hmm_not_moving_expanding] = 1;
		m[ns_hmm_not_moving_alive][ns_hmm_not_moving_dead] = penalized_transition;
	}



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


ns_time_path_posture_movement_solution ns_time_path_movement_markov_solver::estimate_posture_movement_states(int software_version,const ns_analyzed_image_time_path * path, std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2, ns_analyzed_image_time_path * output_path, std::ostream * debug_output)const{

	ns_hmm_solver solver;
	solver.solve(*path, estimator, tmp_storage_1, tmp_storage_2);

	return solver.movement_state_solution;
}

struct ns_gmm_sorter{
	ns_gmm_sorter() {}
	ns_gmm_sorter(double w, double m, double v) :weight(w), mean(m), var(v) {}
	double weight, mean, var;
};
bool operator<(const ns_gmm_sorter & a, const ns_gmm_sorter & b) {
	return a.weight < b.weight;
}
template<class accessor_t>
class ns_emission_probabiliy_sub_model {
public:
	ns_emission_probabiliy_sub_model(): gmm(1, 3) {}
	template<class data_accessor_t>
	void build_from_data(const std::vector<ns_hmm_emission> & observations) {
		data_accessor_t data_accessor;
		
		unsigned long number_of_non_zeros(0);
		double * data = new double[observations.size()];
		for (unsigned long i = 0; i < observations.size(); i++) {
			const auto v = data_accessor(observations[i]);
			if (!data_accessor.is_zero(observations[i])) {
				data[number_of_non_zeros] = v;
				number_of_non_zeros++;
			}
		}
		
		zero_probability = 1.0 - (number_of_non_zeros / (double)observations.size());
		if (number_of_non_zeros < 3) {
			gmm_weights[0] = 1;
			gmm_weights[1] = 0;
			gmm_weights[2] = 0;
			for (unsigned int i = 0; i < 3; i++) {
				gmm_means[i] = 0;
				gmm_var[i] = 1;
			}
			if (number_of_non_zeros > 1) {
				for (unsigned int i = 0; i < number_of_non_zeros; i++)
					gmm_means[0] += data[i];
				gmm_means[0] /= number_of_non_zeros;
			}
			return;
		}
		
		double start_weights[3] = { 1/3.0,1 / 3.0,1 / 3.0 };
		double start_means[3] = { -1,0,1 };
		double start_variance[3] = { 1,1,1 };
		gmm.Train(data, number_of_non_zeros);
		double sum_of_weights = 0;

		//we sort in order of weights, so it's easy to visualize the output of the model
		std::vector< ns_gmm_sorter> sorted(3);
		for (unsigned int i = 0; i < 3; i++)
			sorted[i] = ns_gmm_sorter(gmm.Prior(i), *gmm.Mean(i), *gmm.Variance(i));
		std::sort(sorted.begin(), sorted.end());

		for (unsigned int i = 0; i < 3; i++) {
			gmm_weights[i] = sorted[i].weight;
			gmm_means[i] = sorted[i].mean;
			gmm_var[i] = sorted[i].var;
			sum_of_weights += gmm_weights[i];
		}

		if (abs(sum_of_weights - 1) > 0.01)
			throw ns_ex("GMM problem");
	}
	void flip_model_sign() {
		for (unsigned int i = 0; i < 3; i++) 
			gmm_means[i] = -gmm_means[i];
	}
	double point_emission_probability(const ns_analyzed_image_time_path_element_measurements & e) const {
		accessor_t accessor;
		const double val = accessor(e);
		const bool is_zero(accessor.is_zero(e));
		if (is_zero) 
			return zero_probability;
		return (1 - zero_probability)*gmm.GetProbability(&val);
	}
	static void write_header(std::ostream & o)  {
		o << "P(0)";
		for (unsigned int i = 0; i < 3; i++) {
			o << ",Weight " << i << ", Mean " << i << ", Var " << i;
		}

	}
	void write(std::ostream & o) const {		
		o << zero_probability ;
		for (unsigned int i = 0; i < 3; i++)
			o << "," << gmm_weights[i] << "," << gmm_means[i] << "," << gmm_var[i];
	}
	void read(std::istream & in) {
		ns_get_double get_double;
		get_double(in, zero_probability);
		if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
		for (unsigned int i = 0; i < 3; i++) {
			get_double(in, gmm_weights[i]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
			get_double(in, gmm_means[i]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
			get_double(in, gmm_var[i]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
		}
		for (unsigned int i = 0; i < 3; i++) {
			gmm.setPrior(i,gmm_weights[i]);
			gmm.setMean(i, &gmm_means[i]);
			gmm.setVariance(i, &gmm_var[i]);

		}
		//std::cout << "Just read: ";
		//write(std::cout);
		//std::cout << "\n";
	}
private:

	GMM gmm;
	double zero_probability;
	double gmm_weights[3],
		gmm_means[3],
		gmm_var[3];
};
struct ns_intensity_accessor_1x {
	double operator()(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity_1x;
	}	
	bool is_zero(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity_1x == 0;
	}
}; 
struct ns_intensity_emission_accessor_1x {
	double operator()(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity_1x;
	}
	bool is_zero(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity_1x == 0;
	}
}; 
struct ns_intensity_accessor_2x {
	double operator()(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity_2x;
	}
	bool is_zero(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity_2x == 0;
	}
};
struct ns_intensity_emission_accessor_2x {
	double operator()(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity_2x;
	}
	bool is_zero(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity_2x == 0;
	}
};
struct ns_intensity_accessor_4x {
	double operator()(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity_4x;
	}
	bool is_zero(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity_4x == 0;
	}
};
struct ns_intensity_emission_accessor_4x {
	double operator()(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity_4x;
	}
	bool is_zero(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity_4x == 0;
	}
};
struct ns_movement_accessor {
	double operator()(const ns_analyzed_image_time_path_element_measurements & e) const {
		double d = e.death_time_posture_analysis_measure_v2_cropped()+1;
		return log(d);
	}
	bool is_zero(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.death_time_posture_analysis_measure_v2_cropped() <= 0;
	}
};
struct ns_movement_emission_accessor {
	double operator()(const ns_hmm_emission & e) const {
		double d = e.measurement.death_time_posture_analysis_measure_v2_cropped();
		if (d <= 0) return -DBL_MAX;
		else return log(d);
	}
	bool is_zero(const ns_hmm_emission & e) const {
		return e.measurement.death_time_posture_analysis_measure_v2_cropped() <= 0;
	}
};
class ns_emission_probabiliy_model{
public:

	void build_from_data(const std::vector<ns_hmm_emission> & observations) {
		movement.build_from_data<ns_movement_emission_accessor>(observations);
		intensity_1x.build_from_data< ns_intensity_emission_accessor_1x>(observations);
		intensity_2x.build_from_data< ns_intensity_emission_accessor_2x>(observations);
		intensity_4x.build_from_data< ns_intensity_emission_accessor_4x>(observations);
	}
	double point_emission_probability(const ns_analyzed_image_time_path_element_measurements & e) const {
		return movement.point_emission_probability(e) * pow(intensity_1x.point_emission_probability(e)
														*intensity_2x.point_emission_probability(e)
														*intensity_4x.point_emission_probability(e),.33333333333);
	}
	void sub_probabilities(const ns_analyzed_image_time_path_element_measurements & m,std::vector<double> & measurements, std::vector<double> & probabilities) const {
		measurements.resize(0);
		probabilities.resize(0);
		ns_movement_accessor ma;
		ns_intensity_accessor_1x i1;
		ns_intensity_accessor_2x i2;
		ns_intensity_accessor_4x i4;

		measurements.reserve(5);
		measurements.push_back(ma.is_zero(m));
		measurements.push_back(ma(m));
		if (!std::isfinite(*measurements.rbegin()) || *measurements.rbegin() < -1e300)
			std::cerr << "Yikes";
		measurements.push_back(i1(m));
		measurements.push_back(i2(m));
		measurements.push_back(i4(m));

		probabilities.reserve(5);
		probabilities.push_back(0);
		probabilities.push_back(movement.point_emission_probability(m));
		probabilities.push_back(intensity_1x.point_emission_probability(m));
		probabilities.push_back(intensity_2x.point_emission_probability(m));
		probabilities.push_back(intensity_4x.point_emission_probability(m));

	}
	void sub_probability_names(std::vector<std::string> & names) const {
		names.resize(0);
		names.reserve(5);
		names.push_back("m0");
		names.push_back("m");
		names.push_back("i1");
		names.push_back("i2");
		names.push_back("i4");
	}
	unsigned long number_of_sub_probabilities() const {
		return 5;
	}
	static void write_header(std::ostream & o)  {
		o << "Permissions,Movement State,Variable,";
		ns_emission_probabiliy_sub_model<ns_movement_accessor>::write_header(o);
	}
	void write(const ns_hmm_movement_state state,int extra_data,std::ostream & o) const {
		o << extra_data << "," << ns_hmm_movement_state_to_string(state) << ",m,";
		movement.write(o);
		o << "\n" << extra_data << "," << ns_hmm_movement_state_to_string(state) << ",i1,";
		intensity_1x.write(o);
		o << "\n" << extra_data << "," << ns_hmm_movement_state_to_string(state) << ",i2,";
		intensity_2x.write(o);
		o << "\n" << extra_data << "," << ns_hmm_movement_state_to_string(state) << ",i4,";
		intensity_4x.write(o);
	}
	void read(std::istream & i,ns_hmm_movement_state & state, int & extra_data) {

		ns_get_string get_string;
		std::string tmp;
		int r = 0;
		while (!i.fail()) {
			get_string(i, tmp);
			if (i.fail()) {
				if (r == 0) {
					state = ns_hmm_unknown_state;
					extra_data = 0;
					return;
				}
				else
					throw ns_ex("ns_emission_probabiliy_model::read()::Bad model file");
			}
			extra_data = atoi(tmp.c_str());
			get_string(i, tmp);
			ns_hmm_movement_state state_temp = ns_hmm_movement_state_from_string(tmp);

			if (r != 0 && state_temp != state)
				throw ns_ex("ns_emission_probabiliy_model::read()::Mixed up order of emission probability model!");
			state = state_temp;
			get_string(i, tmp);
			if (i.fail())
				throw ns_ex("ns_emission_probabiliy_model::read()::Bad model file");
			//std::cerr << "movement/intensity:" << tmp << " ";
			if (tmp == "m")
				movement.read(i);
			else if (tmp == "i1")
				intensity_1x.read(i);
			else if (tmp == "i2")
				intensity_2x.read(i);
			else if (tmp == "i4")
				intensity_4x.read(i);
			r++;
			if (r == 4)
				break;
		}
	}
	ns_emission_probabiliy_sub_model<ns_movement_accessor> movement;
	ns_emission_probabiliy_sub_model<ns_intensity_accessor_1x> intensity_1x;
	ns_emission_probabiliy_sub_model<ns_intensity_accessor_2x> intensity_2x;
	ns_emission_probabiliy_sub_model<ns_intensity_accessor_4x> intensity_4x;


};


void ns_emperical_posture_quantification_value_estimator::provide_measurements_and_sub_probabilities(const ns_hmm_movement_state & state, const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & measurement, std::vector<double> & sub_probabilitiy) const {
	bool undefined_state(false);
	auto p = emission_probability_models.find(state);
	
	if (p == emission_probability_models.end()) {
		//if we are debugging a state for which the emission model isn't trained, output N/A.
		if (emission_probability_models.empty())
			throw ns_ex("No emission models!");
		p = emission_probability_models.begin();
		undefined_state = true;
	}
	p->second->sub_probabilities(e, measurement, sub_probabilitiy);


	//if we are debugging a state for which the emission model isn't trained, output N/A.
	if (undefined_state) {
		for (unsigned int i = 0; i < sub_probabilitiy.size(); i++)
			sub_probabilitiy[i] = -1;
	}
}
void ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names(std::vector<std::string> & names) const {
	if (emission_probability_models.empty())
		throw ns_ex("ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names()::Cannot find any probability model");
	emission_probability_models.begin()->second->sub_probability_names(names);
}
unsigned long ns_emperical_posture_quantification_value_estimator::number_of_sub_probabilities() const {
	if (emission_probability_models.empty())
		throw ns_ex("ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names()::Cannot find any probability model");
	return emission_probability_models.begin()->second->number_of_sub_probabilities();
}

bool ns_emperical_posture_quantification_value_estimator::state_defined(const ns_hmm_movement_state & m) const {
	return emission_probability_models.find(m) != emission_probability_models.end();

}

void ns_emperical_posture_quantification_value_estimator::probability_for_each_state(const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & d) const {
	d.resize(0);
	d.resize((int)ns_hmm_unknown_state,0);
	for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); p++) {
		const double tmp(p->second->point_emission_probability(e));
		d[p->first] = tmp;
	}
}
void ns_emperical_posture_quantification_value_estimator::output_debug_info(const ns_analyzed_image_time_path_element_measurements & e, std::ostream & o) const {

	

	for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); p++) {
		std::vector<std::string> names;
		std::vector<double> measurements;
		std::vector<double> probabilities;
		p->second->sub_probability_names(names);
		p->second->sub_probabilities(e, measurements, probabilities);
		for (unsigned int i = 0; i < names.size(); i++) {
			o << p->first << ": " << names[i] << ": " << measurements[i] << ": " << probabilities[i] << "\n";
		}
	}
}
	

void ns_emperical_posture_quantification_value_estimator::write_observation_data(std::ostream & out, const std::string & experiment_name) const {
	out << "region_id,group_id,path_id,data_type,time,hmm_movement_state,";
	ns_analyzed_image_time_path_element_measurements::write_header(out);
	out << "\n";
	//first write normalization stats
	
	for (std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> >::const_iterator p = observed_values.begin(); p != observed_values.end(); p++) {
		for (unsigned int i = 0; i < p->second.size(); i++) {
			out << p->second[i].path_id.detection_set_id << ","
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
		out << p->first.detection_set_id << ","
			<< p->first.group_id << ","
			<< p->first.path_id << ",";
		out << "m,";
		p->second.path_mean.write(out, ns_vector_2d(0, 0), false);
		out << "\n";
		out << p->first.detection_set_id << ","
			<< p->first.group_id << ","
			<< p->first.path_id << ",";
		out << "v,";
		p->second.path_variance.write(out, ns_vector_2d(0, 0), false);
		out << "\n";
		out << p->first.detection_set_id << ","
			<< p->first.group_id << ","
			<< p->first.path_id << ",";
		out << "a," << p->second.source.to_string() << "\n";
	}
}


ns_emperical_posture_quantification_value_estimator::ns_emperical_posture_quantification_value_estimator() {}
ns_emperical_posture_quantification_value_estimator::ns_emperical_posture_quantification_value_estimator(const ns_emperical_posture_quantification_value_estimator& a) {
	normalization_stats = a.normalization_stats;
	observed_values = a.observed_values;
	for (auto p = a.emission_probability_models.begin(); p != a.emission_probability_models.end(); p++)
		emission_probability_models.insert(emission_probability_models.begin(), std::map<ns_hmm_movement_state, ns_emission_probabiliy_model *>::value_type(p->first, new ns_emission_probabiliy_model(*p->second)));

}
ns_emperical_posture_quantification_value_estimator& ns_emperical_posture_quantification_value_estimator::operator=(const ns_emperical_posture_quantification_value_estimator& a) {
	normalization_stats = a.normalization_stats;
	observed_values = a.observed_values;
	for (auto p = a.emission_probability_models.begin(); p != a.emission_probability_models.end(); p++)
		emission_probability_models.insert(emission_probability_models.begin(), std::map<ns_hmm_movement_state, ns_emission_probabiliy_model *>::value_type(p->first, new ns_emission_probabiliy_model(*p->second)));
	return *this;
}

ns_emperical_posture_quantification_value_estimator::~ns_emperical_posture_quantification_value_estimator() {
	for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); ++p)
		delete p->second;
	emission_probability_models.clear();
}
void ns_emperical_posture_quantification_value_estimator::read(std::istream & i) {
	std::string tmp;
	getline(i, tmp, '\n');
	ns_get_string get_string;
	while (true) {
		ns_emission_probabiliy_model * model = new ns_emission_probabiliy_model;
		try {
			ns_hmm_movement_state state;
			int data;
			model->read(i, state,data);
			states_permitted_int = (ns_states_permitted)data;

			if (state == ns_hmm_unknown_state || i.fail()) {
				if (emission_probability_models.size() < 2)
					throw ns_ex("ns_emperical_posture_quantification_value_estimator()::The estimator did not contain enough data.");
				delete model; 
				return;
			}

			auto p2 = emission_probability_models.find(state);
			if (p2 == emission_probability_models.end())
				p2 = emission_probability_models.insert(emission_probability_models.end(),
					std::map < ns_hmm_movement_state, ns_emission_probabiliy_model *>::value_type(state, model));
		}
		catch (...) {
			delete model;
			throw;
		}
	}
}
void ns_emperical_posture_quantification_value_estimator::write(std::ostream & o)const {
	ns_emission_probabiliy_model::write_header(o);
	o << "\n";
	for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); p++) {
		p->second->write(p->first, (int)states_permitted_int,o);
		o << "\n";
	}
}

void ns_emperical_posture_quantification_value_estimator::read_observation_data(std::istream & in){
	std::string tmp;
	getline(in, tmp, '\n');
	//read normalization stats
	ns_vector_2d tmp_d;
	bool tmp_b;
	while (true) {
		getline(in, tmp, ',');
		if (in.fail())
			break;
		ns_stationary_path_id id;
		id.detection_set_id = ns_atoi64(tmp.c_str());
		getline(in, tmp, ',');
		id.group_id = atoi(tmp.c_str());
		getline(in, tmp, ',');
		id.path_id = atoi(tmp.c_str());

		getline(in, tmp, ',');
		if (tmp == "m" || tmp == "v" || tmp == "a") { //reading in a normalization stat
			ns_hmm_emission_normalization_stats & stats = normalization_stats[id];
			
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
			getline(in, tmp, ',');
			
			ns_hmm_movement_state state(ns_hmm_movement_state_from_string(tmp));
			std::vector<ns_hmm_emission> & observations = observed_values[state];
			observations.resize(observations.size() + 1);
			ns_hmm_emission & e(*observations.rbegin());
			e.path_id = id;
			e.measurement.read(in, tmp_d, tmp_b);
		}
		else throw ns_ex("Malformed file");
		if (in.fail())
			throw ns_ex("Malformed file");
	}
}

void ns_emperical_posture_quantification_value_estimator::build_estimator_from_observations(std::string & output,const ns_states_permitted & states_permitted_) {
	states_permitted_int = states_permitted_;
	//if the user hasn't explicitly labeled moving weakly post expansion,
	//use the moving weakly pre expansion as a proxy.
	if (observed_values.find(ns_hmm_moving_weakly_post_expansion) == observed_values.end()) {
		observed_values[ns_hmm_moving_weakly_post_expansion] = observed_values[ns_hmm_moving_weakly];
	}
	else{
		auto p = observed_values.find(ns_hmm_moving_weakly_post_expansion);
		if (p->second.size() < 100) 
			p->second.insert(p->second.end(), observed_values[ns_hmm_moving_weakly].begin(),observed_values[ns_hmm_moving_weakly].end());

	}
	//we mix together expansion annotations with and without animal movement.
	//this removes any bias the machine has as to whether expansion happens before or after the final movement.
	auto p_expansion_moving = observed_values.find(ns_hmm_moving_weakly_expanding);
	auto p_expansion_not_moving = observed_values.find(ns_hmm_not_moving_expanding);
	if (p_expansion_moving != observed_values.end() && p_expansion_not_moving != observed_values.end()) {
		p_expansion_moving->second.insert(p_expansion_moving->second.end(), p_expansion_not_moving->second.begin(), p_expansion_not_moving->second.end());
		p_expansion_not_moving->second = p_expansion_moving->second;
	}

	std::vector<unsigned long > state_counts((int)(ns_hmm_unknown_state), 0);
	for (auto p = observed_values.begin(); p != observed_values.end(); p++) {
		if (p->second.size() < 100) {
			//output+= "Very few annotations were made for the state " + ns_hmm_movement_state_to_string(p->first) + ".  It will not be considered in the model.\n";
			continue;
		}
		auto p2 = emission_probability_models.find(p->first);
		if (p2 == emission_probability_models.end())
			p2 = emission_probability_models.insert(emission_probability_models.end(),
				std::map < ns_hmm_movement_state, ns_emission_probabiliy_model *>::value_type(p->first, 0));
		p2->second = new ns_emission_probabiliy_model;
		p2->second->build_from_data(p->second);
		state_counts[(int)p->first] = p->second.size();
	}

	//special case to detect contraction after expansion.
	//we don't have by hand annotations for this, so we just flip the sign of the by hand annotated expansion states!
	/*auto expanding_data = emission_probability_models.find(ns_hmm_not_moving_expanding);
	state_counts[ns_hmm_contracting_post_expansion] = state_counts[ns_hmm_not_moving_expanding];
	if (expanding_data == emission_probability_models.end()) {
		expanding_data == emission_probability_models.find(ns_hmm_moving_weakly_expanding);
		state_counts[ns_hmm_contracting_post_expansion] = state_counts[ns_hmm_moving_weakly_expanding];
	}*/
	/*if (expanding_data != emission_probability_models.end()) {
		auto contracting_data = emission_probability_models.insert(emission_probability_models.begin(), std::pair< ns_hmm_movement_state, ns_emission_probabiliy_model *>(ns_hmm_contracting_post_expansion, 0));
		(contracting_data->second) = new ns_emission_probabiliy_model;
		*contracting_data->second = *expanding_data->second;
		contracting_data->second->intensity_1x.flip_model_sign();
		contracting_data->second->intensity_2x.flip_model_sign();
		contracting_data->second->intensity_4x.flip_model_sign();
	}*/

	ns_hmm_movement_state required_states[2] = { ns_hmm_missing, ns_hmm_not_moving_dead };
	ns_ex ex;
	//output+= "By hand annotation entries per HMM state:\n";
	//for (unsigned int i = 0; i < state_counts.size(); i++) 
	//	output+= ns_to_string(state_counts[i]) + "\t" + ns_hmm_movement_state_to_string((ns_hmm_movement_state)i) +"\n";
	for (unsigned int i = 0; i < state_counts.size(); i++) {
		if (state_counts[i] < 100) {

			for (unsigned int j = 0; j < 3; j++) {
				if (required_states[j] == i) {
					if (!ex.text().empty())
						ex << "\n";
					else ex << "Too few by-hand annotations were provided to fit an HMM model.\n";
					ex << "Only  " << state_counts[i] << " annotations were provided for the state " << ns_hmm_movement_state_to_string(required_states[j]) << " (" << state_counts[required_states[j]] << ")).";
					continue;
				}
			}
			//if (state_counts[i] == 0)
			//	output += "Warning: No annotations were made for state " + ns_hmm_movement_state_to_string((ns_hmm_movement_state)i) + ".  It will not be considered in the model.\n";
		}
	}

	if (!ex.text().empty())
		throw ex;
}

std::string ns_emperical_posture_quantification_value_estimator::state_permissions_to_string(const ns_states_permitted & s) {
	switch(s){
		case ns_all_states: return "all_states_allowed";
		case ns_no_post_expansion_contraction: return "no_contraction";
		case ns_no_expansion_while_alive: return "no_expansion_while_alive";
		case no_expansion_while_alive_nor_contraction: return "no_expansion_while_alive_nor_contraction";
		case ns_no_expansion_nor_contraction: return "no_expansion";
		case ns_number_of_state_settings:
		default: throw ns_ex("ns_emperical_posture_quantification_value_estimator::state_permissions_to_string()::Unknown permission: ") << (int)s;
	} 
}
ns_emperical_posture_quantification_value_estimator::ns_states_permitted ns_emperical_posture_quantification_value_estimator::state_permissions_from_string(const std::string & s) {
	for (unsigned int i = 0; i < (int)ns_number_of_state_settings; i++)
		if (s == state_permissions_to_string((ns_states_permitted)i))
			return (ns_states_permitted)i;
	throw ns_ex("Unknown state permission string: ") << s;
}


void ns_emperical_posture_quantification_value_estimator::write_visualization(std::ostream & o, const std::string & experiment_name)const{
}

bool ns_emperical_posture_quantification_value_estimator::add_observation(const std::string &software_version, const ns_death_time_annotation & properties,const ns_analyzed_image_time_path * path, const unsigned long device_id){
	//only consider paths with death times annotated.
	if (!path->by_hand_death_time().fully_unbounded()){
		ns_analyzed_image_time_path_element_measurements path_mean, path_mean_square,path_variance;
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
			if (path->by_hand_hmm_movement_state(path->element(i).absolute_time, *this) == ns_hmm_contracting_post_expansion) {
				animal_contracted = true;
				//std::cout << "Animal contracted\n";
				break;
			}
		}

		for (unsigned int i = 0; i < path->element_count(); i++) {
			if (path->element(i).excluded || path->element(i).censored)
				continue;
			ns_hmm_movement_state by_hand_movement_state(path->by_hand_hmm_movement_state(path->element(i).absolute_time, *this));

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
			std::vector<ns_hmm_emission> & v(observed_values[by_hand_movement_state]);
			v.resize(v.size() + 1);
			ns_hmm_emission & e = *v.rbegin();
			e.measurement = path->element(i).measurements;
			e.path_id = properties.stationary_path_id;
			e.emission_time = path->element(i).absolute_time;
			e.device_id = device_id;
			e.region_id = properties.region_info_id;
			ns_hmm_emission_normalization_stats & stats = normalization_stats[e.path_id];
			stats.path_mean = path_mean;
			stats.path_variance = path_variance;
			stats.source = properties;
		}
		return true;
	}
	return false;
}

bool ns_emperical_posture_quantification_value_estimator::state_specified_by_model(const ns_hmm_movement_state s) const {
	auto p = emission_probability_models.find(s);
	return p != emission_probability_models.end();
}

ns_posture_analysis_model ns_posture_analysis_model::dummy(){
		ns_posture_analysis_model m;
		m.posture_analysis_method = ns_posture_analysis_model::ns_hidden_markov;
		m.hmm_posture_estimator = ns_emperical_posture_quantification_value_estimator::dummy();
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
	std::string tmp;
	getline(i,tmp,',');
	if (tmp != "posture_cutoff" || i.fail())
		throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 1");
	i >> posture_cutoff;
	getline(i,tmp,'\n');
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
	 getline(i, tmp, '\n');
	 getline(i, tmp, ',');
	 if (tmp != "death_time_expansion_cutoff" || i.fail())
		 throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 5");
	 i >> death_time_expansion_cutoff;
	 getline(i, tmp, '\n');
	 getline(i, tmp, ',');
	 if (tmp != "death_time_expansion_time_kernel" || i.fail())
		 throw ns_ex("ns_threshold_movement_posture_analyzer_parameters::read()::Syntax error 6");
	 i >> death_time_expansion_time_kernel_in_seconds;
}
void ns_threshold_movement_posture_analyzer_parameters::write(std::ostream & o)const{
	o << "posture_cutoff, " << posture_cutoff << "\n"
		"stationary_cutoff, " << stationary_cutoff << "\n"
		"hold_time_seconds, " << permanance_time_required_in_seconds << "\n"
		"software_version, " <<  version_flag << "\n"
		"death_time_expansion_cutoff, " << death_time_expansion_cutoff << "\n"
		"death_time_expansion_time_kernel, " << death_time_expansion_time_kernel_in_seconds << "\n";
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