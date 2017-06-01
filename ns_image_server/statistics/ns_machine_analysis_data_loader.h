#ifndef NS_MACHINE_ANALYSIS_DATA_LOADER
#define NS_MACHINE_ANALYSIS_DATA_LOADER
#include "ns_sql.h"
#include "ns_death_time_annotation_set.h"
#include "ns_movement_measurement.h"
#include "ns_time_path_image_analyzer.h"
#include <iostream>

class ns_machine_analysis_region_data{
public:
	
	typedef enum {ns_load_all,ns_exclude_fast_moving_animals} ns_loading_details;

	ns_machine_analysis_region_data():summary_series_generated_(false){}
	ns_death_time_annotation_set death_time_annotation_set;
	mutable ns_region_metadata metadata;
	void calculate_survival(){}
	const ns_worm_movement_summary_series & summary_series(const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_strategy_,const ns_death_time_annotation::ns_multiworm_censoring_strategy & cs,
		ns_death_time_annotation::ns_missing_worm_return_strategy & css, const ns_animals_that_slow_but_do_not_die_handling_strategy & sls) const{
		if (!summary_series_generated_){
			ns_death_time_annotation_compiler c;
			c.add(death_time_annotation_set);
			cached_summary_series.from_death_time_annotations(by_hand_strategy_,cs,css,c,sls);
			summary_series_generated_=true;
		}
		return cached_summary_series;
	}
	
	bool recalculate_from_saved_movement_quantification(const ns_64_bit region_id,ns_sql & sql);

	bool load_from_db(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_types_to_load,const ns_loading_details & details,
		const ns_64_bit region_id,ns_sql & sql);
	
	ns_time_path_solution time_path_solution;
	ns_time_path_image_movement_analyzer time_path_image_analyzer;

private:
	mutable ns_worm_movement_summary_series cached_summary_series;
	mutable bool summary_series_generated_;
	//ns_time_path_image_movement_analyzer time_path_image_analyzer;
	//ns_time_path_solution time_path_solution;
};

class ns_machine_analysis_sample_data{
public:
	void load(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_types_to_load,
		const ns_64_bit sample_id, const ns_region_metadata & sample_metadata, ns_sql & sql,
		const ns_64_bit specific_region_id=0, const bool include_excluded_regions=false,
		const ns_machine_analysis_region_data::ns_loading_details & loading_details=ns_machine_analysis_region_data::ns_load_all);
	ns_machine_analysis_sample_data(){}
	ns_machine_analysis_sample_data(const ns_64_bit id):sample_id_(id){}
	std::vector<ns_machine_analysis_region_data> regions;
	ns_64_bit id() const {return sample_id_;}
	const std::string & name() const {return sample_name_;}
	const std::string & device_name() const {return device_name_;}
	void set_id(const ns_64_bit id){sample_id_ =id;}
	void clear(){sample_id_ = 0; device_name_.clear(); sample_name_.clear(); regions.clear();}
private:
	std::string device_name_;
	std::string sample_name_;
	ns_64_bit sample_id_;
};

class ns_machine_analysis_data_loader{
public:
	void clear(){
		samples.resize(0);
		experiment_id_ = 0;
		experiment_name_.resize(0);
		total_number_of_regions_ = 0;
	}
	ns_machine_analysis_data_loader(const bool be_quiet_=false):be_quiet(be_quiet_){}
	unsigned long total_number_of_regions()const{return total_number_of_regions_;}
	void load(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_types_to_load,const ns_64_bit region_id, ns_64_bit sample_id, ns_64_bit experiment_id_a, ns_sql & sql,
				const bool include_excluded_regions=false, const ns_machine_analysis_region_data::ns_loading_details & details=ns_machine_analysis_region_data::ns_load_all);
	//uses much less memory just to compile the data into a survival curve data set
	void load_just_survival(ns_lifespan_experiment_set & set,ns_64_bit region_id, ns_64_bit sample_id, ns_64_bit experiment_id_a, ns_sql & sql, const bool load_excluded_regions, const bool load_by_hand_data);
	const std::string & experiment_name(){return experiment_name_;}
	const ns_64_bit experiment_id(){return experiment_id_;}
	std::vector<ns_machine_analysis_sample_data> samples;
private:
	bool be_quiet;
	void set_up_spec_to_load(const ns_64_bit & region_id, ns_64_bit &sample_id, ns_64_bit & experiment_id_a, ns_sql & sql, const bool load_excluded_regions);
	std::string experiment_name_;
	ns_64_bit experiment_id_;
	unsigned long total_number_of_regions_;
};

#endif
