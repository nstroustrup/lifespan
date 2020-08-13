#include "ns_death_time_annotation_set.h"
#ifndef NS_ONLY_IMAGE_ACQUISITION
#include "ns_time_path_solver.h"
#endif
#include "ns_xml.h"
using namespace std;
#include <cfloat> 

std::string ns_death_time_annotation_set::annotation_types_to_string(const ns_annotation_type_to_load & t){
		switch(t){
			case ns_all_annotations:return "all annotations";
			case ns_censoring_data: return "censoring annotations";
			case ns_movement_transitions:return "movement transition annotations";
			case ns_movement_states:return "movement state annotations";
			case ns_censoring_and_movement_transitions:return "censoring and movement transition annotations";
			case ns_censoring_and_movement_states:return "censoring and movement state annotations";
			case ns_recalculate_from_movement_quantification_data:return "movement quantification data and recalculating";
			case ns_no_annotations:return "no annotations";
			default: throw ns_ex("Unkown ns_annotation_type_to_load::") << (int)t;
		}
	}


unsigned long ns_death_time_annotation::exclusion_value(const ns_exclusion_type & t){
	if ((int)t < (int)ns_number_of_exclusion_values)
		return (unsigned long)t;
	else
	throw ns_ex("ns_death_time_annotation::exclusion_value()::Unknown exclusion type") << (int)t;
}
std::string ns_death_time_annotation::exclusion_string(const ns_exclusion_type & t){
		switch(t){
			case ns_not_excluded: return "Not Excluded or Censored";
			case ns_excluded: return "Excluded";
			case ns_machine_excluded: return "Excluded By Machine";
			case ns_by_hand_excluded: return "Excluded By Hand";
			case ns_both_excluded: return "Excluded By Machine and Hand";
			case ns_censored: return "Censored";
			case ns_excluded_and_censored: return "Censored and Excluded";
			case ns_multiworm_censored: return "Censored upon entering a multiple worm cluster";
			case ns_missing_censored: return "Censored upon going missing";
			case ns_censored_at_end_of_experiment: return "Still alive at end of the experiment";
			default: throw ns_ex("out_JMP_data()::Unknown censoring type:") << (int)t;
		}
}

ns_vector_2i ns_pos_from_str(const std::string & str){
	string::size_type p = str.find(",");
	if (p==string::npos)
		throw ns_ex("Could not interpret position " ) << str;
	return ns_vector_2i(atol(str.substr(0,p).c_str()),atol(str.substr(p+1,string::npos).c_str()));
}

std::string ns_death_time_annotation::brief_description() const {
	bool movement_event(type != ns_no_movement_event);
	bool excluded_event(excluded != ns_death_time_annotation::ns_not_excluded);
	bool flagged_event(flag.specified());
	bool multip_worm_event(number_of_worms() > 0);

	bool volt(volatile_duration_of_time_not_fast_moving ||
		!volatile_time_at_death_associated_expansion_start.fully_unbounded() || !volatile_time_at_death_associated_expansion_end.fully_unbounded() ||
		volatile_time_at_death_associated_post_expansion_contraction_end.fully_unbounded() || volatile_time_at_death_associated_post_expansion_contraction_end.fully_unbounded() ||
		volatile_matches_machine_detected_death);

	//bool has_sticky_properties();

	std::string ret;
	if (movement_event) {
		ret += "[" + ns_movement_event_to_string(type) +"]";
	}
	else ret += "[No movement event]";
	ret += ":";
	if (excluded_event)
		if (is_censored())
			ret += "(" + censor_description(excluded) + ")";
		else if (is_excluded())
			ret += "(Excluded)";
		else ret += "?";

	if (flagged_event)ret += "(" + flag.label() +")";

	if (volt) ret += "(volatile)";
	if (inferred_animal_location) ret += "(Inferred)";
	ret += " @ " + ns_format_time_string_for_human(time.period_end);
	ret += " by " + source_type_to_string(annotation_source) + " @ " + ns_format_time_string_for_human(annotation_time);

		return ret;

}
std::string ns_death_time_annotation::description() const{
	string tmp = ns_movement_event_to_string(type) + "::" + source_type_to_string(annotation_source) +
		"::" + exclusion_string(excluded) + "::pos(" + ns_to_string(position.x) + "," + ns_to_string(position.y) +
		"): size(" + ns_to_string(size.x) + "," + ns_to_string(size.y) + ") :" + ns_to_string(time.period_start) + "-" + ns_to_string(time.period_end) + "(" + ns_format_time_string_for_human(time.period_start) + "-" + ns_format_time_string_for_human(time.period_end) + ") "
		" path_id(" + ns_to_string(stationary_path_id.group_id) + "," + ns_to_string(stationary_path_id.path_id) + ") " + ns_to_string(stationary_path_id.detection_set_id) + ") ";
	switch (disambiguation_type) {
	case ns_death_time_annotation::ns_single_worm:
		tmp += "Single Worm";
		break;
	case ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:
		tmp += "Multi-worm cluster";
		break;
	case ns_death_time_annotation::ns_inferred_censoring_event:
		tmp += "Inferred censoring event";
		break;
	default:throw ns_ex("Unknown disambiguation type");
	}
	tmp +=  " [Machine : " + ns_to_string(number_of_worms_at_location_marked_by_machine ) + " / Hand : " + ns_to_string(number_of_worms_at_location_marked_by_hand) + " extra worms at location ] Flag: " + flag.label() +
				+ " source(" + source_type_to_string(annotation_source) + ")";
	return tmp;
}
void ns_death_time_annotation_set::add(const ns_death_time_annotation& a) { 
	//if (a.type == ns_no_movement_event && !(a.is_excluded())) {
	//	cerr << "WEIRD";
	//}
	events.push_back(a); }
void ns_death_time_annotation_set::add(const ns_death_time_annotation_set& s) {
	events.insert(events.end(), s.events.begin(), s.events.end());
}
bool ns_death_time_annotation_set::compare(const ns_death_time_annotation_set& set) const {
	std::set<std::string> e;
	for (unsigned int i = 0; i < events.size(); i++)
		e.emplace(events[i].to_string(true));
	bool written_header(false);
	for (unsigned int i = 0; i < set.events.size(); i++) {
		auto p = e.find(set.events[i].to_string(true));
		if (p != e.end())
			e.erase(p);
		else {
			if (!written_header) {
				cout << "Missing in first:\n";
				written_header = true;
			}
			cout << "A: " << set.events[i].to_string(true) << "\n";
			cout << "P: " << events[i].to_string(true) << "\n";
		}
	}
	if (!e.empty()) {
		cout << "Missing in second:\n";
		for (auto p = e.begin(); p != e.end(); p++)
			cout << *p << "\n";
		return false;
	}
	return !written_header;
}
std::string ns_death_time_annotation::to_string(bool zero_out_annotation_time) const{
	const int len(35);
	std::string s[len];
	s[0] = ns_to_string(exclusion_value(excluded));
	s[1] = zero_out_annotation_time?"0":ns_to_string(loglikelihood); //room for expansion; the old extra worms at location field
	s[2] = ns_to_string((unsigned long)disambiguation_type);
	s[3] = ns_to_string((unsigned long)type);
	std::string as(annotation_source_details);
	for (unsigned int i = 0; i < as.size(); i++)
		if (as[i]==',') as[i]='|';

	s[4] = as;
	s[5] = zero_out_annotation_time?"0":ns_to_string(annotation_time);
	s[6] = ns_to_string(time.period_end);
	s[7] = ns_to_string(region_info_id);
	s[8] = ns_to_string(region_id);
	s[9] = ns_to_string(position.x);
	s[10] = ns_to_string(position.y);
	s[11] = ns_to_string(size.x);
	s[12] = ns_to_string(size.y);
	s[13] = ns_to_string((int)annotation_source);
	s[14] = ns_to_string(stationary_path_id.group_id);
	s[15] = ns_to_string(stationary_path_id.path_id);
	s[16] = ns_to_string(stationary_path_id.detection_set_id);
	s[17] = ns_to_string(time.period_start);
	s[18] = ns_to_string(time.interval_bound_code());
	s[19] = flag.label_short;
	s[20] = animal_is_part_of_a_complete_trace?"1":"0";
	s[21] = ns_to_string(number_of_worms_at_location_marked_by_machine);
	s[22] = ns_to_string(number_of_worms_at_location_marked_by_hand);
	s[23] = ns_to_string((int)multiworm_censoring_strategy);
	s[24] = ns_to_string((int)missing_worm_return_strategy);
	s[25] = ns_to_string(animal_id_at_position);
	s[26] = ns_to_string(event_observation_type);
	s[27] = ns_to_string(longest_gap_without_observation);
	s[28] = ns_to_string((int)by_hand_annotation_integration_strategy);
	s[29] = ns_to_string((int)inferred_animal_location);
	s[30] = ns_to_string(subregion_info.plate_subregion_id);
	s[31] = ns_to_string(subregion_info.nearest_neighbor_subregion_id);
	s[32] = ns_to_string(subregion_info.nearest_neighbor_subregion_distance.x);
	s[33] = ns_to_string(subregion_info.nearest_neighbor_subregion_distance.y);
	s[34] = ns_to_string((int)event_explicitness);
	string ret;
	ret+=s[0];
	for (unsigned int i = 1; i < len; i++){
		ret += ",";
		ret += s[i];
	}
	return ret;
}
void ns_death_time_annotation::from_string(const std::string v){
	const int expected_len(35);
	std::string s[expected_len];
	int len(1);
	for (unsigned int i = 0; i < v.size(); i++){
		if (v[i] == ',')
			len++;
		else s[len-1]+=v[i];
	}
	if (len != 27 && len != 23 && len != 35)
		throw ns_ex("Invalid annotation encoding: ") << v;

	excluded =(ns_exclusion_type)atol(s[0].c_str());
	loglikelihood = atof(s[1].c_str());
	//bool extra_worms_specified(!s[1].empty());
	//unsigned long extra_worms_at_location = atol(s[1].c_str());

	//number_of_worms_at_location_marked_by_hand = atol(s[1].c_str());
	disambiguation_type = (ns_disambiguation_type)atol(s[2].c_str());
	type = (ns_movement_event)atol(s[3].c_str());
	annotation_source_details = s[4];
	for (unsigned int i = 0; i < annotation_source_details.size(); i++)
		if (annotation_source_details[i]=='|') annotation_source_details[i]=',';

	annotation_time = (unsigned long)ns_atoi64(s[5].c_str());
	time.period_end = (unsigned long)ns_atoi64(s[6].c_str());
	region_info_id = ns_atoi64(s[7].c_str());
	region_id = ns_atoi64(s[8].c_str());
	position.x = atol(s[9].c_str());
	position.y = atol(s[10].c_str());
	size.x = atol(s[11].c_str());
	size.y = atol(s[12].c_str());
	annotation_source = (ns_annotation_source_type)atol(s[13].c_str());
	stationary_path_id.group_id = atol(s[14].c_str());
	stationary_path_id.path_id = atol(s[15].c_str());

	stationary_path_id.detection_set_id = ns_atoi64(s[16].c_str());
	time.period_start = (unsigned long)ns_atoi64(s[17].c_str());
	const unsigned long time_interval_observation_code(atol(s[18].c_str()));
	if (time_interval_observation_code % 2 == 1)
		time.period_start_was_not_observed = true;
	if (time_interval_observation_code > 1)
		time.period_end_was_not_observed = true;
	//s[18];
	//if (s[18].size() == 0)
	//	flag = ns_death_time_annotation_flag::none();
	//else{
		//flag.id = atol(s[18].c_str());
		//flag.label_short = s[19].c_str();
	time.from_interval_bound_code(atol(s[18].c_str()));
	flag.label_short = s[19];
	//}
	animal_is_part_of_a_complete_trace = s[20]=="1";
	//if (s.size() == 23){
	number_of_worms_at_location_marked_by_machine = atol(s[21].c_str());
		//if (extra_worms_specified)
		//	number_of_worms_at_location_marked_by_hand = extra_worms_at_location+1;
		//else
	number_of_worms_at_location_marked_by_hand = atol(s[22].c_str());


	multiworm_censoring_strategy = (ns_multiworm_censoring_strategy)atol(s[23].c_str());
	missing_worm_return_strategy = (ns_missing_worm_return_strategy)atol(s[24].c_str());
	animal_id_at_position = atol(s[25].c_str());
	event_observation_type = (ns_death_time_annotation::ns_event_observation_type)atol(s[26].c_str());
	longest_gap_without_observation = atol(s[27].c_str());
	by_hand_annotation_integration_strategy = (ns_death_time_annotation::ns_by_hand_annotation_integration_strategy)atol(s[28].c_str());
	inferred_animal_location = s[29] == "1";

	subregion_info.plate_subregion_id = atoi(s[30].c_str());
	subregion_info.nearest_neighbor_subregion_id = atoi(s[31].c_str());
	subregion_info.nearest_neighbor_subregion_distance.x = atoi(s[32].c_str());
	subregion_info.nearest_neighbor_subregion_distance.y = atoi(s[33].c_str());
	event_explicitness = (ns_event_explicitness)atoi(s[34].c_str());
}

std::string ns_death_time_annotation::source_type_to_string(const ns_annotation_source_type & t){
	switch(t){
		case ns_unknown: return "Unknown";
		case ns_posture_image: return "Posture Image Analysis";
		case ns_region_image:  return "Region Image Analysis";
		case ns_lifespan_machine: return "Automated Analysis";
		case ns_storyboard: return "Storyboard Annotation";
		default: return std::string("Unknown source type(") + ns_to_string(t) + ")";// throw ns_ex("Unknown source type:") << t;
	}
}
std::string ns_movement_event_to_string(const ns_movement_event & t){
	switch(t){
		case ns_no_movement_event:			return "none";
		case ns_translation_cessation:		return "translation cessation";
		case ns_movement_cessation:			return "movement cessation";
		case ns_fast_movement_cessation:			return "fast movement cessation";
		case ns_fast_moving_worm_observed:			return "fast moving worm observed";
		case ns_movement_censored_worm_observed: return "movement-censored worm observed";
		case ns_slow_moving_worm_observed: return "slow-moving worm observed";
		case ns_posture_changing_worm_observed: return "posture-changing worm observed";
		case ns_stationary_worm_observed: return "stationary worm observed";
		case ns_death_associated_expansion_stop: return "death-associated expansion stop";
		case ns_stationary_worm_disappearance: return "stationary worm disappeared";
		case ns_moving_worm_disappearance: return "moving worm dissapeared";
		case ns_additional_worm_entry: return "additional worm entered path";
		case ns_death_associated_expansion_start: return "death-associated expansion start";
		case ns_death_associated_post_expansion_contraction_start: return "post-expansion contraction start";
		case ns_death_associated_post_expansion_contraction_stop: return "post-expansion contraction stop";
		case ns_death_associated_post_expansion_contraction_observed: return "post-expansion contraction observed";
		case ns_death_associated_expansion_observed: return "death-associated expansion observed";
		default:
			throw ns_ex("ns_movement_event_to_string()::Unknown event type ") << (int)t;
	}
}
std::string ns_movement_event_to_label(const ns_movement_event & t){
	switch(t){
		case ns_no_movement_event: return "none";
		case ns_translation_cessation: return "local_movement_cessation";
		case ns_movement_cessation: return "death";
		case ns_fast_movement_cessation: return "long_distance_movement_cessation";
		case ns_fast_moving_worm_observed: return "fast moving worm observed";
		case ns_movement_censored_worm_observed: return "movement_censored_worm_observed";
		case ns_slow_moving_worm_observed: return "slow_moving_worm_observed";
		case ns_posture_changing_worm_observed: return "posture_changing_worm_observed";
		case ns_stationary_worm_observed: return "";
		case ns_death_associated_expansion_stop: return "death_expansion_stop";
		case ns_stationary_worm_disappearance: return "";
		case ns_moving_worm_disappearance: return "";
		case ns_additional_worm_entry: return "";
		case ns_death_associated_expansion_start: return "death_expansion_start";
		case ns_death_associated_expansion_observed: return "death_expansion_observed";
		case ns_death_associated_post_expansion_contraction_start: return "post_expansion_contraction_start";
		case ns_death_associated_post_expansion_contraction_stop: return "post_expansion_contraction_stop";
		case ns_death_associated_post_expansion_contraction_observed: return "post_expansion_contraction_observed";
		default: throw ns_ex("ns_movement_event_to_string()::Unknown event type ") << (int)t;
	}
}


std::string ns_hmm_movement_state_to_string(const ns_hmm_movement_state & t) {
	switch (t) {
	case ns_hmm_moving_vigorously: return "moving vigorously";
	case ns_hmm_missing: return "missing";
	case ns_hmm_moving_weakly: return "moving weakly";
	case ns_hmm_moving_weakly_expanding: return "moving weakly; expanding";
	case ns_hmm_moving_weakly_post_expansion: return "moving weakly; post expansion";
	case ns_hmm_not_moving_alive: return "not moving; alive";
	case ns_hmm_not_moving_expanding: return "not moving; expanding";
	case ns_hmm_not_moving_dead: return "not moving; dead";
	case ns_hmm_unknown_state :return "unknown";
	case ns_hmm_contracting_post_expansion: return "contracting after expansion";
	default: throw ns_ex("ns_hmm_movement_state_to_string():Unknown hmm movement state");
	}
}
ns_hmm_movement_state ns_hmm_movement_state_from_string_nothrow(const std::string& s) {
	if (s == "moving vigorously")
		return ns_hmm_moving_vigorously;
	if (s == "missing")
		return ns_hmm_missing;
	if (s == "moving weakly")
		return ns_hmm_moving_weakly;
	if (s == "moving weakly; expanding")
		return ns_hmm_moving_weakly_expanding;
	if (s == "moving weakly; post expansion")
		return ns_hmm_moving_weakly_post_expansion;
	if (s == "not moving; alive")
		return ns_hmm_not_moving_alive;
	if (s == "not moving; expanding")
		return ns_hmm_not_moving_expanding;
	if (s == "not moving; dead")
		return ns_hmm_not_moving_dead;
	if (s == "contracting after expansion")
		return ns_hmm_contracting_post_expansion;
	if (s == "unknown")
		return ns_hmm_unknown_state;
	return ns_number_of_state_types;
	throw ns_ex("ns_hmm_movement_state_from_string():Unknown state ") << s;
}
bool ns_string_is_a_state_not_a_transition(std::string & s){
	return (ns_hmm_movement_state_from_string_nothrow(s) != ns_number_of_state_types);
}
 ns_hmm_movement_state ns_hmm_movement_state_from_string(const std::string & s) {
	ns_hmm_movement_state ss = ns_hmm_movement_state_from_string_nothrow(s);
	if (ss == ns_number_of_state_types)
		throw ns_ex("ns_hmm_movement_state_from_string():Unknown state ") << s;
	return ss;
}
 std::string& ns_hmm_state_transition_to_string(const ns_hmm_state_transition & s) {
	 return ns_hmm_movement_state_to_string(s.first) + "->" + ns_hmm_movement_state_to_string(s.second);
 }
 ns_hmm_state_transition ns_hmm_state_transition_from_string(const std::string& s) {
	 auto p = s.find("->");
	 if (p == s.npos)
		 throw ns_ex("Could not parse state transition: ") << s;
	 ns_hmm_state_transition trans;
	 trans.first = ns_hmm_movement_state_from_string(s.substr(0, p));
	 trans.second = ns_hmm_movement_state_from_string(s.substr(p+2,s.npos));
	 return trans;
 }

bool ns_movement_event_is_a_state_transition_event(const ns_movement_event & t){
	switch(t){
		case ns_fast_movement_cessation:
		case ns_translation_cessation:
		case ns_movement_cessation:
		case ns_death_associated_expansion_stop:
		case ns_death_associated_expansion_start:
		case ns_death_associated_post_expansion_contraction_start:
		case ns_death_associated_post_expansion_contraction_stop:
		case ns_moving_worm_disappearance:
		case ns_stationary_worm_disappearance:
		case ns_additional_worm_entry:
			return true;

		case ns_movement_censored_worm_observed:
		case ns_fast_moving_worm_observed:
		case ns_slow_moving_worm_observed:
		case ns_posture_changing_worm_observed:
		case ns_death_associated_expansion_observed:
		case ns_death_associated_post_expansion_contraction_observed:
		case ns_stationary_worm_observed: return false;

		case ns_no_movement_event: return false;
		default: throw ns_ex("ns_movement_event_is_a_state_transition_event()::Unknown event type ") << (int)t;
	};
}


bool ns_movement_event_is_a_state_observation(const ns_movement_event & t){
	return (t != ns_no_movement_event) && !ns_movement_event_is_a_state_transition_event(t);
};

std::string ns_movement_state_to_string(const ns_movement_state & s){
	switch(s){
			case ns_movement_stationary: return "Dead";
			case ns_movement_posture: return "Changing Posture";
			case ns_movement_slow:  return "Moving Slowly";
			case ns_movement_fast:  return "Moving Quickly";
			case ns_movement_by_hand_excluded: return "By Hand Excluded";
			case ns_movement_machine_excluded: return "Machine Excluded";
			case ns_movement_death_associated_expansion: return "Death-associated Contraction";
			case ns_movement_death_associated_post_expansion_contraction: return "Death-associated post-contraction Expansion";
			case ns_movement_total: return "Total";
			case ns_movement_not_calculated: return "Not calculated";
			case ns_movement_number_of_states: throw ns_ex("ns_movement_state_to_string()::Invalid movement state: ") << (int)s;
			default: throw ns_ex("ns_movement_state_to_string()::Unknown movement state: ") << (int)s;
	}
}

std::string ns_movement_state_to_string_short(const ns_movement_state & s) {
	switch (s) {
	case ns_movement_stationary: return "Dead";
	case ns_movement_posture: return "Pos";
	case ns_movement_slow:  return "Slow";
	case ns_movement_fast:  return "Fast";
	case ns_movement_by_hand_excluded: return "EX H";
	case ns_movement_machine_excluded: return "EX M";
	case ns_movement_death_associated_expansion: return "DAE";
	case ns_movement_death_associated_post_expansion_contraction: return "DAPEC";
	case ns_movement_total: return "Total";
	case ns_movement_not_calculated: return "NA";
	case ns_movement_number_of_states: throw ns_ex("ns_movement_state_to_string()::Invalid movement state: ") << (int)s;
	default: throw ns_ex("ns_movement_state_to_string()::Unknown movement state: ") << (int)s;
	}
}
ns_movement_state ns_movement_event_state(const ns_movement_event & e){
	switch(e){
	case ns_no_movement_event:
	case ns_stationary_worm_disappearance:
				return ns_movement_not_calculated;
	case ns_translation_cessation:
	case ns_posture_changing_worm_observed:
		return ns_movement_posture;
	case ns_movement_cessation:
	case ns_stationary_worm_observed:
		return ns_movement_stationary;
	case ns_fast_movement_cessation:
	case ns_slow_moving_worm_observed:
		return ns_movement_slow;
	case ns_fast_moving_worm_observed:
		return ns_movement_fast;
	case ns_movement_censored_worm_observed:
		return ns_movement_machine_excluded;
	default: throw ns_ex("ns_movement_event_state()::Unknown event type:") << (int)e;
	}
}
bool ns_death_time_annotation_set::annotation_matches(const ns_annotation_type_to_load & type,const ns_death_time_annotation & e){
	switch(type){
		case ns_all_annotations: return true;
		case ns_censoring_data:
			return e.is_censored() || e.is_excluded() || e.flag.specified()
				|| (ns_censor_machine_multiple_worms && e.number_of_worms_at_location_marked_by_machine > 1) || e.number_of_worms_at_location_marked_by_hand > 0;

		case ns_movement_transitions:
			return ns_movement_event_is_a_state_transition_event(e.type);
		case ns_censoring_and_movement_transitions:
			return e.is_censored() || e.is_excluded() || e.flag.specified()||ns_movement_event_is_a_state_transition_event(e.type) || (ns_censor_machine_multiple_worms && e.number_of_worms_at_location_marked_by_machine > 1) || e.number_of_worms_at_location_marked_by_hand > 0;

		case ns_movement_states:
			return ns_movement_event_is_a_state_observation(e.type);
		case ns_censoring_and_movement_states:
			return e.is_censored() || e.is_excluded() || e.flag.specified() || ns_movement_event_is_a_state_observation(e.type) || (ns_censor_machine_multiple_worms && e.number_of_worms_at_location_marked_by_machine > 1) || e.number_of_worms_at_location_marked_by_hand > 0;

		case ns_recalculate_from_movement_quantification_data:
			throw ns_ex("Reclaculation event request dispatched to a death time annotation set!");
		case ns_no_annotations:
			return false;
		throw ns_ex("Unknown annotation_type_to_load:") << (int)type;
	}
	return false;
}

#ifndef NS_ONLY_IMAGE_ACQUISITION
void ns_death_time_annotation_compiler_region::output_visualization_csv(std::ostream & o,const bool output_header) const{
	if (output_header)
		ns_time_path_solution::output_visualization_csv_header(o);
	for (ns_location_list::const_iterator p = locations.begin(); p != locations.end(); ++p){
		for (unsigned int i = 0; i < p->annotations.size(); ++i){
			ns_movement_state state;
			if (p->properties.excluded == ns_death_time_annotation::ns_by_hand_excluded)
				state = ns_movement_by_hand_excluded;
			else if (p->properties.excluded == ns_death_time_annotation::ns_machine_excluded)
				state = ns_movement_machine_excluded;
			else{
				switch(p->annotations[i].type){
					case ns_no_movement_event:
						state = ns_movement_fast; break;
					case ns_translation_cessation:
						state = ns_movement_posture; break;
					case ns_movement_cessation:
						state = ns_movement_stationary; break;
					case ns_fast_movement_cessation:
						state = ns_movement_slow; break;
					case ns_fast_moving_worm_observed:
						state = ns_movement_fast; break;
					case ns_death_associated_post_expansion_contraction_start:
					case ns_death_associated_post_expansion_contraction_stop:
						state = ns_movement_death_associated_post_expansion_contraction;
						break;
					case ns_death_associated_expansion_start:
					case ns_death_associated_expansion_stop:
						state = ns_movement_death_associated_expansion;
						break;
					case ns_movement_censored_worm_observed:
						state = ns_movement_machine_excluded;
						break;
					default: throw ns_ex("ns_death_time_annotation_compiler_region::output_visualization_csv()::Unknown event type:") << p->annotations[i].type;
				}
			}
			ns_time_path_solution::output_visualization_csv_data(o,
				(p->annotations[i].time.period_end-metadata.time_at_which_animals_had_zero_age)/(24.0*60.0*60.0),
				p->annotations[i].time.period_end,
				p->properties.position,
				p->properties.size,
				0,0,0,0,
				false,false,
				1,p->annotations[i].number_of_worms_at_location_marked_by_hand,
				state,p->annotations[i].subregion_info,p->annotations[i]
			);
		}
	}
}
#endif
void ns_death_time_annotation_set::remove_all_but_specified_event_type(const ns_annotation_type_to_load & t){
	for (std::vector<ns_death_time_annotation>::iterator p = events.begin(); p!= events.end();){
		if (!annotation_matches(t,*p))
			p = events.erase(p);
		else p++;
	}
}
void ns_death_time_annotation_set::read(const  ns_annotation_type_to_load & type,std::istream & i, const bool exclude_fast_moving_animals){
	char a(i.peek());
	if (i.fail())
		throw ns_ex("ns_death_time_annotation_set::read()::Could not read file");
	if (a == '<')
		read_xml(type,i);
	else read_column_format(type,i,exclude_fast_moving_animals);
}
void ns_death_time_annotation_set::write(std::ostream & o) const{
	write_column_format(o);
}

void ns_death_time_annotation::write_column_format_data(std::ostream & o) const{
	o << (int)type << "," //0
		<< time.period_end << ","
		<< region_id << ","
		<< region_info_id << ","
		<< position.x << ","
		<< position.y << ","
		<< size.x << ","
		<< size.y << ","
		<< (int)annotation_source << ","
		<< annotation_time << ","
		<< annotation_source_details << "," // 10
		<< ns_death_time_annotation::exclusion_value(excluded) << ","
		<< loglikelihood << "," //room for expansion; old location of extra worms
		<< (int)disambiguation_type << ","
		<< stationary_path_id.group_id << ","
		<< stationary_path_id.path_id << ","
		<< stationary_path_id.detection_set_id << ","
		<< time.period_start << ","
		<< time.interval_bound_code() << ","
		<< flag.label_short << ","
		<< animal_is_part_of_a_complete_trace << "," //20
		<< number_of_worms_at_location_marked_by_machine << ","
		<< number_of_worms_at_location_marked_by_hand << ","
		<< (int)multiworm_censoring_strategy << ","
		<< (int)missing_worm_return_strategy << ","
		<< animal_id_at_position << ","
		<< (int)event_observation_type << ","
		<< longest_gap_without_observation << ","
		<< (int)by_hand_annotation_integration_strategy << ","
		<< (inferred_animal_location ? "1" : "0") << ","
		<< subregion_info.plate_subregion_id << "," //30
		<< subregion_info.nearest_neighbor_subregion_id << ","
		<< subregion_info.nearest_neighbor_subregion_distance.x << ","
		<< subregion_info.nearest_neighbor_subregion_distance.y << ","
		<< (int)event_explicitness << ",";  //34
		// one column reserved for future use
}
void write_column_format_header(std::ostream & o){
	o << "Event Type,Event Time, Region Image Id, Region Info Id, Position x, Position y, Size X, Size y, Annotation Source,"
		"Annotation Finish Time, Annotation Source Details, Censored, Loglikelihood, Disambiguation Type, Stationary Worm Group ID,"
		 "Stationary Worm Path Id, Stationary Worm Detection Set ID,Annotation Start Time,Flag Id,Flag Label,"
		 "Complete Trace,Number of Worms Annotated By machine,Number of Worms Annotated By Hand,"
		 "Multiple worm cluster censoring strategy,Missing Worm Return Strategy,Extra Animal ID at position,Event Observation Type,Longest gap without observation,"
		 "By Hand Annotation Integration Strategy,Inferred animal location,Subregion info id,Nearest neighbor subregion id,Nearest neighbor subregion X,Nearest neighbor subregion Y,Event Explictness,(RFE)\n";
}

void ns_death_time_annotation_set::write_split_file_column_format(std::ostream & censored_and_transition_file, std::ostream & state_file)const{
	write_column_format_header(censored_and_transition_file);
	write_column_format_header(state_file);
	for (unsigned int i = 0; i < events.size(); i++){
		if (!annotation_matches(ns_movement_states, events[i]) || events[i].is_censored() || events[i].flag.specified()) {
			events[i].write_column_format_data(censored_and_transition_file);
			censored_and_transition_file << "\n";
		}
		else {
			events[i].write_column_format_data(state_file);
			state_file << "\n";
		}


	}

}

void ns_death_time_annotation_set::write_column_format(std::ostream & o)const{
	write_column_format_header(o);
	for (unsigned int i = 0; i < events.size(); i++) {
		events[i].write_column_format_data(o);
		o << "\n";
	}
	
}

char ns_conditional_getline(istream & i, std::string & val, const std::string & separators){
	char a;
	val.resize(0);
	while(!i.fail()){
		a = i.get();
		for (unsigned int i = 0; i < separators.size(); i++){
			if (a == separators[i])
				return a;
		}
		val+=a;
	}
	return 0;
}
void ns_death_time_annotation_set::read_column_format(const  ns_annotation_type_to_load & type_to_load, std::istream & i, const bool exclude_fast_moving_animals, const bool single_line){
	char a(' ');
	while (!i.fail() && isspace(a))
		a=i.peek();
	if (i.fail() || (!single_line && a != 'E'))
		throw ns_ex("Could not read death time annotation format");

	bool newest_old_record(false);
	bool kind_of_old_record(false);
	string val;
	if (!single_line) {
		getline(i, val, '\n');
		if (i.fail())
			throw ns_ex("Could not read death time annotation format");
		if (val.find("Event Observation Type") == val.npos)
			newest_old_record = true;
		{
			int cur_column(0);
			string column_text,
				previous_column_text;
			for (unsigned int i = 0; i < val.size(); i++) {
				if (val[i] == ',') {
					cur_column++;
					previous_column_text = column_text;
					column_text.resize(0);
				}
				else column_text.push_back(val[i]);
				if (cur_column == 13) {
					if (previous_column_text == " Number of Extra Worms")
						kind_of_old_record = true;
					break;
				}
				if (cur_column > 13)
					break;
			}
		}
	}
	ns_death_time_annotation annotation;
	val.resize(0);
	val.reserve(20);
	if (events.size() == 0)
		events.reserve(100);
	while(true){
		annotation.time.period_start = 0;
		annotation.time.period_end = 0;
		getline(i,val,',');							//0
		if (i.fail())
			return;
		events.resize(events.size()+1);
		ns_death_time_annotation & e(*events.rbegin());
		e.type = (ns_movement_event)atol(val.c_str());
		getline(i,val,',');						
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 1");
		e.time.period_end = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 2");
		e.region_id = ns_atoi64(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 3");
		e.region_info_id  = ns_atoi64(val.c_str());
		if (e.region_info_id == 0)
			throw ns_ex("ns_death_time_annotation_set::read_column_format()::Found an annotation with no region info specified");

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 4");
		e.position.x  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 5");
		e.position.y  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 6");
		e.size.x  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 7");
		e.size.y = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 8");
		e.annotation_source = (ns_death_time_annotation::ns_annotation_source_type)atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 9");
		e.annotation_time  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 10");
		e.annotation_source_details = val;

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 11");

		e.excluded = (ns_death_time_annotation::ns_exclusion_type)atol(val.c_str());
		//if we can specifiy a more specific source for the exclusion, do so.
		if (e.excluded==ns_death_time_annotation::ns_excluded){
			if (e.annotation_source == ns_death_time_annotation::ns_lifespan_machine)
				e.excluded= ns_death_time_annotation::ns_machine_excluded;
			else
				e.excluded = ns_death_time_annotation::ns_by_hand_excluded;
		}

		getline(i,val,',');
		bool old_spec_by_hand_worms = false;
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 12");
		if(val==""){
			old_spec_by_hand_worms = true;
			e.loglikelihood = 1;
		}
		else e.loglikelihood = atof(val.c_str());
		//unsigned long number_of_old_spec_extra_by_hand_worms = atol(val.c_str());
		;//!val.empty() && number_of_old_spec_extra_by_hand_worms != 0;
//		if (e.number_of_extra_worms_at_location != 0)
		//	cerr << "Extra worm found\n";
		getline(i,val,',');

		e.disambiguation_type = (ns_death_time_annotation::ns_disambiguation_type)atol(val.c_str());
		if (i.fail()){
			if (use_debug_read_columns){
				if (!annotation_matches(type_to_load,e))
					events.pop_back();
				return;
			}
			throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 13");
		}

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 14");
		e.stationary_path_id.group_id = atol(val.c_str());

	//	if (e.stationary_path_id.group_id == 28)
	//		cerr << "MA";
		//allow old style records to be read in.  They have 5 fewer records.
		const char delim(ns_conditional_getline(i,val,",\n"));					//15
		const bool really_old_style_record(delim==0 || delim=='\n');

		e.stationary_path_id.path_id = atol(val.c_str());

		if (!really_old_style_record){

			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 16");
			e.stationary_path_id.detection_set_id = ns_atoi64(val.c_str());

			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 17");
			e.time.period_start = atol(val.c_str());
			if (kind_of_old_record){
				getline(i,val,'\n');
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 18");
				if (!single_line)
					continue;
				else return;
			}
			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 18");
			//if (val.size() == 0)
			//e.flag = ns_death_time_annotation_flag::none();
			//else e.flag.id = atol(val.c_str());
			if (!newest_old_record){
				e.time.from_interval_bound_code(atol(val.c_str()));
			}
			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 19");
			e.flag.label_short = val;
			//e.flag.label_short = val;
			//if (e.flag.label_short.size() > 0)
			//	e.flag.id =

			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 20");
			e.animal_is_part_of_a_complete_trace = val=="1";

			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 21");
			e.number_of_worms_at_location_marked_by_machine = atol(val.c_str());

			//if (e.number_of_worms_at_location_marked_by_machine > 200)
			//	throw ns_ex("ns_death_time_annotation_set::Unlikely machine annotation for the number of worms at location:") << e.number_of_worms_at_location_marked_by_machine;

			const char delim(ns_conditional_getline(i,val,",\n"));
			const bool old_style_record(delim==0 || delim=='\n');

			e.number_of_worms_at_location_marked_by_hand = atol(val.c_str());
			if (!old_style_record){
				getline(i,val,',');
				e.multiworm_censoring_strategy = (ns_death_time_annotation::ns_multiworm_censoring_strategy)atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 23");
				getline(i,val,',');
				e.missing_worm_return_strategy = (ns_death_time_annotation::ns_missing_worm_return_strategy)atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 24");
				getline(i,val,',');
				e.animal_id_at_position = atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 25");
				getline(i,val,',');
				e.event_observation_type = (ns_death_time_annotation::ns_event_observation_type)atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 26");
				if (!newest_old_record){
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 27");
					e.longest_gap_without_observation = atol(val.c_str());
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 28");
					e.by_hand_annotation_integration_strategy = (ns_death_time_annotation::ns_by_hand_annotation_integration_strategy)atol(val.c_str());

					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 29");
					e.inferred_animal_location = (val=="1");



					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 30");
					e.subregion_info.plate_subregion_id = atol(val.c_str());
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 31");
					e.subregion_info.nearest_neighbor_subregion_id = atol(val.c_str());
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 32");
					e.subregion_info.nearest_neighbor_subregion_distance.x = atol(val.c_str());
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 33");
					e.subregion_info.nearest_neighbor_subregion_distance.y = atol(val.c_str());
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 34");
					e.event_explicitness = (ns_death_time_annotation::ns_event_explicitness)atoi(val.c_str());
					getline(i,val,'\n');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 35");
				}
				else{
					getline(i,val,'\n');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF 36");
					e.event_observation_type = ns_death_time_annotation::ns_standard;
				}
			}
			else{
				e.multiworm_censoring_strategy = ns_death_time_annotation::ns_unknown_multiworm_cluster_strategy;
				e.missing_worm_return_strategy = ns_death_time_annotation::ns_not_specified;
				e.animal_id_at_position = 0;
			}
			//WWW
		//	if (e.is_censored())
		//		cerr << "WHJA";
		}
		else{
			e.stationary_path_id.detection_set_id = 0;
			e.time.period_start = e.time.period_end;
			e.number_of_worms_at_location_marked_by_machine = 0;
			e.number_of_worms_at_location_marked_by_hand = 0;
			e.loglikelihood = 1;
			e.multiworm_censoring_strategy = ns_death_time_annotation::ns_unknown_multiworm_cluster_strategy;
			e.missing_worm_return_strategy = ns_death_time_annotation::ns_not_specified;
			e.animal_id_at_position = 0;
			e.subregion_info = ns_plate_subregion_info();
		}

	//	if (e.stationary_path_id.group_id == 28)
	//		cerr << "MA";

		//if (old_spec_by_hand_worms)
		//	e.number_of_worms_at_location_marked_by_hand = number_of_old_spec_extra_by_hand_worms+1;

		if (!annotation_matches(type_to_load,e) || ( exclude_fast_moving_animals && e.type == ns_fast_moving_worm_observed))
			events.pop_back();

		if (single_line || i.fail())
			return;
	}
}

void ns_death_time_annotation::transfer_sticky_properties(ns_death_time_annotation & a) const{
	if (excluded != ns_death_time_annotation::ns_not_excluded)
		a.excluded = excluded;
	if (disambiguation_type != ns_single_worm)
		a.disambiguation_type = disambiguation_type;
	if (flag.specified())
		a.flag = flag;
	if (inferred_animal_location)
		a.inferred_animal_location = true;
	if (a.number_of_worms_at_location_marked_by_hand < number_of_worms_at_location_marked_by_hand)
		a.number_of_worms_at_location_marked_by_hand = number_of_worms_at_location_marked_by_hand;
	if (a.number_of_worms_at_location_marked_by_machine < number_of_worms_at_location_marked_by_machine)
		a.number_of_worms_at_location_marked_by_machine = number_of_worms_at_location_marked_by_machine;

	//these should stick to individual annotations and not be contageous
	/*
	if (multiworm_censoring_strategy != ns_not_applicable)
		a.multiworm_censoring_strategy = multiworm_censoring_strategy;
	if (missing_worm_return_strategy != ns_not_specified)
		a.missing_worm_return_strategy = missing_worm_return_strategy;
		*/
}

std::string ns_death_time_annotation::multiworm_censoring_strategy_label(const ns_death_time_annotation::ns_multiworm_censoring_strategy & m){
	switch (m){
		case ns_unknown_multiworm_cluster_strategy: return "Unknown multiworm cluster strategy";
		case ns_no_strategy_needed_for_single_worm_object: return "No strategy needed for single worm object";
		case ns_include_as_single_worm_deaths: return "Include clusters as single deaths";
		case ns_right_censor_multiple_worm_clusters: return "Right Censor clusters";
		case ns_include_directly_observed_deaths_and_infer_the_rest: return "Include directly observed deaths and infer the rest";
		case ns_include_only_directly_observed_deaths: return "Include only directly observed deaths";
		case ns_by_hand_censoring: return "By hand censoring";
		case ns_merge_multiple_worm_clusters_and_missing_and_censor: return "Treat clusters as missing";
		default: throw ns_ex("ns_death_time_annotation::multiworm_censoring_strategy_label()::Unknown censoring strategy: ") << (int)m;
	}
}
std::string ns_death_time_annotation::by_hand_annotation_integration_strategy_label_short(const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & s){
	switch(s){
	case ns_only_machine_annotations:	return "machine_events";
	case ns_machine_annotations_if_no_by_hand: return "best_events";
	case ns_only_by_hand_annotations:	return "hand_events";
	case ns_machine_and_by_hand_annotations: return "both_events";
	default: throw ns_ex("ns_death_time_annotation::by_hand_annotation_integration_strategy_label_short()::Unknown strategy: ") << (int)s;
	}

}
std::string ns_death_time_annotation::multiworm_censoring_strategy_label_short(const ns_death_time_annotation::ns_multiworm_censoring_strategy & m){
	switch (m){
		case ns_unknown_multiworm_cluster_strategy: return "not_applicable";
		case ns_no_strategy_needed_for_single_worm_object: return "is_single_worm";
		case ns_discard_multi_worm_clusters: return "discard_clusters";
		case ns_include_as_single_worm_deaths: return "single_death";
		case ns_right_censor_multiple_worm_clusters: return "right_censor";
		case ns_include_directly_observed_deaths_and_infer_the_rest: return "interval_censor";
		case ns_include_only_directly_observed_deaths: return "multi_death";
		case ns_by_hand_censoring: return "by_hand";
		case ns_merge_multiple_worm_clusters_and_missing_and_censor: return "merge_missing";
		default: throw ns_ex("ns_death_time_annotation::multiworm_censoring_strategy_label()::Unknown censoring strategy: ") << (int)m;
	}
}
std::string ns_death_time_annotation::missing_worm_return_strategy_label(const ns_death_time_annotation::ns_missing_worm_return_strategy & m){
	switch(m){
	case ns_not_specified:
		return "Not Specified";
	case ns_censoring_minimize_missing_times:
		return "Minimal Duration";
	case ns_censoring_assume_uniform_distribution_of_missing_times:
		return "Random Duration";
	case ns_censoring_assume_uniform_distribution_of_only_large_missing_times:
		return "Two Stage Random Duraiton";
	default: throw ns_ex("ns_death_time_annotation::missing_worm_return_strategy_label()Unknown return strategy") << (int)m;
	}
}

std::string ns_death_time_annotation::missing_worm_return_strategy_label_short(const ns_death_time_annotation::ns_missing_worm_return_strategy & m){
	switch(m){
	case ns_not_specified:
		return "not_spec";
	case ns_censoring_minimize_missing_times:
		return "min";
	case ns_censoring_assume_uniform_distribution_of_missing_times:
		return "rand";
	case ns_censoring_assume_uniform_distribution_of_only_large_missing_times:
		return "min_rand";
	default: throw ns_ex("ns_death_time_annotation::missing_worm_return_strategy_label()Unknown return strategy") << (int)m;
	}
}
std::string ns_animals_that_slow_but_do_not_die_handling_strategy_label_short(const ns_animals_that_slow_but_do_not_die_handling_strategy & s){
	switch(s){
	case ns_include_unchanged:
		return "unaltered";
	case ns_force_to_fast_moving:
		return "force_fast";
	default: throw ns_ex("ns_animals_that_slow_but_do_not_die_handling_strategy_label()::Unknown Strategy: ") << (int)s;
	}
}
std::string ns_animals_that_slow_but_do_not_die_handling_strategy_label(const ns_animals_that_slow_but_do_not_die_handling_strategy & s){
	switch(s){
	case ns_include_unchanged:
		return "Do not alter incomplete paths";
	case ns_force_to_fast_moving:
		return "Force incomplete to fast moving";
	default: throw ns_ex("ns_animals_that_slow_but_do_not_die_handling_strategy_label()::Unknown Strategy: ") << (int)s;
	}
}


std::string ns_death_time_annotation::censor_description(const ns_death_time_annotation::ns_exclusion_type & t){
		switch (t){
			case ns_censored:
				return "Not Specified";
			case ns_multiworm_censored:
				return "Entered multi-worm cluster";
			case ns_missing_censored:
				return "Escaped machine detection";
			case ns_censored_at_end_of_experiment:
				return "Objects apparently moving at termination of measurement";
			case ns_excluded_and_censored:
				return "Both Excluded and Censored";
			default: throw ns_ex("ns_death_time_annotation::censor_description()::Not a censoring description: ") << (int)t;
		}
	}

void ns_death_time_annotation_set::read_xml(const ns_annotation_type_to_load & t,std::istream & i){
	ns_xml_simple_object_reader xml;

	xml.from_stream(i);
	events.resize(0);
	events.reserve(xml.objects.size());
	ns_death_time_annotation a;
	for (unsigned int i = 0; i < xml.objects.size(); i++){
		if (xml.objects[i].name != "e")
			throw ns_ex("Unknown element in death time annotation file: ") << xml.objects[i].name;
		a.type = (ns_movement_event)atol(xml.objects[i].tag("y").c_str());
		a.annotation_source = (ns_death_time_annotation::ns_annotation_source_type)atol(xml.objects[i].tag("u").c_str());
		a.time.period_end = atol(xml.objects[i].tag("t").c_str());
		a.time.period_start = atol(xml.objects[i].tag("tt").c_str());
		a.region_info_id = ns_atoi64(xml.objects[i].tag("i").c_str()),
		a.region_id = ns_atoi64(xml.objects[i].tag("r").c_str()),
		a.position = ns_pos_from_str(xml.objects[i].tag("p"));
		a.size = ns_pos_from_str(xml.objects[i].tag("s"));

		if (xml.objects[i].tag_specified("a"))
			a.annotation_time = atol(xml.objects[i].tag("a").c_str());
		else a.annotation_time = 0;

		//old style extra
		if (xml.objects[i].tag_specified("x"))
			a.number_of_worms_at_location_marked_by_hand = atol(xml.objects[i].tag("x").c_str())+1;
		else //new style count
			if (xml.objects[i].tag_specified("xh"))
			a.number_of_worms_at_location_marked_by_hand = atol(xml.objects[i].tag("xh").c_str());
		else a.number_of_worms_at_location_marked_by_hand = 0;


		if (xml.objects[i].tag_specified("xm"))
			a.number_of_worms_at_location_marked_by_machine = atol(xml.objects[i].tag("xm").c_str());
		else a.number_of_worms_at_location_marked_by_machine = 0;

		if (xml.objects[i].tag_specified("c"))
			a.excluded= (ns_death_time_annotation::ns_exclusion_type)atol(xml.objects[i].tag("c").c_str());
		else a.excluded = ns_death_time_annotation::ns_not_excluded;

		if (a.excluded==ns_death_time_annotation::ns_excluded){
			if (a.annotation_source == ns_death_time_annotation::ns_lifespan_machine)
				a.excluded = ns_death_time_annotation::ns_machine_excluded;
			else
				a.excluded = ns_death_time_annotation::ns_by_hand_excluded;
		}

		xml.objects[i].assign_if_present("d",a.annotation_source_details);

		if (xml.objects[i].tag_specified("g"))
			a.disambiguation_type = (ns_death_time_annotation::ns_disambiguation_type)atol(xml.objects[i].tag("g").c_str());

		if (xml.objects[i].tag_specified("q"))
			a.stationary_path_id.group_id= atol(xml.objects[i].tag("q").c_str());

		if (xml.objects[i].tag_specified("w"))
			a.stationary_path_id.path_id = (ns_death_time_annotation::ns_disambiguation_type)atol(xml.objects[i].tag("w").c_str());

		if (xml.objects[i].tag_specified("dt"))
			a.stationary_path_id.detection_set_id = (ns_death_time_annotation::ns_disambiguation_type)atol(xml.objects[i].tag("dt").c_str());

		if (annotation_matches(t,a))
			events.push_back(a);
	}
}
void  ns_death_time_annotation_set::write_xml(std::ostream & o) const{
	ns_xml_simple_writer xml;
	xml.generate_whitespace(true);
	xml.add_header();
	for (unsigned int i = 0; i < events.size(); i++){
		xml.start_group("e");
		xml.add_tag("y",(int)events[i].type);
		xml.add_tag("t",events[i].time.period_end);
		xml.add_tag("tt",events[i].time.period_start);
		xml.add_tag("r",events[i].region_id);
		xml.add_tag("i",events[i].region_info_id);
		xml.add_tag("p",ns_to_string(events[i].position.x) + "," + ns_to_string(events[i].position.y));
		xml.add_tag("s",ns_to_string(events[i].size.x) + "," + ns_to_string(events[i].size.y));
		xml.add_tag("u",(int)events[i].annotation_source);
		if (events[i].annotation_time > 0)
			xml.add_tag("a",events[i].annotation_time);
		if (events[i].annotation_source_details.size() > 0)
			xml.add_tag("d",events[i].annotation_source_details);
		xml.add_tag("c",ns_death_time_annotation::exclusion_value(events[i].excluded));
		if (events[i].number_of_worms_at_location_marked_by_hand > 0)
			xml.add_tag("xh",ns_to_string(events[i].number_of_worms_at_location_marked_by_hand));
		if (events[i].number_of_worms_at_location_marked_by_machine > 0)
			xml.add_tag("xm",ns_to_string(events[i].number_of_worms_at_location_marked_by_machine));
		xml.add_tag("g",(unsigned long)events[i].disambiguation_type);

		xml.add_tag("q",events[i].stationary_path_id.group_id);
		xml.add_tag("w",events[i].stationary_path_id.path_id);
		xml.add_tag("dt", events[i].stationary_path_id.detection_set_id);
		xml.end_group();
	}
	xml.add_footer();
	o << xml.result();
}


ns_death_time_annotation_compiler_location::ns_death_time_annotation_compiler_location(const ns_death_time_annotation & a){
	properties.excluded = ns_death_time_annotation::ns_not_excluded;
	properties.number_of_worms_at_location_marked_by_hand = a.number_of_worms_at_location_marked_by_hand;
	properties.number_of_worms_at_location_marked_by_machine = a.number_of_worms_at_location_marked_by_machine;
	properties.position = a.position;
	properties.size = a.size;

	add_event(a);
}



bool ns_death_time_annotation_compiler_location::add_event(const ns_death_time_annotation & a){
	ns_death_time_annotation_set::iterator p(annotations.events.insert(annotations.end(),a));
	handle_sticky_properties(a);
	return true;
}

bool ns_death_time_annotation_compiler_location::location_matches(const unsigned long distance_cutoff_squared,const ns_vector_2i & position) const{
	return (properties.position - position).squared() < distance_cutoff_squared;
}
bool ns_death_time_annotation_compiler_location::attempt_to_add(const unsigned long distance_cutoff_squared,const ns_death_time_annotation & a){
	if (location_matches(distance_cutoff_squared,a.position)){
		return add_event(a);
	}
	return false;
}

void ns_death_time_annotation_compiler_location::handle_sticky_properties(const ns_death_time_annotation & a){
	if (a.stationary_path_id.specified()){
		if (!this->properties.stationary_path_id.specified())
			this->properties.stationary_path_id = a.stationary_path_id;
		else{
			if (this->properties.stationary_path_id.detection_set_id == a.stationary_path_id.detection_set_id &&
				(this->properties.stationary_path_id.group_id != a.stationary_path_id.group_id ||
				this->properties.stationary_path_id.path_id != a.stationary_path_id.path_id))
				throw ns_ex("ns_death_time_annotation_compiler_location::handle_sticky_properties()::Adding mixed stationary paths!");
		}
	}
	//right now we just make the loglikelihood be the death loglikelihood
	if (a.type == ns_movement_cessation &&
		a.annotation_source == ns_death_time_annotation::ns_lifespan_machine)
		properties.loglikelihood = a.loglikelihood;
	a.transfer_sticky_properties(properties);
}


void ns_death_time_annotation_compiler_region::clear() {
	locations.clear();
	non_location_events.clear();
	fast_moving_animals.clear();
	metadata.clear();
}
void ns_death_time_annotation_compiler_region::empty_but_keep_memory() {
	locations.resize(0);
	non_location_events.clear();
	fast_moving_animals.clear();
	metadata.clear();
}


void ns_death_time_annotation_compiler_region::create_location(const ns_stationary_path_id& s,const ns_vector_2i & position, const ns_vector_2i & size) {
	bool found = false;
	for (ns_location_list::iterator p = locations.begin(); p != locations.end(); p++) {
		if (p->properties.stationary_path_id == s) {
			found = true;
			break;
		}
	}
	if (!found) {
		locations.resize(locations.size()+1);
		locations.rbegin()->properties.excluded = ns_death_time_annotation::ns_not_excluded;
		locations.rbegin()->properties.stationary_path_id = s;
		locations.rbegin()->properties.position = position;
		locations.rbegin()->properties.size = size;
	}
}
void ns_death_time_annotation_compiler_region::add(const ns_death_time_annotation & e, const bool create_new_location){
	if (e.type == ns_fast_moving_worm_observed ){
		fast_moving_animals.reserve(2000);
		fast_moving_animals.push_back(e);
		return;
	}
	//we don't need to match up censoring events, such as worms inferred as going missing, as they have no explicit location.
	if (e.disambiguation_type == ns_death_time_annotation::ns_inferred_censoring_event) {
		non_location_events.reserve(2000);
		non_location_events.add(e);
		return;
	}

	//if the annotation corresponds to a specific stationary path, sort it in a location corresponding to that path.
	if (e.stationary_path_id.specified()){

		//try to find a location already specified for the current annotation's stationary path id.
		//if that fails, find a location that matches the location of the current annotation

		ns_location_list::iterator unassigned_position_match(locations.end());
		ns_location_list::iterator assigned_position_match(locations.end());
		double min_useable_dist(DBL_MAX),
			min_overall_debug_dist(DBL_MAX);
		unsigned long id_of_min_overall(0);
		unsigned long debug_id(0);
		for(ns_location_list::iterator p = locations.begin(); p != locations.end(); p++){

	//		for (unsigned long k = 0; k < p->annotations.size(); k++)
	//			if (p->annotations[k].inferred_animal_location)
	//				cerr << "BA";
			if (p->properties.stationary_path_id == e.stationary_path_id){
					assigned_position_match = p;
					break;
			}
			const double dist = (p->properties.position - e.position).squared();
			if (
				(!p->properties.stationary_path_id.specified() || p->properties.stationary_path_id.detection_set_id != e.stationary_path_id.detection_set_id)
				&& p->location_matches(match_distance_squared, e.position) && dist <= min_useable_dist) {
				min_useable_dist = dist;
				unassigned_position_match = p;
			}
			if (!p->properties.stationary_path_id.specified() || p->properties.stationary_path_id.detection_set_id != e.stationary_path_id.detection_set_id &&
				dist <= min_overall_debug_dist) {
				id_of_min_overall = debug_id;
				min_overall_debug_dist = dist;
			}
		}

		if (assigned_position_match != locations.end()) {
			assigned_position_match->add_event(e);
		}
		else if (unassigned_position_match != locations.end())
			unassigned_position_match->add_event(e);
		else
			if (create_new_location) {
				if (locations.empty())
					locations.reserve(300);
				locations.push_back(ns_death_time_annotation_compiler_location(e));
				locations.rbegin()->annotations.reserve(100);
			}
		return;
	}
	//if the annotation isn't labeled as applying to a specific stationary path, check to see if there are any objects
	//close to its location.
	for(ns_location_list::iterator p = locations.begin(); p != locations.end(); p++){
		//if we find one that matches, turn it into the new assigned location
		if (p->attempt_to_add(match_distance_squared,e))
			return;
	}
	//if we don't find any unassigned matching locations, create a new location.

	//positions at the origin are censoring events or other non-position related events, which should be kept separately
	if (e.position == ns_vector_2i(0, 0) && e.size == ns_vector_2i(0, 0)) {
		std::cerr << "Notice: Encountered a worm at position zero";
		non_location_events.add(e);
	}
	else
		if (create_new_location)
			locations.push_back(ns_death_time_annotation_compiler_location(e));
}


void ns_death_time_annotation_compiler_location::merge(const ns_death_time_annotation_compiler_location & location){
	throw ns_ex("Not implemented!");
	/*
	for (unsigned int i = 0; i < location.annotations.size(); i++){
		add_event(location.annotations[i]);
	}
	if (location.number_of_extra_worms > number_of_extra_worms)
		number_of_extra_worms = location.number_of_extra_worms;
	if (location.machine_excluded)
		machine_excluded = true;
	if (location.by_hand_excluded)
		by_hand_excluded = true;
		*/
}
void ns_death_time_annotation_compiler_region::merge(const ns_death_time_annotation_compiler_region & region, const bool create_new_location){
	//it's a little faster to add all the specified points first and then add the unspecified poitns afterwards
	for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator p = region.locations.begin(); p != region.locations.end(); p++){

		if (p->properties.stationary_path_id.specified()){
			for (unsigned int i = 0; i < p->annotations.size(); i++)
				this->add(p->annotations[i],create_new_location);
		}
	}
	for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator p = region.locations.begin(); p != region.locations.end(); p++){
		if (!p->properties.stationary_path_id.specified()){
		for (unsigned int i = 0; i < p->annotations.size(); i++)
			this->add(p->annotations[i],create_new_location);
		}
	}
	non_location_events.add(region.non_location_events);
	fast_moving_animals.add(region.fast_moving_animals);
}

ns_death_time_annotation::ns_exclusion_type ns_death_time_annotation::combine_exclusion_types(const ns_death_time_annotation::ns_exclusion_type & a, const ns_death_time_annotation::ns_exclusion_type & b){

	if (a == b)
		return a;
	if (a == ns_not_excluded)
		return b;
	if (b == ns_not_excluded)
		return a;
	if (
		(is_censored(a) && (is_excluded(b))) ||
		(is_censored(b) && (is_excluded(a)))
		)
		return ns_excluded_and_censored;
	if (is_censored(a) && (b == ns_not_excluded || is_censored(b))){
		if (a != b && b != ns_not_excluded)
			return ns_censored;
		else return a;
	}
	if (is_censored(b) && (a == ns_not_excluded || is_censored(a))){
		if (b != a && a != ns_not_excluded)
			return ns_censored;
		else return b;
	}
	if (a == ns_both_excluded || b == ns_both_excluded
		|| a == ns_machine_excluded && b == ns_by_hand_excluded
		|| b == ns_machine_excluded && a == ns_by_hand_excluded)
		return ns_both_excluded;
	if (is_excluded(a) && is_excluded(b) && a!=b)
		return ns_excluded;
	if (is_excluded(a))
		return a;
	if (is_excluded(b))
		return b;

	throw ns_ex("Logical error!");
}
void ns_death_time_annotation_compiler::add(const ns_death_time_annotation_set & set, const ns_creation_type creation_type){
	for (unsigned int i = 0; i < set.events.size(); i++){
		if (set.events[i].region_info_id==0)
			throw ns_ex("Attempting to add a death time annotation set with no region info id specified!");
		ns_region_list::iterator r(regions.find(set.events[i].region_info_id));
		if (r == regions.end()){
			if (creation_type != ns_create_all)
				continue;
			r = regions.insert(ns_region_list::value_type(set.events[i].region_info_id,ns_death_time_annotation_compiler_region())).first;
		}
		r->second.add(set.events[i],creation_type != ns_do_not_create_regions_or_locations);
	}
}

void ns_death_time_annotation_compiler::add(const ns_death_time_annotation_compiler & compiler, const ns_creation_type creation_type){

	for (ns_death_time_annotation_compiler::ns_region_list::const_iterator p = compiler.regions.begin(); p != compiler.regions.end(); p++){
		ns_death_time_annotation_compiler::ns_region_list::iterator q = regions.find(p->first);
		if (q == regions.end()){
			if (creation_type != ns_do_not_create_regions && creation_type != ns_do_not_create_regions_or_locations)
				regions[p->first] = p->second;
		}
		else{
			q->second.merge(p->second,creation_type != ns_do_not_create_regions_or_locations);
		}
	}

}

void ns_death_time_annotation_compiler::add_path(const ns_64_bit& region_info_id, const ns_stationary_path_id& p, const ns_vector_2i& position, const ns_vector_2i& size , const ns_region_metadata& metadata) {
	ns_region_list::iterator r(regions.find(region_info_id));
	if (r == regions.end()) {
		r = regions.insert(ns_region_list::value_type(region_info_id, ns_death_time_annotation_compiler_region())).first;
		r->second.metadata = metadata;
	}
	
	r->second.create_location(p, position, size);

}
void ns_death_time_annotation_compiler::add(const ns_death_time_annotation & e,const ns_region_metadata & metadata){

	ns_region_list::iterator r(regions.find(e.region_info_id));
	if (r == regions.end())
		r = regions.insert(ns_region_list::value_type(e.region_info_id,ns_death_time_annotation_compiler_region())).first;

	r->second.add(e,true);
	r->second.metadata = metadata;
}

void ns_death_time_annotation_compiler::add(const ns_death_time_annotation_set & set,const ns_region_metadata & metadata){
	if (set.events.size() == 0)
		return;

	const ns_64_bit region_info_id(set.events[0].region_info_id);

	ns_region_list::iterator r(regions.find(region_info_id));
	if (r == regions.end())
		r = regions.insert(ns_region_list::value_type(region_info_id,ns_death_time_annotation_compiler_region())).first;

	for (unsigned int i = 0; i < set.events.size(); i++){
		if (set.events[i].region_info_id != region_info_id)
			throw ns_ex("ns_death_time_annotation_compiler::add()::Cannot add multi-region set and metadata simultaneously");
		r->second.add(set.events[i],true);
		r->second.metadata = metadata;
	}
}

void ns_death_time_annotation_compiler::specifiy_region_metadata(const ns_64_bit region_id,const ns_region_metadata & metadata){
	if (region_id == 0)
		throw ns_ex("Zero region id!");
	if (metadata.region_id != region_id)
		throw ns_ex("Attempting to insert inconsistant metadata");
	ns_region_list::iterator r(regions.find(region_id));
	if (r == regions.end())
		r = regions.insert(ns_region_list::value_type(region_id,ns_death_time_annotation_compiler_region())).first;
	r->second.metadata = metadata;
}


class ns_death_time_event_compiler_time_aggregator{
	typedef std::map<unsigned long, ns_survival_timepoint> ns_aggregator_timepoint_list;
	ns_aggregator_timepoint_list timepoints;
	ns_region_metadata metadata;
public:
	typedef enum  { ns_normal_event,ns_best_guess_event } ns_best_guess_flag;
	ns_death_time_event_compiler_time_aggregator(const ns_region_metadata & metadata_):metadata(metadata_){}
	void add(const ns_death_time_annotation& e, bool use_by_hand_data, const ns_best_guess_flag  best_guess_flag= ns_normal_event) {

		if (e.type == ns_no_movement_event) return;
		if (!ns_movement_event_is_a_state_transition_event(e.type))
			return;
		if (e.type == ns_stationary_worm_disappearance ||
			e.type == ns_death_associated_expansion_stop ||
			e.type == ns_death_associated_post_expansion_contraction_start ||
			e.type == ns_death_associated_post_expansion_contraction_stop)
			return;
		//	if (e.time.period_start_was_not_observed){
		//		cerr << "Found an event whose start was not observed";
		//		return;
		//	}
		if (e.time.period_end_was_not_observed) {
			cerr << "ns_death_time_event_compiler_time_aggregator()::add()::Found an event whose end was not observed. This type of event is unusual and should be investigated.";
			return;
		}
		//we store event times at the end of the interval in which the event occurred.
		//the actual event time should be accessed by calling best_estimate_event_time_within_interval()
		//but we don't want to store that here as that would have each worm dying at its own time
		//vastly increasing the number of elements in timepoints[]
		const unsigned long event_time((e.time.period_end == 0) ? metadata.time_at_which_animals_had_zero_age : (e.time.period_end));
		ns_aggregator_timepoint_list::iterator p(timepoints.find(event_time));
		if (p == timepoints.end()) {
			p = timepoints.insert(ns_aggregator_timepoint_list::value_type(event_time, ns_survival_timepoint())).first;
			p->second.absolute_time = event_time;
		}


		bool unpositioned_censored_object(false);
		if (best_guess_flag == ns_best_guess_event) 
			add_to_timepoint(e, p->second.best_guess_deaths, use_by_hand_data);
		else {
			switch (e.type) {
			case ns_fast_movement_cessation:
				add_to_timepoint(e, p->second.long_distance_movement_cessations, use_by_hand_data);
				break;
			case ns_translation_cessation:
				add_to_timepoint(e, p->second.local_movement_cessations, use_by_hand_data);
				break;
			case ns_movement_cessation:
				add_to_timepoint(e, p->second.movement_based_deaths, use_by_hand_data);
				break;
			case ns_death_associated_expansion_start:
				add_to_timepoint(e, p->second.death_associated_expansions, use_by_hand_data);
				break;
			case ns_moving_worm_disappearance:
				if (e.excluded == ns_death_time_annotation::ns_not_excluded)
					throw ns_ex("Found a censored worm event that wasn't excluded or censored");
				add_to_timepoint(e, p->second.typeless_censoring_events, use_by_hand_data);
				break;
			default:
				throw ns_ex("ns_death_time_event_compiler_time_aggregator::add()::Unknown event type:") << (int)e.type;
			}
		}
	}
	void add_to_timepoint(const ns_death_time_annotation& e, ns_survival_timepoint_event& timepoint, bool use_by_hand_data) {
		ns_survival_timepoint_event_count ev;
		ev.events.push_back(e);
		ev.number_of_worms_in_machine_worm_cluster = e.number_of_worms_at_location_marked_by_machine;
		if (use_by_hand_data){
			int count(e.number_of_worms_at_location_marked_by_hand);
			if (e.number_of_worms_at_location_marked_by_hand==0)
				count= 1;
			//ev.number_of_clusters_identified_by_machine = ev.number_of_clusters_identified_by_hand = 1;
			ev.number_of_worms_in_machine_worm_cluster =
				ev.number_of_worms_in_by_hand_worm_annotation = count;
		}
		else{
			//ev.number_of_clusters_identified_by_machine =1;

			ev.number_of_worms_in_machine_worm_cluster = e.number_of_worms_at_location_marked_by_machine;
			ev.number_of_worms_in_by_hand_worm_annotation = e.number_of_worms_at_location_marked_by_hand;
		//	if (e.number_of_worms_at_location_marked_by_hand > 0)
		//		ev.number_of_clusters_identified_by_hand = 1;
		//	else ev.number_of_clusters_identified_by_hand = 0;
		}
		//	if (e.multiworm_censoring_strategy != ns_death_time_annotation::ns_none)
		//		cerr << "SDFD";
		ev.from_multiple_worm_disambiguation= e.disambiguation_type==ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster;
		//		ev.number_of_worms_in_by_hand_worm_annotation = e.number_of_worms_at_location_marked_by_hand;
		ev.properties = e;
		if (e.flag.event_should_be_excluded())
			ev.properties.excluded = ns_death_time_annotation::ns_by_hand_excluded;
		timepoint.add(ev);
	}
	void populate_survival_data(ns_survival_data & data){
		data.metadata = metadata;
		data.timepoints.resize(timepoints.size());
		unsigned long i(0);
		for (ns_aggregator_timepoint_list::iterator p = timepoints.begin(); p != timepoints.end(); ++p){
			data.timepoints[i] = p->second;
			i++;
		}
	}
};

bool ns_death_time_annotation_set::use_debug_read_columns = false;

bool operator ==(const ns_stationary_path_id & a, const ns_stationary_path_id & b){
	return a.group_id == b.group_id
		&& a.path_id == b.path_id
		&& a.detection_set_id == b.detection_set_id;
}

bool operator <(const ns_stationary_path_id & a, const ns_stationary_path_id & b) {
	if (a.detection_set_id != b.detection_set_id) return a.detection_set_id < b.detection_set_id;
	if (a.group_id != b.group_id) return a.group_id < b.group_id;
	return a.path_id < b.path_id;
}

ns_death_time_annotation::ns_multiworm_censoring_strategy ns_death_time_annotation::default_censoring_strategy(){
	return ns_death_time_annotation::ns_include_directly_observed_deaths_and_infer_the_rest;
}

ns_death_time_annotation::ns_missing_worm_return_strategy ns_death_time_annotation::default_missing_return_strategy(){
	return ns_death_time_annotation::ns_censoring_minimize_missing_times;
}


void ns_out_times(ostream & o, const ns_dying_animal_description_group<const ns_death_time_annotation> & d, const ns_region_metadata & metadata){
	o << ((d.last_fast_movement_annotation!=0)?ns_to_string((d.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval()-metadata.time_at_which_animals_had_zero_age)/(60.0*60*24)):"");
			o << ",";
			o << ((d.last_slow_movement_annotation!=0)?ns_to_string((d.last_slow_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() -metadata.time_at_which_animals_had_zero_age)/(60.0*60*24)):"");
			o << ",";
			o << ((d.movement_based_death_annotation != 0) ? ns_to_string((d.movement_based_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - metadata.time_at_which_animals_had_zero_age) / (60.0 * 60 * 24)) : "");
			o << ",";
			o << ((d.death_associated_expansion_start != 0) ? ns_to_string((d.death_associated_expansion_start->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - metadata.time_at_which_animals_had_zero_age) / (60.0 * 60 * 24)) : "");
			o << ",";
			o << ((d.best_guess_death_annotation != 0) ? ns_to_string((d.best_guess_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - metadata.time_at_which_animals_had_zero_age) / (60.0 * 60 * 24)) : "");
			o << ",";

			if (d.last_fast_movement_annotation != 0 && d.last_slow_movement_annotation != 0)
				o << (d.last_slow_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - d.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval())/(60.0*60*24);
			o << ",";

			if (d.best_guess_death_annotation != 0 &&d.last_slow_movement_annotation != 0)
				o << (d.best_guess_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - d.last_slow_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval())/(60.0*60*24);
			o << ",";
			if (d.best_guess_death_annotation != 0 && d.last_fast_movement_annotation != 0)
				o << (d.best_guess_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - d.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval())/(60.0*60*24);
			o << ",";
}
#ifdef NS_GENERATE_IMAGE_STATISTICS
void ns_death_time_annotation_compiler::generate_detailed_animal_data_file(const bool output_region_image_data,const ns_capture_sample_region_statistics_set & region_data,std::ostream & o) const{

	//ns_capture_sample_region_statistics_set::out_header(o);
	ns_region_metadata::out_JMP_plate_identity_header(o);
	o << ",Excluded,";
	o << "Machine Fast Movement Cessation Age (Days),Machine Slow Movement Cessation Age (Days), Machine Death Age (Days),"
		"Machine Slow Movement Duration (Days), Machine Posture Changing Duration (Days), Machine Not Fast Moving Duration (Days),"
		"By Hand Fast Movement Cessation Age (Days),By Hand Slow Movement Cessation Age (Days),By Hand Death Age (Days),"
		"By Hand Slow Movement Duration (Days),By Hand Posture Changing Duration (Days),By Hand Not Fast Moving Duration (Days), "
		"Fast Movement Cessation Position X,Fast Movement Cessation Position Y,"
		"Slow Movement Cessation Position X,Slow Movement Cessation Position y,"
		"Death Position X,Death Position y,"
		"Fast Movement Cessation Detection Details, Slow Movement Cessation Detection Details, Death Detection Movement Details";
	if (!output_region_image_data)
		o << "\n";
	else{
		o << ",";
		ns_capture_sample_region_data_timepoint::output_jmp_header("Fast Movement Cessation Image ",o,",");
		ns_capture_sample_region_data_timepoint::output_jmp_header("Slow Movement Cessation Image ",o,",");
		ns_capture_sample_region_data_timepoint::output_jmp_header("Death Image ",o,"\n");
	}

	for (ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		std::map<unsigned long,ns_capture_sample_region_data *>::const_iterator r(region_data.regions_sorted_by_id.end());
		if(output_region_image_data){
			r = region_data.regions_sorted_by_id.find(p->second.metadata.region_id);
			if (r == region_data.regions_sorted_by_id.end())
				throw ns_ex("Could not find region information for region ") << p->second.metadata.region_id;
			r->second->generate_timepoints_sorted_by_time();
		}

		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = p->second.locations.begin(); q != p->second.locations.end(); ++q){
			const ns_death_time_annotation * machine[3] = {0,0,0}, // death, local, long
										   * by_hand[3] = {0,0,0};
			//find death time
			for (unsigned int i = 0; i < q->annotations.size(); i++){
				const ns_death_time_annotation ** annotations;
				annotations = ((q->annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine)?machine:by_hand);

				if (q->annotations[i].type == ns_movement_cessation &&
						(annotations[0] == 0 || q->annotations[i].time.period_end > annotations[0]->time.period_end))
						annotations[0] = &q->annotations[i];
			}
			if (d.death_annotation != 0 && d.death_annotation->stationary_path_id.specified()){
				for (unsigned int i = 0; i < q->annotations.size(); i++){
					const ns_death_time_annotation ** annotations;
					annotations = ((q->annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine)?machine:by_hand);
					if (q->annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine &&
						q->annotations[i].stationary_path_id == annotations[0]->stationary_path_id){
						if (q->annotations[i].type ==  ns_translation_cessation)
							annotations[1] = & q->annotations[i];
						else if (q->annotations[i].type == ns_fast_movement_cessation)
							annotations[2] = & q->annotations[i];
					}
				}
			}
			else{
				//we don't have enough information to make accurate matches between events.

			}

			bool censored(q->properties.is_excluded());
			if (output_region_image_data) censored = censored || r->second->censored;
			p->second.metadata.out_JMP_plate_identity_data(o);
			o << ",";
			o << (censored?"1":"0") << ",";

			ns_out_times(o,machine,p->second.metadata);
			ns_out_times(o,by_hand,p->second.metadata);

			if (d.last_fast_movement_annotation != 0)
				o << d.last_fast_movement_annotation->position.x << "," << d.last_fast_movement_annotation->position.y << ",";
			else o <<",,";
			if (d.last_slow_movement_annotation != 0)
				o << d.last_slow_movement_annotation->position.x << "," << d.last_slow_movement_annotation->position.y << ",";
			else o <<",,";
			if (d.death_annotation !=0)
				o << d.death_annotation->position.x << "," << d.death_annotation->position.y << ",";
			else o<< ",,";

			if (d.last_fast_movement_annotation!= 0)
				o << d.last_fast_movement_annotation->annotation_source_details;
			o <<",";
			if (d.last_slow_movement_annotation != 0)
				o<< d.last_slow_movement_annotation->annotation_source_details;
			o << ",";
			if (d.death_annotation !=0)
				o << d.death_annotation->annotation_source_details;
			if (!output_region_image_data){
				o << "\n";
			}
			else{
				o << ",";

				if (d.death_annotation != 0){
					std::map<unsigned long,ns_capture_sample_region_data_timepoint *>::const_iterator tp = r->second->timepoints_sorted_by_time.find(d.death_annotation->time.period_end);
					if (tp == r->second->timepoints_sorted_by_time.end()) throw ns_ex("Could not locate region timepoint");
					tp->second->output_jmp_data(o,r->second->metadata.time_at_which_animals_had_zero_age,censored,censored,",");
				}
				else
					ns_capture_sample_region_data_timepoint::output_blank_jmp_data(o,",");


				if (d.last_slow_movement_annotation != 0){
					std::map<unsigned long,ns_capture_sample_region_data_timepoint *>::const_iterator tp = r->second->timepoints_sorted_by_time.find(d.last_slow_movement_annotation->time.period_end);
					if (tp == r->second->timepoints_sorted_by_time.end()) throw ns_ex("Could not locate region timepoint");
					tp->second->output_jmp_data(o,r->second->metadata.time_at_which_animals_had_zero_age,censored,censored,",");
				}
				else
					ns_capture_sample_region_data_timepoint::output_blank_jmp_data(o,",");


				if (d.last_fast_movement_annotation != 0){
					std::map<unsigned long,ns_capture_sample_region_data_timepoint *>::const_iterator tp = r->second->timepoints_sorted_by_time.find(d.last_fast_movement_annotation->time.period_end);
					if (tp == r->second->timepoints_sorted_by_time.end()) throw ns_ex("Could not locate region timepoint");
					tp->second->output_jmp_data(o,r->second->metadata.time_at_which_animals_had_zero_age,censored,censored,"\n");
				}
				else
					ns_capture_sample_region_data_timepoint::output_blank_jmp_data(o,"\n");
			}


		}
	}
}
#else

void ns_death_time_annotation_compiler::generate_detailed_animal_data_file(const bool output_region_image_data,std::ostream & o) const{

	//ns_capture_sample_region_statistics_set::out_header(o);
	ns_region_metadata::out_JMP_plate_identity_header(o);
	o << ",Event Frequency,Censored,Excluded,Special Flag, Size of by-hand annotated cluster,";
	o << "Machine Fast Movement Cessation Age (Days),Machine Slow Movement Cessation Age (Days),"
		"Machine Movement Cessation Age (Days),Machine Death-Associated Expansion Age (days), Machine Death Age (days),"
		"Machine Slow Movement Duration (Days), Machine Posture Changing Duration (Days), Machine Not Fast Moving Duration (Days),"
		"By Hand Fast Movement Cessation Age (Days),By Hand Slow Movement Cessation Age (Days),"
		"By Hand Movement Cessation Age (Days),By Hand Death-Associated Expansion Age (days), By Hand Death Age (days),"
		"By Hand Slow Movement Duration (Days),By Hand Posture Changing Duration (Days),By Hand Not Fast Moving Duration (Days), "
		"Fast Movement Cessation Position X,Fast Movement Cessation Position Y,"
		"Slow Movement Cessation Position X,Slow Movement Cessation Position y,"
		"Death Position X,Death Position y,Posture Analysis Log-Likelihood,Worm Group ID, Worm Path ID,"
		"Fast Movement Cessation Detection Details, Slow Movement Cessation Detection Details, Death Detection Movement Details, Worm cluster type, Censored Reason"
		"\n";
	for (ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		//std::map<unsigned long,ns_capture_sample_region_data *>::const_iterator r(region_data.regions_sorted_by_id.end());

		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = p->second.locations.begin(); q != p->second.locations.end(); ++q){
			ns_dying_animal_description_set_const animals;
	//		if (q->properties.stationary_path_id.group_id == 23)
	//			cout << "Found it!";
			q->generate_dying_animal_description_const(true,animals);
			if (animals.descriptions.size() > 1) {
				if (animals.descriptions[0].machine.best_guess_death_annotation == 0)
					continue;
				ns_death_time_annotation_set set;
				//only output best guess death times for cluster.  This is much easier than disambiguating which worm is which.
				ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
					ns_death_time_annotation::default_censoring_strategy(),
					q->properties, animals, ns_metadata_worm_properties::ns_best_guess_death, set, ns_death_time_annotation::ns_machine_annotations_if_no_by_hand);
				for (unsigned int i = 0; i < set.size(); i++) {
					p->second.metadata.out_JMP_plate_identity_data(o);
					o << ",1,0,"
						<< ((set[i].is_excluded() || set[i].flag.event_should_be_excluded()) ? "1" : "0") << ","
						<< set[i].flag.label() << ","
						<< set[i].number_of_worms_at_location_marked_by_hand << ",";

					if (set[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine)
						o << ",,,,"
						<< (set[i].time.best_estimate_event_time_for_possible_partially_unbounded_interval() - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60 * 24) << ",,,,";
					else o << ",,,,,,,,";
					if (set[i].annotation_source == ns_death_time_annotation::ns_storyboard)
						o << ",,,," << (set[i].time.best_estimate_event_time_for_possible_partially_unbounded_interval() - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60 * 24) << ",,,,";
					else o << ",,,,,,,,";

					 o << ",,";
					 o << ",,";
					o << ",,";
					o << q->properties.loglikelihood << ","
						<< q->properties.stationary_path_id.group_id << ","
						<< q->properties.stationary_path_id.path_id << ",";
					o << ",";
					o << ",";
					o << ",multiple worm cluster,";
					o << "\n";

				}
			}
			else{
				ns_dying_animal_description_base<const ns_death_time_annotation> d(animals.descriptions[0]);
				p->second.metadata.out_JMP_plate_identity_data(o);
				o << ",1,0,"
					<< ((q->properties.is_excluded() || q->properties.flag.event_should_be_excluded()) ? "1" : "0") << ","
					<< q->properties.flag.label() << ","
					<< q->properties.number_of_worms_at_location_marked_by_hand << ",";

				ns_out_times(o, d.machine, p->second.metadata);
				ns_out_times(o, d.by_hand, p->second.metadata);

				if (d.machine.last_fast_movement_annotation != 0)
					o << d.machine.last_fast_movement_annotation->position.x << "," << d.machine.last_fast_movement_annotation->position.y << ",";
				else o << ",,";
				if (d.machine.last_slow_movement_annotation != 0)
					o << d.machine.last_slow_movement_annotation->position.x << "," << d.machine.last_slow_movement_annotation->position.y << ",";
				else o << ",,";
				if (d.machine.best_guess_death_annotation != 0)
					o << d.machine.best_guess_death_annotation->position.x << "," << d.machine.best_guess_death_annotation->position.y << ",";
				else o << ",,";
				o << q->properties.loglikelihood << ","
					<< q->properties.stationary_path_id.group_id << ","
					<< q->properties.stationary_path_id.path_id << ",";

				if (d.machine.last_fast_movement_annotation != 0)
					o << d.machine.last_fast_movement_annotation->annotation_source_details;
				o << ",";
				if (d.machine.last_slow_movement_annotation != 0)
					o << d.machine.last_slow_movement_annotation->annotation_source_details;
				o << ",";
				if (d.machine.best_guess_death_annotation != 0)
					o << d.machine.best_guess_death_annotation->annotation_source_details;
				o << ",single worm,";
				o << "\n";
			}

			for (unsigned int i = 0; i < p->second.non_location_events.events.size(); i++) {
				if (p->second.non_location_events.events[i].is_excluded() ||
					!p->second.non_location_events.events[i].is_censored() ||
					p->second.non_location_events.events[i].multiworm_censoring_strategy != ns_death_time_annotation::default_censoring_strategy() ||
					p->second.non_location_events.events[i].missing_worm_return_strategy != ns_death_time_annotation::default_missing_return_strategy() )
					//p->second.non_location_events.events[i].excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment)
					continue;
				o << "," << p->second.non_location_events.events[i].number_of_worms();

				p->second.metadata.out_JMP_plate_identity_data(o);
				o << ",1,0," //censored,excluded
					<< "1," // number of worms
					<< ",," << (p->second.non_location_events.events[i].time.best_estimate_event_time_within_interval() - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60 * 24) << ",,,," //machine death times
					<< ",," << (p->second.non_location_events.events[i].time.best_estimate_event_time_within_interval() - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60 * 24) << ",,,,"; //by hand death times


				o << ",,";
				o << ",,";
				o << ",,"; //death position
				o << ",,,";	//likelihood group and path
				o << ",,"; //details
				o << "non-location_event,";
				o << p->second.non_location_events.events[i].censor_description();
				o << "\n";
				
			}
		}
	}
}

#endif
std::string ns_out_if_not_zero(const double d){
	if (d == 0)
		return "";
	else return ns_to_string(d);
};
void ns_death_time_annotation_compiler::generate_animal_event_method_comparison(std::ostream & o, double & total_death_mean_squared_error, double & total_expansion_mean_squared_error, double & total_contraction_mean_squared_error, 
																								  ns_64_bit & death_N, ns_64_bit & expansion_N, ns_64_bit & contraction_N) const{

	ns_region_metadata::out_JMP_plate_identity_header(o);
	o << ",Animal ID,Multiple worm at position ID,position_x,position_y,size_x,size_y,"
		"By Hand Inspection Fast Movement Cessession Time,By Hand Death Time, By Hand Inspection Movement Cessation Time,By Hand Death-Associated Expansion Time, "
		"Machine Fast Movement Cessation Time, Machine Death Time,Machine Movement Cessation Time,Machine Death-Associated Expansion Time, "
		"Machine-Annotated Worm Count,Hand-Annotated Worm Count, Visual Inspection Excluded, Machine Death Error (Days), Machine Expansion Error (Days), Special Flag for Animal\n";
	for (ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = p->second.locations.begin(); q != p->second.locations.end(); ++q) {
			if (q->properties.is_excluded() || q->properties.is_censored() || q->properties.flag.event_should_be_excluded())
				continue;
			ns_dying_animal_description_set_const animal;
			q->generate_dying_animal_description_const(false, animal);
			for (unsigned int i = 0; i < animal.descriptions.size(); i++) {
				const unsigned long machine_movement_cessation(animal.descriptions[i].machine.movement_based_death_annotation!=0 ?
																animal.descriptions[i].machine.movement_based_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval():
																p->second.metadata.time_at_which_animals_had_zero_age),
					machine_best_guess_death(animal.descriptions[i].machine.best_guess_death_annotation != 0 ?
						animal.descriptions[i].machine.best_guess_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() :
						p->second.metadata.time_at_which_animals_had_zero_age),
					 machine_fast_cess(animal.descriptions[i].machine.last_fast_movement_annotation != 0 ?
						animal.descriptions[i].machine.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() :
						p->second.metadata.time_at_which_animals_had_zero_age),
					 machine_expansion(animal.descriptions[i].machine.death_associated_expansion_start != 0 ?
						animal.descriptions[i].machine.death_associated_expansion_start->time.best_estimate_event_time_for_possible_partially_unbounded_interval() :
						p->second.metadata.time_at_which_animals_had_zero_age),
					vis_best_guess_death(animal.descriptions[i].by_hand.best_guess_death_annotation != 0 ?
						animal.descriptions[i].by_hand.best_guess_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() :
						p->second.metadata.time_at_which_animals_had_zero_age),
					 vis_movement_cessation(animal.descriptions[i].by_hand.movement_based_death_annotation != 0 ?
						animal.descriptions[i].by_hand.movement_based_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() :
						p->second.metadata.time_at_which_animals_had_zero_age),
					 vis_fast_cess(animal.descriptions[i].by_hand.last_fast_movement_annotation != 0 ?
						animal.descriptions[i].by_hand.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval() :
						p->second.metadata.time_at_which_animals_had_zero_age),
					 vis_expansion(animal.descriptions[i].by_hand.death_associated_expansion_start != 0 ?
						animal.descriptions[i].by_hand.death_associated_expansion_start->time.best_estimate_event_time_for_possible_partially_unbounded_interval() :
						p->second.metadata.time_at_which_animals_had_zero_age);


				p->second.metadata.out_JMP_plate_identity_data(o);

				o << "," << q->properties.stationary_path_id.group_id << ",";
				if (animal.descriptions.size() > 1)
					o << i << ",";
				else o << ",";
				bool death_spec((machine_movement_cessation > p->second.metadata.time_at_which_animals_had_zero_age&& vis_movement_cessation > p->second.metadata.time_at_which_animals_had_zero_age)),
					expansion_spec((machine_expansion > p->second.metadata.time_at_which_animals_had_zero_age&& vis_expansion > p->second.metadata.time_at_which_animals_had_zero_age));
				o <<  q->properties.position.x << "," << q->properties.position.y << "," << q->properties.size.x << "," << q->properties.size.y << ","
					<< ns_out_if_not_zero((vis_fast_cess - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< ns_out_if_not_zero((vis_best_guess_death - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< ns_out_if_not_zero((vis_movement_cessation - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< ns_out_if_not_zero((vis_expansion - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< ns_out_if_not_zero((machine_fast_cess - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< ns_out_if_not_zero((machine_best_guess_death - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< ns_out_if_not_zero((machine_movement_cessation - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< ns_out_if_not_zero((machine_expansion - p->second.metadata.time_at_which_animals_had_zero_age) / (60.0 * 60.0 * 24.0)) << ","
					<< q->properties.number_of_worms_at_location_marked_by_machine << ","
					<< q->properties.number_of_worms_at_location_marked_by_hand << ","
					<< (q->properties.is_excluded() ? "1" : "0") << ",";
				if (death_spec) {
					const double m((machine_movement_cessation - (double)vis_movement_cessation) / 60.0);
					o << m / (60.0 * 24.0);
					death_N++;
					total_death_mean_squared_error += m * m;
				}
				o << ",";
				if (expansion_spec) {
					const double m((machine_expansion - (double)vis_expansion) / 60.0);
					o << m / (60.0 * 24.0);
					expansion_N++;
					total_expansion_mean_squared_error += m * m;
				}
				o << ",";
				if (q->properties.flag.specified())
					o << q->properties.flag.label();
				o << "\n";

			}
		}
	}

}


void ns_death_time_annotation_compiler::remove_all_but_specified_event_type(const ns_death_time_annotation_set::ns_annotation_type_to_load & t){
	for(ns_region_list::iterator p = regions.begin(); p != regions.end(); ++p){
			for (ns_death_time_annotation_compiler_region::ns_location_list::iterator q = p->second.locations.begin(); q != p->second.locations.end(); ++q){
				q->annotations.remove_all_but_specified_event_type(t);
			}
	}
}


void ns_death_time_annotation_compiler_region::output_summary(std::ostream & o) const{
	for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = locations.begin(); q != locations.end(); ++q){
		o << "\tLocation position(" << q->properties.position.x << "," << q->properties.position.y << ") size (" <<  q->properties.size.x << ")" << q->properties.size.y << "\n";
		for (unsigned int i = 0; i < q->annotations.size(); i++){
			o << "\t\t" << q->annotations[i].description() << "\n";
		}
	}
}


void ns_death_time_annotation_compiler::output_summary(std::ostream & o) const{
	for(ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		o << "Region " << p->first << ": " << p->second.metadata.sample_name << "::" << p->second.metadata.region_name << "(" << p->second.metadata.sample_id << "::" << p->second.metadata.region_id << ")\n";
		p->second.output_summary(o);
	}
}

template<class source_t, class annotation_t>
class ns_dying_animal_description_generator{

	public:
		void generate(const ns_death_time_annotation& properties,
			source_t& annotations,
			const bool warn_on_movement_problems,
			ns_dying_animal_description_set_base<annotation_t>& description_set) const {
			//find the three time spans for the current position.
			//having a worm die twice would be weird and an alert is generated if that is encountered.
			//However, it is entirely possibel that worms slow down multiple times, and so those are just ignored
			//and the latest transition recorded.
			std::set<unsigned long> animal_id_at_location;

			//find all animal_id_at_location and make a mapping from their ID into the appropriate description to be returned.
			for (unsigned int i = 0; i < annotations.size(); ++i) {
				animal_id_at_location.insert(animal_id_at_location.end(), annotations[i].animal_id_at_position);
			}
			description_set.descriptions.resize(animal_id_at_location.size());
			const unsigned long max_animal_id(*animal_id_at_location.rbegin());
			std::vector<ns_dying_animal_description_base<annotation_t>* > animals;
			animals.resize(max_animal_id + 1, 0);
			int i = 0;
			for (std::set<unsigned long>::iterator p = animal_id_at_location.begin(); p != animal_id_at_location.end(); p++) {
				animals[*p] = &(description_set.descriptions[i]);
				i++;
			}
			ns_dying_animal_description_base<annotation_t>& reference_machine_animal(*description_set.descriptions.begin());

			//if (properties.stationary_path_id.group_id == 23)
		//		cout << "found it!";
			unsigned long maximum_worm_count_by_hand_annotation(0);
			//first we identify all animals at this position based on their movement cessation times.  (all animals must have a movement cessation time but all animals need not have a death-associated expansion time)
			for (unsigned int i = 0; i < annotations.size(); ++i) {

				if (annotations[i].type != ns_movement_cessation)
					continue;
				if (annotations[i].annotation_source != ns_death_time_annotation::ns_lifespan_machine &&
					annotations[i].annotation_source != ns_death_time_annotation::ns_unknown)
					animals[annotations[i].animal_id_at_position]->by_hand.movement_based_death_annotation = &annotations[i];
				else
				{
					if (reference_machine_animal.machine.movement_based_death_annotation == 0) {
						reference_machine_animal.machine.movement_based_death_annotation = &annotations[i];
						//WWW
						//	std::cerr << "Found a death annotation when generating ns_death_time_annotation_compiler_region::generate_survival_curve()\n";
						continue;
					}
					else {
						//if there are two machine annotations at one point, scream
						if (reference_machine_animal.machine.movement_based_death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine &&
							annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine) {
							if (warn_on_movement_problems)
								cerr << "ns_dying_animal_description_generator()::generate()::Multiple deaths found at position!\n";
							unsigned long cur_death_time,
								new_death_time;
							if (!reference_machine_animal.machine.movement_based_death_annotation->time.period_start_was_not_observed &&
								!reference_machine_animal.machine.movement_based_death_annotation->time.period_end_was_not_observed)
								cur_death_time = reference_machine_animal.machine.movement_based_death_annotation->time.best_estimate_event_time_within_interval();
							else if (!reference_machine_animal.machine.movement_based_death_annotation->time.period_start_was_not_observed)
								cur_death_time = reference_machine_animal.machine.movement_based_death_annotation->time.period_start;
							else if (!reference_machine_animal.machine.movement_based_death_annotation->time.period_end_was_not_observed)
								cur_death_time = reference_machine_animal.machine.movement_based_death_annotation->time.period_end;
							else
								throw ns_ex("ns_dying_animal_description_generator()::generate()::Unspecified event time found!");

							if (!annotations[i].time.period_start_was_not_observed &&
								!annotations[i].time.period_end_was_not_observed)
								new_death_time = annotations[i].time.best_estimate_event_time_within_interval();
							else if (!annotations[i].time.period_start_was_not_observed)
								new_death_time = annotations[i].time.period_start;
							else if (!annotations[i].time.period_end_was_not_observed)
								new_death_time = annotations[i].time.period_end;
							else
								throw ns_ex("ns_dying_animal_description_generator()::generate()::Unspecified event time found!");
							if (cur_death_time > new_death_time)
								reference_machine_animal.machine.movement_based_death_annotation = &annotations[i];
						}
						//if there the current annotation is by the machine, keep it
						if (reference_machine_animal.machine.movement_based_death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine)
							continue;
						//if the new annotation is by the machine, use it
						if (annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine) {
							reference_machine_animal.machine.movement_based_death_annotation = &annotations[i];
							continue;
						}
						//use lowest worm id
						if (annotations[i].animal_id_at_position < reference_machine_animal.machine.movement_based_death_annotation->animal_id_at_position) {
							reference_machine_animal.machine.movement_based_death_annotation = &annotations[i];
							continue;
						}
						if (annotations[i].animal_id_at_position > reference_machine_animal.machine.movement_based_death_annotation->animal_id_at_position) {
							continue;
						}

						if (!reference_machine_animal.machine.movement_based_death_annotation->time.period_end_was_not_observed &&
							!annotations[i].time.period_end_was_not_observed) {
							if (reference_machine_animal.machine.movement_based_death_annotation->time.period_end > annotations[i].time.period_end)
								reference_machine_animal.machine.movement_based_death_annotation = &annotations[i];
							continue;
						}
						if (annotations[i].time.period_end_was_not_observed)
							continue;
						throw ns_ex("Unknown problem with machine annotations");
					}
				}

			}

			for (unsigned int i = 0; i < annotations.size(); ++i) {
				if ((reference_machine_animal.machine.movement_based_death_annotation != 0 && reference_machine_animal.machine.movement_based_death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine &&
					!(annotations[i].stationary_path_id == reference_machine_animal.machine.movement_based_death_annotation->stationary_path_id)) ||
					annotations[i].annotation_source == ns_death_time_annotation::ns_unknown)
					continue;
				ns_dying_animal_description_group<annotation_t>* group;
				if (annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine)
					group = &reference_machine_animal.machine;
				else if (annotations[i].annotation_source != ns_death_time_annotation::ns_unknown)
					group = &animals[annotations[i].animal_id_at_position]->by_hand;
				else continue;
				if (annotations[i].event_explicitness == ns_death_time_annotation::ns_explicitly_not_observed)
					continue;
				switch (annotations[i].type) {
				case ns_translation_cessation:
					group->last_slow_movement_annotation = &annotations[i];
					break;
				case ns_fast_movement_cessation:
					group->last_fast_movement_annotation = &annotations[i];
					break;
				case ns_death_associated_expansion_stop:
					group->death_associated_expansion_stop = &annotations[i];
					break;
				case ns_death_associated_expansion_start:
					if (annotations[i].time.period_start == annotations[i].time.period_end && (!annotations[i].time.period_end_was_not_observed || !annotations[i].time.period_end_was_not_observed)) {
						if (annotations[i].annotation_source == ns_death_time_annotation::ns_storyboard)
							;// cerr << "Storyboard annotation yielded a zero duration expansion event that was not flagged as \"not observed\".\n";
						else if (annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine)
							cerr << "Automated analysis yielded a zero duration expansion event that was not flagged as \"not observed\".\n";
						else cerr << "Inproperly flagged annotation expansion event generated from unusual source: " << (int)annotations[i].annotation_source << "\n";
					}
					group->death_associated_expansion_start = &annotations[i];
					break;
				case ns_death_associated_post_expansion_contraction_stop:
					group->death_associated_post_expansion_contraction_stop = &annotations[i];
					break;
				case ns_death_associated_post_expansion_contraction_start:
					if (annotations[i].time.period_start == annotations[i].time.period_end && (!annotations[i].time.period_end_was_not_observed || !annotations[i].time.period_end_was_not_observed)) {
						if (annotations[i].annotation_source == ns_death_time_annotation::ns_storyboard)
							;// cerr << "Storyboard annotation yielded a zero duration contraction event that was not flagged as \"not observed\".\n";
						else if (annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine)
							cerr << "Automated analysis yielded a zero duration contraction event that was not flagged as \"not observed\".\n";
						else cerr << "Inproperly flagged annotation contraction event generated from unusual source: " << (int)annotations[i].annotation_source << "\n";
					}
					group->death_associated_post_expansion_contraction_start = &annotations[i];
					break;
				case ns_stationary_worm_disappearance:
					group->stationary_worm_dissapearance = &annotations[i];
					break;
				case ns_slow_moving_worm_observed:
					group->slow_moving_state_annotations.push_back(&annotations[i]);
					break;
				case ns_posture_changing_worm_observed:
					group->posture_changing_state_annotations.push_back(&annotations[i]);
					break;
				case ns_stationary_worm_observed:
					group->stationary_animal_state_annotations.push_back(&annotations[i]);
					break;
				case ns_movement_censored_worm_observed:
					group->movement_censored_state_annotations.push_back(&annotations[i]);
					break;
				}
			}
			//calculate best guess death times 
			for (unsigned int i = 0; i < description_set.descriptions.size(); i++) {
				description_set.descriptions[i].machine.calculate_best_guess_death_annotation();
				description_set.descriptions[i].by_hand.calculate_best_guess_death_annotation();
			}
			//figure out how many of the death times the user has explicitly annotated
			long by_hand_animals_annotated(0);
			for (unsigned int i = 0; i < description_set.descriptions.size(); i++) {
				if (description_set.descriptions[i].by_hand.best_guess_death_annotation != 0)
					by_hand_animals_annotated++;
			}
			if (!properties.is_censored() && description_set.descriptions.size() > 1)
				description_set.unassigned_multiple_worms = properties.number_of_worms() - by_hand_animals_annotated;
			else description_set.unassigned_multiple_worms = 0;

			for (unsigned int i = 0; i < description_set.descriptions.size(); i++) {
				description_set.descriptions[i].machine = reference_machine_animal.machine;
			}
			//ignore animals that don't die!
			if (reference_machine_animal.machine.best_guess_death_annotation == 0)
				return;
			if (reference_machine_animal.machine.last_slow_movement_annotation == 0) {
				if (warn_on_movement_problems && reference_machine_animal.machine.movement_based_death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine) {
					cerr << "WARNING: An animal was recorded as dead without passing through slow movement, and has been excluded from the output. \n";
					return;
				}
			}
			if (reference_machine_animal.machine.last_fast_movement_annotation == 0) {
				if (warn_on_movement_problems && reference_machine_animal.machine.movement_based_death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine) {
					cerr << "WARNING:  An animal was recorded as dead without passing through fast movement, and has been excluded from the output. \n";

				}
			}

			return;
		}
};

void ns_death_time_annotation_compiler_location::generate_dying_animal_description(const bool warn_on_movement_problems, ns_dying_animal_description_set & descriptions){
	ns_dying_animal_description_generator<ns_death_time_annotation_set, ns_death_time_annotation> d;
	d.generate(properties,annotations,warn_on_movement_problems, descriptions);
};
void ns_death_time_annotation_compiler_location::generate_dying_animal_description_const(const bool warn_on_movement_problems, ns_dying_animal_description_set_const & descriptions) const{
	ns_dying_animal_description_generator<const ns_death_time_annotation_set, const ns_death_time_annotation> d;
	d.generate(properties,annotations, warn_on_movement_problems, descriptions);
};


bool operator==(const ns_death_time_annotation_time_interval& a, const ns_death_time_annotation_time_interval& b) {
	if (a.period_end != b.period_end)
		return false;
	if (a.period_end_was_not_observed != b.period_end_was_not_observed)
		return false;
	if (a.period_start != b.period_start)
		return false;
	if (a.period_start_was_not_observed != a.period_start_was_not_observed)
		return false;
	return true;	
}
bool operator!=(const ns_death_time_annotation_time_interval& a, const ns_death_time_annotation_time_interval& b) {
	return !(a == b);
}


bool ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
						const ns_death_time_annotation & properties,
						const ns_dying_animal_description_set_const & d,
						const ns_metadata_worm_properties::ns_survival_event_type& death_event_to_use,
						ns_death_time_annotation_set & set,
						const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & death_times_to_use){
	bool made_at_least_one_annotation(false);
	for (unsigned int i = ns_death_time_annotation::ns_discard_multi_worm_clusters; i < ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies; i++) {
		bool ret = generate_correct_annotations_for_multiple_worm_cluster((ns_death_time_annotation::ns_multiworm_censoring_strategy)i, properties, d, death_event_to_use,set, death_times_to_use);
		if (ret) made_at_least_one_annotation = true;
	}
	return made_at_least_one_annotation;
}
std::string ns_death_time_annotation::event_observation_label(const ns_event_observation_type & e){
		switch (e){
		case ns_standard: return "";
		case ns_induced_multiple_worm_death: return "Induced Multiple Worm";
		case ns_observed_multiple_worm_death: return "Observed Multiple Worm";
		default: throw ns_ex("Unknown Event Observation Label");
	}
}
//currently this does not generate death-associated expansion times for clusters.  Needs to be fixed.
bool ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
						const ns_death_time_annotation::ns_multiworm_censoring_strategy & censoring_strategy,
						const ns_death_time_annotation & properties,
						const ns_dying_animal_description_set_const & description_set,
						const ns_metadata_worm_properties::ns_survival_event_type& death_event_to_use,
						ns_death_time_annotation_set & set,const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & death_times_to_use){
	unsigned long start_annotation_index(set.size());
	if (description_set.descriptions.size() < 2)
		throw ns_ex("ns_multiple_worm_cluster_death_annotation_handler::encountered a non-multiworm cluster!");
	if (death_times_to_use == ns_death_time_annotation::ns_machine_and_by_hand_annotations)
		throw ns_ex("Undefined multiple worm cluster censoring strategy: ns_machine_and_by_hand_annotation");

	//if (description_set.descriptions[0].machine.death_annotation == 0)
	//	throw ns_ex("Attempting to handle multiple worm cluster with no deaths!");

	for (unsigned int j = 0; j < description_set.descriptions.size(); j++) {

		const bool output_machine_and_inferred_multiple_worm_deaths_this_round(j == 0);
		ns_dying_animal_description_base<const ns_death_time_annotation>::ns_group_type event_data;
		if (death_times_to_use == ns_death_time_annotation::ns_only_machine_annotations) {
			if (!output_machine_and_inferred_multiple_worm_deaths_this_round)  //don't output machine death times more than once
				break;
			if (description_set.descriptions[0].machine.get_event(death_event_to_use) == 0)	//if we only use machine annotations, and there is no death time, give up.
				continue;
			event_data = description_set.descriptions[0].machine;
			
		}
		const ns_dying_animal_description_base<const ns_death_time_annotation> & d(description_set.descriptions[j]);
		if (death_times_to_use == ns_death_time_annotation::ns_only_by_hand_annotations) {
			if (d.by_hand.get_event(death_event_to_use) == 0)
				continue;
		}
		if (death_times_to_use == ns_death_time_annotation::ns_only_by_hand_annotations ||
			death_times_to_use == ns_death_time_annotation::ns_machine_annotations_if_no_by_hand) {
			if (d.by_hand.get_event(death_event_to_use) == 0) {
				if (!output_machine_and_inferred_multiple_worm_deaths_this_round) continue;  //don't output machine death times more than once
				if (d.machine.get_event(death_event_to_use) == 0)
					continue;
				event_data.get_event(death_event_to_use) = d.machine.get_event(death_event_to_use);
			}
			else
				event_data.get_event(death_event_to_use) = d.by_hand.get_event(death_event_to_use);

			if (d.by_hand.last_fast_movement_annotation == 0)
				event_data.last_fast_movement_annotation = d.machine.last_fast_movement_annotation;
			else
				event_data.last_fast_movement_annotation = d.by_hand.last_fast_movement_annotation;

			if (d.by_hand.last_slow_movement_annotation == 0)
				event_data.last_slow_movement_annotation = d.machine.last_slow_movement_annotation;
			else
				event_data.last_slow_movement_annotation = d.by_hand.last_slow_movement_annotation;
		}

		if (event_data.get_event(death_event_to_use) == 0)
			continue;
		ns_death_time_annotation death(*event_data.get_event(death_event_to_use));

		//we use this as debugging info in output file
		if (event_data.last_fast_movement_annotation != 0) {
			//use the death-associated expansion time if possible.
			//if not use the movement cessation time.
			ns_death_time_annotation_time_interval death_time;
			if (event_data.death_associated_expansion_start != 0 && !event_data.death_associated_expansion_start->time.fully_unbounded())
				death_time = event_data.death_associated_expansion_start->time;
			else death_time = death.time;

			death.volatile_duration_of_time_not_fast_moving =
				death_time.best_estimate_event_time_for_possible_partially_unbounded_interval() -
				event_data.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
		}
		properties.transfer_sticky_properties(death);
		int machine_worm_count = d.machine.get_event(death_event_to_use) != 0;
		int by_hand_worm_count = d.by_hand.get_event(death_event_to_use) != 0;

		//we output explicitly-annotated by hand annotations as individual events
		//but if not all multiple worms annotated have explicitly annotated death times, we need to generate inferred death times.
		if (output_machine_and_inferred_multiple_worm_deaths_this_round) {
			death.number_of_worms_at_location_marked_by_hand = by_hand_worm_count +description_set.unassigned_multiple_worms;
			death.number_of_worms_at_location_marked_by_machine = machine_worm_count +description_set.unassigned_multiple_worms;
		}
		else {
			death.number_of_worms_at_location_marked_by_hand = by_hand_worm_count;
			death.number_of_worms_at_location_marked_by_machine = machine_worm_count;
		}
		//we only want to apply fancy censoring strategies in places where we are /inferring/ multiple worms
		//not in cases where they are explicitly annotated by hand.
		ns_death_time_annotation::ns_multiworm_censoring_strategy censoring_strategy_to_use_this_round = censoring_strategy;
		if (!output_machine_and_inferred_multiple_worm_deaths_this_round)
			censoring_strategy_to_use_this_round = ns_death_time_annotation::ns_include_only_directly_observed_deaths;

		switch (censoring_strategy_to_use_this_round) {
			case ns_death_time_annotation::ns_include_only_directly_observed_deaths:
				death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
				set.push_back(death);
				break;
			case ns_death_time_annotation::ns_no_strategy_needed_for_single_worm_object:
				throw ns_ex("Encountered a no strategy needed for single worm object flag!");
			case ns_death_time_annotation::ns_unknown_multiworm_cluster_strategy:
				throw ns_ex("Encountered an unknown multiworm cluster request");
			case ns_death_time_annotation::ns_discard_multi_worm_clusters:
				//don't include multiworm clusters!
				break;
			case ns_death_time_annotation::ns_include_as_single_worm_deaths:
				death.number_of_worms_at_location_marked_by_machine = machine_worm_count;
				death.number_of_worms_at_location_marked_by_hand = 1;
				death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
				set.push_back(death);
				break;
			case ns_death_time_annotation::ns_by_hand_censoring:
				return true;	//ignore
			case ns_death_time_annotation::ns_include_directly_observed_deaths_and_infer_the_rest: {
				//This is the standard option used to process multiple worm clusters that don't have complete by hand annotated death times for each worm.
				if (event_data.last_slow_movement_annotation == 0) {
					cerr << "Found an unusual situation where a multiple worm cluster has no slow down from fast moving time annotationed (perhaps because it existed at the start of observation?)\n";

					death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
					//we don't interval censor clumps with lots of worms, because we probably can't get the death time correct.
					//so we right censor them.
					if (death.number_of_worms_at_location_marked_by_hand > 3) {
						if (death.is_excluded())
							death.excluded = ns_death_time_annotation::ns_excluded_and_censored;
						else
							death.excluded = ns_death_time_annotation::ns_multiworm_censored;
						death.event_observation_type = ns_death_time_annotation::ns_standard;
					}
					set.push_back(death);
				}
				else {
					//we don't interval censor clumps with lots of worms, because we probably can't get the death time correct.
					//so we right censor them.
					if (death.number_of_worms_at_location_marked_by_hand > 3) {
						death.time = event_data.last_slow_movement_annotation->time;
						if (death.is_excluded())
							death.excluded = ns_death_time_annotation::ns_excluded_and_censored;
						else
							death.excluded = ns_death_time_annotation::ns_multiworm_censored;
						death.event_observation_type = ns_death_time_annotation::ns_standard;
						set.push_back(death);
					}
					else {
						ns_death_time_annotation output_death(death);
						//include last, observed death
						output_death.number_of_worms_at_location_marked_by_machine = machine_worm_count;
						output_death.number_of_worms_at_location_marked_by_hand = 1;
						output_death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
						set.push_back(output_death);
						if (death.number_of_worms_at_location_marked_by_hand > 1) {
							//death occurs during the inteval between the animals slowing down to changing posture and
							//dying
							output_death.time.period_start = event_data.last_slow_movement_annotation->time.period_start;
							output_death.time.period_start_was_not_observed = event_data.last_slow_movement_annotation->time.period_start_was_not_observed;
							output_death.number_of_worms_at_location_marked_by_machine = 0;
							output_death.number_of_worms_at_location_marked_by_hand = death.number_of_worms_at_location_marked_by_hand - 1;
							output_death.event_observation_type = ns_death_time_annotation::ns_induced_multiple_worm_death;
							set.push_back(output_death);
						}
					}
				}
			}
			 break;

			case ns_death_time_annotation::ns_right_censor_multiple_worm_clusters: {
				if (event_data.last_slow_movement_annotation == 0) {
					cerr << "Found an unusual situation where a multiple worm cluster has no slow down from fast moving time annotationed (perhaps because it existed at the start of observation?)\n";
					death.type = ns_moving_worm_disappearance;
					if (death.is_excluded())
						death.excluded = ns_death_time_annotation::ns_excluded_and_censored;
					else death.excluded = ns_death_time_annotation::ns_multiworm_censored;
					set.push_back(death);
				}
				else {
					death = *event_data.last_slow_movement_annotation;
					properties.transfer_sticky_properties(death);
					death.type = ns_moving_worm_disappearance;
					if (death.is_excluded())
						death.excluded = ns_death_time_annotation::ns_excluded_and_censored;
					else
						death.excluded = ns_death_time_annotation::ns_multiworm_censored;
					set.push_back(death);
				}
				break;
			}
			case ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor:
				//ignore multiple deaths entirely and let the machine detect them as missing.
				break;
			default: throw ns_ex("ns_worm_movement_summary_series::from_death_time_annotations()::Unknown multiple worm censoring strategy");
			}
	}
	for (unsigned int i = start_annotation_index; i < set.size(); i++) {
		set[i].multiworm_censoring_strategy = censoring_strategy;
		set[i].by_hand_annotation_integration_strategy = death_times_to_use;
		set[i].disambiguation_type = ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster;
	}
	return true;
}

ns_death_time_annotation_time_interval ns_death_time_annotation_compiler_region::latest_interval() const{
	ns_death_time_annotation_time_interval latest_interval(0,0);
	for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator p = locations.begin(); p != locations.end(); p++){
		for (ns_death_time_annotation_set::const_iterator q = p->annotations.begin(); q != p->annotations.end(); q++){
			if (!q->time.period_start_was_not_observed && q->time.period_start > latest_interval.period_start)
				latest_interval.period_start = q->time.period_start;
			if (!q->time.period_end_was_not_observed && q->time.period_end > latest_interval.period_end)
				latest_interval.period_end = q->time.period_end;
		}
	}
	return latest_interval;
}

typedef enum { ns_generate_machine_annotations, ns_generate_by_hand_annotations, ns_generate_best_annotation } ns_annotation_generation_type;

//this function takes a single individual's times of death-associated events ( movement cessation, death-time expansion, etc) and 
//breaks them down in a way that allows them to be aggregated in survival curves.
void ns_add_normal_death_to_set(const ns_annotation_generation_type & requested_annotation_type, ns_dying_animal_description_base<const ns_death_time_annotation>& animal_events, ns_death_time_annotation properties_to_transfer, const ns_death_time_annotation_time_interval & latest_interval, const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_annotation_integration_strategy, const bool use_by_hand_worm_cluster_annotations,ns_death_time_event_compiler_time_aggregator & aggregator) {
	//add 1) movement-based death time 2) slow movement cessation, 3) fast movement cessation and 3) death time expansion start 4) death time expansion stop 5) death time contraction start 6) death time contraction stop 7) best guess death time
	const int number_of_different_events(8);
	const ns_death_time_annotation* a[number_of_different_events] = { 0,0,0,0,0,0,0,0 };

	bool matches_machine_detected_death(animal_events.machine.movement_based_death_annotation != 0);
	switch (requested_annotation_type) {
	//add a machine annotations if requested
	case ns_generate_machine_annotations: {
		a[0] = animal_events.machine.movement_based_death_annotation;
		a[1] = animal_events.machine.last_slow_movement_annotation;
		a[2] = animal_events.machine.last_fast_movement_annotation;
		a[3] = animal_events.machine.death_associated_expansion_start;
		a[4] = animal_events.machine.death_associated_expansion_stop;
		a[5] = animal_events.machine.death_associated_post_expansion_contraction_start;
		a[6] = animal_events.machine.death_associated_post_expansion_contraction_stop;
		a[7] = animal_events.machine.best_guess_death_annotation;
		break;
	}
	//add a by hand annotation, if required
	case ns_generate_by_hand_annotations:{
		a[0] = animal_events.by_hand.movement_based_death_annotation;
		a[1] = animal_events.by_hand.last_slow_movement_annotation;
		a[2] = animal_events.by_hand.last_fast_movement_annotation;
		a[3] = animal_events.by_hand.death_associated_expansion_start;
		a[4] = animal_events.by_hand.death_associated_expansion_stop;
		a[5] = animal_events.by_hand.death_associated_post_expansion_contraction_start;
		a[6] = animal_events.by_hand.death_associated_post_expansion_contraction_stop;
		a[7] = animal_events.machine.best_guess_death_annotation;
		break;
	}
	case ns_generate_best_annotation: {
		//add a by hand annotation, and if one doesn't exist, add a machine annotation
		if (animal_events.by_hand.movement_based_death_annotation != 0)
			a[0] = animal_events.by_hand.movement_based_death_annotation;
		else a[0] = animal_events.machine.movement_based_death_annotation;

		if (animal_events.by_hand.last_slow_movement_annotation != 0)
			a[1] = animal_events.by_hand.last_slow_movement_annotation;
		else a[1] = animal_events.machine.last_slow_movement_annotation;

		if (animal_events.by_hand.last_fast_movement_annotation != 0)
			a[2] = animal_events.by_hand.last_fast_movement_annotation;
		else a[2] = animal_events.machine.last_fast_movement_annotation;

		if (animal_events.by_hand.death_associated_expansion_start != 0)
			a[3] = animal_events.by_hand.death_associated_expansion_start;
		else a[3] = animal_events.machine.death_associated_expansion_start;

		if (animal_events.by_hand.death_associated_expansion_stop != 0)
			a[4] = animal_events.by_hand.death_associated_expansion_stop;
		else a[4] = animal_events.machine.death_associated_expansion_stop;

		if (animal_events.by_hand.death_associated_post_expansion_contraction_start != 0)
			a[5] = animal_events.by_hand.death_associated_post_expansion_contraction_start;
		else a[5] = animal_events.machine.death_associated_post_expansion_contraction_start;

		if (animal_events.by_hand.death_associated_post_expansion_contraction_stop != 0)
			a[6] = animal_events.by_hand.death_associated_post_expansion_contraction_stop;
		else a[6] = animal_events.machine.death_associated_post_expansion_contraction_stop;

		if (animal_events.by_hand.best_guess_death_annotation != 0)
			a[7] = animal_events.by_hand.best_guess_death_annotation;
		else a[7] = animal_events.machine.best_guess_death_annotation;
		break;
	}
	default: throw ns_ex("Unknown death time generation request!");
	}
	for (unsigned int i = 0; i < number_of_different_events; i++) {
		if (a[i] != 0) {
			ns_death_time_annotation b(*a[i]);

			//A special case occurs when
			//1) the lifespan machine notices a worm that is slow moving or changing posture
			//   at the end of an experiment, and generates a censoring event for it
			//2) A human annotates the animal as dying using the storyboard or other means.
			//In this case, the original lifespan machine censoring event needs to be excluded.
			//This is the only location in the code where this issue an be identified and handled!
			if (b.type == ns_movement_cessation &&
				b.annotation_source != ns_death_time_annotation::ns_lifespan_machine &&
				properties_to_transfer.excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment)
				properties_to_transfer.excluded = ns_death_time_annotation::ns_not_excluded;

			//another special case occurs when
			//1) the lifespan machine delcares a worm as dead
			//2) the user flags the worms as still alive at the end of the experiment with the STILL_ALIVE flag
			//Then we need to register the animal as being a censoring event at the end of the experiment.
			//ideally, we would to this at the final measurement time for the plate
			//but this data is not available at this point in the code, so we do the second best thing, which is to
			//put it at the time of the latest interval of any annotation (machine or by hand) registered for the plate.

			if (b.type == ns_movement_cessation &&
				properties_to_transfer.flag.specified()) {
				if (properties_to_transfer.flag.label_short == "STILL_ALIVE") {
					b.time = latest_interval;
					properties_to_transfer.excluded = ns_death_time_annotation::ns_censored_at_end_of_experiment;
					b.annotation_source = ns_death_time_annotation::ns_storyboard;
					b.annotation_source_details = "By-hand annotated as remainig alive at end of observarion period";
				}
			}
			properties_to_transfer.transfer_sticky_properties(b);

			//we use this as debugging info in output file
			if (a[2] != 0 && a[0] != 0)
				b.volatile_duration_of_time_not_fast_moving =
				a[0]->time.best_estimate_event_time_for_possible_partially_unbounded_interval() -
				a[2]->time.best_estimate_event_time_for_possible_partially_unbounded_interval();

			//we use this as debugging info in output file
			if (a[3] != 0 && a[0] != 0) {
				b.volatile_time_at_death_associated_expansion_start = a[3]->time;
				/*if (a[3]->time == a[0]->time)
					cerr << ".";
				else
					cerr << "!";*/
				if (a[3]->time.period_start == a[3]->time.period_end && (!a[3]->time.period_end_was_not_observed || !a[3]->time.period_end_was_not_observed)) {
					if (a[3]->annotation_source != ns_death_time_annotation::ns_storyboard)
						cout << "Encountered an undefined expansion start time for animal " << b.stationary_path_id.group_id << " in region " << a[3]->region_info_id << "!\n";
					b.volatile_time_at_death_associated_expansion_start.period_end_was_not_observed = b.volatile_time_at_death_associated_expansion_start.period_start_was_not_observed = true;
				}
			}
			else
				b.volatile_time_at_death_associated_expansion_start.period_end_was_not_observed = b.volatile_time_at_death_associated_expansion_start.period_start_was_not_observed = true;
			if (a[4] != 0 && a[0] != 0) {
				b.volatile_time_at_death_associated_expansion_end = a[4]->time;
				if (a[4]->time.period_start == a[4]->time.period_end && (!a[4]->time.period_end_was_not_observed || !a[4]->time.period_end_was_not_observed)) {
					if (a[4]->annotation_source != ns_death_time_annotation::ns_storyboard)
						cout << "Encountered an undefined expansion end time for animal " << b.stationary_path_id.group_id << " in region " << a[4]->region_info_id << "!\n";
					b.volatile_time_at_death_associated_expansion_end.period_end_was_not_observed = b.volatile_time_at_death_associated_expansion_end.period_start_was_not_observed = true;
				}
			}
			else
				b.volatile_time_at_death_associated_expansion_end.period_end_was_not_observed = b.volatile_time_at_death_associated_expansion_end.period_start_was_not_observed = true;

			if (a[5] != 0 && a[0] != 0) {
				b.volatile_time_at_death_associated_post_expansion_contraction_start = a[5]->time;
				if (a[5]->time.period_start == a[5]->time.period_end && (!a[5]->time.period_end_was_not_observed || !a[5]->time.period_end_was_not_observed)) {
					if (a[5]->annotation_source != ns_death_time_annotation::ns_storyboard)
						cout << "Encountered an undefined contraction start time for animal " << b.stationary_path_id.group_id << " in region " << a[5]->region_info_id << "!\n";
					b.volatile_time_at_death_associated_post_expansion_contraction_start.period_end_was_not_observed = b.volatile_time_at_death_associated_post_expansion_contraction_start.period_start_was_not_observed = true;
				}
			}
			else
				b.volatile_time_at_death_associated_post_expansion_contraction_start.period_end_was_not_observed = b.volatile_time_at_death_associated_post_expansion_contraction_start.period_start_was_not_observed = true;

			if (a[6] != 0 && a[0] != 0) {
				b.volatile_time_at_death_associated_post_expansion_contraction_end = a[6]->time;
				if (a[6]->time.period_start == a[6]->time.period_end && (!a[6]->time.period_end_was_not_observed || !a[6]->time.period_end_was_not_observed)) {
					if (a[6]->annotation_source != ns_death_time_annotation::ns_storyboard)
						cout << "Encountered an undefined contraction end time for animal " << b.stationary_path_id.group_id << " in region " << a[6]->region_info_id << "!\n";
					b.volatile_time_at_death_associated_post_expansion_contraction_end.period_end_was_not_observed = b.volatile_time_at_death_associated_post_expansion_contraction_end.period_start_was_not_observed = true;
				}
			}
			else
				b.volatile_time_at_death_associated_post_expansion_contraction_end.period_end_was_not_observed = b.volatile_time_at_death_associated_post_expansion_contraction_end.period_start_was_not_observed = true;

			//regions can be re-analyzed. In this case,
			//some locations can dissapear (as they no longer exist in the new analysis)
			//hand anotations remain for these now non-existant locations.
			//we do not want to discard these old annotations completely in case they become useful later
			//but we do want to be able to identify them.
			//therefore, we need to mark locations currently identified by the machine as such.
			b.volatile_matches_machine_detected_death = matches_machine_detected_death;

			//single worm objects should be labeled as such so they pass through filters for specific multi-worm censoring strategies.
			b.multiworm_censoring_strategy = ns_death_time_annotation::ns_no_strategy_needed_for_single_worm_object;
			b.by_hand_annotation_integration_strategy = by_hand_annotation_integration_strategy;
			bool best_guess_death_time(i == 7);

			aggregator.add(b, use_by_hand_worm_cluster_annotations, best_guess_death_time?ns_death_time_event_compiler_time_aggregator::ns_best_guess_event 
																						 : ns_death_time_event_compiler_time_aggregator::ns_normal_event);


		}
	
	}

}

void ns_death_time_annotation_compiler_region::generate_survival_curve(ns_survival_data & curve, const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_annotation_strategy, const bool use_by_hand_worm_cluster_annotations, const bool warn_on_movement_problems) const {

	const ns_death_time_annotation_time_interval latest_interval(ns_death_time_annotation_compiler_region::latest_interval());

	unsigned long multiple_event_count(0);
	//Note that multiple strategies to handling multiple worm death times can be applied and included in censoring data files.
	//One approache to multiple worm clusters is to simply censor all animals that enter them.  
	//The default behavior of the death time caller is to generate death times for the same multiple worm clusters.
	//So, it is important that at this step the code keep track of which strategy is being applied,
	//so that the correct combination of censoring and death times, appropirate for the multiple worm death time strategy

	ns_death_time_event_compiler_time_aggregator aggregator(metadata);
	for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = locations.begin(); q != locations.end(); ++q) {

		ns_dying_animal_description_set_const description_set;
		q->generate_dying_animal_description_const(warn_on_movement_problems, description_set);
		if (description_set.descriptions.empty())
			throw ns_ex("ns_death_time_annotation_compiler_region::generate_survival_curve::Encountered an empty description set!");
		ns_dying_animal_description_base<const ns_death_time_annotation> & machine_death(description_set.descriptions[0]);	//the machine annotation will always be in description[0], as additional worms are only identified through by-hand annotation.
		const ns_death_time_annotation * machine_reference(0);
		if (machine_death.machine.best_guess_death_annotation != 0)
			machine_reference = machine_death.machine.best_guess_death_annotation;
		else if (machine_death.machine.last_slow_movement_annotation != 0)
			machine_reference = machine_death.machine.last_slow_movement_annotation;
		else if (machine_death.machine.last_fast_movement_annotation != 0)
			machine_reference = machine_death.machine.last_fast_movement_annotation;
		if (machine_reference == 0) {	//discard by hand annotations without machine annotations
			//cout << "Discarding a stray by hand annotation.\n";
				continue;
		}
		if (machine_reference != 0 && 
			description_set.descriptions.size() > 1 &&
			machine_reference->annotation_source == //and only for multiworm clusters that the machine recognized
			ns_death_time_annotation::ns_lifespan_machine		 //not by hand annotations that were not matched up to any machine annotation.
			) {


			ns_death_time_annotation::ns_by_hand_annotation_integration_strategy by_hand_annotation_strategy_2(by_hand_annotation_strategy);



			//if we need to output both machine and by hand death times, we need to generate censoring data twice
			if (by_hand_annotation_strategy == ns_death_time_annotation::ns_machine_and_by_hand_annotations) {
				ns_death_time_annotation_set set;
				ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
					q->properties, description_set, ns_metadata_worm_properties::ns_best_guess_death,set, ns_death_time_annotation::ns_only_machine_annotations);
				for (unsigned int i = 0; i < set.size(); i++) {
					set[i].volatile_matches_machine_detected_death = true;
					aggregator.add(set[i], use_by_hand_worm_cluster_annotations);
				}
				by_hand_annotation_strategy_2 = ns_death_time_annotation::ns_only_by_hand_annotations;
			}

			ns_death_time_annotation_set set;
			ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
				q->properties, description_set, ns_metadata_worm_properties::ns_best_guess_death, set, by_hand_annotation_strategy_2);
			for (unsigned int i = 0; i < set.size(); i++) {
				set[i].volatile_matches_machine_detected_death = true;
				aggregator.add(set[i], use_by_hand_worm_cluster_annotations);
			}

		}
		else {
			if (description_set.descriptions.size() > 1)
				throw ns_ex("ns_death_time_annotation_compiler_region::generate_survival_curve::Found an incorrectly annotated multi-worm description set!");
			ns_dying_animal_description_base<const ns_death_time_annotation>& death(description_set.descriptions[0]);

			switch (by_hand_annotation_strategy) {
			case ns_death_time_annotation::ns_only_machine_annotations:
				ns_add_normal_death_to_set(ns_generate_machine_annotations, death,q->properties, latest_interval, by_hand_annotation_strategy, use_by_hand_worm_cluster_annotations, aggregator);
				break;
			case ns_death_time_annotation::ns_machine_annotations_if_no_by_hand:
				ns_add_normal_death_to_set(ns_generate_best_annotation, death, q->properties, latest_interval, by_hand_annotation_strategy, use_by_hand_worm_cluster_annotations, aggregator);
				break;
			case ns_death_time_annotation::ns_only_by_hand_annotations:
				ns_add_normal_death_to_set(ns_generate_by_hand_annotations, death, q->properties, latest_interval, by_hand_annotation_strategy, use_by_hand_worm_cluster_annotations, aggregator);
				break;
			case ns_death_time_annotation::ns_machine_and_by_hand_annotations:
				ns_add_normal_death_to_set(ns_generate_machine_annotations, death, q->properties, latest_interval, by_hand_annotation_strategy, use_by_hand_worm_cluster_annotations, aggregator);
				ns_add_normal_death_to_set(ns_generate_by_hand_annotations, death, q->properties, latest_interval, by_hand_annotation_strategy, use_by_hand_worm_cluster_annotations, aggregator);
				break;
			default: throw ns_ex("Unknown by hand annotation integration strategy");
			}
		}
	}
	for (unsigned int i = 0; i < non_location_events.size(); i++){
		if (non_location_events[i].by_hand_annotation_integration_strategy != by_hand_annotation_strategy)
			continue;
		//std::cerr << "Adding censoring event\n";
		ns_death_time_annotation b(non_location_events[i]);
		b.volatile_matches_machine_detected_death = true;
		aggregator.add(b,use_by_hand_worm_cluster_annotations);
	}

	aggregator.populate_survival_data(curve);
	if (multiple_event_count > 0 && warn_on_movement_problems){
		cerr << "WARNING: " << multiple_event_count << " duplicate animal records were found in analysis.  This could indicate an unusual image processing error\n";
	}

}
struct ns_death_diagnostic{
	double machine_death_time,
			machine_stationary_time,
			machine_longest_gap,
		   by_hand_death_time;
	bool by_hand_excluded;
	static void out_header(std::ostream & o){
		o << "Machine Death Time (days),Machine Last Moving Time (days), Machine Longest gap (days), By Hand Specified,By Hand Death Time (days), By Hand Excluded";
	}
	void out(std::ostream & o, const unsigned long time_zero) const{
		o << (machine_death_time - time_zero)/(60*60*24)<< ","
		  << (machine_stationary_time - time_zero)/(60*60*24) << ","
		  << (machine_longest_gap)/(60*60*24) << ",";
		if (by_hand_death_time != 0)
			o << "1," << (by_hand_death_time - time_zero)/(60*60*24) << ",";
		else o << "0,,";
		if (by_hand_excluded)
			o << "1";
		else o<< "0";
	}
};

void ns_death_time_annotation_compiler::generate_validation_information(std::ostream & o) const{
	ns_region_metadata::out_JMP_plate_identity_header(o);
	o << ",";
	ns_death_diagnostic::out_header(o);
	o << "\n";
	for(ns_region_list::const_iterator p = regions.begin(); p!= regions.end(); ++p){
		const unsigned long tz = p->second.metadata.time_at_which_animals_had_zero_age;
		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = p->second.locations.begin(); q != p->second.locations.end(); q++){
			ns_dying_animal_description_set_const descriptions;
			q->generate_dying_animal_description_const(false, descriptions);
			if (descriptions.descriptions.empty())
				throw ns_ex("Encountered empty descriptions set!");
			if (descriptions.descriptions[0].machine.best_guess_death_annotation == 0)
				continue;
			//only output validation for first death in multi-worm clusters.
			ns_dying_animal_description_base<const ns_death_time_annotation> & d(descriptions.descriptions[0]);
			p->second.metadata.out_JMP_plate_identity_data(o);

			o << ",";
			ns_death_diagnostic data;
			data.machine_death_time = d.machine.best_guess_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
			data.machine_longest_gap = d.machine.best_guess_death_annotation->longest_gap_without_observation;
			if (d.machine.last_slow_movement_annotation != 0)
				data.machine_stationary_time = d.machine.last_slow_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
			else if (d.machine.last_fast_movement_annotation != 0)
				data.machine_stationary_time = d.machine.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
			else data.machine_stationary_time = 0;
			if (d.by_hand.best_guess_death_annotation != 0) {
				data.by_hand_death_time = d.by_hand.best_guess_death_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();
				data.by_hand_excluded = d.by_hand.best_guess_death_annotation->is_excluded();

			}
			else {
				data.by_hand_death_time = 0;
				data.by_hand_excluded = d.machine.best_guess_death_annotation->is_excluded();

			}
			if (q->properties.is_excluded())
				data.by_hand_excluded = true;
			data.out(o, tz);
			o << "\n";
		}
	}
}

void ns_death_time_annotation_compiler::empty_but_keep_regions_and_memory() {

	for (ns_region_list::iterator p = regions.begin(); p != regions.end(); ++p) 
		p->second.empty_but_keep_memory();
}
void ns_death_time_annotation_compiler::generate_survival_curve_set(ns_lifespan_experiment_set & survival_curves, const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & death_times_to_use,const bool use_by_hand_worm_cluster_annotations,const bool warn_on_movement_problems) const{
	unsigned int number_of_nonempty_curves(0);
	survival_curves.resize(regions.size());
	for(ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		const std::vector<ns_survival_data>::size_type s(survival_curves.size());
		p->second.generate_survival_curve(survival_curves.curve(number_of_nonempty_curves),death_times_to_use,use_by_hand_worm_cluster_annotations,warn_on_movement_problems);
		if (survival_curves.curve(number_of_nonempty_curves).timepoints.size() > 0)//don't include empty curves
			number_of_nonempty_curves++;
	}
	survival_curves.resize(number_of_nonempty_curves);
}

void ns_death_time_annotation_compiler::normalize_times_to_zero_age(){
	for(ns_region_list::iterator p = regions.begin(); p != regions.end(); ++p){
		for (ns_death_time_annotation_compiler_region::ns_location_list::iterator q = p->second.locations.begin(); q != p->second.locations.end(); ++q){
			for (unsigned int i = 0; i < q->annotations.size(); i++){
				if (q->annotations[i].time.period_end != 0)
					q->annotations[i].time.period_end-=p->second.metadata.time_at_which_animals_had_zero_age;
				if (q->annotations[i].time.period_start != 0)
					q->annotations[i].time.period_start-=p->second.metadata.time_at_which_animals_had_zero_age;
			}
		}
		for (unsigned int i = 0; i < p->second.non_location_events.size(); i++){
			if (p->second.non_location_events[i].time.period_end != 0)
				p->second.non_location_events[i].time.period_end-=p->second.metadata.time_at_which_animals_had_zero_age;
			if (p->second.non_location_events[i].time.period_start != 0)
				p->second.non_location_events[i].time.period_start-=p->second.metadata.time_at_which_animals_had_zero_age;
		}
		for (unsigned int i = 0; i < p->second.fast_moving_animals.size(); i++){
			if (p->second.fast_moving_animals[i].time.period_end != 0)
				p->second.fast_moving_animals[i].time.period_end-=p->second.metadata.time_at_which_animals_had_zero_age;
			if (p->second.fast_moving_animals[i].time.period_start != 0)
				p->second.fast_moving_animals[i].time.period_start-=p->second.metadata.time_at_which_animals_had_zero_age;
		}
	}
}

ns_lock ns_death_time_annotation_flag::flag_lock("flag");
#ifndef NS_NO_SQL
void ns_death_time_annotation_flag::get_flags_from_db(ns_image_server_sql * sql){


	*sql << "SELECT id, label_short, label, exclude, next_flag_name_in_order, hidden, color FROM annotation_flags";
	ns_sql_result res;
	sql->get_rows(res);
	ns_acquire_lock_for_scope lock(flag_lock,__FILE__,__LINE__);
	cached_flags_by_short_label.clear();
	const bool no_flags_in_database(res.empty());
	for (unsigned int i = 0; i < res.size(); i++){
		const std::string & flag_name(res[i][1].c_str());
		ns_flag_cache_by_short_label::iterator p = cached_flags_by_short_label.find(flag_name);
		if (p != cached_flags_by_short_label.end())
			throw ns_ex("Death Time Annotation Flag ") << flag_name << " has been specified twice; first as " << p->second.label_short << " and then as " << res[i][1];
		ns_death_time_annotation_flag f(ns_death_time_annotation_flag(flag_name,res[i][2],res[i][3] == "1",res[i][4],res[i][6]));
		f.cached_hidden = (res[i][5]=="1");
	//	cached_flags_by_id[flag_id] = f;
		cached_flags_by_short_label[res[i][1]] = f;
	}
	std::vector<ns_death_time_annotation_flag> v;
	//link in defaults
	generate_default_flags(v);
	unsigned long number_of_ends(0);
	ns_flag_cache_by_short_label::iterator tail = cached_flags_by_short_label.end();
	for (ns_flag_cache_by_short_label::iterator p= cached_flags_by_short_label.begin(); p != cached_flags_by_short_label.end(); p++){
		if (p->second.next_flag_name_in_order.empty()){
			if (number_of_ends == 0)
				tail = p;
			number_of_ends++;
		}
	}

	const bool need_to_recalculate_order(!cached_flags_by_short_label.empty() && number_of_ends != 1);

	//look for missing default flags in the database
	for (unsigned int i = 0; i < v.size(); i++){
		ns_flag_cache_by_short_label::iterator p = cached_flags_by_short_label.find(v[i].label_short);
		if (p == cached_flags_by_short_label.end()){
			if (tail != cached_flags_by_short_label.end())
				tail->second.next_flag_name_in_order = v[i].label_short;
			tail = cached_flags_by_short_label.insert(ns_flag_cache_by_short_label::value_type(v[i].label_short,v[i])).first;

			cached_flags_by_short_label[v[i].label_short] = v[i];
			if (v[i].label_short.empty())
				continue;	//don't include ns_none
			//update db with default flags if they aren't present.
			*sql <<  "INSERT INTO annotation_flags SET label_short='" <<v[i].label_short
				<< "',label='"<<v[i].cached_label << "',exclude=" << (v[i].cached_excluded?"1":"0") << ",next_flag_name_in_order='" << v[i].next_flag_name_in_order << "'";
			sql->send_query();
		}
		else{
			//accidentally setting default flag colors to zero can make it look
			//like annotations aren't being loaded or storred correctly
			//so we force it
			if (p->second.cached_color == ns_color_8(0,0,0)){
				p->second.cached_color = v[i].cached_color;
			}
		}

	}
	lock.release();
};
#endif

const char * ns_death_time_annotation_flag::first_default_flag_short_label(){
	return "MULTI_ERR";
}
ns_death_time_annotation_flag ns_death_time_annotation_flag::extra_worm_from_multiworm_disambiguation(){
	return ns_death_time_annotation_flag(first_default_flag_short_label(),
	"Extra worm from multiple-worm disambiguation",true,"3D_CLOUD_ERR","FFFFCC");
}
void ns_death_time_annotation_flag::generate_default_flags(std::vector<ns_death_time_annotation_flag> & flags){
	flags.resize(0);
	ns_death_time_annotation_flag f(ns_death_time_annotation_flag::none());
	f.next_flag_name_in_order = extra_worm_from_multiworm_disambiguation().label_short;
	flags.push_back(f);
	flags.push_back(extra_worm_from_multiworm_disambiguation());
	flags.push_back(ns_death_time_annotation_flag("3D_CLOUD_ERR",
		"Point Cloud Analaysis Error",true,"REG_ERR","FFFF80"));
	flags.push_back(ns_death_time_annotation_flag("REG_ERR",
		"Movement Registration Error",true,"2ND_WORM_ERR","FF8080"));
	flags.push_back(ns_death_time_annotation_flag("2ND_WORM_ERR",
		"Additional Worm Confuses Analysis",true,"","80FF80"));
	flags.push_back(ns_death_time_annotation_flag("STILL_ALIVE",
		"Worm is still alive at the end of the experiment",false,"VIS_INSPECT","CCCCCC"));
	flags.push_back(ns_death_time_annotation_flag("VIS_INSPECT",
		"Flagged for visual inspection", false, "", "FFCC11"));
}

std::string ns_death_time_annotation_flag::label() const{
		if (!specified())
			return "";
		if (label_is_cached)
			return cached_label;
		get_cached_info();
		return cached_label;
	}
void ns_death_time_annotation_flag::get_cached_info() const{

	ns_acquire_lock_for_scope lock(flag_lock,__FILE__,__LINE__);
	if (cached_flags_by_short_label.empty())
		throw ns_ex("Death time annotation flags have not been loaded from the database!");
	ns_flag_cache_by_short_label::const_iterator p(cached_flags_by_short_label.find(this->label_short));
	if (p == cached_flags_by_short_label.end())
		throw ns_ex("Could not load flag information from db for flag ") << label_short;
	label_is_cached = true;
	cached_label = p->second.cached_label;
	cached_excluded = p->second.cached_excluded;
	cached_hidden = p->second.cached_hidden;
	cached_color = p->second.cached_color;
	lock.release();
}
ns_color_8 ns_death_time_annotation_flag::flag_color() const{
	if (!label_is_cached)
		get_cached_info();
	return cached_color;
}
void ns_death_time_annotation_flag::step_event(){

	ns_acquire_lock_for_scope lock(flag_lock,__FILE__,__LINE__);
	if (cached_flags_by_short_label.empty())
		throw ns_ex("ns_death_time_annotation_flag::step_event()::cached flags are not loaded.");

	ns_flag_cache_by_short_label::const_iterator p(cached_flags_by_short_label.find(label_short));
	if (p == cached_flags_by_short_label.end())
		throw ns_ex("ns_death_time_annotation_flag::step_event()::could not find current flag in cache");
	ns_flag_cache_by_short_label::const_iterator q(cached_flags_by_short_label.find(p->second.next_flag_name_in_order));
	if (q == cached_flags_by_short_label.end())
		*this = none();
	*this = q->second;
	lock.release();
	if (this->cached_hidden)
		step_event();
}

void ns_zero_death_interval(ns_death_time_annotation & e) {
	e.time.period_start = e.time.period_end = 0;
	e.time.period_end_was_not_observed = false;
	e.time.period_start_was_not_observed = false;
	e.event_explicitness = ns_death_time_annotation::ns_not_explicit;
}
void ns_crop_time(const ns_time_path_limits & limits, const ns_death_time_annotation_time_interval & first_observation_in_path, const ns_death_time_annotation_time_interval & last_observation_in_path, ns_death_time_annotation_time_interval & target) {
	if (target.period_end == 0)
		return;
	//only crop to the beginning if the animal didn't become stationary in the first frame!
	if (!limits.interval_before_first_observation.period_start_was_not_observed
		&&
		(target.period_start <= limits.interval_before_first_observation.period_start ||
			target.period_end <= limits.interval_before_first_observation.period_end))
		target = limits.interval_before_first_observation;

	if (!limits.last_obsevation_of_plate.period_end_was_not_observed &&
		target.period_end >= last_observation_in_path.period_end)
		target = last_observation_in_path;
}
#ifndef NS_ONLY_IMAGE_ACQUISITION
#include "ns_annotation_handling_for_visualization.h"
void ns_death_timing_data::draw_movement_diagram(const ns_vector_2i & pos, const ns_vector_2i & total_size, const ns_time_path_limits & path_observation_limits, const ns_death_time_annotation_time_interval & current_interval, ns_image_standard & im, const float & scaling, const int current_interval_marker_min_width, const ns_current_position_marker marker_type, const ns_draw_relative_spec draw_spec) {
  //return;

		const unsigned long no_expansion_button_width(total_size.y);
		const unsigned long no_expansion_button_border(no_expansion_button_border);
		const unsigned long right_hand_margin(no_expansion_button_width + no_expansion_button_border);


		const ns_vector_2i size(total_size.x - right_hand_margin, total_size.y);

		if (path_observation_limits.last_obsevation_of_plate.period_end <= path_observation_limits.first_obsevation_of_plate.period_start)
			throw ns_ex("draw_movement_diagram()::Invalid path duration");
       
		ns_death_time_annotation_time_interval start_interval, stop_interval;
		unsigned long path_start_time_t, last_path_frame_time_t;
		if (draw_spec == ns_draw_relative_to_path) {
			start_interval = path_observation_limits.interval_before_first_observation;
			stop_interval = path_observation_limits.interval_after_last_observation;
			path_start_time_t = start_interval.period_end;
			last_path_frame_time_t = stop_interval.period_start;
		}
		else {
			start_interval = path_observation_limits.first_obsevation_of_plate;
			stop_interval = path_observation_limits.last_obsevation_of_plate;
			path_start_time_t = start_interval.period_start;
			last_path_frame_time_t = stop_interval.period_end;
		}

		const unsigned long path_start_time(path_start_time_t);
		const unsigned long last_path_frame_time(last_path_frame_time_t);
		const unsigned long total_time = last_path_frame_time - path_start_time;
		const float pre_current_time_scaling((marker_type == ns_highlight_up_until_current_time) ? 0.5 : 1);



		const float dt(size.x / (float)total_time);
		unsigned long worm_arrival_time;
		if (!fast_movement_cessation.time.period_end_was_not_observed)
		  worm_arrival_time = fast_movement_cessation.time.period_end;
		else{
		  cerr << "No fast movement cessation time specified.\n";
		  worm_arrival_time = path_start_time;
		}
		/*
		cout << path_start_time << "\n";
		cout << worm_arrival_time << "\n";
		cout << dt << "\n";
		cout << translation_cessation.time.period_end << "\n";
		cout << last_path_frame_time << "\n";
		*/
		//we push death relaxation start forward in time as we find more data
		//unsigned long death_relaxation_start_time = worm_arrival_time;

		unsigned int x;
		ns_color_8 c;
		c = ns_movement_colors::color(ns_movement_fast)*scaling;
		if (worm_arrival_time < path_start_time)
			throw ns_ex("Invalid time of additional worm entry into cluster");
		for (x = 0; x < (worm_arrival_time - path_start_time)*dt; x++) {
			if (x + pos.x >= im.properties().width || im.properties().components != 3)
				throw ns_ex("Yikes! 1");
			const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
			for (unsigned int y = 0; y < size.y; y++) {
				im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
			}
		}

		c = ns_movement_colors::color(ns_movement_slow)*scaling;
		if (translation_cessation.time.period_end != 0 && translation_cessation.time.period_end >= path_start_time) {

			//death_relaxation_start_time = translation_cessation.time.period_end;
			if (translation_cessation.time.period_end > last_path_frame_time)
				throw ns_ex("Invalid translation cessation time");
			for (; x < (int)((translation_cessation.time.period_end - path_start_time)*dt); x++) {
				if (pos.x >= im.properties().width || im.properties().components != 3)
					throw ns_ex("Yikes! 2");
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;

				for (unsigned int y = 0; y < size.y; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}
			c = ns_movement_colors::color(ns_movement_posture)*scaling;
		}
		if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end >= path_start_time) {

			//death_relaxation_start_time = movement_cessation.time.period_end;
			if (movement_cessation.time.period_end > last_path_frame_time)
				throw ns_ex("Invalid Movement Cessation Time");
			for (; x < (int)((movement_cessation.time.period_end - path_start_time)*dt); x++) {
				if (x + pos.x >= im.properties().width || im.properties().components != 3)
					throw ns_ex("Yikes! 3");
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
				for (unsigned int y = 0; y < size.y; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}

			c = ns_movement_colors::color(ns_movement_stationary)*scaling;
			if (movement_cessation.excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment)
				c = ns_movement_colors::color(ns_movement_machine_excluded)*scaling;
		}
		//here the color is set by the value of the last specified event
		for (; x < size.x; x++) {

			if (x + pos.x >= im.properties().width || im.properties().components != 3)
				throw ns_ex("Yikes! 6");
			const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
			for (unsigned int y = 0; y < size.y; y++) {
				im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
			}
		}

		if (death_associated_expansion_stop.time.period_end != 0 && death_associated_expansion_stop.time.period_end >= path_start_time) {
			unsigned long death_relaxation_start_time;
			if (death_associated_expansion_start.time.period_end != 0)
				death_relaxation_start_time = death_associated_expansion_start.time.period_end;
			else death_relaxation_start_time = death_associated_expansion_stop.time.period_end;
			//draw animal as dead until death relaxation begins
			if (death_relaxation_start_time > last_path_frame_time)
				throw ns_ex("Invalid Death Posture Relaxation Start Time!");
			c = ns_movement_colors::color(ns_movement_death_associated_expansion)*scaling;

			//make sure there is a minimum width to draw
			long start_pos((death_relaxation_start_time - (ns_s64_bit)path_start_time)*dt),
				stop_pos((death_associated_expansion_stop.time.period_end - (ns_s64_bit)path_start_time)*dt);
		//	std::cout << start_pos << "," << stop_pos << " -> ";
			if (stop_pos - start_pos < 4) {
				if (start_pos > 2)
					start_pos -= 2;
				if (stop_pos + 2 < size.x) {
					stop_pos += 2;
				}
			}
		//	std::cout << start_pos << "," << stop_pos << "\n";
			for (int x = start_pos; x < stop_pos; x++) {
				if (x + pos.x >= im.properties().width || im.properties().components != 3) {
					std::cout << "Out of bounds death relaxation time interval draw (" << x + pos.x << ") in an image (" << im.properties().width << "," << im.properties().height << "\n";
					break;
				}
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
				for (unsigned int y = 0; y < size.y / 2; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}
		}
		if (death_associated_post_expansion_contraction_stop.time.period_end != 0 && death_associated_post_expansion_contraction_stop.time.period_end >= path_start_time) {
			unsigned long ds;
			if (death_associated_post_expansion_contraction_start.time.period_end != 0)
				ds = death_associated_post_expansion_contraction_start.time.period_end;
			else ds = death_associated_post_expansion_contraction_stop.time.period_end;
			//draw animal as dead until death relaxation begins
			if (ds > last_path_frame_time)
				throw ns_ex("Invalid Death Posture Relaxation Start Time!");
			c = ns_movement_colors::color(ns_movement_death_associated_post_expansion_contraction)*scaling;

			//make sure there is a minimum width to draw
			long start_pos((ds - (ns_s64_bit)path_start_time)*dt),
				stop_pos((death_associated_post_expansion_contraction_stop.time.period_end - (ns_s64_bit)path_start_time)*dt);
			//	std::cout << start_pos << "," << stop_pos << " -> ";
			if (stop_pos - start_pos < 4) {
				if (start_pos > 2)
					start_pos -= 2;
				if (stop_pos + 2 < size.x) {
					stop_pos += 2;
				}
			}
			//	std::cout << start_pos << "," << stop_pos << "\n";
			for (int x = start_pos; x < stop_pos; x++) {
				if (x + pos.x >= im.properties().width || im.properties().components != 3) {
					std::cout << "Out of bounds death relaxation time interval draw (" << x + pos.x << ") in an image (" << im.properties().width << "," << im.properties().height << "\n";
					break;
				}
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
				for (unsigned int y = 0; y < size.y / 2; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}
		}
		//draw death relaxation button (empty box if not specified, filled if specified)
		ns_color_8 edge_color = ns_movement_colors::color(ns_movement_death_associated_expansion)*scaling;
		ns_color_8 center_color;

		switch (death_associated_expansion_stop.event_explicitness) {
		case ns_death_time_annotation::ns_not_specified:
		case ns_death_time_annotation::ns_not_explicit:
		case ns_death_time_annotation::ns_explicitly_not_observed:
			center_color = ns_color_8(0, 0, 0)*scaling; break;
		case ns_death_time_annotation::ns_explicitly_observed:
			center_color = ns_movement_colors::color(ns_movement_death_associated_expansion)*scaling; break;
		default: throw ns_ex("Uknown event explicit state");
		}
		const unsigned int button_left = size.x + no_expansion_button_border + pos.x;
		for (unsigned long y = 0; y < no_expansion_button_width; y++) {
			for (unsigned long x = 0 ; x < no_expansion_button_width; x++) {
				if (y < 4 || y + 4 > no_expansion_button_width ||
					x < 4 || x + 4 > no_expansion_button_width) {
					im[y + pos.y][3 * (x + button_left) + 0] = edge_color.x;
					im[y + pos.y][3 * (x + button_left) + 1] = edge_color.y;
					im[y + pos.y][3 * (x + button_left) + 2] = edge_color.z;
				}
				//draw an x for explicitly not observed
				else if (death_associated_expansion_stop.event_explicitness == ns_death_time_annotation::ns_explicitly_not_observed &&
					(x  == y || x == no_expansion_button_width - y-1)) {
					im[y + pos.y][3 * (x + button_left) + 0] = edge_color.x;
					im[y + pos.y][3 * (x + button_left) + 1] = edge_color.y;
					im[y + pos.y][3 * (x + button_left) + 2] = edge_color.z;
				}
				else {
					im[y + pos.y][3 * (x + button_left) + 0] = center_color.x;
					im[y + pos.y][3 * (x + button_left) + 1] = center_color.y;
					im[y + pos.y][3 * (x + button_left) + 2] = center_color.z;
				}
			}
		}
		

		//highlight current time interval
		unsigned long interval_start((current_interval.period_start - path_start_time)*dt),
			interval_end((current_interval.period_end - path_start_time)*dt);
		//cerr << "Interval duration: " << current_interval.period_end - current_interval.period_start << " seconds; " 
		//	<< (current_interval.period_end - current_interval.period_start)*dt << " est pixels; " << interval_end-interval_start << " actual pixels\n";
		if (interval_end - interval_start < current_interval_marker_min_width) {
			interval_end = interval_start + current_interval_marker_min_width;
			if (interval_end >= size.x)
				interval_end = size.x - 1;
		}
		for (unsigned int y = 0; y < size.y; y++) {
			if (!current_interval.period_start_was_not_observed &&
				!current_interval.period_end_was_not_observed &&
				current_interval.period_start >= path_start_time &&
				current_interval.period_end <= last_path_frame_time) {

				if (marker_type == ns_highlight_up_until_current_time) {
					for (unsigned long x = interval_start; x < interval_end; x++) {
						if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
							throw ns_ex("Yikes! 6");

						im[pos.y + y][3 * (x + pos.x)] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x)]) : im[pos.y + y][3 * (x + pos.x)];
						im[pos.y + y][3 * (x + pos.x) + 1] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 1]) : im[pos.y + y][3 * (x + pos.x) + 1];
						im[pos.y + y][3 * (x + pos.x) + 2] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 2]) : im[pos.y + y][3 * (x + pos.x) + 2];
					}
				}
				else if (marker_type == ns_wide_dotted_marker) {
					for (unsigned long x = interval_start; x < interval_end; x++) {
						if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
							throw ns_ex("Yikes! 6");
						ns_8_bit cc = ((x - interval_start) % 2) ? ((y % 2) ? 255 : 0) : ((y % 2) ? 0 : 255);
						im[pos.y + y][3 * (x + pos.x)] = cc;
						im[pos.y + y][3 * (x + pos.x) + 1] = cc;
						im[pos.y + y][3 * (x + pos.x) + 2] = cc;
					}
				}
				else {
					for (unsigned long x = interval_start; x < interval_end; x++) {
						if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
							throw ns_ex("Yikes! 6");

						im[pos.y + y][3 * (x + pos.x)] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x)]) : im[pos.y + y][3 * (x + pos.x)];
						im[pos.y + y][3 * (x + pos.x) + 1] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 1]) : im[pos.y + y][3 * (x + pos.x) + 1];
						im[pos.y + y][3 * (x + pos.x) + 2] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 2]) : im[pos.y + y][3 * (x + pos.x) + 2];
					}

					int bar_start = interval_end;
					if (bar_start > current_interval_marker_min_width)
						bar_start -= current_interval_marker_min_width;
					else bar_start = 0;
					for (unsigned int x = bar_start; x < interval_end; x++) {
						im[pos.y + y][3 * (x + pos.x)] = 40;
						im[pos.y + y][3 * (x + pos.x) + 1] = 40;
						im[pos.y + y][3 * (x + pos.x) + 2] = 40;
					}
				}
			}
		}
}
#endif
		

//ns_death_time_annotation_flag::ns_flag_cache cached_flags_by_id;
ns_death_time_annotation_flag::ns_flag_cache_by_short_label ns_death_time_annotation_flag::cached_flags_by_short_label;
