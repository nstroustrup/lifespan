#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_normal_distribution.h"
#include "ns_posture_analysis_models.h"
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


void ns_hmm_solver::solve(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator) {
	ns_time_path_posture_movement_solution solution;
	bool found_start_time(false);
	unsigned long start_time_i(0);
	std::vector<unsigned long> path_indices;
	for (unsigned int i = start_time_i; i < path.element_count(); i++) {
		if (!path.element(i).excluded && !path.element(i).censored) {
			if (!found_start_time) {
				start_time_i = i;
				found_start_time = true;
			}
			path_indices.push_back(i);
		}
	}
	std::vector<ns_hmm_state_transition_time_path_index > movement_transitions;
	movement_state_solution.loglikelihood_of_solution = run_viterbi(path, estimator, path_indices, movement_transitions);
	build_movement_state_solution_from_movement_transitions(path_indices, movement_transitions);
}

 void ns_hmm_solver::probability_of_path_solution(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const ns_time_path_posture_movement_solution & solution, ns_hmm_movement_optimization_stats_record_path & state_info) {
	std::vector < ns_hmm_movement_state > movement_states(path.element_count(), ns_hmm_unknown_state);
	if (!solution.moving.skipped) {
		for (unsigned int i = 0; i <= solution.moving.start_index; i++)
			movement_states[i] = ns_hmm_missing;
		if (estimator.state_defined(ns_hmm_moving_vigorously)) {
			for (unsigned int i = solution.moving.start_index; i <= solution.moving.end_index; i++)
				movement_states[i] = ns_hmm_moving_vigorously;
		}
		else {
			for (unsigned int i = solution.moving.start_index; i <= solution.moving.end_index; i++)
				movement_states[i] = ns_hmm_moving_weakly;
		}
	}
	if (!solution.slowing.skipped) {
		if (solution.moving.skipped) {
			for (unsigned int i = 0; i <= solution.slowing.start_index; i++)
				movement_states[i] = ns_hmm_missing;
		}
		for (unsigned int i = solution.slowing.start_index; i <= solution.slowing.end_index; i++)
			if (!solution.expanding.skipped && i >= solution.expanding.start_index && i <= solution.expanding.end_index)
				movement_states[i] = ns_hmm_moving_weakly_expanding;
			else if (!solution.expanding.skipped && i > solution.expanding.end_index)
				movement_states[i] = ns_hmm_moving_weakly_post_expansion;
			else movement_states[i] = ns_hmm_moving_weakly;
	}
	if (!solution.dead.skipped) {
		if (solution.moving.skipped && solution.slowing.skipped) {
			for (unsigned int i = 0; i <= solution.dead.start_index; i++)
				movement_states[i] = ns_hmm_missing;
		}
		for (unsigned int i = solution.dead.start_index; i <= solution.dead.end_index; i++)
			if (!solution.expanding.skipped && i >= solution.expanding.start_index && i <= solution.expanding.end_index)
				movement_states[i] = ns_hmm_not_moving_expanding;
			else if (!solution.expanding.skipped && i < solution.expanding.start_index) {
				if (estimator.state_defined(ns_hmm_not_moving_alive))
					movement_states[i] = ns_hmm_not_moving_alive;
				else  movement_states[i] = ns_hmm_not_moving_dead;
			}
			else movement_states[i] = ns_hmm_not_moving_dead;
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

	ns_hmm_movement_state previous_state = movement_states[start_i];
	double cur_p = 0, log_likelihood(0);
	unsigned long cur_pi(0);
	state_info.path.resize(movement_states.size());
	state_info.path[cur_pi].sub_measurements.resize(estimator.number_of_sub_probabilities(), 0);
	state_info.path[cur_pi].sub_probabilities.resize(estimator.number_of_sub_probabilities(), 0);

	for (unsigned int i = 0; i < start_i; i++) {
		state_info.path[i].state= ns_hmm_unknown_state;
		state_info.path[i].total_probability = 0;
		state_info.path[i].sub_measurements.resize(estimator.number_of_sub_probabilities(), 0);
		state_info.path[i].sub_probabilities.resize(estimator.number_of_sub_probabilities(), 0);
	}
	for (unsigned int i = start_i; i < movement_states.size(); i++) {
		if (path.element(i).excluded || path.element(i).censored) {
			state_info.path[i].state = previous_state;
			state_info.path[i].total_probability = cur_p;
			state_info.path[i].sub_measurements = state_info.path[cur_pi].sub_measurements;
			state_info.path[i].sub_probabilities = state_info.path[cur_pi].sub_probabilities;
			continue;
		}
	
		if (movement_states[i] == ns_hmm_unknown_state)
			throw ns_ex("ns_hmm_solver::probability_of_path_solution()::encountered an unknown state");
		estimator.probability_for_each_state(path.element(i).measurements, emission_probabilities);
		cur_p = log(transition_probability[previous_state][movement_states[i]] *
			emission_probabilities[movement_states[i]]);

		log_likelihood += cur_p;
		state_info.path[i].state = movement_states[i];
		state_info.path[i].total_probability = cur_p;
		try {
			estimator.provide_measurements_and_sub_probabilities(movement_states[i], path.element(i).measurements, state_info.path[i].sub_measurements, state_info.path[i].sub_probabilities);
		}
		catch (...) {
			std::cerr << "WHA";
			throw;
		}
		previous_state = movement_states[i];
	}
	state_info.log_likelihood = log_likelihood;
}


//run the viterbi algorithm using the specified indicies of the path

double ns_hmm_solver::run_viterbi(const ns_analyzed_image_time_path & path, const ns_emperical_posture_quantification_value_estimator & estimator, const std::vector<unsigned long> path_indices,
	std::vector<ns_hmm_state_transition_time_path_index > &movement_transitions) {
	const int number_of_states((int)ns_hmm_unknown_state);
	std::vector<std::vector<double> > a;
	build_state_transition_matrix(estimator,a);

	std::vector<unsigned long>optimal_path_state(path_indices.size(), 0);
	double optimal_path_log_probability;
	//std::cerr << "Start:\n";
	{
		unsigned long fbdone = 0, nobs(path_indices.size());
		unsigned long mstat(number_of_states);
		unsigned long lrnrm;
		std::vector<std::vector<double> > probabilitiy_of_path(nobs, std::vector<double>(mstat, 0));
		std::vector< std::vector<unsigned long> > previous_state(nobs, std::vector<unsigned long>(mstat, 0));
		std::vector<double> emission_log_probabilities;
		estimator.probability_for_each_state(path.element(path_indices[0]).measurements, emission_log_probabilities);
		emission_log_probabilities[ns_hmm_moving_weakly_post_expansion] = 0; //do not allow animals to start as moving weakly post-expansion.  They can only start weakly /pre-expansion/ if the expansion was not observed!
		int i, j, t;
		double max_p, max_prev_i;
		for (i = 0; i < mstat; i++) probabilitiy_of_path[0][i] = log(emission_log_probabilities[i]);
		for (t = 1; t < nobs; t++) {
			if (path.element(t).excluded)
				continue;
			estimator.probability_for_each_state(path.element(path_indices[t]).measurements, emission_log_probabilities);

			for (j = 0; j < mstat; j++) { //probability of moving from i at time t-1 to j now
				max_p = -DBL_MAX;
				max_prev_i = 0;
				for (i = 0; i < mstat; i++) {
					const double cur = probabilitiy_of_path[t - 1][i] + log(a[i][j] * emission_log_probabilities[j]);
					if (cur > max_p) {
						max_p = cur;
						max_prev_i = i;
					}
				}
				//	std::cerr << max_prev_i << " " << max_p << "\n";
				probabilitiy_of_path[t][j] = max_p;
				previous_state[t][j] = max_prev_i;
			}
		}
		*optimal_path_state.rbegin() = 0;
		optimal_path_log_probability = -DBL_MAX;
		for (j = 0; j < mstat; j++) {
			if (probabilitiy_of_path[nobs - 1][j] > optimal_path_log_probability) {
				optimal_path_log_probability = probabilitiy_of_path[nobs - 1][j];
				*optimal_path_state.rbegin() = j;
			}
		}
		for (t = nobs - 1; t >= 1; t--)
			optimal_path_state[t - 1] = previous_state[t][optimal_path_state[t]];
	}

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

	return optimal_path_log_probability;
}

void ns_hmm_solver::build_movement_state_solution_from_movement_transitions(const std::vector<unsigned long> path_indices, const std::vector<ns_hmm_state_transition_time_path_index > & movement_transitions){
	movement_state_solution.moving.start_index = path_indices[0];
	ns_movement_state m = ns_movement_fast;
	int expanding_state = 0;
	movement_state_solution.expanding.skipped = true;
	movement_state_solution.moving.skipped = false;
	movement_state_solution.moving.end_index = *path_indices.rbegin();
	//go through each of the state transitions and annotate what has happened in the solution.
	for (unsigned int i = 0; i < movement_transitions.size(); i++) {
		//look for movement transitions
		if (m == ns_movement_fast && (movement_transitions[i].first != ns_hmm_missing && movement_transitions[i].first != ns_hmm_moving_vigorously)) {
			if (i != 0) {
				movement_state_solution.moving.skipped = false;
				movement_state_solution.moving.end_index = path_indices[movement_transitions[i].second];
			}
			else movement_state_solution.moving.skipped = true;
			m = ns_movement_slow;
			movement_state_solution.slowing.start_index = path_indices[movement_transitions[i].second];
			movement_state_solution.slowing.skipped = false;
			movement_state_solution.slowing.end_index = *path_indices.rbegin();
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
			movement_state_solution.dead.skipped = false;
			movement_state_solution.dead.end_index = *path_indices.rbegin();
		}
		//now we handle expansions
		switch (expanding_state) {
		case 0:
			if (movement_transitions[i].first == ns_hmm_moving_weakly_expanding ||
				movement_transitions[i].first == ns_hmm_not_moving_expanding) {
				movement_state_solution.expanding.skipped = false;
				movement_state_solution.expanding.start_index = path_indices[movement_transitions[i].second];
				movement_state_solution.expanding.end_index = *path_indices.rbegin();
				expanding_state = 1;
			}
			else if (movement_transitions[i].first == ns_hmm_moving_weakly_post_expansion)
				throw ns_ex("Unexpected pre-expansion state!");
			break;
		case 1:
			if (movement_transitions[i].first != ns_hmm_moving_weakly_expanding &&
				movement_transitions[i].first != ns_hmm_not_moving_expanding) {
				movement_state_solution.expanding.end_index = path_indices[movement_transitions[i].second];
				expanding_state = 2;
			}
			break;
		case 2: if (movement_transitions[i].first != ns_hmm_moving_weakly_post_expansion &&
			movement_transitions[i].first != ns_hmm_not_moving_dead)
			throw ns_ex("Unexpected post-expansion state!");
		}
	}
	if (movement_state_solution.dead.start_index == 66 &&
		movement_state_solution.dead.end_index == 400)
		std::cerr << "WHA";
}

//m[i][j] is the log probabilitiy that an individual in state i transitions to state j.
void ns_hmm_solver::build_state_transition_matrix(const ns_emperical_posture_quantification_value_estimator & estimator, std::vector<std::vector<double> > & m) {
	m.resize((int)ns_hmm_unknown_state);


	const double penalized_transition = 1e-8;

	for (unsigned int i = 0; i < (int)ns_hmm_unknown_state; i++) {
		m[i].resize(0);
		m[i].resize((int)ns_hmm_unknown_state, 0);
		m[i][i] = 1000;	//make staying in the same state more probable than switching, to discourage short stays in each state
	}
	//if there are any loops anywhere here, the approach will not function
	//because we set all the transition probabilities equal and this works
	//only because the all state transitions are irreversable.
	m[ns_hmm_missing][ns_hmm_moving_vigorously] = 1;
	m[ns_hmm_missing][ns_hmm_moving_weakly] = 1;
	m[ns_hmm_missing][ns_hmm_moving_weakly_expanding] = 1;
	m[ns_hmm_missing][ns_hmm_not_moving_alive] = 1;
	m[ns_hmm_missing][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_missing][ns_hmm_not_moving_dead] = penalized_transition;	//we penalize any path that skips death time expansion

	m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly_expanding] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_alive] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_dead] = penalized_transition;

	m[ns_hmm_moving_weakly][ns_hmm_moving_weakly_expanding] = 1;
	m[ns_hmm_moving_weakly][ns_hmm_not_moving_alive] = 1;
	m[ns_hmm_moving_weakly][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_moving_weakly][ns_hmm_not_moving_dead] = penalized_transition;

	m[ns_hmm_moving_weakly_expanding][ns_hmm_moving_weakly_post_expansion] = 1;
	m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_moving_weakly_post_expansion][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_not_moving_expanding][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_not_moving_alive][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_not_moving_alive][ns_hmm_not_moving_dead] = penalized_transition;



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


ns_time_path_posture_movement_solution ns_time_path_movement_markov_solver::estimate_posture_movement_states(int software_version,const ns_analyzed_image_time_path * path, ns_analyzed_image_time_path * output_path, std::ostream * debug_output)const{

	ns_hmm_solver solver;
	solver.solve(*path, estimator);

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
		if (number_of_non_zeros == 0) {
			gmm_weights[0] = 1;
			gmm_weights[1] = 0;
			gmm_weights[2] = 0;
			for (unsigned int i = 0; i < 3; i++) {
				gmm_means[i] = 0;
				gmm_var[i] = 1;
			}
			return;
		}
		
		double start_weights[3] = { 1/3.0,1 / 3.0,1 / 3.0 };
		double start_means[3] = { -1,0,1 };
		double start_variance[3] = { 1,1,1 };
		gmm.Train(data, number_of_non_zeros);
		double sum_of_weights = 0;
		for (unsigned int i = 0; i < 3; i++) {
			gmm_weights[i] = gmm.Prior(i);
			gmm_means[i] = *gmm.Mean(i);
			gmm_var[i] = *gmm.Variance(i);
			sum_of_weights += gmm_weights[i];
		}
		if (abs(sum_of_weights - 1) > 0.01)
			throw ns_ex("GMM problem");
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
		//output sorted by weight
		std::vector< ns_gmm_sorter> sorted(3);
		for (unsigned int i = 0; i < 3; i++)
			sorted[i] = ns_gmm_sorter(gmm_weights[i], gmm_means[i], gmm_var[i]);
		std::sort(sorted.begin(), sorted.end());
		o << zero_probability ;
		for (unsigned int i = 0; i < 3; i++)
			o << "," << sorted[i].weight << "," << sorted[i].mean << "," << sorted[i].var;
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
		return e.death_time_posture_analysis_measure_v2();
	}
	bool is_zero(const ns_analyzed_image_time_path_element_measurements & e) const {
		return abs(e.spatial_averaged_movement_sum) < .01;
	}
};
struct ns_movement_emission_accessor {
	double operator()(const ns_hmm_emission & e) const {
		return e.measurement.death_time_posture_analysis_measure_v2();
	}
	bool is_zero(const ns_hmm_emission & e) const {
		return abs(e.measurement.spatial_averaged_movement_sum) < .01;
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
		return movement.point_emission_probability(e) * intensity_1x.point_emission_probability(e)
													*intensity_2x.point_emission_probability(e)
													*intensity_4x.point_emission_probability(e);
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
		o << "Movement State,Variable,";
		ns_emission_probabiliy_sub_model<ns_movement_accessor>::write_header(o);
	}
	void write(const ns_hmm_movement_state state,std::ostream & o) const {
		o << ns_hmm_movement_state_to_string(state) << ",m,";
		movement.write(o);
		o << "\n" <<ns_hmm_movement_state_to_string(state) << ",i1,";
		intensity_1x.write(o);
		o << "\n" << ns_hmm_movement_state_to_string(state) << ",i2,";
		intensity_2x.write(o);
		o << "\n" << ns_hmm_movement_state_to_string(state) << ",i4,";
		intensity_4x.write(o);
	}
	ns_hmm_movement_state read(std::istream & i) {
		ns_hmm_movement_state state;
		ns_get_string get_string;
		std::string tmp;
		int r = 0;
		while (!i.fail()) {
			get_string(i, tmp);
			if (i.fail()) {
				if (r == 0)
					return ns_hmm_unknown_state;
				else
					throw ns_ex("ns_emission_probabiliy_model::read()::Bad model file");
			}
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
		return state;
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
		throw ns_ex("ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names()::Cannot find any probabiltiy model");
	emission_probability_models.begin()->second->sub_probability_names(names);
}
unsigned long ns_emperical_posture_quantification_value_estimator::number_of_sub_probabilities() const {
	if (emission_probability_models.empty())
		throw ns_ex("ns_emperical_posture_quantification_value_estimator::provide_sub_probability_names()::Cannot find any probabiltiy model");
	return emission_probability_models.begin()->second->number_of_sub_probabilities();
}

bool ns_emperical_posture_quantification_value_estimator::state_defined(const ns_hmm_movement_state & m) const {
	return emission_probability_models.find(m) != emission_probability_models.end();

}

void ns_emperical_posture_quantification_value_estimator::probability_for_each_state(const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & d) const {
	d.resize(0);
	d.resize((int)ns_hmm_unknown_state,0);
	//double sum(0);
	for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); p++) {
		const double tmp(p->second->point_emission_probability(e));
		d[p->first] = tmp;
		//sum += tmp;
	}
	//for (unsigned int i = 0; i < d.size(); i++)
	//	d[i] = d[i] / sum;
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
			ns_hmm_movement_state state = model->read(i);
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
		p->second->write(p->first,o);
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
		id.group_id = ns_atoi64(tmp.c_str());
		getline(in, tmp, ',');
		id.path_id = ns_atoi64(tmp.c_str());

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

void ns_emperical_posture_quantification_value_estimator::build_estimator_from_observations() {

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
	/*//we mix together expansion annotations with and without animal movement.
	//this removes any bias the machine has as to whether expansion happens before or after the final movement.
	auto p_expansion_moving = observed_values.find(ns_hmm_moving_weakly_expanding);
	auto p_expansion_not_moving = observed_values.find(ns_hmm_not_moving_expanding);
	if (p_expansion_moving != observed_values.end() && p_expansion_not_moving != observed_values.end()) {
		p_expansion_moving->second.insert(p_expansion_moving->second.end(), p_expansion_not_moving->second.begin(), p_expansion_not_moving->second.end());
		p_expansion_not_moving->second = p_expansion_moving->second;
	}*/

	std::vector<unsigned long > state_counts((int)(ns_hmm_unknown_state), 0);
	for (auto p = observed_values.begin(); p != observed_values.end(); p++) {
		if (p->second.size() < 100) {
			std::cerr << "Very few annotations were made for state " << ns_hmm_movement_state_to_string(p->first) << ".  It will not be considered in the model.\n";
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
	ns_hmm_movement_state required_states[3] = { ns_hmm_missing, ns_hmm_moving_weakly, ns_hmm_not_moving_dead };
	ns_ex ex;
	std::cout << "Observation counts:\n";
	for (unsigned int i = 0; i < state_counts.size(); i++) 
		std::cout << ns_hmm_movement_state_to_string((ns_hmm_movement_state)i) << "(" << i << ")\t" << state_counts[i] << "\n";
	for (unsigned int i = 0; i < state_counts.size(); i++) {
		if (state_counts[i] < 100) {

			for (unsigned int j = 0; j < 3; j++) {
				if (required_states[j] == i) {
					ex << "Not enough annotations were provided for state " << ns_hmm_movement_state_to_string(required_states[i]) << " (" << state_counts[required_states[i]] << " provided).\n";
					continue;
				}
			}
			if (state_counts[i] == 0)
				std::cerr << "No annotations were made for state " << ns_hmm_movement_state_to_string((ns_hmm_movement_state)i) << ".  It will not be considered in the model.\n";
		}
	}

	if (!ex.text().empty())
		throw ex;
}
void ns_emperical_posture_quantification_value_estimator::write_visualization(std::ostream & o, const std::string & experiment_name)const{
}

bool ns_emperical_posture_quantification_value_estimator::add_observation(const std::string &software_version, const ns_death_time_annotation & properties,const ns_analyzed_image_time_path * path){
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
		
		for (unsigned int i = 0; i < path->element_count(); i++) {
			if (path->element(i).excluded || path->element(i).censored)
				continue;
			const ns_hmm_movement_state s(path->by_hand_hmm_movement_state(path->element(i).absolute_time,*this));
			if (s == ns_hmm_unknown_state)
				throw ns_ex("Unknown state encountered!");
			//if (s == ns_hmm_missing)
			//	continue;				//do not learn this state
			std::vector<ns_hmm_emission> & v(observed_values[s]);
			v.resize(v.size() + 1);
			ns_hmm_emission & e = *v.rbegin();
			e.measurement = path->element(i).measurements;
			e.path_id = properties.stationary_path_id;
			e.emission_time = path->element(i).absolute_time;
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
	 i >> version_flag;
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
