#include "ns_machine_analysis_data_loader.h"
#include "ns_image_server.h"
#include "ns_survival_curve.h"
#include "ns_time_path_image_analyzer.h"
#include <iostream>
#include "ns_hand_annotation_loader.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#ifdef ZOO23
bool ns_machine_analysis_region_data::load_from_db(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_type_to_load,const ns_loading_details & details,const ns_64_bit region_id,ns_sql & sql){
	death_time_annotation_set.clear();
	metadata.region_id = region_id;
	if (annotation_type_to_load == ns_death_time_annotation_set::ns_recalculate_from_movement_quantification_data)
		return recalculate_from_saved_movement_quantification(region_id,sql);
	
	ns_image_server_results_subject results_subject;
	results_subject.region_id = region_id;
	std::vector<ns_image_server_results_storage::ns_death_time_annotation_file_type> files_to_open;
	switch(annotation_type_to_load){
		case ns_death_time_annotation_set::ns_all_annotations:
		case ns_death_time_annotation_set::ns_censoring_and_movement_states:
			files_to_open.push_back(ns_image_server_results_storage::ns_worm_position_annotations);
			files_to_open.push_back(ns_image_server_results_storage::ns_censoring_and_movement_transitions);
			break;
		case ns_death_time_annotation_set::ns_censoring_data:
		case ns_death_time_annotation_set::ns_movement_transitions:
		case ns_death_time_annotation_set::ns_censoring_and_movement_transitions:
			files_to_open.push_back(ns_image_server_results_storage::ns_censoring_and_movement_transitions);
			break;
		case ns_death_time_annotation_set::ns_movement_states:
			files_to_open.push_back(ns_image_server_results_storage::ns_worm_position_annotations);
		case ns_death_time_annotation_set::ns_no_annotations:
			break;
		default: throw ns_ex("ns_machine_analysis_data_loader::Unknown annotation type request");
	}
	bool could_load_all_files(true);
	for (unsigned int i = 0; i < files_to_open.size(); i++){
		ns_image_server_results_file results(image_server.results_storage.machine_death_times(results_subject,files_to_open[i],"time_path_image_analysis",sql));
		ns_acquire_for_scope<std::istream> tp_i(results.input());
		if (tp_i.is_null()){
			could_load_all_files = false;
			continue;
		}
		death_time_annotation_set.read(annotation_type_to_load,tp_i(),details==ns_machine_analysis_region_data::ns_exclude_fast_moving_animals);
		tp_i.release();
	}
	if (!could_load_all_files)
		return false;
	//remove out of bounds data
/*	for (unsigned int i = 0 ; i < death_time_annotation_set.size(); i++)
		if (death_time_annotation_set.events[i].type == ns_moving_worm_disappearance){
			ns_death_time_annotation a(death_time_annotation_set.events[i]);
			std::cerr << a.description() << "\n";

		}*/
	if (metadata.time_of_last_valid_sample != 0){
		for (ns_death_time_annotation_set::iterator p = death_time_annotation_set.begin(); p != death_time_annotation_set.end();){
//			if(p->stationary_path_id.group_id == 28)
//				std::cerr << "MA";
			if (p->time.fully_unbounded())
				throw ns_ex("Fully unbounded interval encountered!");
			if ((!p->time.period_start_was_not_observed && p->time.period_start > metadata.time_of_last_valid_sample))
				p->time.period_start_was_not_observed = true;
			if ((!p->time.period_end_was_not_observed && p->time.period_end > metadata.time_of_last_valid_sample))
				p->time.period_end_was_not_observed = true;
			if (p->time.fully_unbounded())
				p = death_time_annotation_set.erase(p);
			else{
				//events that straddle the externally specified last observation become unbounded
				if (p->time.period_end > metadata.time_of_last_valid_sample){
					p->time.period_end_was_not_observed = true;
				}
				p++;

			}
		}
	}
	
	return true;
}

bool ns_machine_analysis_region_data::recalculate_from_saved_movement_quantification(const ns_64_bit region_id,ns_sql & sql){
	death_time_annotation_set.clear();
	metadata.region_id = region_id;
	//load 3d point cloud

	const ns_time_path_solver_parameters solver_parameters(ns_time_path_solver_parameters::default_parameters(region_id,sql));
	const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id,sql));
	ns_time_path_solver solver;
	solver.load(region_id,sql);
	solver.solve(solver_parameters,time_path_solution);
	time_path_solution.save_to_db(region_id,sql);
	//time_path_solution.load_from_db(region_id,sql);

	ns_image_server::ns_posture_analysis_model_cache::const_handle_t posture_analysis_model_handle;
	image_server.get_posture_analysis_model_for_region(region_id, posture_analysis_model_handle, sql);
	//load cached movement quantification
		ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
				ns_get_death_time_estimator_from_posture_analysis_model(posture_analysis_model_handle().model_specification));

	time_path_image_analyzer->load_completed_analysis(region_id,time_path_solution,time_series_denoising_parameters, &death_time_estimator(),sql);
	death_time_estimator.release();
	//generate annotations from quantification
	
	time_path_image_analyzer->produce_death_time_annotations(death_time_annotation_set);
	
	time_path_image_analyzer->clear_annotations();
	//save annotations
	ns_image_server_results_subject results_subject;
	results_subject.region_id = region_id;
	ns_image_server_results_file censoring_results(image_server.results_storage.machine_death_times(results_subject,ns_image_server_results_storage::ns_censoring_and_movement_transitions,
				"time_path_image_analysis",sql));
	ns_image_server_results_file state_results(image_server.results_storage.machine_death_times(results_subject,ns_image_server_results_storage::ns_worm_position_annotations,
				"time_path_image_analysis",sql));
	ns_acquire_for_scope<std::ostream> censoring_out(censoring_results.output());
	ns_acquire_for_scope<std::ostream> state_out(state_results.output());
	death_time_annotation_set.write_split_file_column_format(censoring_out(),state_out());
	censoring_out.release();
	state_out.release();
	return true;
}

void ns_machine_analysis_sample_data::load(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_type_to_load,const ns_64_bit sample_id, const ns_region_metadata & sample_metadata,ns_sql & sql, 
	const ns_64_bit specific_region_id, const bool include_excluded_regions, const  ns_machine_analysis_region_data::ns_loading_details & loading_details){
	bool calculate_missing_data = false;
	device_name_ = sample_metadata.device;
	ns_sql_result reg;
	sql << "SELECT r.id FROM sample_region_image_info as r WHERE r.sample_id = " << sample_id << " AND r.censored=0 ";
	if (!include_excluded_regions)
			sql << " AND r.excluded_from_analysis=0";
	if (specific_region_id!=0)
		sql << " AND r.id = " << specific_region_id;
	sql << " ORDER BY r.name";

	sql.get_rows(reg);
	if (reg.empty() && specific_region_id!=0)
		throw ns_ex("Could not identify region ") << specific_region_id << ".  Was it excluded?";
	regions.reserve(reg.size());
	for (unsigned int i = 0; i < reg.size(); i++){
		try{
			const unsigned int s = regions.size();
			regions.resize(s+1);
			regions[s] = new ns_machine_analysis_region_data;
			ns_64_bit region_id = ns_atoi64(reg[i][0].c_str());
			regions[s]->metadata = sample_metadata;
			regions[s]->metadata.load_only_region_info_from_db(region_id,"",sql);
			regions[s]->metadata.technique = "Lifespan Machine";
			regions[s]->load_from_db(annotation_type_to_load,loading_details,region_id,sql);
			//break;
		}
		catch(ns_ex & ex){
			std::cerr << (*regions.rbegin())->metadata.sample_name << "::" << (*regions.rbegin())->metadata.region_name << ": " << ex.text() << "\n";
			regions.pop_back();
		}
	}
	sample_name_ = sample_metadata.sample_name;
	sample_id_ = sample_id;
}

void ns_machine_analysis_data_loader::load_just_survival(ns_lifespan_experiment_set & set,const ns_64_bit region_id, ns_64_bit sample_id, ns_64_bit experiment_id_a, ns_sql & sql, const bool load_excluded_regions, const bool load_by_hand_data){
	set_up_spec_to_load(region_id,sample_id,experiment_id_a,sql,load_excluded_regions);
	
	ns_region_metadata metadata;
	metadata.clear();
	metadata.experiment_name = experiment_name_;
	metadata.experiment_id = experiment_id_a;
	
	std::cerr << "Loading Surival Information\n";
	
	ns_genotype_fetcher genotypes;
	genotypes.load_from_db(&sql);
	//load in just the sample information, one sample at a time to reduce memory consumption
	long last_r(-10);
	for (unsigned int i = 0; i < samples.size(); i++){
		long r = (100 * i) / samples.size();
		if (r - last_r >= 10) {
			std::cout << r << "%...";
			last_r = r;
		}
		metadata.load_only_sample_info_from_db(samples[i].id(),sql);
		samples[i].load(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,metadata.sample_id,metadata,sql,region_id,load_excluded_regions);
		ns_death_time_annotation_compiler compiler;
		for (unsigned int j = 0 ; j < samples[i].regions.size(); j++){
			samples[i].regions[j]->metadata.genotype = genotypes.genotype_from_strain(samples[i].regions[j]->metadata.strain,&sql);
			compiler.add(samples[i].regions[j]->death_time_annotation_set,samples[i].regions[j]->metadata);
			if (load_by_hand_data){
				ns_hand_annotation_loader hand_loader;
				hand_loader.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,samples[i].regions[j]->metadata.region_id,sql);
				compiler.add(hand_loader.annotations);
			}
		}
		set.curves.reserve(set.curves.size()+compiler.regions.size());
		for (ns_death_time_annotation_compiler::ns_region_list::iterator p = compiler.regions.begin(); p != compiler.regions.end(); p++){
			const unsigned long s(set.curves.size());
			set.curves.resize(s+1);
			p->second.generate_survival_curve(set.curves[s],ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,true,false);
		}
		samples[i].clear();
	
	}
	std::cerr << "\n";
}
void ns_machine_analysis_data_loader::load(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotation_types_to_load,const ns_64_bit region_id, ns_64_bit sample_id, ns_64_bit experiment_id_a, ns_sql & sql,
				const bool load_excluded_regions, const ns_machine_analysis_region_data::ns_loading_details & details){
	set_up_spec_to_load(region_id,sample_id,experiment_id_a,sql,load_excluded_regions);

	ns_region_metadata metadata;
	metadata.clear();
	metadata.experiment_name = experiment_name_;
	metadata.experiment_id = experiment_id_a;
	total_number_of_regions_ = 0;
	if(!be_quiet)
	std::cerr << "Loading " << ns_death_time_annotation_set::annotation_types_to_string(annotation_types_to_load) << "\n";
	
	ns_genotype_fetcher genotypes;
	genotypes.load_from_db(&sql);
	long last_r(-10);
	for (unsigned int i = 0; i < samples.size(); i++){
		if (!be_quiet) {
			long r = (100 * i) / samples.size();
			if (r - last_r >= 10) {
				std::cout << r << "%...";
				last_r = r;
			}
		}
		metadata.load_only_sample_info_from_db(samples[i].id(),sql);
	//	if (metadata.sample_name != "frog_c")
	//		continue;
		samples[i].load(annotation_types_to_load,metadata.sample_id,metadata,sql,region_id,load_excluded_regions,details);
		for (unsigned int j = 0 ; j < samples[i].regions.size(); j++)	
			samples[i].regions[j]->metadata.genotype = genotypes.genotype_from_strain(samples[i].regions[j]->metadata.strain,&sql);
		total_number_of_regions_+=samples[i].regions.size();
	//	break;
	}
	if(!be_quiet)
	std::cerr << "\n";
}
void ns_machine_analysis_data_loader::set_up_spec_to_load(const ns_64_bit & region_id, ns_64_bit & sample_id, ns_64_bit & experiment_id_a, ns_sql & sql, const bool load_excluded_regions){
	const bool region_specified(region_id != 0);
	const bool sample_specified(sample_id != 0);
	if (region_id == 0 && sample_id == 0 && experiment_id_a==0)
		throw ns_ex("No data requested!");
		
	if (region_id != 0){
		sql << "SELECT sample_id FROM sample_region_image_info WHERE id = " << region_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_experiment_movement_results::load()::Could not load region information ") << region_id;
		sample_id = ns_atoi64(res[0][0].c_str());
	}
	if (sample_id != 0){
		sql << "SELECT experiment_id FROM capture_samples WHERE id = " << sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_experiment_movement_results::load()::Could not load sample information ") << sample_id;
		experiment_id_a = ns_atoi64(res[0][0].c_str());
	}

	sql << "SELECT name FROM experiments WHERE id=" << experiment_id_a;
	ns_sql_result res;
	sql.get_rows(res);

	if (res.size() == 0)
		throw ns_ex("ns_experiment_movement_results::load()::Could not load experiment id=") << experiment_id_a;

	experiment_name_ = res[0][0];
	experiment_id_ = experiment_id_a;
	std::vector<ns_64_bit > sample_ids;

	if (!region_specified && !sample_specified){
		sql << "SELECT id FROM capture_samples WHERE censored=0 AND experiment_id = " << experiment_id_a;
		if (sample_id != 0)
			sql << " AND id = " << sample_id;
		ns_sql_result samp;
		sql.get_rows(samp);
		samples.resize(samp.size());
		for (unsigned int i = 0; i < samp.size(); i++)
			samples[i].set_id(ns_atoi64(samp[i][0].c_str()));
	}
	else{
		//add just the sample
		samples.resize(1,sample_id);
	}
	
}


ns_time_series_denoising_parameters ns_time_series_denoising_parameters::load_from_db(const ns_64_bit region_id, ns_sql & sql){
	sql << "SELECT time_series_denoising_flag FROM sample_region_image_info WHERE id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_series_denoising_parameters::load_from_db()::Could not find region ") << region_id << " in db";
	ns_time_series_denoising_parameters p;
	p.movement_score_normalization = (ns_time_series_denoising_parameters::ns_movement_score_normalization_type)atol(res[0][0].c_str());

	return p;
}
#endif
