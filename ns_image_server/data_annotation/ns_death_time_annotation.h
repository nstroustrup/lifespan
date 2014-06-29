#ifndef NS_DEATH_TIME_ANNOTATION_H
#define NS_DEATH_TIME_ANNOTATION_H
#include "ns_ex.h"
#include "ns_vector.h"
#include "ns_image_server_sql.h"
#include "ns_movement_state.h"
#include <stdlib.h>
#include <limits.h>


#define ns_censor_by_hand_multiple_worms true
#define ns_censor_machine_multiple_worms false
#define ns_trust_machine_multiworm_data false

struct ns_stationary_path_id{

	ns_stationary_path_id():group_id(-1),path_id(-1),detection_set_id(0){}
	ns_stationary_path_id(const long g,const long p, const unsigned long t):group_id(g),path_id(p),detection_set_id(t){}
	unsigned long detection_set_id;
	long group_id,
		 path_id;
	bool specified()const{return group_id!=-1 && path_id !=-1 && detection_set_id != 0;}
	
};

bool operator <(const ns_stationary_path_id & a, const ns_stationary_path_id & b);
bool operator ==(const ns_stationary_path_id & a, const ns_stationary_path_id & b);

struct ns_death_time_annotation_time_interval{
	ns_death_time_annotation_time_interval():
			period_start_was_not_observed(false),period_end_was_not_observed(false),period_start(UINT_MAX),period_end(UINT_MAX){}
	ns_death_time_annotation_time_interval(const unsigned long s, const unsigned long f):
			period_start(s),period_end(f),period_start_was_not_observed(false),period_end_was_not_observed(false){}
	unsigned long period_start,
				  period_end;
	bool period_start_was_not_observed,
		 period_end_was_not_observed;

	bool overlap(const ns_death_time_annotation_time_interval & t) const{
		return (period_start <= t.period_start && period_end >= t.period_start) ||
			   (period_start <= t.period_end && period_end >= t.period_end) ||
			   (period_start >= t.period_start && period_end <= t.period_end);
	}
	bool fully_unbounded() const{ return period_end_was_not_observed && period_start_was_not_observed;}
	double best_estimate_event_time_for_possible_partially_unbounded_interval() const{
		if (fully_unbounded())
			throw ns_ex("Cannot generate estimate time for fully unbounded interval");
		if (period_end_was_not_observed)
				return period_start;
		if (period_start_was_not_observed)
			return period_end;
		return best_estimate_event_time_within_interval();
	}
	double best_estimate_event_time_within_interval()const{
		if (period_start_was_not_observed || period_end_was_not_observed)
			throw ns_ex("Accessing unbounded event time");
		return period_start*.5+period_end*.5;
	}
	static ns_death_time_annotation_time_interval unobserved_interval(){
		ns_death_time_annotation_time_interval a(0,0);
		a.period_start_was_not_observed = true;
		a.period_end_was_not_observed = true;
		return a;
	}
	int interval_bound_code() const{
		// NB: ternary operator precedence is lower than addition; parens needed.
		return (period_start_was_not_observed?1:0) + (period_end_was_not_observed?2:0);
	}
	void from_interval_bound_code(int code){
		period_start_was_not_observed=(code%2==1);
		period_end_was_not_observed=code>1;
	}
};

struct ns_death_time_annotation_flag{

	ns_death_time_annotation_flag():label_is_cached(false),cached_hidden(false),cached_excluded(false),cached_color(0,0,0){}
	ns_death_time_annotation_flag(const std::string & label_short_, const std::string &label_long_="", const bool &  excluded_=false, const std::string & next_flag_name_in_order_="", const std::string & color_="000000"):
	label_short(label_short_),cached_label(label_long_),cached_excluded(excluded_),label_is_cached(!label_long_.empty() || excluded_),next_flag_name_in_order(next_flag_name_in_order_),cached_hidden(false),cached_color(ns_hex_string_to_color<ns_color_8>(color_)){}
	//unsigned long id;
	std::string label_short;
	std::string label() const;
	void step_event();

	bool event_should_be_excluded() const{
		if (label_is_cached)
			return cached_excluded;
		get_cached_info();
		return cached_excluded;
	}
	bool specified() const{return !label_short.empty();}

	static void get_flags_from_db(ns_sql & sql);
	
	ns_color_8 flag_color() const;

	static char * first_default_flag_short_label();
	static void generate_default_flags(std::vector<ns_death_time_annotation_flag> & flags);

	static ns_death_time_annotation_flag none(){
		return ns_death_time_annotation_flag();
		//return ns_death_time_annotation_flag("","",false,first_default_flag_short_label());
	}
	static ns_death_time_annotation_flag extra_worm_from_multiworm_disambiguation();

private:
	typedef std::map<std::string, ns_death_time_annotation_flag> ns_flag_cache_by_short_label;
	static ns_flag_cache_by_short_label cached_flags_by_short_label;
	mutable bool label_is_cached;
	mutable std::string cached_label;
	mutable bool cached_excluded;
	mutable bool cached_hidden;
	mutable std::string next_flag_name_in_order;
	mutable ns_color_8 cached_color;

	void get_cached_info() const;

};

//the convention is to have position be the center
//of the detected object.
//to get the top left corner, calc position - size/2
struct ns_death_time_annotation_event_count{
	ns_death_time_annotation_event_count():machine_count(0),hand_count(0){}
	ns_death_time_annotation_event_count(const unsigned long machine_, const unsigned long hand_):machine_count(machine_),hand_count(hand_){}
	unsigned long machine_count,hand_count;
};
struct ns_death_time_annotation{
	
	typedef enum{ns_unknown,ns_posture_image,ns_region_image,ns_lifespan_machine,ns_storyboard} ns_annotation_source_type;
	typedef enum{
				ns_not_excluded, 
				ns_excluded,ns_machine_excluded,ns_by_hand_excluded,ns_both_excluded,
				ns_censored,
				ns_excluded_and_censored,
				ns_multiworm_censored,
				ns_missing_censored,
				ns_censored_at_end_of_experiment,
				ns_number_of_exclusion_values
				} ns_exclusion_type;

	typedef enum{ns_single_worm, ns_part_of_a_mutliple_worm_disambiguation_cluster} ns_disambiguation_type;

	enum{maximum_number_of_worms_at_position=6};

	typedef enum{	ns_not_applicable,
					ns_include_as_single_worm_deaths,
					ns_right_censor_multiple_worm_clusters,
					ns_interval_censor_multiple_worm_clusters,
					ns_include_multiple_worm_cluster_deaths,
					ns_by_hand_censoring,
					ns_merge_multiple_worm_clusters_and_missing_and_censor,
					ns_number_of_multiworm_censoring_strategies
	} ns_multiworm_censoring_strategy;

	typedef enum {	ns_not_specified,
					ns_censoring_minimize_missing_times,
					ns_censoring_assume_uniform_distribution_of_missing_times,
					ns_censoring_assume_uniform_distribution_of_only_large_missing_times,
					ns_number_of_missing_worm_return_strategies} ns_missing_worm_return_strategy;

	typedef enum { ns_only_machine_annotations,
				   ns_machine_annotations_if_no_by_hand,
				   ns_only_by_hand_annotations,
				   ns_machine_and_by_hand_annotations} ns_by_hand_annotation_integration_strategy;
	
	static ns_multiworm_censoring_strategy default_censoring_strategy();
	static ns_missing_worm_return_strategy default_missing_return_strategy();
	ns_missing_worm_return_strategy missing_worm_return_strategy;

	typedef enum {ns_standard,
		ns_induced_multiple_worm_death,
		ns_observed_multiple_worm_death} ns_event_observation_type;

	std::string static event_observation_label(const ns_event_observation_type & e);

	ns_death_time_annotation():volatile_duration_of_time_not_fast_moving(0),longest_gap_without_observation(0),
		type(ns_no_movement_event),time(0,0),region_info_id(0),region_id(0),position(0,0),size(0,0),animal_is_part_of_a_complete_trace(false),
		annotation_source(ns_unknown),excluded(ns_not_excluded),number_of_worms_at_location_marked_by_hand(0),multiworm_censoring_strategy(ns_not_applicable),
		number_of_worms_at_location_marked_by_machine(0),annotation_time(0),disambiguation_type(ns_single_worm), flag(ns_death_time_annotation_flag::none()),loglikelihood(1),animal_id_at_position(0),
		missing_worm_return_strategy(ns_not_specified),volatile_matches_machine_detected_death(false),event_observation_type(ns_standard),
		by_hand_annotation_integration_strategy(ns_only_machine_annotations),inferred_animal_location(false){}

	ns_death_time_annotation(const ns_movement_event type_, const unsigned long region_id_, const unsigned long region_info_id_,
		const ns_death_time_annotation_time_interval time_, const ns_vector_2i & pos, const ns_vector_2i & size_,const ns_exclusion_type excluded_,
		const ns_death_time_annotation_event_count & event_counts, const unsigned long annotation_time_, const ns_annotation_source_type source_type,
		const ns_disambiguation_type & d, const ns_stationary_path_id & s_id,const bool animal_is_part_of_a_complete_trace_,const bool inferred_animal_location_, const std::string & annotation_details_="",
		const double loglikelihood_=1,const unsigned long longest_gap_without_observation_ = 0, const ns_multiworm_censoring_strategy & cen_strat = ns_not_applicable, const ns_missing_worm_return_strategy & missing_worm_return_strategy_ = ns_not_specified, 
		const ns_event_observation_type & event_observation_type_=ns_standard, const ns_by_hand_annotation_integration_strategy & by_hand_strategy = ns_only_machine_annotations):multiworm_censoring_strategy(cen_strat),loglikelihood(loglikelihood_),
				type(type_),region_id(region_id_),time(time_),position(pos),size(size_),excluded(excluded_),region_info_id(region_info_id_),volatile_duration_of_time_not_fast_moving(0),
				number_of_worms_at_location_marked_by_hand(event_counts.hand_count),
				number_of_worms_at_location_marked_by_machine(event_counts.machine_count),volatile_matches_machine_detected_death(false),
				annotation_time(annotation_time_),annotation_source(source_type),annotation_source_details(annotation_details_),inferred_animal_location(inferred_animal_location_),
				disambiguation_type(d),stationary_path_id(s_id),flag(ns_death_time_annotation_flag::none()),event_observation_type(event_observation_type_),
				animal_is_part_of_a_complete_trace(animal_is_part_of_a_complete_trace_),longest_gap_without_observation(longest_gap_without_observation_),missing_worm_return_strategy(missing_worm_return_strategy_),animal_id_at_position(0),by_hand_annotation_integration_strategy(by_hand_strategy){}
	
	static std::string source_type_to_string(const ns_annotation_source_type & t);

	std::string description() const;
	static ns_exclusion_type combine_exclusion_types(const ns_exclusion_type & a, const ns_exclusion_type & b);
	std::string to_string() const;
	void from_string(const std::string v);

	static unsigned long exclusion_value(const ns_exclusion_type & t);

	static std::string exclusion_string(const ns_exclusion_type & t);
	
	static inline bool is_censored(const ns_exclusion_type & t)
	{return (t==ns_censored || t==ns_excluded_and_censored || t == 
		ns_multiworm_censored || t == ns_missing_censored || t == ns_censored_at_end_of_experiment);}
	inline bool is_censored() const{return is_censored(excluded);}
	static inline bool is_excluded(const ns_exclusion_type & t) {return (t!=ns_not_excluded && !is_censored(t) || t==ns_excluded_and_censored);}
	inline bool is_excluded()const {return is_excluded(excluded);}

	std::string censor_description()const {
		return censor_description(excluded);
	}
	static std::string censor_description(const ns_death_time_annotation::ns_exclusion_type & t);
	static std::string multiworm_censoring_strategy_label(const ns_multiworm_censoring_strategy & m);
	static std::string multiworm_censoring_strategy_label_short(const ns_multiworm_censoring_strategy & m);
	static std::string missing_worm_return_strategy_label(const ns_missing_worm_return_strategy & m);
	static std::string missing_worm_return_strategy_label_short(const ns_missing_worm_return_strategy & m);
	static std::string by_hand_annotation_integration_strategy_label_short(const ns_by_hand_annotation_integration_strategy & s);

	unsigned long number_of_worms() const{
		if (number_of_worms_at_location_marked_by_hand != 0)
			return number_of_worms_at_location_marked_by_hand;
		return number_of_worms_at_location_marked_by_machine;
	}
	void transfer_sticky_properties(ns_death_time_annotation & a) const;

	bool has_sticky_properties() const{
		return is_excluded() || is_censored() || disambiguation_type != ns_single_worm || 
			   flag.specified() || number_of_worms_at_location_marked_by_hand != 0
			   || number_of_worms_at_location_marked_by_machine != 0;
	}
	void clear_sticky_properties(){
		excluded = ns_not_excluded;
		disambiguation_type = ns_single_worm;
		flag = ns_death_time_annotation_flag::none();
		number_of_worms_at_location_marked_by_machine = 0;
		number_of_worms_at_location_marked_by_hand = 0;
	}
	void clear_movement_properties(){
		type = ns_no_movement_event;
		time.period_end = time.period_start = 0;
	}
	//sticky properties
	ns_exclusion_type excluded;
	ns_disambiguation_type disambiguation_type;
	unsigned long number_of_worms_at_location_marked_by_machine,
				  number_of_worms_at_location_marked_by_hand;

	unsigned long longest_gap_without_observation;

	ns_multiworm_censoring_strategy multiworm_censoring_strategy;

	ns_by_hand_annotation_integration_strategy by_hand_annotation_integration_strategy;

	ns_death_time_annotation_flag flag;
	ns_event_observation_type event_observation_type;

	bool animal_is_part_of_a_complete_trace;

	ns_stationary_path_id stationary_path_id;

	ns_vector_2i size;

	ns_movement_event type;

	ns_annotation_source_type annotation_source;
	std::string annotation_source_details;
	unsigned long annotation_time;
	ns_death_time_annotation_time_interval time;
	unsigned long region_info_id;
	unsigned long region_id;
	//the center of the object
	unsigned long animal_id_at_position; //0 for the first worm in the stationary path; //1 for the second, etc
	ns_vector_2i position;
	double loglikelihood;
	bool inferred_animal_location;
	unsigned long volatile_duration_of_time_not_fast_moving;
	bool volatile_matches_machine_detected_death;
};

typedef enum {ns_include_unchanged,ns_force_to_fast_moving} ns_animals_that_slow_but_do_not_die_handling_strategy;
std::string ns_animals_that_slow_but_do_not_die_handling_strategy_label(const ns_animals_that_slow_but_do_not_die_handling_strategy & s);
std::string ns_animals_that_slow_but_do_not_die_handling_strategy_label_short(const ns_animals_that_slow_but_do_not_die_handling_strategy & s);

#endif
