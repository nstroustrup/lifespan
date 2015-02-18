// concatenate_data.cpp : Defines the entry point for the console application.
//

#//include "stdafx.h"
#include "ns_ex.h"
#include <fstream>
#include "ns_dir.h"
#include "ns_thread.h"
#include "ns_xml.h"
#include <regex>
using namespace std;

struct ns_data_concatenate_specification_type{
	std::string base_directory;
	std::vector<std::string> names;
};
struct ns_data_concatenate_specification{
	ns_data_concatenate_specification_type machine_data,
										   by_hand_data;
	std::string output_directory;
	std::string output_title;
	std::vector<std::string> require_string;
};

struct ns_survival_file_spec_file{
	ns_survival_file_spec_file():file(){};
	std::string filename;
	std::ofstream * file;
};

struct ns_survival_file_spec{
	std::string sub_dir;
	std::vector<ns_survival_file_spec_file> files;
};
struct ns_survival_files{
	typedef std::vector<ns_survival_file_spec> ns_specification_list;
	ns_specification_list specifications;

	unsigned long count() const{
		unsigned long c(0);
		for (unsigned int i = 0; i < specifications.size(); i++){
			c+=specifications[i].files.size();
		}
		return c;
	}
	static ns_survival_files extended_files(){
		ns_survival_files f;
		f.specifications.resize(4);
		f.specifications[0].sub_dir = "survival_simple";
		f.specifications[1].sub_dir = "survival_simple_with_control_groups";
		f.specifications[2].sub_dir = "survival_multiple_events";
		for (unsigned int i = 0; i < 3; i++){
			f.specifications[i].files.resize(4);
			f.specifications[i].files[0].filename = "machine_jmp_days";
			f.specifications[i].files[0].filename = "machine_hand_jmp_days";
			f.specifications[i].files[1].filename = "machine_jmp_hours";
			f.specifications[i].files[2].filename = "machine_jmp_time_standardized_days";
			f.specifications[i].files[3].filename = "machine_jmp_time_standardized_hours";
		}
		f.specifications[3].sub_dir = "detailed_animal_data";
		f.specifications[3].files.resize(1);
		f.specifications[3].files[0].filename = "detailed_animal_data";
	return f;
	}
	static ns_survival_files simple(){
		ns_survival_files f;
		f.specifications.resize(1);
		f.specifications[0].sub_dir = "survival_simple";
		for (unsigned int i = 0; i < f.specifications.size(); i++){
			f.specifications[i].files.resize(2);
			f.specifications[i].files[0].filename = "machine_jmp_days";
			f.specifications[i].files[1].filename = "machine_hand_jmp_days";
	//		f.specifications[i].files[1].filename = "machine_jmp_time_interval_days";
//		f.specifications[i].files[1].filename = "machine_jmp_hours";
		}
	return f;
	}
};



class ns_data_concatenate{
public:
	static void concatenate(ns_data_concatenate_specification & data_to_concatenate,ns_survival_files & spec){
		for (unsigned int i = 0; i < data_to_concatenate.machine_data.names.size(); i++){
			for (unsigned int j = i+1; j < data_to_concatenate.machine_data.names.size(); j++)
				if (data_to_concatenate.machine_data.names[i] == data_to_concatenate.machine_data.names[j])
					throw ns_ex("Data file ") << data_to_concatenate.machine_data.names[i] << " is duplicated in the specification";
		}
		string tmp;
		string output_title(data_to_concatenate.output_title);
		string regex_str;
		for (unsigned int i = 0; i < data_to_concatenate.require_string.size(); i++){
			regex_str += "(";
			regex_str += data_to_concatenate.require_string[i];
			regex_str += ")";
			if (i + 1 != data_to_concatenate.require_string.size())
				regex_str += "|";
		}
		if (output_title.size() == 0)
			output_title="concatenated_data";
		 std::regex regex_obj(regex_str);

		ns_data_concatenate_specification_type *specs[2] = {&data_to_concatenate.machine_data,&data_to_concatenate.by_hand_data};
		//check all files are there before starting
		for (unsigned int xx = 0; xx <2; xx++){//handle both machine and by hand data.
			ns_data_concatenate_specification_type & c(*specs[xx]);
			for (std::vector<std::string>::const_iterator experiment_name = c.names.begin(); experiment_name != c.names.end(); experiment_name++){
				for (ns_survival_files::ns_specification_list::iterator data_spec = spec.specifications.begin(); data_spec != spec.specifications.end(); data_spec++){
					ns_dir dir;
					vector<string> current_directory_files;
					std::string experiment_base_directory(c.base_directory+ DIR_CHAR_STR + *experiment_name + DIR_CHAR_STR + "survival_data");
					std::string cur_dir(experiment_base_directory+ DIR_CHAR_STR + data_spec->sub_dir);
					dir.load_masked(cur_dir,"csv",current_directory_files);
					unsigned long found_count(0);
					for (unsigned int cur_dir_file_id = 0; cur_dir_file_id < current_directory_files.size(); cur_dir_file_id++){
						for (unsigned int i = 0; i < data_spec->files.size(); i++){
							if (current_directory_files[cur_dir_file_id].find(data_spec->files[i].filename)!=current_directory_files[cur_dir_file_id].npos){
								string infile(cur_dir + DIR_CHAR_STR + current_directory_files[cur_dir_file_id]);
								ifstream in(infile.c_str());
								if (in.fail())
									throw ns_ex("Could not open file") << infile;

							}
						}
					}
				}
			}
		}
		for (unsigned int xx = 0; xx <2; xx++){//handle both machine and by hand data.
			ns_data_concatenate_specification_type & c(*specs[xx]);
			for (std::vector<std::string>::const_iterator experiment_name = c.names.begin(); experiment_name != c.names.end(); experiment_name++){
				for (ns_survival_files::ns_specification_list::iterator data_spec = spec.specifications.begin(); data_spec != spec.specifications.end(); data_spec++){
					ns_dir dir;
					vector<string> current_directory_files;
					std::string experiment_base_directory(c.base_directory+ DIR_CHAR_STR + *experiment_name + DIR_CHAR_STR + "survival_data");
					std::string cur_dir(experiment_base_directory+ DIR_CHAR_STR + data_spec->sub_dir);
					dir.load_masked(cur_dir,"csv",current_directory_files);
					unsigned long found_count(0);
					std::vector<string>found_filenames;
					for (unsigned int cur_dir_file_id = 0; cur_dir_file_id < current_directory_files.size(); cur_dir_file_id++){
						for (unsigned int i = 0; i < data_spec->files.size(); i++){
							if (current_directory_files[cur_dir_file_id].find(data_spec->files[i].filename)!=current_directory_files[cur_dir_file_id].npos){
								string infile(cur_dir + DIR_CHAR_STR + current_directory_files[cur_dir_file_id]);
								found_filenames.push_back(cur_dir + DIR_CHAR_STR + current_directory_files[cur_dir_file_id]);
								ifstream in(infile.c_str());
								cerr << "Reading " << *experiment_name << "::" << data_spec->sub_dir <<"::" << data_spec->files[i].filename << "...\n";
								if (in.fail())
									throw ns_ex("Could not open file") << infile;
								//remove the header
								if (data_spec->files[i].file != 0) //only write header line once.
									getline(in,tmp,'\n');
								else{
									std::string filename(data_to_concatenate.output_directory + DIR_CHAR_STR+output_title + "=" + data_spec->sub_dir);
									ns_dir::create_directory_recursive(filename);
									filename += DIR_CHAR_STR;
									filename += output_title;
									filename +="=";
									filename += data_spec->files[i].filename + ".csv";
									data_spec->files[i].file = new ofstream(filename.c_str());
									if (data_spec->files[i].file->fail())
										throw ns_ex("Could not open file ") << filename;
								}
								if (data_to_concatenate.require_string.size() == 0){
									*data_spec->files[i].file<< in.rdbuf();
								}
								else{
									string tmp;
									bool first_line(true);
									while (true){
										getline(in,tmp);
										if (in.fail()) break;
										if (first_line | regex_search(tmp,regex_obj))
											*data_spec->files[i].file << tmp << "\n";
										first_line = false;
									}
								}
								in.close();
								found_count++;
								break;
							}
						}
					}
					if (found_count > spec.count()){
						 ns_ex ex("An ambiguity was found looking for the requested files.  Perhaps there are two files with similar names ");
							 ex << "in the results directory for the experiment " << *experiment_name << ":\n";
							for (unsigned long i = 0; i < found_filenames.size(); i++){
								ex << found_filenames[i] << "\n";
							}
							throw ex;
					}
				}
			}
		}
	}
};



int main(int argc, char * argv[])
{
	try{
		string base_dir("Y:\\image_server_storage\\results");

		if (argc > 1){
			ns_data_concatenate_specification c;
			c.machine_data.base_directory = base_dir;
			string spec_file = argv[1];
			ns_xml_simple_object_reader xml;
			ifstream infile(spec_file.c_str());
			if (infile.fail())
				throw ns_ex("Could not read concatentation specification file ") << spec_file;
			xml.from_stream(infile);
			for (unsigned int i = 0; i < xml.objects.size(); i++){
				if (xml.objects[i].name == "experiment_name")
					c.machine_data.names.push_back(xml.objects[i].value);
			
				else if (xml.objects[i].name == "experiment_base_directory")
					c.machine_data.base_directory = xml.objects[i].value;
				
				else if (xml.objects[i].name == "by_hand_file_name")
					c.by_hand_data.names.push_back(xml.objects[i].value);
				
				else if (xml.objects[i].name == "by_hand_base_directory")
					c.by_hand_data.base_directory = xml.objects[i].value;
				
				else if (xml.objects[i].name == "output_directory")
					c.output_directory = xml.objects[i].value;
				
				else if (xml.objects[i].name == "output_title")
					c.output_title = xml.objects[i].value;

				else if (xml.objects[i].name == "require_string")
					c.require_string.push_back(xml.objects[i].value);
				else throw ns_ex("Unknown concatenation specification: ") << xml.objects[i].name;

			}
			if (c.output_directory.size() == 0)
				throw ns_ex("No output directory specified");

			ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			return 0;
		}
		else{
			cerr << "usage: ns_concacenate_files <xml_file>\n";
			return 1;
			if(0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
			//	c.machine_data.names.push_back("2011_04_02_ins_hs_0");
				c.machine_data.names.push_back("2011_04_03_ins_hs_01");
				c.machine_data.names.push_back("2011_04_04_ins_hs_02");
				c.machine_data.names.push_back("2011_04_05_ins_hs_03");
				c.machine_data.names.push_back("2011_04_06_ins_hs_04");
				c.machine_data.names.push_back("2011_04_07_ins_hs_05");
				c.machine_data.names.push_back("2011_04_08_ins_hs_06");
				c.machine_data.names.push_back("2011_04_09_ins_hs_07");
				c.machine_data.names.push_back("2011_04_10_ins_hs_08");
				c.machine_data.names.push_back("2011_04_11_ins_hs_09");
				c.output_directory ="Y:\\image_server_storage\\results\\2011_04_02_ins_hs_0\\analysis";	
				c.output_title = "2011_04_11=thermotolerance_aging";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2013_05_03_qz0_2375C");
				//c.machine_data.names.push_back("2013_01_27_temp_series_25C");
				c.machine_data.names.push_back("2012_08_31_temp_series_3075C");
				c.machine_data.names.push_back("2012_08_24_temp_series_3145C");
				c.machine_data.names.push_back("2012_08_17_temp_series_27C");
				c.machine_data.names.push_back("2012_08_17_temp_series_26C");
				c.machine_data.names.push_back("2012_08_03_temp_series_289C");
				c.machine_data.names.push_back("2012_08_03_temp_series_34C");
				c.machine_data.names.push_back("2012_06_08_temp_series_325C");
				//c.machine_data.names.push_back("2012_06_08_temp_series_295C");
				c.machine_data.names.push_back("2012_04_27_temperature_series_20C");
				c.machine_data.names.push_back("2012_03_16_temp_series_30C");
			//	c.machine_data.names.push_back("2012_03_16_temp_series_30C");
				c.machine_data.names.push_back("2013_03_22_qz0_25C");
				c.machine_data.names.push_back("2013_03_22_qz0_33C");
				c.output_directory = "Y:\\image_server_storage\\results\\2013_05_03_qz0_2375C\\ts_analysis";
				c.output_title = "2013_09_17=temperature_series_focused";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2012_03_02_first_31C");
				c.machine_data.names.push_back("2012_03_02_first_28C");
				c.machine_data.names.push_back("2012_03_16_temp_series_33C");
				c.machine_data.names.push_back("2012_03_16_temp_series_30C");
				c.machine_data.names.push_back("2012_04_20_temp_series_carb_345C");
				c.machine_data.names.push_back("2012_05_04_temp_series_28C");
				c.machine_data.names.push_back("2012_05_04_temp_series_31C");
				c.machine_data.names.push_back("2012_04_27_temperature_series_20C");
				c.machine_data.names.push_back("2012_05_10_temp_series_225C");
				c.machine_data.names.push_back("2012_05_17_temp_series_25C");
				c.machine_data.names.push_back("2012_05_25_temp_series_365C");
				c.machine_data.names.push_back("2012_06_08_temp_series_295C");
				c.machine_data.names.push_back("2012_06_08_temp_series_325C");
				c.machine_data.names.push_back("2012_07_21_temp_series_3375C");
				c.machine_data.names.push_back("2012_07_21_temp_series_3175C");
				c.machine_data.names.push_back("2012_07_21_temp_series_2825C");
				c.machine_data.names.push_back("2012_07_21_temp_series_2875C");
				c.machine_data.names.push_back("2012_08_03_temp_series_34C");
				c.machine_data.names.push_back("2012_08_03_temp_series_289C");
				c.machine_data.names.push_back("2012_08_10_temp_series_355C");
				c.machine_data.names.push_back("2012_08_10_temp_series_322C");
				c.machine_data.names.push_back("2012_08_24_temp_series_295C");
				c.machine_data.names.push_back("2012_08_24_temp_series_3145C");
				c.machine_data.names.push_back("2012_08_31_temp_series_3075C");
				c.machine_data.names.push_back("2012_08_17_temp_series_27C");
				c.machine_data.names.push_back("2012_08_17_temp_series_26C");
				c.machine_data.names.push_back("2012_09_13_temp_series_351C");
				c.machine_data.names.push_back("2013_01_27_temp_series_25C");
				c.machine_data.names.push_back("2013_03_22_qz0_33C");
				c.machine_data.names.push_back("2013_05_03_qz0_2375C");
				c.machine_data.names.push_back("2013_04_12_daf2_age1_25C");
				c.machine_data.names.push_back("2013_04_05_daf2_age1_33C");
				c.machine_data.names.push_back("2013_03_29_daf16_hsf1_25C");
				c.machine_data.names.push_back("2013_03_29_daf16_hsf1_33C");
				c.machine_data.names.push_back("2013_03_22_qz0_25C");
				c.machine_data.names.push_back("2013_05_16_qz0_qz60_never_20C");

				c.output_directory = "Y:\\image_server_storage\\results\\2012_06_08_temp_series_325C\\ts_analysis";
				c.output_title = "2011_07_25=temperature_series";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				
				c.machine_data.names.push_back("2013_06_07_daf2_age1_incubator_1_and_3");
				c.machine_data.names.push_back("2013_06_07_daf2_age1_incubator_4_and_5");
				c.machine_data.names.push_back("2013_06_07_daf2_age1_incubator_2");
				c.machine_data.names.push_back("2013_06_14_daf16_hsf1_incubator_1_and_3");
				c.machine_data.names.push_back("2013_06_14_daf16_hsf1_incubator_4_and_5");
				c.machine_data.names.push_back("2013_06_14_daf16_hsf1_incubator_2");
				
				c.machine_data.names.push_back("2013_06_21_daf16_hsf1_32C_to_35C");
				c.machine_data.names.push_back("2013_06_28_daf2_age1_32C_to_35C");
				c.machine_data.names.push_back("2013_07_12_daf2_age1_21_to_27C");
				c.machine_data.names.push_back("2013_07_19_daf16_hsf1_21_to_27C");
				c.machine_data.names.push_back("2013_09_27_Qz0_Sp11_incubators_1_3");
				c.machine_data.names.push_back("2013_08_15_nuo6_28_33C");
				c.machine_data.names.push_back("2013_08_24_Nuo_6_incubator_2_3_5");
				c.machine_data.names.push_back("2013_09_07_nuo6_hif1_21_to_27C");
				c.machine_data.names.push_back("2014_03_07_tspan_daf2_daf16_age1");
				c.output_directory = "y:\\image_server_storage\\results\\2013_06_14_daf16_hsf1_incubator_2\\analysis";
				c.output_title = "2013_06_17=temp_scaling_controlled";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				
				c.machine_data.names.push_back("2012_12_09_worm_zoo_2_345C");
				c.machine_data.names.push_back("2012_12_09_worm_zoo_2b_345C");
				c.machine_data.names.push_back("2012_12_14_worm_zoo_3_345C");
				c.machine_data.names.push_back("2012_10_19_worm_zoo_345C");
				c.machine_data.names.push_back("2012_12_14_worm_zoo_25C");
				c.machine_data.names.push_back("2012_12_14_worm_zoo_b_25C");
				c.output_directory = "y:\\image_server_storage\\results\\2012_12_14_worm_zoo_b_25C\\analysis";
				c.output_title = "2013_06_17=worm_zoo_composite";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;


				c.machine_data.names.push_back("2012_08_10_temp_series_322C");
				c.machine_data.names.push_back("2012_07_21_temp_series_3375C");
				c.machine_data.names.push_back("2013_03_22_qz0_33C");
				c.machine_data.names.push_back("2013_04_05_daf2_age1_33C");
				c.machine_data.names.push_back("2013_09_20_hsf1_live_dead_NEC");
				c.machine_data.names.push_back("2012_10_05_acquired_thermotolerance_33C");
				c.machine_data.names.push_back("2014_07_05_25_to_33_temp_shift_day3adult");
				c.machine_data.names.push_back("2013_01_27_temp_series_25C");
				c.machine_data.names.push_back("2012_05_17_temp_series_25C");
				c.machine_data.names.push_back("2013_04_12_daf2_age1_25C");
				c.machine_data.names.push_back("2013_03_29_daf16_hsf1_25C");
				c.machine_data.names.push_back("2013_03_22_qz0_25C");
				c.output_directory = "Y:\\image_server_storage\\results\\2014_07_05_25_to_33_temp_shift_day3adult\\analysis";
				c.output_title = "2014_07_29=25_33C=concatenated";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2013_10_19_tbooh_dosage");
				c.machine_data.names.push_back("2013_11_08_tbooh_dosage_2");
				c.machine_data.names.push_back("2013_11_25_tbooh_dosage_3");
				c.machine_data.names.push_back("2013_11_29_tbooh_degredation");
				c.machine_data.names.push_back("2013_12_13_tbooh_dosage_4");
				c.machine_data.names.push_back("2013_12_20_tbooh_dosage_5");
				c.machine_data.names.push_back("2013_12_23_tbooh_degradation_5");
				c.machine_data.names.push_back("2014_02_07_tbooh_dosage_6");
				c.machine_data.names.push_back("2014_02_21_tbooh_dosage_7");
				c.machine_data.names.push_back("2014_04_11_tbooh_8_1");
				c.output_directory = "Y:\\image_server_storage\\results\\2013_11_25_tbooh_dosage_3\\analysis";
				c.output_title = "2013_11_25_tbooh=concatenated";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
	  			ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2014_02_03_multi_temp_shift");
				c.machine_data.names.push_back("2014_02_17_temp_multi_shift_2");
				c.machine_data.names.push_back("2014_03_03_multi_temp_shift_3");
				c.output_directory = "Y:\\image_server_storage\\results\\2014_02_17_temp_multi_shift_2\\analysis";
				c.output_title = "2014_02_17_multi_temp_concatenated";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (1){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2013_05_28_qz0_qz60_day_nineteen_27C");
				c.machine_data.names.push_back("2013_05_25_qz0_qz60_day_sixteen_27C");
				c.machine_data.names.push_back("2013_05_22_qz0_qz60_day_thirteen_27C");
				c.machine_data.names.push_back("2013_05_16_qz0_qz60_day_seven_27C");
				c.machine_data.names.push_back("2013_05_16_qz0_qz60_never_20C");
				c.output_directory = "Y:\\image_server_storage\\results\\2013_05_28_qz0_qz60_day_nineteen_27C\\analysis";
				c.output_title = "2013_06_12=25_27_shift";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (1){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2013_04_12_daf2_age1_25C");
				c.machine_data.names.push_back("2013_03_29_daf16_hsf1_25C");
				c.machine_data.names.push_back("2013_04_05_daf2_age1_33C");
				c.machine_data.names.push_back("2013_03_29_daf16_hsf1_33C");
				c.output_directory = "Y:\\image_server_storage\\results\\2013_04_12_daf2_age1_25C\\analysis";
				c.output_title = "2013_04_12=25_33_comparrison";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2012_07_05_insulins_hs_6");
				c.machine_data.names.push_back("2012_02_28_insulins_hs_5");
				c.machine_data.names.push_back("2011_12_19_lifespan_RT_1");
				c.machine_data.names.push_back("2011_12_05_insulins_hs_4");
				c.machine_data.names.push_back("2011_11_14_insulins_hs_3");
				c.machine_data.names.push_back("2011_11_07_insulins_hs_2");
				c.machine_data.names.push_back("2011_10_31_insulins_hs_1");
				c.output_directory = "Y:\\image_server_storage\\results\\2011_12_19_lifespan_RT_1\\analysis";
				c.output_title = "2011_02_28=insulin_thermotolerance";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			string by_hand_dir = "X:\\stroustrup\\by_hand_lifespandata";
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2010_11_20_ins_25C");
				c.machine_data.names.push_back("2010_11_23_ten_n2");
			//	c.machine_data.names.push_back("2010_11_21_daf2_rnai_25C");
				c.machine_data.names.push_back("2010_12_23_subtilis_tdp1");
			//	c.machine_data.names.push_back("2010_12_20_javier_daf2_rnai");
		//		c.machine_data.names.push_back("2011_02_10_mev_1");
			//	c.machine_data.names.push_back("2011_02_02_isp1");
				c.machine_data.names.push_back("2011_01_31_eat2");
				c.machine_data.names.push_back("2011_05_14_ins_trip_ls");
				c.machine_data.names.push_back("2011_07_13_ins_rep");
		//		c.machine_data.names.push_back("2011_09_26_daf2_alleles_ls");
				c.machine_data.names.push_back("2011_10_01_all_n2");
				c.machine_data.names.push_back("2011_10_19_short_lifespan_1");
				c.machine_data.names.push_back("2011_10_28_mixed_lifespan");
				c.machine_data.names.push_back("2011_11_11_shaken_lifespan");
				c.machine_data.names.push_back("2011_12_02_stirred_lifespan");
				c.machine_data.names.push_back("2011_12_09_iced_lifespan");
				c.machine_data.names.push_back("2011_12_16_nogged_lifespan");
				c.machine_data.names.push_back("2012_01_13_spiked_lifespan");
				c.machine_data.names.push_back("2012_08_03_garnish_lifespan");
				c.machine_data.names.push_back("2012_08_31_relish_ls");;
				c.require_string.push_back("CGC Wildtype");
				c.require_string.push_back("Alcedo Wildtype");
				c.output_directory = "y:\\image_server_storage\\results\\2012_04_04=N2s";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2011_10_19_short_lifespan_1");
				c.machine_data.names.push_back("2011_10_28_mixed_lifespan");
				c.machine_data.names.push_back("2011_11_11_shaken_lifespan");
				c.machine_data.names.push_back("2011_12_02_stirred_lifespan");
				c.machine_data.names.push_back("2011_12_09_iced_lifespan");
				c.machine_data.names.push_back("2011_12_16_nogged_lifespan");
				c.machine_data.names.push_back("2012_01_13_spiked_lifespan");
				//c.machine_data.names.push_back("2012_08_03_garnish_lifespan");
				//c.machine_data.names.push_back("2012_08_31_relish_ls");
				c.output_directory = "y:\\image_server_storage\\results\\2012_01_13_spiked_lifespan\\analysis";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());

			}
			if (0){
				
				ns_data_concatenate_specification c;
				c.machine_data.base_directory = base_dir;
				c.machine_data.names.push_back("2010_12_11_daf2_rnai_hs");
				c.machine_data.names.push_back("2010_12_13_daf2_rnai_hs_2");
				c.machine_data.names.push_back("2010_12_15_daf2_rnai_hs_3");
				c.machine_data.names.push_back("2010_12_17_daf2_rnai_hs_4");
				c.machine_data.names.push_back("2010_12_19_daf2_rnai_hs_5");
				c.machine_data.names.push_back("2010_12_21_daf2_rnai_hs_6");
				c.machine_data.names.push_back("2010_12_23_daf2_rnai_hs_7");
				c.output_directory = "Y:\\image_server_storage\\results\\2010_12_11_daf2_rnai_hs\\analysis";
				c.output_title = "2010_12_23=thermotolerance_aging";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}

			//c.machine_data.names.push_back("2010_12_23_daf2_rnai_hs_7");
			//c.machine_data.names.push_back("2011_02_10_mev_1_hs");
			//c.machine_data.names.push_back("2010_12_21_javier_daf2_rnai_hs");
			//c.machine_data.names.push_back("2011_03_11_daf16_hs_day_2");
			//c.machine_data.names.push_back("2011_03_09_daf16_hs_d0");
		
		
			
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.names.push_back("2010_11_20_ins_25C");
				
				c.by_hand_data.names.push_back("2010_11_20_ins_at_25_byhand_detailed_jmp.csv");
				c.by_hand_data.names.push_back("2010_11_20_ins_at_25_byhand_simple_jmp.csv");
				c.output_directory = "y:\\image_server_storage\\results\\2010_11_23_ten_n2\\analysis";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.names.push_back("2010_11_20_ins_25C");
				c.machine_data.names.push_back("2010_11_23_ten_n2");
			//	c.machine_data.names.push_back("2010_11_21_daf2_rnai_25C");
				c.machine_data.names.push_back("2010_12_23_subtilis_tdp1");
			//	c.machine_data.names.push_back("2010_12_20_javier_daf2_rnai");
		//		c.machine_data.names.push_back("2011_02_10_mev_1");
				c.machine_data.names.push_back("2011_02_02_isp1");
				c.machine_data.names.push_back("2011_01_31_eat2");
				c.machine_data.names.push_back("2011_05_14_ins_trip_ls");
				c.by_hand_data.base_directory = by_hand_dir;
				c.by_hand_data.names.push_back("2010_11_20_ins_at_25_byhand_jmp.csv");
			//	c.by_hand_data.names.push_back("2010_11_21_daf2_rnai_25C_byhand_jmp.csv");
				c.by_hand_data.names.push_back("2010_11_23_ten_N2_jmp.csv");
				c.output_directory = "y:\\image_server_storage\\results\\2010_11_23_ten_n2\\analysis";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			
			}
			if (0){
				ns_data_concatenate_specification c;
				c.machine_data.names.push_back("2010_11_21_ins_20C");
				c.machine_data.names.push_back("2010_11_22_daf2_rnai_20C");
				c.by_hand_data.base_directory = by_hand_dir;
				c.by_hand_data.names.push_back("2010_11_21_ins_at_20C_byhand_jmp.csv");
				c.by_hand_data.names.push_back("2010_11_22_daf2_rnai_20C_byhand_jmp.csv");
				c.output_directory= "Y:\\image_server_storage\\results\\2010_11_21_ins_20C\\analysis";
				ns_data_concatenate::concatenate(c,ns_survival_files::simple());
			}
		}
		return 0;
	}
	catch(ns_ex & ex){
		cerr << "Error: " << ex.text() << "\n";
		for (unsigned int i = 0; i < 5; i++){
			cerr << 5-i << "...";
			ns_thread::sleep(1);
		}
		return 1;
	}
}