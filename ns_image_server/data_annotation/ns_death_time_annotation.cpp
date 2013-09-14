#include "ns_death_time_annotation_set.h"
#include "ns_time_path_solver.h"
#include "ns_xml.h"
using namespace std;


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


std::string ns_death_time_annotation::description() const{
	return ns_movement_event_to_string(type) + "::" + source_type_to_string(annotation_source) + 
		"::" + exclusion_string(excluded) +"::pos(" + ns_to_string(position.x) + "," + ns_to_string(position.y) +
		"): size(" + ns_to_string(size.x) + "," + ns_to_string(size.y) + ") :" + ns_to_string(time.period_start) + "-" + ns_to_string(time.period_end) + "(" + ns_format_time_string_for_human(time.period_start) + "-" + ns_format_time_string_for_human(time.period_end) + ") "
				" path_id(" + ns_to_string(stationary_path_id.group_id) + "," + ns_to_string(stationary_path_id.path_id) + ") "+ ns_to_string(stationary_path_id.detection_set_id) + ") "
				+ ((disambiguation_type==ns_death_time_annotation::ns_single_worm)?"Single Worm Source":"Multi-worm disambiguation")
				+ " [Machine : " + ns_to_string(number_of_worms_at_location_marked_by_machine ) + " / Hand : " + ns_to_string(number_of_worms_at_location_marked_by_hand) + " extra worms at location ] Flag: " + flag.label() + 
				+ " source(" + source_type_to_string(annotation_source) + ")";
}

std::string ns_death_time_annotation::to_string() const{
	std::vector<std::string> s(35);
	s[0] = ns_to_string(exclusion_value(excluded));
	s[1] = ns_to_string(loglikelihood); //room for expansion; the old extra worms at location field
	s[2] = ns_to_string((unsigned long)disambiguation_type);
	s[3] = ns_to_string((unsigned long)type);
	std::string as(annotation_source_details);
	for (unsigned int i = 0; i < as.size(); i++)
		if (as[i]==',') as[i]='|';
	
	s[4] = as;
	s[5] = ns_to_string(annotation_time);
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

	string ret;
	ret+=s[0];
	for (unsigned int i = 1; i < s.size(); i++){
		ret += ",";
		ret += s[i];
	}
	return ret;
}
void ns_death_time_annotation::from_string(const std::string v){
	std::vector<std::string> s(1);
	for (unsigned int i = 0; i < v.size(); i++){											
		if (v[i] == ',')																	
			s.resize(s.size()+1);															
		else (*s.rbegin())+=v[i];															
	}																						
	if (s.size() != 27 && s.size() != 23 && s.size() != 35)																		
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
																							
	annotation_time = atol(s[5].c_str());													
	time.period_end = atol(s[6].c_str());
	region_info_id = atol(s[7].c_str());
	region_id = atol(s[8].c_str());
	position.x = atol(s[9].c_str());
	position.y = atol(s[10].c_str());
	size.x = atol(s[11].c_str());
	size.y = atol(s[12].c_str());
	annotation_source = (ns_annotation_source_type)atol(s[13].c_str());
	stationary_path_id.group_id = atol(s[14].c_str());
	stationary_path_id.path_id = atol(s[15].c_str());
	
	stationary_path_id.detection_set_id = atol(s[16].c_str());
	time.period_start = atol(s[17].c_str());
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
		case ns_worm_death_posture_relaxation_termination: return "worm death relaxation termination";
		case ns_stationary_worm_disappearance: return "stationary worm disappeared";
		case ns_moving_worm_disappearance: return "moving worm dissapeared";
		case ns_additional_worm_entry: return "additional worm entered path";
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
		case ns_worm_death_posture_relaxation_termination: return "worm_death_relaxation_termination";
		case ns_stationary_worm_disappearance: return "";
		case ns_moving_worm_disappearance: return "";
		case ns_additional_worm_entry: return "";
		default: throw ns_ex("ns_movement_event_to_string()::Unknown event type ") << (int)t;
	}
}

bool ns_movement_event_is_a_state_transition_event(const ns_movement_event & t){
	switch(t){
		case ns_fast_movement_cessation:
		case ns_translation_cessation:
		case ns_movement_cessation: 
		case ns_worm_death_posture_relaxation_termination:
		case ns_moving_worm_disappearance:
		case ns_stationary_worm_disappearance: 
		case ns_additional_worm_entry:
			return true;

		case ns_movement_censored_worm_observed: 
		case ns_fast_moving_worm_observed:
		case ns_slow_moving_worm_observed:
		case ns_posture_changing_worm_observed: 
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
			case ns_movement_death_posture_relaxation: return "Death Posture Relaxation";
			case ns_movement_total: return "Total";
			case ns_movement_not_calculated: return "Not calculated";
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
					case ns_worm_death_posture_relaxation_termination:
						state = ns_movement_death_posture_relaxation;
					case ns_movement_censored_worm_observed:
						state = ns_movement_machine_excluded;
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
				state
			);
		}
	}
}
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

void write_column_format_data(std::ostream & o, const ns_death_time_annotation & a){
	o << (int)a.type << ","
		<< a.time.period_end << ","
		<< a.region_id << ","
		<< a.region_info_id << ","
		<< a.position.x << ","
		<< a.position.y << ","
		<< a.size.x << ","
		<< a.size.y << ","
		<< (int)a.annotation_source << ","
		<< a.annotation_time << ","
		<< a.annotation_source_details << ","
		<< ns_death_time_annotation::exclusion_value(a.excluded) << ","
		<< a.loglikelihood << "," //room for expansion; old location of extra worms
		<< (int)a.disambiguation_type << ","
		<< a.stationary_path_id.group_id << ","
		<< a.stationary_path_id.path_id << ","
		<< a.stationary_path_id.detection_set_id << ","
		<< a.time.period_start << ","
		<< a.time.interval_bound_code() << ","
		<< a.flag.label_short <<"," 
		<< a.animal_is_part_of_a_complete_trace << ","
		<< a.number_of_worms_at_location_marked_by_machine << ","
		<< a.number_of_worms_at_location_marked_by_hand << ","
		<< (int)a.multiworm_censoring_strategy <<","
		<< (int)a.missing_worm_return_strategy <<","
		<< a.animal_id_at_position << ","
		<< (int)a.event_observation_type << "," 
		<< a.longest_gap_without_observation << ","
		// reserved for future use
		<< ",,,,,,,"
		<< "\n";
}
void write_column_format_header(std::ostream & o){
	o << "Event Type,Event Time, Region Image Id, Region Info Id, Position x, Position y, Size X, Size y, Annotation Source,"
		"Annotation Finish Time, Annotation Source Details, Censored, Loglikelihood, Disambiguation Type, Stationary Worm Group ID,"
		 "Stationary Worm Path Id, Stationary Worm Detection Set ID,Annotation Start Time,Flag Id,Flag Label,"
		 "Complete Trace,Number of Worms Annotated By machine,Number of Worms Annotated By Hand,"
		 "Multiple worm cluster censoring strategy,Missing Worm Return Strategy,Extra Animal ID at position,Event Observation Type,Longest gap without observation,"
		 "(Room For Expansion),(RFE),(RFE),(RFE),(RFE),(RFE),(RFE),(RFE)\n";
}
	
void ns_death_time_annotation_set::write_split_file_column_format(std::ostream & censored_and_transition_file, std::ostream & state_file)const{
	write_column_format_header(censored_and_transition_file);
	write_column_format_header(state_file);
	for (unsigned int i = 0; i < events.size(); i++){
		if (!annotation_matches(ns_movement_states,events[i]))
			write_column_format_data(censored_and_transition_file,events[i]);
		else 
			write_column_format_data(state_file,events[i]);
	
	}

}

void ns_death_time_annotation_set::write_column_format(std::ostream & o)const{
	write_column_format_header(o);
	for (unsigned int i = 0; i < events.size(); i++){
		write_column_format_data(o,events[i]);
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
void ns_death_time_annotation_set::read_column_format(const  ns_annotation_type_to_load & type_to_load, std::istream & i, const bool exclude_fast_moving_animals){
	char a(' ');
	while (!i.fail() && isspace(a))
		a=i.get();
	if (i.fail() || a != 'E')
		throw ns_ex("Could not read death time annotation format");
	
	string val;
	getline(i,val,'\n');
	if (i.fail())
		throw ns_ex("Could not read death time annotation format");
	bool newest_old_record(false);
	if (val.find("Event Observation Type") == val.npos)
		newest_old_record = true;
	bool kind_of_old_record(false);
	{
		int cur_column(0);
		string column_text,
				previous_column_text;
		for (unsigned int i = 0; i < val.size(); i++){
			if (val[i] == ','){
				cur_column++;
				previous_column_text = column_text;
				column_text.resize(0);
			}
			else column_text.push_back(val[i]);
			if (cur_column == 13){
				if (previous_column_text == " Number of Extra Worms")
					kind_of_old_record = true;
				break;
			}
			if (cur_column > 13)
				break;
		}
	}
	ns_death_time_annotation annotation;
	val.resize(0);
	while(true){
		annotation.time.period_start = 0;
		annotation.time.period_end = 0;
		getline(i,val,',');
		if (i.fail())
			return;
		events.resize(events.size()+1);
		ns_death_time_annotation & e(*events.rbegin());
		e.type = (ns_movement_event)atol(val.c_str());
		getline(i,val,',');
		e.time.period_end = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.region_id = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.region_info_id  = atol(val.c_str());
		if (e.region_info_id == 0)
			throw ns_ex("ns_death_time_annotation_set::read_column_format()::Found an annotation with no region info specified");

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.position.x  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.position.y  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.size.x  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.size.y = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.annotation_source = (ns_death_time_annotation::ns_annotation_source_type)atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.annotation_time  = atol(val.c_str());

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.annotation_source_details = val;

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		
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
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
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
			throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		}

		getline(i,val,',');
		if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
		e.stationary_path_id.group_id = atol(val.c_str());

	//	if (e.stationary_path_id.group_id == 28)
	//		cerr << "MA";
		//allow old style records to be read in.  They have 5 fewer records.
		const char delim(ns_conditional_getline(i,val,",\n"));
		const bool really_old_style_record(delim==0 || delim=='\n');

		e.stationary_path_id.path_id = atol(val.c_str());

		if (!really_old_style_record){
			
			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
			e.stationary_path_id.detection_set_id = atol(val.c_str());

			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
			e.time.period_start = atol(val.c_str());
			if (kind_of_old_record){
				getline(i,val,'\n');
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
				continue;
			}
			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
			//if (val.size() == 0)
			//e.flag = ns_death_time_annotation_flag::none();
			//else e.flag.id = atol(val.c_str());
			if (!newest_old_record){
				e.time.from_interval_bound_code(atol(val.c_str()));
			}
			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
			e.flag.label_short = val;
			//e.flag.label_short = val;
			//if (e.flag.label_short.size() > 0)
			//	e.flag.id = 
	
			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
			e.animal_is_part_of_a_complete_trace = val=="1";

			getline(i,val,',');
			if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
			e.number_of_worms_at_location_marked_by_machine = atol(val.c_str());
			
			if (e.number_of_worms_at_location_marked_by_machine > 100)
				throw ns_ex("ns_death_time_annotation_set::Unlikely machine annotation for the number of worms at location:") << e.number_of_worms_at_location_marked_by_machine;
			
			const char delim(ns_conditional_getline(i,val,",\n"));
			const bool old_style_record(delim==0 || delim=='\n');

			e.number_of_worms_at_location_marked_by_hand = atol(val.c_str());
			if (!old_style_record){
				getline(i,val,',');
				e.multiworm_censoring_strategy = (ns_death_time_annotation::ns_multiworm_censoring_strategy)atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
				getline(i,val,',');
				e.missing_worm_return_strategy = (ns_death_time_annotation::ns_missing_worm_return_strategy)atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
				getline(i,val,',');
				e.animal_id_at_position = atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
				getline(i,val,',');
				e.event_observation_type = (ns_death_time_annotation::ns_event_observation_type)atol(val.c_str());
				if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
				if (!newest_old_record){
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					e.longest_gap_without_observation = atol(val.c_str());
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					getline(i,val,',');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					getline(i,val,'\n');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
				}
				else{
					getline(i,val,'\n');
					if (i.fail()) throw ns_ex("ns_death_time_annotation_set::read_column_format()::Unexpected EOF");
					e.event_observation_type = ns_death_time_annotation::ns_standard;
				}
			}
			else{
				e.multiworm_censoring_strategy = ns_death_time_annotation::ns_not_applicable;
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
			e.multiworm_censoring_strategy = ns_death_time_annotation::ns_not_applicable;
			e.missing_worm_return_strategy = ns_death_time_annotation::ns_not_specified;
			e.animal_id_at_position = 0;
		}
		
	//	if (e.stationary_path_id.group_id == 28)
	//		cerr << "MA";

		//if (old_spec_by_hand_worms)
		//	e.number_of_worms_at_location_marked_by_hand = number_of_old_spec_extra_by_hand_worms+1;

		if (!annotation_matches(type_to_load,e) && ( !exclude_fast_moving_animals || e.type != ns_fast_moving_worm_observed))
			events.pop_back();

		if (i.fail())
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
		case ns_not_applicable: return "Not Applicable";
		case ns_death_time_annotation::ns_include_as_single_worm_deaths: return "Include clusters as single deaths";
		case ns_right_censor_multiple_worm_clusters: return "Right Censor clusters";
		case ns_interval_censor_multiple_worm_clusters: return "Interval Censor clusters";
		case ns_include_multiple_worm_cluster_deaths: return "Include clusters as deaths";
		case ns_by_hand_censoring: return "By hand censoring";
		case ns_merge_multiple_worm_clusters_and_missing_and_censor: return "Treat clusters as missing";
		default: throw ns_ex("ns_death_time_annotation::multiworm_censoring_strategy_label()::Unknown censoring strategy") << (int)m;
	}
}

std::string ns_death_time_annotation::multiworm_censoring_strategy_label_short(const ns_death_time_annotation::ns_multiworm_censoring_strategy & m){
	switch (m){
		case ns_not_applicable: return "not_applicable";
		case ns_death_time_annotation::ns_include_as_single_worm_deaths: return "single_death";
		case ns_right_censor_multiple_worm_clusters: return "right_censor";
		case ns_interval_censor_multiple_worm_clusters: return "interval_censor";
		case ns_include_multiple_worm_cluster_deaths: return "multi_death";
		case ns_by_hand_censoring: return "by_hand";
		case ns_merge_multiple_worm_clusters_and_missing_and_censor: return "merge_missing";
		default: throw ns_ex("ns_death_time_annotation::multiworm_censoring_strategy_label()::Unknown censoring strategy") << (int)m;
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
		a.region_info_id = atol(xml.objects[i].tag("i").c_str()),
		a.region_id = atol(xml.objects[i].tag("r").c_str()),
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


bool operator <(const ns_stationary_path_id & a, const ns_stationary_path_id & b){
	if (a.group_id != b.group_id) return a.group_id < b.group_id;
	return a.path_id < b.path_id;
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

void ns_death_time_annotation_compiler_region::add(const ns_death_time_annotation & e, const bool create_new_location){
//	if (e.stationary_path_id.path_id == 0  && e.stationary_path_id.group_id == 0 && e.stationary_path_id.detection_set_id != 0)
//			cerr << "WHAA";
	//if (e.stationary_path_id.group_id == 28)
	//	cerr << "AH";
	//fast moving worms aren't linked to any specific location.
	if (e.type == ns_fast_moving_worm_observed ){
		fast_moving_animals.push_back(e);
		return;
	}

	//if the annotation corresponds to a specific stationary path, sort it in a location corresponding to that path.
	if (e.stationary_path_id.specified()){

		//try to find a location already specified for the current annotation's stationary path id.
		//if that fails, find a location that matches the location of the current annotation

		ns_location_list::iterator unassigned_position_match(locations.end());
		ns_location_list::iterator assigned_position_match(locations.end());
		
		for(ns_location_list::iterator p = locations.begin(); p != locations.end(); p++){
			if (p->properties.stationary_path_id == e.stationary_path_id){
					assigned_position_match = p;
					break;
			}
			if (
				(!p->properties.stationary_path_id.specified() || p->properties.stationary_path_id.detection_set_id != e.stationary_path_id.detection_set_id)
				&& p->location_matches(match_distance_squared,e.position))
				unassigned_position_match = p;
		}
	
		if (assigned_position_match != locations.end())
			assigned_position_match->add_event(e);
		else if (unassigned_position_match != locations.end())
			unassigned_position_match->add_event(e);
		else
			if (create_new_location)
				locations.push_back(ns_death_time_annotation_compiler_location(e));
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
	if (e.position == ns_vector_2i(0,0) && e.size == ns_vector_2i(0,0))
		non_location_events.add(e);
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
			r = regions.insert(ns_region_list::value_type(set.events[i].region_info_id,ns_death_time_annotation_compiler_region(match_distance))).first;
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
void ns_death_time_annotation_compiler::add(const ns_death_time_annotation & e,const ns_region_metadata & metadata){
	
	ns_region_list::iterator r(regions.find(e.region_info_id));
	if (r == regions.end())
		r = regions.insert(ns_region_list::value_type(e.region_info_id,ns_death_time_annotation_compiler_region(match_distance))).first;
		
	r->second.add(e,true);
	r->second.metadata = metadata;
}

void ns_death_time_annotation_compiler::add(const ns_death_time_annotation_set & set,const ns_region_metadata & metadata){
	if (set.events.size() == 0)
		return;

	unsigned long region_info_id(set.events[0].region_info_id);
	for (unsigned int i = 1; i < set.events.size(); i++){
		if (set.events[i].region_info_id != region_info_id)
			throw ns_ex("ns_death_time_annotation_compiler::add()::Cannot add multi-region set and metadata simultaneously");
	}	

	ns_region_list::iterator r(regions.find(region_info_id));
	if (r == regions.end())
		r = regions.insert(ns_region_list::value_type(region_info_id,ns_death_time_annotation_compiler_region(match_distance))).first;
		
	for (unsigned int i = 0; i < set.events.size(); i++){
		if (set.events[i].number_of_worms_at_location_marked_by_machine > 100)
			throw ns_ex("GARB");
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
		r = regions.insert(ns_region_list::value_type(region_id,ns_death_time_annotation_compiler_region(match_distance))).first;
	r->second.metadata = metadata;
}


class ns_death_time_event_compiler_time_aggregator{
	typedef std::map<unsigned long, ns_survival_timepoint> ns_aggregator_timepoint_list;
	ns_aggregator_timepoint_list timepoints;
	ns_region_metadata metadata;
public:
	ns_death_time_event_compiler_time_aggregator(const ns_region_metadata & metadata_):metadata(metadata_){}
	void add(const ns_death_time_annotation & e,bool use_by_hand_data){
	
		if (e.type == ns_no_movement_event) return;
		if (!ns_movement_event_is_a_state_transition_event(e.type))
			return;
		if (e.type == ns_stationary_worm_disappearance)
			return;
	//	if (e.time.period_start_was_not_observed){
	//		cerr << "Found an event whose start was not observed";
	//		return;
	//	}
		if (e.time.period_end_was_not_observed){
			cerr << "ns_death_time_event_compiler_time_aggregator()::add()::Found an event whose end was not observed.. This type of event is unusual and should be investigated.";
			return;
		}
		//we store event times at the end of the interval in which the event occurred.
		//the actual event time should be accessed by calling best_estimate_event_time_within_interval()
		//but we don't want to store that here as that would have each worm dying at its own time
		//vastly increasing the number of elements in timepoints[]
		const unsigned long event_time((e.time.period_end == 0)?metadata.time_at_which_animals_had_zero_age:(e.time.period_end));
		ns_aggregator_timepoint_list::iterator p(timepoints.find(event_time));
		if (p == timepoints.end()){
			p = timepoints.insert(ns_aggregator_timepoint_list::value_type(event_time,ns_survival_timepoint())).first;
			p->second.absolute_time = event_time;
		}

		
		ns_survival_timepoint_event * timepoint[3] = {0,0,0};
		bool unpositioned_censored_object(false);
		switch(e.type){
			case ns_fast_movement_cessation:
				timepoint[0] = &p->second.long_distance_movement_cessations;
				break;
			case ns_translation_cessation:
				timepoint[0] = &p->second.local_movement_cessations;
				break;
			case ns_movement_cessation:
				timepoint[0] = &p->second.deaths;
				break;
			case ns_moving_worm_disappearance:
				if (e.excluded == ns_death_time_annotation::ns_not_excluded)
					throw ns_ex("Found a censored worm event that wasn't excluded or censored");
				timepoint[0] = &p->second.long_distance_movement_cessations;
				timepoint[1] = &p->second.local_movement_cessations;
				timepoint[2] = &p->second.deaths;
				break;
			default: 
				throw ns_ex("ns_death_time_event_compiler_time_aggregator::add()::Unknown event type:") << (int)e.type;
		}
		for (unsigned int i = 0; i < 3; i++){
			if (timepoint[i] == 0) break;

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
			timepoint[i]->add(ev);
		}
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

ns_death_time_annotation::ns_multiworm_censoring_strategy ns_death_time_annotation::default_censoring_strategy(){ 
	return ns_death_time_annotation::ns_interval_censor_multiple_worm_clusters;
}

ns_death_time_annotation::ns_missing_worm_return_strategy ns_death_time_annotation::default_missing_return_strategy(){
	return ns_death_time_annotation::ns_censoring_minimize_missing_times;
}


void ns_out_times(ostream & o, const ns_dying_animal_description_group<const ns_death_time_annotation> & d, const ns_region_metadata & metadata){
	o << ((d.last_fast_movement_annotation!=0)?ns_to_string((d.last_fast_movement_annotation->time.period_end-metadata.time_at_which_animals_had_zero_age)/(60.0*60*24)):"");
			o << ",";
			o << ((d.last_slow_movement_annotation!=0)?ns_to_string((d.last_slow_movement_annotation->time.period_end-metadata.time_at_which_animals_had_zero_age)/(60.0*60*24)):"");
			o << ",";
			o << ((d.death_annotation!=0)?ns_to_string((d.death_annotation->time.period_end-metadata.time_at_which_animals_had_zero_age)/(60.0*60*24)):"");
			o << ",";
	
			if (d.last_fast_movement_annotation != 0 && d.last_slow_movement_annotation != 0)
				o << (d.last_slow_movement_annotation->time.period_end - d.last_fast_movement_annotation->time.period_end)/(60.0*60*24);
			o << ",";

			if (d.death_annotation != 0 &&d.last_slow_movement_annotation != 0)
				o << (d.death_annotation->time.period_end - d.last_slow_movement_annotation->time.period_end)/(60.0*60*24);
			o << ",";
			if (d.death_annotation != 0 && d.last_fast_movement_annotation != 0)
				o << (d.death_annotation->time.period_end - d.last_fast_movement_annotation->time.period_end)/(60.0*60*24);
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
	o << ",Censored,Excluded,Size of by-hand annotated cluster,";
	o << "Machine Fast Movement Cessation Age (Days),Machine Slow Movement Cessation Age (Days), Machine Death Age (Days),"
		"Machine Slow Movement Duration (Days), Machine Posture Changing Duration (Days), Machine Not Fast Moving Duration (Days),"
		"By Hand Fast Movement Cessation Age (Days),By Hand Slow Movement Cessation Age (Days),By Hand Death Age (Days),"
		"By Hand Slow Movement Duration (Days),By Hand Posture Changing Duration (Days),By Hand Not Fast Moving Duration (Days), "
		"Fast Movement Cessation Position X,Fast Movement Cessation Position Y,"
		"Slow Movement Cessation Position X,Slow Movement Cessation Position y,"
		"Death Position X,Death Position y,Posture Analysis Log-Likelihood,Worm Group ID, Worm Path ID,"
		"Fast Movement Cessation Detection Details, Slow Movement Cessation Detection Details, Death Detection Movement Details"
		"\n";
	for (ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		//std::map<unsigned long,ns_capture_sample_region_data *>::const_iterator r(region_data.regions_sorted_by_id.end());
		
		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = p->second.locations.begin(); q != p->second.locations.end(); ++q){
			ns_dying_animal_description_const d(q->generate_dying_animal_description_const(true));
			
			p->second.metadata.out_JMP_plate_identity_data(o);
			o << ",0,"
				<< ((q->properties.is_excluded() || q->properties.flag.event_should_be_excluded())?"1":"0") << ","
			  << q->properties.number_of_worms_at_location_marked_by_hand << ",";
			
			ns_out_times(o,d.machine,p->second.metadata);
			ns_out_times(o,d.by_hand,p->second.metadata);

			if (d.machine.last_fast_movement_annotation != 0)
				o << d.machine.last_fast_movement_annotation->position.x << "," << d.machine.last_fast_movement_annotation->position.y << ",";
			else o <<",,";
			if (d.machine.last_slow_movement_annotation != 0)
				o << d.machine.last_slow_movement_annotation->position.x << "," << d.machine.last_slow_movement_annotation->position.y << ",";
			else o <<",,";
			if (d.machine.death_annotation !=0)
				o << d.machine.death_annotation->position.x << "," << d.machine.death_annotation->position.y << ",";
			else o<< ",,";
			o << q->properties.loglikelihood << ","
				<< q->properties.stationary_path_id.group_id << ","
				<< q->properties.stationary_path_id.path_id << ",";

			if (d.machine.last_fast_movement_annotation!= 0)
				o << d.machine.last_fast_movement_annotation->annotation_source_details;
			o <<",";
			if (d.machine.last_slow_movement_annotation != 0)
				o<< d.machine.last_slow_movement_annotation->annotation_source_details;
			o << ",";
			if (d.machine.death_annotation !=0)
				o << d.machine.death_annotation->annotation_source_details;
			o << "\n";
		}
	
		for (unsigned int i = 0; i < p->second.non_location_events.events.size(); i++){
			if (p->second.non_location_events.events[i].is_excluded() ||
				!p->second.non_location_events.events[i].is_censored() ||
					p->second.non_location_events.events[i].multiworm_censoring_strategy != ns_death_time_annotation::default_censoring_strategy() ||
					p->second.non_location_events.events[i].missing_worm_return_strategy != ns_death_time_annotation::default_missing_return_strategy() ||
					p->second.non_location_events.events[i].excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment)
					continue;
			for (unsigned int j = 0; j < p->second.non_location_events.events[i].number_of_worms(); j++){
					
				p->second.metadata.out_JMP_plate_identity_data(o);
				o << ",1,0," //censored,excluded
					<< "1," // number of worms 
					<< ",," << (p->second.non_location_events.events[i].time.best_estimate_event_time_within_interval()-p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60*24) << ",,,," //machine death times
					<< ",," << (p->second.non_location_events.events[i].time.best_estimate_event_time_within_interval()-p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60*24) << ",,,,"; //by hand death times
	

				o << ",,";
				o << ",,";
				o << ",,"; //detah position
				o << ",,,";	//likelihood group and path
				o << ",,"; //details
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
void ns_death_time_annotation_compiler::generate_animal_event_method_comparison(std::ostream & o) const{
	
	ns_region_metadata::out_JMP_plate_identity_header(o);	
	o << ",position_x,position_y,size_x,size_y,"
		"Visual Inspection Fast Movement Cessession Time,Visual Inspection Slow Movement Cessession Time,Visual Inspection Death Time,"
		"Machine Fast Movement Cessation Time,Machine Slow Movement Cessession Time, Machine Death Time,"
		"Machine-Annotated Worm Count,Hand-Annotated Worm Count, Visual Inspection Excluded, Fully Specified, Machine Error (Days), Machine Error (Hours) \n";
	for (ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = p->second.locations.begin(); q != p->second.locations.end(); ++q){
			if (q->properties.is_excluded() || q->properties.is_censored())
				continue;
			unsigned long machine_death(p->second.metadata.time_at_which_animals_had_zero_age),
						  machine_fast_cess(p->second.metadata.time_at_which_animals_had_zero_age),
						  machine_slow_cess(p->second.metadata.time_at_which_animals_had_zero_age),
						  vis_death(p->second.metadata.time_at_which_animals_had_zero_age),
						  vis_fast_cess(p->second.metadata.time_at_which_animals_had_zero_age),
						  vis_slow_cess(p->second.metadata.time_at_which_animals_had_zero_age);
			for (unsigned int i = 0; i < q->annotations.size(); i++){
				if (q->annotations[i].time.fully_unbounded()){
					cerr << "Ignoring fully unbounded time interval.\n";
					continue;
				}
				if (q->annotations[i].time.period_end_was_not_observed){
					cerr << "Ignoring interval with no end bound.\n";
					continue;
				}
				if (q->annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine){
					switch(q->annotations[i].type){
						case ns_fast_movement_cessation:
							machine_fast_cess = q->annotations[i].time.period_end;
							break;
						case ns_translation_cessation:
							machine_slow_cess = q->annotations[i].time.period_end;
							break;
						case ns_movement_cessation:
							machine_death = q->annotations[i].time.period_end;
							break;
					}
				}
				else if (q->annotations[i].annotation_source == ns_death_time_annotation::ns_posture_image ||
						 q->annotations[i].annotation_source == ns_death_time_annotation::ns_region_image ||
						 q->annotations[i].annotation_source == ns_death_time_annotation::ns_storyboard){
					switch(q->annotations[i].type){
						case ns_fast_movement_cessation:
							vis_fast_cess = q->annotations[i].time.period_end;
							break;
						case ns_translation_cessation:
							vis_slow_cess = q->annotations[i].time.period_end;
							break;
						case ns_movement_cessation:
							vis_death = q->annotations[i].time.period_end;
							break;
					}
				}
			}
			if (vis_death == 0)
				continue;
			p->second.metadata.out_JMP_plate_identity_data(o);
			bool fully_spec((machine_death > p->second.metadata.time_at_which_animals_had_zero_age && vis_death > p->second.metadata.time_at_which_animals_had_zero_age));
			o << "," << q->properties.position.x << "," << q->properties.position.y << "," << q->properties.size.x << "," << q->properties.size.y << ","
				<< ns_out_if_not_zero((vis_fast_cess - p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60.0*24.0)) << "," 
				<< ns_out_if_not_zero((vis_slow_cess  - p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60.0*24.0)) << "," 
				<< ns_out_if_not_zero((vis_death  - p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60.0*24.0)) << "," 
				<< ns_out_if_not_zero((machine_fast_cess - p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60.0*24.0)) << "," 
				<< ns_out_if_not_zero((machine_slow_cess - p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60.0*24.0)) << "," 
				<< ns_out_if_not_zero((machine_death - p->second.metadata.time_at_which_animals_had_zero_age)/(60.0*60.0*24.0)) << ","
				<< q->properties.number_of_worms_at_location_marked_by_machine << "," 
				<< q->properties.number_of_worms_at_location_marked_by_hand << ","
				<< (q->properties.is_excluded()?"1":"0")<< ","
				<< (fully_spec?"1":"0") << ",";
			if (!fully_spec){
				o << ",\n";
			}
			else{
				o << ((double)machine_death - (double)vis_death)/(60.0*60.0*24.0) << ","
				<< ((double)machine_death - (double)vis_death)/(60.0*24.0) << "\n";
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

template<class source_t, class dest_t>
class ns_dying_animal_description_generator{


	public:
	dest_t generate(const ns_death_time_annotation & properties,
										source_t & annotations, 
										const bool warn_on_movement_problems) const{
		//find the three time spans for the current position.
		//having a worm die twice would be weird and an alert is generated if that is encountered.
		//However, it is entirely possibel that worms slow down multiple times, and so those are just ignored
		//and the latest transition recorded.
		dest_t d;
		for (unsigned int i = 0; i < annotations.size(); ++i){
			switch(annotations[i].type){
				case ns_movement_cessation: 
					if (annotations[i].annotation_source != ns_death_time_annotation::ns_lifespan_machine &&
						annotations[i].annotation_source != ns_death_time_annotation::ns_unknown)
							d.by_hand.death_annotation = & annotations[i];

					if (d.machine.death_annotation != 0){
						//if there are two machine annotations at one point, scream
						if (d.machine.death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine &&
							annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine){
							if (warn_on_movement_problems)
								cerr << "ns_dying_animal_description_generator()::generate()::Multiple deaths found at position!\n";
							unsigned long cur_death_time,
										   new_death_time;
							if(!d.machine.death_annotation->time.period_start_was_not_observed &&
							   !d.machine.death_annotation->time.period_end_was_not_observed)
								cur_death_time = d.machine.death_annotation->time.best_estimate_event_time_within_interval();
							else if (!d.machine.death_annotation->time.period_start_was_not_observed)
								cur_death_time = d.machine.death_annotation->time.period_start;
							else if (!d.machine.death_annotation->time.period_end_was_not_observed)
								cur_death_time = d.machine.death_annotation->time.period_end;
							else 
								throw ns_ex("ns_dying_animal_description_generator()::generate()::Unspecified event time found!");
							
							if(!annotations[i].time.period_start_was_not_observed &&
							   !annotations[i].time.period_end_was_not_observed)
								new_death_time = annotations[i].time.best_estimate_event_time_within_interval();
							else if (!annotations[i].time.period_start_was_not_observed)
								new_death_time = annotations[i].time.period_start;
							else if (!annotations[i].time.period_end_was_not_observed)
								new_death_time = annotations[i].time.period_end;
							else 
								throw ns_ex("ns_dying_animal_description_generator()::generate()::Unspecified event time found!");
							if (cur_death_time > new_death_time)
									d.machine.death_annotation = & annotations[i];
						}
						//if there the current annotation is by the machine, keep it
						if (d.machine.death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine)
							break;
						//if the new annotation is by the machine, use it
						if (annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine){
							d.machine.death_annotation = & annotations[i];
							break;
						}
						//use lowest worm id
						if (annotations[i].animal_id_at_position < d.machine.death_annotation->animal_id_at_position){
							d.machine.death_annotation = & annotations[i];
							break;
						}
						if (annotations[i].animal_id_at_position > d.machine.death_annotation->animal_id_at_position){
							break;
						}

						if (!d.machine.death_annotation->time.period_end_was_not_observed &&
							!annotations[i].time.period_end_was_not_observed){
							if (d.machine.death_annotation->time.period_end > annotations[i].time.period_end)
								d.machine.death_annotation = & annotations[i];
							break;
						}
						if (annotations[i].time.period_end_was_not_observed)
							break;
					}
					else {
						d.machine.death_annotation = & annotations[i];
						//WWW
					//	std::cerr << "Found a death annotation when generating ns_death_time_annotation_compiler_region::generate_survival_curve()\n";
						break;
					}
			}
		}
		for (unsigned int i = 0; i < annotations.size(); ++i){
			if ((d.machine.death_annotation != 0 && d.machine.death_annotation->annotation_source == ns_death_time_annotation::ns_lifespan_machine &&
				!(annotations[i].stationary_path_id == d.machine.death_annotation->stationary_path_id)) ||
				annotations[i].annotation_source == ns_death_time_annotation::ns_unknown)
				continue;
			typename dest_t::ns_group_type * group;
			if (annotations[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine)
				group = &d.machine;
			else if (annotations[i].annotation_source != ns_death_time_annotation::ns_unknown)
				group =& d.by_hand;
			else continue;
			switch(annotations[i].type){
				case ns_translation_cessation:
					group->last_slow_movement_annotation = & annotations[i];
					break;
				case ns_fast_movement_cessation:
						group->last_fast_movement_annotation = & annotations[i];
					break;
				case ns_worm_death_posture_relaxation_termination:
						group->death_posture_relaxation_termination = & annotations[i];
					break;
				case ns_stationary_worm_disappearance:
						group->stationary_worm_dissapearance = & annotations[i];
					break;
				case ns_slow_moving_worm_observed:
						group->slow_moving_state_annotations.push_back(& annotations[i]);
					break;
				case ns_posture_changing_worm_observed:
						group->posture_changing_state_annotations.push_back(& annotations[i]);
					break;
				case ns_stationary_worm_observed:
						group->stationary_animal_state_annotations.push_back(& annotations[i]);
					break;
				case ns_movement_censored_worm_observed:
						group->movement_censored_state_annotations.push_back(& annotations[i]);
					break;
			}
		}

		//check for duplications in the machine annotation records that 
		//would suggest something is screwed up or loaded twice
		for (unsigned int i = 0; i < annotations.size(); ++i){
			if(annotations[i].annotation_source != ns_death_time_annotation::ns_lifespan_machine ||
				annotations[i].disambiguation_type == ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster)
				continue;
			for (unsigned int j = i+1; j < annotations.size(); ++j){
				if(annotations[j].annotation_source == ns_death_time_annotation::ns_lifespan_machine &&
					annotations[i].time.period_end == annotations[j].time.period_end &&
					annotations[i].type == annotations[j].type &&
					annotations[j].disambiguation_type != ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster){
				//	multiple_event_count++;
				}
				//	throw ns_ex("Multiple similar events found at location!");
			}
		}
	
			
		//ignore animals that don't die!
		if (d.machine.death_annotation == 0)
			return d;
		if (d.machine.last_slow_movement_annotation == 0){
			/*ns_death_time_annotation a(*death_annotation);
			properties.transfer_sticky_properties(a);
			aggregator.add(a);
			ofstream o("c:\\debug.txt");
			p->second.output_summary(o);
			o.close();*/
			if (warn_on_movement_problems && d.machine.death_annotation->annotation_source==ns_death_time_annotation::ns_lifespan_machine){
				cerr << "WARNING: An animal was recorded as dead without passing through slow movement, and has been excluded from the output. \n";
				return d;
			}
		}
		if (d.machine.last_fast_movement_annotation == 0){	
			/*ns_death_time_annotation a(*death_annotation);
			properties.transfer_sticky_properties(a);
			aggregator.add(a);
			ofstream o("c:\\debug.txt");
			p->second.output_summary(o);
			o.close();*/
			if (warn_on_movement_problems && d.machine.death_annotation->annotation_source==ns_death_time_annotation::ns_lifespan_machine){
				cerr << "WARNING:  An animal was recorded as dead without passing through fast movement, and has been excluded from the output. \n";
					
			}
		}
		return d;
	}
};

ns_dying_animal_description ns_death_time_annotation_compiler_location::generate_dying_animal_description(const bool warn_on_movement_problems){
	ns_dying_animal_description_generator<ns_death_time_annotation_set,ns_dying_animal_description> d;
	return d.generate(properties,annotations,warn_on_movement_problems);
};
ns_dying_animal_description_const ns_death_time_annotation_compiler_location::generate_dying_animal_description_const(const bool warn_on_movement_problems) const{
	ns_dying_animal_description_generator<const ns_death_time_annotation_set,ns_dying_animal_description_const> d;
	return d.generate(properties,annotations,warn_on_movement_problems);
};

void ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster( 
						const unsigned long number_of_animals,
						const ns_death_time_annotation & properties,
						const ns_dying_animal_description_const & d, 
						ns_death_time_annotation_set & set,
						const ns_death_time_annotation_compiler_region::ns_death_times_to_use & death_times_to_use){

	for (unsigned int i = 0; i < ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies; i++)
		generate_correct_annotations_for_multiple_worm_cluster((ns_death_time_annotation::ns_multiworm_censoring_strategy)i,number_of_animals,properties,d,set,death_times_to_use);

}
std::string ns_death_time_annotation::event_observation_label(const ns_event_observation_type & e){
		switch (e){
		case ns_standard: return "";
		case ns_induced_multiple_worm_death: return "Induced Multiple Worm";
		case ns_observed_multiple_worm_death: return "Observed Multiple Worm";
		default: throw ns_ex("Unknown Event Observation Label");
	}
}

void ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
						const ns_death_time_annotation::ns_multiworm_censoring_strategy & censoring_strategy,
						const unsigned long number_of_animals,
						const ns_death_time_annotation & properties,
						const ns_dying_animal_description_const & d, 
						ns_death_time_annotation_set & set,const ns_death_time_annotation_compiler_region::ns_death_times_to_use & death_times_to_use){
	unsigned long start_annotation_index(set.size());
	if (d.machine.death_annotation == 0)
		throw ns_ex("Attempting to handle multiple worm cluster with no deaths!");

	ns_dying_animal_description_const::ns_group_type event_data;
	if (death_times_to_use == ns_death_time_annotation_compiler_region::ns_machine_only)
		event_data = d.machine;
	if (death_times_to_use == ns_death_time_annotation_compiler_region::ns_by_hand_only){
		if (d.by_hand.death_annotation == 0)
			return;
	}
	if (death_times_to_use == ns_death_time_annotation_compiler_region::ns_by_hand_only ||
		death_times_to_use == ns_death_time_annotation_compiler_region::ns_machine_if_not_by_hand){
		if (d.by_hand.death_annotation == 0)
			event_data.death_annotation = d.machine.death_annotation;
		else
			event_data.death_annotation = d.by_hand.death_annotation;
		
		if (d.by_hand.last_fast_movement_annotation == 0)
			event_data.last_fast_movement_annotation = d.machine.last_fast_movement_annotation;
		else
			event_data.last_fast_movement_annotation = d.by_hand.last_fast_movement_annotation;
		
		if (d.by_hand.last_slow_movement_annotation == 0)
			event_data.last_slow_movement_annotation = d.machine.last_slow_movement_annotation;
		else
			event_data.last_slow_movement_annotation = d.by_hand.last_slow_movement_annotation;
	}
	
	ns_death_time_annotation death(*event_data.death_annotation);

	//we use this as debugging info in output file
	if (event_data.last_fast_movement_annotation != 0)
		death.volatile_duration_of_time_not_fast_moving = 	
							death.time.best_estimate_event_time_for_possible_partially_unbounded_interval() - 
							event_data.last_fast_movement_annotation->time.best_estimate_event_time_for_possible_partially_unbounded_interval();

	properties.transfer_sticky_properties(death);
	switch(censoring_strategy){
		case ns_death_time_annotation::ns_include_multiple_worm_cluster_deaths:
			death.number_of_worms_at_location_marked_by_machine = 
				death.number_of_worms_at_location_marked_by_hand = number_of_animals;
			death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
			set.push_back(death);
			break;
		case ns_death_time_annotation::ns_not_applicable:
			//don't include multiworm clusters!
			//this allows the "ns_not_applicable" label to be applied only to singletons.
			break;
		case ns_death_time_annotation::ns_include_as_single_worm_deaths:
			death.number_of_worms_at_location_marked_by_machine = 
				death.number_of_worms_at_location_marked_by_hand = 1;
			death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
			set.push_back(death);
			break;
		case ns_death_time_annotation::ns_by_hand_censoring:
			return;	//ignore
		case ns_death_time_annotation::ns_interval_censor_multiple_worm_clusters:{
			if (number_of_animals < 2)
				throw ns_ex("Weird!");
			if (event_data.last_slow_movement_annotation == 0){
				cerr << "Found an unusual situation where a multiple worm cluster has no slow down from fast moving time annotationed (perhaps because it existed at the start of observation?)\n";
				death.number_of_worms_at_location_marked_by_machine = 
				death.number_of_worms_at_location_marked_by_hand = number_of_animals;
				death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
				//we don't interval censor clumps with lots of worms, because we probably can't get the death time correct.
				//so we right censor them.
				if (number_of_animals > 3){
					death.excluded = ns_death_time_annotation::ns_multiworm_censored;
					death.event_observation_type = ns_death_time_annotation::ns_standard;
				}
				set.push_back(death);
			}
			else{
				//we don't interval censor clumps with lots of worms, because we probably can't get the death time correct.
				//so we right censor them.
				if (number_of_animals > 3){
					death.time = event_data.last_slow_movement_annotation->time;
					death.excluded = ns_death_time_annotation::ns_multiworm_censored;
					death.event_observation_type = ns_death_time_annotation::ns_standard;
					death.number_of_worms_at_location_marked_by_machine = 
					death.number_of_worms_at_location_marked_by_hand = number_of_animals;
					set.push_back(death);
				}
				else{
					//include last, observed death
					death.number_of_worms_at_location_marked_by_machine = 
					death.number_of_worms_at_location_marked_by_hand = 1;
					death.event_observation_type = ns_death_time_annotation::ns_observed_multiple_worm_death;
					set.push_back(death);
						
					//death occurs during the inteval between the animals slowing down to changing posture and 
					//dying
					////if(death.time.period_start>2000000000)
					//	cerr << "WHA";
					death.time.period_start = event_data.last_slow_movement_annotation->time.period_start;
					death.time.period_start_was_not_observed = event_data.last_slow_movement_annotation->time.period_start_was_not_observed;
					death.number_of_worms_at_location_marked_by_machine = 
						death.number_of_worms_at_location_marked_by_hand = number_of_animals-1;
					death.event_observation_type = ns_death_time_annotation::ns_induced_multiple_worm_death;
					set.push_back(death);
				}
			}
		}
		break;
			
		case ns_death_time_annotation::ns_right_censor_multiple_worm_clusters:{
			if (event_data.last_slow_movement_annotation == 0){
				cerr << "Found an unusual situation where a multiple worm cluster has no slow down from fast moving time annotationed (perhaps because it existed at the start of observation?)\n";
				death.type = ns_moving_worm_disappearance;
				death.number_of_worms_at_location_marked_by_machine = 
				death.number_of_worms_at_location_marked_by_hand = number_of_animals;
				death.excluded = ns_death_time_annotation::ns_multiworm_censored;
				set.push_back(death);
			}
			else{
				death = *event_data.last_slow_movement_annotation;
				properties.transfer_sticky_properties(death);
				death.type = ns_moving_worm_disappearance;
				death.number_of_worms_at_location_marked_by_machine = 
				death.number_of_worms_at_location_marked_by_hand = number_of_animals;
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
	for (unsigned int i = start_annotation_index; i < set.size(); i++)
		set[i].multiworm_censoring_strategy = censoring_strategy;
}

void ns_death_time_annotation_compiler_region::generate_survival_curve(ns_survival_data & curve, const ns_death_times_to_use & death_times_to_use,const bool use_by_hand_worm_cluster_annotations, const bool warn_on_movement_problems) const{
		unsigned long multiple_event_count(0);
		//Note that the censoring code will generate censoring events for multi-worm clusters.
		//The machine also records death events
		//Both censoring events and death events will be propigated so it is very important
		//that downstram clients choose between one of the two strategies.

		ns_death_time_event_compiler_time_aggregator aggregator(metadata);
		for (ns_death_time_annotation_compiler_region::ns_location_list::const_iterator q = locations.begin(); q != locations.end(); ++q){
			ns_dying_animal_description_const death(q->generate_dying_animal_description_const(warn_on_movement_problems));
			if (death.machine.death_annotation != 0 && //only generate fancy censoring strategies for actual deaths
				q->properties.number_of_worms() > 1 && //and only for multiple-worm clusters
				death.machine.death_annotation->annotation_source == //and only for multiworm clusters that the machine recognized 
				ns_death_time_annotation::ns_lifespan_machine		 //not by hand annotations that were not matched up to any machine annotation.
				){
				ns_death_times_to_use death_times_to_use_2(death_times_to_use);
				
			

				//if we need to output both machine and by hand death times, we need to generate censoring data twice
				if (death_times_to_use == ns_machine_and_by_hand){
						ns_death_time_annotation_set set;
						ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
						q->properties.number_of_worms(),q->properties,death,set,ns_machine_only);
					for (unsigned int i = 0; i < set.size(); i++){
						set[i].volatile_matches_machine_detected_death = true;
						aggregator.add(set[i],use_by_hand_worm_cluster_annotations);
					}
					death_times_to_use_2 = ns_by_hand_only;
				}
				
				ns_death_time_annotation_set set;
				ns_multiple_worm_cluster_death_annotation_handler::generate_correct_annotations_for_multiple_worm_cluster(
					q->properties.number_of_worms(),q->properties,death,set,death_times_to_use_2);
				for (unsigned int i = 0; i < set.size(); i++){
					set[i].volatile_matches_machine_detected_death = true;
					aggregator.add(set[i],use_by_hand_worm_cluster_annotations);
				}
			}
			else{
			
				//by_hand == 0: add machine annotations
				//by_hand == 1: add by_hand annotations
				for (int by_hand = 0; by_hand < 2; by_hand++){

					//add fast movement cessation, slow movement cessation, and death events
					const ns_death_time_annotation *a[3] = {0,0,0};

					bool matches_machine_detected_death(death.machine.death_annotation != 0);

					//add a machine annotations if requested
					if (!by_hand && (death_times_to_use == ns_machine_only|| death_times_to_use == ns_machine_and_by_hand)){
						a[0] = death.machine.death_annotation;
						a[1] = death.machine.last_slow_movement_annotation;
						a[2] = death.machine.last_fast_movement_annotation;
					}
					//add a by hand annotation, if requested
					else if (by_hand && (death_times_to_use == ns_by_hand_only || death_times_to_use == ns_machine_and_by_hand)){
						a[0] = death.by_hand.death_annotation;
						a[1] = death.by_hand.last_slow_movement_annotation;
						a[2] = death.by_hand.last_fast_movement_annotation;
					}
					//add a by hand annotation, and if one doesn't exist, add a machine annotation
					else if (!by_hand && (death_times_to_use == ns_machine_if_not_by_hand)){
						if (death.by_hand.death_annotation != 0){
							a[0] = death.by_hand.death_annotation;
							a[1] = death.by_hand.last_slow_movement_annotation;
							a[2] = death.by_hand.last_fast_movement_annotation;
						}
						else{
							a[0] = death.machine.death_annotation;
							a[1] = death.machine.last_slow_movement_annotation;
							a[2] = death.machine.last_fast_movement_annotation;
						}
					}
					else continue;

					for (unsigned int i = 0; i < 3; i++){
						if (a[i]!=0){
							ns_death_time_annotation b(*a[i]);
							q->properties.transfer_sticky_properties(b);
						
							//we use this as debugging info in output file
							if (a[2] != 0 && a[0] != 0)
								b.volatile_duration_of_time_not_fast_moving = 
								a[0]->time.best_estimate_event_time_for_possible_partially_unbounded_interval() - 
								a[2]->time.best_estimate_event_time_for_possible_partially_unbounded_interval();

							//regions can be re-analyzed. In this case,
							//some locations can dissapear (as they no longer exist in the new analysis)
							//hand anotations remain for these now non-existant locations.
							//we do not want to discard these old annotations completely in case they become useful later
							//but we do want to be able to identify them.
							//therefore, we need to mark locations currently identified by the machine as such.
							b.volatile_matches_machine_detected_death = matches_machine_detected_death;

							aggregator.add(b,use_by_hand_worm_cluster_annotations);

							
						}
					}
				}
			}
		}
		for (unsigned int i = 0; i < non_location_events.size(); i++){
			//www
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
void ns_death_time_annotation_compiler::generate_survival_curve_set(ns_lifespan_experiment_set & survival_curves, const ns_death_time_annotation_compiler_region::ns_death_times_to_use & death_times_to_use,const bool use_by_hand_worm_cluster_annotations,const bool warn_on_movement_problems) const{
	survival_curves.curves.reserve(regions.size());
	for(ns_region_list::const_iterator p = regions.begin(); p != regions.end(); ++p){
		const std::vector<ns_survival_data>::size_type s(survival_curves.curves.size());
		survival_curves.curves.resize(s+1);
		p->second.generate_survival_curve(survival_curves.curves[s],death_times_to_use,use_by_hand_worm_cluster_annotations,warn_on_movement_problems);
		if (survival_curves.curves[s].timepoints.size() == 0)//don't include empty curves
			survival_curves.curves.resize(s);
	}
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
void ns_death_time_annotation_flag::get_flags_from_db(ns_sql & sql){
	cached_flags_by_short_label.clear();
		
	sql << "SELECT id, label_short, label, exclude, next_flag_name_in_order, hidden, color FROM annotation_flags";
	ns_sql_result res;
	sql.get_rows(res);
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
			sql <<  "INSERT INTO annotation_flags SET label_short='" <<v[i].label_short 
				<< "',label='"<<v[i].cached_label << "',exclude=" << (v[i].cached_excluded?"1":"0") << ",next_flag_name_in_order='" << v[i].next_flag_name_in_order << "'";
			sql.send_query();
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
		
};


char * ns_death_time_annotation_flag::first_default_flag_short_label(){
	return "MULTI_ERR";
}
ns_death_time_annotation_flag ns_death_time_annotation_flag::extra_worm_from_multiworm_disambiguation(){
	return ns_death_time_annotation_flag(first_default_flag_short_label(),
	"Extra worm from multiple-worm disambiguation",true,"3D_CLOUD_ERR","FFFFCC");
}
void ns_death_time_annotation_flag::generate_default_flags(std::vector<ns_death_time_annotation_flag> & flags){
	flags.resize(0);
	flags.push_back(ns_death_time_annotation_flag::none());
	flags.push_back(extra_worm_from_multiworm_disambiguation());
	flags.push_back(ns_death_time_annotation_flag("3D_CLOUD_ERR",
		"Point Cloud Analaysis Error",true,"REG_ERR","FFFF80"));
	flags.push_back(ns_death_time_annotation_flag("REG_ERR",
		"Movement Registration Error",true,"2ND_WORM_ERR","FF8080"));
	flags.push_back(ns_death_time_annotation_flag("2ND_WORM_ERR",
		"Additional Worm Confuses Analysis",true,"","80FF80"));
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
	if (cached_flags_by_short_label.empty())
		throw ns_ex("Death time annotation flags have not been loaded from the database!");
	ns_flag_cache_by_short_label::const_iterator p(cached_flags_by_short_label.find(this->label_short));
	if (p == cached_flags_by_short_label.end())
		throw ns_ex("Could not load flag information from db for flag ") << label_short;
	label_is_cached = true;;
	cached_label = p->second.cached_label;
	cached_excluded = p->second.cached_excluded;
	cached_hidden = p->second.cached_hidden;
	cached_color = p->second.cached_color;
}
ns_color_8 ns_death_time_annotation_flag::flag_color() const{
	if (!label_is_cached) 
		get_cached_info();
	return cached_color;
}
void ns_death_time_annotation_flag::step_event(){
		
		if (cached_flags_by_short_label.empty())
			throw ns_ex("ns_death_time_annotation_flag::step_event()::cached flags are not loaded.");
		ns_flag_cache_by_short_label::const_iterator p(cached_flags_by_short_label.find(label_short));
		if (p == cached_flags_by_short_label.end())
			throw ns_ex("ns_death_time_annotation_flag::step_event()::could not find current flag in cache");
		ns_flag_cache_by_short_label::const_iterator q(cached_flags_by_short_label.find(p->second.next_flag_name_in_order));
		if (q == cached_flags_by_short_label.end())
			*this = none();
		*this = q->second;
		if (this->cached_hidden)
			step_event();
	}
//ns_death_time_annotation_flag::ns_flag_cache cached_flags_by_id;
ns_death_time_annotation_flag::ns_flag_cache_by_short_label ns_death_time_annotation_flag::cached_flags_by_short_label;