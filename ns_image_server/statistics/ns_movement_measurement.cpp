#include "ns_movement_measurement.h"
#include "ns_vector_bitmap_interface.h"
#include "ns_time_path_solver.h"
#include <vector>
#include <algorithm>

using namespace std;
template<class T>
class ns_measurement_stationary_processed_accessor{
public:
	ns_measurement_stationary_processed_accessor(T & d):data(&d){}
	inline unsigned long &operator[](const unsigned long i){
		return (*data)[i].number_stationary_processed;
	}
	inline unsigned long size(){return (unsigned long)data->size();}
	typedef unsigned long value_type;
	T * data;
};
template<class T>
class ns_measurement_not_translating_accessor{
public:
	ns_measurement_not_translating_accessor(T & d):data(&d){}
	inline unsigned long &operator[](const unsigned long i){
		return (*data)[i].number_not_translating;
	}
	typedef unsigned long value_type;
	T * data;
};
/*
void ns_worm_movement_summary_series::calculate_totals(){
	calculate_maximums();
}*/

struct ns_max_info{
	unsigned long survival_max_value,
				  survival_max_id,
			      not_translating_max_value,
				  not_translating_max_id;

	void update_max(const unsigned long & survival_value, const unsigned long & not_translating_value,const unsigned long & id){
		if (survival_max_value < survival_value ){
			survival_max_value = survival_value;
			survival_max_id = id;
		}
		if (not_translating_max_value < not_translating_value){
			not_translating_max_value = not_translating_value;
			not_translating_max_id = id;
		}
	}
	void update_min(const unsigned long & survival_value, const unsigned long & not_translating_value,const unsigned long & id){
		if (survival_max_value > survival_value){
			survival_max_value = survival_value;
			survival_max_id = id;
		}
		if (not_translating_max_value > not_translating_value){
			not_translating_max_value = not_translating_value;
			not_translating_max_id = id;
		}
	}
};
typedef std::map<ns_worm_movement_measurement_summary_timepoint_type,ns_max_info> 
	ns_worm_movement_summary_series_maximum_info_list;
/*
void ns_worm_movement_summary_series::calculate_survival(){
	if (measurements.size() < 3)
		return;
	//copy data over to processed slot
	/*for (unsigned int i = 0; i < measurements.size(); i++){
			for (ns_worm_movement_measurement_summary_timepoint_list::iterator p = measurements[i].measurements.begin(); p != measurements[i].measurements.end(); ++p){
			p->second.number_stationary_processed = p->second.number_stationary;
			p->second.number_not_translating = p->second.number_moving_slow +
											   p->second.number_changing_posture+
											   p->second.number_stationary;
			p->second.number_total = 
						p->second.number_moving_fast + 
						p->second.number_not_translating;
		}
		measurements[i].all_measurement_types_total.number_stationary_processed = 
			measurements[i].all_measurement_types_total.number_stationary;

		measurements[i].all_measurement_types_total.number_not_translating = 
			measurements[i].all_measurement_types_total.number_moving_slow + 
			measurements[i].all_measurement_types_total.number_changing_posture + 
			measurements[i].all_measurement_types_total.number_stationary;
		measurements[i].all_measurement_types_total.number_total = 
			measurements[i].all_measurement_types_total.number_moving_fast + 
			measurements[i].all_measurement_types_total.number_not_translating;
		measurements[i].all_measurement_types_excluded_total.number_stationary_processed = 
			measurements[i].all_measurement_types_excluded_total.number_stationary;
		measurements[i].all_measurement_types_censored_total.number_not_translating = 
			measurements[i].all_measurement_types_censored_total.number_moving_slow + 
			measurements[i].all_measurement_types_censored_total.number_changing_posture + 
			measurements[i].all_measurement_types_censored_total.number_stationary;
		measurements[i].all_measurement_types_censored_total.number_total = 
			measurements[i].all_measurement_types_censored_total.number_moving_fast + 
			measurements[i].all_measurement_types_censored_total.number_not_translating;
		measurements[i].all_measurement_types_censored_total.number_stationary_processed = 
			measurements[i].all_measurement_types_censored_total.number_stationary;
		measurements[i].all_measurement_types_excluded_total.number_not_translating = 
			measurements[i].all_measurement_types_excluded_total.number_moving_slow + 
			measurements[i].all_measurement_types_excluded_total.number_changing_posture + 
			measurements[i].all_measurement_types_excluded_total.number_stationary;
		measurements[i].all_measurement_types_excluded_total.number_total = 
			measurements[i].all_measurement_types_excluded_total.number_moving_fast + 
			measurements[i].all_measurement_types_excluded_total.number_not_translating;
	}
	
	//smooth processed data
	//ns_measurement_stationary_processed_accessor< std::vector<ns_worm_movement_measurement_summary> > ac(measurements);
	//ns_median_smoother(ac,2);
	//ns_measurement_not_translating_accessor< std::vector<ns_worm_movement_measurement_summary> > tc(measurements);
	//ns_median_smoother(tc,2);
		
	ns_worm_movement_summary_series_maximum_info_list maximums;
	ns_max_info total_maximums,
				total_censored_maximums,
				total_excluded_maximums;
	if (measurements.size() > 0){
		total_maximums.survival_max_id = 0;
		total_maximums.survival_max_value = measurements[0].all_measurement_types_total.number_stationary;
		total_censored_maximums.survival_max_id = 0;
		total_censored_maximums.survival_max_value = measurements[0].all_measurement_types_censored_total.number_stationary_processed;
		total_excluded_maximums.survival_max_id = 0;
		total_excluded_maximums.survival_max_value = measurements[0].all_measurement_types_excluded_total.number_stationary_processed;

		total_maximums.not_translating_max_id = 0;
		total_maximums.not_translating_max_value = measurements[0].all_measurement_types_total.number_not_translating;
		total_censored_maximums.not_translating_max_id= 0;
		total_censored_maximums.not_translating_max_value = measurements[0].all_measurement_types_censored_total.number_not_translating;
		total_excluded_maximums.not_translating_max_id = 0;
		total_excluded_maximums.not_translating_max_value = measurements[0].all_measurement_types_excluded_total.number_not_translating;
	}

	//locate maximum number of stationary objects
	for (long t = 0; t < (long)measurements.size(); t++){
		for (ns_worm_movement_measurement_summary_timepoint_list::iterator p = measurements[t].measurements.begin(); p != measurements[t].measurements.end(); ++p){
			ns_worm_movement_summary_series_maximum_info_list::iterator maximum(maximums.find(p->first));
			if (maximum == maximums.end()){
				maximum = maximums.insert(ns_worm_movement_summary_series_maximum_info_list::value_type(p->first,ns_max_info())).first;
				maximum->second.not_translating_max_id = 0;
				maximum->second.not_translating_max_value = p->second.translation;
				maximum->second.survival_max_id = 0;
				maximum->second.survival_max_value = p->second.number_stationary_processed;
			}
			else{
				maximum->second.update_max(p->second.number_stationary_processed,p->second.number_not_translating,t);
			}
		}
		total_maximums.update_max(measurements[t].all_measurement_types_total.number_stationary_processed,measurements[t].all_measurement_types_total.number_not_translating,t);
		total_censored_maximums.update_max(measurements[t].all_measurement_types_censored_total.number_stationary_processed,measurements[t].all_measurement_types_censored_total.number_not_translating,t);
		total_excluded_maximums.update_max(measurements[t].all_measurement_types_excluded_total.number_stationary_processed,measurements[t].all_measurement_types_excluded_total.number_not_translating,t);
	}

	for (ns_worm_movement_summary_series_maximum_info_list::const_iterator p = maximums.begin(); p != maximums.end(); p++){
		ns_worm_movement_measurement_summary_timepoint_data & max_not_translating_measurement(
				measurements[p->second.not_translating_max_id].measurements[p->first]);
		ns_worm_movement_measurement_summary_timepoint_data & max_stationary_measurement(
				measurements[p->second.not_translating_max_id].measurements[p->first]);

		number_of_moving_animals[p->first].at_time_of_maximum_number_of_stationary_animals =  
															 max_stationary_measurement.number_changing_posture 
															+ max_stationary_measurement.number_moving_fast
									   						+ max_stationary_measurement.number_moving_slow;
		number_of_moving_animals[p->first].at_time_of_maximum_number_of_non_translating_animals =  
															 max_not_translating_measurement.number_changing_posture 
															+ max_not_translating_measurement.number_moving_fast
									   						+ max_not_translating_measurement.number_moving_slow;
	}

	total_number_of_animals.at_time_of_maximum_number_of_stationary_animals = measurements[total_maximums.survival_max_id].all_measurement_types_total.total_moving();
	total_number_of_animals.at_time_of_maximum_number_of_non_translating_animals  = measurements[total_maximums.not_translating_max_id].all_measurement_types_total.total_moving();
	total_number_of_censored_animals.at_time_of_maximum_number_of_stationary_animals = measurements[total_maximums.survival_max_id].all_measurement_types_censored_total.total_moving();
	total_number_of_censored_animals.at_time_of_maximum_number_of_non_translating_animals  = measurements[total_maximums.not_translating_max_id].all_measurement_types_censored_total.total_moving();
	total_number_of_excluded_animals.at_time_of_maximum_number_of_stationary_animals = measurements[total_maximums.survival_max_id].all_measurement_types_excluded_total.total_moving();
	total_number_of_excluded_animals.at_time_of_maximum_number_of_non_translating_animals  = measurements[total_maximums.not_translating_max_id].all_measurement_types_excluded_total.total_moving();
	
	
	//discard stationary objects missing after maximum
	for (unsigned int t = 0; t < (long)measurements.size(); t++){
		for (ns_worm_movement_measurement_summary_timepoint_list::iterator p = measurements[t].measurements.begin(); p != measurements[t].measurements.end(); ++p){
			ns_worm_movement_summary_series_maximum_info_list::iterator maximum(maximums.find(p->first));
			if (t > maximum->second.not_translating_max_id)
				p->second.number_not_translating = maximum->second.not_translating_max_value;	
			if (t > maximum->second.survival_max_id)
				p->second.number_stationary_processed = maximum->second.survival_max_value;
		}
		if (t > total_maximums.not_translating_max_id)
			measurements[t].all_measurement_types_total.number_not_translating = total_maximums.not_translating_max_value;
		if (t > total_maximums.survival_max_id)
			measurements[t].all_measurement_types_total.number_stationary_processed = total_maximums.survival_max_value;
		
		if (t > total_censored_maximums.not_translating_max_id)
			measurements[t].all_measurement_types_censored_total.number_not_translating = total_censored_maximums.not_translating_max_value;
		if (t > total_censored_maximums.survival_max_id)	
			measurements[t].all_measurement_types_censored_total.number_stationary_processed = total_censored_maximums.survival_max_value;
		
		if (t > total_excluded_maximums.not_translating_max_id)
			measurements[t].all_measurement_types_excluded_total.number_not_translating = total_excluded_maximums.not_translating_max_value;
		if (t > total_excluded_maximums.survival_max_id)
			measurements[t].all_measurement_types_excluded_total.number_stationary_processed = total_excluded_maximums.survival_max_value;
		
	}

	//caclulate running minimum
	//we use variables that say "max" because it doesn't make sense
	//to make a new container type just to have them say "min".
	ns_worm_movement_summary_series_maximum_info_list minimums;
	ns_max_info total_minimums,
				total_censored_minimums,
				total_excluded_minimums;
	if (measurements.size() > 0){
		total_minimums.survival_max_id = measurements.size()-1;
		total_minimums.survival_max_value = measurements.rbegin()->all_measurement_types_total.number_stationary_processed;
		total_censored_minimums.survival_max_id = measurements.size()-1;
		total_censored_minimums.survival_max_value = measurements.rbegin()->all_measurement_types_censored_total.number_stationary_processed;
		total_excluded_minimums.survival_max_id = measurements.size()-1;
		total_excluded_minimums.survival_max_value = measurements.rbegin()->all_measurement_types_excluded_total.number_stationary_processed;
	}
	for (long t = measurements.size()-1; t >= 0; t--){
		for (ns_worm_movement_measurement_summary_timepoint_list::iterator p = measurements[t].measurements.begin(); p != measurements[t].measurements.end(); ++p){
			ns_worm_movement_summary_series_maximum_info_list::iterator minimum(minimums.find(p->first));
			if (minimum == minimums.end()){
				minimum = minimums.insert(ns_worm_movement_summary_series_maximum_info_list::value_type(p->first,ns_max_info())).first;
				minimum->second.not_translating_max_value = p->second.number_not_translating;
				minimum->second.survival_max_value = p->second.number_stationary_processed;
			}
			else{
				minimum->second.update_min(p->second.number_stationary_processed,p->second.number_not_translating,t);
			}
			p->second.number_not_translating = minimum->second.not_translating_max_value;
			p->second.survival = minimum->second.survival_max_value;
		}
		total_minimums.update_min(measurements[t].all_measurement_types_total.number_stationary_processed,
									measurements[t].all_measurement_types_total.number_not_translating,
									t);
		total_censored_minimums.update_min(measurements[t].all_measurement_types_censored_total.number_stationary_processed,
									measurements[t].all_measurement_types_censored_total.number_not_translating,
									t);
		total_excluded_minimums.update_min(measurements[t].all_measurement_types_excluded_total.number_stationary_processed,
									measurements[t].all_measurement_types_excluded_total.number_not_translating,
									t);
		measurements[t].all_measurement_types_total.number_not_translating = total_minimums.not_translating_max_value;
		measurements[t].all_measurement_types_total.survival = total_minimums.survival_max_value;
		measurements[t].all_measurement_types_censored_total.number_not_translating = total_censored_minimums.not_translating_max_value;
		measurements[t].all_measurement_types_censored_total.survival = total_censored_minimums.survival_max_value;
		measurements[t].all_measurement_types_excluded_total.number_not_translating = total_excluded_minimums.not_translating_max_value;
		measurements[t].all_measurement_types_excluded_total.survival = total_excluded_minimums.survival_max_value;
	}

	calculate_maximums();

	
}*/

void ns_worm_movement_summary_series::generate_censoring_annotations(const ns_region_metadata & m,ns_death_time_annotation_set & set){
	
	//calculate_survival();
	
	//exclude_stray_fast_moving_observations();
	//unsigned long index_of_last_death = calculate_missing_moving_worms(missing_worm_return_strategy);
	if (measurements.size() == 1)
		throw ns_ex("Generating censoring annotation for summary series with only one observation!");
	if (!measurements.empty() && estimated_number_of_worms_alive_at_measurement_end > 0){
		set.add(
			ns_death_time_annotation(ns_moving_worm_disappearance,
			0,m.region_id,
			ns_death_time_annotation_time_interval((measurements.rbegin()+1)->time,measurements.rbegin()->time),
			ns_vector_2i(0,0),
			ns_vector_2i(0,0),
			ns_death_time_annotation::ns_censored_at_end_of_experiment,
			ns_death_time_annotation_event_count(estimated_number_of_worms_alive_at_measurement_end,0),ns_current_time(),
			ns_death_time_annotation::ns_lifespan_machine,
			ns_death_time_annotation::ns_single_worm,
			ns_stationary_path_id(),false,
			"Apparently Moving at end of experiment",
			1,
			this->multiworm_cluster_censoring_strategy,
			missing_worm_return_strategy)
		);
	}

	for (unsigned int i = 1; i < measurements.size(); i++){
		//const int multiworm_censoring(measurements[i].censoring_data.cumulative_number_censored_from_multiworm_clusters
		//							  - measurements[i-1].censoring_data.cumulative_number_censored_from_multiworm_clusters);
		
		const int missing_censored (measurements[i].all_measurement_types_total.number_permanantly_lost - 
			(long)measurements[i-1].all_measurement_types_total.number_permanantly_lost);

		//const int estimated_multiworm_deaths(measurements[i].all_measurement_types_total.estimated_multiple_deaths.cumulative - 
		//									 measurements[i-1].all_measurement_types_total.estimated_multiple_deaths.cumulative);

		/*
		//These censoring events are added by direct interpretation of multi-worm deaths by ns_multiple_worm_cluster_death_annotation_handler()
		if (multiworm_cluster_censoring_strategy != ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor && 
			multiworm_censoring > 0){
			ns_death_time_annotation::ns_exclusion_type censoring_type 
									= ns_death_time_annotation::ns_multiworm_censored;
			set.add(
					 ns_death_time_annotation(ns_moving_worm_disappearance,
					 0,m.region_id,
					 ns_death_time_annotation_time_interval(measurements[i-1].time,measurements[i].time),
					 ns_vector_2i(0,0),
					 ns_vector_2i(0,0),
					 censoring_type,
					 ns_death_time_annotation_event_count(multiworm_censoring,0),ns_current_time(),
					 ns_death_time_annotation::ns_lifespan_machine,
					 ns_death_time_annotation::ns_single_worm,
					 ns_stationary_path_id(),false,
					 "Worms entered multiworm cluster",
					 1,
					 this->multiworm_cluster_censoring_strategy,
					 missing_worm_return_strategy)
				);
		}*/
	
		if (missing_censored  > 0)
			set.add(
					 ns_death_time_annotation(ns_moving_worm_disappearance,
					 0,m.region_id,
					 ns_death_time_annotation_time_interval(measurements[i-1].time,measurements[i].time),
					 ns_vector_2i(0,0),
					 ns_vector_2i(0,0),
					 ns_death_time_annotation::ns_missing_censored,
					 ns_death_time_annotation_event_count(missing_censored,0),ns_current_time(),
					 ns_death_time_annotation::ns_lifespan_machine,
					 ns_death_time_annotation::ns_single_worm,
					 ns_stationary_path_id(),false,
					 "Worm Went Missing",
					 1,
					 this->multiworm_cluster_censoring_strategy,
					 missing_worm_return_strategy)
				);
	}
}

template<class T>
class ns_random_exclusion_set{
public:
	virtual void reserve(unsigned long i)=0;
	virtual void add(const T & t)=0;
	virtual void remove_element_at_random()=0;
	virtual unsigned long size()const=0;
	virtual void get_values(std::vector<T> &v)const = 0;
};

template<class T>
class ns_fast_random_exclusion_set : public ns_random_exclusion_set<T>{
public:
	void reserve(unsigned long i){values.reserve(i);}
	void add(const T & t){values.push_back(t);}
	void remove_element_at_random(){
		if (values.size() == 0) throw ns_ex("ns_fast_random_exclusion_set()::Removing from an empty set!");
		remove_specific_element(random_element_index());
	}
	void remove_specific_element(const int i){
		if (i >= values.size()) throw ns_ex("ns_fast_random_exclusion_set()::Removing invalid entry!");
		values[i] = *values.rbegin();
		values.pop_back();
	}
	unsigned long random_element_index(){
		return (int)(values.size()*(rand()/(RAND_MAX + 1.0)));
	}
	unsigned long size()const{return values.size();}
	void get_values(std::vector<T> &v)const{
		v.clear();
		v.insert(v.end(),values.begin(),values.end());
	}
	std::vector<T> values;
};

template<class T>
class ns_two_stage_fast_random_exclusion_set : public ns_random_exclusion_set<T>{
public:
	ns_two_stage_fast_random_exclusion_set(const T cutoff_duration_):cutoff_duration(cutoff_duration_),most_recent_time(0){}
	ns_fast_random_exclusion_set<T> recent,
								 old;

	void reserve(unsigned long i){}
	void add(const T & t){
		if (most_recent_time > t)
			throw ns_ex("ns_two_stage_fast_random_exclusion_set()::Elements must be added in increasing temporal order!");
		most_recent_time = t;
		const T cutoff_time(most_recent_time - cutoff_duration);
		//update old and new
		for (unsigned int i = 0; i < recent.values.size(); i++){
			if (recent.values[i] < cutoff_time){
				old.add(recent.values[i]);
				recent.remove_specific_element(i);
			}
		}
		recent.add(t);
	}
	void remove_element_at_random(){
		if (!recent.values.empty())
			recent.remove_element_at_random();
		else
		old.remove_element_at_random();
	}
	unsigned long size() const{
		return old.size() + recent.size();
	}	
	void get_values(std::vector<T> &v)const{
		v.clear();
		v.insert(v.end(),recent.values.begin(),recent.values.end());
		v.insert(v.end(),old.values.begin(),old.values.end());
	}

private:
	T cutoff_duration;
	T most_recent_time;
};

struct ns_time_series_region_data{

	ns_worm_movement_summary_series::ns_timepoints_sorted_by_time measurements;
	ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator last_death_timepoint;
	unsigned long number_missing_at_end,
		estimated_total_number_of_worms;

	void calculate_cumulatives(){
		ns_worm_movement_measurement_summary_timepoint_data sum;
		ns_worm_movement_measurement_summary_timepoint_data excluded_sum;

		unsigned long cumulative_censored(0);

		for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = measurements.begin(); t != measurements.end(); t++){
			t->second.calculate_all_measurement_types_total();

			sum.singleton_deaths.cumulative += t->second.all_measurement_types_total.singleton_deaths.observed;
			sum.observed_multiple_deaths.cumulative += t->second.all_measurement_types_total.observed_multiple_deaths.observed;
			sum.estimated_multiple_deaths.cumulative += t->second.all_measurement_types_total.estimated_multiple_deaths.observed;

			t->second.all_measurement_types_total.singleton_deaths.cumulative			=	sum.singleton_deaths.cumulative;
			t->second.all_measurement_types_total.observed_multiple_deaths.cumulative   =	sum.observed_multiple_deaths.cumulative;
			t->second.all_measurement_types_total.estimated_multiple_deaths.cumulative  =	sum.estimated_multiple_deaths.cumulative;
			

			excluded_sum.singleton_deaths.cumulative += t->second.all_measurement_types_excluded_total.singleton_deaths.observed;
			excluded_sum.observed_multiple_deaths.cumulative += t->second.all_measurement_types_excluded_total.observed_multiple_deaths.observed;
			excluded_sum.estimated_multiple_deaths.cumulative += t->second.all_measurement_types_excluded_total.estimated_multiple_deaths.observed;

			t->second.all_measurement_types_excluded_total.singleton_deaths.cumulative			=	excluded_sum.singleton_deaths.cumulative;
			t->second.all_measurement_types_excluded_total.observed_multiple_deaths.cumulative   =	excluded_sum.observed_multiple_deaths.cumulative;
			t->second.all_measurement_types_excluded_total.estimated_multiple_deaths.cumulative  =	excluded_sum.estimated_multiple_deaths.cumulative;
			

			cumulative_censored+=t->second.censoring_data.number_newly_censored_from_multiworm_clusters;
			t->second.censoring_data.cumulative_number_censored_from_multiworm_clusters = cumulative_censored;
		}
	//	cerr << "Found " << sum.singleton_deaths.cumulative << " singleton deaths.\n";
	//	cerr << " Found " << sum.observed_multiple_deaths.cumulative << " observed multiples; " << sum.estimated_multiple_deaths.cumulative << " estimated multiples\n";
	//	cerr << " Found " << excluded_sum.observed_multiple_deaths.cumulative << " excluded observed multiples; " << excluded_sum.estimated_multiple_deaths.cumulative << " excluded estimated multiples\n";

	}
	void calculate_missing_moving_worms(const ns_death_time_annotation::ns_multiworm_censoring_strategy & multiworm_censoring_strategy,const ns_death_time_annotation::ns_missing_worm_return_strategy & missing_worm_return_strategy){

		if (measurements.size() == 0){
			last_death_timepoint = measurements.begin();
			return;
		}

		//find the maximum number of worms seen on the plate
		unsigned long maximum_number_of_worms_seen_on_plate(0);
		
		last_death_timepoint = measurements.end();
		long death_count_at_90th_percentile;
		//if (missing_worm_return_strategy != ns_death_time_annotation::ns_censoring_minimize_missing_times)
		//	death_count_at_90th_percentile = measurements.rbegin()->second.all_measurement_types_total.cumulative_deaths;
		death_count_at_90th_percentile = ((measurements.rbegin()->second.all_measurement_types_total.number_cumulative_deaths()*95)/100);


		for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = measurements.begin(); t != measurements.end(); t++){
			long s(t->second.all_measurement_types_total.number_alive()+
				t->second.all_measurement_types_total.number_cumulative_deaths()+
				t->second.censoring_data.cumulative_number_censored_from_multiworm_clusters);
			if (s > maximum_number_of_worms_seen_on_plate)
				maximum_number_of_worms_seen_on_plate = s;
			if (last_death_timepoint == measurements.end() && t->second.all_measurement_types_total.number_cumulative_deaths() >= death_count_at_90th_percentile){
				last_death_timepoint = t;
			}
		}
		
		estimated_total_number_of_worms = maximum_number_of_worms_seen_on_plate;

		//calculate the number of missing worms
		if (multiworm_censoring_strategy == ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor){
			//ignore multiple worms and assume they are "missing"
			for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = measurements.begin(); t != measurements.end(); t++){
				long s(t->second.all_measurement_types_total.number_alive() + 
					   t->second.all_measurement_types_total.number_cumulative_deaths());
					   //t->second.censoring_data.cumulative_number_censored_from_multiworm_clusters);

				t->second.all_measurement_types_total.number_of_missing_animals = 
								maximum_number_of_worms_seen_on_plate - s;
			}
		}
		else{
			for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = measurements.begin(); t != measurements.end(); t++){
				long s(t->second.all_measurement_types_total.number_alive() + 
					t->second.all_measurement_types_total.number_cumulative_deaths()
					+ t->second.censoring_data.cumulative_number_censored_from_multiworm_clusters);

				t->second.all_measurement_types_total.number_of_missing_animals = 
								maximum_number_of_worms_seen_on_plate - s;
			}
		}

		//calculate the number dead at the end of the experiment (the minimum number of missing animals
		//after the last death was observed)
	
		//int death_buffer(10);
		if (last_death_timepoint != measurements.end()){
			std::vector<unsigned long> number_missing_after_death;
			for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = last_death_timepoint; t != measurements.end(); t++){
				number_missing_after_death.push_back(t->second.all_measurement_types_total.number_of_missing_animals);
			}
			std::sort(number_missing_after_death.begin(),number_missing_after_death.end());
			number_missing_at_end = number_missing_after_death[number_missing_after_death.size()/2];
		}
		else{
			number_missing_at_end = 0;
		}
		
		for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = measurements.begin(); t != measurements.end(); t++)
			t->second.all_measurement_types_total.number_of_missing_animals_smoothed = t->second.all_measurement_types_total.number_of_missing_animals;
		
		//calculate running minimum of number missin
		{
			//set all the number of missing animals as being censored at the time of the last death
			for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = last_death_timepoint; t != measurements.end(); t++)
				t->second.all_measurement_types_total.number_permanantly_lost = number_missing_at_end;
				//measurements[index_of_last_death-death_buffer].all_measurement_types_total.number_of_missing_animals;	
		//	;
		//	}

			if (missing_worm_return_strategy == ns_death_time_annotation::ns_censoring_minimize_missing_times){
				if (last_death_timepoint != measurements.begin()){
					//if the last death timepoint is the first timepoint then we don't have to do anything.
					int cur_min(number_missing_at_end);
					ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator s(last_death_timepoint);
					s--;
					for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = s;;t--){
						if (t->second.all_measurement_types_total.number_of_missing_animals < cur_min)
							cur_min = t->second.all_measurement_types_total.number_of_missing_animals;
						t->second.all_measurement_types_total.number_permanantly_lost = cur_min;

						if (t == measurements.begin())
							break;
					}
				}
			}
			else{
				/*
			//	std::vector<unsigned long> missing_smoothed(measurements.size());
					unsigned long kernel_half_width(8);
					if (missing_worm_return_strategy == ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_only_large_missing_times &&
						measurements.size() > 2*kernel_half_width+2){
					for (unsigned int i = kernel_half_width; i < measurements.size()-kernel_half_width; i++){
						//unsigned long sum(0);
						//for (unsigned int k = i-kernel_half_width; k <= i+kernel_half_width; k++)
						//	sum+=measurements[k].all_measurement_types_total.number_of_missing_animals;
						//missing_smoothed[i] = sum/(2*kernel_half_width+1);
						unsigned long mmin(measurements[i].all_measurement_types_total.number_of_missing_animals);
						for (unsigned int k = i-kernel_half_width; k <= i+kernel_half_width; k++)
							if (measurements[k].all_measurement_types_total.number_of_missing_animals < mmin)
								mmin = measurements[k].all_measurement_types_total.number_of_missing_animals;
						measurements[i].all_measurement_types_total.number_of_missing_animals_smoothed = mmin;
					}
					for (unsigned int i = 0; i < kernel_half_width; i++)
						measurements[i].all_measurement_types_total.number_of_missing_animals_smoothed = measurements[kernel_half_width].all_measurement_types_total.number_of_missing_animals_smoothed;
					for (unsigned int i = measurements.size()-kernel_half_width; i < measurements.size(); i++)
						measurements[i].all_measurement_types_total.number_of_missing_animals_smoothed =measurements[measurements.size()-kernel_half_width-1].all_measurement_types_total.number_of_missing_animals_smoothed;
				}
				*/
				
				
				
				ns_fast_random_exclusion_set<unsigned long> single_stage_missing_times;
				ns_two_stage_fast_random_exclusion_set<unsigned long> two_stage_missing_times(60*60*6);
				ns_random_exclusion_set<unsigned long> * missing_times;
				if (missing_worm_return_strategy == ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_missing_times)
					missing_times = &single_stage_missing_times;
				else  missing_times = &two_stage_missing_times;

				for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = measurements.begin(); t !=last_death_timepoint; t++){

					long number_newly_missing;
					if (t == measurements.begin())
						number_newly_missing = (long)t->second.all_measurement_types_total.number_of_missing_animals;
					else{
						ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t_prev(t);
						t_prev--;
						number_newly_missing = ((long)t->second.all_measurement_types_total.number_of_missing_animals 
												 - (long)(t_prev->second.all_measurement_types_total.number_of_missing_animals));
					}
			//		if (number_newly_missing > 100)
			//			cerr <<"IKES";

					unsigned long cur_t(t->second.time);
					if (number_newly_missing==0)
						continue;
					if (number_newly_missing>0)
						for (unsigned long j = 0; j < number_newly_missing; j++)
							missing_times->add(cur_t);
					if(number_newly_missing<0){
						for (unsigned long j = 0; j < (unsigned long)(-number_newly_missing); j++){
							if (missing_times->size() == 0)
								break;
							missing_times->remove_element_at_random();
						}
					}
				}
				while(missing_times->size() > number_missing_at_end)
					missing_times->remove_element_at_random();
				std::vector<unsigned long > values;
				missing_times->get_values(values);

				std::sort(values.begin(),values.end());
				ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator cur_measurement_timepoint = measurements.begin();
				unsigned long current_total_missing(0);
				for (unsigned int i = 0; i < values.size(); i++){
					const unsigned long t(values[i]);
					while (cur_measurement_timepoint->second.time < t){
						cur_measurement_timepoint->second.all_measurement_types_total.number_permanantly_lost = current_total_missing;
						cur_measurement_timepoint++;
						if (cur_measurement_timepoint == last_death_timepoint)
							throw ns_ex("Overflowed looking for missing times!");
					}
					if (cur_measurement_timepoint->second.time != t)
						throw ns_ex("Error searching for t!");
					current_total_missing++;
					cur_measurement_timepoint->second.all_measurement_types_total.number_permanantly_lost = current_total_missing;
				}
				for (; cur_measurement_timepoint != last_death_timepoint; cur_measurement_timepoint++)
					cur_measurement_timepoint->second.all_measurement_types_total.number_permanantly_lost = current_total_missing;
			}
		}
		//calculate the number missing discounting the animals still alve at the end of the experiment
	/*	if (index_of_last_death < death_buffer)
			estimated_number_of_worms_alive_at_measurement_end = 0;
		else {
			estimated_number_of_worms_alive_at_measurement_end = number_missing_at_end - 
																	 measurements[index_of_last_death-death_buffer].all_measurement_types_total.number_permanantly_lost;
			for (unsigned long i = index_of_last_death-death_buffer+1; i < measurements.size(); i++){
				long m(measurements[index_of_last_death-death_buffer].all_measurement_types_total.number_permanantly_lost);

				measurements[i].all_measurement_types_total.number_permanantly_lost_before_end_of_experiment = m;
			}
		}
	

		return index_of_last_death;
		*/
	/*	if (measurements.size() == 0)
			return;

		measurements[0].all_measurement_types_total.number_of_moving_animal_dissapearances = 0;
		measurements[0].all_measurement_types_total.net_number_of_animals_lost = 0;
		for (unsigned int i = 1; i < measurements.size(); i++){
			long number_missing( measurements[i-1].all_measurement_types_total.total_moving() -
								 measurements[i].all_measurement_types_total.total_moving());

		}
			
		for (unsigned int i = 1; i < measurements.size(); i++){
			measurements[i].all_measurement_types_total.number_of_moving_animal_dissapearances = (
																	   measurements[i-1].all_measurement_types_total.total_moving() -
																	   (
																		   measurements[i-1].all_measurement_types_excluded_total.total_moving()
															
																	   )
																	)
																	-
																	(
																	measurements[i].all_measurement_types_total.total_moving() -
																	   (
																		   measurements[i].all_measurement_types_excluded_total.total_moving()
																		  )
																	)

																   - measurements[i].all_measurement_types_total.number_of_deaths;
			measurements[i].all_measurement_types_total.net_number_of_animals_lost = measurements[i].all_measurement_types_total.number_of_moving_animal_dissapearances + measurements[i-1].all_measurement_types_total.net_number_of_animals_lost;
		}
	
		unsigned long end_buffer(6);
		if (measurements.size() < end_buffer)
			end_buffer = 0;
		long cur_min(measurements[measurements.size()-end_buffer].all_measurement_types_total.net_number_of_animals_lost);
		for (long i = (long)measurements.size()-end_buffer-1; i >= 0; i--){
			if (measurements[i].all_measurement_types_total.net_number_of_animals_lost < cur_min)
				cur_min = measurements[i].all_measurement_types_total.net_number_of_animals_lost;
			measurements[i].all_measurement_types_total.processed_number_of_animals_lost = cur_min;
		}
		for (unsigned long i = 0; i < measurements.size()-end_buffer; i++)
			measurements[i].all_measurement_types_total.processed_number_of_animals_lost -= cur_min;
		for (unsigned long i = measurements.size()-end_buffer; i < measurements.size(); i++)
			measurements[i].all_measurement_types_total.processed_number_of_animals_lost = measurements[measurements.size()-end_buffer-1].all_measurement_types_total.processed_number_of_animals_lost;

		*/
		}
		
	void exclude_stray_fast_moving_observations(){
		const float fraction_required_fast(.5);
		unsigned long number_greater_than_zero(0);
		if (measurements.size() <= 10)
			return;
		
		ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::reverse_iterator t(measurements.rbegin());
		for (long i = 0; i < 10; i++){
			if (t->second.all_measurement_types_total.number_moving_fast > 0)
				number_greater_than_zero++;
			t++;
		}
		
		//still lots of animals alive.  don't do any denoising.
		if (fraction_required_fast < number_greater_than_zero/10.0)
			return;
		ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::reverse_iterator start_of_no_fast_moving(measurements.rbegin());
		ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::reverse_iterator t_minus_10(measurements.rbegin());
		for (; t!= measurements.rend(); t++,t_minus_10++){
			if (t->second.all_measurement_types_total.number_moving_fast > 0)
				number_greater_than_zero++;

			if (t_minus_10->second.all_measurement_types_total.number_moving_fast > 0)
				number_greater_than_zero--;
			if(fraction_required_fast < number_greater_than_zero/10.0){
				start_of_no_fast_moving = t;
				break;
			}
		}
		start_of_no_fast_moving++; //so we can use != start_of_no_fast_moving as an end point
		//censor random fast moving noise
		for (t = measurements.rbegin(); t != start_of_no_fast_moving; t++){
			t->second.all_measurement_types_excluded_total.number_moving_fast += t->second.all_measurement_types_total.number_moving_fast;
			t->second.all_measurement_types_total.number_moving_fast = 0;
		}
	}
};

struct ns_time_series_for_each_region{
	typedef std::map<unsigned long,ns_time_series_region_data> ns_time_series;
	ns_time_series time_series;
	void calculate_totals_and_cumulatives(const ns_death_time_annotation::ns_multiworm_censoring_strategy & multiworm_censoring_strategy,
										  const ns_death_time_annotation::ns_missing_worm_return_strategy & missing_worm_return_strategy){

		for (ns_time_series::iterator p = time_series.begin(); p != time_series.end(); p++){
			p->second.calculate_cumulatives();
			p->second.exclude_stray_fast_moving_observations();
			p->second.calculate_missing_moving_worms(multiworm_censoring_strategy,missing_worm_return_strategy);
		}
	}
	
};

template<class T>
void ns_assign_if_greater(T & a, const T & b){
	if (a < b)
		a = b;
}
template<class T>
void ns_assign_if_lesser(T & a, const T & b){
	if (a > b)
		a = b;
}

void ns_update_max_and_min_from_timepoint(ns_worm_movement_measurement_summary_timepoint_data & data,ns_worm_movemement_measurement_summary_statistics & stats){
		double total =	data.total_animals_observed();
		//data.number_total = total;

		ns_assign_if_greater(stats.maximum_count_for_movement_state[(int)ns_movement_total],total);
			
		ns_assign_if_greater(stats.maximum_count_for_movement_state[(int)ns_movement_fast] , (double)data.number_moving_fast);
		ns_assign_if_greater(stats.maximum_count_for_movement_state[(int)ns_movement_slow] , (double)data.number_moving_slow);
		ns_assign_if_greater(stats.maximum_count_for_movement_state[(int)ns_movement_posture] ,(double)data.number_changing_posture);
		ns_assign_if_greater(stats.maximum_count_for_movement_state[(int)ns_movement_stationary] , (double)data.number_stationary);

		ns_assign_if_lesser(stats.minimum_count_for_movement_state[(int)ns_movement_total],(double)total);
		ns_assign_if_lesser(stats.minimum_count_for_movement_state[(int)ns_movement_fast] , (double)data.number_moving_fast);
		ns_assign_if_lesser(stats.minimum_count_for_movement_state[(int)ns_movement_slow] , (double)data.number_moving_slow);
		ns_assign_if_lesser(stats.minimum_count_for_movement_state[(int)ns_movement_posture] , (double)data.number_changing_posture);
		ns_assign_if_lesser(stats.minimum_count_for_movement_state[(int)ns_movement_stationary] , (double)data.number_stationary);
}

void ns_worm_movement_summary_series::calculate_maximums(){
	if (measurements.size() == 0)
		return;

	{
		unsigned long s(total_summary_statistics.maximum_count_for_movement_state.size());
		total_summary_statistics.maximum_count_for_movement_state.resize(s,0);
		total_summary_statistics.minimum_count_for_movement_state.resize(s,DBL_MAX);
		//total_censored_summary_statistics.maximum_count_for_movement_state.resize(s,0);
		//total_censored_summary_statistics.minimum_count_for_movement_state.resize(s,DBL_MAX);
		total_excluded_summary_statistics.maximum_count_for_movement_state.resize(s,0);
		total_excluded_summary_statistics.minimum_count_for_movement_state.resize(s,DBL_MAX);
	}
		
	//calculate maximums 
	for (unsigned int i = 0; i < measurements.size(); i++){
		ns_update_max_and_min_from_timepoint(measurements[i].all_measurement_types_total,total_summary_statistics);
//		ns_update_max_and_min_from_timepoint(measurements[i].all_measurement_types_censored_total,total_censored_summary_statistics);
		ns_update_max_and_min_from_timepoint(measurements[i].all_measurement_types_excluded_total,total_excluded_summary_statistics);
		for (ns_worm_movement_measurement_summary_timepoint_list::iterator p = measurements[i].measurements.begin(); p != measurements[i].measurements.end(); p++){
			ns_worm_movemement_measurement_summary_statistics_list::iterator q = movement_summary_statistics.find(p->first);
			if (q == movement_summary_statistics.end()){
				q = movement_summary_statistics.insert(ns_worm_movemement_measurement_summary_statistics_list::value_type(p->first,ns_worm_movemement_measurement_summary_statistics_list::mapped_type())).first;
				q->second.maximum_count_for_movement_state.resize(total_summary_statistics.maximum_count_for_movement_state.size(),0);
				q->second.minimum_count_for_movement_state.resize(total_summary_statistics.maximum_count_for_movement_state.size(),DBL_MAX);
			}
			ns_update_max_and_min_from_timepoint(p->second,q->second);
		}
	}

	unsigned long s(total_summary_statistics.maximum_count_for_movement_state.size());
	for (unsigned int i = 0; i < s; i++){
		if (total_summary_statistics.minimum_count_for_movement_state[i] == DBL_MAX)
			total_summary_statistics.minimum_count_for_movement_state[i]  = 0;
		if (total_summary_statistics.minimum_count_for_movement_state[i]  == DBL_MAX)
			total_summary_statistics.minimum_count_for_movement_state[i]  = 0;
		if (total_summary_statistics.minimum_count_for_movement_state[i]  == DBL_MAX)
			total_summary_statistics.minimum_count_for_movement_state[i]  = 0;
	}
}

void ns_worm_movement_summary_series::to_file(const ns_region_metadata & metadata,std::ostream & o) const{
	for (unsigned int i = 0; i < measurements.size(); i++){
		measurements[i].to_file(metadata,o);
	}
}
/*
void ns_worm_movement_summary_series::from_file(std::istream & i){
	std::string in;
	getline(i,in,'\n');
	measurements.reserve(100);
	while(true){
		std::string::size_type s(measurements.size());
		measurements.resize(s+1);
		if (!measurements[s].from_file(i)){
			measurements.resize(s);
			break;
		}
	}
}
*/

void ns_worm_movement_measurement_summary::out_header(std::ostream & o){
	o << "Strain,Genotype,Strain Condition 1,Strain Condition 2,Genotype::Condition,Plate Name,Time (Seconds), Time (Hours),Time (Days),";
	o << "Total Animals Observed,";
	ns_worm_movement_measurement_summary_timepoint_data::out_header("",o,',');
	o << "Total Excluded Animals,";
	ns_worm_movement_measurement_summary_timepoint_data::out_header("Excluded",o,',');
	ns_worm_movement_censoring_data::out_header(o,'\n');
}
void ns_worm_movement_measurement_summary::to_file(const ns_region_metadata & metadata,ostream & o) const{
	o << metadata.strain << "," << metadata.genotype << ","
	<< metadata.strain_condition_1 << "," << metadata.strain_condition_2 << "," 
	<<((metadata.genotype.size()>0)?metadata.genotype:metadata.strain) 
		<< ((metadata.strain_condition_1.size()>0)?(string("::") + metadata.strain_condition_1):"")
		<< ((metadata.strain_condition_2.size()>0)?(string("::") + metadata.strain_condition_2):"")
		<< "," << metadata.plate_name()
	<< "," << time
	<< "," << time/(60.0*60)
	<< "," << time/(60.0*60*24) << ",";
	o << all_measurement_types_total.number_alive() + all_measurement_types_total.number_cumulative_deaths() 
		+ censoring_data.cumulative_number_censored_from_multiworm_clusters << ",";
	all_measurement_types_total.out_data(o,',');

	o << all_measurement_types_excluded_total.number_alive() + all_measurement_types_excluded_total.number_cumulative_deaths() << ",";
	all_measurement_types_excluded_total.out_data(o,',');
	censoring_data.out_data(o,'\n');
}
//bool ns_worm_movement_measurement_summary::from_file(istream & i){
//	throw ns_ex("Depreciated");
//	return true;
//}

std::string ns_movement_data_source_type::type_string(const ns_movement_data_source_type::type & t){
	switch (t){
		case ns_time_path_analysis_data: return "time_path";
		case ns_time_path_image_analysis_data: return "time_path_image";
		case ns_triplet_data: return "triplet";
		case ns_triplet_interpolated_data: return "triplet_interpolated";
	}
	throw ns_ex("ns_movement_series_summary_producer::type_string()::Unknown source type: ") << (unsigned long)t;
}



void ns_worm_movement_description_series::output_position_visualization_csv(std::ostream & o) const{
	ns_time_path_solution::output_visualization_csv_header(o);
	//output unassigned points
	for (unsigned int i = 0; i< timepoints.size(); i++){
		for (unsigned int j = 0; j < timepoints[i].worms.size(); j++){
			ns_time_path_solution::output_visualization_csv_data(o,
				(timepoints[i].time - timepoints[0].time)/60.0/60.0/24.0,
				timepoints[i].time,
				timepoints[i].worms[j].region_position + timepoints[i].worms[j].region_size/2,
				timepoints[i].worms[j].region_size,
				timepoints[i].worms[j].path_id, 0,
				0,(timepoints[i].worms[j].movement == ns_movement_machine_excluded || 
				timepoints[i].worms[j].movement == ns_movement_by_hand_excluded),0,
				false,false,
				0,
				timepoints[i].worms[j].movement
				);
		}
	}
}



bool operator < (const ns_worm_movement_measurement_summary_timepoint_type & a, const ns_worm_movement_measurement_summary_timepoint_type & b){
	if (a.exclusion_type != b.exclusion_type)
		return (int)a.exclusion_type < (int)b.exclusion_type;
	if (a.number_of_worms_in_cluster_by_machine_annotation != b.number_of_worms_in_cluster_by_machine_annotation)
		return a.number_of_worms_in_cluster_by_machine_annotation < b.number_of_worms_in_cluster_by_machine_annotation;
	return a.number_of_worms_in_cluster_by_hand_annotation < b.number_of_worms_in_cluster_by_hand_annotation;
}
void ns_worm_movement_measurement_summary_timepoint_data::out_header(const std::string & name, std::ostream & o, const char separator){
		o 	<< name << " Total Observed Alive,"
			<< name << " Moving Fast," 
			<< name << " Moving Slow," 
			<< name << " Changing Posture,"
			<< name << " Stationary," 
			<< name << " Death Posture Relaxing, " 
			<< name << " Cumulative Deaths,"
			<< name << " Cumulative Observed Singletons Dead, "
			<< name << " Cumulative Observed Multiples Dead, "
			<< name << " Cumulative Estimated Multiples Dead, "
			<< name << " Stationary Animal Disapearances, " 
			<< name << " Number of Missing Animals,"
			<< name << " Number of Missing Animals (Smoothed),"
			<< name << " Cumulative Number Permanantly Lost,"
			<< name << " Cumulative Number Permanantly Lost Before The Experiment Ended"
			<< separator;
}
void ns_worm_movement_measurement_summary_timepoint_data::out_data(std::ostream & o, const char & separator) const{
	o << number_alive() << ","
		<< number_moving_fast << ","
		<< number_moving_slow << ","
		<< number_changing_posture << ","
		<< number_stationary << ","	
		<< number_death_posture_relaxing << ","	
		<< number_cumulative_deaths() << ","
		<< singleton_deaths.cumulative << ","
		<< observed_multiple_deaths.cumulative << ","
		<< estimated_multiple_deaths.cumulative << ","
		<< number_of_stationary_worm_dissapearances << ","	
		<< number_of_missing_animals << ","
		<< number_of_missing_animals_smoothed << ","
		<< number_permanantly_lost << ","
		<< number_permanantly_lost_before_end_of_experiment
		<< separator;
}
void ns_worm_movement_measurement_summary_timepoint_data::add(const ns_worm_movement_measurement_summary_timepoint_data & s){
						
	number_moving_fast					+=	s.number_moving_fast;					
	number_moving_slow					+=	s.number_moving_slow;					
	number_changing_posture				+=	s.number_changing_posture;				
	number_stationary					+=	s.number_stationary;		
	number_of_stationary_worm_dissapearances += s.number_of_stationary_worm_dissapearances;
	number_death_posture_relaxing		+=	s.number_death_posture_relaxing;			
	number_of_missing_animals			+=	s.number_of_missing_animals;		
	number_permanantly_lost	+=	s.number_permanantly_lost;	
	number_permanantly_lost_before_end_of_experiment += s.number_permanantly_lost_before_end_of_experiment;
	number_of_missing_animals_smoothed += s.number_of_missing_animals_smoothed;
//	if (observed_multiple_deaths.observed != 0 || observed_multiple_deaths.cumulative !=0)
//		cerr << "WHA";
	singleton_deaths.add(s.singleton_deaths);
	observed_multiple_deaths.add(s.observed_multiple_deaths);
	estimated_multiple_deaths.add(s.estimated_multiple_deaths);

}

void ns_worm_movement_summary_series::output_censoring_diagnostic_header(std::ostream & o){
	ns_region_metadata::out_JMP_plate_identity_header_short(o);
	o << ",Censoring Scheme, Missing worm Return Scheme,Time of clump nucleation, Time of death of last animal in the clump,Number of worms in the clump marked by hand,"
		"Number of worms in the clump marked by machine, Number of worms in the clump used,"
		"Number of singleton deaths during clump existance,Number of Singletons At Risk At Time of Nucleation,"
		"Death Rate for Animals in Clump, Death Rate for Singletons,"
		"Interval Hazard Rate For Clump,Interval Hazard Rate for Singletons,"
		"Log Interval Hazard Rate for Clump,Log Interval Hazard Rate for Singletons,"
		"Average Survival Time after Entering Clump, Average Survival Time after Not Entering Clump\n";
}
void ns_worm_movement_summary_series::generate_censoring_diagnostic_file(const ns_region_metadata & metadata,std::ostream & o){
	if (this->multiworm_cluster_censoring_strategy == ns_death_time_annotation::ns_include_multiple_worm_cluster_deaths ||
		this->multiworm_cluster_censoring_strategy == ns_death_time_annotation::ns_not_applicable)
					throw ns_ex("Censoring Diagnostics can be computed when clumps have been ignored or included as deaths, "
									"but the answer will most likely be wrong, so we refuse to calculate it.");

	for (unsigned int i = 0; i < multiple_worm_clump_details.size(); i++){
		const ns_multiple_worm_description & d(multiple_worm_clump_details[i]);
		if (d.properties.is_excluded())continue;
		bool found_start_timepoint(false),found_stop_timepoint(false);
		unsigned long at_risk_general_population_at_nucleation(0);
		unsigned long number_of_singleton_deaths_during_clump_existance(0);
		unsigned long number_of_singletons_dead_at_nucleation(0);
		long duration (d.time_of_death_machine - (long)d.time_of_nucleation);
		if (duration <= 0)
			continue;
		unsigned long long average_singleton_death_time_after_clump_formation(0);
		unsigned long number_of_singleton_deaths_after_clump_formation(0);
		for (ns_measurement_timeseries::iterator p = measurements.begin(); p != measurements.end(); p++){
			if (p->time == d.time_of_nucleation){
				found_start_timepoint = true;
				at_risk_general_population_at_nucleation = estimated_total_number_of_worms 
					- p->all_measurement_types_total.number_permanantly_lost 
					- p->all_measurement_types_total.number_cumulative_deaths();
				number_of_singletons_dead_at_nucleation = p->all_measurement_types_total.number_cumulative_deaths();
			}
			if (p->time == d.time_of_death_machine){
				number_of_singleton_deaths_during_clump_existance = p->all_measurement_types_total.number_cumulative_deaths() - number_of_singletons_dead_at_nucleation;
				found_stop_timepoint = true;
			}
			if (found_start_timepoint && p != measurements.begin()){
				ns_measurement_timeseries::iterator p_minus_1(p);
				p_minus_1--;
				average_singleton_death_time_after_clump_formation += p->time*(unsigned long long)(p->all_measurement_types_total.number_cumulative_deaths()-p_minus_1->all_measurement_types_total.number_cumulative_deaths());
				number_of_singleton_deaths_after_clump_formation++;
				
			}
			
		}
		if (!found_start_timepoint)
			throw ns_ex("ns_worm_movement_summary_series::generate_censoring_diagnostic_file()::Could not find clump nucleation_time");
		if (!found_stop_timepoint)
				throw ns_ex("ns_worm_movement_summary_series::generate_censoring_diagnostic_file()::Cound not find clump death time");
		metadata.out_JMP_plate_identity_data_short(o);
		o << ","
			<< ns_death_time_annotation::multiworm_censoring_strategy_label(this->multiworm_cluster_censoring_strategy) << ","
			<< ns_death_time_annotation::missing_worm_return_strategy_label(this->missing_worm_return_strategy) << ","
			<< d.time_of_nucleation << "," << d.time_of_death_machine << "," 
			<< d.properties.number_of_worms_at_location_marked_by_hand << "," << d.properties.number_of_worms_at_location_marked_by_machine << ","
			<< d.properties.number_of_worms() << "," << number_of_singleton_deaths_during_clump_existance << "," << at_risk_general_population_at_nucleation << ",";
		
		if (duration <= 0 || at_risk_general_population_at_nucleation == 0)
			o << ",,,,,,";
		else o << 1.0 << "," << (number_of_singleton_deaths_during_clump_existance/(double)at_risk_general_population_at_nucleation) << ","
			   << 1.0/duration << "," << (number_of_singleton_deaths_during_clump_existance/(double)at_risk_general_population_at_nucleation)/(double)duration << ","
			   << -log((double)duration) << "," << log((double)number_of_singleton_deaths_during_clump_existance)-log((double)at_risk_general_population_at_nucleation)-log((double)duration) << ",";
		double avg((double)(average_singleton_death_time_after_clump_formation/(unsigned long long)number_of_singleton_deaths_after_clump_formation)
			+((double)(average_singleton_death_time_after_clump_formation%(unsigned long long)number_of_singleton_deaths_after_clump_formation))
				/(double)number_of_singleton_deaths_after_clump_formation);

		o << duration/2 << "," << avg;
		o << "\n";
	}
}

ns_worm_movement_measurement_summary_timepoint_type::ns_worm_movement_measurement_summary_timepoint_type(const ns_death_time_annotation & a){
	number_of_worms_in_cluster_by_hand_annotation = a.number_of_worms_at_location_marked_by_hand;
	number_of_worms_in_cluster_by_machine_annotation = a.number_of_worms_at_location_marked_by_machine;
	exclusion_type = a.excluded;
}

void ns_worm_movement_summary_series::from_death_time_annotations(const ns_death_time_annotation::ns_multiworm_censoring_strategy & censoring_strategy, const ns_animals_that_slow_but_do_not_die_handling_strategy & patial_path_strategy,const ns_death_time_annotation_compiler_region & region, ns_timepoints_sorted_by_time & all_timepoints,std::vector<ns_multiple_worm_description> & multiple_worm_clump_details){
	//add fast moving animals
	{
		for (unsigned int i = 0; i < region.fast_moving_animals.size(); i++){
			ns_worm_movement_measurement_summary_timepoint_type timepoint_type(region.fast_moving_animals[i]);
			ns_worm_movement_measurement_summary & measurement_summary(all_timepoints[region.fast_moving_animals[i].time.period_end]);
			ns_worm_movement_measurement_summary_timepoint_data & measurement(measurement_summary.measurements[timepoint_type]);
			measurement.number_moving_fast++;
		}
	}
	
	//the merge_multiworm and missing option doesn't differentiate		
	//between missing animals and animals
	//and animals in clusters, but saves us from having
	//to approximate when animals enter clusters.
	
	unsigned long number_of_multiworm_clusters_correctly_censored(0),
						number_stray_multiworm_clusters(0);
	
	for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q(region.locations.begin()); q != region.locations.end(); q++){
			

		ns_worm_movement_measurement_summary_timepoint_type timepoint_type(q->properties);

		//add slow, posture changing animals
		for (unsigned int i = 0; i < q->annotations.size(); i++){
			if(q->annotations[i].type !=  ns_slow_moving_worm_observed &&
				q->annotations[i].type !=  ns_posture_changing_worm_observed &&
				q->annotations[i].type != ns_fast_moving_worm_observed )
				continue;
			
			ns_worm_movement_measurement_summary & measurement_summary(all_timepoints[q->annotations[i].time.period_end]);
			ns_worm_movement_measurement_summary_timepoint_data & measurement(measurement_summary.measurements[timepoint_type]);
			switch(q->annotations[i].type){
				case ns_slow_moving_worm_observed: 
					if (patial_path_strategy == ns_force_to_fast_moving
						&& !q->annotations[i].animal_is_part_of_a_complete_trace)
						measurement.number_moving_fast++;
					else
						measurement.number_moving_slow++; break;
				case ns_posture_changing_worm_observed: 
					if (patial_path_strategy == ns_force_to_fast_moving
						&& !q->annotations[i].animal_is_part_of_a_complete_trace)
						measurement.number_moving_fast++;
					else
						measurement.number_changing_posture++; break;
				case ns_fast_moving_worm_observed:
					measurement.number_moving_fast++; break;
			}
		}

		//now we need to add deaths and and censoring events.
		ns_dying_animal_description_const d(q->generate_dying_animal_description_const(true));
		if (d.machine.death_annotation == 0 || d.machine.death_annotation->annotation_source != ns_death_time_annotation::ns_lifespan_machine ||
			d.machine.death_annotation->time.period_end_was_not_observed){
				
	//		if(q->properties.is_excluded())
	//			cerr << "WHA!";
			continue;
		}
			
	//	if(q->properties.is_excluded())
	//		cerr << "WHA!";
		{
			ns_multiple_worm_description clump;
			q->properties.transfer_sticky_properties(clump.properties);
			clump.time_of_death_machine = d.machine.death_annotation->time.period_end;
			if (d.machine.last_fast_movement_annotation != 0)
				clump.time_of_nucleation = d.machine.last_fast_movement_annotation->time.period_end;
			multiple_worm_clump_details.push_back(clump);
		}
		const unsigned long number_of_animals(timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all));
		if(number_of_animals <= 1){
			ns_death_time_annotation a(*d.machine.death_annotation);
			q->properties.transfer_sticky_properties(a);
	//		if (a.time.period_end - region.metadata.time_at_which_animals_had_zero_age <= 222828)
	//			std::cerr << a.region_info_id << " " << region.metadata.plate_name() << "\n";;
			ns_worm_movement_measurement_summary_timepoint_type t(a);
			ns_worm_movement_measurement_summary_timepoint_data & timepoint(all_timepoints[a.time.period_end].measurements[t]);
			timepoint.singleton_deaths.observed++;
		}
		else{
			ns_death_time_annotation_set set;
			ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(censoring_strategy,number_of_animals,q->properties,d,set);
			for (unsigned int i = 0; i < set.size(); i++){
				ns_worm_movement_measurement_summary_timepoint_type t(set[i]);
				ns_worm_movement_measurement_summary & timepoint(all_timepoints[set[i].time.period_end]);
				ns_worm_movement_measurement_summary_timepoint_data & measurement(timepoint.measurements[t]);
				const unsigned long new_number_of_animals(t.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all));
				if (set[i].is_censored())
					timepoint.censoring_data.number_newly_censored_from_multiworm_clusters+=new_number_of_animals;
				else if (set[i].event_observation_type == ns_death_time_annotation::ns_induced_multiple_worm_death)
					measurement.estimated_multiple_deaths.observed+=new_number_of_animals;
				else if (set[i].event_observation_type == ns_death_time_annotation::ns_observed_multiple_worm_death)
					measurement.observed_multiple_deaths.observed+=new_number_of_animals;
				else measurement.singleton_deaths.observed += new_number_of_animals;
			}
		}

		/*
		ns_worm_movement_measurement_summary *  observed_death_time(0);
		ns_worm_movement_measurement_summary_timepoint_data * observed_death_time_measurement(0);
		
		ns_worm_movement_measurement_summary *  last_observed_alive_time(0);
		ns_worm_movement_measurement_summary_timepoint_data * last_observed_alive_time_measurement(0);
		unsigned long death_observation_time(0),
					  last_alive_observation_time(0);

		if (d.machine.death_annotation != 0 && !d.machine.death_annotation->time.period_end_was_not_observed){
			death_observation_time = d.machine.death_annotation->time.period_end;
			observed_death_time = & all_timepoints[d.machine.death_annotation->time.period_end];
			observed_death_time_measurement = & observed_death_time->measurements[timepoint_type];

			ns_multiple_worm_description clump;
			q->properties.transfer_sticky_properties(clump.properties);
			clump.time_of_death_machine = d.machine.death_annotation->time.period_end;
			if (d.machine.last_fast_movement_annotation != 0)
				clump.time_of_nucleation = d.machine.last_fast_movement_annotation->time.period_end;
			multiple_worm_clump_details.push_back(clump);
		}
		else 
			continue;

		if (d.machine.last_fast_movement_annotation != 0 && !d.machine.last_fast_movement_annotation->time.period_end_was_not_observed){
			last_alive_observation_time = d.machine.last_fast_movement_annotation->time.period_end;
			last_observed_alive_time = & all_timepoints[last_alive_observation_time];
			last_observed_alive_time_measurement = & last_observed_alive_time->measurements[timepoint_type];
		}
	
		if(timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all) == 1){
			observed_death_time_measurement->singleton_deaths.observed++;
		}
		else{
			switch(censoring_strategy){
				case ns_death_time_annotation::ns_include_multiple_worm_cluster_deaths:
					observed_death_time_measurement->observed_multiple_deaths.observed+=timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all);
					break;
				case ns_death_time_annotation::ns_none:
					observed_death_time_measurement->observed_multiple_deaths.observed+=timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_ignore_by_hand_multiworm_annotations);
					break;
				case ns_death_time_annotation::ns_by_hand_censoring:
					return;	//ignore
				case ns_death_time_annotation::ns_interval_censor_multiple_worm_clusters:
					{
					unsigned long number_of_worms(timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all));
					if (number_of_worms < 2)
						throw ns_ex("Weird!");

					timepoint_type.number_of_worms_in_cluster_by_machine_annotation = 1;
					timepoint_type.number_of_worms_in_cluster_by_hand_annotation = 1;

					if (last_observed_alive_time_measurement == 0){
						cerr << "Found an unusual situation where a multiple worm cluster has no slow down from fast moving time annotationed (perhaps because it existed at the start of observation?)\n";
						observed_death_time_measurement->observed_multiple_deaths.observed+=number_of_worms;
					}
					else{
						
						//include the last observed death
						observed_death_time_measurement->observed_multiple_deaths.observed++;
						//include the rest as death at the mean time in the interval they cannot be observed.
						double mean_missing_time(death_observation_time*.5+ last_alive_observation_time*.5);
						ns_worm_movement_measurement_summary & measurement_summary(all_timepoints[(unsigned long)mean_missing_time]);
						ns_worm_movement_measurement_summary_timepoint_data & measurement(measurement_summary.measurements[timepoint_type]);
						measurement.estimated_multiple_deaths.observed+=(number_of_worms-1);
					}
					}
					break;
			
				case ns_death_time_annotation::ns_right_censor_multiple_worm_clusters:{
					unsigned long number_of_worms(timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all));
					if (last_observed_alive_time_measurement == 0){
						cerr << "Found an unusual situation where a multiple worm cluster has no slow down from fast moving time annotationed (perhaps because it existed at the start of observation?)\n";
						observed_death_time->censoring_data.number_newly_censored_from_multiworm_clusters+=number_of_worms;
					}
					else{
						last_observed_alive_time->censoring_data.number_newly_censored_from_multiworm_clusters+=number_of_worms;
					}
					break;
				}
				case ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor:
					//ignore multiple deaths entirely and let the machine detect them as missing.
					break;
				default: throw ns_ex("ns_worm_movement_summary_series::from_death_time_annotations()::Unknown multiple worm censoring strategy");
			}
			*/
			
		//this gives us one death if we're ignoring by hand annotations
		//or if we want to include one death as part of our interval censoring strategy.
		//it returns multiple worms if we want to include all worms as deaths.
		/*const unsigned long number_of_worms_
		const unsigned long number_of_worms_excluding_multiples_if_requested(timepoint_type.number_of_worms(multiple_handing_strategy));

		unsigned long number_of_worms_in_multiple_worm_clump(timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all));
		if (number_of_worms_in_multiple_worm_clump < 2)
			number_of_worms_in_multiple_worm_clump = 0;
		
		{
		*/
		
		/*
		if (d.machine.stationary_worm_dissapearance != 0 && !d.machine.stationary_worm_dissapearance->time.period_end_was_not_observed){
			ns_worm_movement_measurement_summary & measurement_summary(all_timepoints[d.machine.stationary_worm_dissapearance->time.period_end]);
			ns_worm_movement_measurement_summary_timepoint_data & measurement(measurement_summary.measurements[timepoint_type]);
			measurement.number_of_stationary_worm_dissapearances++;
		}
			if (d.machine.death_annotation != 0 && !d.machine.death_annotation->time.period_end_was_not_observed){
				ns_worm_movement_measurement_summary & measurement_summary(all_timepoints[d.machine.death_annotation->time.period_end]);
				ns_worm_movement_measurement_summary_timepoint_data & measurement(measurement_summary.measurements[timepoint_type]);
				measurement.number_of_deaths_observed+= number_of_worms_excluding_multiples_if_requested;
				
				ns_multiple_worm_description clump;
				q->properties.transfer_sticky_properties(clump.properties);
				clump.time_of_death_machine = d.machine.death_annotation->time.period_end;
				if (d.machine.last_fast_movement_annotation != 0)
					clump.time_of_nucleation = d.machine.last_fast_movement_annotation->time.period_end;
				multiple_worm_clump_details.push_back(clump);

			}
			
			
				
		}
			
		//add censoring events for animals that enter multiworm clusters
		{
			unsigned long detection_time_of_multiple_worm_cluster = 0;
			bool is_fast_moving_worm(false);

			for (unsigned int i = 0; i < q->annotations.size(); i++){
				if (q->annotations[i].type == ns_fast_movement_cessation)
					detection_time_of_multiple_worm_cluster = q->annotations[i].time.start_time;
				if (q->annotations[i].type == ns_fast_moving_worm_observed)
					is_fast_moving_worm = true;
			}
			if (!q->properties.is_excluded() 
				&& censor_at_start_of_trace 
				&& censoring_strategy == ns_death_time_annotation::ns_censor_multiple_worm_clusters
				&& !is_fast_moving_worm){

				if (detection_time_of_multiple_worm_cluster == 0){
					number_stray_multiworm_clusters++;
				//	throw ns_ex("Could not find start time of a multi-worm cluster!");
				}
				else{
					number_of_multiworm_clusters_correctly_censored++;
				ns_worm_movement_measurement_summary & timepoint_data(all_timepoints[detection_time_of_multiple_worm_cluster]);
					int x = timepoint_type.number_of_worms(ns_worm_movement_measurement_summary_timepoint_type::ns_all);
					if (x > 0)
							
						timepoint_data.censoring_data.number_newly_censored_from_multiworm_clusters+=x;
				}
			}
				
			if (number_stray_multiworm_clusters > 0){
		//		std::cerr << "\n" << number_stray_multiworm_clusters << " stray multiworm annotations found out of "
			//			  << number_of_multiworm_clusters_correctly_censored + number_stray_multiworm_clusters << " total.\n";
			}
		}
		*/
	}
	for (ns_timepoints_sorted_by_time::iterator p = all_timepoints.begin(); p != all_timepoints.end(); p++){
		p->second.time = p->first;
	}
	/*	for (ns_worm_movement_measurement_summary_timepoint_list::iterator q = p->second.measurements.begin(); q != p->second.measurements.end(); q++){
			if (q->second.estimated_multiple_deaths.observed > 0){
				cerr << "WHA!";
			}
		}
	}*/
}
void ns_worm_movement_summary_series::from_death_time_annotations(const ns_death_time_annotation::ns_multiworm_censoring_strategy & censoring_strategy,
									const ns_death_time_annotation::ns_missing_worm_return_strategy & missing_worm_return_s,
									const ns_death_time_annotation_compiler & annotation_compiler, 
									const ns_animals_that_slow_but_do_not_die_handling_strategy & patial_path_strategy){
	this->clear();
	//IMPORTANT INFO ON MULTIPLE WORM CLUSTERS!
	//If ns_censor_machine_multiple_worms or ns_censor_by_hand_multiple_worms
	//are set, then the machine will generate BOTH death events for the animals
	//AND censoring events at the start of the traces.
	//downstream clients need to pick which of the annotations to use.
	//the death events are marked by having number_of_worm flags set.
	//the censoring events have their excluded field set as ns_multiworm_censored
	multiworm_cluster_censoring_strategy = censoring_strategy;
	missing_worm_return_strategy = missing_worm_return_s;
	ns_time_series_for_each_region time_series_for_each_region;
	
	//calculate cumulative and missing statistics for each plate
	std::set<unsigned long> all_times;
	for (ns_death_time_annotation_compiler::ns_region_list::const_iterator region = annotation_compiler.regions.begin(); region != annotation_compiler.regions.end(); region++){
		from_death_time_annotations(censoring_strategy,patial_path_strategy,region->second,time_series_for_each_region.time_series[region->first].measurements,multiple_worm_clump_details);
	}
	time_series_for_each_region.calculate_totals_and_cumulatives(censoring_strategy,missing_worm_return_strategy);
	//aggregate statistics and get all observation times
	this->estimated_number_of_worms_alive_at_measurement_end = 0;
	this->estimated_total_number_of_worms = 0;		
	for (ns_time_series_for_each_region::ns_time_series::iterator p = time_series_for_each_region.time_series.begin(); p!=time_series_for_each_region.time_series.end(); p++){
		this->estimated_number_of_worms_alive_at_measurement_end+=p->second.number_missing_at_end;
		this->estimated_total_number_of_worms+=p->second.estimated_total_number_of_worms;
		for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = p->second.measurements.begin(); t != p->second.measurements.end(); t++){
			all_times.insert(all_times.end(),t->first);
		}
	}

	measurements.resize(0);
	measurements.resize(all_times.size());
	std::set<unsigned long>::iterator t(all_times.begin());
	for (unsigned int i = 0; i < measurements.size(); i++){
		measurements[i].time = *t;
		t++;
	}
	//each measurement in a region indicates that for a span of time a worm was detected in a position.
	//we go through for each span in the region (that is, each interval between region_events[i][j] and region_events[i][j+1])
	//and increment the total number of worms for each measurement[] time point found between these points.
	
	//this is done this way because different regions are measured at different times and we have to aggregate them properly.

	unsigned long current_index_in_aggregate_data;
	for (ns_time_series_for_each_region::ns_time_series::iterator region = time_series_for_each_region.time_series.begin(); region!=time_series_for_each_region.time_series.end(); region++){
	
		current_index_in_aggregate_data = 0;
		for (ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t = region->second.measurements.begin(); t != region->second.measurements.end(); t++){
			ns_worm_movement_summary_series::ns_timepoints_sorted_by_time::iterator t_plus_one(t);
			t_plus_one++;
			bool added(false);
			if(t_plus_one == region->second.measurements.end()){
				//extend the last measurement of the plate to the end of the expeirment.
				//this prevents the total number of worms measured to remain meaningful during the last few time points of an experiment
				//where an increasing number of plates are no longer being measured.

				for (;current_index_in_aggregate_data < measurements.size(); current_index_in_aggregate_data++){
					measurements[current_index_in_aggregate_data].add(t->second);
					t->second.clear_observed_event_counts();	//we don't want to carry over single events
				}
				break;
			}
			for (;measurements[current_index_in_aggregate_data].time < t->first; current_index_in_aggregate_data++){
				measurements[current_index_in_aggregate_data].add(t->second);
				t->second.clear_observed_event_counts();	//we don't want to carry over single events
			}
		}
	}
	/*for (unsigned int i = 0; i < measurements.size(); i++)
		measurements[i].calculate_all_measurement_types_total();
	unsigned long x(0);

	//WWW
	//for (unsigned int i = 0; i < measurements.size(); i++){
	///	x+=measurements[i].all_measurement_types_total.number_of_deaths_observed;
	//	cerr << x << ",";
	//}*/
}

void ns_worm_movement_description_series::calculate_visualization_grid(const ns_vector_2i & extra_space_for_metadata) const{
	
	const bool include_metadata(extra_space_for_metadata.x > 0);

	const vector<ns_vector_2i> & sizes(group_context_sizes);


	//identify paths that should be included in the visualization
	group_should_be_displayed.resize(sizes.size(),0);
	unsigned long output_group_count(0);
	for (unsigned int t = 0; t < timepoints.size(); t++){
		for (unsigned int i = 0; i < timepoints[t].worms.size(); i++){
			if (timepoints[t].worms[i].path_id == -1) continue;
			if (timepoints[t].worms[i].path_id >= group_should_be_displayed.size())
				throw ns_ex("ns_worm_movement_description_series::calculate_visualization_grid()::Inconsistant path labeling!  Path id ")
					<< timepoints[t].worms[i].path_id << " found in description series with " << group_should_be_displayed.size() << " groups specified.";
			if (/*timepoints[t].worms[i].movement != ns_movement_fast && */
				group_should_be_displayed[timepoints[t].worms[i].path_id] == 0){
				group_should_be_displayed[timepoints[t].worms[i].path_id] = 1;
				output_group_count++;
			}
		}
	}
	
	if (output_group_count == 0)
		throw ns_ex("Region Contained No Paths");
	group_positions_on_visualization_grid.resize(group_context_sizes.size());
	if (include_metadata){
		metadata_dimensions = extra_space_for_metadata;
		metadata_positions_on_visualization_grid.resize(group_context_sizes.size());
	}

	const unsigned long worms_per_row((unsigned long)ceil(1.3*sqrt((double)output_group_count)));
	const unsigned long border = 15;
	unsigned long current_row_height(0);
	unsigned long maximum_row_width(0);
	ns_vector_2i cur_pos(border,border);

	unsigned long paths_placed(0);
	for (unsigned int i = 0; i < sizes.size(); i++){
		if(!group_should_be_displayed[i]){
			metadata_positions_on_visualization_grid[i] = ns_vector_2i(0,0);
			group_positions_on_visualization_grid[i] = ns_vector_2i(0,0);
			continue;
		}
	//	cerr << group_position[i] << " by " << sizes[i] << "\n";
		group_positions_on_visualization_grid[i] = cur_pos;
		if (include_metadata)
			metadata_positions_on_visualization_grid[i] = cur_pos + ns_vector_2i(0,sizes[i].y);

		ns_vector_2i item_size(sizes[i]);
		if (include_metadata){
			if (sizes[i].x < metadata_dimensions.x)
				item_size.x = metadata_dimensions.x;
			item_size.y += metadata_dimensions.y;
		}

		cur_pos.x += item_size.x + border;

		if (item_size.y > current_row_height)
			current_row_height = item_size.y;

		if (cur_pos.x > maximum_row_width)
			 maximum_row_width = cur_pos.x;

		if (paths_placed%worms_per_row == worms_per_row-1){
			cur_pos.x = border;
			cur_pos.y+=current_row_height + border;
			current_row_height = 0;
		}
		paths_placed++;
	}
	 visualization_grid_dimensions.x =  maximum_row_width;
	 visualization_grid_dimensions.y = cur_pos.y + current_row_height + border;
}
