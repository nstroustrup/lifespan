#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_normal_distribution.h"
#include "ns_posture_analysis_models.h"
	

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
		if (!cumulatives_calculated)
			calculate_cumulative_loglikelihoods(movement,tm);
		switch((ns_state)current_state_id){
			case ns_moving: //for now, don't define the skipping state
				return -std::numeric_limits<double>::infinity();
			case ns_slowing:{
				//if the region starts at position 0 (a common case) we can use the stored cumulative sum.
				if ((likelihood_window_size <= 0 && start_index == 0)|| (likelihood_window_size > 0 && stop_index-likelihood_window_size <= 0)){
					if (stop_index == moving_cumulative_loglikelihood.size())
						return moving_cumulative_loglikelihood[moving_cumulative_loglikelihood.size()-1];
					return moving_cumulative_loglikelihood[stop_index];
				}
				//but if the region doesn't start at zero we have to recalculate the sum
				//as using the cached cumulative introduces numerical precision problems
				if (likelihood_window_size > 0 && stop_index - likelihood_window_size > start_index)
					start_index = stop_index-likelihood_window_size;

				double d(0);
				for (unsigned int i = start_index; i < stop_index; i++)
					d+=moving_loglikelihood[i];
				return ns_catch_infinity(d);
			}
			case ns_dead:{
				if ((likelihood_window_size <= 0 && stop_index == movement.size()) || (likelihood_window_size > 0 && start_index+likelihood_window_size >= movement.size()))
					return reverse_cumulative_dead_loglikelihood[start_index];
				if (likelihood_window_size > 0 && start_index + likelihood_window_size < stop_index)
					stop_index = start_index+likelihood_window_size;
				
				double d(0);
				for (unsigned int i = start_index; i < stop_index; i++)
					d+=dead_loglikelihood[i];
				return ns_catch_infinity(d);

			}
			default: throw ns_ex("Invalid state:") << (ns_state)current_state_id;
		}
	}
	static int state_count(){return 3;}
	const int likelihood_window_size;
	mutable bool cumulatives_calculated;

	void calculate_cumulative_loglikelihoods(const std::vector<double> & movement, const std::vector<double> & tm) const;
	inline double loglikelihood_of_moving_animal(const double d) const {
		const double mp(value_estimator.fraction_of_moving_values_less_than(d)),
			  dp(value_estimator.fraction_of_dead_samples_greater_than(d));
		if (mp == 0)
			return ns_catch_infinity(-std::numeric_limits<double>::infinity());
		return log(mp)-log(mp+dp);
	}
	inline double loglikelihood_of_dead_animal(const double d) const{
		const double mp(value_estimator.fraction_of_moving_values_less_than(d)),
			  dp(value_estimator.fraction_of_dead_samples_greater_than(d));
		if (dp==0)
			return ns_catch_infinity(-std::numeric_limits<double>::infinity());
		return log(dp)-log(mp+dp);
	}
	mutable std::vector<double> moving_loglikelihood;
	mutable std::vector<double> dead_loglikelihood;

	mutable std::vector<double> moving_cumulative_loglikelihood;
	mutable std::vector<double> reverse_cumulative_dead_loglikelihood;

	const ns_emperical_posture_quantification_value_estimator value_estimator;
};

double inline ns_truncate_positive(const double & d){
	return (d<=0)?(std::numeric_limits<double>::epsilon()):(d);
};

double inline ns_truncate_negatives(const double & d){
	return (d>=0?d:0);
}

#include <fstream>
void ns_posture_change_markov_likelihood_estimator::calculate_cumulative_loglikelihoods(const std::vector<double> & movement, const std::vector<double> & tm) const{
	if (movement.size() == 0)
		throw ns_ex("ns_posture_change_markov_likelihood_estimator::calculate_cumulative_loglikelihoods()::Empty time series received");

	moving_cumulative_loglikelihood.resize(movement.size());
	reverse_cumulative_dead_loglikelihood.resize(movement.size());

	moving_loglikelihood.resize(movement.size());
	dead_loglikelihood.resize(movement.size());

	moving_loglikelihood[0] = loglikelihood_of_moving_animal(movement[0]);
	moving_cumulative_loglikelihood[0] = moving_loglikelihood[0];

	dead_loglikelihood[movement.size()-1] = loglikelihood_of_dead_animal(movement[movement.size()-1]);
	reverse_cumulative_dead_loglikelihood[movement.size()-1] = dead_loglikelihood[movement.size()-1];
	
	for (unsigned int i = 1; i < movement.size(); i++){
		moving_loglikelihood[i] = loglikelihood_of_moving_animal(movement[i]);
		dead_loglikelihood[movement.size()-i-1] = loglikelihood_of_dead_animal(movement[movement.size()-i-1]);

		moving_cumulative_loglikelihood[i] = moving_cumulative_loglikelihood[i-1] + moving_loglikelihood[i];
		reverse_cumulative_dead_loglikelihood[movement.size()-i-1] = reverse_cumulative_dead_loglikelihood[movement.size()-i] + dead_loglikelihood[movement.size()-i-1];

	}
	
	cumulatives_calculated = true;
	/*
	std::ofstream out("c:\\probs3.csv");
	out << "t,R,Cumulative loglikelihood(Moving;R),Cumulative loglikelihood(Dead;R),Windowed loglikelihood(Moving;R),Windowed loglikelihood(Dead;R)\n";
	for (unsigned int i = 0; i < movement.size(); i++)
		out << tm[i] << "," << movement[i] << "," << moving_cumulative_loglikelihood[i] << "," << reverse_cumulative_dead_loglikelihood[i] << ","
			<< (*this)(1,0,i,movement,tm) << "," << (*this)(2,i,movement.size(),movement,tm) << "\n";
	
	out.close();*/
};

ns_time_path_posture_movement_solution ns_time_path_movement_markov_solver::estimate_posture_movement_states(int software_version,const ns_analyzed_image_time_path * path, ns_analyzed_image_time_path * output_path, std::ostream * debug_output)const{
	std::vector<double> movement_ratio, 
					    tm;
	std::vector<unsigned long> index_mapping;
	movement_ratio.reserve(path->element_count());
	index_mapping.reserve(path->element_count()+1);
	tm.reserve(path->element_count()); //we use tm to 
	
	for (unsigned int i = 0; i < path->element_count(); i++){
		if (path->element(i).excluded)
			continue;
		movement_ratio.push_back((software_version == 1 ? path->element(i).measurements.death_time_posture_analysis_measure_v1() : path->element(i).measurements.death_time_posture_analysis_measure_v2()));
		tm.push_back(path->element(i).absolute_time);
		index_mapping.push_back(i);
	}
	index_mapping.push_back(path->element_count());
	//we ran the analysis using only data that was not excluded
	//now we must replace the internal indices to our non-excluded time series
	//with those that match the original time series
	
	ns_sequential_hidden_markov_solution markov_solution(3);
	ns_time_path_posture_movement_solution solution(estimate_posture_movement_states(movement_ratio,tm,output_path!=0,markov_solution,debug_output));
	if (output_path != 0){
		output_path->posture_quantification_extra_debug_field_names.resize(markov_solution.number_of_states()+1);
		output_path->posture_quantification_extra_debug_field_names[0] = "Slow Moving State Loglikelihood";
		output_path->posture_quantification_extra_debug_field_names[1] = "Changing Posture State Loglikelihood";
		output_path->posture_quantification_extra_debug_field_names[2] = "Dead Loglikelihood";
		output_path->posture_quantification_extra_debug_field_names[3] = "Total Model Loglikelihood";
		for (unsigned int i = 0; i < markov_solution.debug_state_loglikelihoods_over_time.size(); i++){
			const int j = index_mapping[i];
			output_path->element(j).measurements.posture_quantification_extra_debug_fields.resize(markov_solution.number_of_states()+1);
			for (unsigned int k = 0; k < markov_solution.number_of_states(); k++){
				output_path->element(j).measurements.posture_quantification_extra_debug_fields[k] = markov_solution.debug_state_loglikelihoods_over_time[i].loglikelihood[k];
			}
			output_path->element(j).measurements.posture_quantification_extra_debug_fields[3] =  
				output_path->element(j).measurements.posture_quantification_extra_debug_fields[1] +
				output_path->element(j).measurements.posture_quantification_extra_debug_fields[2];
		}
	}
	if (!solution.dead.skipped){
		solution.dead.start_index = index_mapping[solution.dead.start_index];
		solution.dead.end_index = index_mapping[solution.dead.end_index];
	}
	if (!solution.moving.skipped){
		solution.moving.start_index = index_mapping[solution.moving.start_index];
		solution.moving.end_index = index_mapping[solution.moving.end_index];
	}
	if (!solution.slowing.skipped){
		solution.slowing.start_index = index_mapping[solution.slowing.start_index];
		solution.slowing.end_index = index_mapping[solution.slowing.end_index];
	}
	return solution;
}

/ns_time_path_posture_movement_solution ns_time_path_movement_markov_solver::estimate_posture_movement_states(const std::vector<double> & movement_ratio, const std::vector<double> & tm, bool output_loglikelihood_series, ns_sequential_hidden_markov_solution & markov_solution, std::ostream * debug_output)const{
/*	for (unsigned int i = 0; i < movement_ratio.size(); i++)
		if (movement_ratio[i] <= 0)
			throw ns_ex("ns_movement_markov_solver()::solve()::Cannot handle negative ratios!");*/

	ns_time_path_posture_movement_solution sol;
	ns_sequential_hidden_markov_state_estimator<3,ns_posture_change_markov_likelihood_estimator> markov_estimator(debug_output);

	markov_estimator.run(markov_solution,movement_ratio,tm,ns_posture_change_markov_likelihood_estimator(estimator,0),output_loglikelihood_series);
	sol.loglikelihood_of_solution = markov_solution.cumulative_solution_loglikelihood;
	sol.moving.skipped = markov_solution.state_was_skipped(0);
	sol.slowing.skipped = markov_solution.state_was_skipped(1);
	sol.dead.skipped = markov_solution.state_was_skipped(2);
	if (!sol.moving.skipped){
		sol.moving.start_index = markov_solution.state_start_indices[0];
		if (!sol.slowing.skipped)
			sol.moving.end_index = markov_solution.state_start_indices[1];
		else if (!sol.dead.skipped)
			sol.moving.end_index = markov_solution.state_start_indices[2];
		else sol.moving.end_index = movement_ratio.size();
	}
	if (!sol.slowing.skipped){
		sol.slowing.start_index = markov_solution.state_start_indices[1];
		if (!sol.dead.skipped)
			sol.slowing.end_index = markov_solution.state_start_indices[2];
		else sol.slowing.end_index = movement_ratio.size();
	}	
	if (!sol.dead.skipped){
		sol.dead.start_index = markov_solution.state_start_indices[2];
		sol.dead.end_index = movement_ratio.size();
	}
	return sol;
}

void ns_emperical_posture_quantification_value_estimator::read_observation_file(std::ostream & out, std::ostream & dead_cdf_out, std::ostream * vis, const std::string & experiment_name) const {
	out << "HMM training data from " << experiment_name << ",\n";
	//first write normalization stats
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
	for (std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> >::const_iterator p = observed_values.begin(); p != observed_values.end(); p++) {
		for (unsigned int i = 0; i < p->second.size(); i++) {
			<< p->second[i].measurement.detection_set_id << ","
				<< p->second[i].measurement.group_id << ","
				<< p->second[i].measurement.path_id << ","
				<< "d," << (int)p->first << ","
				p->second[i].measurement.write(out, ns_vector_2i(0, 0), false);
			out << "\n";
		}
	}
	if (vis != 0)
		write_visualization(*vis, experiment_name);
}

void ns_emperical_posture_quantification_value_estimator::read_observation_file(std::istream & in){
	string tmp;
	getline(in, tmp, "\n");
	getline(in, tmp, "\n");
	if (tmp != "path_stats")
		throw ns_ex("Invalid output");
	//read normalization stats
	ns_vector_2d tmp_d;
	bool tmp_b;
	while (true) {
		getline(in, tmp, ",");
		if (tmp == "measurements")
			break;
		ns_stationary_path_id id;
		id.detection_set_id = ns_atoi64(tmp.c_str());
		getline(in, tmp, ",");
		id.group_id = ns_atoi64(tmp.c_str());
		getline(in, tmp, ",");
		id.path_id = ns_atoi64(tmp.c_str());

		getline(in, tmp, ",");
		if (tmp == "m" || tmp == "v" || tmp == "a") { //reading in a normalization stat
			ns_hmm_emission_normalization_stats & stats = normalization_stats[id];
			
			if (tmp == "m")
				stats.mean.read(in, tmp_d, tmp_b);
			else if (tmp == "v")
				stats.variance.read(in, tmp_d, tmp_b);
			else if (tmp == "a") {
				getline(tmp, "\n");
				stats.source.from_string(tmp);
			}
		}
		else if (tmp == "d") { //reading in a data point
			getline(in, tmp, ",");
			int state_i = atoi(tmp.c_str());
			if (state_i >= ns_hmm_unknown_state)
				throw ns_ex("Malformed state record");
			ns_hmm_movement_state state((ns_hmm_movement_state)state_i);
			std::vector<ns_hmm_emission> & observations = observed_values[state_i];
			observations.resize(observations.size() + 1);
			ns_hmm_emission & e(*observations.rbegin());
			e.source = id;
			e.measurement.read(in, tmp_d, tmp_b);
		}
		else throw ns_ex("Malformed file");
		if (in.fail())
			throw ns_ex("Malformed file");
	}
	if (vis != 0)
		write_visualization(*vis, experiment_name);
}


void ns_emperical_posture_quantification_value_estimator::write_visualization(std::ostream & o, const std::string & experiment_name)const{
}
bool ns_emperical_posture_quantification_value_estimator::add_by_hand_data_to_sample_set(const std::string &software_version, const ns_death_time_annotation & properties,const ns_analyzed_image_time_path * path){
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
	dead_cdf.generate_ntiles_from_samples(250);
	dead_cdf.clear_samples();
	processed_moving_cdf = raw_moving_cdf;
	raw_moving_cdf.generate_ntiles_from_samples(250);
	raw_moving_cdf.clear_samples();
	processed_moving_cdf.remove_mixture(dead_cdf,25);
	if (!processed_moving_cdf.samples().empty())
		processed_moving_cdf.generate_ntiles_from_samples(250);
	processed_moving_cdf.clear_samples();
}

ns_posture_analysis_model ns_posture_analysis_model::dummy(){
		ns_posture_analysis_model m;
		m.posture_analysis_method = ns_posture_analysis_model::ns_hidden_markov;
		m.hmm_posture_estimator = ns_emperical_posture_quantification_value_estimator::dummy();
		return m;
	}

ns_emperical_posture_quantification_value_estimator ns_emperical_posture_quantification_value_estimator::dummy(){
	ns_emperical_posture_quantification_value_estimator e;
	for (unsigned int i = 0; i < 5; i++){
		e.raw_moving_cdf.add_sample(0);
		e.dead_cdf.add_sample(0);
	}
	e.raw_moving_cdf.generate_ntiles_from_samples(5);
	e.dead_cdf.generate_ntiles_from_samples(5);
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