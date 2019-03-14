#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_normal_distribution.h"
#include "ns_posture_analysis_models.h"
#include "gmm.h"
	

double inline ns_catch_infinity(const double & d){
	return (d==-std::numeric_limits<double>::infinity())?-20:d;
}

class ns_posture_change_markov_likelihood_estimator{
	
public:
	ns_posture_change_markov_likelihood_estimator(const ns_emperical_posture_quantification_value_estimator & e,const int window_size=0):
	  cumulatives_calculated(false),value_estimator(e),likelihood_window_size(window_size){ }
	typedef enum {ns_moving=0,ns_slowing=1,ns_dead=2} ns_state;

	double fill_in_loglikelihood_timeseries(int state, unsigned long index, const std::vector<double> & movement, const std::vector<double> & tm) const{
		if (state == ns_slowing)
		return this->operator()(state,0,index+1,movement,tm);
		if (state == ns_dead)
			return this->operator()(state,index,movement.size(),movement,tm);
		return 1;
		
	}

	double operator()(int current_state_id,int start_index,int stop_index, const std::vector<double> & movement, const std::vector<double> & tm) const{
		return 0;
	}
	static int state_count(){return 3;}
	const int likelihood_window_size;
	mutable bool cumulatives_calculated;


	const ns_emperical_posture_quantification_value_estimator value_estimator;
};

double inline ns_truncate_positive(const double & d){
	return (d<=0)?(std::numeric_limits<double>::epsilon()):(d);
};

double inline ns_truncate_negatives(const double & d){
	return (d>=0?d:0);
}
//m[i][j] is the log probabilitiy that an individual in state i transitions to state j.
void ns_build_state_transition_matrix(std::vector<std::vector<double> > & m) {
	m.resize((int)ns_hmm_unknown_state);
	for (unsigned int i = 0; i < (int)ns_hmm_unknown_state; i++) {
		m[i].resize(0);
		m[i].resize((int)ns_hmm_unknown_state, 0);
		m[i][i] = 1;
	}
	//if there are any loops anywhere here, the approach will not function
	//because we set all the transition probabilities equal and this works
	//only because the all state transitions are irreversable.
	m[ns_hmm_missing][ns_hmm_moving_vigorously] = 1;
	m[ns_hmm_missing][ns_hmm_moving_weakly] = 1;
	m[ns_hmm_missing][ns_hmm_moving_weakly_expanding] = 1;
	m[ns_hmm_missing][ns_hmm_not_moving_alive] = 1;
	m[ns_hmm_missing][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_missing][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_moving_weakly_expanding] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_alive] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_moving_vigorously][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_moving_weakly][ns_hmm_moving_weakly_expanding] = 1;
	m[ns_hmm_moving_weakly][ns_hmm_not_moving_alive] = 1;
	m[ns_hmm_moving_weakly][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_moving_weakly][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_moving_weakly_expanding][ns_hmm_moving_weakly_post_expansion] = 1;
	m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_moving_weakly_expanding][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_moving_weakly_post_expansion][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_not_moving_expanding][ns_hmm_not_moving_dead] = 1;

	m[ns_hmm_not_moving_alive][ns_hmm_not_moving_expanding] = 1;
	m[ns_hmm_not_moving_alive][ns_hmm_not_moving_dead] = 1;

	//normalize all transition probabilities
	for (unsigned int i = 0; i < (int)ns_hmm_unknown_state; i++) {
		unsigned int sum = 0;
		for (unsigned int j = 0; j < (int)ns_hmm_unknown_state; j++)
			sum += m[i][j];
		for (unsigned int j = 0; j < (int)ns_hmm_unknown_state; j++)
			m[i][j] =  m[i][j] / (double)sum;
	}
}

ns_hmm_movement_state most_probable_state(const std::vector<double> & d) {
	ns_hmm_movement_state s((ns_hmm_movement_state)0);
	double p(d[0]);
	for (unsigned int i = 1; i < d.size(); i++)
		if (d[i] > p) {
			s = (ns_hmm_movement_state)i;
			p = d[i];
		}
	return s;
}
ns_time_path_posture_movement_solution ns_time_path_movement_markov_solver::estimate_posture_movement_states(int software_version, const ns_analyzed_image_time_path * path, ns_analyzed_image_time_path * output_path, std::ostream * debug_output)const {

	ns_time_path_posture_movement_solution solution;
	bool found_start_time(false);
	unsigned long start_time_i(0);
	std::vector<unsigned long> path_indices;
	for (unsigned int i = start_time_i; i < path->element_count(); i++) {
		if (!path->element(i).excluded) {
			if (!found_start_time) {
				start_time_i = i;
				found_start_time = true;
			}
			path_indices.push_back(i);
		}
	}

	const int number_of_states((int)ns_hmm_unknown_state);
	std::vector<std::vector<double> > a;
	ns_build_state_transition_matrix(a);

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
		estimator.probability_for_each_state(path->element(path_indices[0]).measurements, emission_log_probabilities);
		int i, j, t;
		double max_p, max_prev_i;
		for (i = 0; i < mstat; i++) probabilitiy_of_path[0][i] = emission_log_probabilities[i];
		for (t = 1; t < nobs; t++) {
			if (path->element(t).excluded)
				continue;
			estimator.probability_for_each_state(path->element(path_indices[t]).measurements, emission_log_probabilities);

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

	//std::cerr << "stop\n";
	solution.loglikelihood_of_solution = optimal_path_log_probability;

	/*
	//forward backward
	unsigned long fbdone = 0, nobs(path_indices.size());
	unsigned long mstat(number_of_states);
	unsigned long lrnrm;
	std::vector<std::vector<double> > alpha(nobs,std::vector<double>(mstat,0)), 
									  beta(nobs, std::vector<double>(mstat, 0)),
									  pstate(nobs, std::vector<double>(mstat, 0));
	std::vector<unsigned long> arnrm(nobs,0), brnrm(nobs,0);

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
				for (i = 0; i < mstat; i++) sum += alpha[t - 1][i] * a[i][j] * emission_log_probabilities[j];
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
	*/
	
	if (optimal_path_state.empty())
		throw ns_ex("Empty path state!");
	//now find transition of times between states
	typedef std::pair<ns_hmm_movement_state, unsigned long> ns_movement_transition;
	std::vector<ns_movement_transition > movement_transitions;
	movement_transitions.push_back(ns_movement_transition((ns_hmm_movement_state)optimal_path_state[0], 0));
	for (unsigned int i = 1; i < optimal_path_state.size(); i++) {
		const ns_hmm_movement_state s = (ns_hmm_movement_state)optimal_path_state[i];
		if (s != movement_transitions.rbegin()->first)
			movement_transitions.push_back(ns_movement_transition(s, i));
	}
	solution.moving.start_index = path_indices[0];
	ns_movement_state m = ns_movement_fast;
	int expanding_state = 0;
	solution.expanding.skipped = true;
	solution.moving.skipped = false;
	solution.moving.end_index = *path_indices.rbegin();
	//go through each of the state transitions and annotate what has happened in the solution.
	for (unsigned int i = 0; i < movement_transitions.size(); i++) {
		//look for movement transitions
		if (m == ns_movement_fast && (movement_transitions[i].first != ns_hmm_missing && movement_transitions[i].first != ns_hmm_moving_vigorously)) {
			if (i != 0) {
				solution.moving.skipped = false;
				solution.moving.end_index = path_indices[movement_transitions[i].second - 1];
			}
			else solution.moving.skipped = true;
			m = ns_movement_slow;
			solution.slowing.start_index = path_indices[movement_transitions[i].second];
			solution.slowing.skipped = false;
			solution.slowing.end_index = *path_indices.rbegin();
		}
		if (m == ns_movement_slow && (
			movement_transitions[i].first != ns_hmm_moving_weakly &&
			movement_transitions[i].first != ns_hmm_moving_weakly_expanding &&
			movement_transitions[i].first != ns_hmm_moving_weakly_post_expansion)) {
			if (path_indices[i] != solution.slowing.start_index)
				solution.slowing.end_index = path_indices[movement_transitions[i].second - 1];
			else solution.slowing.skipped = true;
			m = ns_movement_stationary;
			solution.dead.start_index = path_indices[movement_transitions[i].second];
			solution.dead.skipped = false;
			solution.dead.end_index = *path_indices.rbegin();
		}
		//now we handle expansions
		switch (expanding_state) {
		case 0:
			if (movement_transitions[i].first == ns_hmm_moving_weakly_expanding ||
				movement_transitions[i].first == ns_hmm_not_moving_expanding) {
				solution.expanding.skipped = false;
				solution.expanding.start_index = path_indices[movement_transitions[i].second];
				solution.expanding.end_index = *path_indices.rbegin();
				expanding_state = 1;
			}
			if (movement_transitions[i].first == ns_hmm_moving_weakly_post_expansion)
				throw ns_ex("Unexpected pre-expansion state!");
			break;
		case 1:
			if (movement_transitions[i].first != ns_hmm_moving_weakly_expanding &&
				movement_transitions[i].first != ns_hmm_not_moving_expanding) {
				solution.expanding.end_index = path_indices[movement_transitions[i].second - 1];
				expanding_state = 2;
			}
			break;
		case 2: if (movement_transitions[i].first != ns_hmm_moving_weakly_post_expansion &&
			movement_transitions[i].first != ns_hmm_not_moving_dead)
			throw ns_ex("Unexpected post-expansion state!");
		}
	}

	return solution;
}

ns_time_path_posture_movement_solution ns_time_path_movement_markov_solver::estimate_posture_movement_states(const std::vector<double> & movement_ratio, const std::vector<double> & tm, bool output_loglikelihood_series, ns_sequential_hidden_markov_solution & markov_solution, std::ostream * debug_output)const {
	/*	for (unsigned int i = 0; i < movement_ratio.size(); i++)
			if (movement_ratio[i] <= 0)
				throw ns_ex("ns_movement_markov_solver()::solve()::Cannot handle negative ratios!");*/

	ns_time_path_posture_movement_solution sol;
	ns_sequential_hidden_markov_state_estimator<3, ns_posture_change_markov_likelihood_estimator> markov_estimator(debug_output);

	markov_estimator.run(markov_solution, movement_ratio, tm, ns_posture_change_markov_likelihood_estimator(estimator, 0), output_loglikelihood_series);
	sol.loglikelihood_of_solution = markov_solution.cumulative_solution_loglikelihood;
	sol.moving.skipped = markov_solution.state_was_skipped(0);
	sol.slowing.skipped = markov_solution.state_was_skipped(1);
	sol.dead.skipped = markov_solution.state_was_skipped(2);
	if (!sol.moving.skipped) {
		sol.moving.start_index = markov_solution.state_start_indices[0];
		if (!sol.slowing.skipped)
			sol.moving.end_index = markov_solution.state_start_indices[1];
		else if (!sol.dead.skipped)
			sol.moving.end_index = markov_solution.state_start_indices[2];
		else sol.moving.end_index = movement_ratio.size();
	}
	if (!sol.slowing.skipped) {
		sol.slowing.start_index = markov_solution.state_start_indices[1];
		if (!sol.dead.skipped)
			sol.slowing.end_index = markov_solution.state_start_indices[2];
		else sol.slowing.end_index = movement_ratio.size();
	}
	if (!sol.dead.skipped) {
		sol.dead.start_index = markov_solution.state_start_indices[2];
		sol.dead.end_index = movement_ratio.size();
	}
	return sol;
}
template<class accessor_t>
class ns_emission_probabiliy_sub_model {
public:
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
		
		
		double start_weights[3] = { 1/3.0,1 / 3.0,1 / 3.0 };
		double start_means[3] = { -1,0,1 };
		double start_variance[3] = { 1,1,1 };
		GMM gmm(3, start_weights, start_means, start_variance,1000, 1e-5,false);
		gmm.estimate(data, number_of_non_zeros);
		for (unsigned int i = 0; i < 3; i++) {
			gmm_weights[i] = gmm.getMixCoefficient(i);
			gmm_means[i] = gmm.getMean(i);
			gmm_stdev[i] = sqrt(gmm.getVar(i));
		}
	}
	double point_emission_probability(const ns_analyzed_image_time_path_element_measurements & e) const {
		accessor_t accessor;
		const double val = accessor(e);
		const bool is_zero(accessor.is_zero(e));
		if (is_zero) 
			return zero_probability;
		double p = 0;
		for (unsigned int i = 0; i < 3; i++) 
			p+=gmm_weights[i]*ns_likelihood_of_normal_zcore((val - gmm_means[i]) / gmm_stdev[i]);
		return p*(1-zero_probability);
	}
	void write(std::ostream & o) const {
		o << zero_probability ;
		for (unsigned int i = 0; i < 3; i++) 
			o << "," << gmm_weights[i] << "," << gmm_means[i] << "," << gmm_stdev[i];
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
			get_double(in, gmm_stdev[i]);
			if (in.fail()) throw ns_ex("ns_emission_probabiliy_model():read():invalid format");
		}
	}
private:
	
	double zero_probability;
	double gmm_weights[3],
		gmm_means[3],
		gmm_stdev[3];
};
struct ns_intensity_accessor {
	double operator()(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity;
	}	
	bool is_zero(const ns_analyzed_image_time_path_element_measurements & e) const {
		return e.change_in_total_stabilized_intensity == 0;
	}
}; 
struct ns_intensity_emission_accessor {
	double operator()(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity;
	}
	bool is_zero(const ns_hmm_emission & e) const {
		return e.measurement.change_in_total_stabilized_intensity == 0;
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
		intensity.build_from_data< ns_intensity_emission_accessor>(observations);
	}
	double point_emission_probability(const ns_analyzed_image_time_path_element_measurements & e) const {
		return movement.point_emission_probability(e) * intensity.point_emission_probability(e);
	}
	void write(std::ostream & o) const {
		o << "m" << ",";
		movement.write(o);
		o << "\ni,";
		intensity.write(o);
	}
	void read(std::istream & i) {
		std::string tmp;
		getline(i, tmp, ',');
		if (i.fail())
			throw ns_ex("ns_emission_probabiliy_model()::Bad model file");
		int r= 0;
		while (!i.fail()) {
			if (tmp == "m")
				movement.read(i);
			else if (tmp == "i")
				intensity.read(i);
			r++;
			if (r == 2)
				break;
		}
	}
	ns_emission_probabiliy_sub_model<ns_movement_accessor> movement;
	ns_emission_probabiliy_sub_model<ns_intensity_accessor> intensity;

};

//Adapted from forward/backward algorithm in Numerical Recipies in in C++

void ns_emperical_posture_quantification_value_estimator::probability_for_each_state(const ns_analyzed_image_time_path_element_measurements & e, std::vector<double> & d) const {
	d.resize(0);
	d.resize((int)ns_hmm_unknown_state,0);
	double sum(0);
	for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); p++) {
		const double tmp(p->second->point_emission_probability(e));
		d[p->first] = tmp;
		sum += tmp;
	}
	for (unsigned int i = 0; i < d.size(); i++)
		d[i] = d[i] / sum;
}
	

void ns_emperical_posture_quantification_value_estimator::write_observation_data(std::ostream & out, const std::string & experiment_name) const {
	out << "region_id,group_id,path_id,data_type,hmm_movement_state,";
	ns_analyzed_image_time_path_element_measurements::write_header(out);
	out << "\n";
	//first write normalization stats
	
	for (std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> >::const_iterator p = observed_values.begin(); p != observed_values.end(); p++) {
		for (unsigned int i = 0; i < p->second.size(); i++) {
			out << p->second[i].path_id.detection_set_id << ","
				<< p->second[i].path_id.group_id << ","
				<< p->second[i].path_id.path_id << ","
				<< "d," << ns_hmm_movement_state_to_string(p->first) << ",";
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

void ns_emperical_posture_quantification_value_estimator::read(std::istream & i) {
	std::string tmp;
	while (true) {
		getline(i,tmp, ',');
		ns_hmm_movement_state s = ns_hmm_movement_state_from_string(tmp);
		auto p2 = emission_probability_models.find(s);
		if (p2 == emission_probability_models.end())
			p2 = emission_probability_models.insert(emission_probability_models.end(),
				std::map < ns_hmm_movement_state, ns_emission_probabiliy_model *>::value_type(s, new ns_emission_probabiliy_model));
		p2->second->read(i);
	}
}
void ns_emperical_posture_quantification_value_estimator::write(std::ostream & o)const {
	for (auto p = emission_probability_models.begin(); p != emission_probability_models.end(); p++) {
		o << ns_hmm_movement_state_to_string(p->first) << ",";
		p->second->write(o);
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
	for (auto p = observed_values.begin(); p != observed_values.end(); p++) {
		auto p2 = emission_probability_models.find(p->first);
		if (p2 == emission_probability_models.end())
			p2 = emission_probability_models.insert(emission_probability_models.end(),
				std::map < ns_hmm_movement_state, ns_emission_probabiliy_model *>::value_type(p->first, new ns_emission_probabiliy_model ));
		p2->second->build_from_data(p->second);
	}
}
void ns_emperical_posture_quantification_value_estimator::write_visualization(std::ostream & o, const std::string & experiment_name)const{
}

bool ns_emperical_posture_quantification_value_estimator::add_observation(const std::string &software_version, const ns_death_time_annotation & properties,const ns_analyzed_image_time_path * path){
	if (path->by_hand_data_specified()){
		ns_analyzed_image_time_path_element_measurements path_mean, path_mean_square,path_variance;
		path_mean.zero();
		path_variance.zero();
		unsigned long n(0);
		for (unsigned int i = 0; i < path->element_count(); i++) {
			if (path->element(i).excluded)
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
			if (path->element(i).excluded)
				continue;
			const ns_hmm_movement_state s(path->by_hand_hmm_movement_state(path->element(i).absolute_time));
			if (s == ns_hmm_unknown_state)
				return false;
			std::vector<ns_hmm_emission> & v(observed_values[s]);
			v.resize(v.size() + 1);
			ns_hmm_emission & e = *v.rbegin();
			e.measurement = path->element(i).measurements;
			e.path_id = properties.stationary_path_id;
			ns_hmm_emission_normalization_stats & stats = normalization_stats[e.path_id];
			stats.path_mean = path_mean;
			stats.path_variance = path_variance;
			stats.source = properties;
		}
		return true;
	}
	return false;
}
void ns_emperical_posture_quantification_value_estimator::generate_estimators_from_samples(){
	
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