#include "ns_posture_analysis_cross_validation.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_death_time_annotation_set.h"
#include "ns_posture_analysis_models.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_time_path_solver.h"
#include "ns_image_server_results_storage.h"
#include "ns_hand_annotation_loader.h"
#include "ns_threshold_movement_posture_analyzer.h"

#include "ns_probability_model_measurement_accessor.h"
#include <map>
#include <set>
#include <random>
#include <tuple>

struct ns_hmm_test_subject {
	ns_hmm_test_subject():region_name(0) {}
	ns_hmm_test_subject(const std::string * db_name, const ns_64_bit & experiment,const std::string * r, const ns_64_bit & rid,const ns_64_bit& g) :database_name(db_name),experiment_id(experiment),region_name(r), region_info_id(rid), group_id(g) {}
	const std::string* region_name;
	ns_64_bit region_info_id;
	ns_64_bit experiment_id;
	const std::string* database_name;
	ns_64_bit group_id;
};
std::string ns_to_string(const ns_hmm_test_subject& sub) {
	return *sub.region_name + " " + ns_to_string(sub.group_id);
}
bool operator<(const ns_hmm_test_subject& l, const ns_hmm_test_subject& r) {
	if (l.region_info_id != r.region_info_id)
		return l.region_info_id < r.region_info_id;
	return l.group_id < r.group_id;
}

class  ns_cross_validation_replicate_spec {
public:
	ns_cross_validation_replicate_spec() :replicate_id(0), use_to_generate_model_file(false){}
	std::set<ns_hmm_test_subject> test_set;
	ns_hmm_observation_set training_set;
	std::set< ns_hmm_test_subject> training_set_individuals;
	std::vector<std::string> replicate_training_groups, replicate_testing_groups, replicate_skipped_groups;
	bool use_to_generate_model_file;
	unsigned long replicate_id;
};

class  ns_cross_validation_replicate_analysis_results {
public:
	ns_cross_validation_replicate_analysis_results():use_to_generate_model_file(false),replicate_description(0){}

	ns_emperical_posture_quantification_value_estimator model_built_in_this_replicate;

	ns_hmm_movement_analysis_optimizatiom_stats results;

	bool use_to_generate_model_file;

	const std::string * replicate_description;
	const ns_cross_validation_replicate_spec* replicate_spec;

	void test_model(const ns_time_path_movement_markov_solver& markov_solver, const ns_cross_validation_replicate_spec & replicate_spec,const std::string* database, ns_64_bit& region_id, const ns_death_time_annotation_compiler& by_hand_annotations, const ns_time_series_denoising_parameters& time_series_denoising_parameters, ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>& time_path_image_analyzer, ns_time_path_solution& time_path_solution, ns_sql& sql,
		ns_hmm_movement_analysis_optimizatiom_stats& output_stats, bool generate_detailed_path_info) {

		bool found_valid_individual(false);
		for (auto p = replicate_spec.test_set.begin(); p != replicate_spec.test_set.end(); p++) {
			if (p->region_info_id == region_id) {
				found_valid_individual = true;
				break;
			}
		}
		if (!found_valid_individual)
			return;
		try {
			//we might need to load everything again, if it has been cleared to reduce memory usage
			if (time_path_image_analyzer.size() == 0) {
				//std::cerr << "loading solution data from disk\n";
				//time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations);
				time_path_image_analyzer.load_image_quantification_and_rerun_death_time_detection(
					region_id,
					time_path_solution,
					time_series_denoising_parameters,
					&markov_solver,
					sql,
					-1);
				time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations);

			}
			//time_path_image_analyzer.add_by_hand_annotations(by_hand_annotations);
			else {
				//std::cerr << "re-analyzing with current data\n";
				time_path_image_analyzer.reanalyze_with_different_movement_estimator(time_series_denoising_parameters, &markov_solver);
			}

			std::set<ns_stationary_path_id> individuals_to_test;
			for (auto p = replicate_spec.test_set.begin(); p != replicate_spec.test_set.end(); p++) {
				if (*p->database_name == *database && p->region_info_id == region_id)
					individuals_to_test.emplace(ns_stationary_path_id(p->group_id, 0, time_path_image_analyzer.db_analysis_id()));
			}
			time_path_image_analyzer.calculate_optimzation_stats_for_current_hmm_estimator(database, output_stats, &model_built_in_this_replicate, individuals_to_test, generate_detailed_path_info);
		}
		catch (ns_ex& ex2) {
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

	static std::string description() { return "plate"; }
	typedef std::pair<const std::string *,const std::string *> index_type;
	typedef std::size_t subgroup_index_type;
	template<class T>
	static index_type get_index_for_observation(const ns_hmm_labeled_data<T>& e) { return index_type(e.database_name,e.region_name); }
	static int minimum_population_size() { return 10; }	//don't use plates with fewer than 10 individuals.
	template<class T>
	static subgroup_index_type get_index_for_subgroup(const ns_hmm_labeled_data <T>& e) { return 0; }
	static int minimum_number_of_subgroups() { return 0; }	//no subgroup requirements
}; 
std::string ns_to_string(const ns_plate_replicate_generator::index_type& i) {
	return *i.first + "::" + *i.second;
}
struct ns_genotype_replicate_generator {
	static std::string description() { return "genotype"; }
	typedef std::string index_type;
	typedef std::size_t subgroup_index_type;
	template<class T>
	static index_type get_index_for_observation(const ns_hmm_labeled_data <T> & e) {
		if (e.genotype == 0)
			return "";
		return *e.genotype;
	}
	static int minimum_population_size() { return 25; }	//don't use genotypes with fewer than 25 individuals.
	template<class T>
	static subgroup_index_type get_index_for_subgroup(const ns_hmm_labeled_data <T>& e) { return 0; }
	static int minimum_number_of_subgroups() { return 0; }	//no subgroup requirements
};
struct ns_device_replicate_generator {
	static std::string description() { return "device"; }
	typedef std::tuple<const std::string *,ns_64_bit,const std::string *> index_type;
	typedef std::string subgroup_index_type;
	template<class T>
	static index_type get_index_for_observation(const ns_hmm_labeled_data <T> & e) {
		return index_type(e.database_name, e.experiment_id, e.device_name);
	}
	static int minimum_population_size() { return 20; }
	template<class T>
	static subgroup_index_type get_index_for_subgroup(const ns_hmm_labeled_data <T>& e) { return *e.region_name; }
	static int minimum_number_of_subgroups() { return 2; }	//don't use devices with fewer than two plates. (this leads to overfitting on specific plates)
};

std::string ns_to_string(const ns_device_replicate_generator::index_type& i) {
	return *std::get<0>(i) + "::" + ns_to_string(std::get<1>(i)) + "::" + *std::get<2>(i);
}
struct ns_individual_replicate_generator {
	static std::string description() { return "individual"; }
	typedef ns_hmm_test_subject index_type;
	typedef std::size_t subgroup_index_type;
	template<class T>
	static index_type get_index_for_observation(const ns_hmm_labeled_data <T>& e) {
		return ns_hmm_test_subject(e.database_name,e.experiment_id,e.region_name, e.region_info_id, e.path_id.group_id);
	}
	static int minimum_population_size() { return 1; }
	template<class T>
	static subgroup_index_type get_index_for_subgroup(const ns_hmm_labeled_data <T>& e) { return 0; }
	static int minimum_number_of_subgroups() { return 0; }	//no subgroup requirements
};

struct ns_cross_validation_sub_result {
	ns_cross_validation_sub_result() :mean_err(0), mean_N(0), var_err(0), var_N(0),event_not_found_N(0), var_event_not_found(0), mean_event_not_found(0){}
	//averages across all replicates
	double mean_err, mean_N, mean_event_not_found, var_err, var_N, var_event_not_found;
	//values for each replicate
	std::vector<double> err, N,event_not_found_N;
};
struct ns_cross_validation_results {
	ns_cross_validation_sub_result movement, expansion;
	double mean_training_set_N, mean_test_set_N, var_training_set_N, var_test_set_N;
};

typedef std::map<std::string, std::map<ns_64_bit, ns_region_metadata>> ns_metadata_cache;

class ns_hmm_cross_validation_analysis_type_and_results {
public:
	ns_hmm_cross_validation_analysis_type_and_results() : generate_detailed_path_info(false), states_permitted(ns_all_states_permitted) {}
	ns_model_building_specification spec;
	bool generate_detailed_path_info;
	bool model_building_completed;
	ns_hmm_states_permitted states_permitted;
	std::vector< ns_cross_validation_replicate_analysis_results> results;

};
class ns_hmm_cross_validation_set {
public:
	ns_hmm_cross_validation_set() {}
	std::vector<ns_cross_validation_replicate_spec> replicates_to_run;
	std::vector<ns_hmm_cross_validation_analysis_type_and_results> analysis_types_and_results;


	std::string description; 
	ns_cross_validation_subject spec;

	void set_up_analysis_structure() {
		analysis_types_and_results.resize(spec.specification.size());
		for (unsigned int i = 0; i < analysis_types_and_results.size(); i++) {
			analysis_types_and_results[i].results.resize(replicates_to_run.size());
			analysis_types_and_results[i].spec = spec.specification[i];
			for (unsigned int j = 0; j < replicates_to_run.size(); j++) {
				analysis_types_and_results[i].results[j].use_to_generate_model_file = replicates_to_run[j].use_to_generate_model_file;
				analysis_types_and_results[i].results[j].replicate_description = &description;
				analysis_types_and_results[i].results[j].replicate_spec = & replicates_to_run[j];
			}
		}
	}

	bool build_device_cross_validation_set(int k_fold_validation, const ns_cross_validation_subject& spec_) {
		spec = spec_;
		return build_independent_replicates<ns_device_replicate_generator>(k_fold_validation, *spec.observations);
	}
	bool build_plate_cross_validation_set(int k_fold_validation, const ns_cross_validation_subject& spec_) {
		spec = spec_;
		return build_independent_replicates<ns_plate_replicate_generator>(k_fold_validation, *spec.observations);
	}
	bool build_individual_cross_validation_set(int k_fold_validation, const ns_cross_validation_subject& spec_) {
		spec = spec_;
		return build_independent_replicates<ns_individual_replicate_generator>(k_fold_validation, *spec.observations);
	}
	bool build_genotype_cross_validation_set(int k_fold_validation, const ns_cross_validation_subject& spec_) {
		spec = spec_;
		return build_independent_replicates<ns_genotype_replicate_generator>(k_fold_validation, *spec.observations);
	}
	void build_all_vs_all_set(const ns_cross_validation_subject& spec_) {
		spec = spec_;
		replicates_to_run.resize(1);
		//all individuals go into training set
		replicates_to_run[0].training_set = *spec.observations;
		//all individuals go into test set
		for (auto observation_list = spec.observations->obs.begin(); observation_list != spec.observations->obs.end(); observation_list++) {
			for (auto observation = observation_list->second.begin(); observation != observation_list->second.end(); observation++) {
				replicates_to_run[0].test_set.emplace(ns_hmm_test_subject(observation->database_name,observation->experiment_id,observation->region_name,observation->region_info_id, observation->path_id.group_id));
			}
		}
		replicates_to_run[0].training_set_individuals = replicates_to_run[0].test_set;
		replicates_to_run[0].use_to_generate_model_file = true;
		set_up_analysis_structure();
	}

	//model_type is the index of the particular model specificiation in the original spec
	//errors are summarized for all replicates analyzed using a model of that spec.
	ns_cross_validation_results calculate_error(const ns_hmm_cross_validation_analysis_type_and_results & analysis) const{
		ns_cross_validation_results results;

		results.mean_test_set_N = results.mean_training_set_N = results.var_test_set_N = results.var_training_set_N = 0;

		for (unsigned int e = 0; e < replicates_to_run.size(); e++) {
			results.mean_training_set_N += replicates_to_run[e].training_set_individuals.size();
			results.mean_test_set_N += replicates_to_run[e].test_set.size();
		}
		if (replicates_to_run.size() > 0) {
			results.mean_training_set_N /= replicates_to_run.size();
			results.mean_test_set_N /= replicates_to_run.size();
		}
		for (unsigned int e = 0; e < replicates_to_run.size(); e++) {
			results.var_training_set_N += pow(replicates_to_run[e].training_set_individuals.size() - results.mean_training_set_N, 2);
			results.var_test_set_N += pow(replicates_to_run[e].test_set.size() - results.mean_test_set_N, 2);
		}
		if (replicates_to_run.size() > 0) {
			results.var_training_set_N = sqrt(results.var_training_set_N / replicates_to_run.size());
			results.var_test_set_N = sqrt(results.var_test_set_N / replicates_to_run.size());
		}

		ns_cross_validation_sub_result* sub_r[2] = { &results.movement,&results.expansion };
		const ns_movement_event step[2] = { ns_movement_cessation,ns_death_associated_expansion_start };
		for (unsigned int m = 0; m < 2; m++) {
			ns_cross_validation_sub_result& r = *sub_r[m];
			r.err.resize(replicates_to_run.size(), 0);
			r.N.resize(replicates_to_run.size(), 0);
			r.event_not_found_N.resize(replicates_to_run.size(), 0);

			bool found_animals = false;
			const ns_hmm_cross_validation_analysis_type_and_results & p = analysis;
			for (unsigned int e = 0; e < p.results.size(); e++) {
				unsigned long movement_Nc(0);
				double movement_square_error(0);
				for (unsigned int i = 0; i < p.results[e].results.animals.size(); i++) {
					found_animals = true;
					const ns_hmm_movement_analysis_optimizatiom_stats_record& animal = p.results[e].results.animals[i];
					auto measurement(animal.measurements.find(step[m]));
					if (measurement==animal.measurements.end() ||
						measurement->second.by_hand_identified &&
						measurement->second.machine_identified) {
						const double d = (measurement->second.machine.best_estimate_event_time_for_possible_partially_unbounded_interval() -
							measurement->second.by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval()) / 24.0 / 60.0 / 60.0;
						movement_square_error += d * d;
						movement_Nc++;
					}
					else r.event_not_found_N[e]++;
				}
				if (movement_Nc > 0)
					r.err[e] = sqrt(movement_square_error / movement_Nc);
				else r.err[e] = 0;
				r.N[e] = movement_Nc;
			}
			if (!found_animals)
				continue;

			for (unsigned int i = 0; i < r.err.size(); i++) {
				r.mean_err += r.err[i];
				r.mean_N += r.N[i];
				r.mean_event_not_found += r.event_not_found_N[i];
			}
			r.mean_err /= replicates_to_run.size();
			r.mean_N /= replicates_to_run.size();
			r.mean_event_not_found /= replicates_to_run.size();

			for (unsigned int i = 0; i < replicates_to_run.size(); i++) {
				r.var_err += (r.err[i] - r.mean_err) * (r.err[i] - r.mean_err);
				r.var_N += (r.N[i] - r.mean_N) * (r.N[i] - r.mean_N);
				r.var_event_not_found += (r.event_not_found_N[i] - r.mean_event_not_found) * (r.event_not_found_N[i] - r.mean_event_not_found);
			}
			r.var_err = sqrt(r.var_err) / replicates_to_run.size();
			r.var_N = sqrt(r.var_N) / replicates_to_run.size();
			r.var_event_not_found = sqrt(r.var_event_not_found) / replicates_to_run.size();
		}
		return results;
	}

private:
	template<class ns_replicate_generator>
	bool build_independent_replicates(int training_to_test_set_size_ratio, const ns_hmm_observation_set& all_observations) {

		//build a randomly shuffled list of all unique observation labels.
		//for devices, this will be a list of all unique devices, for plates it will be plates, for individuals it will be each individual
		//the code is written as if for devices, but the template makes it general.

		//first we make a list of the individuals located on each device,  so we can exclude devices that are too small.
		std::map<typename ns_replicate_generator::index_type, std::set<ns_hmm_test_subject> > individuals_on_each_device;
		std::map<typename ns_replicate_generator::index_type, std::set<typename ns_replicate_generator::subgroup_index_type> > subgroups_per_device;	//we keep track of how many plates there are on each device
		for (auto observation_list = all_observations.obs.begin(); observation_list != all_observations.obs.end(); observation_list++) {
			for (auto observation = observation_list->second.begin(); observation != observation_list->second.end(); observation++) {
				individuals_on_each_device[ns_replicate_generator::get_index_for_observation(*observation)].emplace(ns_hmm_test_subject(observation->database_name,observation->experiment_id,observation->region_name,observation->region_info_id, observation->path_id.group_id));
				subgroups_per_device[ns_replicate_generator::get_index_for_observation(*observation)].emplace(ns_replicate_generator::get_index_for_subgroup(*observation));
			}
		}

		for (auto p = individuals_on_each_device.begin(); p != individuals_on_each_device.end();) {

			//remove devices with too few individuals
			if (p->second.size() < ns_replicate_generator::minimum_population_size()) {
				p = individuals_on_each_device.erase(p);
				continue;
			}
			//remove devices with too few plates
			auto q = subgroups_per_device.find(p->first);
			if (q->second.size() < ns_replicate_generator::minimum_number_of_subgroups()) {
				p = individuals_on_each_device.erase(p);
				continue;
			}
			p++;
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
		unsigned long test_set_size = devices_to_use.size() / (1 + training_to_test_set_size_ratio);
		if (test_set_size == 0)
			test_set_size = 1;

		//it may be that having this many replicates leaves too few animals in the test set
		//therefore, we shrink the replicate number until we have at least 10 individuals in the test set per replicate.
		std::vector<unsigned long> number_of_worms_in_replicate;
		unsigned long number_of_replicates;
		bool found_enough_worms_in_each_replicate(false);
		for (; test_set_size >= 1; test_set_size--) {
			number_of_replicates = devices_to_use.size() / test_set_size + ((devices_to_use.size() % test_set_size) ? 1 : 0);
			if (number_of_replicates < 2)
				break;
			number_of_worms_in_replicate.resize(number_of_replicates, 0);
			for (unsigned int i = 0; i < devices_to_use.size(); i++)
				number_of_worms_in_replicate[i / test_set_size] += individuals_on_each_device[devices_to_use[i]].size();
			unsigned long min_replicate_size = 10000;
			for (unsigned int i = 0; i < number_of_worms_in_replicate.size(); i++)
				if (number_of_worms_in_replicate[i] < min_replicate_size)
					min_replicate_size = number_of_worms_in_replicate[i];
			if (min_replicate_size > ns_replicate_generator::minimum_population_size()) {
				found_enough_worms_in_each_replicate = true;
				break;
			}
		}


		if (!found_enough_worms_in_each_replicate)
			return false;
		//each replicate gets set_size devices in its test set.  All devices not in the test set get added to the training set.
		//to make this happen, we first make a mapping for each device to the replicate id in which it in is the test set.
		replicates_to_run.resize(number_of_replicates);
		std::map<typename ns_replicate_generator::index_type, unsigned long> replicate_for_which_device_is_in_the_test_set;
		for (unsigned int i = 0; i < devices_to_use.size(); i++)
			replicate_for_which_device_is_in_the_test_set[devices_to_use[i]] = i / test_set_size;

		//just to re-count number of individuals in each training set.
		std::vector<std::set< ns_hmm_test_subject> > individuals_in_training_sets(replicates_to_run.size());

		//now load all observations into the training or test sets
		for (auto observation_list = all_observations.obs.begin(); observation_list != all_observations.obs.end(); observation_list++) {
			//find appropriate movement state list in this estimator  
			//go through each observation
			for (auto observation = observation_list->second.begin(); observation != observation_list->second.end(); observation++) {
				const auto rep_id = replicate_for_which_device_is_in_the_test_set.find(ns_replicate_generator::get_index_for_observation(*observation));

				if (rep_id == replicate_for_which_device_is_in_the_test_set.end())
					continue;	//observations from devices that have been discarded for having too few individuals
				//for each replicate, each observation either gets added to the testing or the training sets.
				const ns_hmm_test_subject subject(observation->database_name, observation->experiment_id, observation->region_name, observation->region_info_id, observation->path_id.group_id);
				for (unsigned int i = 0; i < replicates_to_run.size(); i++) {
					if (rep_id->second != i) {
						replicates_to_run[i].training_set.obs[observation_list->first].push_back(*observation);
						replicates_to_run[i].training_set_individuals.emplace(subject);
					}
					else replicates_to_run[i].test_set.emplace(subject);
				}
			}
		}
		for (auto duration_list = all_observations.state_durations.begin(); duration_list != all_observations.state_durations.end(); duration_list++) {
			//find appropriate movement state list in this estimator  
			//go through each observation
			for (auto duration = duration_list->second.begin(); duration != duration_list->second.end(); duration++) {
				const auto rep_id = replicate_for_which_device_is_in_the_test_set.find(ns_replicate_generator::get_index_for_observation(*duration));

				if (rep_id == replicate_for_which_device_is_in_the_test_set.end())
					continue;	//observations from devices that have been discarded for having too few individuals
				//for each replicate, each observation either gets added to the testing or the training sets.
				const ns_hmm_test_subject subject(duration->database_name, duration->experiment_id, duration->region_name, duration->region_info_id, duration->path_id.group_id);
				for (unsigned int i = 0; i < replicates_to_run.size(); i++) {
					if (rep_id->second != i) {
						replicates_to_run[i].training_set.state_durations[duration_list->first].push_back(*duration);
					}
				}
			}
		}
		for (unsigned int i = 0; i < replicates_to_run.size(); i++) {
			replicates_to_run[i].training_set.volatile_number_of_individuals_fully_annotated = replicates_to_run[i].training_set.volatile_number_of_individuals_observed = replicates_to_run[i].training_set_individuals.size();
			replicates_to_run[i].replicate_id = i;
			for (unsigned int d = 0; d < devices_to_use.size(); d++) {
				auto rep_id = replicate_for_which_device_is_in_the_test_set.find(devices_to_use[d]);
				if (rep_id == replicate_for_which_device_is_in_the_test_set.end())
					replicates_to_run[i].replicate_skipped_groups.push_back(ns_to_string(devices_to_use[d]));
				else if (rep_id->second == i)
					replicates_to_run[i].replicate_testing_groups.push_back(ns_to_string(devices_to_use[d]));
				else replicates_to_run[i].replicate_training_groups.push_back(ns_to_string(devices_to_use[d]));
			}

		}
		set_up_analysis_structure();
		return true;
	}
};



std::string ns_model_building_specification::model_description() const {
	std::string o;
	o += "Model " + name + "\n";
	o += "Generated on " + ns_format_time_string_for_human(ns_current_time()) + "\n";
	o += "State transitions are made" + estimator_type_to_long_string(cross_replicate_estimator_type) + "with " + this->state_transition_type_to_string(state_transition_type) + " transition probabilities.\n";
	o += "The following quantitative metrics are used to classify states: \n";
	for (unsigned int i = 0; i < model_features_to_use.size(); i++) {
		o += "\t" + ns_hmm_emission_probability_model_organizer::dimension_description(model_features_to_use[i]) + "\n";
	}
	return o;
}


struct ns_hmm_cross_validation_manager {
	std::map<std::string, ns_hmm_cross_validation_set > validation_runs_sorted_by_validation_type;

	bool all_vs_all_exists() const {
		return validation_runs_sorted_by_validation_type.find("none")!= validation_runs_sorted_by_validation_type.end();
	}
	const ns_hmm_cross_validation_set& all_vs_all() const {
		const auto p = validation_runs_sorted_by_validation_type.find("none");
		if (p == validation_runs_sorted_by_validation_type.end())
			throw ns_ex("ns_hmm_cross_validation_set::all_vs_all()::Could not find all set");
		return p->second;
	}

	void build_models_for_cross_validation(std::string& output) {
		for (auto validation_subject = validation_runs_sorted_by_validation_type.begin(); validation_subject != validation_runs_sorted_by_validation_type.end();) {
			ns_hmm_states_permitted state_specification;
			ns_emperical_posture_quantification_value_estimator::ns_hmm_states_transition_types transition_type;
			//ns_emperical_posture_quantification_value_estimator::ns_all_states;
			for (auto cur_analysis = validation_subject->second.analysis_types_and_results.begin(); cur_analysis != validation_subject->second.analysis_types_and_results.end(); ) {
				switch (cur_analysis->spec.cross_replicate_estimator_type) {
				case ns_model_building_specification::ns_strict_ordering:
					state_specification = ns_no_expansion_while_alive; break;
				case ns_model_building_specification::ns_simultaneous_movement_cessation_and_expansion:
					state_specification = ns_require_movement_expansion_synchronicity; break;
				default:
					state_specification = ns_all_states_permitted; break;
				}
				switch (cur_analysis->spec.cross_replicate_estimator_type) {
				case ns_model_building_specification::ns_static:
					transition_type = ns_emperical_posture_quantification_value_estimator::ns_static; break;
				case ns_model_building_specification::ns_static_mod:
					transition_type = ns_emperical_posture_quantification_value_estimator::ns_static_mod; break;
				case ns_model_building_specification::ns_empirical:
					transition_type = ns_emperical_posture_quantification_value_estimator::ns_empirical; break;
				case ns_model_building_specification::ns_empirical_without_weights:
					transition_type = ns_emperical_posture_quantification_value_estimator::ns_empirical_without_weights; break;
				default:
					state_specification = ns_all_states_permitted; break;
				}

				bool all_replicates_supported_model_building = true;

				ns_probability_model_generator gen(cur_analysis->spec.model_features_to_use);

				cur_analysis->model_building_completed = true;
				for (unsigned int replicate_id = 0; replicate_id < validation_subject->second.replicates_to_run.size(); replicate_id++) {
					try {
						bool success =
							cur_analysis->results[replicate_id].model_built_in_this_replicate.build_estimator_from_observations(
								cur_analysis->spec.model_description(),
								validation_subject->second.replicates_to_run[replicate_id].training_set, &gen, state_specification, transition_type, output
							);
						if (!success) {
							cur_analysis->model_building_completed = all_replicates_supported_model_building = false;
							break;
						}
						else cur_analysis->model_building_completed = true;
					}
					catch (ns_ex & ex) {
						std::cout << "Error building HMM model: " << ex.text() << "\n";
						cur_analysis->model_building_completed = all_replicates_supported_model_building = false;
						break;
					}
				}

				if (all_replicates_supported_model_building)
					cur_analysis++;
				else {
					//if some of the replicates failed, delete the whole validation approach.
					cur_analysis = validation_subject->second.analysis_types_and_results.erase(cur_analysis);
				}
			}
			if (validation_subject->second.analysis_types_and_results.empty())
				validation_subject = validation_runs_sorted_by_validation_type.erase(validation_subject);
			else validation_subject++;
		}
	}
	
	void generate_cross_validations_to_test(int k_fold_validation, const ns_cross_validation_subject & spec, bool try_different_states) {
		validation_runs_sorted_by_validation_type.clear();
		if (1){
			ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["none"];
			set.build_all_vs_all_set(spec);
			for (unsigned int j = 0; j < set.analysis_types_and_results.size(); j++)
				set.analysis_types_and_results[j].generate_detailed_path_info = false;
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
			if (!set.build_device_cross_validation_set(k_fold_validation, spec))
				validation_runs_sorted_by_validation_type.erase("devices");
			else set.description = "Device Cross-Validation";
		}
		{
			ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["plates"];
			if (!set.build_plate_cross_validation_set(k_fold_validation, spec))
				validation_runs_sorted_by_validation_type.erase("plates");
			else set.description = "Plate Cross-Validation";
		}
		{
			ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["individuals"];
			if (!set.build_individual_cross_validation_set(k_fold_validation, spec))
				validation_runs_sorted_by_validation_type.erase("individuals");
			else set.description = "Individual Cross-Validation";
		}
	}

	void generate_genotype_cross_validations_to_test(int k_fold_validation, const ns_cross_validation_subject& spec) {

		ns_hmm_cross_validation_set& set = validation_runs_sorted_by_validation_type["genotype"];
		if (!set.build_genotype_cross_validation_set(k_fold_validation, spec))
			validation_runs_sorted_by_validation_type.erase("genotype");
		else set.description = "Genotype Cross-Validation";
	}
};

struct ns_results_info {
	ns_results_info() :failed(false) {}
	ns_results_info(const std::string& r, bool f) :result(r), failed(f) {}
	std::string result;
	bool failed;
};
void ns_run_hmm_cross_validation(std::string& results_summary, ns_image_server_results_subject sub, ns_machine_analysis_data_loader& movement_results, const std::map < std::string, ns_cross_validation_subject>& models_to_fit, ns_sql& sql) {
	const bool run_hmm_cross_validation = true;
	const bool test_different_state_restrictions_on_viterbi_algorithm = false;

	int k_fold_validation = run_hmm_cross_validation ? 5 : 1;

	std::set<std::pair<std::string, ns_64_bit> > db_experiment_used_in_training_set;
	std::set<std::pair<std::string, ns_64_bit> > db_region_used_in_training_set;

	//build metadata cache for all regions in all experiments across all dbs required.

	//first string is the database name, second map is the 64_bit id
	ns_metadata_cache metadata_cache;
	std::map<std::string, ns_region_metadata> metadata_cache_by_name;
	std::map <std::string, std::map <ns_64_bit, ns_time_series_denoising_parameters>> time_series_denoising_parameters_cache;

	for (auto p = models_to_fit.begin(); p != models_to_fit.end(); p++) {
		for (auto q = p->second.observations->obs.begin(); q != p->second.observations->obs.end(); q++) {
			for (unsigned int i = 0; i < q->second.size(); i++) {
				if (q->second[i].experiment_id == 0)
					throw ns_ex("Encountered a zero experiment id");
				db_experiment_used_in_training_set.insert(db_experiment_used_in_training_set.end(), std::pair<std::string, ns_64_bit>(*q->second[i].database_name, q->second[i].experiment_id));
				db_region_used_in_training_set.insert(db_region_used_in_training_set.end(), std::pair<std::string, ns_64_bit>(*q->second[i].database_name, q->second[i].region_info_id));
			}
		}
	}

	for (auto p = db_experiment_used_in_training_set.begin(); p != db_experiment_used_in_training_set.end(); p++) {

		ns_select_database_for_scope db(p->first, sql);
		sql << "SELECT r.id FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = " << p->second;
		ns_sql_result res;
		sql.get_rows(res);
		std::map <ns_64_bit, ns_region_metadata >& metadata_cache_for_db = metadata_cache[p->first];
		std::map <ns_64_bit, ns_time_series_denoising_parameters>& ts_cache = time_series_denoising_parameters_cache[p->first];
		for (unsigned int i = 0; i < res.size(); i++) {
			const ns_64_bit region_id(ns_atoi64(res[i][0].c_str()));
			auto m = metadata_cache_for_db.find(region_id);
			if (m == metadata_cache_for_db.end()) {
				m = metadata_cache_for_db.insert(metadata_cache_for_db.end(), std::pair<ns_64_bit, ns_region_metadata>(region_id, ns_region_metadata()));
				m->second.load_from_db(region_id, "", sql);
				const std::string plate_type_summary(m->second.plate_type_summary("-", true));

				metadata_cache_by_name[plate_type_summary] = m->second;

				if (ts_cache.find(region_id) == ts_cache.end())
					ts_cache[region_id] = ns_time_series_denoising_parameters::load_from_db(region_id, sql);
			}

		}

	}


	/*
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
	*/

	std::map<std::string, ns_hmm_cross_validation_manager> cross_validation_sets;

	std::map<std::string, ns_results_info> model_building_and_testing_info;
	std::map<std::string, std::string> model_filenames;
	unsigned long estimators_compiled(0);
	if (0) {
		std::cout << "Writing state emission data to disk...\n";
		//for each type of plate, build an estimator from the observations collected.
		for (auto p = models_to_fit.begin(); p != models_to_fit.end(); p++) {
			std::cout << ns_to_string_short((100.0 * estimators_compiled) / models_to_fit.size(), 2) << "%...";
			estimators_compiled++;
			//all estimator types share the same input data--we don't need to write multiple times.
			ns_select_database_for_scope db("", sql);
			if (!sub.database_name.empty())
				db.select(sub.database_name);
			ns_acquire_for_scope<ns_ostream> all_observations(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("hmm_dur=") + p->first, true, sql).output());
			p->second.observations->write_emissions(all_observations()(), sub.experiment_name);
			all_observations.release();
		}
	}
	if (1) {
		std::cout << "Writing state duration data to disk...\n";
		//for each type of plate, build an estimator from the observations collected.
		for (auto p = models_to_fit.begin(); p != models_to_fit.end(); p++) {
			std::cout << ns_to_string_short((100.0 * estimators_compiled) / models_to_fit.size(), 2) << "%...";
			estimators_compiled++;
			//all estimator types share the same input data--we don't need to write multiple times.
			ns_select_database_for_scope db("", sql);
			if (!sub.database_name.empty())
				db.select(sub.database_name);
			ns_acquire_for_scope<ns_ostream> all_observations(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("hmm_dur=") + p->first, true, sql).output());
			p->second.observations->write_durations(all_observations()(), sub.experiment_name);
			all_observations.release();
		}
	}
	else std::cout << "Skipping state emission data output.\n";
	std::cout << "\nPrepping for Cross Validation...\n";
	//set up models for cross validation
	estimators_compiled = 0;
	for (auto p = models_to_fit.begin(); p != models_to_fit.end(); p++) {
		int this_k_fold_validation = k_fold_validation;
		if (p->second.observations->volatile_number_of_individuals_fully_annotated < 50)
			this_k_fold_validation = 2;
		std::cout << ns_to_string_short((100.0 * estimators_compiled) / models_to_fit.size(), 2) << "%...";
		estimators_compiled++;
		cross_validation_sets[p->first].generate_cross_validations_to_test(this_k_fold_validation, p->second, test_different_state_restrictions_on_viterbi_algorithm);
	}

	//cross validate across genotypes
	for (auto p = models_to_fit.begin(); p != models_to_fit.end(); p++) {
		if (p->second.cross_replicate_type == ns_cross_validation_subject::ns_genotype_specific)	//we can't cross validate across genotypes if there is only one genotype!
			continue;
		cross_validation_sets[p->first].generate_genotype_cross_validations_to_test(k_fold_validation, p->second);
	}
	if (cross_validation_sets.size() == 0) {
		results_summary += "No HMM models could be built";
		return;
	}
	else {
		if (cross_validation_sets.size() > 1)
			results_summary += "== Attempting to build HMM models on " + ns_to_string(cross_validation_sets.size()) + " different subsets of animals. ==\n";
		else
			results_summary += "== Attempting to build an HMM model using all animals. ==\n";

		results_summary += "All data was collated and analyzed at " + ns_format_time_string_for_human(ns_current_time()) + "\n\n";
	}
	std::cout << "\nBuilding HMM Models...\n";
	unsigned long num_built(0);
	const unsigned long num_to_build(cross_validation_sets.size());
	for (auto p = cross_validation_sets.begin(); p != cross_validation_sets.end(); ) {

		std::cout << ns_to_string_short((100.0 * num_built) / num_to_build, 2) << "%...";
		num_built++;
		std::string genotype;
		if (p->second.validation_runs_sorted_by_validation_type.begin()->second.spec.cross_replicate_type == ns_cross_validation_subject::ns_all_data ||
			p->second.validation_runs_sorted_by_validation_type.begin()->second.spec.cross_replicate_type == ns_cross_validation_subject::ns_experiment_specific)
			genotype = "all animal types";
		else
			genotype = *(p->second.validation_runs_sorted_by_validation_type.begin()->second.replicates_to_run.begin()->training_set.obs.begin()->second.begin()->genotype);
		if (!p->second.validation_runs_sorted_by_validation_type.begin()->second.spec.subject.empty())
			genotype += " in " + p->second.validation_runs_sorted_by_validation_type.begin()->second.spec.subject;

		p->second.build_models_for_cross_validation(model_building_and_testing_info[p->first].result);
		//if none of the validation approaches could be used for building a model, delete this whole cross validation set.
		if (p->second.validation_runs_sorted_by_validation_type.empty()) {
			model_building_and_testing_info[p->first].result = " No model could be built for " + genotype + " because the existing annotations were insufficient for cross-validation. \n" + model_building_and_testing_info[p->first].result;
			model_building_and_testing_info[p->first].failed = true;
			p = cross_validation_sets.erase(p);
			continue;
		}
		else {
			model_building_and_testing_info[p->first].result = "== HMM model built for " + genotype + " ==\n";
			model_building_and_testing_info[p->first].failed = false;
		}

		if (p->second.all_vs_all_exists()) {
			ns_select_database_for_scope db("", sql);
			if (!sub.database_name.empty())
				db.select(sub.database_name);
			//all vs all has all multiple types of analysis, each with a single replicate containing all individuals
			for (auto q = p->second.all_vs_all().analysis_types_and_results.begin(); q != p->second.all_vs_all().analysis_types_and_results.end(); q++) {
				for (auto r = q->results.begin(); r != q->results.end(); r++) {
					std::string file_suffix(p->first + "=" + ns_model_building_specification::estimator_type_to_short_string(q->spec.cross_replicate_estimator_type) + "=" + q->spec.name + "=" + *r->replicate_description);
					if (!r->use_to_generate_model_file)
						continue;
					ns_image_server_results_file ps(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, std::string("hmm=") + file_suffix, sql));
					model_filenames[p->first] = ps.output_filename();
					ns_acquire_for_scope<ns_ostream> both_parameter_set(ps.output());
					r->model_built_in_this_replicate.write(both_parameter_set()());
					both_parameter_set.release();
					//test file I/O
					ns_acquire_for_scope<ns_istream> both_parameter_set2(ps.input());
					ns_emperical_posture_quantification_value_estimator dummy;
					dummy.read(both_parameter_set2()());
					if (!(r->model_built_in_this_replicate == dummy))
						throw ns_ex("HMM Model File I/O error!");

					both_parameter_set2.release();

					ns_acquire_for_scope<ns_ostream>  graphvis_file(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("state_transition_graph=") + file_suffix, true, sql).output());
					ns_hmm_solver solver;
					std::vector<std::vector<double> > state_transition_weight_matrix, state_transition_matrix;
					solver.build_state_transition_weight_matrix(r->model_built_in_this_replicate, state_transition_weight_matrix);
					for (unsigned int i = 0; i < state_transition_weight_matrix.size(); i++)
						for (unsigned int j = 0; j < state_transition_weight_matrix[i].size(); j++)
							state_transition_weight_matrix[i][j] = log(state_transition_weight_matrix[i][j]);
					r->model_built_in_this_replicate.state_transition_log_probabilities(60 * 60 * 6, state_transition_weight_matrix, state_transition_matrix);
					for (unsigned int i = 0; i < state_transition_matrix.size(); i++)
						for (unsigned int j = 0; j < state_transition_matrix[i].size(); j++)
							state_transition_matrix[i][j] = exp(state_transition_matrix[i][j]);
					for (unsigned int i = 0; i < file_suffix.size(); i++)
						if (file_suffix[i] == '=' || file_suffix[i] == ' ') file_suffix[i] = '_';
					solver.output_state_transition_matrix("g_" + file_suffix, state_transition_matrix, graphvis_file()());
					graphvis_file.release();
				}
			}

		}
		else model_filenames[p->first] = "(Not Written)";
		++p;
	}
	if (cross_validation_sets.empty()) {
		throw ns_ex("No model files could be built from this data set.");
	}


	//Now iterate across all experiments and test all the models!
	for (auto experiment = db_experiment_used_in_training_set.begin(); experiment != db_experiment_used_in_training_set.end(); experiment++) {
		const bool changed_db(experiment->first != sql.database());
		ns_select_database_for_scope db(experiment->first, sql);

		if (experiment->second == 0)
			throw ns_ex("Encountered a zero experiment id");
		if (changed_db || experiment->second != movement_results.experiment_id())
			movement_results.load(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, 0, 0, experiment->second, sql, false);

		std::cout << "\nCross-validating HMM models on experiment " << movement_results.experiment_name() << "\n";
		num_built = 0;
		unsigned long region_count = 0;
		for (unsigned int i = 0; i < movement_results.samples.size(); i++) {
			if (!sub.device_name.empty() && movement_results.samples[i].device_name() != sub.device_name)
				continue;
			for (unsigned int j = 0; j < movement_results.samples[i].regions.size(); j++) {
				if (sub.region_id != 0 && sub.region_id != movement_results.samples[i].regions[j]->metadata.region_id)
					continue;

				//only consider regions needed for building the hmm model
				if (db_region_used_in_training_set.find(std::pair<std::string, ns_64_bit>(experiment->first, movement_results.samples[i].regions[j]->metadata.region_id)) == db_region_used_in_training_set.end())
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


				//only consider regions needed for building the hmm model
				if (db_region_used_in_training_set.find(std::pair<std::string, ns_64_bit>(experiment->first, movement_results.samples[i].regions[j]->metadata.region_id)) == db_region_used_in_training_set.end())
					continue;

				const std::string plate_type_summary = movement_results.samples[i].regions[j]->metadata.plate_type_summary("-", true);
				const std::string hmm_group = movement_results.samples[i].regions[j]->metadata.strain_condition_3;
				const std::string experiment_name_to_use = movement_results.samples[i].regions[j]->metadata.experiment_name;
				movement_results.samples[i].regions[j]->contains_a_by_hand_death_time_annotation = true;
				movement_results.samples[i].regions[j]->time_path_solution.load_from_db(movement_results.samples[i].regions[j]->metadata.region_id, sql, true);

				/*
				ns_posture_analysis_model dummy_model(ns_posture_analysis_model::dummy());
				const ns_posture_analysis_model* posture_analysis_model(&dummy_model);
				ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
				image_server.get_posture_analysis_model_for_region(movement_results.samples[i].regions[j]->metadata.region_id, handle, sql);
				posture_analysis_model = &handle().model_specification;

				ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
					ns_get_death_time_estimator_from_posture_analysis_model(
						handle().model_specification));
				*/


				movement_results.samples[i].regions[j]->time_path_image_analyzer->load_completed_analysis(
					movement_results.samples[i].regions[j]->metadata.region_id,
					movement_results.samples[i].regions[j]->time_path_solution,
					sql,
					false);
				//death_time_estimator.release();


				ns_hand_annotation_loader by_hand_annotations;
				by_hand_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,
					movement_results.samples[i].regions[j]->metadata.region_id,
					experiment->second,
					movement_results.samples[i].regions[j]->metadata.experiment_name,
					movement_results.samples[i].regions[j]->metadata,
					sql);

				movement_results.samples[i].regions[j]->by_hand_annotations = by_hand_annotations.annotations;
				movement_results.samples[i].regions[j]->time_path_image_analyzer->add_by_hand_annotations(by_hand_annotations.annotations);


				std::cout << ns_to_string_short((100.0 * num_built) / region_count, 2) << "%...";
				num_built++;
				const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(movement_results.samples[i].regions[j]->metadata.region_id, sql));

				for (auto p = cross_validation_sets.begin(); p != cross_validation_sets.end(); p++) {
					//make sure we only test on the regions specified in the cross validation set
					const ns_cross_validation_subject& spec(p->second.validation_runs_sorted_by_validation_type.begin()->second.spec);
					if (!(spec.cross_replicate_type == ns_cross_validation_subject::ns_genotype_specific &&
						spec.genotype != plate_type_summary)
						&&
						!((spec.cross_replicate_type == ns_cross_validation_subject::ns_hmm_user_specified_specific ||
							spec.cross_replicate_type == ns_cross_validation_subject::ns_hmm_user_specified_experiment_specific) &&
							spec.genotype != hmm_group)
						)
						for (auto cross_validation_runs = p->second.validation_runs_sorted_by_validation_type.begin(); cross_validation_runs != p->second.validation_runs_sorted_by_validation_type.end(); ++cross_validation_runs) {

							//xxx 
							//if (p->first != "Alcedo+Wildtype-0-20C-uv+nec-control")
							//	continue;

							for (auto analysis_type = cross_validation_runs->second.analysis_types_and_results.begin(); analysis_type != cross_validation_runs->second.analysis_types_and_results.end(); analysis_type++) {
								for (auto replicate = analysis_type->results.begin(); replicate != analysis_type->results.end(); replicate++) {
									replicate->test_model(replicate->model_built_in_this_replicate, *(replicate->replicate_spec), &experiment->first,
										movement_results.samples[i].regions[j]->metadata.region_id, movement_results.samples[i].regions[j]->by_hand_annotations,
										time_series_denoising_parameters, *movement_results.samples[i].regions[j]->time_path_image_analyzer, movement_results.samples[i].regions[j]->time_path_solution, sql,
										replicate->results, analysis_type->generate_detailed_path_info);
								}
							}
						}
				}
				movement_results.samples[i].regions[j]->time_path_image_analyzer->clear();
			}
		}
	}
	{
		ns_select_database_for_scope scope(sub.database_name, sql);

		std::cout << "\nWriting HMM model test results to disk.\n";
		//write all the different plate type stats to disk
		num_built = 0;
		for (auto p = cross_validation_sets.begin(); p != cross_validation_sets.end(); p++) {
			std::cout << ns_to_string_short((100.0 * num_built) / cross_validation_sets.size(), 2) << "%...";
			num_built++;

			//start at 1 to skip the all vs all.
			for (auto validation_run = p->second.validation_runs_sorted_by_validation_type.begin(); validation_run != p->second.validation_runs_sorted_by_validation_type.end(); ++validation_run) {
				for (auto analysis_type = validation_run->second.analysis_types_and_results.begin(); analysis_type != validation_run->second.analysis_types_and_results.end(); analysis_type++) {
					ns_cross_validation_results results = validation_run->second.calculate_error(*analysis_type);

					const bool many_movement_failures(results.movement.mean_event_not_found > results.movement.mean_N / 10),
						many_expansion_failures(results.expansion.mean_event_not_found > results.expansion.mean_N / 10);
					double movement_failure_rate = (100.0 * results.movement.mean_event_not_found) / results.movement.mean_N;
					double expansion_failure_rate = (100.0 * results.expansion.mean_event_not_found) / results.expansion.mean_N;
					std::string& txt(model_building_and_testing_info[p->first].result);
					txt += "= ";
					if (analysis_type->results.size() > 1)
						txt += ns_to_string(analysis_type->results.size()) + "-fold ";
					txt += validation_run->second.description + " " + analysis_type->spec.name + " =\n";
					txt += "Each replicate trained on " + ns_to_string_short(results.mean_training_set_N) + "+-" + ns_to_string_short(results.var_training_set_N) + " animals and was tested on " + ns_to_string_short(results.mean_test_set_N) + "+-" + ns_to_string_short(results.var_test_set_N) + " animals.\n";
					if (results.movement.mean_N + results.movement.mean_event_not_found > 0)
						txt += "[Movement cessation]:   Average error: " + ns_to_string_short(results.movement.mean_err, 2) + "+-" + ns_to_string_short(results.movement.var_err, 2) + " days across " + ns_to_string((int)results.movement.mean_N) + " animals, with " + ns_to_string((int)results.movement.mean_event_not_found) + "+-" + ns_to_string((int)results.movement.var_event_not_found) + " animals' movement cessation remaining unidentifiable.\n";
					if (results.expansion.mean_N + results.expansion.mean_event_not_found > 0)
						txt += "[Death-Associated expansion]: Average error: " + ns_to_string_short(results.expansion.mean_err, 2) + " +-" + ns_to_string_short(results.expansion.var_err, 2) + " days across " + ns_to_string((int)results.expansion.mean_N) + " animals, with " + ns_to_string((int)results.expansion.mean_event_not_found) + "+-" + ns_to_string((int)results.expansion.var_event_not_found) + " animals' expansion times remaining unidentifiable.\n";

					if (many_movement_failures)
						txt += "Movement cessation times could not be found for " + ns_to_string_short(movement_failure_rate, 1) + "% of individuals. This is probably not a useful model file.\n";
					if (many_expansion_failures)
						txt += "Death-associated expansion times could not be found for " + ns_to_string_short(expansion_failure_rate, 1) + "% of individuals. This is probably not a useful model file.\n";
					if (!many_movement_failures && !many_expansion_failures && (results.movement.mean_N < 50 || results.expansion.mean_N < 50))
						txt += "Warning: These results will not be meaningful until ~50 animals are annotated by hand.\n";
					txt += "The model file was written to \"" + model_filenames[p->first] + "\"\n";
					txt += "\n";
				}
			}
			//output performance statistics
			ns_acquire_for_scope<ns_ostream>  performance_stats_output(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("hmm_performance=") + p->first, true, sql).output());
			//get all measurements used by any analysis model
			std::set<std::string > measurement_names;
			for (auto r = p->second.validation_runs_sorted_by_validation_type.begin(); r != p->second.validation_runs_sorted_by_validation_type.end(); ++r) {
				for (auto analysis_type = r->second.analysis_types_and_results.begin(); analysis_type != r->second.analysis_types_and_results.end(); analysis_type++) {
					if (analysis_type->results.empty())
						continue;
					if (!analysis_type->results.begin()->results.animals.empty()) {
						std::vector<std::string>& names(analysis_type->results.begin()->results.animals[0].state_info_variable_names);
						measurement_names.insert(names.begin(), names.end());
						break;
					}
				}
			}
			std::vector<std::string > measurement_names_ordered(measurement_names.size());
			std::size_t mm_pos(0);

			for (auto mm = measurement_names.begin(); mm != measurement_names.end(); mm++) {
				measurement_names_ordered[mm_pos] = *mm;
				mm_pos++;
			}

			if (!measurement_names.empty()) {
				ns_hmm_movement_analysis_optimizatiom_stats::write_error_header(performance_stats_output()(), measurement_names_ordered);
				for (auto cross_validation_set = p->second.validation_runs_sorted_by_validation_type.begin(); cross_validation_set != p->second.validation_runs_sorted_by_validation_type.end(); ++cross_validation_set) {
					for (auto analysis_type = cross_validation_set->second.analysis_types_and_results.begin(); analysis_type != cross_validation_set->second.analysis_types_and_results.end(); analysis_type++) {
						for (auto r = analysis_type->results.begin(); r != analysis_type->results.end(); r++)
							r->results.write_error_data(analysis_type->spec.name, measurement_names_ordered, performance_stats_output()(), p->first, cross_validation_set->first, r->replicate_spec->replicate_id, metadata_cache);
					}
				}
			}
			else
				std::cerr << "Could not find state info variable names!\n";
			performance_stats_output.release();


			ns_acquire_for_scope<ns_ostream>  replicate_info_output(image_server.results_storage.time_path_image_analysis_quantification(sub, std::string("hmm_replicate_info=") + p->first, true, sql).output());
			replicate_info_output()() << "Cross Validation Strategy,Replicate ID,Set Type,Set Member\n";
			unsigned int tmp = 0;
			for (auto cross_validation_set = p->second.validation_runs_sorted_by_validation_type.begin(); cross_validation_set != p->second.validation_runs_sorted_by_validation_type.end(); ++cross_validation_set) {

				for (auto q = cross_validation_set->second.replicates_to_run.begin(); q != cross_validation_set->second.replicates_to_run.end(); q++) {

					for (unsigned int k = 0; k < q->replicate_training_groups.size(); k++)
						replicate_info_output()() << cross_validation_set->first << "," << q->replicate_id << ",training," << q->replicate_training_groups[k] << "\n";
					for (unsigned int k = 0; k < q->replicate_testing_groups.size(); k++)
						replicate_info_output()() << cross_validation_set->first << "," << q->replicate_id << ",testing," << q->replicate_testing_groups[k] << "\n";
					for (unsigned int k = 0; k < q->replicate_skipped_groups.size(); k++)
						replicate_info_output()() << cross_validation_set->first << "," << q->replicate_id << ",training," << q->replicate_skipped_groups[k] << "\n";
				}
				for (auto q = cross_validation_set->second.analysis_types_and_results.begin(); q != cross_validation_set->second.analysis_types_and_results.end(); q++) {
					for (auto rep = q->results.begin(); rep != q->results.end(); ++rep) {

						if (!q->generate_detailed_path_info || rep->results.animals.empty())
							continue;
						//output per-path statistics
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
						rep->results.write_hmm_path_header(path_stats_output()());
						rep->results.write_hmm_path_data(q->spec.name, path_stats_output()(), metadata_cache);
						path_stats_output.release();
					}
				}
			}
			replicate_info_output.release();
		}
	}
	//write info about successful models first
	for (auto p = model_building_and_testing_info.begin(); p != model_building_and_testing_info.end(); p++)
		if (!model_building_and_testing_info[p->first].failed)
			results_summary += model_building_and_testing_info[p->first].result + "\n";
	results_summary += "== Models that could not be built ==\n";
	for (auto p = model_building_and_testing_info.begin(); p != model_building_and_testing_info.end(); p++)
		if (model_building_and_testing_info[p->first].failed)
			results_summary += model_building_and_testing_info[p->first].result + "\n";
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
					best.posture_cutoff = best.stationary_cutoff * 10;
					best.permanance_time_required_in_seconds = best_parameter_set_by_count[i - min_number_of_counts].second;
				}
				else {
					//	best.death_time_expansion_cutoff = best_parameter_set_by_count[i - min_number_of_counts].first;
					//	best.death_time_expansion_time_kernel_in_seconds = best_parameter_set_by_count[i - min_number_of_counts].second;
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
	if (v1_parameters == true) {
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

	ns_acquire_for_scope<ns_ostream> posture_analysis_optimization_output;// , expansion_analysis_optimization_output;
	if (run_posture) {
		posture_analysis_optimization_output.attach(image_server.results_storage.time_path_image_analysis_quantification(sub, "threshold_posture_optimization_stats", true, sql).output());
		ns_analyzed_image_time_path::write_posture_analysis_optimization_data_header(posture_analysis_optimization_output()());
	}
	//if (run_expansion) {
	//	expansion_analysis_optimization_output.attach(image_server.results_storage.time_path_image_analysis_quantification(sub, "threshold_expansion_optimization_stats", true, sql).output());
	//	ns_analyzed_image_time_path::write_expansion_analysis_optimization_data_header(expansion_analysis_optimization_output()());
	//}
	unsigned region_count(0);

	//0:region name, 1:region_id, 2:sample_name, 3:sample_id, 4:device_name, 5:excluded
	for (unsigned int i = 0; i < experiment_regions.size(); i++) {
		if (!sub.device_name.empty() && experiment_regions[i][4] != sub.device_name)
			continue;
		if (sub.region_id != 0 && ns_atoi64(experiment_regions[i][1].c_str()) != sub.region_id)
			continue;
		region_count++;
	}

	std::map<std::string, ns_parameter_set_optimization_record> best_posture_parameter_sets;


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
					posture_model_used = time_path_image_analyzer.measurement_format_version_used;
				else if (posture_model_used != time_path_image_analyzer.measurement_format_version_used)
					throw ns_ex("Not all regions are using the same posture analysis version!  Please update to the latest model version and run \"ReBuild Worm Analysis from Cached Solution\" for all regions.");

				ns_posture_analysis_model mod(handle().model_specification);
				ns_threshold_movement_posture_analyzer_parameters default_model_parameters = mod.threshold_parameters;
				default_model_parameters.use_v1_movement_score = time_path_image_analyzer.measurement_format_version_used == "1";
				default_model_parameters.version_flag = ns_threshold_movement_posture_analyzer::current_software_version();

				handle.release();
				//ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(

				//	ns_get_death_time_estimator_from_posture_analysis_model(mod));

				time_path_image_analyzer.load_completed_analysis(region_id, time_path_solution, sql);
				//death_time_estimator.release();
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
				//if (run_expansion)
				//	expansion_analysis_optimization_output()() << "\n";
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
						p_all->second.best = default_model_parameters;
					}
					std::map<std::string, ns_parameter_set_optimization_record>::iterator p = best_posture_parameter_sets.find(strain_name);
					if (p == best_posture_parameter_sets.end()) {
						if (image_server.verbose_debug_output())
							std::cout << "Creating record for plate type " << strain_name << "\n";


						p = best_posture_parameter_sets.insert(best_posture_parameter_sets.begin(),
							std::pair<std::string, ns_parameter_set_optimization_record>(strain_name,
								ns_parameter_set_optimization_record(posture_analysis_thresholds, posture_analysis_hold_times)));
						p->second.best = default_model_parameters;

					}
					else {

						if (image_server.verbose_debug_output())
							std::cout << "Adding to record for plate type " << strain_name << "\n";
					}

					time_path_image_analyzer.write_posture_analysis_optimization_data(time_path_image_analyzer.measurement_format_version_used, posture_analysis_thresholds, posture_analysis_hold_times, metadata, posture_analysis_optimization_output()(), p->second.new_parameters_results, &(p_all->second.new_parameters_results));

					//include existing model file
					std::vector<double> thresh;
					std::vector<unsigned long>hold_t;
					if (log(posture_analysis_model_handle().model_specification.threshold_parameters.stationary_cutoff) > 20)
						std::cerr << "Invalid model file spec";
					else {
						thresh.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.stationary_cutoff);
						hold_t.push_back(posture_analysis_model_handle().model_specification.threshold_parameters.permanance_time_required_in_seconds);
						time_path_image_analyzer.write_posture_analysis_optimization_data(time_path_image_analyzer.measurement_format_version_used, thresh, hold_t, metadata, posture_analysis_optimization_output()(), p->second.current_parameter_results, &(p_all->second.current_parameter_results));
					}
				}
				/*
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
					
				}*/

			}

		}
		catch (ns_ex & ex) {
			std::cerr << "\n" << ex.text() << "\n";
		}
	}

	std::vector<double>* thresh;
	std::vector<unsigned long>* hold;
	std::string data_name;
	data_name = "Posture Analysis";
	thresh = &posture_analysis_thresholds;
	hold = &posture_analysis_hold_times;
		
	
	results_text += "===Automated " + data_name + " Calibration Results == \n";
	results_text += "Calculated at " + ns_format_time_string_for_human(ns_current_time()) + "\n\n";
	for (auto p = best_posture_parameter_sets.begin(); p != best_posture_parameter_sets.end(); p++) {

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
	

	//output best parameters to disk
	/*if (run_posture && run_expansion) {
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
	}*/
	//else {
		
		for (auto p = best_posture_parameter_sets.begin(); p != best_posture_parameter_sets.end(); p++) {
			if (p->second.new_parameters_results.number_valid_worms == 0)
				continue;
			ns_acquire_for_scope<ns_ostream> parameter_set_output(image_server.results_storage.optimized_posture_analysis_parameter_set(sub, p->second.filename, sql).output());
			std::string& d(p->second.best.model_description_text);
			d += p->second.filename + "\n";
			d += "Model generated on " + ns_format_time_string_for_human(ns_current_time()) + "\n";
			d += "Model is based on " + ns_to_string(p->second.total_count) + " annotated individuals.";
			p->second.best.write(parameter_set_output()());
			parameter_set_output.release();
		}
	//}
}
