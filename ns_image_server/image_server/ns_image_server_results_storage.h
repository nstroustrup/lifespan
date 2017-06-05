#ifndef NS_IMAGE_SERVER_RESULTS_STORAGE_H
#define NS_IMAGE_SERVER_RESULTS_STORAGE_H
#include "ns_ex.h"
#include "ns_file_location_specification.h"
#include "ns_death_time_annotation.h"
#include "ns_image_storage.h"
#include "ns_sql.h"
#include <iostream>
#include <fstream>

class ns_image_server_results_subject{
	std::string add_if_extant(const std::string & s){if (s.size() > 0)return s+"="; return "";}
	std::string add_if_extant(const ns_64_bit id){if (id != 0) return ns_to_string(id)+"="; return "";}
	std::string add_time_interval(){if (start_time == 0 && stop_time == 0) return ""; return ns_to_string(start_time) + "_" + ns_to_string(stop_time) + "=";}
	public:
	ns_image_server_results_subject():experiment_id(0),sample_id(0),region_id(0),start_time(0),stop_time(0){}

	ns_64_bit experiment_id,
				  sample_id,
				  region_id;
	std::string device_name;

	unsigned long start_time,
				   stop_time;

	std::string experiment_name,
				sample_name,
				region_name;

	void get_names(ns_sql & sql){
		if (region_id != 0 && 
			(sample_id == 0 || experiment_id == 0 ||
			region_name.size() == 0 || sample_name.size() == 0 || experiment_name.size() == 0))
			ns_region_info_lookup::get_region_info(region_id,&sql, region_name, sample_name,sample_id, experiment_name,experiment_id);
		if (sample_id != 0 && (
			experiment_id == 0 ||
			sample_name.size() == 0 || experiment_name.size() == 0))
			ns_region_info_lookup::get_sample_info(sample_id,&sql, sample_name, experiment_name,experiment_id);
		if (experiment_id != 0 && experiment_name.size() == 0)
			ns_region_info_lookup::get_experiment_info(experiment_id,&sql, experiment_name);
	}
	std::string region_filename(unsigned int max_length=40){
		return 	add_if_extant(ns_image_server_results_subject::create_short_name(experiment_name,max_length)) + add_if_extant(experiment_id) +
				add_if_extant(sample_name) + add_if_extant(sample_id) +
				add_if_extant(region_name) + add_if_extant(region_id) + add_time_interval();
	}
	std::string experiment_filename(){
		return 	add_if_extant(experiment_name) + add_time_interval();
	}
	
	static std::string create_short_name(std::string name, const unsigned long limit=40){
		return ns_shorten_filename(name,limit);
	}
	std::string device_filename(){
		return device_name + "=" + add_time_interval();
	}

};

class ns_image_server_results_file{
	public:
	ns_image_server_results_file(const std::string & p,const std::string & d,const std::string & f):long_term_directory(p),relative_directory(d),filename(f){}
	std::ostream * output(){

		ns_dir::create_directory_recursive(dir());
		std::ofstream * o(new std::ofstream(path().c_str()));
		if (o->fail()){
			delete o;
			throw ns_ex("ns_image_server_results_file::output()::Could not open ") << filename;
		}
		return o;
	}
	std::istream * input(){
		std::ifstream * i(new std::ifstream(path().c_str()));
		if (i->fail()){
			delete i;
			return 0;
		}
		return i;
	}
	bool erase(){
		return ns_dir::delete_file(path());
	}
	const std::string & output_filename() const{return filename;}
	private:
	std::string long_term_directory,relative_directory, filename;
	std::string path(){return dir() + DIR_CHAR_STR + filename;}
	std::string dir() { return long_term_directory + DIR_CHAR_STR + relative_directory; }
	friend class ns_image_server_results_storage;
	friend class ns_image_storage_handler;
};
class ns_image_server_results_storage{
	static std::string multi_experiment_data_folder(){return "multiple_experiment_data";}
	static std::string survival_data_folder(){return "survival_data";}
	static std::string image_statistics_folder(){return "image_statistics";}
	static std::string movement_timeseries_folder(){return "movement_timeseries";}
	static std::string hand_curation_folder(){return "hand_curation";}
	static std::string machine_death_time_annotations(){return "machine_event_annotations";}
	static std::string animal_position_folder(){return "animal_position";}
	static std::string device_data_folder(){return "device_data";}
	static std::string animal_event_folder(){return "animal_event_data";}
	static std::string machine_learning_training_set_folder(){return "machine_learning_training_set_images";}
	static std::string time_path_image_analysis_quantification(){return "posture_analysis";}
public:
	void set_results_directory(const std::string & dir){results_directory = dir;}

	ns_image_server_results_file capture_sample_image_statistics(ns_image_server_results_subject & spec, ns_sql & sql,bool multi_experiment=false) const{
		spec.get_names(sql);
		return ns_image_server_results_file(results_directory, ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR +image_statistics_folder(),
											ns_image_server_results_subject::create_short_name(spec.experiment_filename())  + "capture_sample_image_statistics.csv");
	}

	std::string db_export_directory(ns_image_server_results_subject & spec,ns_sql & sql){
		spec.get_names(sql);
		return results_directory + DIR_CHAR_STR + ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR 
					+ spec.experiment_name + "=db_export";
	}

	ns_image_server_results_file capture_region_image_statistics(ns_image_server_results_subject & spec,ns_sql & sql,bool multi_experiment=false) const {
		spec.get_names(sql);
		std::string dir(multi_experiment?multi_experiment_data_folder() + DIR_CHAR_STR + image_statistics_folder()
										:ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + image_statistics_folder());
		return ns_image_server_results_file(results_directory, dir,
											ns_image_server_results_subject::create_short_name(spec.experiment_filename()) + "capture_region_image_statistics.csv");
	}
	
	ns_image_server_results_file device_capture_image_statistics(ns_image_server_results_subject & spec, ns_sql & sql)const{
		spec.get_names(sql);
		return ns_image_server_results_file(results_directory, multi_experiment_data_folder() + DIR_CHAR_STR +image_statistics_folder(),
											spec.device_filename() + "capture_device_image_statistics.csv");
	}

	ns_image_server_results_file device_history(ns_image_server_results_subject & spec, ns_sql & sql)const{
		spec.get_names(sql);
		return ns_image_server_results_file(results_directory, multi_experiment_data_folder() + DIR_CHAR_STR +device_data_folder(), 
											spec.device_filename() + "capture_device_history.csv");
	}

	ns_image_server_results_file survival_data(ns_image_server_results_subject & spec,const std::string & sub_dir,const std::string & format_type,const std::string & extension, ns_sql & sql,bool make_matlab_safe=false)const{
		spec.get_names(sql);
		std::string dir(ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR +survival_data_folder());
		if (!sub_dir.empty())
			dir+=DIR_CHAR_STR + sub_dir;
		std::string file(ns_image_server_results_subject::create_short_name(spec.experiment_filename()) + "survival=" + format_type + "." + extension);
		if (make_matlab_safe){
			for (unsigned int i = 0; i < file.size(); i++){
				switch(file[i]){
					case '=':file[i]='_';
				}
			}
			if (file.size() > 0 && isdigit(file[0]))
				file = std::string("m") + file;
		}

		return ns_image_server_results_file(results_directory,dir,file);
	
		
	}
	static void replace_spaces_by_underscores(std::string & s){
		for (unsigned int i = 0; i < s.size(); i++)
			if (s[i] == ' ')
				s[i] = '_';
	}
	ns_image_server_results_file animal_event_data(ns_image_server_results_subject & spec,const std::string & format_type, ns_sql & sql) const{
		spec.get_names(sql);
		return ns_image_server_results_file(results_directory, ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + survival_data_folder(), 
											ns_image_server_results_subject::create_short_name(spec.experiment_filename()) + "animal_events=" + format_type + ".csv");
	}
	typedef enum{ns_censoring_and_movement_transitions,ns_worm_position_annotations} ns_death_time_annotation_file_type;
	static std::string death_time_annotation_file_type_label(const ns_death_time_annotation_file_type & t) {
		switch(t){
		case ns_censoring_and_movement_transitions: return "transitions";
		case ns_worm_position_annotations: return "positions";
		default: throw ns_ex("Unknown death time annotation file type");
		}
	}
	ns_image_server_results_file machine_death_times(ns_image_server_results_subject & spec, const ns_death_time_annotation_file_type & type,const std::string & analysis_type,ns_sql & sql) const{		
		spec.get_names(sql);
		return ns_image_server_results_file(results_directory,ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + machine_death_time_annotations(),
											spec.region_filename() + "machine_event_annotations=" + death_time_annotation_file_type_label(type) + "=" + analysis_type + ".csv");
	}
	ns_image_server_results_file hand_curated_death_times(ns_image_server_results_subject & spec, ns_sql & sql) const{		
		spec.get_names(sql);
		return ns_image_server_results_file(results_directory,ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + hand_curation_folder(),
											spec.region_filename() + "hand_curation.xml");
	}

	ns_image_server_results_file time_path_image_analysis_quantification(ns_image_server_results_subject & spec,const std::string & type, const bool store_in_results_directory,ns_sql & sql, bool abbreviated_time_series=false, bool compress_file_names=true) const;

	ns_image_server_results_file movement_timeseries_plot(ns_image_server_results_subject & spec,const std::string & type, ns_sql & sql) const{	
		spec.get_names(sql);
		if (spec.region_id != 0)
			return ns_image_server_results_file(results_directory,ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + movement_timeseries_folder() + DIR_CHAR_STR + "regions",
												spec.region_filename() + type + ".svg");
		else
			return ns_image_server_results_file(results_directory,ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + movement_timeseries_folder(),
												ns_image_server_results_subject::create_short_name(spec.experiment_filename()) + type + ".svg");
	}

	ns_image_server_results_file movement_timeseries_data(const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_strategy, 
		const ns_death_time_annotation::ns_multiworm_censoring_strategy & multiworm_strategy,
		const ns_death_time_annotation::ns_missing_worm_return_strategy & missing_worm_return_strategy,
		const ns_animals_that_slow_but_do_not_die_handling_strategy & partial_path_strategy,
		ns_image_server_results_subject & spec,const std::string & type, const std::string & details,ns_sql & sql) const{	
		spec.get_names(sql);
		std::string sub_dir(type);
		replace_spaces_by_underscores(sub_dir);
		std::string filename(type
			+ "=" + details
			+ "=" + ns_death_time_annotation::by_hand_annotation_integration_strategy_label_short(by_hand_strategy)
			+ "=" + ns_death_time_annotation::multiworm_censoring_strategy_label_short(multiworm_strategy) 
			+ "="+ ns_death_time_annotation::missing_worm_return_strategy_label_short(missing_worm_return_strategy) 
			+ "=" + ns_animals_that_slow_but_do_not_die_handling_strategy_label_short(partial_path_strategy));
		replace_spaces_by_underscores(filename);
		if (spec.region_id != 0)
			return ns_image_server_results_file(
			results_directory ,ns_image_server_results_subject::create_short_name(spec.experiment_name) + 
			DIR_CHAR_STR + movement_timeseries_folder() + DIR_CHAR_STR + "single_regions" + DIR_CHAR_STR + sub_dir,
			spec.region_filename(30) + filename + ".csv");
		else
			return ns_image_server_results_file(
			results_directory,ns_image_server_results_subject::create_short_name(spec.experiment_name) + 
			DIR_CHAR_STR + movement_timeseries_folder() + DIR_CHAR_STR + sub_dir,
					ns_image_server_results_subject::create_short_name(spec.experiment_filename()) + filename + ".csv");
	}

	ns_image_storage_reciever_handle<ns_8_bit> machine_learning_training_set_image(ns_image_server_results_subject & spec,const unsigned long max_line_length, ns_sql & sql);


	ns_image_storage_reciever_handle<ns_8_bit> movement_timeseries_collage(ns_image_server_results_subject & spec,const std::string & graph_type ,const ns_image_type & image_type, const unsigned long max_line_length, ns_sql & sql);

	typedef enum {ns_3d_plot,ns_3d_plot_with_annotations,ns_3d_plot_launcher,ns_3d_plot_launcher_environment} ns_timeseries_3d_plot_type;

	ns_image_server_results_file animal_position_timeseries_3d(ns_image_server_results_subject & spec,ns_sql & sql,ns_timeseries_3d_plot_type type =  ns_3d_plot, const std::string & calibration_data = "") const{	
		spec.get_names(sql);
		std::string calibration_dir = calibration_data.empty()?"":(std::string(DIR_CHAR_STR) + "calibration");
		std::string calibration_str = calibration_data.empty()?"":std::string("=") + calibration_data;
		if (type==ns_3d_plot_launcher_environment) //location of the R file that sets up the environment for viewing 3d plot files
			return ns_image_server_results_file(results_directory, ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + animal_position_folder() + calibration_dir,
										"set_environment.r");
		//location of the R file that either contains or loads the 3d plot data
		return ns_image_server_results_file(results_directory ,ns_image_server_results_subject::create_short_name(spec.experiment_name) + DIR_CHAR_STR + animal_position_folder() + calibration_dir,
											spec.region_filename() + "animal_position_3d" + ((type==ns_3d_plot_with_annotations)?"_with_hand_annotations":"")+calibration_str + ((type==ns_3d_plot_launcher)?".r":".csv"));
	}

	void write_animal_position_timeseries_3d_launcher(ns_image_server_results_subject & spec,const ns_timeseries_3d_plot_type & graph_type,ns_sql & sql, const std::string & calibration_data = "") const{	

		ns_image_server_results_file launcher_file_spec(animal_position_timeseries_3d(spec,sql,ns_3d_plot_launcher,calibration_data));
		ns_image_server_results_file launcher_file_environment_spec(animal_position_timeseries_3d(spec,sql,ns_3d_plot_launcher_environment,calibration_data));
		ns_image_server_results_file subject_file_spec(animal_position_timeseries_3d(spec,sql,graph_type,calibration_data));

		ns_acquire_for_scope<std::ostream> out(launcher_file_spec.output());
		out() << "#" << spec.region_filename() << "\n";
		out() << "rd <- read.csv(\"";
		out() << subject_file_spec.filename;
		out() << "\")\n";
		out() << "title_text <- \"" << spec.region_filename() << "\"\n";
		out() << "ns_plot_3d_animal_movement(rd,title_text)\n";
		out.release();

		const std::string dir(launcher_file_spec.dir());
		ns_acquire_for_scope<std::ostream> out2(launcher_file_environment_spec.output());
		out2() << "#" << dir << "\n";
		out2() << "setwd(\"";
		for (unsigned int i = 0; i < dir.size(); i++){
			if (dir[i] == '\\')
				out2() << "\\\\";
			else out2() << dir[i];
		}
		out2() << "\")\n";
		out2.release();
	}

	private:
	std::string results_directory;
};

#endif
