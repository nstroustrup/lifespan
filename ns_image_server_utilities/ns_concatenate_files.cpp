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
		}
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