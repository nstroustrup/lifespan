#include "ns_survival_curve.h"
#include "ns_ex.h"
#include <fstream>
#include <string>
#include <iostream>
#include "ns_movement_state.h"
#include <set>
#include <algorithm>
#include "ns_by_hand_lifespan.h"
#include "ns_jmp_file.h"
#include "ns_xml.h"
#include <limits>
#include "ns_death_time_annotation_set.h"
using namespace std;

ns_image_pool<ns_survival_data, ns_survival_data_resizer> ns_lifespan_experiment_set::memory_pool;

#ifndef NS_NO_SQL
void ns_genotype_fetcher::load_from_db(ns_image_server_sql * sql, const bool load_all){
	*sql << "SELECT strain,genotype,id FROM strain_aliases";
	if (!load_all) *sql << " WHERE used_in_cluster = 1";
	ns_sql_result res;
	sql->get_rows(res);
	for (unsigned int i = 0; i < res.size(); i++){
		genotypes[res[i][0]] = ns_genotype_db_internal_info(atol(res[i][2].c_str()),res[i][1]);
	}
}
	
const std::string & ns_genotype_fetcher::genotype_from_strain(const std::string & strain, ns_image_server_sql * sql) const{
	ns_genotype_list::const_iterator p(genotypes.find(strain));
	if (p == genotypes.end()){
		*sql << "SELECT strain,genotype,id FROM strain_aliases WHERE strain='" << strain << "'";
		ns_sql_result res;
		sql->get_rows(res);
		if (res.size() == 0)
			return empty_string;
		else 
			p = genotypes.insert(
			ns_genotype_list::value_type(res[0][0],ns_genotype_db_internal_info(atol(res[0][2].c_str()),res[0][1]))
			).first;
		*sql << "UPDATE strain_aliases SET used_in_cluster = 1 WHERE id = " << res[0][2];
		sql->send_query();
	}
	return p->second.genotype;
}
#endif

void ns_survival_data::convert_absolute_times_to_ages(){
	for (unsigned int i = 0; i < timepoints.size(); i++)
		timepoints[i].absolute_time-=metadata.time_at_which_animals_had_zero_age;
	metadata.time_at_which_animals_had_zero_age = 0;
}
void ns_lifespan_experiment_set::convert_absolute_times_to_ages(){

	for (unsigned int i = 0; i < curves.size(); i++)
		curves[i]->convert_absolute_times_to_ages();
}

void ns_lifespan_experiment_set::load_from_by_hand_lifespan_file(std::ifstream & in){
	ns_by_hand_lifespan_experiment_specification spec;
	spec.load(in);
	spec.convert_to_lifespan_experiment_set(*this);
}

void ns_region_metadata::load_from_fields(const std::map<std::string,std::string> & m,std::map<std::string,std::string> &unknown_values){
	clear();
	std::string time_string;
	for (std::map<std::string,std::string>::const_iterator p = m.begin(); p != m.end(); p++){
		const string f(ns_to_lower(p->first));
		if (is_age_zero_field(f)){
			time_string = p->second;
			time_at_which_animals_had_zero_age = ns_time_from_format_string(p->second);
		}
		else{
			std::string * v(get_field_by_name(f));
			if (v != 0) *v = p->second;
			else unknown_values.insert(p,p);
		}
	}
	cerr << "Converted plate " << plate_name() << "'s start time from " << time_string << " to " << ns_format_time_string_for_human(this->time_at_which_animals_had_zero_age) << "\n";
}
void ns_region_metadata::recognized_field_names(std::vector<std::string> & names){
	names.push_back("details");
	names.push_back("device");
	names.push_back("experiment");
	names.push_back("region OR plate");
	names.push_back("sample");
	names.push_back("technique");
	names.push_back("strain");
	names.push_back("genotype");
	names.push_back("analysis type");
	names.push_back("technique");
	names.push_back("culturing temperature");
	names.push_back("experiment temperature");
	names.push_back("food source");
	names.push_back("environmental conditions");
	names.push_back("condition 1 OR strain condition 1");
	names.push_back("condition 2 OR strain condition 2");
	names.push_back("condition 3 OR strain condition 3");
	names.push_back("at age zero OR time at age zero OR age zero");
}

bool ns_region_metadata::is_age_zero_field(const std::string & s){return s.find("at age zero")!=std::string::npos || s.find("time at age zero")!=std::string::npos|| s.find("age zero")!=std::string::npos;}

std::string * ns_region_metadata::get_field_by_name(const std::string & s){
	if (s.find("details")!=std::string::npos)
		return &details;
	else if (s.find("device")!=std::string::npos)
		return &device;
	else if (s.find("culturing_temperature")!=std::string::npos || s.find("culturing temperature")!=std::string::npos)
		return &culturing_temperature;
	else if (s.find("experiment_temperature")!=std::string::npos || s.find("experiment temperature")!=std::string::npos)
		return &experiment_temperature;
	else if (s.find("experiment")!=std::string::npos)
		return &experiment_name;
	else if (s.find("region")!=std::string::npos ||
			 s.find("plate")!=std::string::npos)
		return &region_name;
	else if (s.find("sample")!=std::string::npos)
		return &sample_name;
	else if (s.find("technique")!=std::string::npos)
		return &technique;
	else if (s.find("strain")!=std::string::npos)
		return &strain;
	else if (s.find("genotype")!=std::string::npos)
		return &genotype;
	else if (s.find("analysis_type")!=std::string::npos || s.find("analysis type")!= std::string::npos)
		return &analysis_type;
	else if (s.find("technique")!=std::string::npos)
		return &technique;	
	else if (s.find("condition 1")!=std::string::npos || s.find("strain condition 1")!=std::string::npos || s.find("strain_condition_1")!=std::string::npos)
		return &strain_condition_1;	
	else if (s.find("condition 2")!=std::string::npos || s.find("strain condition 2")!=std::string::npos || s.find("strain_condition_2")!=std::string::npos)
		return &strain_condition_2;
	else if (s.find("condition 3")!=std::string::npos || s.find("strain condition 3")!=std::string::npos || s.find("strain_condition_3")!=std::string::npos)
		return &strain_condition_3;	
	else if (s.find("food_source")!=std::string::npos || s.find("food source")!=std::string::npos)
		return &food_source;
	else if (s.find("environmental_conditions")!=std::string::npos || s.find("environmental conditions")!=std::string::npos)
		return &environmental_conditions;
	else if (is_age_zero_field(s))
		throw ns_ex("get_field_by_name()::Found zero age field");
	return 0;
}


#ifndef NS_NO_SQL
void ns_region_metadata::load_only_region_info_from_db(const ns_64_bit region_info_id, const std::string &analysis_type_, ns_sql & sql){
	sql << "SELECT sample_id, name, strain, strain_condition_1,strain_condition_2,strain_condition_3,"
		   "culturing_temperature,experiment_temperature,food_source, environmental_conditions,"
		   "time_at_which_animals_had_zero_age,details,time_of_last_valid_sample,"
		   "position_in_sample_x,position_in_sample_y,size_x,size_y, latest_movement_rebuild_timestamp,latest_by_hand_annotation_timestamp "
		   "FROM sample_region_image_info WHERE id = " << region_info_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("Could not load metadata for region ") << region_info_id;
	region_id = region_info_id;
	sample_id = ns_atoi64(res[0][0].c_str());
	region_name = res[0][1];
	strain = res[0][2];
	strain_condition_1 = res[0][3];
	strain_condition_2 = res[0][4];
	strain_condition_3 = res[0][5];
	culturing_temperature = res[0][6];
	experiment_temperature = res[0][7];
	food_source = res[0][8];
	environmental_conditions = res[0][9];
	time_at_which_animals_had_zero_age = atol(res[0][10].c_str());
	details = res[0][11];
	time_of_last_valid_sample = atol(res[0][12].c_str());
	position_of_region_in_sample.x = atof(res[0][13].c_str());
	position_of_region_in_sample.y = atof(res[0][14].c_str());
	size.x = atof(res[0][15].c_str());
	size.y = atof(res[0][16].c_str());
	movement_rebuild_timestamp = atol(res[0][17].c_str());
	by_hand_annotation_timestamp = atol(res[0][18].c_str());
	analysis_type = analysis_type_;
}

void ns_region_metadata::load_only_sample_info_from_db(const ns_64_bit sample_id_, ns_sql & sql){
	sample_id = sample_id_;
	sql << "SELECT experiment_id,name,device_name,incubator_name,incubator_location, position_x, position_y, image_resolution_dpi FROM capture_samples WHERE id = " << sample_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("Could not load metadata for sample ") << sample_id;
	sample_name = res[0][1];
	device = res[0][2];
	incubator_name = res[0][3];
	incubator_location = res[0][4];
	experiment_id = ns_atoi64(res[0][0].c_str());
	float resolution(atof(res[0][7].c_str()));
	if (resolution == 0)
		throw ns_ex("Sample had a resolution of 0 specified in the database!");
	position_of_sample_on_scanner = ns_vector_2i(atof(res[0][5].c_str()),atof(res[0][6].c_str()));

	sql << "SELECT name,id FROM experiments WHERE id = " << res[0][0];
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("Could not load metadata for sample ") << sample_id << ", experiment " << res[0][0];
	experiment_name = res[0][0];
}

void ns_region_metadata::load_from_db(const ns_64_bit region_info_id, const std::string &analysis_type_, ns_sql & sql){
	load_only_region_info_from_db(region_info_id,analysis_type_,sql);
	load_only_sample_info_from_db(sample_id,sql);
}
#endif
bool operator<(const ns_survival_timepoint & a, const ns_survival_timepoint & b){
	return a.absolute_time < b.absolute_time;
}

void ns_lifespan_experiment_set::load_from_JMP_file(std::ifstream & in){
	ns_jmp_file file;
	file.load(in);
	file.generate_survival_curve_set(*this);
}

void ns_lifespan_experiment_set::output_R_file(std::ostream & out)const{
	throw ns_ex("Not implemented");
}

void ns_lifespan_experiment_set::group_strains(ns_lifespan_experiment_set & new_set) const{
//	throw ns_ex("Not implemented");
	if (!set_is_on_common_time())
		throw ns_ex("ns_lifespan_experiment_set::group_strains()::Can only group curves after they are placed on a common time axis.");

	
	const std::vector<unsigned long> & c(common_time());
	new_set.clear();
	new_set.common_time_.insert(new_set.common_time_.begin(),common_time().begin(),common_time().end());
	new_set.curves.reserve(11);
	map<string,unsigned int> strain_ids;
	for (unsigned int i = 0; i < curves.size(); i++){
		unsigned long new_id;
		{
			string cur_name(curves[i]->metadata.plate_type_summary());
			map<string,unsigned int>::iterator p = strain_ids.find(cur_name);
			if (p == strain_ids.end()){
				new_id = new_set.curves.size();
				new_set.curves.resize(new_id+1,memory_pool.get(0));
				new_set.curves[new_id]->metadata = curves[i]->metadata;
				new_set.curves[new_id]->metadata.sample_name = "All " + cur_name + " animals";
				new_set.curves[new_id]->metadata.region_name = "All " + cur_name + " animals";
				new_set.curves[new_id]->timepoints.resize(common_time().size());
				strain_ids[cur_name] = new_id;
			}
			else new_id = p->second;
		}
		for (unsigned int t = 0; t < c.size(); t++)
			new_set.curves[new_id]->timepoints[t].add(curves[i]->timepoints[t]);
	}
	new_set.curves_on_constant_time_interval = curves_on_constant_time_interval;
}

double ns_find_percentile_average(const std::vector<double> & d, const unsigned long percentile){
		unsigned int start,
					 stop;
		switch(percentile){
			case 1: start = 0; stop = d.size()/10; break;
			case 9: start = (9*d.size())/10; stop = d.size(); break;
			default: throw ns_ex("Lazy percentile implentation can't handle your reasonable input");
		}
		double sum(0);
		for (unsigned int i = start; i < stop; i++)
			sum+=d[i];
		if (start==stop)
			return 0;
		return sum/(stop-start);
}

std::string ns_region_metadata::to_xml() const{
	ns_xml_simple_writer xml;
	xml.add_tag("device",device);
	xml.add_tag("technique",technique);
	xml.add_tag("experiment_name",experiment_name);
	xml.add_tag("sample_name",sample_name);
	xml.add_tag("region_name",region_name);
	xml.add_tag("strain",strain);
	xml.add_tag("strain_condition_1",strain_condition_1);
	xml.add_tag("strain_condition_2",strain_condition_2);
	xml.add_tag("strain_condition_3",strain_condition_3);
	xml.add_tag("culturing_temperature",culturing_temperature);
	xml.add_tag("experiment_temperature",experiment_temperature);
	xml.add_tag("food_source",food_source);
	xml.add_tag("environmental_conditions",environmental_conditions);
	xml.add_tag("genotype",genotype);
	xml.add_tag("details",details);
	xml.add_tag("analysis_type",analysis_type);
	xml.add_tag("region_id",region_id);
	return xml.result();
}

bool ns_region_metadata::from_xml(const ns_xml_simple_object & o){
	int found_something(0);
	found_something+=(int)o.assign_if_present("device",device);
	found_something+=(int)o.assign_if_present("technique",technique);
	found_something+=(int)o.assign_if_present("experiment_name",experiment_name);
	found_something+=(int)o.assign_if_present("sample_name",sample_name);
	found_something+=(int)o.assign_if_present("region_name",region_name);
	found_something+=(int)o.assign_if_present("strain",strain);
	found_something+=(int)o.assign_if_present("strain_condition_1",strain_condition_1);
	found_something+=(int)o.assign_if_present("strain_condition_2",strain_condition_2);
	found_something+=(int)o.assign_if_present("culturing_temperature",culturing_temperature);
	found_something+=(int)o.assign_if_present("experiment_temperature",experiment_temperature);
	found_something+=(int)o.assign_if_present("food_source",food_source);
	found_something+=(int)o.assign_if_present("environmental_conditions",environmental_conditions);

	found_something+=(int)o.assign_if_present("genotype",genotype);
	found_something+=(int)o.assign_if_present("details",details);
	found_something+=(int)o.assign_if_present("analysis_type",analysis_type);
	region_id = 0;
	found_something+=(int)o.assign_if_present("region_id",region_id);
	return found_something > 0;
}

void ns_survival_data_summary::from_xml(const ns_xml_simple_object & o){
	if (o.name !=  xml_tag())
		throw ns_ex("ns_survival_data_summary::from_xml()::Unknown object type: " ) << o.name;
	if (!metadata.from_xml(o))
		throw ns_ex("ns_survival_data_summary::from_xml()::Could not load any metadata for the specified data");

	o.assign_if_present("ldc",		long_distance_movement_cessation.count);
	o.assign_if_present("ldbcc",	long_distance_movement_cessation.by_hand_excluded_count);
	o.assign_if_present("ldmcc",	long_distance_movement_cessation.machine_excluded_count);
	o.assign_if_present("ldcs",	long_distance_movement_cessation.censored_count);
	o.assign_if_present("ldmw",		long_distance_movement_cessation.number_of_events_involving_multiple_worm_disambiguation);
	o.assign_if_present("ldm",		long_distance_movement_cessation.mean );
	o.assign_if_present("ldv",		long_distance_movement_cessation.variance);
	o.assign_if_present("ldp10",	long_distance_movement_cessation.percentile_10th);
	o.assign_if_present("ldp50",	long_distance_movement_cessation.percentile_50th );
	o.assign_if_present("ldp90",	long_distance_movement_cessation.percentile_90th);
	o.assign_if_present("ldmin",	long_distance_movement_cessation.minimum );
	o.assign_if_present("ldmax",	long_distance_movement_cessation.maximum );
	
	o.assign_if_present("loc",		local_movement_cessation.count);
	o.assign_if_present("lobcc",	local_movement_cessation.by_hand_excluded_count);
	o.assign_if_present("lomcc",	local_movement_cessation.machine_excluded_count);
	o.assign_if_present("locs",	local_movement_cessation.censored_count);
	o.assign_if_present("lomw",	local_movement_cessation.number_of_events_involving_multiple_worm_disambiguation);
	o.assign_if_present("lom",		local_movement_cessation.mean );
	o.assign_if_present("lov",		local_movement_cessation.variance );
	o.assign_if_present("lop10",	local_movement_cessation.percentile_10th);
	o.assign_if_present("lop50",	local_movement_cessation.percentile_50th );
	o.assign_if_present("lop90",	local_movement_cessation.percentile_90th);
	o.assign_if_present("lomin",	local_movement_cessation.minimum);
	o.assign_if_present("lomax",	local_movement_cessation.maximum);


	o.assign_if_present("dc",		death.count);
	o.assign_if_present("dmcc",		death.machine_excluded_count);
	o.assign_if_present("dbcc",		death.by_hand_excluded_count);
	o.assign_if_present("dcs",		death.censored_count);
	o.assign_if_present("dmw",		death.number_of_events_involving_multiple_worm_disambiguation);
	o.assign_if_present("dm",		death.mean);
	o.assign_if_present("dv",		death.variance );
	o.assign_if_present("dp10",		death.percentile_10th);
	o.assign_if_present("dp50",		death.percentile_50th );
	o.assign_if_present("dp90",		death.percentile_90th );
	o.assign_if_present("dmin",		death.minimum );
	o.assign_if_present("dmax",		death.maximum);
}

std::string ns_survival_data_summary::to_xml() const{
	ns_xml_simple_writer xml;
	xml.start_group( xml_tag());
	xml.add_raw(metadata.to_xml());

	xml.add_tag("ldc",		long_distance_movement_cessation.count);
	xml.add_tag("ldbcc",	long_distance_movement_cessation.by_hand_excluded_count);
	xml.add_tag("ldmcc",	long_distance_movement_cessation.machine_excluded_count);
	xml.add_tag("ldcs",		long_distance_movement_cessation.censored_count);
	xml.add_tag("ldmw",		long_distance_movement_cessation.number_of_events_involving_multiple_worm_disambiguation);
	xml.add_tag("ldm",		long_distance_movement_cessation.mean );
	xml.add_tag("ldv",		long_distance_movement_cessation.variance);
	xml.add_tag("ldp10",	long_distance_movement_cessation.percentile_10th);
	xml.add_tag("ldp50",	long_distance_movement_cessation.percentile_50th );
	xml.add_tag("ldp90",	long_distance_movement_cessation.percentile_90th);
	xml.add_tag("ldmin",	long_distance_movement_cessation.minimum );
	xml.add_tag("ldmax",	long_distance_movement_cessation.maximum );
	
	xml.add_tag("loc",		local_movement_cessation.count);
	xml.add_tag("lomcc",	local_movement_cessation.machine_excluded_count);
	xml.add_tag("lobcc",	local_movement_cessation.by_hand_excluded_count);
	xml.add_tag("locs",		local_movement_cessation.censored_count);
	xml.add_tag("lomw",		local_movement_cessation.number_of_events_involving_multiple_worm_disambiguation);
	xml.add_tag("lom",		local_movement_cessation.mean );
	xml.add_tag("lov",		local_movement_cessation.variance );
	xml.add_tag("lop10",	local_movement_cessation.percentile_10th);
	xml.add_tag("lop50",	local_movement_cessation.percentile_50th );
	xml.add_tag("lop90",	local_movement_cessation.percentile_90th);
	xml.add_tag("lomin",	local_movement_cessation.minimum);
	xml.add_tag("lomax",	local_movement_cessation.maximum);


	xml.add_tag("dc",		death.count);
	xml.add_tag("dmcc",		death.machine_excluded_count);
	xml.add_tag("dbcc",		death.by_hand_excluded_count);
	xml.add_tag("dcs",		death.censored_count);
	xml.add_tag("dmw",		death.number_of_events_involving_multiple_worm_disambiguation);
	xml.add_tag("dm",		death.mean);
	xml.add_tag("dv",		death.variance );
	xml.add_tag("dp10",	death.percentile_10th);
	xml.add_tag("dp50",	death.percentile_50th );
	xml.add_tag("dp90",	death.percentile_90th );
	xml.add_tag("dmin",	death.minimum );
	xml.add_tag("dmax",	death.maximum);
	xml.end_group();
	return xml.result();
}

void ns_survival_data_quantities::out_jmp_header(const std::string & type, std::ostream & o, const std::string & terminator){
	o << type << " Mean,"<< type << " Variance," << type << " Coefficient of Variation,"
		<< type << " Count," << type << " Machine Exclusion Count," << type << " By Hand Exclusion Count," << type << " Censored Count,"
		#ifdef NS_OUTPUT_MULTIWORM_STATS
		<< type << " Number of Events Originating as Mutliple Worm Disambiguation Events, "
		#endif
		<< type << " 10th Percentile,"<< type << " 50th Percentile,"<< type << " 90th Percentile,"
		<< type << " 90-10 Interdecile Range," << type << " Normalized Interdecile Range,"
		<< type << " Maximum,"<< type << " Minimum" << terminator;
}
void ns_survival_data_quantities::out_jmp_data(const ns_region_metadata & metadata, std::ostream & o, const std::string & terminator) const{

	o << (mean-metadata.time_at_which_animals_had_zero_age)/(60*60*24) <<"," 
		<< variance/((60.0*60*24)*(60*60*24)) <<"," 
		<< (((mean-metadata.time_at_which_animals_had_zero_age)!=0)?(variance/(mean-metadata.time_at_which_animals_had_zero_age)):0) << ","
		<< count << "," 
		<< machine_excluded_count << "," 
		<< by_hand_excluded_count << "," 
		<< censored_count << "," 
		#ifdef NS_OUTPUT_MULTIWORM_STATS
		<< number_of_events_involving_multiple_worm_disambiguation << ","
		#endif
		<< (percentile_10th-metadata.time_at_which_animals_had_zero_age)/(60*60*24) << "," 
		<< (percentile_50th-metadata.time_at_which_animals_had_zero_age)/(60*60*24) << "," 
		<< (percentile_90th-metadata.time_at_which_animals_had_zero_age)/(60*60*24) << ","
		<< (percentile_90th-percentile_10th)/(60*60*24) << ","
		<< (((percentile_50th-metadata.time_at_which_animals_had_zero_age)!=0)?
			((percentile_90th-percentile_10th)/(percentile_50th-metadata.time_at_which_animals_had_zero_age)):0) << ","
		<<(maximum-metadata.time_at_which_animals_had_zero_age)/(60*60*24) << "," 
		<< (minimum-metadata.time_at_which_animals_had_zero_age)/(60*60*24) << terminator;
}


void ns_survival_data_quantities::out_blank_jmp_data(std::ostream & o, const std::string & terminator){
	o  <<"," 
		 "," 
		 "," 
		 "," 
		 "," 
		 "," 
		 ","
		 "," 
		 << terminator;
}

void ns_survival_data_summary::out_jmp_header(const std::string & label,std::ostream & o, const std::string & terminator){
	ns_survival_data_quantities::out_jmp_header(std::string("Death (") + label + ")",o,",");
	ns_survival_data_quantities::out_jmp_header(std::string("Long Distance Movement Cessation (") + label + ")",o,",");
	ns_survival_data_quantities::out_jmp_header(std::string("Local Movement Cessation (") + label + ")",o,terminator);
}

void ns_survival_data_summary::out_jmp_data(std::ostream & o, const std::string & terminator) const{
	death.out_jmp_data(metadata,o,",");
	long_distance_movement_cessation.out_jmp_data(metadata,o,",");
	local_movement_cessation.out_jmp_data(metadata,o,terminator);
}


void ns_survival_data_summary::out_blank_jmp_data(std::ostream & o, const std::string & terminator){
	ns_survival_data_quantities::out_blank_jmp_data(o,",");
	ns_survival_data_quantities::out_blank_jmp_data(o,",");
	ns_survival_data_quantities::out_blank_jmp_data(o,terminator);
}
/*
void ns_lifespan_experiment_set::output_xml_summary_file(std::ostream & o) const{
	ns_xml_simple_writer xml;
	xml.add_header();
	for (unsigned int i = 0; i < curves.size(); i++){
		ns_survival_data_summary s(curves[i]->produce_summary());
		xml.add_raw(s.to_xml());
	}
	xml.add_footer();
	o << xml.result();
}
void ns_lifespan_experiment_set::output_JMP_summary_file(std::ostream & o) const{
	//ns_region_metadata::out_JMP_plate_identity_header(o);
	ns_survival_data_summary::out_jmp_header("",o,"\n");

	for (unsigned int i = 0; i < curves.size(); i++){
		ns_survival_data_summary s(curves[i]->produce_summary());
	//	curves[i]->metadata.out_JMP_plate_identity_data(o);
		s.out_jmp_data(o);
	}
}
*/
const ns_survival_timepoint_event& ns_get_correct_event(const ns_metadata_worm_properties::ns_survival_event_type & type, const ns_survival_timepoint& e) {
	switch(type){
		case ns_metadata_worm_properties::ns_movement_based_death: return e.movement_based_deaths;
		case ns_metadata_worm_properties::ns_best_guess_death: return e.best_guess_deaths;
		case ns_metadata_worm_properties::ns_local_movement_cessation: return e.local_movement_cessations;
		case ns_metadata_worm_properties::ns_long_distance_movement_cessation: return e.long_distance_movement_cessations;
		case ns_metadata_worm_properties::ns_death_associated_expansion: return e.death_associated_expansions;
		case ns_metadata_worm_properties::ns_typeless_censoring_events: return e.typeless_censoring_events;
		default: throw ns_ex("ns_get_correct_event()::Unknown event spec");
	}
}

ns_survival_timepoint_event & ns_get_correct_event(const ns_metadata_worm_properties::ns_survival_event_type & type, ns_survival_timepoint & e){
	switch(type){
			case ns_metadata_worm_properties::ns_movement_based_death: return e.movement_based_deaths;
			case ns_metadata_worm_properties::ns_best_guess_death: return e.best_guess_deaths;
			case ns_metadata_worm_properties::ns_local_movement_cessation: return e.local_movement_cessations;
			case ns_metadata_worm_properties::ns_long_distance_movement_cessation: return e.long_distance_movement_cessations;
			case ns_metadata_worm_properties::ns_death_associated_expansion: return e.death_associated_expansions;
			case ns_metadata_worm_properties::ns_typeless_censoring_events: return e.typeless_censoring_events;
		default: throw ns_ex("ns_get_correct_event()::Unknown event spec");
	}
}

double ns_lifespan_device_normalization_statistics::calculate_additive_device_regression_residual(const double measurement) const{
	//if (this->external_fix_point_specified())
	//	return measurement - additive_device_regression_coefficient - grand_strain_mean + control_mean_external_fix_point;

	return measurement - (double)additive_device_regression_coefficient;
}
double ns_lifespan_device_normalization_statistics::calculate_multiplicative_device_regression_residual(const double measurement) const{
//	if (this->external_fix_point_specified())
	//	return measurement/multiplicative_device_regression_coefficient/grand_strain_mean*control_mean_external_fix_point;
	return measurement/multiplicative_device_regression_coefficient;
}
double ns_lifespan_device_normalization_statistics::calculate_multiplicative_device_regression_residual_additive_offset(const double measurement) const{
	//if (external_fix_point_specified())
	//return (measurement-multiplicative_additive_device_regression_additive_coefficient)/multiplicative_additive_device_regression_multiplicative_coefficient
	//		+ control_mean_external_fix_point;

	return 
		(measurement-multiplicative_additive_device_regression_additive_coefficient)/multiplicative_additive_device_regression_multiplicative_coefficient
			+ grand_strain_mean_used;
}


ns_death_time_annotation_time_interval ns_lifespan_device_normalization_statistics::calculate_additive_device_regression_residual(const ns_death_time_annotation_time_interval & e) const{
	ns_death_time_annotation_time_interval r(e);
	//implicit rounding to the nearest second.
	{
		double start(calculate_additive_device_regression_residual(e.period_start));
		if (start >= 0)
			r.period_start = start;
		else{
			r.period_start_was_not_observed = true;
			r.period_start = 0;
		}	
	}
	{
		double end(calculate_additive_device_regression_residual(e.period_end));
		if (end >= 0)
			r.period_end = end;
		else{
			r.period_end_was_not_observed = true;
			r.period_end = 0;
		}	

	}
	//r.period_end = calculate_additive_device_regression_residual(e.period_end);
	return r;
}
ns_death_time_annotation_time_interval ns_lifespan_device_normalization_statistics::calculate_multiplicative_device_regression_residual(const ns_death_time_annotation_time_interval& e) const{
	ns_death_time_annotation_time_interval r(e);
	//implicit rounding to the nearest second.
	r.period_start = calculate_multiplicative_device_regression_residual(e.period_start);
	r.period_end = calculate_multiplicative_device_regression_residual(e.period_end);
	return r;
}
ns_death_time_annotation_time_interval ns_lifespan_device_normalization_statistics::calculate_multiplicative_device_regression_residual_additive_offset(const ns_death_time_annotation_time_interval& e) const{
	ns_death_time_annotation_time_interval r(e);
	//implicit rounding to the nearest second.
	r.period_start = calculate_multiplicative_device_regression_residual_additive_offset(e.period_start);
	r.period_end = calculate_multiplicative_device_regression_residual_additive_offset(e.period_end);
	return r;
}




void ns_survival_timepoint_event::add(const ns_survival_timepoint_event_count & e){
	//look for existing event types that match
	/*for (unsigned int i = 0; i < events.size(); i++){
		if (events[i].number_of_worms_in_machine_worm_cluster == e.number_of_worms_in_machine_worm_cluster &&
			events[i].from_multiple_worm_disambiguation == e.from_multiple_worm_disambiguation &&
			events[i].number_of_worms_in_by_hand_worm_annotation == e.number_of_worms_in_by_hand_worm_annotation
			&& events[i].properties.multiworm_censoring_strategy == e.properties.multiworm_censoring_strategy
			&& events[i].properties.missing_worm_return_strategy == e.properties.missing_worm_return_strategy
			&& (events[i].properties.excluded == e.properties.excluded || 
				events[i].properties.is_excluded() && e.properties.is_excluded())){
					events[i].events.insert(events[i].events.end(),e.events.begin(),e.events.end());
				events[i].number_of_clusters_identified_by_machine+=e.number_of_clusters_identified_by_machine;
				events[i].number_of_clusters_identified_by_hand+=e.number_of_clusters_identified_by_hand;
			return;
		}
	}*/
	//make a new event type if one doesn't exist
	events.resize(events.size()+1,e);
}

void ns_lifespan_experiment_set::out_detailed_JMP_header(const ns_time_handing_behavior & time_handling_behavior,std::ostream & o, const std::string & time_units,const std::string & terminator){
	ns_region_metadata::out_JMP_plate_identity_header(o);
	o << ",Event Frequency,";
	if (time_handling_behavior == ns_output_single_event_times){
		o<< "Age at Death (" << time_units << "),";
	//	 "Age at Death (" << time_units << ") Additive Regression Model Residuals,"
		// "Age at Death (" << time_units << ") Multiplicative Regression Model Residuals,";
	}
	else{
		o<< "Age at Death (" << time_units << ") Start ,"
		<< "Age at Death (" << time_units << ") End ,";
		// "Age at Death (" << time_units << ") Additive Regression Model Residuals Start,"
		// "Age at Death (" << time_units << ") Additive Regression Model Residuals End,"
		// "Age at Death (" << time_units << ") Multiplicative Regression Model Residuals Start,"
		// "Age at Death (" << time_units << ") Multiplicative Regression Model Residuals End,";
	}
	o << "Duration Not fast moving (" << time_units << "), Censored,Censoring Reason,Censoring Strategy::Missing Worm Return strategy,Excluded,Event Observation Type,Size of Machine-Annotated Worm Cluster,Size of by-hand Annotated Worm Cluster,Event Type,Technique,Analysis Type,loglikelihood,Flags,"
	#ifdef NS_OUTPUT_MULTIWORM_STATS
	"Originated as a Mutliworm Disambiguation Result,"
	#endif 
	<< "By Hand Annotation Strategy,"
	"Details,"
	"Animal Center X,Animal Center Y,"
	"Plate subregion id,Plate subregion nearest neighbor ID,Plate subregion nearest neighbor distance X,Plate subregion nearest neighbor distance Y"
	<< terminator;
}

void ns_output_JMP_time_interval(const ns_lifespan_experiment_set::ns_time_handing_behavior & time_handling_behavior, 
								const ns_death_time_annotation_time_interval & e, 
								const double time_scaling_factor,
								std::ostream & o){
	if (time_handling_behavior == ns_lifespan_experiment_set::ns_output_single_event_times){
		o  << e.best_estimate_event_time_for_possible_partially_unbounded_interval()/time_scaling_factor;
		if (e.best_estimate_event_time_for_possible_partially_unbounded_interval() / time_scaling_factor > 100)
			cerr << "WHA";
		return;
	}
	if (!e.period_start_was_not_observed)
		o << e.period_start/time_scaling_factor;
	o << ",";
	if (!e.period_end_was_not_observed)
		o << e.period_end/time_scaling_factor;
}
ns_death_time_annotation_time_interval operator-(ns_death_time_annotation_time_interval i,const unsigned long r){
	i.period_start-=r;
	i.period_end-=r;
	return i;
}
void ns_region_metadata::out_JMP_plate_identity_data(std::ostream & o) const{
		
	o << device << ","  << experiment_name << "," << plate_name()   << ",";
	o << out_proc(plate_type_summary());
	o << "," << plate_position() << ","
		<< region_name << ","  //row
		<< plate_column() << "," 
		<< center_position_of_region_on_scanner().x << "," << center_position_of_region_on_scanner().y << ","
		<< size.x << "," << size.y << ","
		<< strain << "," << out_proc(genotype) << "," << out_proc(strain_condition_1) << "," << out_proc(strain_condition_2) << ","
		<< out_proc(strain_condition_3) << "," 
		<< out_proc(culturing_temperature) << ","
		<< out_proc(experiment_temperature) << ","
		<< out_proc(food_source) << "," 
		<< out_proc(environmental_conditions) << ","
		<< out_proc(incubator_name) << ",";
	if(incubator_location_specified())
		o << incubator_column();
	o << ",";
	if(incubator_location_specified())
		o << incubator_shelf();
}

std::string ns_region_metadata::out_proc(const std::string & s){
	std::string a;
	a.reserve(s.size()+2);
	a+="\"";
	a+=s;
	a+="\"";
	return a;
	/*for (unsigned int i = 0; i < s.size(); i++){
		if (s[i] != ',')
			a+=s[i];
		else{
			a+= '\\';
			a+= ',';
		}
	}*/
}

void ns_lifespan_experiment_set::out_detailed_JMP_event_data(const ns_time_handing_behavior & time_handling_behavior, std::ostream & o,  const ns_lifespan_device_normalization_statistics * regression_stats,const ns_region_metadata & metadata,const ns_metadata_worm_properties & prop,const double time_scaling_factor,const std::string & terminator, const bool output_raw_data_as_regression, const bool output_full_censoring_detail){
	//const unsigned long event_count(prop.events.total_number_of_worms());
	for (unsigned int i = 0; i < prop.events->events.size(); i++){
		
		const ns_death_time_annotation & properties (prop.properties_override_set?prop.properties_override:prop.events->properties);
		ns_death_time_annotation a(prop.events->events[i]);

		if (time_handling_behavior == ns_output_single_event_times){
			if (a.time.period_start_was_not_observed && time_handling_behavior == ns_lifespan_experiment_set::ns_output_single_event_times){
				a.time.period_start = a.time.period_end;
				a.time.period_start_was_not_observed = false;
			//	cerr << "ns_lifespan_experiment_set::out_detailed_JMP_event_data()::Placing an that started before first observation at the first event time.\n";
				//continue;
			}
			if (a.time.period_end_was_not_observed && time_handling_behavior == ns_lifespan_experiment_set::ns_output_single_event_times){
				cerr << "ns_lifespan_experiment_set::out_detailed_JMP_event_data()::Ignoring an event that continued after the end of the experiment.\n";
				continue;
			}
		}
		if(a.time.period_end <  metadata.time_at_which_animals_had_zero_age){
			cerr << "An event was identified as occuring before the specified time at which animals had age zero.  It has been ommitted\n";
			continue;
		}
			//	double event_time(a.time.best_estimate_event_time_within_interval() - metadata.time_at_which_animals_had_zero_age);
		metadata.out_JMP_plate_identity_data(o);
			o << "," << prop.events->number_of_worms_in_annotation(i) << ",";

	
		ns_output_JMP_time_interval(time_handling_behavior,a.time - metadata.time_at_which_animals_had_zero_age,
							time_scaling_factor,o);
		o << ",";
		o << a.volatile_duration_of_time_not_fast_moving/time_scaling_factor << ",";
		//event_time/time_scaling_factor << ",";
	
		/*if (output_raw_data_as_regression){
			ns_output_JMP_time_interval(time_handling_behavior,a.time - metadata.time_at_which_animals_had_zero_age,
							time_scaling_factor,o);
			o << ",";
			ns_output_JMP_time_interval(time_handling_behavior,a.time - metadata.time_at_which_animals_had_zero_age,
								time_scaling_factor,o);
			o << ",";
			/*o << event_time/time_scaling_factor << ","
				<< event_time/time_scaling_factor << ","
				<< event_time/time_scaling_factor << ",";
		}

		if (regression_stats == 0)
			o << ",,";
		else {
			if (regression_stats->additive_device_regression_coefficient != 0){
				ns_death_time_annotation_time_interval 
					reg(regression_stats->calculate_additive_device_regression_residual(a.time- metadata.time_at_which_animals_had_zero_age));
				if (reg.fully_unbounded()){
					cerr << "Additive Device Regression pushed death before time 0\n";
					reg.period_end_was_not_observed = false;
					reg.period_end = metadata.time_at_which_animals_had_zero_age; 
				}
				ns_output_JMP_time_interval(time_handling_behavior,reg,time_scaling_factor,o);
			}
			else if (time_handling_behavior == ns_lifespan_experiment_set::ns_output_event_intervals) o << ",";
			o << ",";

			//o << regression_stats->calculate_additive_device_regression_residual(event_time)/time_scaling_factor << ",";
			if (regression_stats->multiplicative_device_regression_coefficient != 0)
				ns_output_JMP_time_interval(time_handling_behavior,regression_stats->calculate_multiplicative_device_regression_residual(a.time)- metadata.time_at_which_animals_had_zero_age,time_scaling_factor,o);
			else if (time_handling_behavior == ns_lifespan_experiment_set::ns_output_event_intervals) o<<",";
			//	o << regression_stats->calculate_multiplicative_device_regression_residual(event_time)/time_scaling_factor << ",";
			o << ",";

		}
		//	"Censored,Excluded, Size of Machine-Annotated Worm Cluster,Size of by-hand Annotated Worm Cluster,Event Type,Technique,Analysis Type,Flags,"
	*/
		o << (properties.is_censored()?"1":"0") << ",";
		if(properties.is_censored() )
			o << properties.censor_description() << ",";
		else o << ",";
	
	
		//if we have a multiworm event, output that it should only bec included under
		//the censoring strategy where multiple worms are included as deaths.
		if (!properties.is_censored() && 
			(prop.events->number_of_worms_in_by_hand_worm_annotation > 1||
					ns_trust_machine_multiworm_data && prop.events->number_of_worms_in_machine_worm_cluster> 1)){
			o << ns_death_time_annotation::multiworm_censoring_strategy_label(ns_death_time_annotation::ns_include_as_single_worm_deaths) << "::"
				<< ns_death_time_annotation::missing_worm_return_strategy_label(properties.missing_worm_return_strategy) << ",";
		}
		//if we have a censoring annotation, output what strategy it was included as
		else if (output_full_censoring_detail || properties.is_censored()){
			o << ns_death_time_annotation::multiworm_censoring_strategy_label(properties.multiworm_censoring_strategy) << "::"
				<< ns_death_time_annotation::missing_worm_return_strategy_label(properties.missing_worm_return_strategy) << ",";
		}
		else o << ",";

		o << (properties.is_excluded()?ns_death_time_annotation::exclusion_value(properties.excluded):0) <<","
			<< ns_death_time_annotation::event_observation_label(properties.event_observation_type) << ","
			<< prop.events->number_of_worms_in_machine_worm_cluster << ","
			<< prop.events->number_of_worms_in_by_hand_worm_annotation << ","
			<< ns_metadata_worm_properties::event_type_to_string(prop.event_type) << ","
			<< metadata.technique << ","
			<< metadata.analysis_type << ","
			<< properties.loglikelihood << ","
			<< prop.flag.label() << ","
			#ifdef NS_OUTPUT_MULTIWORM_STATS
			<< (prop.events->from_multiple_worm_disambiguation?"multiple":"single") << ","
			#endif
			<< ns_death_time_annotation::by_hand_annotation_integration_strategy_label_short(a.by_hand_annotation_integration_strategy) << ","
			<< metadata.details <<","
			<< a.position.x << ","
			<< a.position.y << ","
			<< a.subregion_info.plate_subregion_id << ","
			<< a.subregion_info.nearest_neighbor_subregion_id << ","
			<< a.subregion_info.nearest_neighbor_subregion_distance.x << ","
			<< a.subregion_info.nearest_neighbor_subregion_distance.y
			<< terminator;
	}
}

void ns_lifespan_experiment_set::out_simple_JMP_header(const ns_time_handing_behavior & time_handling_behavior, const ns_control_group_behavior & control_group_behavior,std::ostream & o, const std::string & time_units, bool multiple_events,const std::string & terminator){

	ns_region_metadata::out_JMP_plate_identity_header(o);	
	if (control_group_behavior == ns_include_control_groups)
		o << ",Control Group";
	o << ",Event Frequency,"
		"Animal ID,";
	if (time_handling_behavior == ns_output_single_event_times){
		o <<
			"Age at Death (" << time_units << "),"
			"Age at Final  Movement (" << time_units << "),"
			"Age at Death-Associated Expansion (" << time_units << "),"
			"Duration between Expansion and Final Movement (" << time_units << "),"
			"Duration Not Fast Moving (" << time_units << "),"
			"Longest Gap in Measurement (" << time_units << "),";
	}
	else{
		o <<
			"Age at Death (" << time_units << ") Start,"
			"Age at Death (" << time_units << ") End,"
			"Age at Final Movement(" << time_units << ") Start,"
			"Age at Final Movement(" << time_units << ") End,"
			"Age at Death-Associated Expansion (" << time_units << ") Start,"
			"Age at Death-Associated Expansion (" << time_units << ") End,"
			"Duration between Expansion and Final Movement (" << time_units << ")"
			"Duration Not Fast Moving (" << time_units << "),"
			"Longest Gap in Measurement (" << time_units << "),";
	}
		// "Age at Death (" << time_units << ") Multiplicative + Additive offset Regression Model Residuals,"
	o << "Censored,Censored Reason,Event Observation Type,Annotation Source,Technique,Analysis Type";
	if (multiple_events)
		o << ",Event Type, Special Flag";
	o << terminator;
}


void ns_lifespan_experiment_set::out_simple_JMP_event_data(const ns_time_handing_behavior & time_handling_behavior, const ns_control_group_behavior & control_group_behavior,const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_strategy,const ns_death_time_annotation::ns_multiworm_censoring_strategy & s, const ns_death_time_annotation::ns_missing_worm_return_strategy & w, std::ostream & o,  const ns_lifespan_device_normalization_statistics * regression_stats,const ns_region_metadata & metadata,const ns_metadata_worm_properties & prop,const double time_scaling_factor,const bool output_mulitple_events,const std::string & terminator, const bool output_raw_data_as_regression){
	const ns_death_time_annotation & properties (prop.properties_override_set?prop.properties_override:prop.events->properties);
	
	if (properties.is_excluded())
		return;
	for (unsigned int i = 0; i < prop.events->events.size(); i++){
		ns_death_time_annotation  a(prop.events->events[i]);

		std::string relaxation_vs_movement_death_times = "";
		if (!a.volatile_time_at_death_associated_expansion_start.fully_unbounded())
			relaxation_vs_movement_death_times = ns_to_string((a.volatile_time_at_death_associated_expansion_start.best_estimate_event_time_for_possible_partially_unbounded_interval() - a.time.best_estimate_event_time_for_possible_partially_unbounded_interval())/time_scaling_factor);

		//we indicate right censoring via outputting a blank value for the end time
		//of the interval during which the animal dies
		if (properties.is_censored() && time_handling_behavior == ns_output_event_intervals)
			a.time.period_end_was_not_observed = true;
		if (!a.is_censored() && a.multiworm_censoring_strategy == ns_death_time_annotation::ns_unknown_multiworm_cluster_strategy)
			continue;
		if (!(
			a.is_censored() && a.disambiguation_type != ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster
			||
			//animals that were stationary but alive at the end of an experiment
			a.type == ns_movement_cessation && a.excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment

			||
			(
				//animals explicitly entered as by-hand censored
				a.multiworm_censoring_strategy == ns_death_time_annotation::ns_by_hand_censoring
				||
				//events for which no sensoring strategy is needed
				a.multiworm_censoring_strategy == ns_death_time_annotation::ns_no_strategy_needed_for_single_worm_object
				||
				//animals for which the specified censoring strategy was used
				a.multiworm_censoring_strategy == s
				)
			&&
			a.by_hand_annotation_integration_strategy == by_hand_strategy
			&&
			(a.missing_worm_return_strategy == w
				||
				a.missing_worm_return_strategy == ns_death_time_annotation::ns_not_specified
				)

			)
			) {
			continue;
		}

		//don't include censoring events for fast moving worms that go missing.
		if (a.type != ns_movement_cessation && a.type != ns_death_associated_expansion_start && a.excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment) {
			continue;
		}
		if (time_handling_behavior == ns_output_single_event_times){
			if (a.time.period_start_was_not_observed){	
				a.time.period_start = a.time.period_end;
				a.time.period_start_was_not_observed = false;
			//	cerr << "ns_lifespan_experiment_set::out_detailed_JMP_event_data()::Placing an event that started before first observation at the first event time.\n";
				//continue;
			}
			if (!properties.is_censored() && a.time.period_end_was_not_observed){
				continue;
			}
		}
	//	double event_time(a.time.best_estimate_event_time_within_interval() - metadata.time_at_which_animals_had_zero_age);

		
		metadata.out_JMP_plate_identity_data(o);
		if (control_group_behavior==ns_include_control_groups){
			if (prop.control_group == -1)
				throw ns_ex("ns_lifespan_experiment_set::out_detailed_JMP_event_data()::Encountered an unspecified control group");
			else o << "," << prop.control_group << ",";
		}
		else o << ",";
		o << prop.events->number_of_worms_in_annotation(i) << ",";
		o << a.stationary_path_id.group_id << ",";
		bool already_zeroed = false;
		//#1 for the "Death Time", use the death-associated expansion time if possible, and movement cessation otherwise.
		ns_death_time_annotation_time_interval * best_guess = ns_dying_animal_description_group<ns_death_time_annotation_time_interval>::calculate_best_guess_death_annotation(&a.time, &a.volatile_time_at_death_associated_expansion_start);
		
		if (best_guess->best_estimate_event_time_for_possible_partially_unbounded_interval() < metadata.time_at_which_animals_had_zero_age)
			{if (!already_zeroed) { already_zeroed = true; cout << "Skipping zero-ed death time! This suggests an incorrectly set time-at-which-animals-had-zero-age or a software bug.\n"; }}
		else
		ns_output_JMP_time_interval(time_handling_behavior, *best_guess - metadata.time_at_which_animals_had_zero_age,
			time_scaling_factor, o);
		
		o << ",";
		//#2 output movement cessation time (this is the only death time available in the old threshold algorithm)
		if (a.time.best_estimate_event_time_for_possible_partially_unbounded_interval() < metadata.time_at_which_animals_had_zero_age)
				{if (!already_zeroed) { already_zeroed = true; cout << "Skipping zero-ed death time! This suggests an incorrectly set time-at-which-animals-had-zero-age or a software bug.\n"; }}
		else
		ns_output_JMP_time_interval(time_handling_behavior, a.time - metadata.time_at_which_animals_had_zero_age,
			time_scaling_factor, o);
		o << ",";
		//#3 output death-associated expansion time		
		//we make sure for inferred censoring events to use the movement-based death time for both movement and expansion based event times as only the former is defined.
		if (a.disambiguation_type == ns_death_time_annotation::ns_inferred_censoring_event) {
			if (a.time.best_estimate_event_time_for_possible_partially_unbounded_interval() < metadata.time_at_which_animals_had_zero_age)
				{if (!already_zeroed) { already_zeroed = true; cout << "Skipping zero-ed death time! This suggests an incorrectly set time-at-which-animals-had-zero-age or a software bug.\n"; }}
			else
			ns_output_JMP_time_interval(time_handling_behavior, a.time - metadata.time_at_which_animals_had_zero_age,
				time_scaling_factor, o);
		}else{
			//death-associated expansion timeif (a.time.best_estimate_event_time_for_possible_partially_unbounded_interval() < metadata.time_at_which_animals_had_zero_age)
			
			if (a.volatile_time_at_death_associated_expansion_start.fully_unbounded())
				o << (time_handling_behavior == ns_output_single_event_times ? "" : ",");
			else {
				if (a.volatile_time_at_death_associated_expansion_start.best_estimate_event_time_for_possible_partially_unbounded_interval() < metadata.time_at_which_animals_had_zero_age)
				{if (!already_zeroed) { already_zeroed = true; cout << "Skipping zero-ed death time! This suggests an incorrectly set time-at-which-animals-had-zero-age or a software bug.\n"; }}
				else
				ns_output_JMP_time_interval(time_handling_behavior, a.volatile_time_at_death_associated_expansion_start - metadata.time_at_which_animals_had_zero_age,
					time_scaling_factor, o);
			}
		}
		o << ",";
		o << relaxation_vs_movement_death_times << ",";
		o << a.volatile_duration_of_time_not_fast_moving/time_scaling_factor << ",";
		o << a.longest_gap_without_observation/time_scaling_factor << ",";	
			
			
		o << (properties.is_censored()?"1":"0") << ",";
		if (properties.is_censored())
			o << properties.censor_description();
		o << "," 
			<< ns_death_time_annotation::event_observation_label(a.event_observation_type) << ","
			<< a.source_type_to_string(a.annotation_source) << ","
			<< metadata.technique << ","
			<< metadata.analysis_type << ",";
		if (output_mulitple_events)
			o <<  ns_metadata_worm_properties::event_type_to_string(prop.event_type) << ",";
		o << properties.flag.label();
		o << "\n";
	}
}
#ifndef NS_NO_SQL
void ns_genotype_fetcher::add_information_to_database(const std::vector<ns_genotype_db_info> & info,ns_image_server_sql * sql){
	load_from_db(sql,true);
	for (unsigned int i = 0; i < info.size(); i++){
		ns_genotype_list::iterator p = genotypes.find(info[i].strain);
		if (p == genotypes.end()){
			*sql << "INSERT INTO strain_aliases SET strain='" << sql->escape_string(info[i].strain) 
			     << "', genotype='" << sql->escape_string(info[i].genotype) << "' conditions=''";
			//cout << sql->query() << "\n";
			sql->send_query();
		}
		else{
			if (p->second.genotype == info[i].genotype)
				continue;
			*sql << "UPDATE strain_aliases SET  strain='" << sql->escape_string(info[i].strain) << "', genotype='" << sql->escape_string(info[i].genotype) << "'" <<
				" WHERE id = " << p->second.id;
		//	cout << sql->query() << "\n";
			sql->send_query();
		}
	}

}
#endif


void ns_lifespan_experiment_set::output_JMP_file(const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_strategy,const ns_lifespan_experiment_set::ns_time_handing_behavior & time_handling_behavior,const ns_time_units & time_units,std::ostream & o,const ns_output_file_type& detail, const bool output_header) const{
	
	if (detail == ns_detailed_with_censoring_repeats)
		throw ns_ex("Depreciated output file");
	std::string time_unit_string;
	double time_scaling_factor;
	switch(time_units){
		case ns_seconds:
			time_unit_string = "s";
			time_scaling_factor = 1;
			break;
		case ns_minutes:
			time_unit_string = "m";
			time_scaling_factor = 60;
			break;
		case ns_hours:
			time_unit_string = "h";
			time_scaling_factor = 60*60;
			break;
		case ns_days:
			time_unit_string = "d";
			time_scaling_factor = 24*60*60;
			break;
		default: throw ns_ex("ns_lifespan_experiment_set::output_JMP_file::Invalid time unit type:") << (int)time_units;
	}
	if (output_header){
		switch(detail){
			case ns_lifespan_experiment_set::ns_detailed_compact:
			case ns_lifespan_experiment_set::ns_detailed_with_censoring_repeats:
				out_detailed_JMP_header(time_handling_behavior,o,time_unit_string,"\n");
				break;
			case ns_lifespan_experiment_set::ns_simple_with_control_groups:
				out_simple_JMP_header(time_handling_behavior,ns_include_control_groups,o,time_unit_string,"\n");
				break;
			case ns_multiple_events:
			case ns_lifespan_experiment_set::ns_simple:
				out_simple_JMP_header(time_handling_behavior,ns_do_not_include_control_groups,o,time_unit_string,"\n");
				break;
			default: throw ns_ex("Unknown Output Detail Level: ") << (int)detail;
		}
	}

	ns_metadata_worm_properties::ns_survival_event_type event_type[(int)ns_metadata_worm_properties::ns_number_of_event_types] = {
																ns_metadata_worm_properties::ns_movement_based_death,
																ns_metadata_worm_properties::ns_best_guess_death,
																ns_metadata_worm_properties::ns_local_movement_cessation, 
																ns_metadata_worm_properties::ns_long_distance_movement_cessation,
																ns_metadata_worm_properties::ns_death_associated_expansion,
																ns_metadata_worm_properties::ns_typeless_censoring_events
																};

	for (unsigned int event_t = 0; event_t < (int)ns_metadata_worm_properties::ns_number_of_event_types; event_t++){
		
		if ((detail != ns_multiple_events) &&  event_type[event_t] != ns_metadata_worm_properties::ns_movement_based_death && event_type[event_t] != ns_metadata_worm_properties::ns_typeless_censoring_events)
			continue;

		/*//we make a list of all censoring strategies included in the data set.
		std::map<ns_death_time_annotation::ns_multiworm_censoring_strategy,
			std::set<ns_death_time_annotation::ns_missing_worm_return_strategy> > censoring_strategies;

		if (detail == ns_detailed_with_censoring_repeats){
			for (unsigned int i = 0; i < curves.size(); i++){
				for (unsigned int j = 0; j < curves[i]->timepoints.size();j++){
					const ns_survival_timepoint_event & te(ns_get_correct_event(event_type[event_t],curves[i]->timepoints[j]));
					for (unsigned int k = 0; k < te.events.size(); k++){
						//ignore boring standard events
						if (!te.events[k].properties.is_censored() && !te.events[k].properties.is_excluded() &&
								te.events[k].properties.number_of_worms_at_location_marked_by_machine == 1)
								continue;

						censoring_strategies[te.events[k].properties.multiworm_censoring_strategy].insert(te.events[k].properties.missing_worm_return_strategy);
					}
				}
			}
		}*/
		
		const ns_lifespan_device_normalization_statistics_set & normalization_stats((
			(event_type[event_t] == ns_metadata_worm_properties::ns_best_guess_death)? normalization_stats_for_death:
				(
					((event_type[event_t] == ns_metadata_worm_properties::ns_local_movement_cessation)?
													normalization_stats_for_translation_cessation:
													normalization_stats_for_fast_movement_cessation)
														)
														)
														);
		
		const bool output_regression_parameters(!normalization_stats.devices.empty());
		const bool output_raw_data_as_normalization_data(normalization_stats.produce_identity);
		
		for (unsigned int i = 0; i < curves.size(); i++){
			
			const ns_lifespan_device_normalization_statistics * regression_stats(0);
			
			if (output_regression_parameters){
				ns_lifespan_device_normalization_statistics_set::ns_device_stats_list::const_iterator d(normalization_stats.devices.find(curves[i]->metadata.device));
				if (d == normalization_stats.devices.end())
					throw ns_ex("Could not find normalization stats for device");
				if (d->second.device_had_control_plates)
					regression_stats = &d->second.regression_statistics;
			}

			for (unsigned int j = 0; j < curves[i]->timepoints.size(); ++j){
				
				if (detail == ns_detailed_compact){
					
					//the old behavior here was to output all possibble interpretations of multi-worm clumps.
					//This is no longer done by default, as it was confusing to everyone and rarely if ever used correctly.
					const ns_survival_timepoint_event & te(ns_get_correct_event(event_type[event_t],curves[i]->timepoints[j]));
					for (unsigned int k = 0; k < te.events.size(); k++){
						//if you want to output all possible strategies, remove this "continue"
						if (te.events[k].properties.multiworm_censoring_strategy != ns_death_time_annotation::default_censoring_strategy() ||
							te.events[k].properties.missing_worm_return_strategy != ns_death_time_annotation::default_missing_return_strategy())
							continue;
						ns_metadata_worm_properties p;
						p.events = &te.events[k];
						p.event_period_end_time =  curves[i]->timepoints[j].absolute_time-curves[i]->metadata.time_at_which_animals_had_zero_age;
						p.event_type = event_type[event_t];
						p.flag = ns_death_time_annotation_flag::none();
						if (!p.events->empty()){
								out_detailed_JMP_event_data(time_handling_behavior,o,regression_stats,curves[i]->metadata,p,time_scaling_factor,"\n",output_raw_data_as_normalization_data);
						}
					}
				}/*
				if (detail == ns_detailed_with_censoring_repeats){
					//Note that the machine will output both censoring events 
					//and death events for multi-worm clusters.
					//the user should figure out which one is best and only use that.
					const ns_survival_timepoint_event & te(ns_get_correct_event(event_type[event_t],curves[i]->timepoints[j]));
					for (unsigned int k = 0; k < te.events.size(); k++){
						//don't output excluded events (for brevity's sake)
						if (te.events[k].properties.is_excluded())
							continue;
						ns_metadata_worm_properties p;
						p.events = & te.events[k];
						p.event_period_end_time = curves[i]->timepoints[j].absolute_time-curves[i]->metadata.time_at_which_animals_had_zero_age;
						p.event_type = event_type[event_t];
						p.flag = ns_death_time_annotation_flag::none();
						if (!p.events->empty() == 0)
							continue;
						//totally normal worm, not censoring or noise or a multiple worm clump
						//output it for each censoring strategy
						if (!p.events->properties.is_censored() && !p.events->properties.is_excluded() &&
							p.events->properties.number_of_worms_at_location_marked_by_machine == 1){
								
								ns_death_time_annotation::ns_multiworm_censoring_strategy mult(p.events->properties.multiworm_censoring_strategy);
								ns_death_time_annotation::ns_missing_worm_return_strategy miss(p.events->properties.missing_worm_return_strategy);
								for (std::map<ns_death_time_annotation::ns_multiworm_censoring_strategy,
										std::set<ns_death_time_annotation::ns_missing_worm_return_strategy> >::iterator mcs = censoring_strategies.begin();
										mcs!= censoring_strategies.end(); mcs++){
											p.properties_override = p.events->properties;
											p.properties_override_set = true;
											for (std::set<ns_death_time_annotation::ns_missing_worm_return_strategy>::iterator miss = mcs->second.begin(); miss != mcs->second.end(); miss++){
												p.properties_override.multiworm_censoring_strategy = mcs->first;
												p.properties_override.missing_worm_return_strategy = *miss;
												out_detailed_JMP_event_data(time_handling_behavior,o,regression_stats,curves[i]->metadata,p,time_scaling_factor,"\n",output_raw_data_as_normalization_data,true);
											}
								}
						}
						else					
						out_detailed_JMP_event_data(time_handling_behavior,o,regression_stats,curves[i]->metadata,p,time_scaling_factor,"\n",output_raw_data_as_normalization_data,true);

					}
				}*/
				else if (detail == ns_simple || detail == ns_multiple_events || detail == ns_simple_with_control_groups && regression_stats == 0){
					//simple
					//first we get all the events of the required type that occured at this time point
					const ns_survival_timepoint_event & te(ns_get_correct_event(event_type[event_t],curves[i]->timepoints[j]));
					//now we output each event one at a time
					for (unsigned int k = 0; k < te.events.size(); k++){
							ns_metadata_worm_properties p;
							p.events = &te.events[k];
							p.event_period_end_time =  curves[i]->timepoints[j].absolute_time-curves[i]->metadata.time_at_which_animals_had_zero_age;
							p.event_type = event_type[event_t];
							p.flag = ns_death_time_annotation_flag::none();
							p.control_group = 0;
							if (!p.events->empty()){
								const bool output_control_groups(false);
								out_simple_JMP_event_data(time_handling_behavior,output_control_groups?ns_include_control_groups:ns_do_not_include_control_groups,
															by_hand_strategy,
															ns_death_time_annotation::default_censoring_strategy(),
														  ns_death_time_annotation::default_missing_return_strategy(),
														o,regression_stats,curves[i]->metadata,p,
														time_scaling_factor,
														detail == ns_multiple_events,"\n",output_raw_data_as_normalization_data);
							}
						}
				}
				else if (detail == ns_simple_with_control_groups && regression_stats != 0){

					ns_lifespan_device_normalization_statistics_set::ns_control_group_plate_list::const_iterator control_group_info
						(normalization_stats.control_group_plate_list.find(curves[i]->metadata.region_id));
					if (control_group_info == normalization_stats.control_group_plate_list.end())
						throw ns_ex("Could not find plate information in device normalization statistics set");
					if (control_group_info->second.control_group_memberships.size() == 0)
						throw ns_ex("Encountered a plate with no group memberships");
					//output the controls multiple times, one for each mutant on the same scanner
					for (ns_control_group_plate_assignment::ns_control_group_membership_list::const_iterator g = control_group_info->second.control_group_memberships.begin(); g != control_group_info->second.control_group_memberships.end(); g++){

						const ns_survival_timepoint_event & te(ns_get_correct_event(event_type[event_t],curves[i]->timepoints[j]));
						for (unsigned int k = 0; k < te.events.size(); k++){
							ns_metadata_worm_properties p;
							p.events = &te.events[k];
							p.event_period_end_time =  curves[i]->timepoints[j].absolute_time-curves[i]->metadata.time_at_which_animals_had_zero_age;
							p.event_type = event_type[event_t];
							p.flag = ns_death_time_annotation_flag::none();
							p.control_group = *g;
							if (!p.events->empty()){
						/*		for (unsigned int l = 0; l < te.events[k].events.size(); l++){
									if (te.events[k].events[l].event_observation_type == ns_death_time_annotation::ns_induced_multiple_worm_death){
										cerr << "HA";
									}
								}*/
								out_simple_JMP_event_data(time_handling_behavior,ns_include_control_groups,
														by_hand_strategy,
														ns_death_time_annotation::default_censoring_strategy(),
														ns_death_time_annotation::default_missing_return_strategy(),
														o,regression_stats,curves[i]->metadata,p,
														time_scaling_factor,false,"\n",output_raw_data_as_normalization_data);
							}
						}
					}
				}
			}
		}
	}
}
void ns_lifespan_experiment_set::output_matlab_file(std::ostream & out) const{
	const string line_span("...");
	unsigned max_t;
	if (set_is_on_common_time()){
		max_t = common_time().size();
		out << "t= [";
		for (unsigned int i = 0; i < common_time().size(); ++i){
			if (i%100 == 0) out << line_span << "\n";
			out << common_time()[i] << " ";
		}
		out << line_span << "\n];\n\n";
	}
	else{
		
		max_t=0;
		for (unsigned int i = 0; i < curves.size(); i++){
			if (curves[i]->timepoints.size() > max_t)
				max_t = curves[i]->timepoints.size();
		}
		out << "t= [";

		for (unsigned int i = 0; i < curves.size(); i++){
			out << "[";
			for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
				out << curves[i]->timepoints[j].absolute_time << " ";
				if (j%100 == 0) out << line_span << "\n";
			}
			for (unsigned j = curves[i]->timepoints.size(); j < max_t; j++){
				out <<  curves[i]->timepoints[curves[i]->timepoints.size()-1].absolute_time + j - curves[i]->timepoints.size() << " ";
				if (j%100 == 0) out << line_span << "\n";
			}
			out << "]; " << line_span << "\n";
		}
		out << "];\n\n";
	}



	out << "number_not_dead = [";
	for (unsigned int i = 0; i < curves.size(); i++){
		//out << "diestributions{" << i+1 << "} = [";
		out << "[";
		for (unsigned int j = 0; j < curves[i]->risk_timeseries.best_guess_death.data.probability_of_surviving_up_to_interval.size(); j++){
			out << curves[i]->risk_timeseries.best_guess_death.data.number_surviving_excluding_censoring(j) << " ";
			if (j%100==0) out << line_span << "\n";
		}

		for (unsigned j = curves[i]->timepoints.size(); j < max_t; j++){
			out << "NaN ";
			if (j%100==0) out << line_span << "\n";
		}
		out << "]; " << line_span << "\n";
	}
	out << "];\n";

	out << "survival_including_censored = [";
	for (unsigned int i = 0; i < curves.size(); i++){
		//out << "diestributions{" << i+1 << "} = [";
		out << "[";
		for (unsigned int j = 0; j < curves[i]->risk_timeseries.best_guess_death.data.probability_of_surviving_up_to_interval.size(); j++){
			out << curves[i]->risk_timeseries.best_guess_death.data.probability_of_surviving_up_to_interval[j] << " ";
			if (j%100==0) out << line_span << "\n";
		}

		for (unsigned j = curves[i]->timepoints.size(); j < max_t; j++){
			out << "NaN ";
			if (j%100==0) out << line_span << "\n";
		}
		out << "]; " << line_span << "\n";
	}
	out << "];\n";

	out << "mortality = [";
	for (unsigned int i = 0; i < curves.size(); i++){
		//out << "diestributions{" << i+1 << "} = [";
		out << "[";
		for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
			out << curves[i]->risk_timeseries.best_guess_death.data.number_of_events[j] << " ";
			if (j%100==0) out << line_span << "\n";
		}

		for (unsigned j = curves[i]->timepoints.size(); j < max_t; j++){
			out << "NaN ";
			if (j%100==0) out << line_span << "\n";
		}
		out << "]; " << line_span << "\n";
	}
	out << "];\n";

	out << "mortality_normalized = [";
	for (unsigned int i = 0; i < curves.size(); i++){
		//out << "diestributions{" << i+1 << "} = [";
		out << "[";
		float base(curves[i]->risk_timeseries.best_guess_death.data.total_number_of_deaths);
		if (base == 0) base = 1;
		for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
			out << curves[i]->risk_timeseries.best_guess_death.data.number_of_events[j]/base << " ";
			if (j%100==0) out << line_span << "\n";
		}

		for (unsigned j = curves[i]->timepoints.size(); j < max_t; j++){
			out << "NaN ";
			if (j%100==0) out << line_span << "\n";
		}
		out << "]; " << line_span << "\n";
	}
	out << "];\n";

	out << "long_distance_movement_span_normalized = [";
	for (unsigned int i = 0; i < curves.size(); i++){
		//out << "diestributions{" << i+1 << "} = [";
		out << "[";
		float base(curves[i]->risk_timeseries.long_distance_movement_cessation.data.total_number_of_censoring_events);
		if (base == 0) base = 1;
		for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
			out << curves[i]->risk_timeseries.best_guess_death.data.number_of_events[j]/base << " ";
			if (j%100==0) out << line_span << "\n";
		}
		for (unsigned j = curves[i]->timepoints.size(); j < max_t; j++){
			out << "NaN ";
			if (j%100==0) out << line_span << "\n";
		}
		out << "]; " << line_span << "\n";
	}
	out << "];\n";

	out << "local_movement_span_normalized = [";
	for (unsigned int i = 0; i < curves.size(); i++){
		//out << "diestributions{" << i+1 << "} = [";
		out << "[";
		float base(curves[i]->risk_timeseries.local_movement_cessations.data.total_number_of_deaths);
		if (base == 0) base = 1;
		for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
			out << curves[i]->risk_timeseries.local_movement_cessations.data.number_of_events[j]/base << " ";
			if (j%100==0) out << line_span << "\n";
		}
		for (unsigned j = curves[i]->timepoints.size(); j < max_t; j++){
			out << "NaN ";
			if (j%100==0) out << line_span << "\n";
		}
		out << "]; " << line_span << "\n";
	}
	out << "];\n";

		
	out << "time_at_which_animals_had_zero_age= [";
	for (unsigned int i = 0; i < curves.size(); i++){
		out << curves[i]->metadata.time_at_which_animals_had_zero_age << " ";
		if (i%100==0) out << line_span << "\n";
	}
	out << "];\n\n";

	out << "strain = {";
	for (unsigned int i = 0; i < curves.size(); i++){
		out << "'" << curves[i]->metadata.strain << "', " << line_span << "\n";
	}
	out << "};\n\n";	

	out << "genotype = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.genotype << "', " << line_span << "\n";
	out << "};\n\n";

	out << "experiment_name = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.experiment_name << "', " << line_span << "\n";
	out << "};\n\n";

	out << "analysis_type = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.analysis_type << "', " << line_span << "\n";
	out << "};\n\n";	
	
	out << "technique = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.technique << "', " << line_span << "\n";
	out << "};\n\n";

	out << "details = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.details << "', " << line_span << "\n";
	out << "};\n\n";	
	out << "strain_condition_1 = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.strain_condition_1 << "', " << line_span << "\n";
	out << "};\n\n";	
	out << "strain_condition_2 = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.strain_condition_2 << "', " << line_span << "\n";
		out << "strain_condition_3 = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.strain_condition_3 << "', " << line_span << "\n";
		out << "culturing_temperature = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.culturing_temperature << "', " << line_span << "\n";
		out << "experiment_temperature = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.experiment_temperature << "', " << line_span << "\n";
		out << "food_source = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.food_source  << "', " << line_span << "\n";
		out << "environmental_conditions = {";
	for (unsigned int i = 0; i < curves.size(); i++)
		out << "'" << curves[i]->metadata.environmental_conditions << "', " << line_span << "\n";
	out << "};\n\n";
}
#ifndef NS_NO_SQL
void ns_lifespan_experiment_set::load_genotypes(ns_sql & sql){
	ns_genotype_fetcher fetcher;
	fetcher.load_from_db(&sql);
	for (unsigned int i = 0; i < curves.size(); ++i)
		curves[i]->metadata.genotype = fetcher.genotype_from_strain(curves[i]->metadata.strain,&sql);
	
}
#endif

void ns_lifespan_experiment_set::clear(){
	for (unsigned int i = 0; i < curves.size(); i++)
		memory_pool.release(curves[i]);
	curves.resize(0); 
	common_time_.clear(); 
}
void ns_lifespan_experiment_set::include_only_events_detected_by_machine(){
	for (unsigned int i = 0; i < curves.size(); ++i){
		for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
			curves[i]->timepoints[j].movement_based_deaths.remove_purely_non_machine_events();
			curves[i]->timepoints[j].best_guess_deaths.remove_purely_non_machine_events();
			curves[i]->timepoints[j].local_movement_cessations.remove_purely_non_machine_events();
			curves[i]->timepoints[j].long_distance_movement_cessations.remove_purely_non_machine_events();
			curves[i]->timepoints[j].death_associated_expansions.remove_purely_non_machine_events();
		}
	}
}
void ns_lifespan_experiment_set::generate_survival_statistics(){
	for (unsigned int i = 0; i < curves.size(); ++i)
		curves[i]->genenerate_survival_statistics();
}
struct ns_event_count{
	ns_event_count():number_of_events(0),number_of_censoring_events(0){}
	unsigned long
		number_of_events,
		number_of_censoring_events;
};

void ns_lifespan_experiment_set::generate_aggregate_risk_timeseries(const ns_region_metadata & m, bool filter_by_strain, const std::string & specific_device, const ns_64_bit & specific_region_id, ns_survival_data_with_censoring& best_guess_based_survival, ns_survival_data_with_censoring & movement_based_survival, ns_survival_data_with_censoring& death_associated_expansion_survival, std::vector<unsigned long> & timepoints,bool use_external_time) const{
	std::map<unsigned long, std::vector<ns_survival_timepoint_event> > best_guess_events, movement_death_events, death_associated_expansion_events;
	ns_region_metadata mm(m);
	mm.genotype.clear();
	
	ns_survival_data aggregate_set;
	for (unsigned int i = 0; i <this->curves.size(); i++){
		if (filter_by_strain && (mm.device_regression_match_description() != curves[i]->metadata.device_regression_match_description())
			|| specific_region_id != 0 && curves[i]->metadata.region_id != specific_region_id ||
			!specific_device.empty() && specific_device != curves[i]->metadata.device)
			continue;

		//add all events except for excluded objects
		for (unsigned long j = 0; j < curves[i]->timepoints.size(); j++){
			movement_death_events[curves[i]->timepoints[j].absolute_time].push_back(curves[i]->timepoints[j].movement_based_deaths);
			best_guess_events[curves[i]->timepoints[j].absolute_time].push_back(curves[i]->timepoints[j].best_guess_deaths);
			death_associated_expansion_events[curves[i]->timepoints[j].absolute_time].push_back(curves[i]->timepoints[j].death_associated_expansions);
		}
	}
	//we need to put both the deaths and the death associated expansion events on the same time steps.
	//so we find the union of all event times
	std::set<unsigned long> unique_timepoints;
	for (std::map<unsigned long, std::vector<ns_survival_timepoint_event> >::iterator p = movement_death_events.begin(); p != movement_death_events.end(); p++)
		unique_timepoints.insert(p->first);
	for (std::map<unsigned long, std::vector<ns_survival_timepoint_event> >::iterator p = best_guess_events.begin(); p != best_guess_events.end(); p++)
		unique_timepoints.insert(p->first);
	for (std::map<unsigned long, std::vector<ns_survival_timepoint_event> >::iterator p = death_associated_expansion_events.begin(); p != death_associated_expansion_events.end(); p++)
		unique_timepoints.insert(p->first);
	if (!use_external_time) {
		aggregate_set.timepoints.resize(unique_timepoints.size());
		timepoints.resize(unique_timepoints.size());
	}
	else {
		aggregate_set.timepoints.resize(timepoints.size());
		for (unsigned int i = 0; i < aggregate_set.timepoints.size(); i++)
			aggregate_set.timepoints[i].absolute_time = timepoints[i];
	}


	std::map<unsigned long, unsigned long> time_lookup;
	unsigned int i = 0;
	for (std::set<unsigned long >::iterator p = unique_timepoints.begin(); p != unique_timepoints.end(); p++) {
		if (!use_external_time) {
			timepoints[i] = *p;
			time_lookup[*p] = i;
			i++;
		}
		else {
			for (i; i < timepoints.size(); i++)
				if (timepoints[i] == *p) {
					time_lookup[*p] = i;
					break;
				}
			if (i == timepoints.size())
				throw ns_ex("ns_lifespan_experiment_set::generate_aggregate_risk_timeseries()::Could not find a timepoint in externally specified timepoint list");
		}
	}

	for (std::map<unsigned long,std::vector<ns_survival_timepoint_event> >::iterator p = movement_death_events.begin(); p != movement_death_events.end(); p++){
		const unsigned int pos = time_lookup[p->first];
		aggregate_set.timepoints[pos].absolute_time = p->first;
		for (unsigned int j = 0; j < p->second.size(); j++)
			aggregate_set.timepoints[pos].movement_based_deaths.add(p->second[j]);
	}
	movement_death_events.clear();
	for (std::map<unsigned long, std::vector<ns_survival_timepoint_event> >::iterator p = best_guess_events.begin(); p != best_guess_events.end(); p++) {
		const unsigned int pos = time_lookup[p->first];
		aggregate_set.timepoints[pos].absolute_time = p->first;
		for (unsigned int j = 0; j < p->second.size(); j++)
			aggregate_set.timepoints[pos].best_guess_deaths.add(p->second[j]);
	}
	best_guess_events.clear();
	for (std::map<unsigned long, std::vector<ns_survival_timepoint_event> >::iterator p = death_associated_expansion_events.begin(); p != death_associated_expansion_events.end(); p++) {
		const unsigned int pos = time_lookup[p->first];
		aggregate_set.timepoints[pos].absolute_time = p->first;
		for (unsigned int j = 0; j < p->second.size(); j++)
			aggregate_set.timepoints[pos].death_associated_expansions.add(p->second[j]);
	}
	death_associated_expansion_events.clear();
	aggregate_set.genenerate_survival_statistics();
	movement_based_survival = aggregate_set.risk_timeseries.movement_based_death;
	best_guess_based_survival = aggregate_set.risk_timeseries.best_guess_death;
	death_associated_expansion_survival = aggregate_set.risk_timeseries.death_associated_expansion_start;
}
void ns_lifespan_experiment_set::generate_common_time_set(ns_lifespan_experiment_set & new_set) const{

	//build up a list of all sample times and the curves that were measured at those times
	//std::map<a, std::vector< pair<b,c> > >
	//a: the time at which the sample was taken
	//b: std::vector<> a list of measurements taken for that time
	//c: the index of the strain in ex.curves[] of the experiment who's sample was taken
	//d: the index of the sample data in ex.curves[c].survival that records the result
	//   of the sample.
	new_set.clear();
	if (curves.size() == 0)
		return;
	std::set<unsigned long> all_points;
	for (unsigned int i = 0; i < curves.size(); i++){
		for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
			all_points.insert(all_points.begin(),curves[i]->timepoints[j].absolute_time);
		}
	}
	new_set.curves.resize(curves.size());
	for (unsigned int i = 0; i < new_set.curves.size(); i++)
		new_set.curves[i] = memory_pool.get(0);

	new_set.common_time_.resize(all_points.size());
	{
		unsigned long t(0);
		for (std::set<unsigned long>::iterator p = all_points.begin(); p != all_points.end(); p++){
			new_set.common_time()[t] = (*p);
			t++;
		}
	}
	for (unsigned int i = 0; i < curves.size(); i++){
		new_set.curves[i]->timepoints.resize(new_set.common_time().size());
		for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++)
			new_set.curves[i]->timepoints[j].absolute_time = new_set.common_time()[j];
		new_set.curves[i]->metadata = curves[i]->metadata;
	}
	
	for (unsigned int i = 0; i < curves.size(); i++){
		
		unsigned long new_curve_i(0);
		for (unsigned int old_curve_i = 0; old_curve_i < curves[i]->timepoints.size(); old_curve_i++){
			while (new_curve_i < new_set.common_time().size()
					&& curves[i]->timepoints[old_curve_i].absolute_time > new_set.common_time()[new_curve_i]){
					++new_curve_i;
			}

				
			if (new_curve_i > new_set.common_time().size())
				throw ns_ex("ns_lifespan_experiment_set::generate_common_time_set()::Found an event in curve # ") << i << "[" << old_curve_i << "] that occurred after the final timepoitn of the common time set";
			if (curves[i]->timepoints[old_curve_i].absolute_time != new_set.common_time()[new_curve_i])
				throw ns_ex("ns_lifespan_experiment_set::generate_common_time_set()::Found an event in curve # ") << i << "[" << old_curve_i << "] that occurred at a time not included in the common time set";
			
			new_set.curves[i]->timepoints[new_curve_i] = curves[i]->timepoints[old_curve_i];
		}
	}
}

//events that occured at a position detected by the machine have 
//a special annotation added during survival curve assembly.
//we check for that annotation
void ns_survival_timepoint_event::remove_purely_non_machine_events(){
	for (std::vector<ns_survival_timepoint_event_count>::iterator p = events.begin(); p != events.end();){
		bool found_machine(false);
		for (unsigned int i = 0; i < p->events.size(); i++){
			if (p->events[i].volatile_matches_machine_detected_death){
				found_machine = true;
				break;
			}
		}
		if (!found_machine)
			p = events.erase(p);
		else p++;
	}
}

void  ns_survival_timepoint_event::add(const ns_survival_timepoint_event & e) {
	events.insert(events.end(),e.events.begin(),e.events.end());
}
void ns_survival_timepoint::add(const ns_survival_timepoint & t){
	absolute_time = (absolute_time>t.absolute_time)?absolute_time:t.absolute_time;
	movement_based_deaths.add(t.movement_based_deaths);
	best_guess_deaths.add(t.best_guess_deaths);
	local_movement_cessations.add(t.local_movement_cessations);
	long_distance_movement_cessations.add(t.long_distance_movement_cessations);
	death_associated_expansions.add(t.death_associated_expansions);
	typeless_censoring_events.add(t.typeless_censoring_events);
}

void ns_lifespan_experiment_set::force_common_time_set_to_constant_time_interval(const unsigned long interval_time_in_seconds,ns_lifespan_experiment_set & new_set) const{
	if (!set_is_on_common_time())
		throw ns_ex("ns_lifespan_experiment_set::force_common_time_set_to_constant_time_interval()::Constant time intervals can only be set for experiment sets on a common time interval");
	
	if (interval_time_in_seconds == 0)
		throw ns_ex("ns_lifespan_experiment_set::force_common_time_set_to_constant_time_interval()::Cannot set constant time interval to zero!");
	
	new_set.clear();
	if (curves.size() == 0)
		return;

	unsigned long start_time = common_time()[0];
	unsigned long stop_time = common_time()[common_time().size()-1];
	unsigned long number_of_points = (stop_time - start_time)/interval_time_in_seconds + 1;
	
	new_set.common_time_.resize(number_of_points);
	for (unsigned int i = 0; i < new_set.common_time().size(); ++i)
		new_set.common_time()[i] = start_time+interval_time_in_seconds*i;
	
	new_set.curves.resize(0);
	new_set.curves.resize(curves.size());	

	for (unsigned int i = 0; i < curves.size(); ++i){
		new_set.curves[i] = memory_pool.get(0);
		new_set.curves[i]->timepoints.resize(number_of_points);
		new_set.curves[i]->metadata = curves[i]->metadata;

		unsigned long old_curve_i = 0;
		
		for (std::vector<unsigned long>::size_type new_curve_i = 0; new_curve_i < new_set.common_time().size(); ++new_curve_i){
			new_set.curves[i]->timepoints[new_curve_i].absolute_time = new_set.common_time()[new_curve_i];
			if (old_curve_i >= curves[i]->timepoints.size() || curves[i]->timepoints[old_curve_i].absolute_time > new_set.common_time()[new_curve_i])
				continue;

			ns_survival_timepoint events_that_have_occurred;
			for(;old_curve_i < curves[i]->timepoints.size() &&
				curves[i]->timepoints[old_curve_i].absolute_time < new_set.common_time()[new_curve_i];old_curve_i++){
					if (new_curve_i==0)
						throw ns_ex("Found events before first event in new curve!");
				events_that_have_occurred.add(curves[i]->timepoints[old_curve_i]);
			}
			if (new_curve_i==0)
				continue;
			events_that_have_occurred.absolute_time = new_set.common_time()[new_curve_i-1];
			new_set.curves[i]->timepoints[new_curve_i-1] = events_that_have_occurred;
		}
		new_set.curves[i]->genenerate_survival_statistics();
	}
	new_set.curves_on_constant_time_interval = true;
	new_set.normalization_stats_for_death = normalization_stats_for_death;
	new_set.normalization_stats_for_fast_movement_cessation = normalization_stats_for_fast_movement_cessation;
	new_set.normalization_stats_for_translation_cessation = normalization_stats_for_translation_cessation;
}


void ns_explode_llist(const std::string & s, vector<std::string> & vals){
	string cur;
	for (unsigned int i = 0; i < s.size(); i++){
			if (s[i] == ';'){
				vals.push_back(cur);
				cur.resize(0);
			}
			else cur+=s[i];
		}
		vals.push_back(cur);
};

#ifndef NS_NO_SQL
void ns_device_temperature_normalization_data::load_data_for_experiment(const unsigned long experiment_id,ns_sql & sql){
	control_strains.resize(0);
	sql<< "SELECT control_strain_for_device_regression FROM experiments WHERE id = " << experiment_id;
	ns_sql_result control_strain;
	sql.get_rows(control_strain);
		
	if (control_strain.empty())
		return;
	strain_description_detail_type = ns_region_metadata::ns_strain_and_conditions_1_2_and_3;	
	//external_fixed_control_mean_lifespan;
	fix_control_mean_lifespan = false;

	std::vector<string> exp;
	ns_explode_llist(control_strain[0][0],exp);
	if (exp.empty())
		return;
	strain_description_detail_type = ns_region_metadata::description_detail_type_from_string(exp[0]);
	if (exp.size() <= 1)
		return;

	control_strains.resize(exp.size()-2);

	for (unsigned int i = 1; i < exp.size()-1; i++){
		ns_64_bit control_strain_region_id(ns_atoi64(exp[i].c_str()));
		if (control_strain_region_id == 0)
			throw ns_ex("Could not parse experiment strain specification");
		control_strains[i-1].load_from_db(control_strain_region_id,"",sql);
	}
	if (exp.rbegin()->size() > 0){
		double f(atof(exp.rbegin()->c_str()));
		if (f > 0){
			fix_control_mean_lifespan = true;
			external_fixed_control_mean_lifespan = f*24*60*60; //convert days to seconds
		}
	}
}
#endif
void ns_survival_data_quantities::add(const ns_survival_data_quantities & s){
	count += s.count;
	number_of_events_involving_multiple_worm_disambiguation += s.number_of_events_involving_multiple_worm_disambiguation;
	by_hand_excluded_count += s.by_hand_excluded_count;
	machine_excluded_count += s.machine_excluded_count;
	censored_count += s.censored_count;
	mean += s.mean;
	variance += s.variance;
	percentile_90th += s.percentile_90th;
	percentile_50th += s.percentile_50th;
	percentile_10th += s.percentile_10th;
	if (s.maximum > maximum)
		maximum = s.maximum;	
	if (s.minimum < minimum)
		minimum= s.minimum;
}
void ns_survival_data_summary::add(const ns_survival_data_summary & s){
	death.add(s.death);
	local_movement_cessation.add(s.local_movement_cessation);
	long_distance_movement_cessation.add(s.long_distance_movement_cessation);
}
struct ns_death_stat_key{
	ns_death_stat_key(const ns_death_stat_key & key){
		*this = key;
	}
	ns_death_stat_key(const ns_region_metadata & m):metadata(m){}

	ns_region_metadata metadata;
};
bool operator<(const ns_death_stat_key & a, const ns_death_stat_key & b){
	if (a.metadata.device != b.metadata.device) return a.metadata.device < b.metadata.device;
	if (a.metadata.technique != b.metadata.technique) return a.metadata.technique < b.metadata.technique;
	if (a.metadata.strain != b.metadata.strain) return a.metadata.strain < b.metadata.strain;
	if (a.metadata.strain_condition_1 != b.metadata.strain_condition_1) return a.metadata.strain_condition_1 < b.metadata.strain_condition_1;
	//if (a.metadata.strain_condition_2 != b.metadata.strain_condition_2)
	return a.metadata.strain_condition_2 < b.metadata.strain_condition_2;
	/*if (a.metadata.strain_condition_3 != b.metadata.strain_condition_3) return a.metadata.strain_condition_3 < b.metadata.strain_condition_3;
	if (a.metadata.culturing_temperature != b.metadata.culturing_temperature) return a.metadata.culturing_temperature < b.metadata.culturing_temperature;
	if (a.metadata.experiment_temperature != b.metadata.experiment_temperature) return a.metadata.experiment_temperature < b.metadata.experiment_temperature;
	if (a.metadata.food_source != b.metadata.food_source) return a.metadata.strain_condition_1 < b.metadata.strain_condition_1;
	return a.metadata.environmental_conditions < b.metadata.environmental_conditions;*/
}

void ns_lifespan_experiment_set::generate_aggregate_for_strain(const ns_region_metadata & m, ns_lifespan_experiment_set & r) const{
	r.clear();
	throw ns_ex("This hasn't been debugged yet");
	const std::string d(m.device_regression_match_description());
	for (unsigned int i = 0; i < curves.size(); i++){
		if (curves[i]->metadata.device_regression_match_description() == d){
			if (r.curves.size() == 0)
				r.curves.resize(1, memory_pool.get(0));
				(*(*r.curves.rbegin())) = *curves[i];
		}
		else{
			vector<ns_survival_timepoint>::iterator p(r.curves[0]->timepoints.end());
			bool found(false);
			for (unsigned int j = 0; j < curves[i]->timepoints.size(); j++){
				for (p = r.curves[0]->timepoints.begin(); p != r.curves[0]->timepoints.end(); p++){
					if (p->absolute_time == curves[i]->timepoints[j].absolute_time){
						found = true;
						break;
					}
					if (p->absolute_time > curves[i]->timepoints[j].absolute_time){
						break;
					}
				}
				if (!found){
					p = r.curves[0]->timepoints.insert(p,curves[i]->timepoints[j]);
				}
				else{
					p->add(curves[i]->timepoints[j]);
				}
			}
		}
	}
}
void ns_survival_data::detetermine_best_guess_death_times() {


}
void ns_survival_data::genenerate_survival_statistics(){
	generate_risk_timeseries();
	generate_survival_statistics(risk_timeseries.movement_based_death,survival_statistics.movement_based_death);
	generate_survival_statistics(risk_timeseries.local_movement_cessations,survival_statistics.local_movement_cessations);
	generate_survival_statistics(risk_timeseries.long_distance_movement_cessation,survival_statistics.long_distance_movement_cessation);
	generate_survival_statistics(risk_timeseries.death_associated_expansion_start, survival_statistics.death_associated_expansion_start);
	generate_survival_statistics(risk_timeseries.best_guess_death, survival_statistics.best_guess_death);
}
void ns_survival_data::generate_survival_statistics(const ns_survival_data_with_censoring & survival_data,ns_survival_statistics & stats) const{
	if (timepoints.size() == 0)
		return;
	if (survival_data.data.number_of_animals_at_risk.size() !=
		survival_data.data_excluding_tails.number_of_animals_at_risk.size() ||
		survival_data.data.number_of_animals_at_risk.size() != timepoints.size())
		throw ns_ex("ns_survival_data::generate_survival_statistics()::Incorrecty built timeseries!");
	stats.clear();


	/*stats.mean.mean_survival_including_censoring = 
		stats.mean.mean_survival_including_censoring_computed_with_log = 
		stats.mean_excluding_tails.mean_survival_including_censoring =
		stats.mean_excluding_tails.mean_survival_including_censoring_computed_with_log = 
			1.0*timepoints[i].absolute_time-metadata.time_at_which_animals_had_zero_age;*/

	for (unsigned int i = 0; i < survival_data.data.number_of_events.size(); i++){
		const unsigned long relative_time(timepoints[i].absolute_time-metadata.time_at_which_animals_had_zero_age);
	

		//unsigned long long weighted_event_count(((unsigned long long)survival_data.data.total_number_of_deaths)*
		//															relative_time);

		stats.mean.weighted_sum_of_death_times += ((unsigned long long)survival_data.data.number_of_events[i])*
																	relative_time;
		stats.mean_excluding_tails.weighted_sum_of_death_times += ((unsigned long long)survival_data.data_excluding_tails.number_of_events[i])*
																	relative_time;

		stats.mean.number_of_deaths+=survival_data.data.number_of_events[i];
		stats.mean.number_censored+=survival_data.data.number_of_censoring_events[i];
		stats.mean_excluding_tails.number_of_deaths+=survival_data.data_excluding_tails.number_of_events[i];
		stats.mean_excluding_tails.number_censored+=survival_data.data_excluding_tails.number_of_censoring_events[i];
	
		//from SAS/STAT(R) 9.2 User's Guide, Second Edition
		//"Product Limit Method"
		if (i == 0){
			stats.mean.mean_survival_including_censoring_computed_with_log 
				=
			stats.mean.mean_survival_including_censoring 
				=
			stats.mean_excluding_tails.mean_survival_including_censoring_computed_with_log 
				=
			stats.mean_excluding_tails.mean_survival_including_censoring 
				= 1.0*timepoints[i].absolute_time-metadata.time_at_which_animals_had_zero_age;
		}
		else{
			const unsigned long duration = timepoints[i].absolute_time - timepoints[i-1].absolute_time;

			stats.mean.mean_survival_including_censoring_computed_with_log 
				+= exp(survival_data.data.log_probability_of_surviving_up_to_interval[i-1])*duration;
			stats.mean.mean_survival_including_censoring 
				+= survival_data.data.probability_of_surviving_up_to_interval[i-1]*duration;

			stats.mean_excluding_tails.mean_survival_including_censoring_computed_with_log 
				+= exp(survival_data.data_excluding_tails.log_probability_of_surviving_up_to_interval[i-1])*duration;
			stats.mean_excluding_tails.mean_survival_including_censoring 
				+= survival_data.data_excluding_tails.probability_of_surviving_up_to_interval[i-1]*duration;
		}
	}
	stats.number_of_plates = 1;
}

void ns_survival_data_with_censoring_timeseries::calculate_risks_and_cumulatives(){
	//calculate risk of death in first interval
	cumulative_number_of_deaths[0] = number_of_events[0];
	cumlative_number_of_censoring_events[0] = number_of_censoring_events[0];
	if (number_of_animals_at_risk[0] == 0)
		risk_of_death_in_interval[0] = 0;
	else risk_of_death_in_interval[0] = number_of_events[0]/(double)number_of_animals_at_risk[0];

	//calculate probability of survival up to first interval
	probability_of_surviving_up_to_interval[0] = 1*(1.0-risk_of_death_in_interval[0]);
	if (risk_of_death_in_interval[0] == 1)
		 log_probability_of_surviving_up_to_interval[0] = -std::numeric_limits<double>::infinity();
	else 
		log_probability_of_surviving_up_to_interval[0] = log(1.0-risk_of_death_in_interval[0]);


	for (unsigned int i = 1; i < risk_of_death_in_interval.size(); i++){
		if (number_of_animals_at_risk[i] == 0)
			risk_of_death_in_interval[i] = 0;
		else risk_of_death_in_interval[i] = number_of_events[i]/(double)number_of_animals_at_risk[i];

		probability_of_surviving_up_to_interval[i] = probability_of_surviving_up_to_interval[i-1]
													 *(1.0-risk_of_death_in_interval[i]);
		if (risk_of_death_in_interval[i] == 1)
			log_probability_of_surviving_up_to_interval[i] = -std::numeric_limits<double>::infinity();
		else log_probability_of_surviving_up_to_interval[i] = log_probability_of_surviving_up_to_interval[i-1]
															  + log(1.0-risk_of_death_in_interval[i]);

		cumulative_number_of_deaths[i] = number_of_events[i]+cumulative_number_of_deaths[i-1];
		cumlative_number_of_censoring_events[i] = number_of_censoring_events[i] + cumlative_number_of_censoring_events[i - 1];
	}
}


void ns_survival_data_with_censoring_timeseries::resize(unsigned long i, const double v){
	cumulative_number_of_deaths.resize(i,v);
	cumlative_number_of_censoring_events.resize(i, v);
	number_of_animals_at_risk.resize(i,v);
	number_of_events.resize(i,v);
	number_of_censoring_events.resize(i,v);
	risk_of_death_in_interval.resize(i,v);
	probability_of_surviving_up_to_interval.resize(i,v);
	log_probability_of_surviving_up_to_interval.resize(i,v);
}
void ns_survival_data_with_censoring_timeseries::resize(unsigned long i){
	cumulative_number_of_deaths.resize(i);
	cumlative_number_of_censoring_events.resize(i);
	number_of_animals_at_risk.resize(i);
	number_of_events.resize(i);
	number_of_censoring_events.resize(i);
	risk_of_death_in_interval.resize(i);
	probability_of_surviving_up_to_interval.resize(i);
	log_probability_of_surviving_up_to_interval.resize(i);
}
struct ns_timepoint_search{
	bool operator ()( const ns_survival_timepoint & a, const ns_survival_timepoint & b)const{
		return a.absolute_time < b.absolute_time;
	}
};
std::vector<ns_survival_timepoint>::const_iterator ns_find_correct_point_for_time(const double t,const std::vector<ns_survival_timepoint> & timepoints){
	ns_survival_timepoint s;
	s.absolute_time = t;
	return std::lower_bound(timepoints.begin(),timepoints.end(),s,ns_timepoint_search());
};
const ns_survival_timepoint_event * ns_correct_event(const ns_movement_event & e, const ns_survival_timepoint & s){
	switch(e){
			case ns_movement_cessation:
				return &s.movement_based_deaths;
			case ns_translation_cessation:
				return &s.local_movement_cessations;
			case ns_fast_movement_cessation:
				return &s.long_distance_movement_cessations;
			case ns_death_associated_expansion_start:
				return &s.death_associated_expansions;
			case ns_additional_worm_entry:
				return &s.best_guess_deaths;
			default:
				throw ns_ex("Unknown normalization event specification: ") << (int)e;
		}
}
void ns_survival_data::generate_risk_timeseries(const ns_movement_event & event_type,ns_survival_data_with_censoring & risk_timeseries) const{
//	ns_survival_statistics stats;
	//find the total number of deaths
	unsigned long total_number_of_deaths(0),total_number_of_censored(0);
	
	risk_timeseries.data.resize(timepoints.size(),0);
	if (timepoints.size() == 0)
		return;

	//ok!  This is a bit complicated.  Hold with me here.
	//Events occur within intervals between observations, not at the observation times themselves.
	//Our best guess at the specific time during the interval at which each event  occurs is provided by 
	//ns_death_time_annotation_time_interval::best_estimate_event_time_for_possible_partially_unbounded_interval()
	//These times are not going to match up with the observation times in timepoints[].
	
	//There are two ways to calculate the risk time series.  One way would be to 
	//calculate the change in risk after every event.  However, because event times are decoupled from timepoints[]
	//this would mean there would be an enormous number of events in the risk_timeseries[] *and* there would be no mapping
	//from any value of timepoints[] to the risk timeseries.

	//Instead, we map the best estimates of the time events into the appropriate interval in timepoints[j].
	//That is, we calculate the actual time of an event, and then find the two consecutive observations in timepoints[]
	//between which that event time occurs.

	//Thus risk_timeseries[] and timepoints[] remain linked.
	for (unsigned int j = 0; j < timepoints.size(); j++){	

		//special case here if event_type is set to ns_additional_worm_entry, indicating we should choose the best guess death time
		const ns_survival_timepoint_event * e(ns_correct_event(event_type,timepoints[j]));

		for (unsigned int k = 0; k < e->events.size(); k++){
			if (!e->events[k].properties.is_excluded() &&
				!e->events[k].properties.is_censored()){
				if (e->events[k].properties.multiworm_censoring_strategy == ns_death_time_annotation::ns_no_strategy_needed_for_single_worm_object ||
					e->events[k].properties.multiworm_censoring_strategy == ns_death_time_annotation::default_censoring_strategy()){
					for (unsigned int m = 0; m < e->events[k].events.size(); m++){
						double event_time(0);
						if (e->events[k].events[m].time.fully_unbounded()){
							cerr << "ns_survival_data::generate_risk_timeseries()::Ignoring a fully-unbounded death event\n";
							continue;
						}
						if (e->events[k].events[m].time.period_start_was_not_observed)
							event_time = e->events[k].events[m].time.period_end;
						else if (e->events[k].events[m].time.period_end_was_not_observed)
							event_time = e->events[k].events[m].time.period_start;
						else event_time = e->events[k].events[m].time.best_estimate_event_time_within_interval();
						const std::vector<ns_survival_timepoint>::const_iterator 
							timepoint (ns_find_correct_point_for_time(event_time,timepoints));
						if (timepoint == timepoints.end())
							throw ns_ex("Could not find timepoint for time") << e->events[k].events[m].time.best_estimate_event_time_within_interval();
						const ns_64_bit index_id(timepoint-timepoints.begin());
					//	if (/*risk_timeseries.data.number_of_events[index_id] += e->events[k].events[m].number_of_worms() > 1 &&*/ e->events[k].events[m].stationary_path_id.group_id==23)
				//			cout << "found";
						risk_timeseries.data.number_of_events[index_id]+=e->events[k].events[m].number_of_worms();//number_of_worms_in_annotation;
						total_number_of_deaths+=e->events[k].events[m].number_of_worms();//number_of_worms_in_annotation;
					}
				}
			//	else 
			//		cerr << "s";
			}
		//	else 
		//		cerr << "S";
			if (e->events[k].is_appropriate_in_default_censoring_scheme()){
				for (unsigned int m = 0; m < e->events[k].events.size(); m++){
					if (e->events[k].events[m].time.period_end_was_not_observed){
						cerr << "Found an event that extended past the end of the experiment\n";
						continue;
					}
					if (e->events[k].events[m].time.period_start_was_not_observed){
						cerr << "Found an event that extended past the beginning of the experiment\n";
						continue;
					}
					const std::vector<ns_survival_timepoint>::const_iterator 
					timepoint (ns_find_correct_point_for_time(e->events[k].events[m].time.best_estimate_event_time_within_interval(),timepoints));
					if (timepoint == timepoints.end())
						throw ns_ex("Could not find timepoint for time") << e->events[k].events[m].time.best_estimate_event_time_within_interval();
					const ns_64_bit index_id(timepoint-timepoints.begin());
					risk_timeseries.data.number_of_censoring_events[index_id]+=e->events[k].events[m].number_of_worms();
					total_number_of_censored+=e->events[k].events[m].number_of_worms();
				}
			}
		}
	}
	
	//we only include the middle 90% of data points in calculating
	const unsigned long start_n(total_number_of_deaths/20),
					count_n(total_number_of_deaths-2*start_n);

	risk_timeseries.data.total_number_of_deaths = total_number_of_deaths;
	risk_timeseries.data.total_number_of_censoring_events = total_number_of_censored;
	risk_timeseries.data_excluding_tails.total_number_of_deaths = count_n;
	//risk_timeseries.data_excluding_tails.total_number_of_censoring_events = 0;  //set later
	//the regression parameters
	//find the start index
	unsigned long start_index(0),stop_index(0),
		number_of_deaths_up_to_start(0),
		number_of_censoring_up_to_start(0),
		number_of_deaths_before_end(0),
		number_of_censoring_events_before_end(0),
		number_of_deaths_occuring_at_first_timepoint(0),
		number_of_censoring_occuring_at_first_timepoint(0),
		cumulative_deaths(0),
		cumulative_censored(0),
		number_of_censoring_at_last_timepoint(0),
		number_of_deaths_at_last_timepoint(0);

	risk_timeseries.data_excluding_tails.resize(timepoints.size(),0);

	for (unsigned int j = 0; j < timepoints.size(); j++){
		unsigned long event_count(risk_timeseries.data.number_of_events[j]),
							censor_count(risk_timeseries.data.number_of_censoring_events[j]);
			
		//if we're before the start
		if (cumulative_deaths + event_count <= start_n){
			number_of_deaths_up_to_start+=event_count;
			number_of_censoring_up_to_start+=censor_count;

			cumulative_deaths += event_count;
			cumulative_censored += censor_count;
			continue;
		}
		//if we've just hit the start
		if (cumulative_deaths <= start_n){
			start_index = j;
			number_of_deaths_occuring_at_first_timepoint = cumulative_deaths+event_count-start_n;
			number_of_deaths_up_to_start = start_n;
			number_of_censoring_occuring_at_first_timepoint = 0;
			number_of_censoring_up_to_start+=censor_count; 

			//include deaths up until start_n but
			//leave the remainder in case we cross the stop here too.
			event_count = number_of_deaths_occuring_at_first_timepoint;
			cumulative_deaths=start_n;
			censor_count = 0;
		}

		//if we're after the end
		if (cumulative_deaths + event_count >= start_n+count_n){
			cumulative_censored+=censor_count;	//even though we may have hit the death count 
											//at in the previous interval, we could all censored
											//there.
			//if we've just hit the end
			if (cumulative_deaths < start_n+count_n){
				stop_index = j;
				number_of_deaths_before_end = start_n+count_n;
				number_of_censoring_events_before_end = cumulative_censored;
				
				number_of_deaths_at_last_timepoint = number_of_deaths_before_end-cumulative_deaths;
				number_of_censoring_at_last_timepoint = censor_count;
				cumulative_deaths += number_of_deaths_at_last_timepoint;
			}
			//we're done!
			break;
		}
		cumulative_censored+=censor_count;
		cumulative_deaths+=event_count;
	}
	if (risk_timeseries.data_excluding_tails.total_number_of_deaths != cumulative_deaths - number_of_deaths_up_to_start)
		throw ns_ex("Summing error:") << risk_timeseries.data_excluding_tails.total_number_of_deaths << " expected; " << cumulative_deaths - number_of_deaths_up_to_start << " received.";
	risk_timeseries.data_excluding_tails.total_number_of_censoring_events = cumulative_censored - number_of_censoring_up_to_start;


	//Now we know the at risk population for the entire interval for which we want to calculate the mean
	//So we start!
	unsigned long total_at_risk_population_excluding_tails = (risk_timeseries.data_excluding_tails.total_number_of_deaths)
															 + (risk_timeseries.data_excluding_tails.total_number_of_censoring_events);
	unsigned long total_at_risk_population_for_all_data =(risk_timeseries.data.total_number_of_deaths)
															 + (risk_timeseries.data.total_number_of_censoring_events);


	//add statistics for first timepoint in excluded-tail set  (as it has slightly different numbers than others)
	risk_timeseries.data_excluding_tails.number_of_animals_at_risk[start_index] = total_at_risk_population_excluding_tails;
	risk_timeseries.data_excluding_tails.number_of_censoring_events[start_index] = number_of_censoring_occuring_at_first_timepoint;
	risk_timeseries.data_excluding_tails.number_of_events[start_index] = number_of_deaths_occuring_at_first_timepoint;

	total_at_risk_population_excluding_tails -= (number_of_deaths_occuring_at_first_timepoint + number_of_censoring_occuring_at_first_timepoint);

	unsigned long cur_n(0);
	for (unsigned int j = 0; j < timepoints.size(); j++){
		unsigned long event_count(risk_timeseries.data.number_of_events[j]),
						censoring_count(risk_timeseries.data.number_of_censoring_events[j]);
		
		//update the total at risk removing censored animals only after calculating current risk;
		//as censoring only effects later points
		risk_timeseries.data.number_of_animals_at_risk[j] = total_at_risk_population_for_all_data;
		total_at_risk_population_for_all_data -= (event_count + censoring_count);

		if (j <= start_index || j > stop_index) //we've handled the j == start_index case before the loop
			continue;

		if (j == stop_index){
			risk_timeseries.data_excluding_tails.number_of_animals_at_risk[j] = total_at_risk_population_excluding_tails;
			risk_timeseries.data_excluding_tails.number_of_censoring_events[j] = number_of_censoring_at_last_timepoint;
			risk_timeseries.data_excluding_tails.number_of_events[j] = number_of_deaths_at_last_timepoint;
			continue;
		}

		risk_timeseries.data_excluding_tails.number_of_censoring_events[j] = censoring_count;
		risk_timeseries.data_excluding_tails.number_of_events[j] = event_count;
		risk_timeseries.data_excluding_tails.number_of_animals_at_risk[j] = total_at_risk_population_excluding_tails;
		total_at_risk_population_excluding_tails -= (event_count + censoring_count);
	}
	risk_timeseries.data.calculate_risks_and_cumulatives();
	risk_timeseries.data_excluding_tails.calculate_risks_and_cumulatives();
}
		

void ns_lifespan_experiment_set::compute_device_normalization_regression(const ns_device_temperature_normalization_data & data, const ns_censoring_strategy & s, const ns_tail_strategy & t){
	compute_device_normalization_regression(ns_translation_cessation, data,normalization_stats_for_translation_cessation,s,t);
	compute_device_normalization_regression(ns_movement_cessation, data,normalization_stats_for_death,s,t);
	compute_device_normalization_regression(ns_fast_movement_cessation, data,normalization_stats_for_fast_movement_cessation,s,t);
}

typedef std::vector<ns_survival_statistics *> ns_survival_statistics_list;
typedef std::map<std::string,ns_survival_statistics_list> ns_grouped_survival_statistics_list;
typedef std::map<std::string,std::map<std::string,ns_survival_statistics_list> > ns_grouped_grouped_survival_statistics_list;

void ns_weighted_mean_add(const ns_mean_statistics a, const double & s, ns_mean_statistics & r){
	r.mean_survival_including_censoring += s*a.mean_survival_including_censoring;
	r.mean_survival_including_censoring_computed_with_log += s*a.mean_survival_including_censoring_computed_with_log;
	r.number_of_deaths+=a.number_of_deaths;
	r.weighted_sum_of_death_times+=a.weighted_sum_of_death_times;
}
void ns_aggregate_statistics(const ns_survival_statistics_list & stats, ns_survival_statistics & combined){
	unsigned long total_population_size(0),
				  total_population_size_with_tails_excluded(0);
	for (unsigned int i = 0; i < stats.size(); i++){
		total_population_size+=stats[i]->mean.number_of_deaths;
		total_population_size_with_tails_excluded+=stats[i]->mean_excluding_tails.number_of_deaths;
	}
	combined.clear();
	for (unsigned int i = 0; i < stats.size(); i++){
		ns_weighted_mean_add(stats[i]->mean,stats[i]->mean.number_of_deaths/(double)total_population_size,combined.mean);
		ns_weighted_mean_add(stats[i]->mean_excluding_tails,
			stats[i]->mean_excluding_tails.number_of_deaths/(double)total_population_size_with_tails_excluded,combined.mean_excluding_tails);
	}
	combined.number_of_plates = stats.size();
}
class ns_find_or_create_iterator{
public:
	template<class key_t, class value_t>
	static typename std::map<key_t,value_t>::iterator find(const key_t & k, const value_t & v,typename std::map<key_t,value_t> & m){
		typename std::map<key_t,value_t>::iterator p = m.find(k);
		if (p == m.end())
			return m.insert(typename std::pair<key_t,value_t>(k,v)).first;
		return p;
	}
};


void ns_lifespan_experiment_set::compute_device_normalization_regression(const ns_movement_event & event_type,  const ns_device_temperature_normalization_data & regression_specification,
	ns_lifespan_device_normalization_statistics_set & regression_statistics_set, const ns_censoring_strategy & censoring_strategy,const ns_tail_strategy & tail_strategy){
	regression_statistics_set.produce_identity = regression_specification.identity();
	//<animal description,death statistics for that strain>

	//make statistics for each strain on each scanner
	//<device name, strain, stat>
	ns_grouped_grouped_survival_statistics_list strain_statistics_for_each_device;
	//<strain, stat>
	ns_grouped_survival_statistics_list strain_statistics;
	//<device,stat>
	ns_grouped_survival_statistics_list control_strain_statistics_for_each_device;
	ns_survival_statistics_list all_control_strain_statistics;

	map<std::string,ns_region_metadata> strain_metadata;
	std::set<std::string> all_device_names;

	std::map<std::string,bool> strain_is_control;

	for (std::vector<unsigned long>::size_type i = 0; i < curves.size(); i++){
		if (curves[i]->metadata.device.size() == 0)
			throw ns_ex("ns_lifespan_experiment_set::normalize_by_device_mean()::Found an unspecified device name");
		if (curves[i]->risk_timeseries.movement_based_death.data.number_of_animals_at_risk.empty())
			curves[i]->genenerate_survival_statistics();
		all_device_names.insert(all_device_names.end(),curves[i]->metadata.device);
		ns_region_metadata aggregate_metadata = curves[i]->metadata;
		aggregate_metadata.region_name.clear();
		aggregate_metadata.region_id = 0;
		aggregate_metadata.sample_id = 0;
		aggregate_metadata.sample_name.clear();
		strain_metadata[curves[i]->metadata.device_regression_match_description()] = aggregate_metadata;

		ns_survival_statistics * statistics_to_use(0);
		switch(event_type){
			case ns_movement_cessation: statistics_to_use = &curves[i]->survival_statistics.movement_based_death;
				break;
			case ns_additional_worm_entry: statistics_to_use = &curves[i]->survival_statistics.best_guess_death;
				break;
			case ns_translation_cessation: statistics_to_use = &curves[i]->survival_statistics.local_movement_cessations;
				break;
			case ns_fast_movement_cessation: statistics_to_use = &curves[i]->survival_statistics.long_distance_movement_cessation;
				break;
			default: throw ns_ex("Unknown movement event");
		}
		//find the entry for the strain on its device list
		ns_grouped_grouped_survival_statistics_list::iterator device_stats(
			ns_find_or_create_iterator::find(curves[i]->metadata.device,ns_grouped_survival_statistics_list(),
										strain_statistics_for_each_device)
			);
		ns_find_or_create_iterator::find(curves[i]->metadata.device_regression_match_description(),ns_survival_statistics_list(),
											device_stats->second)
											->second.push_back(statistics_to_use);

		//find the entry for the strain on all strains list
		ns_find_or_create_iterator::find(curves[i]->metadata.device_regression_match_description(),ns_survival_statistics_list(),
											strain_statistics)->second.push_back(statistics_to_use);

		//if the device is a control strain, add it to the control strain list
	//	bool is_control_strain(false);
		for (unsigned int k = 0; k < regression_specification.control_strains.size(); k++){
			if(curves[i]->metadata.matches(regression_specification.strain_description_detail_type,regression_specification.control_strains[k])){
				strain_is_control[curves[i]->metadata.device_regression_match_description()] = true;
				ns_find_or_create_iterator::find(curves[i]->metadata.device,ns_survival_statistics_list(),
												control_strain_statistics_for_each_device)
												->second.push_back(statistics_to_use);
				all_control_strain_statistics.push_back(statistics_to_use);
				break;
			}
			else strain_is_control[curves[i]->metadata.device_regression_match_description()] = false;
		}
	}
	
	//control groups are mutant strains paired with their wildtype control.
	//we make a control group for each type of plate not declared as a control
	//map<std::string,long> control_groups;
	unsigned long max_control_group_id = 0;
	

	
	//calculate grand mean for all control plates in experiments
	ns_aggregate_statistics(all_control_strain_statistics,regression_statistics_set.grand_control_mean);

	//calculate grand means for each strains in experiment
	for (ns_grouped_survival_statistics_list::iterator strain =  strain_statistics.begin(); strain != strain_statistics.end(); strain++)
		ns_aggregate_statistics(strain->second,regression_statistics_set.grand_strain_mean[strain->first]);


	//Go through and calculate the control plate regression parameters for each device
	for (std::set<std::string>::iterator device_name = all_device_names.begin(); device_name != all_device_names.end(); device_name++){
		ns_lifespan_device_normalization_statistics_set::ns_device_stats_list::iterator regression_stats_device(
			ns_find_or_create_iterator::find(*device_name,ns_lifespan_device_normalization_statistics_for_device(),
											regression_statistics_set.devices));

		ns_grouped_survival_statistics_list::iterator device_control_stats = 
			control_strain_statistics_for_each_device.find(*device_name);
		if (device_control_stats == control_strain_statistics_for_each_device.end()){
			regression_stats_device->second.device_had_control_plates = false;
			continue;
		}
		regression_stats_device->second.device_had_control_plates = true;

		//calculate the mean of all control plates on the device
		ns_aggregate_statistics(device_control_stats->second,regression_stats_device->second.device_control_plate_statistics);
		
		ns_mean_statistics & device_mean_stats((tail_strategy == ns_exclude_tails)?regression_stats_device->second.device_control_plate_statistics.mean_excluding_tails
															:regression_stats_device->second.device_control_plate_statistics.mean);
		ns_mean_statistics & grand_mean_stats((tail_strategy == ns_exclude_tails)?regression_statistics_set.grand_control_mean.mean_excluding_tails
															:regression_statistics_set.grand_control_mean.mean);
		const double device_mean((censoring_strategy == ns_include_censoring_data)?device_mean_stats.mean_survival_including_censoring:
													device_mean_stats.mean_excluding_censoring());
		const double grand_mean((censoring_strategy == ns_include_censoring_data)?grand_mean_stats.mean_survival_including_censoring:
													grand_mean_stats.mean_excluding_censoring());

		regression_stats_device->second.regression_statistics.external_control_mean_fix_point_specified = 
			regression_specification.fix_control_mean_lifespan;
		if (regression_specification.fix_control_mean_lifespan)
			regression_stats_device->second.regression_statistics.control_mean_fix_point = regression_specification.external_fixed_control_mean_lifespan;
		else
			regression_stats_device->second.regression_statistics.control_mean_fix_point = grand_mean;
			
		if (device_mean != 0){
			if (regression_stats_device->second.regression_statistics.control_mean_fix_point == 0)
				throw ns_ex("Zero control mean fix point!");
		}
		regression_stats_device->second.regression_statistics.additive_device_regression_coefficient 
						= device_mean - regression_stats_device->second.regression_statistics.control_mean_fix_point;
		regression_stats_device->second.regression_statistics.multiplicative_additive_device_regression_additive_coefficient
						= device_mean/regression_stats_device->second.regression_statistics.control_mean_fix_point;
		//left for future use.
		regression_stats_device->second.regression_statistics.multiplicative_additive_device_regression_additive_coefficient = 0;
		regression_stats_device->second.regression_statistics.multiplicative_additive_device_regression_multiplicative_coefficient = 0;
	}

	//collect which pair-wise comparrison group each mutant (and each wildtype) strain should be in for each scanner.
	//mutants will by defintion be only included in one control group, but controls may act as controls in
	//multiple groups.
	std::vector<ns_control_group_strain_assignment> current_device_control_groups;

	//Now go through and get the average behavior of each strain on each device (for diagnostic information)
	for (ns_grouped_grouped_survival_statistics_list::iterator cur_device = strain_statistics_for_each_device.begin(); cur_device != strain_statistics_for_each_device.end(); cur_device++){
	current_device_control_groups.resize(0);

	//find the correct regression statistics entry for device
		ns_lifespan_device_normalization_statistics_set::ns_device_stats_list::iterator regression_stats_device(
			ns_find_or_create_iterator::find(cur_device->first,ns_lifespan_device_normalization_statistics_for_device(),
											regression_statistics_set.devices));
		
		//look at each strain on device
		for (ns_grouped_survival_statistics_list::iterator device_strain = cur_device->second.begin(); 
														device_strain!= cur_device->second.end(); ++device_strain){
			if (!strain_is_control[device_strain->first]){
				ns_control_group_strain_assignment assignment;
				assignment.device = cur_device->first;
				assignment.strain = device_strain->first;
				assignment.control_group_memberships.insert(max_control_group_id);
				current_device_control_groups.push_back(assignment);
				max_control_group_id++;
			}
			ns_lifespan_device_normalization_statistics_for_device::ns_strain_stat_list::iterator regression_stats_device_strain(
				regression_stats_device->second.strains.find(device_strain->first));
			if (regression_stats_device_strain != regression_stats_device->second.strains.end())
				throw ns_ex("Repeat strains found!");
			else{
				regression_stats_device_strain = regression_stats_device->second.strains.insert(
					ns_lifespan_device_normalization_statistics_for_device::ns_strain_stat_list::value_type(
					device_strain->first,ns_lifespan_device_normalization_statistics())).first;
				regression_stats_device_strain->second.is_a_control_plate = 
					strain_is_control[device_strain->first];
			}
			ns_aggregate_statistics(device_strain->second,regression_stats_device_strain->second.device_strain_mean);
			regression_stats_device_strain->second.control_mean_fix_point; // TODO: Should this be set to something?
			
			regression_stats_device_strain->second.grand_strain_mean = regression_statistics_set.grand_strain_mean[device_strain->first];
			regression_stats_device_strain->second.strain_info = strain_metadata[device_strain->first];
			
			ns_mean_statistics & device_mean_stats((tail_strategy == ns_exclude_tails)?regression_stats_device_strain->second.device_strain_mean.mean_excluding_tails
																							:regression_stats_device_strain->second.device_strain_mean.mean);
			ns_mean_statistics & grand_mean_stats((tail_strategy == ns_exclude_tails)?regression_stats_device_strain->second.grand_strain_mean.mean_excluding_tails
																						:regression_stats_device_strain->second.grand_strain_mean.mean);
			const double device_mean((censoring_strategy == ns_include_censoring_data)?device_mean_stats.mean_survival_including_censoring:
													   device_mean_stats.mean_excluding_censoring());
			regression_stats_device_strain->second.device_strain_mean_used = device_mean;
			regression_stats_device_strain->second.device_death_count_used = device_mean_stats.number_of_deaths;
			regression_stats_device_strain->second.device_censoring_count_used = device_mean_stats.number_censored;
			const double grand_mean((censoring_strategy == ns_include_censoring_data)?grand_mean_stats.mean_survival_including_censoring:
													   grand_mean_stats.mean_excluding_censoring());
			regression_stats_device_strain->second.grand_strain_mean_used = grand_mean;
			
			regression_stats_device_strain->second.external_control_mean_fix_point_specified = false;
			regression_stats_device->second.regression_statistics.control_mean_fix_point = grand_mean;

			regression_stats_device_strain->second.additive_device_regression_coefficient =
								device_mean - regression_stats_device_strain->second.control_mean_fix_point;
			regression_stats_device_strain->second.multiplicative_device_regression_coefficient = 
								device_mean/regression_stats_device_strain->second.control_mean_fix_point;
			//Left for future use
			regression_stats_device_strain->second.multiplicative_additive_device_regression_additive_coefficient = 0;
			regression_stats_device_strain->second.multiplicative_additive_device_regression_multiplicative_coefficient = 0;
		}
		const unsigned long last_mutant_current_device_control_group(current_device_control_groups.size());
		for (ns_grouped_survival_statistics_list::iterator device_strain = cur_device->second.begin(); 
														device_strain!= cur_device->second.end(); ++device_strain){
			//each control strain on each scanner is part of each mutant's control group.
			//in other words, each mutant it independantly compared to its control
			if (strain_is_control[device_strain->first]){
				ns_control_group_strain_assignment assignment;
				assignment.device = cur_device->first;
				assignment.strain = device_strain->first;
				if (last_mutant_current_device_control_group != 0){
					for (unsigned int i = 0; i < last_mutant_current_device_control_group; i++)
						assignment.control_group_memberships.insert(*current_device_control_groups[i].control_group_memberships.begin());
					current_device_control_groups.push_back(assignment);
				}
				else{
					ns_control_group_strain_assignment assignment;
					assignment.device = cur_device->first;
					assignment.strain = device_strain->first;
					assignment.control_group_memberships.insert(max_control_group_id);
					current_device_control_groups.push_back(assignment);
					max_control_group_id++;
				}
			}
		}
		regression_statistics_set.control_group_strain_list.insert(regression_statistics_set.control_group_strain_list.end(),current_device_control_groups.begin(),current_device_control_groups.end());
	}
	//We now create the lookup table to allow us to identifiy the group assignment for each plate.
	for (unsigned int i = 0; i < this->curves.size(); i++){
		const ns_control_group_strain_assignment * group_assignment(0);
		for (unsigned int j = 0; j < regression_statistics_set.control_group_strain_list.size(); j++){
			if (regression_statistics_set.control_group_strain_list[j].device == curves[i]->metadata.device &&
				regression_statistics_set.control_group_strain_list[j].strain == curves[i]->metadata.device_regression_match_description()){
					group_assignment = &regression_statistics_set.control_group_strain_list[j];
					break;
			}
		}
		if (group_assignment == 0)
			throw ns_ex("Could not find control group assignment for plate ") << curves[i]->metadata.plate_name();

		regression_statistics_set.control_group_plate_list[curves[i]->metadata.region_id].control_group_memberships.insert(group_assignment->control_group_memberships.begin(),group_assignment->control_group_memberships.end());
		regression_statistics_set.control_group_plate_list[curves[i]->metadata.region_id].plate_id = curves[i]->metadata.region_id;
	}
}
/*
void ns_survival_data_summary_aggregator::add(const ns_movement_event normalization_type, const ns_survival_data_summary & summary){
	ns_plate_list::iterator p(plate_list.find(summary.metadata.plate_name()));
	if (p == plate_list.end())
		p = plate_list.insert(ns_plate_list::value_type(summary.metadata.plate_name(),ns_plate_normalization_list())).first;
	ns_plate_normalization_list::iterator q = p->second.find(normalization_type);
	if (q!= p->second.end())
		throw ns_ex("Duplicate specificaiton of region ") << summary.metadata.plate_name() << " with normalization " << ns_movement_event_to_label(normalization_type);
	p->second.insert(ns_plate_normalization_list::value_type(normalization_type,summary));
}

void ns_survival_data_summary_aggregator::add(const ns_movement_event normalization_type,const ns_lifespan_experiment_set & set){
	for (unsigned int i = 0; i < set.curves.size(); i++)
		add(normalization_type,set.curves[i]->produce_summary());
}

std::vector<ns_movement_event> ns_survival_data_summary_aggregator::events_to_output;
ns_survival_data_summary_aggregator::ns_survival_data_summary_aggregator(){
	if (events_to_output.empty()){
		events_to_output.push_back(ns_no_movement_event);
		events_to_output.push_back(ns_movement_cessation);
		events_to_output.push_back(ns_translation_cessation);
		events_to_output.push_back(ns_fast_movement_cessation);
	}
}

void ns_survival_data_summary_aggregator::out_JMP_summary_data_header(std::ostream & o, const std::string & terminator) {
	for (unsigned int i = 0; i < events_to_output.size(); i++){
		std::string terminator((i != events_to_output.size()-1)?",":terminator);
		ns_survival_data_summary::out_jmp_header(std::string("Normalized to ") +ns_movement_event_to_string(events_to_output[i]),o,terminator);
	}
}

void ns_survival_data_summary_aggregator::out_JMP_summary_data(const ns_plate_list::const_iterator & region,std::ostream & o)const{
	for (unsigned int i = 0; i < events_to_output.size(); i++){
		std::string terminator((i != events_to_output.size()-1)?",":"");
		ns_plate_normalization_list::const_iterator q(region->second.find(events_to_output[i]));
		if (q == region->second.end()){
			ns_survival_data_summary::out_blank_jmp_data(o,terminator);
		}
		else{
			q->second.out_jmp_data(o,terminator);
		}
	}
}
void ns_survival_data_summary_aggregator::out_JMP_empty_summary_data(std::ostream & o){
	for (unsigned int i = 0; i < events_to_output.size(); i++){
		std::string terminator((i != events_to_output.size()-1)?",":"");
		ns_survival_data_summary::out_blank_jmp_data(o,terminator);
	}
}
void ns_survival_data_summary_aggregator::out_JMP_summary_file(std::ostream & o) const {

	ns_region_metadata::out_JMP_plate_identity_header(o);
	o<<",";
	out_JMP_summary_data_header(o,"\n");

	for (ns_plate_list::const_iterator p(plate_list.begin()); p != plate_list.end(); p++){
		if (events_to_output.size() == 0) continue;
		p->second.begin()->second.metadata.out_JMP_plate_identity_data(o) ;
		o<< ",";
		out_JMP_summary_data(p,o);
		o << "\n";
	}
}

*/