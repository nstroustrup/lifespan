#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include "ns_ex.h"
#include "ns_thread.h"
#include "ns_dir.h"
#include <time.h>
#include <stdio.h>
#include "ns_movement_state.h"
#include "ns_survival_curve.h"
#include "ns_death_time_annotation.h"
#include "ns_by_hand_lifespan.h"
#include "ns_jmp_file.h"
using namespace std;

#define ns_version_number std::string("0.91")

string usage(const string & name){
	string ret("Usage: ");
	ret += name;
	ret+=": filename\n";
	ret+="Version ";
	ret+=ns_version_number;
	ret+="\n";
	ret+="Recognized standard field names:\n";
	vector<string> names;
	ns_region_metadata::recognized_field_names(names);
	for (unsigned int i = 0; i < names.size(); i++){
		ret+= "\t";
		ret += names[i];
		ret +="\n";
	}
	return ret;
}
int main(int argc, char* argv[]){
	try{
		bool load_jmp(false);
		//Grab handle for metadata
		string filename;
		if (argc==2){
			filename = argv[1];
		}
		else if (argc==3){
			if (string(argv[1]) == "--jmp")
				load_jmp = true;
			else if (string(argv[1]) == "--hand")
				load_jmp = false;
			else
				throw ns_ex(usage(argv[0]));
			filename = argv[2];
		}
		else 
			throw ns_ex(usage(argv[0]));

	
		ifstream in(filename.c_str());
		if (in.fail())
			throw ns_ex("Could not open file: ") << filename;

		cout << "Processing " << filename << "\n";
		
		//open files first
		//so if there's a problem in analysis
		//all output files will be empty.
	
		string base_name = ns_dir::extract_filename_without_extension(filename);
		string jmp_w_extra_metadata_name(base_name +"_jmp_extra_metadata.csv");
		string jmp_strict_detailed_name(base_name +"_detailed_jmp.csv");
		string jmp_strict_simple_name(base_name +"_simple_jmp.csv");
		string matlab_name(base_name +"_matlab.m");

		ofstream o_jmp(jmp_w_extra_metadata_name.c_str());
		if (o_jmp.fail())
			throw ns_ex("Could not open file for output: ") << jmp_w_extra_metadata_name;
		ofstream o_jmp_strict_detailed(jmp_strict_detailed_name.c_str());
		if (o_jmp_strict_detailed.fail())
			throw ns_ex("Could not open file for output: ") << jmp_strict_detailed_name;
		ofstream o_jmp_strict_simple(jmp_strict_simple_name.c_str());
		if (o_jmp_strict_simple.fail())
			throw ns_ex("Could not open file for output: ") << jmp_strict_simple_name;
		ofstream o_matlab(matlab_name.c_str());
		if (o_matlab.fail())
			throw ns_ex("Could not open file for output: ") << matlab_name;

		ns_lifespan_experiment_set set;

		if (!load_jmp){

			ns_by_hand_lifespan_experiment_specification by_hand_spec;
			by_hand_spec.load(in);
			in.close();
			by_hand_spec.convert_to_lifespan_experiment_set(set);

			try{
		//		ofstream o_jmp(jmp_w_extra_metadata_name.c_str());
		//		if (o_jmp.fail())
		//			throw ns_ex("Could not open file for output: ") << jmp_w_extra_metadata_name;
				
				by_hand_spec.output_jmp_file_with_extra_metadata(ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_control_group_behavior::ns_do_not_include_control_groups,o_jmp);
				o_jmp.close();
			}
			catch(...){}
		}
		else{
			set.load_from_JMP_file(in);
			in.close();
		}
		

	
		ns_device_temperature_normalization_data regression_data;
		regression_data.produce_identity();
		set.compute_device_normalization_regression(regression_data,ns_lifespan_experiment_set::ns_ignore_censoring_data,ns_lifespan_experiment_set::ns_include_tails);
		set.output_JMP_file(ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,o_jmp_strict_detailed,ns_lifespan_experiment_set::ns_detailed_compact);
		set.output_JMP_file(ns_lifespan_experiment_set::ns_output_single_event_times,ns_lifespan_experiment_set::ns_days,o_jmp_strict_simple,ns_lifespan_experiment_set::ns_simple);
	//	ns_lifespan_experiment_set set_2,set_3;
	//	set.convert_absolute_times_to_ages();
	//	set.generate_common_time_set(set_2);
	//	set_2.group_strains(set_3);
	//	set_3.force_common_time_set_to_constant_time_interval(12*60*60,set_2);
	//	set_2.output_matlab_file(o_matlab);
	}
	catch(ns_ex & ex){
		cerr << ex.text();
		char a;
		cin >> a;
		return 1;
	}
	return 0;
}

