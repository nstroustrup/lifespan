#ifndef NS_MOVEMENT_MEASUREMENT_H
#define NS_MOVEMENT_MEASUREMENT_H
#include "ns_ex.h"
#include <ostream>
#include <string>
#include "ns_movement_state.h"
#include "ns_vector.h"
#include "ns_survival_curve.h"
#include "ns_death_time_annotation_set.h"
#include <limits.h>

struct ns_worm_movemement_measurement_summary_statistics{
	ns_worm_movemement_measurement_summary_statistics():maximum_count_for_movement_state(ns_movement_number_of_states,0),minimum_count_for_movement_state(ns_movement_number_of_states,ULONG_MAX){}
	std::vector<double> maximum_count_for_movement_state;
	std::vector<double> minimum_count_for_movement_state;
};
struct ns_worm_movement_measurement_summary_timepoint_data_departure_type{
	ns_worm_movement_measurement_summary_timepoint_data_departure_type():observed(0),cumulative(0){}
	unsigned long observed,
				cumulative;
	void add(const ns_worm_movement_measurement_summary_timepoint_data_departure_type & b){
		observed+=b.observed;
		cumulative+=b.cumulative;
	}
};
struct ns_worm_movement_measurement_summary_timepoint_data{
	ns_worm_movement_measurement_summary_timepoint_data():	
		number_moving_fast(0),
		number_moving_slow(0),
		number_changing_posture(0),
		number_stationary(0),
		number_death_posture_relaxing(0),
		number_of_stationary_worm_dissapearances(0),
		number_of_missing_animals(0),
		number_permanantly_lost(0),
		number_of_missing_animals_smoothed(0),
		number_permanantly_lost_before_end_of_experiment(0){}

	static void out_header(const std::string & name, std::ostream & o, const char separator);
	void out_data(const unsigned long max_total_observed_alive,std::ostream & o, const char & separator) const;
	
	unsigned long
		number_moving_fast,
		number_moving_slow,
		number_changing_posture,
		number_stationary,
		number_death_posture_relaxing,
		number_of_stationary_worm_dissapearances;

	ns_worm_movement_measurement_summary_timepoint_data_departure_type singleton_deaths,
																    observed_multiple_deaths,
																	estimated_multiple_deaths;
	long number_of_missing_animals,
		 number_of_missing_animals_smoothed,
		 number_permanantly_lost,
		 number_permanantly_lost_before_end_of_experiment;
	
	unsigned long number_cumulative_deaths()const{
		return singleton_deaths.cumulative + observed_multiple_deaths.cumulative+estimated_multiple_deaths.cumulative;
	}
	unsigned long number_alive()const{return number_moving_fast+number_moving_slow+number_changing_posture;}
	unsigned long number_not_translating()const {return number_changing_posture+number_cumulative_deaths();}
	unsigned long total_animals_observed()const{return number_alive() + number_cumulative_deaths()+number_cumulative_deaths();}
	unsigned long total_animals_inferred()const{return number_alive() + number_cumulative_deaths() +  number_permanantly_lost;}
	
	void add(const ns_worm_movement_measurement_summary_timepoint_data & s);

	unsigned long & movement_state_count(const ns_movement_state & state){
		switch(state){
			case ns_movement_fast: return number_moving_fast;
			case ns_movement_slow: return number_moving_slow;
			case ns_movement_posture: return number_changing_posture;
			case ns_movement_stationary: return number_stationary;
			case ns_movement_death_posture_relaxation: return number_death_posture_relaxing;
			default:throw ns_ex("ns_worm_movement_measurement_summary::movement_state_count():: invalid state: " )<< (long)state;
		}
	}
};
struct ns_multiple_worm_description{
	unsigned long time_of_nucleation,
		time_of_death_machine,
		time_of_death_hand;
	ns_death_time_annotation properties;
};
struct ns_worm_movement_measurement_summary_timepoint_type{

	ns_worm_movement_measurement_summary_timepoint_type():
		number_of_worms_in_cluster_by_hand_annotation(0),number_of_worms_in_cluster_by_machine_annotation(0),exclusion_type(ns_death_time_annotation::ns_not_excluded){}
	ns_worm_movement_measurement_summary_timepoint_type(const ns_death_time_annotation & a);

	typedef enum {ns_all,ns_exclude_multiples,ns_interval_censor_multiples,ns_ignore_by_hand_multiworm_annotations} ns_multiple_handling;

	ns_death_time_annotation::ns_exclusion_type exclusion_type;
	unsigned int number_of_worms_in_cluster_by_hand_annotation;
	unsigned int number_of_worms_in_cluster_by_machine_annotation;
	
	int number_of_machine_worms(const ns_multiple_handling & h)const{
		if (ns_trust_machine_multiworm_data){
			if (h == ns_exclude_multiples && number_of_worms_in_cluster_by_machine_annotation>1)
				return 0;
			return number_of_worms_in_cluster_by_machine_annotation;
		}
		return (number_of_worms_in_cluster_by_machine_annotation>0)?1:0;
	}
	int number_of_worms(const ns_multiple_handling & h)const{
		if (h == ns_ignore_by_hand_multiworm_annotations || number_of_worms_in_cluster_by_hand_annotation == 0
			|| h == ns_interval_censor_multiples) //this gives us one animal if we either want to ignore annotations, or for interval censoring include the latest death
			return number_of_machine_worms(h);
		if (h == ns_exclude_multiples && number_of_worms_in_cluster_by_hand_annotation > 1)
			return 0;
		return number_of_worms_in_cluster_by_hand_annotation;
	}

	static void out_header(const std::string & name,std::ostream & o,const char & separator){
		o << name << " Part of a Multiple Worm Disambiguation Cluster," << name << " Number of Worms Annotated by Hand," << name << " Number of Worms Annotated by Machine " <<separator;
	}
	void out_data(std::ostream & o, const char & separator) const{
		o << (int)exclusion_type << "," 
			<< number_of_worms_in_cluster_by_hand_annotation << ","
			<< number_of_worms_in_cluster_by_machine_annotation << separator;
	}
};
bool operator < (const ns_worm_movement_measurement_summary_timepoint_type & a, const ns_worm_movement_measurement_summary_timepoint_type & b);

typedef std::map<ns_worm_movement_measurement_summary_timepoint_type,ns_worm_movement_measurement_summary_timepoint_data> 
	ns_worm_movement_measurement_summary_timepoint_list;

typedef std::map<ns_worm_movement_measurement_summary_timepoint_type,ns_worm_movemement_measurement_summary_statistics> 
	ns_worm_movemement_measurement_summary_statistics_list;

struct ns_worm_movement_censoring_data{
	unsigned long number_newly_censored_from_multiworm_clusters;
	unsigned long cumulative_number_censored_from_multiworm_clusters;

	ns_worm_movement_censoring_data():number_newly_censored_from_multiworm_clusters(0),
										 cumulative_number_censored_from_multiworm_clusters(0){}
	static void out_header(std::ostream & o, const char separator=','){
		o << "Number Animals Newly Censored Entering Mutli-worm clusters,"
			"Cumulative number of animals censored after enting multi-worm clusters"
			<< separator;
	}
	void out_data(std::ostream & o, const char separator=',')const{
		o << number_newly_censored_from_multiworm_clusters << ","
			<< cumulative_number_censored_from_multiworm_clusters << separator;
	}
	void add(const ns_worm_movement_censoring_data & s){
	number_newly_censored_from_multiworm_clusters			+=s.number_newly_censored_from_multiworm_clusters;
	cumulative_number_censored_from_multiworm_clusters		+=s.cumulative_number_censored_from_multiworm_clusters;
	}
};
struct ns_worm_movement_measurement_summary{

	void clear_observed_event_counts(){
		for (ns_worm_movement_measurement_summary_timepoint_list::iterator p = measurements.begin(); p != measurements.end(); p++){
			p->second.singleton_deaths.observed = 0;
			p->second.estimated_multiple_deaths.observed = 0;
			p->second.observed_multiple_deaths.observed = 0;
			p->second.number_of_stationary_worm_dissapearances = 0;
		}
		censoring_data.number_newly_censored_from_multiworm_clusters = 0;
	}
	ns_worm_movement_measurement_summary():time(0){}
	unsigned long time;

	ns_worm_movement_measurement_summary_timepoint_list measurements;	//measurements made for the current time point from various
																		//combinations of data
	ns_worm_movement_censoring_data censoring_data;

	ns_worm_movement_measurement_summary_timepoint_data all_measurement_types_total;
	ns_worm_movement_measurement_summary_timepoint_data all_measurement_types_excluded_total;

	void calculate_all_measurement_types_total(){
		all_measurement_types_total = ns_worm_movement_measurement_summary_timepoint_data();
		all_measurement_types_excluded_total = ns_worm_movement_measurement_summary_timepoint_data();
		for(ns_worm_movement_measurement_summary_timepoint_list::const_iterator p = measurements.begin(); p != measurements.end(); p++){
			if (ns_death_time_annotation::is_excluded(p->first.exclusion_type))
				all_measurement_types_excluded_total.add(p->second);
			else all_measurement_types_total.add(p->second);
		}
	}

	void add(const ns_worm_movement_measurement_summary & s){

		//add similar survival types together
		for(ns_worm_movement_measurement_summary_timepoint_list::const_iterator p = s.measurements.begin(); p != s.measurements.end(); p++){
			ns_worm_movement_measurement_summary_timepoint_list::iterator q(measurements.find(p->first));
			if (q == measurements.end())
				measurements.insert(ns_worm_movement_measurement_summary_timepoint_list::value_type(p->first,p->second));
			else q->second.add(p->second);
		}
		all_measurement_types_total.add(s.all_measurement_types_total);
		all_measurement_types_excluded_total.add(s.all_measurement_types_excluded_total);
		censoring_data.add(s.censoring_data);
	}

	static void out_header(std::ostream & o);
	void to_file(const ns_region_metadata & metadata,const unsigned long total_number_of_worms,std::ostream & o) const;
//	bool from_file(std::istream & o);
};

struct ns_moving_animal_count{
	unsigned long at_time_of_maximum_number_of_stationary_animals,
					at_time_of_maximum_number_of_non_translating_animals;
};
typedef std::map<ns_worm_movement_measurement_summary_timepoint_type,ns_moving_animal_count> 
	ns_worm_movement_summary_series_maximum_list;

struct ns_worm_movement_summary_series{
	

	void from_death_time_annotations(const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy &,
									const ns_death_time_annotation::ns_multiworm_censoring_strategy & censoring_strategy,
									const ns_death_time_annotation::ns_missing_worm_return_strategy & missing_worm_return_strategy,
									const ns_death_time_annotation_compiler & annotation_set, 
									const ns_animals_that_slow_but_do_not_die_handling_strategy & patial_path_strategy);

	ns_worm_movement_summary_series():number_uncalculated(0),estimated_total_number_of_worms(0),estimated_number_of_worms_alive_at_measurement_end(0){}
	typedef std::vector<ns_worm_movement_measurement_summary> ns_measurement_timeseries;
	ns_measurement_timeseries measurements;
	
	unsigned long estimated_total_number_of_worms,
			estimated_number_of_worms_alive_at_measurement_end;

	ns_worm_movemement_measurement_summary_statistics_list movement_summary_statistics;
	ns_worm_movemement_measurement_summary_statistics total_summary_statistics,
													  total_excluded_summary_statistics,
													  total_multiple_worm_summary_statistics;

	
	std::vector<ns_multiple_worm_description> multiple_worm_clump_details;
	
	void generate_censoring_annotations(const ns_region_metadata & m,ns_death_time_annotation_set & set);
	static void output_censoring_diagnostic_header(std::ostream & o);
	void generate_censoring_diagnostic_file(const ns_region_metadata & metadata,std::ostream & o);
	unsigned long number_of_worms_at_start(const bool refresh_cache=true)const;	
	//
	//ns_worm_movement_summary_series_maximum_list number_of_moving_animals;
	//ns_moving_animal_count total_number_of_animals,
	//					   total_number_of_censored_animals,
	//					   total_number_of_excluded_animals;

	unsigned long number_uncalculated;
	float percent_calculated() const{
		if (measurements.size()+number_uncalculated == 0) return 0; 
		return((float)measurements.size())/(float)(measurements.size()+number_uncalculated);
	}
	bool empty() const{return measurements.size()==0;}
	
	//unsigned long calculate_missing_moving_worms(const ns_death_time_annotation::ns_missing_worm_return_strategy & missing_worm_return_strategy);
	//void calculate_survival();	
	//void calculate_totals();
	//void exclude_stray_fast_moving_observations();
	void calculate_maximums();

	void to_file(const ns_region_metadata & metadata,std::ostream & o) const;
//	void from_file(std::istream & o);
	void clear(){
		multiworm_cluster_censoring_strategy = ns_death_time_annotation::ns_not_applicable;
		cached_number_of_worms_at_start = 0;
		this->estimated_number_of_worms_alive_at_measurement_end = 0;
		this->estimated_total_number_of_worms = 0;
		measurements.clear();
		movement_summary_statistics.clear();
		total_summary_statistics=ns_worm_movemement_measurement_summary_statistics();
		total_excluded_summary_statistics=ns_worm_movemement_measurement_summary_statistics();
	}
	typedef std::map<unsigned long,ns_worm_movement_measurement_summary> ns_timepoints_sorted_by_time;
	private:
	
	ns_death_time_annotation::ns_multiworm_censoring_strategy multiworm_cluster_censoring_strategy;
	ns_death_time_annotation::ns_missing_worm_return_strategy missing_worm_return_strategy;
	ns_death_time_annotation::ns_by_hand_annotation_integration_strategy by_hand_strategy;
	static void from_death_time_annotations(const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_strategy,const ns_death_time_annotation::ns_multiworm_censoring_strategy & censoring_strategy, const ns_animals_that_slow_but_do_not_die_handling_strategy & patial_path_strategy,const ns_death_time_annotation_compiler_region & region, ns_timepoints_sorted_by_time & region_events,std::vector<ns_multiple_worm_description> & multiple_worm_clump_details);
	mutable unsigned long cached_number_of_worms_at_start;
};
class ns_detected_worm_info;

struct ns_worm_movement_measurement_description{
	ns_worm_movement_measurement_description(const ns_detected_worm_info * w=0,
											 const ns_movement_state & m=ns_movement_not_calculated, 
											 const ns_vector_2i & pos=ns_vector_2i(0,0),
											 const ns_vector_2i & siz = ns_vector_2i(0,0),
											 const long path_id_ = -1):
												worm_(w),movement(m),region_position(pos),region_size(siz),path_id(path_id_){}
	bool worm_specified() const {return worm_ != 0;}
	const ns_detected_worm_info & worm() const {return *worm_;}
	long path_id;
	ns_movement_state movement;
	ns_vector_2i region_position,region_size;
private:
	const ns_detected_worm_info * worm_;
};
struct ns_worm_movement_measurement_description_timepoint{
	unsigned long time;
	std::vector<ns_worm_movement_measurement_description> worms;
};

struct ns_worm_movement_description_series{
	std::vector<ns_worm_movement_measurement_description_timepoint> timepoints;

	std::vector<ns_vector_2i> group_region_position_in_source_image;
	std::vector<ns_vector_2i> group_region_sizes;
	std::vector<ns_vector_2i> group_context_position_in_source_image;
	std::vector<ns_vector_2i> group_context_sizes;
	mutable std::vector<char> group_should_be_displayed;

	void calculate_visualization_grid(const ns_vector_2i & extra_space_for_metadata = ns_vector_2i(0,0)) const ;
	void output_position_visualization_csv(std::ostream & o) const;

	private:
	mutable std::vector<ns_vector_2i> group_positions_on_visualization_grid;
	mutable ns_vector_2i visualization_grid_dimensions;
	mutable std::vector<ns_vector_2i> metadata_positions_on_visualization_grid;
	mutable ns_vector_2i metadata_dimensions;
	friend class ns_movement_summarizer;
	friend class ns_time_path_image_movement_analyzer;
};

class ns_movement_series_summary_producer{
public:
	virtual void make_movement_summary_series(ns_worm_movement_summary_series & series,const ns_movement_data_source_type::type & type) const = 0;
	virtual bool ready_to_produce_movement_data() const = 0;
	virtual std::string analysis_description(const ns_movement_data_source_type::type & type)const =0;
};

class ns_movement_series_description_producer{
public:
	virtual const ns_worm_movement_description_series & movement_description_series(const ns_movement_data_source_type::type & type) const = 0;
	virtual bool ready_to_produce_movement_data() const = 0;
};


class ns_movement_colors{
public:
	static ns_color_8 color(const ns_movement_state & m){
		switch(m){
			case ns_movement_death_posture_relaxation: return ns_color_8 (180,0,20);
			case ns_movement_stationary: return ns_color_8(255,0,0);
			case ns_movement_posture: return ns_color_8(255,255,0);
			case ns_movement_slow: return ns_color_8(0,255,0);
			case ns_movement_fast:return  ns_color_8(255,0,255);
			case ns_movement_machine_excluded: return ns_color_8(175,175,175);
			case ns_movement_by_hand_excluded: return ns_color_8(225,225,225);
			
			case ns_movement_not_calculated: return ns_color_8(0,0,0);
			default: throw ns_ex("Uknown movement color request:") << (unsigned long)m;
		}
	}
};
#endif
