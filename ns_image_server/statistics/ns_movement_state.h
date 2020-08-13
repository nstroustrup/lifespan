#ifndef NS_MOVEMENT_STATE_H
#define NS_MOVEMENT_STATE_H
#include <string>

typedef enum{ 
			ns_movement_stationary, 
			ns_movement_posture, 
			ns_movement_slow, 
			ns_movement_fast, 
			ns_movement_by_hand_excluded,
			ns_movement_machine_excluded,
			ns_movement_death_associated_expansion,
			ns_movement_death_associated_post_expansion_contraction,
			ns_movement_total,
			ns_movement_not_calculated,
			ns_movement_number_of_states
		} ns_movement_state;

std::string ns_movement_state_to_string(const ns_movement_state & s);
std::string ns_movement_state_to_string_short(const ns_movement_state & s);

class ns_movement_data_source_type{
public:
	typedef enum{ns_time_path_analysis_data,ns_time_path_image_analysis_data,ns_triplet_data,ns_triplet_interpolated_data,ns_all_data} type;
	static std::string type_string(const type & t);
};


typedef enum{ns_no_movement_event,				//nothing happens at the specified time
			ns_translation_cessation,			//the animal's center of mass stops moving and afterwards only posture changes
			ns_movement_cessation,				//the animal stops moving altogether and appears dead.
			ns_fast_movement_cessation,			//the animal comes to rest within a small bounding box and moves within it.
			ns_fast_moving_worm_observed,		//a fast moving animal was seen at the specified time point
												//used for marking animals still moving at the end of a time period,
			ns_movement_censored_worm_observed,	//either static noise or non-stationary_object
			
			ns_slow_moving_worm_observed,
			ns_posture_changing_worm_observed,
			ns_stationary_worm_observed,
			ns_stationary_worm_disappearance,
			ns_death_associated_expansion_stop,
			ns_moving_worm_disappearance,
			ns_additional_worm_entry,
			ns_death_associated_expansion_start,
			ns_death_associated_expansion_observed,
			ns_death_associated_post_expansion_contraction_start,
			ns_death_associated_post_expansion_contraction_stop,
			ns_death_associated_post_expansion_contraction_observed,
			ns_number_of_movement_event_types
} ns_movement_event;

std::string ns_movement_event_to_string(const ns_movement_event & t);
std::string ns_movement_event_to_label(const ns_movement_event & t);
bool ns_movement_event_is_a_state_transition_event(const ns_movement_event & t);	
bool ns_movement_event_is_a_state_observation(const ns_movement_event & t);	
ns_movement_state ns_movement_event_state(const ns_movement_event & e);

typedef enum {
	ns_hmm_missing,
	ns_hmm_moving_vigorously,
	ns_hmm_moving_weakly,
	ns_hmm_moving_weakly_expanding,
	ns_hmm_moving_weakly_post_expansion,
	ns_hmm_not_moving_alive,
	ns_hmm_not_moving_expanding,
	ns_hmm_not_moving_dead,
	ns_hmm_contracting_post_expansion,
	ns_hmm_unknown_state,
	ns_number_of_state_types
} ns_hmm_movement_state;
typedef std::pair< ns_hmm_movement_state, ns_hmm_movement_state> ns_hmm_state_transition;

std::string ns_hmm_movement_state_to_string(const ns_hmm_movement_state & t);
ns_hmm_movement_state ns_hmm_movement_state_from_string(const std::string & s);
bool ns_string_is_a_state_not_a_transition(std::string& s);
std::string& ns_hmm_state_transition_to_string(const ns_hmm_state_transition& s);
ns_hmm_state_transition ns_hmm_state_transition_from_string(const std::string& s);
#endif
