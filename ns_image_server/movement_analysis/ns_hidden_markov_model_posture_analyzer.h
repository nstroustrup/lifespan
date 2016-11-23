#ifndef NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H
#define NS_HIDDEN_MARKOV_MODEL_POSTURE_ANALYZER_H

#include "ns_ex.h"
#include "ns_hidden_markov_model.h"
#include "ns_death_time_annotation.h"
#include "ns_time_path_posture_movement_solution.h"
#include "ns_posture_analysis_models.h"
#include <limits.h>

/*
template<class T>
ns_linear_model_parameters ns_fit_two_part_linear_model(const std::vector<T> & y, const std::vector<T> & t, const unsigned long start_i, const unsigned long size){
	if (size < 2)
		throw ns_ex("ns_fit_two_part_linear_model()::Attempting to run a linear regression on ") << size << " points";
	double min_y(y[start_i]),min_t((t[start_i]),max_y(y[start_i]),max_t(t[start_i]);

	for (unsigned int i = start_i+1; i < start_i+size; i++){
		if (min_y > y[i])
			min_y = y[i];
		if (min_t > t[i])
			min_t = t[i];
		if (max_y < y[i])
			max_y = y[i];
		if (max_t < t[i])
			max_t = t[i];
	}
	double y_offset = min_y/2+max_y/2,
		   t_offset = min_t/2+max_t/2;
	double mean_y(0),mean_t(0);
	for (unsigned int i = start_i+1; i < start_i+size; i++){
		mean_y+=y[i]-y_offset;
		mean_t+=t[i]-t_offset;
	}
	mean_y = mean_y/size+y_offset;
	mean_t = mean_t/size+t_offset;
	
	ns_linear_model_parameters p;
	p.slope = 0;
	double denom(0);
	for (unsigned int i = start_i+1; i < start_i+size; i++){
		p.slope+=(y[i]-mean_y)*(t[i]-mean_t);
		denom += (y[i]-mean_y)*(y[i]-mean_y);
	}
	p.slope/=denom;
	p.y_intercept = mean_y-p.slope*mean_t;

	p.mean_squared_error = 0;
	for (unsigned int i = start_i+1; i < start_i+size; i++){
		p.mean_squared_error += (y[i]-(p.y_intercept+p.slope*t[i]))*
								(y[i]-(p.y_intercept+p.slope*t[i]));
	}
	p.mean_squared_error/=size;

	return p;
};

struct ns_two_part_linear_model_parameters{
	ns_linear_model_parameters first,
							    second;
	double mean_squared_error() const{return first.mean_squared_error()+second.mean_squared_error();}
	unsigned long number_of_points_in_first;
	double time_of_two_line_intersection(){
		return
	}
};
template<class T>
ns_two_part_linear_model_parameters ns_fit_two_part_linear_model(const std::vector<T> & y, const std::vector<T> & t){
	if (y.size() != t.size())
		throw ns_ex("ns_fit_two_part_linear_model()::Unmatched y and t vectors!");
	if (y.size() < 6)
		throw ns_ex("ns_fit_two_part_linear_model()::Attempting to fit two linear models with only ") << y.size() << " data points";;

	ns_two_part_linear_model_parameters best_model;
	double minimum_mean_squared_error(MAX_DBL);
	unsigned long last_point_in_first_line
	for (unsigned int i = 3; i < y.size() -3){
		ns_two_part_linear_model_parameters model;
		model.first = ns_fit_two_part_linear_model(y, t, 0, i);
		model.second = ns_fit_two_part_linear_model(y, t, i+1, y.size()-(i+1));
		if (model.mean_squared_error() < minimum_mean_squared_error || minimum_mean_squared_error == MAX_DBL){
			minimum_mean_squared_error = model.mean_squared_error();
			best_model = model;
			best_model.number_of_points_in_first = i;
		}
	}
	return best_model;
};
*/


class ns_time_path_movement_markov_solver : public ns_analyzed_image_time_path_death_time_estimator{
public:
	ns_time_path_movement_markov_solver(const ns_emperical_posture_quantification_value_estimator & e):estimator(e){}
	ns_time_path_posture_movement_solution operator()(const ns_analyzed_image_time_path * path, std::ostream * debug_output_=0)const{return estimate_posture_movement_states(2,path,0,debug_output_);}
	ns_time_path_posture_movement_solution operator() (ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::ostream * debug_output=0)const{return estimate_posture_movement_states(2,path,path,debug_output);}

	ns_time_path_posture_movement_solution estimate_posture_movement_states(int software_value,const ns_analyzed_image_time_path * source_path, ns_analyzed_image_time_path * output_path = 0,std::ostream * debug_output=0) const;
	ns_time_path_posture_movement_solution estimate_posture_movement_states(const std::vector<double> & movement_ratio, const std::vector<double> & tm, bool output_loglikelihood_series, ns_sequential_hidden_markov_solution & solution,std::ostream * debug_output=0) const;
	const ns_emperical_posture_quantification_value_estimator & estimator;
	int software_version_number() const { return 2; }
	unsigned long latest_possible_death_time(const ns_analyzed_image_time_path * path,
		const unsigned long last_observation_time) const{return  last_observation_time; }
};


class ns_toy_markov_likelihood_estimator{
public:
	enum {ns_moving,ns_slowing,ns_dead};
	double operator()(int current_state_id,int start_index,int stop_index, const std::vector<double> & movement, const std::vector<double> & tm){
		unsigned long hit(0);
		if (start_index >= stop_index)
			throw ns_ex("Yikes!");
		for (int i = start_index; i < stop_index; i++){
			if (movement[i] == current_state_id)
				hit++;
		}
		return log(hit/(double)(stop_index-start_index));
	}
	static int state_count(){return 3;}
};

#endif
