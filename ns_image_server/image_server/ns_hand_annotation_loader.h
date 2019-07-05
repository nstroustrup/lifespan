#ifndef NS_HAND_ANNOTATION_LOADER_H
#define NS_HAND_ANNOTATION_LOADER_H

#include "ns_image_server.h"
#include "ns_death_time_annotation_set.h"

class ns_hand_annotation_loader{
	
public:
	
	ns_death_time_annotation_compiler annotations;

	ns_region_metadata load_region_annotations(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_type_to_load,const ns_64_bit region_id, ns_sql & sql){
		ns_region_metadata m;
		m.load_from_db(region_id,"",sql);
		load_region_annotations(annotation_type_to_load,region_id,m.experiment_id,m.experiment_name,m,sql);
		return m;
	}
	bool load_region_annotations(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_type_to_load,const ns_64_bit region_id,const ns_64_bit experiment_id, const std::string & experiment_name,const ns_region_metadata & metadata,ns_sql & sql){
		ns_image_server_results_subject results_subject;
		results_subject.experiment_id = experiment_id;
		results_subject.experiment_name = experiment_name;
		results_subject.region_id = metadata.region_id;
		results_subject.region_name = metadata.region_name;
		results_subject.sample_name = metadata.sample_name;

		annotations.specifiy_region_metadata(metadata.region_id,metadata);

		ns_acquire_for_scope<ns_istream> in(image_server.results_storage.hand_curated_death_times(results_subject,sql).input());
		if (in.is_null()){
			in.release();
			return false;
		}
		ns_death_time_annotation_set set;
		set.read(annotation_type_to_load,in()());
		in.release();
		for (unsigned int i = 0; i < set.size(); i++){
			if (set[i].annotation_source == ns_death_time_annotation::ns_lifespan_machine){
				std::cerr << "Yikes! Lifespan machine annotations found in a by_hand annotation file! The annotation label has been changed, but whoever wrote the file made a mistake.";
				set[i].annotation_source = ns_death_time_annotation::ns_storyboard;
				set[i].annotation_source_details = "ns_by_hand_annotation_loader::Fixed to by hand during loading";
			}
		}
		annotations.add(set);
		return true;

	}
	void load_experiment_annotations(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_type_to_load,const ns_64_bit experiment_id,ns_sql & sql){
		sql << "SELECT name FROM experiments WHERE id = " << experiment_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_hand_annotation_loader::load_experiment_annotations()::Could not find experiment in db");
		std::string experiment_name = res[0][0];

		sql << "SELECT id,name, device_name,incubator_name,incubator_location FROM capture_samples WHERE experiment_id=" << experiment_id << " ORDER BY name ASC"; //load in annotations from censored regions
																												//as they are still legit by hand annotations and might be useful
		sql.get_rows(res);
		ns_region_metadata metadata;
		metadata.experiment_name = experiment_name;
		for (unsigned int i = 0; i < res.size(); i++){
			sql << "SELECT id,name, strain,time_at_which_animals_had_zero_age FROM sample_region_image_info WHERE sample_id= " << res[i][0] << " ORDER BY name ASC"; //load in annotations from censored regions
			ns_sql_result res2;
			sql.get_rows(res2);
			metadata.sample_name = res[i][1];
			metadata.device = res[i][2];
			metadata.incubator_name = res[i][3];
			metadata.incubator_location = res[i][4];
			for (unsigned int j = 0; j < res2.size(); j++){
				metadata.region_id = ns_atoi64(res2[j][0].c_str());
				metadata.region_name = res2[j][1];
				metadata.strain = res2[j][2];
				metadata.technique = ns_region_metadata::by_hand_technique_label();
				metadata.analysis_type = ns_region_metadata::by_hand_technique_label();
				metadata.time_at_which_animals_had_zero_age = atol(res2[j][3].c_str());
				try{
					load_region_annotations(annotation_type_to_load,metadata.region_id,experiment_id,experiment_name,metadata,sql);
				}
				catch(ns_ex & ex){
					throw ns_ex("Could not load by hand annotations for region ") << metadata.experiment_name << "::" << metadata.sample_name << "::" << metadata.region_name << " (" << metadata.region_id << ")"
						":" << ex.text() << ".  The easiest way to fix this would be to delete the annotation file and start again.";
				}

			}
		}
		
	}
};
#endif
