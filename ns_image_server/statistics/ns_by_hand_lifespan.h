#ifndef NS_BY_HAND_LIFESPAN
#define NS_BY_HAND_LIFESPAN
#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns_ex.h"
#include "ns_survival_curve.h"
#include "ns_death_time_annotation.h"
#include <set>

struct ns_by_hand_lifespan_plate_event{
	std::string end_date;
	ns_death_time_annotation_time_interval time;

	unsigned int deaths, censored;
};


class ns_by_hand_lifespan_plate_specification{
public:

	typedef std::vector<std::string> ns_metadata_specification;
	typedef std::map<std::string,std::string> ns_column_metadata;
	typedef std::vector<std::vector<std::string> > ns_grid_type;

	ns_by_hand_lifespan_plate_specification(){}

	ns_region_metadata standard_info;
	ns_column_metadata extra_metadata;
	ns_by_hand_lifespan_plate_specification(const ns_column_metadata & metadata,const ns_grid_type & grid, const long time_column, const long data_column, const long censored_column,const long data_start_row){

		//get standard metadata (if specified)
		standard_info.load_from_fields(metadata,extra_metadata);

		if (standard_info.analysis_type.size() == 0)
			standard_info.technique = "By Hand";
		if (standard_info.device.size() == 0)
			standard_info.device = "By Hand";
		if (standard_info.time_at_which_animals_had_zero_age == 0)
			throw ns_ex("Could not specification of animal zero age");

		ns_by_hand_lifespan_plate_event ev;
		for (unsigned int i = data_start_row; i < grid.size(); i++){
			ev.deaths = atol(grid[i][data_column].c_str());
			if (censored_column!=-1)
				ev.censored = atol(grid[i][censored_column].c_str());
			else ev.censored = 0;
		//	if (ev.deaths == 0 && ev.censored == 0) continue;
			//we include the fact that the population was observed!  This is important 
			//as we count deaths as occurring halfway between the observation interval
			ev.end_date = grid[i][time_column];
			if (ev.end_date == "TOTAL") continue;
			try{
				ev.time.period_start= ev.time.period_end = ns_time_from_format_string(ev.end_date);
			}
			catch(ns_ex & ex){
				throw ns_ex("Encountered an error on line ") << i << ", column " << time_column << ": " << ex.text();
			}
			std::cerr << "Converted " << ev.end_date << " to " << ns_format_time_string_for_human(ev.time.period_start) << "(" << ev.deaths << " Deaths)\n";
			events.push_back(ev);
		}
		//generate_annotations();
	}
	std::vector<ns_by_hand_lifespan_plate_event> events;
	//std::vector<ns_survival_timepoint_event_count> annotations;

	static void out_jmp_header(const ns_lifespan_experiment_set::ns_time_handing_behavior & b,const ns_lifespan_experiment_set::ns_control_group_behavior & control_group_behavior,std::ostream & o, const ns_metadata_specification & spec){
		ns_lifespan_experiment_set::out_simple_JMP_header(b,control_group_behavior,o,"?","");
		for (unsigned long i = 0; i < spec.size(); i++)
			o << "," << spec[i];
		o << "\n";
	}

/*	void generate_annotations(){
		annotations.resize(0);
		annotations.reserve(events.size());
		for (unsigned int i = 0; i < events.size(); i++){
			annotations.resize(annotations.size()+1);
			ns_survival_timepoint_event_count & p(*annotations.rbegin());
			{
				ns_death_time_annotation a;
				a.type = ns_movement_cessation;
				a.time = events[i].time;
				a.number_of_worms_at_location_marked_by_hand = 1;
				a.number_of_worms_at_location_marked_by_machine = 0;
				//add an annotation for each event.
				p.events.resize(events[i].deaths,a);
				p.number_of_worms_in_by_hand_worm_annotation = 1;
				p.number_of_worms_in_machine_worm_cluster = 0;
				p.from_multiple_worm_disambiguation = false;
			}

			if (events[i].censored > 0){
				ns_death_time_annotation a;
				a.time = events[i].time;
				a.excluded = ns_death_time_annotation::ns_censored;
				p.events.resize(p.events.size()+events[i].censored,a);
			}
		}
	}*/
	void out_jmp(const ns_lifespan_experiment_set::ns_time_handing_behavior & b,std::ostream & o,const ns_metadata_specification & metadata_spec) const{
		return ;
		//	if (annotations.size() == 0)
	//		throw ns_ex("No Annotations Provided");
/*
		for (unsigned int i = 0; i < annotations.size(); i++){
			ns_metadata_worm_properties p;
			p.event_period_end_time = 0;
			p.event_type = ns_metadata_worm_properties::ns_death;
			p.events = &annotations[i];
			ns_lifespan_experiment_set::out_simple_JMP_event_data(b,ns_death_time_annotation::default_censoring_strategy(),
														  ns_death_time_annotation::default_missing_return_strategy(),
														  o,0,standard_info,p,1,"");
			for (unsigned int j = 0; j < metadata_spec.size(); j++){
				ns_column_metadata::const_iterator p = extra_metadata.find(metadata_spec[j]);
				if (p!=extra_metadata.end()) 
					o << "," << p->second;
			}
			o << "\n";
		}*/
	}
private:

	double age_at_death(const ns_by_hand_lifespan_plate_event & ev) const{
		//cerr << ev.date << "(" << ns_date_from_string(ev.date) <<") - ";
	//	cerr << start_date << "(" << ns_date_from_string(start_date) << ") = " << (ns_date_from_string(ev.date) - ns_date_from_string(start_date)) << "\n";
		return (ns_time_from_format_string(ev.end_date) - standard_info.time_at_which_animals_had_zero_age)/(60.0*60*24);
	}

};

class ns_by_hand_lifespan_experiment_specification{
	
	public:
	void load(std::istream & in){
		grid.resize(0);
		grid.resize(1,std::vector<std::string>(1));

		//Load data into a matrix
		unsigned long i(0),j(0);
		while (true){
			char a;
			in.read(&a,1);
			if (in.fail())
				break;
			switch(a){
				case ',': i++; grid[j].resize(i+1);break;
				case '\n': j++; grid.resize(j+1,std::vector<std::string>(1)); i = 0; break;
				default: 
					grid[j][i].push_back(a);
			}
		}

		if (grid.size() < 3){
			ns_ex ex("Could not parse data file: The file appears to contain a ");
				ex << grid.size();
				if (grid.size() == 0)
					ex << "x0";
				else ex << "x" << grid[0].size();
				ex << " grid.\n";
				throw ex;
		}
		extract_plates_from_grid();
		
	}

	void convert_to_lifespan_experiment_set(ns_lifespan_experiment_set & set){
		set.curves.resize(0);
		set.curves.resize(plates.size());
		std::set<unsigned long> observation_times;
		for (unsigned int i = 0; i < plates.size(); i++){
			set.curves[i].metadata = plates[i].standard_info;
			set.curves[i].timepoints.resize(plates[i].events.size());
			observation_times.clear();
			//build a list of all observation times
			for (unsigned int j = 0; j < plates[i].events.size(); j++){
				set.curves[i].timepoints[j].absolute_time =  plates[i].events[j].time.period_end;
				observation_times.insert(plates[i].events[j].time.period_end);
			}			
			for (unsigned int j = 0; j < plates[i].events.size(); j++){
				//find the start of the observation interval
				if (true){
					std::set<unsigned long>::const_iterator p(observation_times.find(plates[i].events[j].time.period_end));
					if (p == observation_times.begin())
						plates[i].events[j].time.period_start_was_not_observed = true;
					else{
						p--;
						plates[i].events[j].time.period_start = *p;
					}
				}
				ns_survival_timepoint_event_count c;
				c.number_of_worms_in_by_hand_worm_annotation = 1;
				c.number_of_worms_in_machine_worm_cluster = 1;
				c.from_multiple_worm_disambiguation = false;
				ns_death_time_annotation d(ns_movement_cessation,
							0,0,
							plates[i].events[j].time,
							ns_vector_2i(0,0),
							ns_vector_2i(0,0),
							ns_death_time_annotation::ns_not_excluded,
							ns_death_time_annotation_event_count(1,0),
							ns_current_time(),
							ns_death_time_annotation::ns_lifespan_machine,
							ns_death_time_annotation::ns_single_worm,
							ns_stationary_path_id(0,0,0),true,false);
				if (plates[i].events[j].deaths > 0){
					for (unsigned int k = 0; k < plates[i].events[j].deaths; k++){
						c.events.push_back(d);
					}
					set.curves[i].timepoints[j].deaths.add(c);
				}
	//			plates[i].annotations.push_back(c);

				c.events.resize(0);
				c.properties.excluded = d.excluded = ns_death_time_annotation::ns_censored;
				d.multiworm_censoring_strategy = ns_death_time_annotation::ns_by_hand_censoring;
				d.missing_worm_return_strategy = ns_death_time_annotation::default_missing_return_strategy();
				d.type = ns_moving_worm_disappearance;
				
				if (plates[i].events[j].censored > 0){
					for (unsigned int k = 0; k < plates[i].events[j].censored; k++){
						c.events.push_back(d);
					}
					set.curves[i].timepoints[j].deaths.add(c);
				}
	//			plates[i].annotations.push_back(c);
				

				//c.number_of_clusters_identified_by_hand = 
			//		c.number_of_clusters_identified_by_machine  = plates[i].events[j].censored;
				
				//for (unsigned int k = 0; k < plates[i].annotations[j].events;
		//		for (unsigned int k = 0; k < set.curves[i].timepoints[j].deaths.events.size(); k++)
		//			set.curves[i].timepoints[j].deaths.add(plates[i].annotations[j]);
			}
		}
	}
	void output_jmp_file_with_extra_metadata(const ns_lifespan_experiment_set::ns_time_handing_behavior & b,const ns_lifespan_experiment_set::ns_control_group_behavior & control_group_behavior,std::ostream & output){

		ns_by_hand_lifespan_plate_specification::ns_metadata_specification spec(row_names);
		ns_remove_standard_fields_from_metadata_spec(spec);
		ns_by_hand_lifespan_plate_specification::out_jmp_header(b,control_group_behavior,output,spec);
		for (unsigned int i = 0; i < plates.size(); i++)
			plates[i].out_jmp(b,output,spec);
	}

private:
	


	std::vector<std::vector<std::string> > grid;
	std::vector<ns_by_hand_lifespan_plate_specification> plates;
	ns_by_hand_lifespan_plate_specification::ns_metadata_specification row_names;

	
	void extract_plates_from_grid(){
		//crop empty columns from right of the first row to find the propper number of columns
		long  last_good_column;
		for (last_good_column = grid[0].size()-1; last_good_column >= 0; last_good_column--)
			if (grid[0][last_good_column].size() != 0)
				break;
		grid[0].resize(last_good_column+1);
		unsigned columns = grid[0].size();

		//crop empty columns down from right of the first row
		for (unsigned int i = 0; i < grid.size(); i++){
			//if (grid[i].size() < columns)
			//	grid[i].resize(columns);
			/*for (last_good_column = grid[i].size()-1; last_good_column >= 0; last_good_column--)
				if (grid[i][last_good_column].size() != 0){
					//last_good_column--;
					break;
				}*/
			grid[i].resize(last_good_column+1);
		}
		
			
		long last_good_row;

		for (last_good_row = grid.size()-1; last_good_row >= 0; last_good_row--){
			if (ns_to_lower(grid[last_good_row][0]).find("total") != grid[last_good_row][0].npos)
				continue;
			if (grid[last_good_row].size() == 0)
				continue;

			bool found_values(false);
			for (unsigned int i = 0; i < grid[last_good_row].size(); i++){
				if (grid[last_good_row][i].size() > 0){
					found_values = true;
					break;
				}
			}
			if (found_values){
				if (grid[last_good_row][0].size() == 0)
					throw ns_ex("Found data in row ") << last_good_row << " that had no date specified.";
				else
					break;	//we've found the last row; stop searching
			}
		}
		long first_data_row(-1);
		long event_type_row(-1);
		for (unsigned int i = 0; i < grid.size(); i++){
			if (ns_to_lower(grid[i][0])=="event type"){
				event_type_row=i;
				first_data_row=i+1;
				break;
			}
			row_names.push_back(ns_to_lower(grid[i][0]));
		}
		if (first_data_row == -1)
				throw ns_ex("Could not find row \"Event Type\"");

		if (grid.size() < 2)
			throw ns_ex("Could not deduce data format.  The row \"Event Type\" must be specified.");

		grid.resize(last_good_row+1);
		for (unsigned i = 0; i < grid.size(); i++){
			if (grid[i].size() > columns)
				throw ns_ex("Row has wrong length: grid[") << i << "].size() == " << grid[i].size() << " != " << columns;
			else 
				grid[i].resize(columns);
		}

		plates.resize(0);
		plates.reserve(grid[0].size()-1);
		try{
			for (unsigned int i = 1; i < columns; ){
				//get metadata for lifespan specified in each column
				ns_by_hand_lifespan_plate_specification::ns_column_metadata metadata;
				for (int j = 0; j < event_type_row; j++)
					metadata[row_names[j]]=grid[j][i];

				//check to see if data and  censoring data is specified
				long data_column,
					  censored_column(-1);
				if (ns_to_lower(grid[event_type_row][i]) != "deaths"){
					if (ns_to_lower(grid[event_type_row][i]) != "censored"){
						throw ns_ex("If a \"censored\" column is specified for a strain, it must be on the right of the \"deaths\" column.");
					}
					else 
						throw ns_ex("Unknown Event type: \"") << grid[event_type_row][i] << "\"";
				}
				data_column=i;
				if (i+1<grid[event_type_row].size() && ns_to_lower(grid[event_type_row][i+1]) == "censored")
					censored_column = i+1;

				//parse the lifespan data
				plates.resize(plates.size()+1,ns_by_hand_lifespan_plate_specification(metadata,grid,0,data_column,censored_column,first_data_row));
				std::cerr << "\n";
				if (censored_column != -1)
					i+=2;
				else ++i;
			}
		}
		catch(ns_ex & ex){
			throw ex; 
		}
	}
		
	static void ns_remove_standard_fields_from_metadata_spec(ns_by_hand_lifespan_plate_specification::ns_column_metadata & spec){
		spec.erase("strain_condition_1");
		spec.erase("strain_condition_2");
		spec.erase("device");
		spec.erase("region");
		spec.erase("sample");
		spec.erase("technique");
		spec.erase("details");
		spec.erase("date at age zero");
		spec.erase("strain");
		spec.erase("experiment");
	}
	static void inline ns_remove_if_possible(ns_by_hand_lifespan_plate_specification::ns_metadata_specification & spec,const std::string & key){
		for (ns_by_hand_lifespan_plate_specification::ns_metadata_specification::iterator p = spec.begin(); p != spec.end(); ++p){
			if (*p == key){
				spec.erase(p);
				return;
			}
		}
	}
	static void ns_remove_standard_fields_from_metadata_spec(ns_by_hand_lifespan_plate_specification::ns_metadata_specification & spec){
		ns_remove_if_possible(spec,"details");
		ns_remove_if_possible(spec,"strain_condition_1");
		ns_remove_if_possible(spec,"strain_condition_2");
		ns_remove_if_possible(spec,"device");
		ns_remove_if_possible(spec,"region");
		ns_remove_if_possible(spec,"sample");
		ns_remove_if_possible(spec,"technique");
		ns_remove_if_possible(spec,"date at age zero");
		ns_remove_if_possible(spec,"strain");
		ns_remove_if_possible(spec,"experiment");
	}
};

#endif
