#ifndef NS_JMP_FILE
#define NS_JMP_FILE

#include "ns_ex.h"
#include <fstream>
#include <string>
#include <iostream>
#include <algorithm>
#include "ns_survival_curve.h"

class ns_jmp_file{
public:
	void load(std::istream & in){
		load_line(in,column_names);
		if (column_names.size() == 0)
			throw ns_ex("Could not load any column names from file\n");
		rows.resize(0);
		rows.resize(1);
		for (unsigned int i = 0; load_line(in,rows[i]); ++i){
			if (rows[i].size() != column_names.size())
				throw ns_ex("Found an incorrect number of columns on line ") << i;
			rows.resize(rows.size()+1);
		}
		rows.resize(rows.size()-1);
	}

	void generate_survival_curve_set(ns_lifespan_experiment_set & set){
		ns_region_metadata line_data;
		std::vector<std::string *> column_bindings(column_names.size(),0);
		long time_at_zero_column(-1),
			 frequency_column(-1),
			 censor_column(-1),
			 date_column(-1);
		//figure out which columns correspond to which metadata elements
		for (unsigned int i = 0; i < column_names.size(); i++){
			const std::string f(ns_to_lower(column_names[i]));
			if (line_data.is_age_zero_field(f)){
				time_at_zero_column = i;
				continue;
			}
			column_bindings[i] = line_data.get_field_by_name(f);
			if (column_bindings[i] == 0){
				if (f.find("freq")!=std::string::npos)
					frequency_column = i;
				else if (f.find("censor")!=std::string::npos)
					censor_column = i;
				else if (f.find("date")!=std::string::npos &&
						f.find("zero ")==std::string::npos)
						date_column = i;
			}
		}

		if (frequency_column == -1)
			throw ns_ex("Could not identify the frequency column");
		if (time_at_zero_column == -1)
			throw ns_ex("Could not identify the Age At Time Zero column");
		if (censor_column== -1)
			throw ns_ex("Could not identify the Censor Column");
		if (date_column == -1)
			throw ns_ex("Could not identify the date column");

		for (long i = 0; i < rows.size(); i++){
			
			if (rows[i][date_column].empty() && rows[i][frequency_column].empty())
				continue;

			//load in the metadata for the current row
			line_data.clear();
			for (long j = 0; j < rows[i].size(); j++){
				if (j == time_at_zero_column){
					line_data.time_at_which_animals_had_zero_age = ns_time_from_format_string(rows[i][j]);
					continue;
				}
				if (column_bindings[j] == 0) continue;
				*column_bindings[j] = rows[i][j];
			}
		
			//find if any elements from the current set already exist
			ns_survival_data * current_curve(0);
			for (unsigned int j = 0; j < set.size(); j++){
				if (set.curve(j).metadata.experiment_name == line_data.experiment_name &&
					set.curve(j).metadata.sample_name == line_data.sample_name &&
					set.curve(j).metadata.region_name == line_data.region_name &&
					set.curve(j).metadata.technique == line_data.technique &&
					set.curve(j).metadata.time_at_which_animals_had_zero_age == line_data.time_at_which_animals_had_zero_age &&
					set.curve(j).metadata.strain == line_data.strain){
					current_curve = &set.curve(j);
					break;
				}
			}
			if (current_curve == 0){
				set.resize(set.size()+1);
				current_curve = &(set.curve(set.size()-1));
				current_curve->metadata = line_data;
			}
			unsigned long current_time = ns_time_from_format_string(rows[i][date_column]);

			//if we have no timepoints, or the current time doesn't match the old one
			if (current_curve->timepoints.empty() || 
				current_curve->timepoints.rbegin()->absolute_time != current_time)
				current_curve->timepoints.resize(current_curve->timepoints.size()+1);

			ns_survival_timepoint & t(*current_curve->timepoints.rbegin());
			t.absolute_time = ns_time_from_format_string(rows[i][date_column]);
			ns_survival_timepoint_event_count c;
			ns_death_time_annotation a;
			a.type = ns_movement_cessation;
			a.time.period_end = a.time.period_start = t.absolute_time;
			unsigned long number_of_events(atol(rows[i][frequency_column].c_str()));
			c.events.resize(number_of_events,a);
			c.number_of_worms_in_by_hand_worm_annotation = 1;
			c.number_of_worms_in_machine_worm_cluster = 0;
			c.properties.excluded = ns_death_time_annotation::ns_not_excluded;
			if (rows[i][censor_column].empty() || rows[i][censor_column] == "0") {
				t.movement_based_deaths.add(c);
				t.best_guess_deaths.add(c);
			}
			else {
				c.properties.excluded = ns_death_time_annotation::ns_censored;
				t.movement_based_deaths.add(c);
				t.best_guess_deaths.add(c);
			}
		}

		//go through all curves, sort the timepoints and remove any duplicate time points.
		for (unsigned int i = 0; i < set.size(); i++){
			std::sort(set.curve(i).timepoints.begin(),set.curve(i).timepoints.end());
			for (std::vector<ns_survival_timepoint>::iterator p = set.curve(i).timepoints.begin(); p != set.curve(i).timepoints.end();){

				std::vector<ns_survival_timepoint>::iterator q(p);
				q++;
				if (q==set.curve(i).timepoints.end())
					break;

				if (q->absolute_time == p->absolute_time){
					std::cerr << "Multiple identical timepoints found!\n";
					p->add(*q);
					set.curve(i).timepoints.erase(q);
				}
				else ++p;
			}
		}
		set.generate_survival_statistics();
	}

private:
		
	std::vector<std::string> column_names;
	std::vector<std::vector<std::string> > rows;
	static bool load_line(std::istream & in,std::vector<std::string> & fields){
		char a(0);
		bool data_found(false);
		//load in column name
		fields.resize(0);
		fields.resize(1);
		while(true){
			a = in.get();
			if (in.fail())
				return false;
			if (a == '\n') break;
			if (!data_found && !isspace(a)) data_found=true;
			if (a==',')
				fields.resize(fields.size()+1);
			else (*fields.rbegin())+=a;
		}
		if (!data_found)
			fields.resize(0);
		return true;
	}
};
#endif
