#include "ns_posture_analysis_cross_validation.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_death_time_annotation_set.h"
#include "ns_posture_analysis_models.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_time_path_solver.h"
#include "ns_image_server_results_storage.h"
#include "ns_hand_annotation_loader.h"
#include <map>
#include <set>
#include <random>

struct ns_hmm_test_subject {
	ns_hmm_test_subject() {}
	ns_hmm_test_subject(const ns_64_bit& r, const ns_64_bit& g) :region_id(r), group_id(g) {}
	ns_64_bit region_id, group_id;
};
bool operator<(const ns_hmm_test_subject& l, const ns_hmm_test_subject& r) {
	if (l.region_id != r.region_id)
		return l.region_id < r.region_id;
	return l.group_id < r.group_id;
}
class  ns_cross_validation_replicate {
public:
	ns_cross_validation_replicate() : generate_detailed_path_info(false), states_permitted(ns_emperical_posture_quantification_value_estimator::ns_all_states) {}
	std::set<ns_hmm_test_subject> test_set;
	ns_emperical_posture_quantification_value_estimator training_set;
	ns_hmm_movement_analysis_optimizatiom_stats results;
	std::string replicate_description;
	bool generate_detailed_path_info;
	unsigned long replicate_id;
	bool model_building_completed;
	ns_emperical_posture_quantification_value_estimator::ns_states_permitted states_permitted;
	void test_model(const ns_64_bit& region_id, const ns_death_time_annotation_compiler& by_hand_annotations, const ns_time_series_denoising_parameters& time_series_denoising_parameters, ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>& time_path_image_analyzer, ns_time_path_solution& time_path_solution, ns_sql& sql,
		ns_hmm_movement_analysis_optimizatiom_stats& output_stats, bool generate_detailed_path_info) {

		bool found_valid_individual(false);
		for (auto p = test_set.begin(); p != test_set.end(); p++) {
			if (p->region_id == region_id) {
				found_valid_individual = true;
				break;
			}
		}
		if (!found_valid_individual)
			return;
		try {
			ns_time_path_movement_markov_solver markov_solver(training_set);
			//we might need to load everything again, if it has been cleared to reduce memory usage
			if (time_path_image_analyzer.size() == 0) {

				time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations);
				time_path_image_analyzer.load_completed_analysis_(
					region_id,
					time_path_solution,
					time_series_denoising_parameters,
					&markov_solver,
					sql,
					false);
				time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations);

			}
			else {
				//time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations);
				time_path_image_analyzer.reanalyze_with_different_movement_estimator(time_series_denoising_parameters, &markov_solver);
			}
			std::set<ns_stationary_path_id> individuals_to_test;
			for (auto p = test_set.begin(); p != test_set.end(); p++) {
				if (p->region_id == region_id)
					individuals_to_test.emplace(ns_stationary_path_id(p->group_id, 0, 0));
			}
			for (std::set<ns_stationary_path_id>::iterator pp = individuals_to_test.begin(); pp != individuals_to_test.end(); pp++)
				individuals_to_test.emplace(ns_stationary_path_id(pp->group_id, 0, time_path_image_analyzer.db_analysis_id()));
			time_path_image_analyzer.calculate_optimzation_stats_for_current_hmm_estimator(output_stats, &training_set, individuals_to_test, generate_detailed_path_info);
		}
		catch (ns_ex & ex2) {
			sql << "select r.name, s.name FROM sample_region_image_info as r, capture_samples as s WHERE r.id = " << region_id << " AND s.id = r.sample_id";
			ns_sql_result res;
			sql.get_rows(res);
			ns_ex ex("Error analyzing plate ");
			if (res.empty()) {
				ex << "region" << region_id;
			}
			else {
				ex << res[0][1] << "::" << res[0][0];
			}
			ex << ": " << ex2.text();
			image_server.register_server_event(ex, &sql);
		}
	}

};

struct ns_plate_replicate_generator {
	typedef ns_64_bit index_type;
	static index_type get_index_for_observation(const ns_hmm_emission& e) {
		return e.region_id;
	}
	static int minimum_subgroup_size() { return 10; }
};
struct ns_device_replicate_generator {
	typedef ns_64_bit index_type;
	static index_type get_index_for_observation(const ns_hmm_emission& e) {
		return e.device_id;
	}
	static int minimum_subgroup_size() { return 20; }
};
struct ns_individual_replicate_generator {
	typedef ns_hmm_test_subject index_type;
	static index_type get_index_for_observation(const ns_hmm_emission& e) {
		return ns_hmm_test_subject(e.region_id, e.path_id.group_id);
	}
	static int minimum_subgroup_size() { return 1; }
};

struct ns_cross_validation_sub_result {
	ns_cross_validation_sub_result() :mean_err(0), mean_N(0), var_err(0), var_N(0) {}
	double mean_err, mean_N, var_err, var_N;
	std::vector<double> err, N;
};
struct ns_cross_validation_results {
	ns_cross_validation_sub_result movement, expansion;
};
class ns_hmm_cross_validation_set {
public:
	ns_hmm_cross_validation_set() {}
	std::vector<ns_cross_validation_replicate> replicates;
	std::string description;

	bool build_device_cross_validation_set(int k_fold_validation, const ns_emperical_posture_quantification_value_estimator& all_observations) {
		return build_independent_replicates<ns_device_replicate_generator>(k_fold_validation, all_observations);
	}
	bool build_plate_cross_validation_set(int k_fold_validation, const ns_emperical_posture_quantification_value_estimator& all_observations) {
		return build_independent_replicates<ns_plate_replicate_generator>(k_fold_validation, all_observations);
	}
	bool build_individual_cross_validation_set(int k_fold_validation, const ns_emperical_posture_quantification_value_estimator& all_observations) {
		return build_independent_replicates<ns_individual_replicate_generator>(k_fold_validation, all_observations);
	}
	void build_all_vs_all_set(const ns_emperical_posture_quantification_value_estimator& all_observations) {
		replicates.resize(1);
		//now load all observations into the training or test sets
		for (auto observation_list = all_observations.observed_values.begin(); observation_list != all_observations.observed_values.end(); observation_list++) {
			//find appropriate movement state list in this estimator  
			//go through each observation
			for (auto observation = observation_list->second.begin(); observation != observation_list->second.end(); observation++) {
				replicates[0].training_set.observed_values[observation_list->first].push_back(*observation);
				replicates[0].test_set.emplace(ns_hmm_test_subject(observation->region_id, observation->path_id.group_id));
			}
		}
	}

	ns_cross_validation_results calculate_error() {
		ns_cross_validation_results results;
		ns_cross_validation_sub_result* sub_r[2] = { &results.movement,&results.expansion };
		const ns_movement_event step[2] = { ns_movement_cessation,ns_death_associated_expansion_start };
		for (unsigned int m = 0; m < 2; m++) {
			ns_cross_validation_sub_result& r = *sub_r[m];
			r.err.resize(replicates.size(), 0);
			r.N.resize(replicates.size(), 0);

			bool found_animals;
			for (unsigned int e = 0; e < replicates.size(); e++) {
				unsigned long movement_Nc(0);
				double movement_square_error(0);
				for (unsigned int i = 0; i < replicates[e].results.animals.size(); i++) {
					found_animals = true;
					ns_hmm_movement_analysis_optimizatiom_stats_record& animal = replicates[e].results.animals[i];
					if (animal.measurements[step[m]].by_hand_identified &&
						animal.measurements[step[m]].machine_identified) {
						const double d = (animal.measurements[step[m]].machine.best_estimate_event_time_for_possible_partially_unbounded_interval() -
							animal.measurements[step[m]].by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval()) / 24.0 / 60.0 / 60.0;
						movement_square_error += d * d;
						movement_Nc++;
					}
				}
				r.err[e] = sqrt(movement_square_error / movement_Nc);
				r.N[e] = movement_Nc;
			}
			if (!found_animals)
				continue;

			for (unsigned int i = 0; i < r.err.size(); i++) {
				r.mean_err += r.err[i];
				r.mean_N += r.N[i];
			}
			r.mean_err /= replicates.size();
			r.mean_N /= replicates.size();

			for (unsigned int i = 0; i < replicates.size(); i++) {
				r.var_err += (r.err[i] - r.mean_err) * (r.err[i] - r.mean_err);
				r.var_N += (r.N[i] - r.mean_N) * (r.N[i] - r.mean_N);
			}
			r.var_err = sqrt(r.var_err) / replicates.size();
		}
		return results;
	}

private:
	template<class ns_replicate_generator>
	bool build_independent_replicates(int k_fold_validation, const ns_emperical_posture_quantification_value_estimator& all_observations) {

		//build a randomly shuffled list of all unique observation labels.
		//for devices, this will be a list of all unique devices, for plates it will be plates, for individuals it will be each individual
		//the code is written as if for devices, but the template makes it general.

		//first we make a list of the individuals located on each device,  so we can exclude devices that are too small.
		std::map<typename ns_replicate_generator::index_type, std::set<ns_hmm_test_subject> > individuals_on_each_device;
		for (auto observation_list = all_observations.observed_values.begin(); observation_list != all_observations.observed_values.end(); observation_list++) {
			for (auto observation = observation_list->second.begin(); observation != observation_list->second.end(); observation++) {
				individuals_on_each_device[ns_replicate_generator::get_index_for_observation(*observation)].emplace(ns_hmm_test_subject(observation->region_id, observation->path_id.group_id));
			}
		}

		for (auto p = individuals_on_each_device.begin(); p != individuals_on_each_device.end();) {
			if (p->second.size() < ns_replicate_generator::minimum_subgroup_size())
				p = individuals_on_each_device.erase(p);
			else p++;
		}

		if (individuals_on_each_device.size() < 2) //if there are fewer than two devices, we cannot do any cross validation
			return false;


		//now we have all the devices for cross validation!
		//we shuffle their order and use this order to define the independent cross validation sets
		std::vector<typename ns_replicate_generator::index_type> devices_to_use;
		for (auto p = individuals_on_each_device.begin(); p != individuals_on_each_device.end(); ++p)
			devices_to_use.push_back(p->first);

		std::random_device rd;
		std::mt19937 g(rd());
		std::shuffle(devices_to_use.begin(), devices_to_use.end(), g);

		//group devices into k independent sets (accounting for the fact that there might not be exactly equal set sizes)
		unsigned long set_size = devices_to_use.size() / k_fold_validation;
		if (set_size == 0)
			set_size = 1;

		//it may be that having this many replicates leaves too few worms per replicate.
		//therefore, we shrink the replicate number until we have at least 10 individuals per replicate.
		std::vector<unsigned long> number_of_worms_in_replicate;
		unsigned long number_of_replicates;
		bool found_enough_worms_in_each_replicate(false);
		for (; set_size >= 1; set_size--) {
			number_of_replicates = devices_to_use.size() / set_size + ((devices_to_use.size() % set_size) ? 1 : 0);
			if (number_of_replicates < 2)
				break;
			number_of_worms_in_replicate.resize(number_of_replicates, 0);
			for (unsigned int i = 0; i < devices_to_use.size(); i++)
				number_of_worms_in_replicate[i / set_size] += individuals_on_each_device[devices_to_use[i]].size();
			unsigned long min_replicate_size = 10000;
			for (unsigned int i = 0; i < number_of_worms_in_replicate.size(); i++)
				if (number_of_worms_in_replicate[i] < min_replicate_size)
					min_replicate_size = number_of_worms_in_replicate[i];
			if (min_replicate_size > ns_replicate_generator::minimum_subgroup_size()) {
				found_enough_worms_in_each_replicate = true;
				break;
			}
		}


		if (!found_enough_worms_in_each_replicate)
			return false;

		//each replicate gets set_size devices in its training set.  All devices not in the training set get added to the test set.
		//to set this up, we first make a mapping for each device to the replicate id in which it in is the training set.
		replicates.resize(number_of_replicates);
		std::map<typename ns_replicate_generator::index_type, unsigned long> replicate_id_lookup_for_training;
		for (unsigned int i = 0; i < devices_to_use.size(); i++)
			replicate_id_lookup_for_training[devices_to_use[i]] = i / set_size;

		//now load all observations into the training or test sets
		for (auto observation_list = all_observations.observed_values.begin(); observation_list != all_observations.observed_values.end(); observation_list++) {
			//find appropriate movement state list in this estimator  
			//go through each observation
			for (auto observation = observation_list->second.begin(); observation != observation_list->second.end(); observation++) {
				auto rep_id = replicate_id_lookup_for_training.find(ns_replicate_generator::get_index_for_observation(*observation));

				if (rep_id == replicate_id_lookup_for_training.end())
					continue;	//observations from devices that have been discarded for having too few individuals
				//for each replicate, each observation either gets added to the testing or the training sets.
				for (unsigned int i = 0; i < replicates.size(); i++) {
					if (rep_id->second == i)
						replicates[i].training_set.observed_values[observation_list->first].push_back(*observation);
					else replicates[i].test_set.emplace(ns_hmm_test_subject(observation->region_id, observation->path_id.group_id));
				}
			}
		}
		return true;
	}
};
struct ns_hmm_cross_validation_manager {
	std::map<std::string, ns_hmm_cross_validation_set > validation_runs_sorted_by_validation_type;

	const ns_hmm_cross_validation_set& all_vs_all() const {
		const auto p = validation_runs_sorted_by_validation_type.find("all");
		if (p == validation_runs_sorted_by_validation_type.end())
			throw ns_ex("ns_hmm_cross_validation_set::all_vs_all()::Could not find all set");
		return p->second;
	}
	void generate_cross_validations_to_test(int k_fold_validation, const ns_emperical_posture_quantification_value_estimator& all_observations, bool try_different_states) {
		validation_runs_sorted_by_validation_type.clear();
		{
			ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["all"];
			set.build_all_vs_all_set(all_observations);
			set.description = "No Cross-Validation";
		}
		if (try_different_states) {

			throw ns_ex("not implemented");
		}
		if (k_fold_validation == 1)
			return;
		//if there are not many devices, systematically remove each device
		{
			ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["devices"];
			if (!set.build_device_cross_validation_set(k_fold_validation, all_observations))
				validation_runs_sorted_by_validation_type.erase("devices");
			else set.description = "Device Cross-Validation";
		}
		{
			ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["plates"];
			if (!set.build_plate_cross_validation_set(k_fold_validation, all_observations))
				validation_runs_sorted_by_validation_type.erase("plates");
			else set.description = "Plate Cross-Validation";
		}
		{
			ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["individuals"];
			if (!set.build_individual_cross_validation_set(k_fold_validation, all_observations))
				validation_runs_sorted_by_validation_type.erase("individuals");
			else set.description = "Individual Cross-Validation";
		}
	}

};


void ns_run_hmm_cross_validation(std::string & results_text, ns_image_server_results_subject sub, ns_machine_analysis_data_loader & movement_results,const std::map < std::string, ns_emperical_posture_quantification_value_estimator> & observations_sorted_by_genotype, ns_sql & sql) {
	const bool run_hmm_cross_validation = true;
	const bool test_different_state_restrictions_on_viterbi_algorithm = false;



	std::map<ns_64_bit, ns_region_metadata> metadata_cache;
	std::map<std::string, ns_region_metadata> metadata_cache_by_name;

	std::map <ns_64_bit, ns_time_series_denoising_parameters> time_series_denoising_parameters_cache;
	for (unsigned int i = 0; i < movement_results.samples.size(); i++) {
		if (!sub.device_name.empty() && movement_results.samples[i].device_name() != sub.device_name)
			continue;
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++) {
			//get some metadata needed to specifiy cross-validation replicate sets
			const std::string plate_type_summary(movement_results.samples[i].regions[j]->metadata.plate_type_summary("-", true));
			metadata_cache[movement_results.samples[i].regions[j]->metadata.region_id] = movement_results.samples[i].regions[j]->metadata;
			metadata_cache_by_name[plate_type_summary] = movement_results.samples[i].regions[j]->metadata;
			time_series_denoising_parameters_cache[movement_results.samples[i].regions[j]->metadata.region_id] = ns_time_series_denoising_parameters::load_from_db(movement_results.samples[i].regions[j]->metadata.region_id, sql);
		}
	}


	std::map<std::string, ns_hmm_cross_validation_manager> different_validation_approaches_for_each_genotype;

	std::map<std::string, std::string> model_building_and_testing_info;
	std::map<std::string, std::string> model_filenames;
	std::cout << "Writing state emission data to disk...\n";
	//for each type of plate, build an estimator from the observations collected.
	unsigned long estimators_compiled(0);
	for (auto p = observations_sorted_by_genotype.begin(); p != observations_sorted_by_genotype.end(); p++) {
		std::cout << ns_to_string_short((100.0 * estimators_compiled) / observations_sorted_by_genotype.size(), 2) << "%...";
		estimators_compiled++;
		//xxx
		//ns_acquire_for_scope<ns_ostream> all_observations(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("hmm_obs=") + p->first, true, sql).output());
		//p->second.write_observation_data(all_observations()(), experiment_name);
		//all_observations.release();
	}
	std::cout << "\nPrepping for Cross Validation...\n";
	//set up models for cross validation
	estimators_compiled = 0;
	for (auto p = observations_sorted_by_genotype.begin(); p != observations_sorted_by_genotype.end(); p++) {
		std::cout << ns_to_string_short((100.0 * estimators_compiled) / observations_sorted_by_genotype.size(), 2) << "%...";
		estimators_compiled++;
		int k_fold_validation = run_hmm_cross_validation ? 5 : 1;
		different_validation_approaches_for_each_genotype[p->first].generate_cross_validations_to_test(k_fold_validation, p->second, test_different_state_restrictions_on_viterbi_algorithm);
	}
	if (different_validation_approaches_for_each_genotype.size() == 0)
		results_text += "No genotype or condition specific HMM models were built";
	else {
		results_text += ns_to_string(different_validation_approaches_for_each_genotype.size() - 1) + " HMM model";
		if (different_validation_approaches_for_each_genotype.size() > 2)
			results_text += "s were";
		else results_text += " was ";

		results_text += " built separately for groups:\n";

		for (auto p = different_validation_approaches_for_each_genotype.begin(); p != different_validation_approaches_for_each_genotype.end(); p++)
			results_text += metadata_cache_by_name[p->first].device_regression_match_description() + "\n";
		results_text += "All data was collated and analyzed at " + ns_format_time_string_for_human(ns_current_time()) + "\n\n";
	}
	std::cout << "\nBuilding HMM Models...\n";
	unsigned long num_built(0);
	for (auto p = different_validation_approaches_for_each_genotype.begin(); p != different_validation_approaches_for_each_genotype.end(); ++p) {

		std::cout << ns_to_string_short((100.0 * num_built) / different_validation_approaches_for_each_genotype.size(), 2) << "%...";
		num_built++;
		if (p->first == "all")
			model_building_and_testing_info[p->first] = "== HMM model built from all animal types ==\n";
		else {
			const std::string plate_type_long = metadata_cache_by_name[p->first].device_regression_match_description();
			model_building_and_testing_info[p->first] = "== HMM model for group \"" + plate_type_long + "\" ==\n";
		}
		bool output_all_states = p->first == "all";
		for (auto q = p->second.validation_runs_sorted_by_validation_type.begin(); q != p->second.validation_runs_sorted_by_validation_type.end(); q++) {
			for (auto r = q->second.replicates.begin(); r != q->second.replicates.end();) {
				try {
					r->training_set.build_estimator_from_observations(model_building_and_testing_info[p->first], ns_emperical_posture_quantification_value_estimator::ns_all_states);
					++r;
				}
				catch (ns_ex & ex) {
					std::cout << ex.text() << "\n";
					r = q->second.replicates.erase(r);
				}
			}
		}

		ns_image_server_results_file ps(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, std::string("hmm=") + p->first, sql));
		model_filenames[p->first] = ps.output_filename();
		ns_acquire_for_scope<ns_ostream> both_parameter_set(ps.output());
		p->second.all_vs_all().replicates[0].training_set.write(both_parameter_set()());
		both_parameter_set.release();
	}
	std::cout << "\nTesting HMM models on current experiment...\n";
	num_built = 0;
	unsigned long region_count = 0;
	for (unsigned int i = 0; i < movement_results.samples.size(); i++) {
		if (!sub.device_name.empty() && movement_results.samples[i].device_name() != sub.device_name)
			continue;
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++) {
			if (sub.region_id != 0 && sub.region_id != movement_results.samples[i].regions[j]->metadata.region_id)
				continue;
			if (!movement_results.samples[i].regions[j]->contains_a_by_hand_death_time_annotation)
				continue;
			region_count++;
		}
	}
	//go through and calculate the optimization stats for each region
	for (unsigned int i = 0; i < movement_results.samples.size(); i++) {
		if (!sub.device_name.empty() && movement_results.samples[i].device_name() != sub.device_name)
			continue;
		for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++) {
			if (sub.region_id != 0 && sub.region_id != movement_results.samples[i].regions[j]->metadata.region_id)
				continue;
			if (!movement_results.samples[i].regions[j]->contains_a_by_hand_death_time_annotation)
				continue;

			std::cout << ns_to_string_short((100.0 * num_built) / region_count, 2) << "%...";
			num_built++;
			const ns_time_series_denoising_parameters time_series_denoising_parameters(time_series_denoising_parameters_cache[movement_results.samples[i].regions[j]->metadata.region_id]);
			const std::string plate_type_summary(movement_results.samples[i].regions[j]->metadata.plate_type_summary("-", true));
			for (auto p = different_validation_approaches_for_each_genotype.begin(); p != different_validation_approaches_for_each_genotype.end(); p++) {

				if (p->first == "all" || p->first == plate_type_summary) {
					for (auto cross_validation_runs = p->second.validation_runs_sorted_by_validation_type.begin(); cross_validation_runs != p->second.validation_runs_sorted_by_validation_type.end(); ++cross_validation_runs) {

						//xxx 
						//if (p->first != "Alcedo+Wildtype-0-20C-uv+nec-control")
						//	continue;

						for (auto cross_validation_replicate = cross_validation_runs->second.replicates.begin(); cross_validation_replicate != cross_validation_runs->second.replicates.end(); cross_validation_replicate++) {
							//make sure we only test on the regions specified in the cross validation set
							cross_validation_replicate->test_model(movement_results.samples[i].regions[j]->metadata.region_id, movement_results.samples[i].regions[j]->by_hand_annotations,
								time_series_denoising_parameters, *movement_results.samples[i].regions[j]->time_path_image_analyzer, movement_results.samples[i].regions[j]->time_path_solution, sql,
								cross_validation_replicate->results, cross_validation_replicate->generate_detailed_path_info);
						}
					}
				}
			}
			if (!run_hmm_cross_validation)
				movement_results.samples[i].regions[j]->time_path_image_analyzer->clear();
		}
	}
	std::cout << "\nWriting HMM model test results to disk.\n";
	//write all the different plate type stats to disk
	num_built = 0;
	for (auto p = different_validation_approaches_for_each_genotype.begin(); p != different_validation_approaches_for_each_genotype.end(); p++) {
		std::cout << ns_to_string_short((100.0 * num_built) / different_validation_approaches_for_each_genotype.size(), 2) << "%...";
		num_built++;
		//start at 1 to skip the all vs all.
		for (auto validation_run = p->second.validation_runs_sorted_by_validation_type.begin(); validation_run != p->second.validation_runs_sorted_by_validation_type.end(); ++validation_run) {
			ns_cross_validation_results results = validation_run->second.calculate_error();

			model_building_and_testing_info[p->first] += "= Cross validation strategy: " + validation_run->second.description + " =:\n";
			model_building_and_testing_info[p->first] += "= " + ns_to_string(validation_run->second.replicates.size()) + "-fold cross validation =:\n";

			if (results.movement.mean_N > 0)
				model_building_and_testing_info[p->first] += "[Movement cessation]:   mean error: " + ns_to_string_short(results.movement.mean_err, 2) + "+-" + ns_to_string_short(results.movement.var_err, 2) + " days across " + ns_to_string((int)results.movement.mean_N) + " animals.\n";
			if (results.expansion.mean_N > 0)
				model_building_and_testing_info[p->first] += "[Death time expansion]: mean error: " + ns_to_string_short(results.expansion.mean_err, 2) + " + -" + ns_to_string_short(results.expansion.var_err, 2) + " days across " + ns_to_string((int)results.expansion.mean_N) + " animals.\n";
			if (results.movement.mean_N < 50 || results.expansion.mean_N < 50)
				model_building_and_testing_info[p->first] += "Warning: These results will not be meaningful until more worms are annotated by hand.\n";
			else model_building_and_testing_info[p->first] += "The model file was written to \"" + model_filenames[p->first] + "\"\n";

		}
		//output performance statistics
		ns_acquire_for_scope<ns_ostream>  performance_stats_output(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("hmm_performance=") + p->first, true, sql).output());
		std::vector<std::string > measurement_names;
		if (!p->second.all_vs_all().replicates[0].results.animals.empty())
			measurement_names = p->second.all_vs_all().replicates[0].results.animals[0].state_info_variable_names;
		ns_hmm_movement_analysis_optimizatiom_stats::write_error_header(performance_stats_output()(), measurement_names);
		for (auto cross_validation_set = p->second.validation_runs_sorted_by_validation_type.begin(); cross_validation_set != p->second.validation_runs_sorted_by_validation_type.end(); ++cross_validation_set) {
			for (unsigned int r = 0; r < cross_validation_set->second.replicates.size(); r++) {
				cross_validation_set->second.replicates[r].results.write_error_data(performance_stats_output()(), p->first, cross_validation_set->first, cross_validation_set->second.replicates[r].replicate_id, metadata_cache);
			}
		}
		performance_stats_output.release();
		//output per-path statistics
		unsigned int tmp = 0;
		for (auto cross_validation_set = p->second.validation_runs_sorted_by_validation_type.begin(); cross_validation_set != p->second.validation_runs_sorted_by_validation_type.end(); ++cross_validation_set) {

			for (auto q = cross_validation_set->second.replicates.begin(); q != cross_validation_set->second.replicates.end(); q++) {
				if (!q->generate_detailed_path_info || q->results.animals.empty())
					continue;
				std::string suffix;
				if (tmp > 0)
					suffix = "=" + ns_to_string(tmp);
				//cout << "Writing path data for " << p->first << "\n";
				ns_acquire_for_scope<ns_ostream>  path_stats_output(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("hmm_path=") + p->first + suffix, true, sql).output());
				if (path_stats_output()().fail()) {
					std::cout << "Could not open file for " << p->first << "\n";
					continue;
				}
				tmp++;
				q->results.write_hmm_path_header(path_stats_output()());
				q->results.write_hmm_path_data(path_stats_output()(), metadata_cache);
				path_stats_output.release();
			}
		}
	}

	for (auto p = model_building_and_testing_info.begin(); p != model_building_and_testing_info.end(); p++)
		results_text += model_building_and_testing_info[p->first] + "\n";
}

struct ns_parameter_set_optimization_record {
	ns_parameter_set_optimization_record(std::vector<double>& thresholds, std::vector<unsigned long>& hold_times) :total_count(0), best_parameter_count(0), new_parameters_results(thresholds.size(), hold_times.size()), current_parameter_results(1, 1) {}


	ns_parameter_optimization_results new_parameters_results;
	ns_parameter_optimization_results current_parameter_results;
	ns_threshold_movement_posture_analyzer_parameters best;
	double smallest_mean_squared_error;
	ns_64_bit total_count;
	unsigned long best_parameter_count;
	std::string filename;

	ns_ex find_best_parameter_set(std::vector<double>& thresholds, std::vector<unsigned long>& hold_times, bool posture_not_expansion) {
		const double minimum_convergence_fraction(0.9);
		const double how_much_deviation_from_best_mean_square_error_to_tolerate_for_better_convergence(.9);

		//double best_weighted_mean_squared_error = DBL_MAX;
		const ns_64_bit total_count_expected = new_parameters_results.number_valid_worms;
		const ns_64_bit min_counts_considered = new_parameters_results.number_valid_worms * minimum_convergence_fraction;
		ns_64_bit max_number_of_counts(0), min_number_of_counts(total_count_expected);
		for (unsigned int i = 0; i < thresholds.size(); i++) {
			for (unsigned int j = 0; j < hold_times.size(); j++) {
				if (new_parameters_results.counts[i][j] > max_number_of_counts)
					max_number_of_counts = new_parameters_results.counts[i][j];
				if (new_parameters_results.counts[i][j] < min_number_of_counts && new_parameters_results.counts[i][j] >= min_counts_considered)
					min_number_of_counts = new_parameters_results.counts[i][j];
			}
		}
		if (max_number_of_counts / (double)new_parameters_results.number_valid_worms < minimum_convergence_fraction)
			return ns_ex("No parameters execeeded the minimum convergence threshold of ") << minimum_convergence_fraction * 100 << "% of worms";

		//we find the best parameter set for each number of worms for whom the analysis provided a death time.
		//(note: the complexity here is that some parameters do not yeild a death time for all animals, and we must balance this with the quality of analysis for the worms that worked)
		std::vector<double> best_mean_squared_error_by_count(max_number_of_counts - min_number_of_counts + 1, DBL_MAX);
		std::vector<std::pair<double, unsigned long> > best_parameter_set_by_count(max_number_of_counts - min_number_of_counts + 1);
		double overall_smallest_mean_squared_error = DBL_MAX;
		unsigned long overall_smallest_mean_squared_error_count = 0;
		for (unsigned int i = 0; i < thresholds.size(); i++) {
			for (unsigned int j = 0; j < hold_times.size(); j++) {
				if (new_parameters_results.counts[i][j] < min_counts_considered)
					continue;
				total_count += new_parameters_results.counts[i][j];
				//penalize for parameter combinations that do not converge
				const double cur_msq = (new_parameters_results.death_total_mean_square_error_in_hours[i][j] / (double)new_parameters_results.counts[i][j]);
				//	cout << cur_msq << " " << total_count_expected - new_parameters_results.counts[i][j] << " " << weighted_msq << "\n";
				const unsigned long count_index = new_parameters_results.counts[i][j] - min_number_of_counts;
				if (cur_msq < best_mean_squared_error_by_count[count_index]) {
					best_mean_squared_error_by_count[count_index] = cur_msq;
					best_parameter_set_by_count[count_index] = std::pair<double, unsigned long>(thresholds[i], hold_times[j]);
				}
				if (cur_msq < overall_smallest_mean_squared_error) {
					overall_smallest_mean_squared_error = cur_msq;
					overall_smallest_mean_squared_error_count = new_parameters_results.counts[i][j];
				}
			}
		}

		//now we have the best parameter sets for each number of converged answers.  we balance the strength of the parameter set against the number of convergence.
		for (unsigned long i = overall_smallest_mean_squared_error_count; i <= max_number_of_counts; i++) {
			if (how_much_deviation_from_best_mean_square_error_to_tolerate_for_better_convergence * best_mean_squared_error_by_count[i - min_number_of_counts] <= overall_smallest_mean_squared_error) {
				if (posture_not_expansion) {
					best.stationary_cutoff = best_parameter_set_by_count[i - min_number_of_counts].first;
					best.permanance_time_required_in_seconds = best_parameter_set_by_count[i - min_number_of_counts].second;
				}
				else {
					best.death_time_expansion_cutoff = best_parameter_set_by_count[i - min_number_of_counts].first;
					best.death_time_expansion_time_kernel_in_seconds = best_parameter_set_by_count[i - min_number_of_counts].second;
				}
				smallest_mean_squared_error = best_mean_squared_error_by_count[i - min_number_of_counts];
				best_parameter_count = i;
				total_count = i;
			}

		}
		return ns_ex();
	}
};

void ns_identify_best_threshold_parameteters(std::string& results_text, const ns_parameter_set_range& range, ns_image_server_results_subject sub, ns_sql& sql) {
	ns_sql_result experiment_regions;
	sql << "SELECT r.name,r.id,s.name,s.id,s.device_name, r.excluded_from_analysis OR r.censored OR s.excluded_from_analysis OR s.censored FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = " << sub.experiment_id;
	sql.get_rows(experiment_regions);

	bool run_posture(true), run_expansion(false);
	//posture analysis
	std::vector<double> posture_analysis_thresholds,
		expansion_analysis_thresholds;
	std::vector<unsigned long> posture_analysis_hold_times, //in seconds
		expansion_analysis_hold_times;
	const bool v1_parameters = false;
	//old movement scores
	if (v1_parameters == "1") {
		const double min_thresh(.0005);
		const double max_thresh(.5);
		const long number_of_thresholds(60);
		const double log_dt(((log(max_thresh) - log(min_thresh)) / number_of_thresholds));
		posture_analysis_thresholds.resize(number_of_thresholds);

		double cur = min_thresh;
		for (unsigned long i = 0; i < number_of_thresholds; i++) {
			posture_analysis_thresholds[i] = exp(log(min_thresh) + log_dt * i);
		}
	}
	else {
		//new movement scores
		const double min_thresh(.1);
		const double max_thresh(2000);
		const long number_of_thresholds(40);
		const double log_dt(((log(max_thresh) - log(min_thresh)) / number_of_thresholds));
		posture_analysis_thresholds.resize(number_of_thresholds);

		double cur = min_thresh;
		for (unsigned long i = 0; i < number_of_thresholds; i++) {
			posture_analysis_thresholds[i] = exp(log(min_thresh) + log_dt * i);
		}

	}


	//generate optimization training set 
	const unsigned long near_zero(30);
	const unsigned long thresh_num(20);
	bool by_hand_range(false);
	if (range == ns_v2) {

		posture_analysis_hold_times.reserve(40);
		posture_analysis_hold_times.push_back(0);
		for (unsigned int i = 0; i < 15; i++)
			posture_analysis_hold_times.push_back(i * 30 * 60);
		for (unsigned int i = 0; i < 6; i++)
			posture_analysis_hold_times.push_back((i) * 2 * 60 * 60);
		int p(posture_analysis_hold_times.size());
		for (unsigned int i = 6; i < 14; i++)
			posture_analysis_hold_times.push_back(posture_analysis_hold_times[p - 1] + (i - 6) * 6 * 60 * 60);
	}
	else if (range == ns_thermotolerance) {

		posture_analysis_hold_times.reserve(16);
		posture_analysis_hold_times.push_back(0);
		for (unsigned int i = 0; i < 15; i++)
			posture_analysis_hold_times.push_back(i * 45 * 60);
	}
	else if (range == ns_quiecent) {

		posture_analysis_hold_times.reserve(21);
		for (unsigned int i = 0; i < 6; i++)
			posture_analysis_hold_times.push_back((i) * 2 * 60 * 60);

		int p(posture_analysis_hold_times.size());
		for (unsigned int i = 6; i < 14; i++)
			posture_analysis_hold_times.push_back(p + (i - 6) * 6 * 60 * 60);
	}
	else {


		posture_analysis_hold_times.reserve(21);
		posture_analysis_hold_times.push_back(0);
		posture_analysis_hold_times.push_back(60 * 30);
		posture_analysis_hold_times.push_back(60 * 60);
		for (unsigned int i = 0; i < 11; i++)
			posture_analysis_hold_times.push_back((i + 1) * 2 * 60 * 60);
		for (unsigned int i = 0; i < 6; i++)
			posture_analysis_hold_times.push_back((i + 5) * 6 * 60 * 60);
	}


	for (unsigned int i = 0; i < 16; i++)
		expansion_analysis_thresholds.push_back((i) * 25);

	for (unsigned int i = 0; i < 16; i++)
		expansion_analysis_hold_times.push_back((i) * 30 * 60);

	ns_acquire_for_scope<ns_ostream> posture_analysis_optimization_output, expansion_analysis_optimization_output;
	if (run_posture) {
		posture_analysis_optimization_output.attach(image_server.results_storage.time_path_image_analysis_quantification(sub, "threshold_posture_optimization_stats", true, sql).output());
		ns_analyzed_image_time_path::write_posture_analysis_optimization_data_header(posture_analysis_optimization_output()());
	}
	if (run_expansion) {
		expansion_analysis_optimization_output.attach(image_server.results_storage.time_path_image_analysis_quantification(sub, "threshold_expansion_optimization_stats", true, sql).output());
		ns_analyzed_image_time_path::write_expansion_analysis_optimization_data_header(expansion_analysis_optimization_output()());
	}
	unsigned region_count(0);

	//0:region name, 1:region_id, 2:sample_name, 3:sample_id, 4:device_name, 5:excluded
	for (unsigned int i = 0; i < experiment_regions.size(); i++) {
		if (!sub.device_name.empty() && experiment_regions[i][4] != sub.device_name)
			continue;
		if (sub.region_id != 0 && ns_atoi64(experiment_regions[i][1].c_str()) != sub.region_id)
			continue;
		region_count++;
	}

	std::map<std::string, ns_parameter_set_optimization_record> best_posture_parameter_sets, best_expansion_parameter_sets;


	if (image_server.verbose_debug_output())
		std::cout << "Considering " << region_count << " regions.\n";

	//0:region name, 1:region_id, 2:sample_name, 3:sample_id, 4:device_name, 5:excluded
	unsigned pos(0);
	for (unsigned int i = 0; i < experiment_regions.size(); i++) {
		if (!sub.device_name.empty() && experiment_regions[i][4] != sub.device_name)
			continue;
		if (sub.region_id != 0 && ns_atoi64(experiment_regions[i][1].c_str()) != sub.region_id)
			continue;

		bool found_regions(false);
		std::cerr << ns_to_string_short((100.0 * pos) / region_count) << "%...";
		pos++;
		if (experiment_regions[i][5] != "0")
			continue;
		found_regions = true;
		if (image_server.verbose_debug_output())
			std::cout << "\nConsidering " << experiment_regions[i][2] << "::" << experiment_regions[i][0] << "\n";
		std::string posture_model_used = "";
		try {

			ns_time_series_denoising_parameters::ns_movement_score_normalization_type norm_type[1] =
			{ ns_time_series_denoising_parameters::ns_none };

			for (unsigned int k = 0; k < 1; k++) {
				const ns_64_bit region_id(ns_atoi64(experiment_regions[i][1].c_str()));


				ns_time_path_solution time_path_solution;
				time_path_solution.load_from_db(region_id, sql, true);

				ns_time_series_denoising_parameters denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id, sql));
				denoising_parameters.movement_score_normalization = norm_type[k];

				ns_time_path_image_movement_analysis_memory_pool<ns_overallocation_resizer> memory_pool;
				ns_time_path_image_movement_analyzer<ns_overallocation_resizer> time_path_image_analyzer(memory_pool);

				ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
				image_server.get_posture_analysis_model_for_region(region_id, handle, sql);

				if (posture_model_used == "")
					posture_model_used = time_path_image_analyzer.posture_model_version_used;
				else if (posture_model_used != time_path_image_analyzer.posture_model_version_used)
					throw ns_ex("Not all regions are using the same posture analysis version!  Please run the  \"Analyze Worm Movement Using Cached Images\" for all regions.");

				ns_posture_analysis_model mod(handle().model_specification);
				mod.threshold_parameters.use_v1_movement_score = time_path_image_analyzer.posture_model_version_used == "1";
				mod.threshold_parameters.version_flag = time_path_image_analyzer.posture_model_version_used;

				handle.release();
				ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(

					ns_get_death_time_estimator_from_posture_analysis_model(mod));

				time_path_image_analyzer.load_completed_analysis_(region_id, time_path_solution, denoising_parameters, &death_time_estimator(), sql);
				death_time_estimator.release();
				ns_region_metadata metadata;

				try {
					ns_hand_annotation_loader by_hand_region_annotations;
					metadata = by_hand_region_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, region_id, sql);
					time_path_image_analyzer.add_by_hand_annotations(by_hand_region_annotations.annotations);

				}
				catch (ns_ex & ex) {
					std::cerr << ex.text();
					metadata.load_from_db(region_id, "", sql);
				}

				if (run_posture)
					posture_analysis_optimization_output()() << "\n";
				if (run_expansion)
					expansion_analysis_optimization_output()() << "\n";
				std::cerr << metadata.plate_name() << "\n";

				//get current parameters
				ns_image_server::ns_posture_analysis_model_cache::const_handle_t posture_analysis_model_handle;
				image_server.get_posture_analysis_model_for_region(region_id, posture_analysis_model_handle, sql);
				
				const std::string strain_name = metadata.plate_type_summary("-", true);
				if (run_posture) {
					std::map<std::string, ns_parameter_set_optimization_record>::iterator p_all = best_posture_parameter_sets.find("all");
					if (p_all == best_posture_parameter_sets.end()) {
						if (image_server.verbose_debug_output())
							std::cout << "Creating record for all plate types\n";


						p_all = best_posture_parameter_sets.insert(best_posture_parameter_sets.begin(),
							std::pair<std::string, ns_parameter_set_optimization_record>("all",
								ns_parameter_set_optimization_record(posture_analysis_thresholds, posture_analysis_hold_times)));
						p_all->second.best = posture_analysis_model_handle().model_specification.threshold_parameters;   //set default parameters
					}
					std::map<std::string, ns_parameter_set_optimization_record>::iterator p = best_posture_parameter_sets.find(strain_name);
					if (p == best_posture_parameter_sets.end()) {
						if (image_server.verbose_debug_output())
							std::cout << "Creating record for plate type " << strain_name << "\n";


						p = best_posture_parameter_sets.insert(best_posture_parameter_sets.begin(),
							std::pair<std::string, ns_parameter_set_optimization_record>(strain_name,
								ns_parameter_set_optimization_record(posture_analysis_thresholds, posture_analysis_hold_times)));
						p->second.best = posture_analysis_model_handle().model_specification.threshold_parameters;   //set default parameters

					}
					else {

						if (image_server.verbose_debug_output())
							std::cout << "Adding to record for plate type " << strain_name << "\n";
					}

					time_path_image_analyzer.write_posture_analysis_optimization_data(time_path_image_analyzer.posture_model_version_used, posture_analysis_thresholds, posture_analysis_hold_times, metadata, posture_analysis_optimization_output()(), p->second.new_parameters_results, &(p_all->second.new_parameters_results));

					//include existing model file
					std::vector<double> thresh;
					std::vector<unsigned long>hold_t;
					thresh.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.stationary_cutoff);
					hold_t.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.permanance_time_required_in_seconds);
					time_path_image_analyzer.write_posture_analysis_optimization_data(time_path_image_analyzer.posture_model_version_used, thresh, hold_t, metadata, posture_analysis_optimization_output()(), p->second.current_parameter_results, &(p_all->second.current_parameter_results));
				}
				if (run_expansion) {
					std::map<std::string, ns_parameter_set_optimization_record>::iterator p = best_expansion_parameter_sets.find(strain_name);
					if (p == best_expansion_parameter_sets.end()) {
						p = best_expansion_parameter_sets.insert(best_expansion_parameter_sets.begin(), std::pair<std::string, ns_parameter_set_optimization_record>(strain_name, ns_parameter_set_optimization_record(posture_analysis_thresholds, posture_analysis_hold_times)));
						p->second.best = posture_analysis_model_handle().model_specification.threshold_parameters;   //set default parameters
					}
					std::map<std::string, ns_parameter_set_optimization_record>::iterator p_all = best_expansion_parameter_sets.find("all");
					if (p_all == best_expansion_parameter_sets.end()) {
						if (image_server.verbose_debug_output())
							std::cout << "Creating record for all plate types\n";
						p_all = best_expansion_parameter_sets.insert(best_expansion_parameter_sets.begin(), std::pair<std::string, ns_parameter_set_optimization_record>("all", ns_parameter_set_optimization_record(posture_analysis_thresholds, posture_analysis_hold_times)));
						p_all->second.best = posture_analysis_model_handle().model_specification.threshold_parameters;   //set default parameters
					}
					time_path_image_analyzer.write_expansion_analysis_optimization_data(expansion_analysis_thresholds, expansion_analysis_hold_times, metadata, expansion_analysis_optimization_output()(), p->second.new_parameters_results, &(p_all->second.new_parameters_results));


					std::vector<double> thresh;
					std::vector<unsigned long>hold_t;
					thresh.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.death_time_expansion_cutoff);
					hold_t.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.death_time_expansion_time_kernel_in_seconds);
					time_path_image_analyzer.write_expansion_analysis_optimization_data(thresh, hold_t, metadata, expansion_analysis_optimization_output()(), p->second.current_parameter_results, &(p_all->second.current_parameter_results));
				}

			}

		}
		catch (ns_ex & ex) {
			std::cerr << "\n" << ex.text() << "\n";
		}
	}

	for (unsigned int i = 0; i < 2; i++) {
		std::map<std::string, ns_parameter_set_optimization_record>* best_parameter_sets;
		std::vector<double>* thresh;
		std::vector<unsigned long>* hold;
		std::string data_name;
		if (i == 0) {
			if (!run_posture)
				continue;
			data_name = "Posture Analysis";
			best_parameter_sets = &best_posture_parameter_sets;
			thresh = &posture_analysis_thresholds;
			hold = &posture_analysis_hold_times;
		}
		else {
			if (!run_expansion)
				continue;
			data_name = "Expansion Analysis";
			best_parameter_sets = &best_expansion_parameter_sets;
			thresh = &expansion_analysis_thresholds;
			hold = &expansion_analysis_hold_times;
		}

		results_text += "===Automated " + data_name + " Calibration Results == \n";
		results_text += "Calculated at " + ns_format_time_string_for_human(ns_current_time()) + "\n\n";
		for (auto p = best_parameter_sets->begin(); p != best_parameter_sets->end(); p++) {

			results_text += "**For plates of type " + p->first + "\n";
			ns_ex convergence_problem = p->second.find_best_parameter_set(*thresh, *hold, data_name == "Posture Analysis");
			p->second.filename = p->first;
			if (image_server.verbose_debug_output())
				std::cout << "Filename " << p->second.filename;
			if (p->second.new_parameters_results.number_valid_worms == 0) {

				results_text += "No worms were annotated by hand.\n\n";
				continue;
			}
			if (p->second.total_count == 0) {
				results_text += "No parameter sets converged on a solution.\n\n";
				continue;
			}

			for (unsigned int i = 0; i < p->second.filename.size(); i++)
				if (!isalnum(p->second.filename[i]))
					p->second.filename[i] = '-';

			if (image_server.verbose_debug_output())
				std::cout << " was fixed to " << p->second.filename << "\n";

			const bool current_parameters_converged(p->second.current_parameter_results.counts[0][0] > 0);
			const bool best_parameters_converged(p->second.best_parameter_count > 0);
			const double current_msqerr = (p->second.current_parameter_results.death_total_mean_square_error_in_hours[0][0] / p->second.current_parameter_results.counts[0][0]);
			const double best_msqerr = p->second.smallest_mean_squared_error;

			if (!convergence_problem.text().empty()) {
				results_text += convergence_problem.text() + "\n";
			}
			else if (!current_parameters_converged)
				results_text += "The existing parameter set never converged on a solution.\n";
			else {
				results_text += "The existing parameter set converged on a solution in " + ns_to_string_short((100.0 * p->second.current_parameter_results.counts[0][0]) / p->second.current_parameter_results.number_valid_worms, 1) + "% of cases\n";
				results_text += "The existing parameter set produced estimates that differed from by - hand annotations by\n " + ns_to_string_short(sqrt(current_msqerr), 3) + " days on average(a mean squared error of " + ns_to_string_short(current_msqerr, 3) + " days squared)\n";
			}
			if (!best_parameters_converged)
				results_text += "The best possible parameter set never converged on a solution\n";
			else {
				results_text += "The best possible parameter set converged on a solution in " + ns_to_string_short((100.0 * p->second.best_parameter_count) / p->second.current_parameter_results.number_valid_worms, 1) + "% of cases\n";
				results_text += "The best possible parameter set produced estimates that differed from by-hand annotations by\n" + ns_to_string_short(sqrt(best_msqerr), 3) + " days on average (a mean squared error of " + ns_to_string_short(best_msqerr, 3) + " days squared)\n";
			}
			bool enough_worms = p->second.new_parameters_results.number_valid_worms >= 50;
			bool enough_convergences = 1.25 * p->second.best_parameter_count >= p->second.new_parameters_results.number_valid_worms;
			if (!enough_worms)
				results_text += "Only " + ns_to_string(p->second.best_parameter_count) + " individuals were annotated by hand.\nThese results will not be meaningful until you annotate more individuals.\n";
			else if (!enough_convergences)
				results_text += "The best parameter set did not converge in most cases.\nThese parameters are not recommended\n";
			else results_text += ns_to_string(p->second.best_parameter_count) + " individuals were annotated by hand to produce these estimates.\n";


			if (enough_convergences && enough_worms && current_parameters_converged && best_parameters_converged) {
				bool substantial_improvement = current_msqerr <= .8 * best_msqerr;
				if (enough_worms && substantial_improvement)
					results_text += "A new parameter file named " + p->second.filename + " has been written to disk.\n  It is recommended that you use this model for subsequent analysis of this type of animals.\n";
			}
			results_text += "\n";
		}
	}

	//output best parameters to disk
	if (run_posture && run_expansion) {
		for (auto expansion_p = best_expansion_parameter_sets.begin(); expansion_p != best_expansion_parameter_sets.end(); expansion_p++) {
			//build parameter file with optimal posture and expansion parameters		
			auto posture_p = best_posture_parameter_sets.find(expansion_p->first);
			if (posture_p == best_posture_parameter_sets.end())
				throw ns_ex("Could not find expansion index");

			if (posture_p->second.new_parameters_results.number_valid_worms == 0 && expansion_p->second.new_parameters_results.number_valid_worms == 0)
				continue;
			expansion_p->second.best.permanance_time_required_in_seconds = posture_p->second.best.permanance_time_required_in_seconds;
			expansion_p->second.best.posture_cutoff = posture_p->second.best.posture_cutoff;
			expansion_p->second.best.stationary_cutoff = posture_p->second.best.stationary_cutoff;
			ns_acquire_for_scope<ns_ostream> both_parameter_set(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, expansion_p->second.filename, sql).output());
			expansion_p->second.best.write(both_parameter_set()());
			both_parameter_set.release();
		}
	}
	else {
		std::map<std::string, ns_parameter_set_optimization_record>* best_parameter_sets;
		if (run_posture) {
			best_parameter_sets = &best_posture_parameter_sets;
		}
		else if (run_expansion) {
			best_parameter_sets = &best_expansion_parameter_sets;
		}
		else throw ns_ex("Err");
		for (auto p = best_parameter_sets->begin(); p != best_parameter_sets->end(); p++) {
			if (p->second.new_parameters_results.number_valid_worms == 0)
				continue;
			ns_acquire_for_scope<ns_ostream> parameter_set_output(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, p->second.filename, sql).output());
			p->second.best.write(parameter_set_output()());
			parameter_set_output.release();
		}
	}
}