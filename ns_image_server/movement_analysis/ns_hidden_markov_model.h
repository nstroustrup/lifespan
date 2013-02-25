#ifndef NS_HIDDEN_MARKOV_H
#define NS_HIDDEN_MARKOV_H
#include "ns_ex.h"
#include <vector>
#include <iostream>
#include <math.h>
struct ns_state_loglikelihood_timeseries{
	std::vector<double> loglikelihood;
};
struct ns_sequential_hidden_markov_solution{
	
typedef std::vector<ns_state_loglikelihood_timeseries> ns_state_loglikelihood_timeseries_list;
public:
	std::vector<double> state_loglikelihoods;
	double cumulative_solution_loglikelihood;

	std::vector<long> state_start_indices;
	enum{ns_not_specified = -1, ns_skipped = -2, ns_loglikelihood_not_calculated=1};
	
	bool complete() const{return current_state_+1 >= number_of_states_;}

	const int & current_state() const{return current_state_;}
	ns_sequential_hidden_markov_solution(const int number_of_states,const int initial_state=-1, const long initial_index=-1):
		current_state_(initial_state),
		state_loglikelihoods(number_of_states,ns_loglikelihood_not_calculated),
		state_start_indices(number_of_states,ns_not_specified),
		number_of_states_(number_of_states),cumulative_solution_loglikelihood(0){
		for (int i = 0; i < initial_state; i++)
			state_start_indices[i] = ns_skipped;
		if (initial_state >= 0)
			state_start_indices[initial_state] = initial_index;
	}

	bool state_was_skipped(const int state) const{ return state_start_indices[state] == ns_skipped;}
	bool state_was_calculated(const int state) const{ return state_start_indices[state] != ns_not_specified;}
	int number_of_states() const{
		return number_of_states_;
	}
	void set_current_state(const int & i){current_state_ = i;}

	
	ns_state_loglikelihood_timeseries_list debug_state_loglikelihoods_over_time;

private:
	int current_state_;
	int number_of_states_;
};

template<int number_of_states,class ns_markov_state_likelihood_calculator>
class ns_sequential_hidden_markov_state_estimator{

	typedef std::vector<ns_sequential_hidden_markov_solution> ns_solution_list;

	ns_solution_list solutions;
	ns_solution_list current_round_solutions;

	ns_sequential_hidden_markov_solution best_solution;
	bool best_solution_specified;

	double calculate_interstate_likelihood(const ns_sequential_hidden_markov_solution & s, const int start_state, const int end_index,const std::vector<double> & movement, const std::vector<double> & tm, const ns_markov_state_likelihood_calculator & estimator){

		return estimator(start_state,s.state_start_indices[start_state],end_index,movement,tm);
	}
	
	void generate_initial_solutions(){
		ns_sequential_hidden_markov_solution s(ns_markov_state_likelihood_calculator::state_count());
		for (int i = 0; i < s.number_of_states(); i++){
			for (int j = 0; j < i; j++)
				s.state_start_indices[j] = ns_sequential_hidden_markov_solution::ns_skipped;
			s.state_start_indices[i] = 0;
			s.set_current_state(i);
			current_round_solutions.insert(current_round_solutions.end(),s);
		}
	}
	void generate_solutions_by_skipping_to_end(const ns_sequential_hidden_markov_solution & s,
		const std::vector<double> & movement, const std::vector<double> & tm,const ns_markov_state_likelihood_calculator & lc){
		ns_sequential_hidden_markov_solution n(s);
		for (int i = s.current_state()+1; i < s.number_of_states(); i++)
			n.state_start_indices[i] = ns_sequential_hidden_markov_solution::ns_skipped;
		n.set_current_state(s.number_of_states());
		
		const double cur_state_loglikelihood(calculate_interstate_likelihood(s,s.current_state(),tm.size(),movement,tm,lc));
		n.state_loglikelihoods[s.current_state()] = cur_state_loglikelihood;
		n.cumulative_solution_loglikelihood += cur_state_loglikelihood;
		if (better_than_current_best_solution(n.cumulative_solution_loglikelihood)){
			current_round_solutions.insert(current_round_solutions.end(),n);
		}
	}
	bool generate_new_solutions(const unsigned long current_index,const ns_sequential_hidden_markov_solution & s,
								const std::vector<double> & movement, const std::vector<double> & tm, const ns_markov_state_likelihood_calculator & estimator){
		//allow existing solutions to remain in the same state through this time point
		current_round_solutions.insert(current_round_solutions.end(),s);
		if (debug_output){
			if (debug_output) *debug_output << "Generating next round solutions for\n";
			for (int j = 0; j < s.number_of_states(); j++){
					if (s.state_was_skipped(j))
						if (debug_output) *debug_output << "s ";
					else if (!s.state_was_calculated(j))
						if (debug_output) *debug_output << ". ";
					else if (debug_output) *debug_output << s.state_start_indices[j] << " ";
				}
		}
		//create a solution that skips all the way to the end of the experiment
		const double cur_state_loglikelihood(calculate_interstate_likelihood(s,s.current_state(),current_index,movement,tm,estimator));
		const double cur_cumulative_loglikelihood(s.cumulative_solution_loglikelihood + cur_state_loglikelihood);
		bool added(false);
		
		if (debug_output) *debug_output << ": ending cur state now yields " << cur_cumulative_loglikelihood << "\n";
//		if (s.state_start_indices[0] == 0)
//			std::cerr << "YIKES!";
		//the current solution has already been beaten
		for (int i = s.current_state()+1; i < s.number_of_states(); i++){
			ns_sequential_hidden_markov_solution n(s);
			for (int j = s.current_state()+1; j < i; j++)
				n.state_start_indices[j] = ns_sequential_hidden_markov_solution::ns_skipped;
			n.state_start_indices[i] = current_index;
			n.set_current_state(i);
			
			n.state_loglikelihoods[s.current_state()] = cur_state_loglikelihood;
			n.cumulative_solution_loglikelihood += cur_state_loglikelihood;
			if (n.complete()){
				const double f_state_loglikelihood(calculate_interstate_likelihood(n,n.current_state(),movement.size(),movement,tm,estimator));
				n.state_loglikelihoods[n.current_state()] = f_state_loglikelihood;
				n.cumulative_solution_loglikelihood+=f_state_loglikelihood;
			}
			if (debug_output){
				*debug_output << "\tPossible continuation: \n";
				for (int j = 0; j < n.number_of_states(); j++){
					if (n.state_was_skipped(j))
						*debug_output << "s ";
					else if (!n.state_was_calculated(j))
						*debug_output << ". ";
					else *debug_output << n.state_start_indices[j] << " ";
				}
				*debug_output << ": " << n.cumulative_solution_loglikelihood << " ";
			}
			if (better_than_current_best_solution(n.cumulative_solution_loglikelihood)){
				if (debug_output) *debug_output << " is marked as new current and added\n";
				added = true;
				current_round_solutions.insert(current_round_solutions.end(),n);
			}
			else if (debug_output) *debug_output << " is worse than current and discarded\n";
			
		}
		
		return added;
	}
	bool better_than_current_best_solution(const double d){
		if (d > 0)
			std::cout << "WHOA";
	//	return true;
		return d > best_solution.cumulative_solution_loglikelihood || !best_solution_specified;
	}
	std::ostream * debug_output;
public:
	ns_sequential_hidden_markov_state_estimator(std::ostream * debug_output_=0):debug_output(debug_output_),best_solution(ns_markov_state_likelihood_calculator::state_count()){}
	
	void run(ns_sequential_hidden_markov_solution & solution,const std::vector<double> & movement, const std::vector<double> & tm, const ns_markov_state_likelihood_calculator & estimator, const bool produce_loglikelihoods_over_time){
		if (tm.size() != movement.size())
			throw ns_ex("ns_sequential_hidden_markov_state_estimator()::run()::x and y do not agree in size");
		best_solution_specified = false;
		generate_initial_solutions();
		for (ns_solution_list::iterator p = current_round_solutions.begin(); p != current_round_solutions.end(); p++){
			if (p->complete()){
				const double f_state_loglikelihood(calculate_interstate_likelihood(*p,p->current_state(),movement.size(),movement,tm,estimator));
				p->state_loglikelihoods[p->current_state()] = f_state_loglikelihood;
				p->cumulative_solution_loglikelihood+=f_state_loglikelihood;
			}
		}

		for (unsigned int i = 1; i < movement.size()+2; i++){
				if (debug_output){
					if (current_round_solutions.size() == 0)
						*debug_output << "No solutions left active.\n";
					*debug_output << "Starting round t=" << i << "\n";
				}
				//handle solutions generated in previous round
				for (ns_solution_list::iterator p = current_round_solutions.begin(); p != current_round_solutions.end(); p++){
				if (debug_output){
					*debug_output << "\t";
					for (int j = 0; j < p->number_of_states(); j++){
						if (p->state_was_skipped(j))
								*debug_output << "s ";
							else if (!p->state_was_calculated(j))
								*debug_output << ". ";
							else *debug_output << p->state_start_indices[j] << " ";
						}
					*debug_output << ": ";
					if (!p->complete())
						*debug_output << "? ";
					else; *debug_output << exp(p->cumulative_solution_loglikelihood);
				}

				if (p->complete()){
					if (better_than_current_best_solution(p->cumulative_solution_loglikelihood)){
							//don't allow solutions that are all skips
						bool state_specified(false);
						for (int j = 0; j < p->number_of_states(); j++){
							if (!p->state_was_skipped(j)){
								state_specified = true;
								break;
							}
						}
						if (state_specified){
							if (debug_output)
								*debug_output << " complete and marked as new best.\n";
							best_solution_specified = true;
							best_solution = *p;
						}
						else if (debug_output) *debug_output << " complete and would be the best but has all skips.\n";
					}
				}
				else{
					if (i == 0 || p->state_start_indices[p->current_state()] == i-1){
						solutions.insert(solutions.end(),*p);
						if (debug_output)*debug_output << " incomplete and added to possible solutions.\n";
					}
					else if (debug_output) *debug_output << " incomplete but already in possible solutions.\n";
				}
			}
			current_round_solutions.clear();
			if (i < movement.size()){
				for (ns_solution_list::iterator p = solutions.begin(); p != solutions.end(); ){
					generate_new_solutions(i,*p,movement,tm,estimator);
					//delete solutions that have lower incomplete likelihood than the best complete solution
					if (best_solution_specified && 
						p->current_state() > 0 &&
						p->cumulative_solution_loglikelihood < best_solution.cumulative_solution_loglikelihood)	p = solutions.erase(p);
					else 
						p++;
				}
			}
			else if (i == movement.size()){
					for (ns_solution_list::iterator p = solutions.begin(); p != solutions.end(); p++)
						generate_solutions_by_skipping_to_end(*p,movement,tm,estimator);
				}
			else 
				break;
			}
		
		solution = best_solution;
		if (produce_loglikelihoods_over_time){
			solution.debug_state_loglikelihoods_over_time.resize(movement.size());
			for (unsigned int i = 0; i < movement.size(); i++){
				solution.debug_state_loglikelihoods_over_time[i].loglikelihood.resize(ns_markov_state_likelihood_calculator::state_count());
				for (long s = 0; s < solution.debug_state_loglikelihoods_over_time[i].loglikelihood.size(); s++){
					solution.debug_state_loglikelihoods_over_time[i].loglikelihood[s] = estimator.fill_in_loglikelihood_timeseries(s,i,movement,tm);
				}

			}
		}
	}
};
#endif
