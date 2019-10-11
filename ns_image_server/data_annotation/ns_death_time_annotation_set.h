#ifndef NS_DEATH_TIME_ANNOTATION_SET
#define NS_DEATH_TIME_ANNOTATION_SET
#include "ns_death_time_annotation.h"
#include "ns_survival_curve.h"
#ifdef NS_GENERATE_IMAGE_STATISTICS
//#include "ns_captured_image_statistics_set.h"
#endif
#include <set>
#include <vector>
class ns_death_time_annotation_set{
public:
	
	typedef enum { ns_all_annotations,
		ns_censoring_data,ns_movement_transitions,
		ns_movement_states,
		ns_censoring_and_movement_transitions,
		ns_censoring_and_movement_states,
		ns_recalculate_from_movement_quantification_data,
		ns_no_annotations} ns_annotation_type_to_load;
	
	static bool annotation_matches(const ns_annotation_type_to_load & type,const ns_death_time_annotation & e);
	
	static std::string annotation_types_to_string(const ns_annotation_type_to_load & t);

	void read(const ns_annotation_type_to_load & t,std::istream & i, const bool exclude_fast_moving_animals=true);
	void write(std::ostream & i)const;
	
	void write_column_format(std::ostream & o)const;
	void write_split_file_column_format(std::ostream & censored_and_transition_file, std::ostream & state_file)const;
	void read_column_format(const ns_annotation_type_to_load & t,std::istream & o, const bool exclude_fast_moving_animals=true, const bool single_line=false);
	void read_xml(const ns_annotation_type_to_load & t,std::istream & i);
	void write_xml(std::ostream & o) const;

	void add(const ns_death_time_annotation& a);
	void add(const ns_death_time_annotation_set& s);
	std::vector<ns_death_time_annotation> events;
	bool compare(const ns_death_time_annotation_set& set) const;
	//make the set act like a vector
	ns_death_time_annotation & operator[](const unsigned long i){return events[i];}
	const ns_death_time_annotation & operator[](const unsigned long i) const{return events[i];}
	std::vector<ns_death_time_annotation>::size_type  size() const{return events.size();}
	typedef std::vector<ns_death_time_annotation>::iterator iterator;
	typedef std::vector<ns_death_time_annotation>::const_iterator const_iterator;
	iterator begin(){return events.begin();}
	const_iterator begin()const{return events.begin();}
	iterator end(){return events.end();}
	const_iterator end()const{return events.end();}
	iterator erase(const iterator & p){return events.erase(p);}
	bool empty()const{return events.empty();}
	void push_back(const ns_death_time_annotation & e){events.push_back(e);}

	void clear(){events.resize(0);}

	void remove_all_but_specified_event_type(const ns_annotation_type_to_load & t);

	static bool use_debug_read_columns;
};

//represents a location in an experiment where a worm dies
//the list of events that happen there are stored in annotations[].
//attempt_to_add() checks to see if an event occured within the distance cutoff
//and, if so, adds the event to its annotations[]
template<class T>
struct ns_dying_animal_description_group{
	ns_dying_animal_description_group():death_annotation(0),last_slow_movement_annotation(0),last_fast_movement_annotation(0),
									death_associated_expansion_start(0), death_associated_expansion_stop(0), 
									death_associated_post_expansion_contraction_start(0), death_associated_post_expansion_contraction_stop(0),
		stationary_worm_dissapearance(0){}
	T *death_annotation,
								*last_slow_movement_annotation,
								*last_fast_movement_annotation,
								*death_associated_expansion_start,
								*death_associated_expansion_stop,
								*death_associated_post_expansion_contraction_start,
								*death_associated_post_expansion_contraction_stop,
								*stationary_worm_dissapearance;
	std::vector<T *> slow_moving_state_annotations,
				   posture_changing_state_annotations,
				   stationary_animal_state_annotations,
				   movement_censored_state_annotations;
};

//single locations can be annotated to hold multiple worms.  So we need a container corresponding to each location that holds annotations for each worm in the multiple worm clusters.
template<class annotation_t>
struct ns_dying_animal_description_base {
	ns_dying_animal_description_base() : final_number_of_worms_by_hand(0), final_number_of_worms_by_machine(0) {}
	typedef ns_dying_animal_description_group<annotation_t> ns_group_type;
	ns_group_type
		by_hand,
		machine;
	unsigned long final_number_of_worms_by_hand,
		final_number_of_worms_by_machine;
};

template<class annotation_t>
struct ns_dying_animal_description_set_base {
	typedef std::vector< ns_dying_animal_description_base<annotation_t> > description_set_type;
	//each description corresponds to a worm.
	//normally, each location will have just a worm,
	//but when users annotate extra worms, then these additional worms
	//each get their own description.
	description_set_type descriptions;
	unsigned long unassigned_multiple_worms;
};

typedef ns_dying_animal_description_set_base<ns_death_time_annotation> ns_dying_animal_description_set;
typedef ns_dying_animal_description_set_base<const ns_death_time_annotation> ns_dying_animal_description_set_const;


class ns_death_time_annotation_compiler_location{
	void handle_sticky_properties(const ns_death_time_annotation & a);
public:
	ns_death_time_annotation_compiler_location(){}
	ns_death_time_annotation properties;
	ns_death_time_annotation_set annotations;
	
	ns_death_time_annotation_compiler_location(const ns_death_time_annotation & a);
	void add_location(ns_stationary_path_id& id);
	bool location_matches(const unsigned long distance_cutoff_squared,const ns_vector_2i & position) const;
	bool attempt_to_add(const unsigned long distance_cutoff_squared,const ns_death_time_annotation & a);
	bool add_event(const ns_death_time_annotation & a);
	void merge(const ns_death_time_annotation_compiler_location & location);

	void generate_dying_animal_description_const(const bool warn_on_movement_errors, ns_dying_animal_description_set_const & descriptions) const;
	void generate_dying_animal_description(const bool warn_on_movement_errors, ns_dying_animal_description_set & description);

};
//represents the set of all events that occur inside a region
//users feed add() each event that occurred in the region,
//and ns_death_time_annotation_compiler_region sorts them into
//groups based on their location.  This allows events that happen to the same worm
//to be linked together.
class ns_death_time_annotation_compiler_region;

class ns_death_time_annotation_compiler_region{
	unsigned long match_distance_squared;

public:

	ns_death_time_annotation_compiler_region():match_distance_squared(125*125){}
	typedef std::vector<ns_death_time_annotation_compiler_location> ns_location_list;
	void output_visualization_csv(std::ostream & o,const bool output_header) const;

	void merge(const ns_death_time_annotation_compiler_region & region, const bool create_new_location);
	ns_location_list locations;
	ns_death_time_annotation_set non_location_events;
	ns_death_time_annotation_set fast_moving_animals;
	void add(const ns_death_time_annotation & e, const bool create_new_location);
	void create_location(const ns_stationary_path_id & s, const ns_vector_2i& position, const ns_vector_2i& size);
	void clear();
	ns_region_metadata metadata;

	ns_death_time_annotation_time_interval latest_interval() const;
	
	void generate_survival_curve(ns_survival_data & curve, const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & death_times_to_use,const bool use_by_hand_worm_cluster_annotations,const bool warn_on_movement_problems) const;
	void output_summary(std::ostream & o) const;
};

class ns_death_time_annotation_compiler{


public:	
	typedef enum {ns_create_all,ns_do_not_create_regions,ns_do_not_create_regions_or_locations} ns_creation_type;
	typedef std::map<ns_64_bit,ns_death_time_annotation_compiler_region> ns_region_list;
	void clear(){regions.clear();}
	ns_region_list regions;
	
	
	void remove_all_but_specified_event_type(const ns_death_time_annotation_set::ns_annotation_type_to_load & t);
	void generate_survival_curve_set(ns_lifespan_experiment_set & survival_curves, const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & death_times_to_use,const bool use_by_hand_worm_cluster_annotations, const bool warn_on_movement_problems) const;
	void generate_animal_event_method_comparison(std::ostream & o,  double & total_death_mean_squared_error, double & total_expansion_mean_squared_error, double & total_contraction_mean_squared_error, ns_64_bit & death_N, ns_64_bit & expansion_N, ns_64_bit & contraction_N) const;
	
	#ifdef NS_GENERATE_IMAGE_STATISTICS
	void generate_detailed_animal_data_file(const bool output_region_image_data,const ns_capture_sample_region_statistics_set & region_data,std::ostream & o) const;
	#else
	void generate_detailed_animal_data_file(const bool output_region_image_data,std::ostream & o) const;
	#endif
	void add(const ns_death_time_annotation_set & set,const ns_creation_type = ns_create_all);
	void add(const ns_death_time_annotation_set & set,const ns_region_metadata & metadata);
	void add(const ns_death_time_annotation_compiler & compiler, const ns_creation_type = ns_create_all);
	void add(const ns_death_time_annotation & e,const ns_region_metadata & metadata);
	void add_path(const ns_64_bit &region_info_id, const ns_stationary_path_id &p, const ns_vector_2i & position, const ns_vector_2i & size, const ns_region_metadata & metadata);
	

	void generate_validation_information(std::ostream & o) const;


	void specifiy_region_metadata(const ns_64_bit region_id,const ns_region_metadata & metadata);

	void normalize_times_to_zero_age();

	void output_summary(std::ostream & o) const;
};


class ns_multiple_worm_cluster_death_annotation_handler{
public:
	//we need the properties to make sure generated annotations have correct excluded, flag, etc information
	static bool generate_correct_annotations_for_multiple_worm_cluster(
				const ns_death_time_annotation::ns_multiworm_censoring_strategy & censoring_strategy,
				const ns_death_time_annotation & properties,
				const ns_dying_animal_description_set_const & d, 
				ns_death_time_annotation_set & set,
				const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & death_times_to_use);
	
	static bool generate_correct_annotations_for_multiple_worm_cluster(
				const ns_death_time_annotation & properties,
				const ns_dying_animal_description_set_const & d,
				ns_death_time_annotation_set & set,
				const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & death_times_to_use);
};

/*
class ns_death_time_annotation_producer{
	virtual void produce_death_time_annotations(ns_death_time_annotation_set & set, ns_worm_movement_summary_series & series) const=0;
};
*/
#endif
