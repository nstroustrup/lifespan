#include "ns_capture_schedule.h"
#include "ns_xml.h"
#include <fstream>
#ifndef NS_ONLY_IMAGE_ACQUISITION
#include "ns_image_processing_pipeline.h"
#else
#endif
#include "ns_processing_job_scheduler.h"
using namespace std;
#include <set>



std::string ns_experiment_capture_specification::submit_schedule_to_db(std::vector<std::string> & warnings,ns_sql & sql,bool actually_write, const ns_handle_existing_experiment & handle_existing){
	string debug;
	if (!device_schedule_produced)
		throw ns_ex("ns_experiment_capture_specification::submit_schedule_to_db()::The device schedule has not yet been compiled");
	if (name.length() > 40)
			throw ns_ex("To avoid lengthy filenames, experiment names must contain 40 characters or less.");
	ns_sql_result res;
	//check that all devices requested exist
	for (unsigned long i = 0; i < capture_schedules.size(); i++){
		ns_device_schedule_list & device_schedules(capture_schedules[i].device_schedules);
		for (ns_device_schedule_list::iterator p = device_schedules.begin(); p != device_schedules.end(); p++){
			sql << "SELECT name FROM devices WHERE name = '" << sql.escape_string(p->second.device_name) << "'";
			sql.get_rows(res);
			if (res.size() == 0)
				throw ns_ex("ns_experiment_capture_specification::submit_schedule_to_db()::Could not find device ") << p->second.device_name << " attached to cluster";
			for (ns_device_capture_schedule::ns_sample_group_list::iterator q = p->second.sample_groups.begin(); q!= p->second.sample_groups.end(); ++q){
				if (q->second.samples.size() != 4 && q->second.samples.size() != 6){
					string warning;
					warning+="Device ";
					warning+=p->second.device_name + " has " + ns_to_string(q->second.samples.size()) + " samples scheduled on a single device";
					warnings.push_back(warning);
					debug += "WARNING: ";
					debug += warning + ".\n\n";
				}
				for (unsigned int k = 0; k < q->second.samples.size(); k++){
					if (q->second.samples[k]->width < .75 || q->second.samples[k]->height < .75 ||
						q->second.samples[k]->width > 2.5 || q->second.samples[k]->height > 10){
						string warning;
						warning+="Sample ";
						warning+=q->second.samples[i]->sample_name + " has unusual dimensions: " + ns_to_string(q->second.samples[i]->width) + "x" + ns_to_string(q->second.samples[i]->height);
						warnings.push_back(warning);
						debug += "WARNING: ";
						debug += warning + ".\n\n";
					}
				}
				if (q->second.schedule->device_capture_period < 10*60 || q->second.schedule->device_capture_period > 20*60){
						string warning;
						warning+="The schedule contains an unusual device capture period: ";
						warning+=ns_to_string(q->second.schedule->device_capture_period/60);
						warnings.push_back(warning);
						debug += "WARNING: ";
						debug += warning + ".\n\n";
				}
			}
		}
	}

	std::map<std::string,std::string> incubator_assignments;
	std::map<std::string,std::string> incubator_location_assignments;
	sql << "SELECT device_name, incubator_name, incubator_location FROM device_inventory";
	//ns_sql_result res;
	sql.get_rows(res);
	for (unsigned int i = 0; i < res.size(); ++i){
		incubator_assignments[res[i][0]] = res[i][1];
		incubator_location_assignments[res[i][0]] = res[i][2];
	}

	res.resize(0);
	sql.clear_query();
	sql.send_query("BEGIN");
	sql << "SELECT id FROM experiments WHERE name='" << sql.escape_string(name) << "'";
	sql.get_rows(res);
	bool experiment_already_exists(false);
	if(res.size() == 0){
		sql << "INSERT INTO experiments SET name='" << sql.escape_string(name) << "',description='',`partition`='', time_stamp=0";
		if (!actually_write){
			experiment_id = 0;
			debug+="Creating a new experiment named ";
			debug+= name + "\n";
		}
		else experiment_id = sql.send_query_get_id();
	}
	else{
		if (handle_existing==ns_stop)
			throw ns_ex("ns_experiment_capture_specification::submit_schedule_to_db::Experiment already exists and stopping behavior requested.");
		experiment_already_exists = true;
		if (!actually_write){
			if (handle_existing==ns_overwrite)
				debug+="Overwriting an existing experiment named ";
			else if(handle_existing == ns_append)
				debug += "Appending to an existing experiment named ";
			else throw ns_ex("Unknown handler option!");
			debug+= name + " with id = " + res[0][0] + "\n";
		}
		experiment_id = ns_atoi64(res[0][0].c_str());
	}



	sql.clear_query();
	res.resize(0);
	unsigned long latest_scheduled_capture_across_all_samples(0);
	try{
		for (unsigned int i = 0; i < samples.size(); i++){
			sql << "SELECT id, name, device_name,parameters FROM capture_samples WHERE experiment_id = " << experiment_id << " AND name='" << sql.escape_string(samples[i].sample_name) << "'";
			sql.get_rows(res);
			if (res.size() != 0) {
				if (handle_existing == ns_stop)
					throw ns_ex("ns_experiment_capture_specification::submit_schedule_to_db::Sample ") << samples[i].sample_name << " already exists and stopping behavior was requested";


				samples[i].sample_id = ns_atoi64(res[0][0].c_str());
				ns_processing_job job;
				job.sample_id = samples[i].sample_id;

				if (handle_existing == ns_overwrite) {
					if (!actually_write)
						debug += "Deleting previous sample " + samples[i].sample_name + ".\n";
					else ns_handle_image_metadata_delete_action(job, sql);
				}
				else {
					if (!actually_write)
						debug += "Appending to previous sample " + samples[i].sample_name + ".\n";

				}
			}

			if (handle_existing == ns_overwrite) {
				sql << "INSERT INTO capture_samples SET experiment_id = " << ns_to_string(experiment_id) << ",name='" << sql.escape_string(samples[i].sample_name) << "'"
					<< ",device_name='" << sql.escape_string(samples[i].device) << "',parameters='" << sql.escape_string(samples[i].capture_parameters()) << "'"
					<< ",position_x=" << samples[i].x_position << ",position_y=" << samples[i].y_position
					<< ",size_x=" << samples[i].width << ",size_y=" << samples[i].height
					<< ",incubator_name='" << sql.escape_string(incubator_assignments[samples[i].device])
					<< "',incubator_location='" << sql.escape_string(incubator_location_assignments[samples[i].device])
					<< "',desired_capture_duration_in_seconds=" << samples[i].desired_minimum_capture_duration
					<< ",description='',model_filename='',reason_censored='',image_resolution_dpi='" << samples[i].resolution
					<< "',device_capture_period_in_seconds=" << capture_schedules[samples[i].internal_schedule_id].device_capture_period
					<< ",number_of_consecutive_captures_per_sample=" << capture_schedules[samples[i].internal_schedule_id].number_of_consecutive_captures_per_sample
					<< ", time_stamp=0";
				if (!actually_write) {
					samples[i].sample_id = 0;
					debug += "Creating a new sample: name:";
					debug += samples[i].sample_name + ", device:" + samples[i].device + "\n\tcapture parameters: \"";
					debug += samples[i].capture_parameters() + "\"\n";
				}
				else {
					samples[i].sample_id = sql.send_query_get_id();
				}
			}

			if (handle_existing == ns_append) {

				sql << "SELECT scheduled_time FROM capture_schedule WHERE sample_id = " << samples[i].sample_id << " ORDER BY scheduled_time DESC LIMIT 1";
				res.resize(0);
				sql.get_rows(res);
				if (res.size() == 0) {
					std::string err = std::string("WARNING: Could not find any scheduled captures for sample id ") + ns_to_string(samples[i].sample_id);
					debug += err + "\n";
					warnings.push_back(err);
				}
				else {
					samples[i].latest_existing_scheduled_scan_time = atol(res[0][0].c_str());
					if (latest_scheduled_capture_across_all_samples < samples[i].latest_existing_scheduled_scan_time)
						latest_scheduled_capture_across_all_samples = samples[i].latest_existing_scheduled_scan_time;
					debug+= "The last scan idenfified for sample " + ns_to_string(samples[i].sample_id) + " was " + ns_format_time_string_for_human(samples[i].latest_existing_scheduled_scan_time) + "\n";

				}


			}

			sql.clear_query();
			res.resize(0);
		}

		sql.clear_query();
		res.resize(0);

		unsigned long longest_device_capture_period(0);

		if (handle_existing == ns_append) {
			for (unsigned long i = 0; i < capture_schedules.size(); i++) {
			if (longest_device_capture_period < capture_schedules[i].device_capture_period)
				longest_device_capture_period = capture_schedules[i].device_capture_period;
			}
			latest_scheduled_capture_across_all_samples += longest_device_capture_period;
		//	debug += "Adding new scans to an existing experiment, starting at " + ns_to_string(latest_scheduled_capture_across_all_samples) + "\n";
		}

		for (unsigned long i = 0; i < capture_schedules.size(); i++){

			unsigned long device_start_offset = 2*60;
			unsigned long s_offset(0);
			ns_device_schedule_list & device_schedules(capture_schedules[i].device_schedules);
			ns_device_start_offset_list & device_start_offsets(capture_schedules[i].device_start_offsets);
			for (ns_device_schedule_list::iterator p = device_schedules.begin(); p != device_schedules.end(); p++){
				device_start_offsets[p->first] = s_offset;
				s_offset+=device_start_offset;
				if (s_offset >= 20*60)
					s_offset = 0;
			}
		}

		for (unsigned int i = 0; i < capture_schedules.size(); i++){
			//compile correct start and stop time for each device.
			if (capture_schedules[i].start_time == 0) {
				if (handle_existing == ns_append) {
					debug += "Adding new scans to an existing experiment, starting at time " + ns_format_time_string_for_human(latest_scheduled_capture_across_all_samples) + "\n";
					capture_schedules[i].start_time = latest_scheduled_capture_across_all_samples;
				}
				else
					capture_schedules[i].start_time = ns_current_time() + 2 * 60;
			}
			capture_schedules[i].stop_time = 0;

			ns_device_schedule_list & device_schedules(capture_schedules[i].device_schedules);
			ns_device_start_offset_list & device_start_offsets(capture_schedules[i].device_start_offsets);
			for (ns_device_schedule_list::iterator p = device_schedules.begin(); p != device_schedules.end(); p++){
				const string & device_name = p->first;
				if (p->second.sample_groups.size() == 0) continue;
				p->second.effective_device_period = p->second.sample_groups.begin()->second.schedule->device_capture_period;
				p->second.number_of_consecutive_captures_per_sample = p->second.sample_groups.begin()->second.schedule->number_of_consecutive_captures_per_sample;
				if (p->second.effective_device_period == 0) throw ns_ex("Device period specified as zero!");
				if (p->second.number_of_consecutive_captures_per_sample == 0) throw ns_ex("Number of consecutive_captures_per_sample specified as zero!");

				//find earliest start time, stop time
				for (ns_device_capture_schedule::ns_sample_group_list::iterator q = p->second.sample_groups.begin(); q != p->second.sample_groups.end(); q++){
					if (q->second.schedule->start_time != 0 && q->second.schedule->start_time < ns_current_time() && handle_existing != ns_append)
						throw ns_ex("Start time specified is in the past ") << ns_format_time_string_for_human(q->second.schedule->start_time);
					if (q->first->start_time != 0)
						q->second.schedule->effective_start_time = q->first->start_time + device_start_offsets[device_name];
					else
						q->second.schedule->effective_start_time = capture_schedules[i].start_time + device_start_offsets[device_name];

					q->second.schedule->effective_stop_time = q->second.schedule->effective_start_time + q->second.schedule->duration  + device_start_offsets[device_name];

					if (q->second.schedule->effective_start_time < capture_schedules[i].start_time)
						capture_schedules[i].start_time = q->second.schedule->effective_start_time;
					if (q->second.schedule->effective_stop_time > capture_schedules[i].stop_time)
						capture_schedules[i].stop_time = q->second.schedule->effective_stop_time;

					if (q->second.schedule->device_capture_period != p->second.effective_device_period)
						throw ns_ex("Invalid device capture period specified for device") << p->second.device_name;
					if (q->second.schedule->number_of_consecutive_captures_per_sample != p->second.number_of_consecutive_captures_per_sample)
						throw ns_ex("Invalid device consecutive samples per sample specified for device") << p->second.device_name;
					if (q->second.samples.size() == 0)
						throw ns_ex("Empty device sample group found!");
				}
			}
			std::set<string> incubators;

			for (ns_device_schedule_list::iterator device = device_schedules.begin(); device != device_schedules.end(); device++){
				incubators.insert(incubator_assignments[device->second.device_name]);
			}

			debug +=  string("Schedule Involves ") + ns_to_string(device_schedules.size()) + " devices in " + ns_to_string( incubators.size()) + " location";
			if (incubators.size() != 1)
				debug+="s";
			debug +=":";
			for(std::set<string>::const_iterator p = incubators.begin(); p != incubators.end(); p++){
				debug+=*p;
				debug+=",";
			}
			debug += "\n";
			for (ns_device_schedule_list::iterator device = device_schedules.begin(); device != device_schedules.end(); device++){
				if (!actually_write){
					debug+=string("\tDevice ") + device->second.device_name + " runs between " +
						ns_format_time_string_for_human(capture_schedules[i].start_time + device_start_offsets[device->second.device_name]) +
							" and " +
							ns_format_time_string_for_human(capture_schedules[i].stop_time +  device_start_offsets[device->second.device_name]);
					debug+=" with a capture period of " + ns_capture_schedule::time_string(device->second.effective_device_period) + "\n";
				}
			}
		}
		ns_device_start_offset_list device_stop_times;
		ns_device_start_offset_list device_interval_at_stop;

		for (unsigned int i = 0; i < capture_schedules.size(); i++){
			ns_device_schedule_list & device_schedules(capture_schedules[i].device_schedules);
			ns_device_start_offset_list & device_start_offsets(capture_schedules[i].device_start_offsets);
			unsigned long number_of_captures(0);
			for (ns_device_schedule_list::iterator device = device_schedules.begin(); device != device_schedules.end(); device++){
				const string & device_name = device->first;

				ns_device_start_offset_list::iterator stop_time(device_stop_times.find(device_name));
				if(stop_time == device_stop_times.end()){
					device_stop_times[device_name] = 0;
					stop_time = device_stop_times.find(device_name);
				}


				if (!actually_write){
					debug+=string("Schedule for device ") + device->second.device_name + ":\n";
				}
				char have_started(false);
				ns_device_capture_schedule::ns_sample_group_list::iterator current_sample_group(device->second.sample_groups.begin());
				unsigned long current_sample_id = 0;


				for (unsigned long t = capture_schedules[i].start_time+device_start_offsets[device_name];  t < capture_schedules[i].stop_time+device_start_offsets[device_name];){

					ns_device_capture_schedule::ns_sample_group_list::iterator loop_start_group = current_sample_group;
					unsigned long loop_start_sample_id = current_sample_id;
					//find the next active sample at this time
					while(true){
						if (have_started){
							current_sample_id++;
							if (current_sample_id >= current_sample_group->second.samples.size()){
									current_sample_group++;
								current_sample_id = 0;
								if (current_sample_group == device->second.sample_groups.end())
									current_sample_group = device->second.sample_groups.begin();
							}
						}
						else have_started = true;
						if (current_sample_group->second.schedule->effective_start_time <= t+device->second.effective_device_period &&
							current_sample_group->second.schedule->effective_stop_time >= t+device->second.effective_device_period)
							break;
						if (current_sample_group == loop_start_group && current_sample_id == loop_start_sample_id )
							break;
					}

					//schedule the scans
					unsigned long dt_total = device->second.effective_device_period*device->second.number_of_consecutive_captures_per_sample;
					for (unsigned int dt = 0; dt < dt_total; dt+=device->second.effective_device_period){
						sql << "INSERT INTO capture_schedule SET experiment_id = " << experiment_id << ", scheduled_time = " << t+dt << ","
							<< "sample_id = " << current_sample_group->second.samples[current_sample_id]->sample_id << ", time_stamp = 0";
						if (!actually_write){
							sql.clear_query();
							debug +="\t";
							debug+= current_sample_group->second.samples[current_sample_id]->sample_name + ": " + ns_format_time_string_for_human(t+dt) + "\n";
						}
						else{
							sql.send_query();
							number_of_captures++;
						}
					}
					if (t+dt_total> stop_time->second){
						stop_time->second = t+dt_total;
						device_interval_at_stop[device_name] = device->second.effective_device_period;
					}

					t+=dt_total;
				}

				if (handle_existing != ns_append) {
					sql << "UPDATE experiments SET num_time_points = " << number_of_captures << ", first_time_point=" << capture_schedules[i].start_time
						<< ", last_time_point= " << capture_schedules[i].stop_time << " WHERE id=" << experiment_id;
				}
				else {

					sql << "UPDATE experiments SET num_time_points = num_time_points+" << number_of_captures
						<< ", last_time_point= " << capture_schedules[i].stop_time << " WHERE id=" << experiment_id;

				}
				if (actually_write)
					sql.send_query();
				sql.clear_query();
			}
		}
		
		//start autoscans to keep scanners running after the end of the experiment
		for (ns_device_start_offset_list::iterator p = device_stop_times.begin(); p != device_stop_times.end(); p++) {
			if (actually_write) {
				sql << "DELETE FROM autoscan_schedule WHERE device_name ='" << p->first << "'";
				sql.send_query();

				sql << "UPDATE devices SET autoscan_interval=0 WHERE name ='" << p->first << "'";
				sql.send_query();
				debug += "Stopping existing and previously scheduled autoscans on " + p->first + "\n";
			}
			sql << "INSERT INTO autoscan_schedule SET device_name='" << p->first
					<< "', autoscan_start_time=" << (p->second + device_interval_at_stop[p->first])
					<< ", scan_interval = " << device_interval_at_stop[p->first];
				if (actually_write)
					sql.send_query();
				else{
					debug+="Scheduling an ";
					debug+=ns_to_string(device_interval_at_stop[p->first]) + " second autoscan sequence on device "
						+ p->first + " at " + ns_format_time_string_for_human(p->second + device_interval_at_stop[p->first]) + "\n";
				}
		}
		sql.send_query("COMMIT");

		//start any remote servers downloading the new records all at once.
		sql.send_query("UPDATE experiments SET time_stamp = NOW() WHERE time_stamp = 0");
		sql.send_query("UPDATE capture_samples SET time_stamp = NOW() WHERE time_stamp = 0");
		sql.send_query("UPDATE capture_schedule SET time_stamp = NOW() WHERE time_stamp = 0");

	}
	catch(...){
		sql.send_query("ROLLBACK");
		throw;
	}
	return debug;
}


void ns_sample_capture_specification::from_scan_area_string(const std::string & scan_area){
 	string temp;
	char state(0);
	double * r[4] = {&x_position,&y_position,&width,&height};

	for (unsigned int i = 0; i < scan_area.size(); i++){
		if (scan_area[i]==','){
			if (state == 3) throw ns_ex("Invalid scan_area specification (too many components): ") << scan_area;
			*r[state] = ns_sample_capture_specification::get_inches(temp);
			temp.resize(0);
			state++;
		}
		else temp+=scan_area[i];
	}
	if (temp.size() == 0 || state!=3) throw ns_ex("Invalid scan_area specification (too few components): ") << scan_area;
	*r[state] = ns_sample_capture_specification::get_inches(temp);
}
double ns_sample_capture_specification::get_inches(const std::string & s){
	unsigned int i;
	for (i = 0; i < s.size(); i++)
		if (!isdigit(s[i]) && s[i] != '.') break;
	double val(atof(s.substr(0,i).c_str()));
	string unit(s.substr(i));
	if (unit.size() == 0 || unit == "mm")
		return val/25.4;
	if (unit=="cm")
		return val/2.54;
	if (unit=="in")
		return val;
	else throw ns_ex("Could not interpret unit specification: ") << unit;
}
void ns_capture_schedule::populate_sample_names_from_list(const string & slist,vector<ns_sample_capture_specification> & source_samples){
	if (slist.size() == 0) return;
	long s = 0;
	vector<std::string> names;
	names.resize(1);
	for (unsigned int i = 0; i < slist.size(); i++){
		if (slist[i]==','){
			s++;
			names.resize(s+1);
		}
		else names[s]+=slist[i];
	}
	samples.resize(names.size());
	for (unsigned int i = 0; i < names.size(); i++){
		cerr << "Looking for name \"" << names[i] << "\"\n";
		bool found(false);
		for (unsigned int j = 0; j < source_samples.size(); j++){
			cerr << "\tChecking \"" << source_samples[j].sample_name << "\"\n";
			if (names[i] == source_samples[j].sample_name){
				found = true;
				samples[i] = &source_samples[j];
				source_samples[j].internal_schedule_id = internal_id;
				break;
			}
		}
			if (!found)
				throw ns_ex("Could not find specification for sample ") << names[i] << " requested in schedule";
	}
}
unsigned long ns_capture_schedule::decode_time_string(const string & s){
	unsigned long t(0);

	string cur_time;
	for (unsigned int i = 0; i < s.size(); i++){
		if (isdigit(s[i]))
			cur_time+=s[i];
		else{
			switch(tolower(s[i])){
				case 'w': t+= 7*24*60*60*atol(cur_time.c_str()); break;
				case 'd': t+= 24*60*60*atol(cur_time.c_str()); break;
				case 'h': t+= 60*60*atol(cur_time.c_str()); break;
				case 'm': t+= 60*atol(cur_time.c_str()); break;
				case 's': t+= atol(cur_time.c_str()); break;
				default: throw ns_ex("Invalid time specification: ") << s;
			}
			cur_time.resize(0);
		}
	}
	if (cur_time.size() != 0)
		 throw ns_ex("Invalid time specification: ") << s;
	return t;
}


void ns_experiment_capture_specification::produce_device_schedule(){

	//link up schedules to their specified samples
	for (unsigned int i = 0; i < capture_schedules.size(); i++){
		if (capture_schedules[i].sample_name_list.size() > 0){
			if (capture_schedules[i].use_all_samples)
				throw ns_ex("A schedule was specified as applying to all samples, but then a sample list was specified.");
			capture_schedules[i].populate_sample_names_from_list(capture_schedules[i].sample_name_list,samples);
		}
		else if (capture_schedules[i].use_all_samples == false)
			throw ns_ex("A schedule was specified with no samples.");
	}

	check_samples_and_fill_in_defaults();
	for (unsigned int i = 0; i < capture_schedules.size(); i++){
		for (unsigned int j = 0; j < capture_schedules[i].samples.size(); j++){

			ns_sample_capture_specification * s(capture_schedules[i].samples[j]);
			//find or make the device schedule
			ns_device_schedule_list::iterator p(capture_schedules[i].device_schedules.find(s->device));
			if (p == capture_schedules[i].device_schedules.end()){
				p = capture_schedules[i].device_schedules.insert(capture_schedules[i].device_schedules.begin(),std::pair<string,ns_device_capture_schedule>(s->device,ns_device_capture_schedule()));
				p->second.device_name = s->device;
			}
			//find or make the schedule group
			ns_device_capture_schedule::ns_sample_group_list::iterator q(p->second.sample_groups.find(&capture_schedules[i]));
			if (q == p->second.sample_groups.end()){
				q = p->second.sample_groups.insert(p->second.sample_groups.begin(),std::pair<ns_capture_schedule *,ns_device_capture_schedule_sample_group>(&capture_schedules[i],ns_device_capture_schedule_sample_group()));
				q->second.schedule = &capture_schedules[i];
			}
			//add the sample to the appropriate sample group
			q->second.samples.push_back(s);
		}
	}

	//name unnamed samples
	//also catch different intervals being placed on the same device
	for (unsigned int k = 0; k < capture_schedules.size(); k++){
		ns_device_schedule_list & device_schedules(capture_schedules[k].device_schedules);
		for(ns_device_schedule_list::iterator p = device_schedules.begin(); p != device_schedules.end(); p++){
			if (p->second.sample_groups.size() == 0) continue;
			unsigned long device_capture_period(p->second.sample_groups.begin()->second.schedule->device_capture_period);
			for (ns_device_capture_schedule::ns_sample_group_list::iterator q = p->second.sample_groups.begin(); q != p->second.sample_groups.end(); q++){

				if (q->second.schedule->device_capture_period != device_capture_period)
					throw ns_ex("Device ") << p->second.device_name << " has multiple periods specified: " << q->second.schedule->device_capture_period << "," << device_capture_period;

				//assign names by sorting them by location
				std::sort(q->second.samples.begin(),q->second.samples.end(),ns_sample_capture_specification_sorter());
				char name = 'a';
				for (unsigned int i = 0; i < q->second.samples.size(); i++){
					if (q->second.samples[i]->sample_name.size() == 0){
						if (default_sample_naming == ns_experiment_capture_specification::ns_none) throw ns_ex("Sample with no name specified is specified to run on device ") << p->second.device_name << ", although no default naming scheme is specified";
						q->second.samples[i]->sample_name = (p->second.device_name + "_" + name);
						name++;
						if (name > 'z') throw ns_ex("Too many unnamed samples specified for device ") << p->second.device_name;
					}
				}
			}
		}
	}
	device_schedule_produced = true;
}

void ns_experiment_capture_specification::clear(){
	samples.clear();
	capture_schedules.clear();
	experiment_id = 0;
	name.clear();
	default_capture_configuration_parameters.clear();
	default_desired_minimum_capture_duration = 0;
	device_schedule_produced = false;
}

void ns_experiment_capture_specification::save_to_xml_file(const string & filename) const{
	ofstream o(filename.c_str());
	if (o.fail()) throw ns_ex("ns_experiment_capture_specification::save_to_xml_file()::Cannot open file ") << filename;
	string os;
	save_to_xml(os);
	o << os;
	o.close();
}
void ns_experiment_capture_specification::save_to_xml(string & o)const{
	ns_xml_simple_writer xml;
	xml.generate_whitespace();
	xml.add_header();
	xml.start_group("experiment");
	xml.add_tag("name",name);
	if (default_capture_configuration_parameters.size() != 0)
		xml.add_tag("default_capture_configuration_parameters",default_capture_configuration_parameters);

	xml.add_tag("default_desired_minimum_capture_duration",default_desired_minimum_capture_duration);
	if (default_sample_naming != ns_experiment_capture_specification::ns_none)
		xml.add_tag("default_sample_naming","by_device");
	xml.add_tag("capture_resolution",image_resolution);

	xml.end_group();
	for (unsigned int i = 0; i < samples.size(); i++){
		xml.start_group("sample");
		xml.add_tag("name",samples[i].sample_name);
		xml.add_tag("scan_area",samples[i].generate_scan_area_string());
		if (samples[i].capture_configuration_parameters.size() != 0 &&
			default_capture_configuration_parameters !=
			samples[i].capture_configuration_parameters
			)
			xml.add_tag("capture_configuration_parameters",samples[i].capture_configuration_parameters);
		if (samples[i].desired_minimum_capture_duration!=0)
			xml.add_tag("desired_minimum_capture_duration",samples[i].desired_minimum_capture_duration);

		xml.add_tag("device",samples[i].device);
		xml.end_group();
	}
	for (unsigned int i = 0; i < capture_schedules.size(); i++){
		xml.start_group("schedule");
		if (capture_schedules[i].use_all_samples)
			xml.add_tag("sample_set_type","all");
		else{
			string sample_names;
			for (unsigned int j = 0; j < capture_schedules[i].samples.size(); j++)
				sample_names+= capture_schedules[i].samples[j]->sample_name + ",";
			if (sample_names.size() != 0) //remove trailing comma
				sample_names.resize(sample_names.size()-1);
			xml.add_tag("sample_list",sample_names);
		}
		xml.add_tag("start_time",ns_capture_schedule::time_string(capture_schedules[i].start_time));
		xml.add_tag("duration",ns_capture_schedule::time_string(capture_schedules[i].duration));
		xml.add_tag("device_capture_period",ns_capture_schedule::time_string(capture_schedules[i].device_capture_period));
		xml.add_tag("number_of_consecutive_captures_per_sample",ns_capture_schedule::time_string(capture_schedules[i].number_of_consecutive_captures_per_sample));
		xml.end_group();
	}
	xml.add_footer();
	o = xml.result();
}


std::string ns_capture_schedule::time_string(unsigned long s){
	string o;
	if (s/(60*60*24) > 0){
		o += ns_to_string((unsigned long)(s/(60*60*24))) + "d";
		s-=(60*60*24)*(s/(60*60*24));
	}
	if (s/(60*60) > 0){
		o += ns_to_string((unsigned long)(s/(60*60))) + "h";
		s-=(60*60)*(s/(60*60));
	}
	if (s/60 > 0){
		o += ns_to_string((unsigned long)(s/60)) + "m";
		s-=60*(s/60);
	}
	if (s > 0)
		o += ns_to_string(s) + "s";
	return o;
}
void ns_experiment_capture_specification::load_from_xml_file(const string & filename){
	ifstream i(filename.c_str());
	if (i.fail()) throw ns_ex("ns_experiment_capture_specification::load_from_xml_file()::Cannot open file ") << filename <<" .  This can be caused by unclosed XML tags or quotation marks, or by whitespace after the (optional) final </xml> tag";
	string os;
	//os.reserve(500);
	while(true){
		char c(i.get());
		if (i.fail()) break;
		os+=c;
	}
	/*
	char buf[256];
	while(true){
		i.read(buf,255);
		int r(i.gcount());
		buf[r]=0;
		os+=buf;
		if (r < 255 || i.fail())
			break;
	}*/
	i.close();
	load_from_xml(os);
}
void ns_experiment_capture_specification::confirm_valid_name(const string & name){
			if (   name.find("=") != name.npos
				|| name.find("$") != name.npos
				|| name.find("\\") != name.npos
				|| name.find("/") != name.npos
				|| name.find(":") != name.npos
				|| name.find("?") != name.npos
				|| name.find("\"") != name.npos
				|| name.find("<") != name.npos
				|| name.find(">") != name.npos
				|| name.find(".") != name.npos
				|| name.find(",") != name.npos
				|| name.find(" ") != name.npos)
				throw ns_ex("Experiment names cannot include the characters =\\/:*?\"<>| .,");
}
void ns_experiment_capture_specification::load_from_xml(const string & o){
	clear();
	ns_xml_simple_object_reader xml;
	xml.from_string(o);
	for (unsigned int i = 0; i < xml.objects.size(); i++){
		//Handle an Experiment Specification
		if (xml.objects[i].name == "experiment"){

			name = xml.objects[i].tag("name");
			confirm_valid_name(name);
			if (xml.objects[i].tag_specified("default_capture_configuration_parameters"))
				default_capture_configuration_parameters = xml.objects[i].tag("default_capture_configuration_parameters");
			string tmp;
			xml.objects[i].assign_if_present("default_sample_naming",tmp);
			image_resolution = atol(xml.objects[i].tag("capture_resolution").c_str());
			if (tmp.size() == 0 || tmp == "none")
				default_sample_naming = ns_experiment_capture_specification::ns_none;
			else if (tmp == "by_device" || tmp == "device")
				default_sample_naming = ns_by_device;
			else throw ns_ex("Unknown default sample naming scheme: ") << default_sample_naming;
			default_desired_minimum_capture_duration =  ns_capture_schedule::decode_time_string(xml.objects[i].tag("default_desired_minimum_capture_duration"));
		}
		//Handle a Sample Specification
		else if (xml.objects[i].name == "sample"){
			long s = samples.size();
			samples.resize(s+1);
			xml.objects[i].assign_if_present("name",samples[s].sample_name);
			confirm_valid_name(samples[s].sample_name);
			samples[s].device = xml.objects[i].tag("device");
			//if (samples[s].sample_name.size() == 0) cerr << "Found sample on device " << samples[s].device << "\n";
			string tmp;
			if (xml.objects[i].assign_if_present("scan_area",tmp))
				samples[s].from_scan_area_string(tmp);
			else{
				samples[s].x_position = ns_sample_capture_specification::get_inches(xml.objects[i].tag("x_position"));
				samples[s].y_position = ns_sample_capture_specification::get_inches(xml.objects[i].tag("y_position"));
				samples[s].width = ns_sample_capture_specification::get_inches(xml.objects[i].tag("width"));
				samples[s].height = ns_sample_capture_specification::get_inches(xml.objects[i].tag("height"));
			}
			xml.objects[i].assign_if_present("capture_configuration_parameters",samples[s].capture_configuration_parameters);
			std::string cap_dur;
			if (!xml.objects[i].assign_if_present("desired_minimum_capture_duration",cap_dur))
				samples[s].desired_minimum_capture_duration = 0;
			else samples[s].desired_minimum_capture_duration = ns_capture_schedule::decode_time_string(cap_dur);
		}
		//Handle a Schedule Specification
		else if (xml.objects[i].name == "schedule"){
			long s = capture_schedules.size();
			capture_schedules.resize(s+1,s);

			string set_type;
			xml.objects[i].assign_if_present("samples_that_belong_to_schedule",set_type);
			if (set_type.size()!=0){
				if (set_type == "all")
					capture_schedules[s].use_all_samples = true;
				else if (set_type == "list")
					capture_schedules[s].use_all_samples = false;
				else throw ns_ex("Unknown sample_set_type") << set_type;
			}
			else capture_schedules[s].use_all_samples = false;
			string tmp;
			if (xml.objects[i].assign_if_present("start_time",tmp))
				capture_schedules[s].start_time = atol(tmp.c_str());
			else capture_schedules[s].start_time = 0;
			if(xml.objects[i].assign_if_present("start_time_offset",tmp))
				capture_schedules[s].start_time = ns_current_time() + ns_capture_schedule::decode_time_string(tmp);

			capture_schedules[s].duration = ns_capture_schedule::decode_time_string(xml.objects[i].tag("duration"));
			capture_schedules[s].device_capture_period =  ns_capture_schedule::decode_time_string(xml.objects[i].tag("device_capture_period"));
			capture_schedules[s].number_of_consecutive_captures_per_sample = atol(xml.objects[i].tag("number_of_consecutive_captures_per_sample").c_str());

		}
		else throw ns_ex("Unkown experiment specification tag:") << xml.objects[i].name;
	}


	cerr << "Loaded " << capture_schedules.size() << " schedules.\n";

	produce_device_schedule();
}
void ns_experiment_capture_specification::check_samples_and_fill_in_defaults(){
	//do a few sanity checks and fill in default parameters
	//fill in default scanner parameters
	for (unsigned int i = 0; i < samples.size(); i++){
		if (samples[i].capture_configuration_parameters.size() == 0){
			if (default_capture_configuration_parameters.size() == 0)
				throw ns_ex("ns_experiment_capture_specification::load_from_xml()::No capture parameters specified either in experiment or sample ") << samples[i].sample_name;
			samples[i].capture_configuration_parameters = default_capture_configuration_parameters;
			samples[i].resolution = image_resolution;
		}
		if (samples[i].desired_minimum_capture_duration == 0)
			samples[i].desired_minimum_capture_duration = default_desired_minimum_capture_duration;
	}
	//fill in sample names for capture schedules specifying all samples
	for (unsigned int i = 0; i < capture_schedules.size(); i++){
		if (capture_schedules[i].use_all_samples){
			if (capture_schedules[i].samples.size() != 0)
				throw ns_ex("Capture Schedule specifies all samples but has sample_list specified");
			capture_schedules[i].samples.resize(samples.size());
			for (unsigned int j = 0; j < samples.size(); j++){
				capture_schedules[i].samples[j] = &samples[j];
				samples[j].internal_schedule_id = capture_schedules[i].internal_id;
			}
		}
	}
	//check to see that the experiment has a valid name
	if (name.size() == 0)
		throw ns_ex("Experiment has no name specified");
	if(name.find("=") != string::npos)
		throw ns_ex("Experiment names cannot contain equals signs: ") << name;

	//check to see that all devices are specified
	for (unsigned int i = 0; i < samples.size(); i++){
		if (samples[i].device.size() == 0)
			throw ns_ex("Sample named ") << samples[i].sample_name << " has no device specified";
	}
}
