#include "ns_analyze_movement_over_time.h"
#include "ns_processing_job_processor.h"
#include "ns_processing_job_push_scheduler.h"
#include "ns_experiment_storyboard.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_hand_annotation_loader.h"
#include "ns_heat_map_interpolation.h"
#include "ns_captured_image_statistics_set.h"
#include "ns_image_server_time_path_inferred_worm_aggregator.h"





void analyze_worm_movement_across_frames(const ns_processing_job & job, ns_image_server * image_server, ns_sql & sql, bool log_output) {
	if (job.region_id == 0)
		throw ns_ex("Movement data can be rebuilt only for regions.");

	ns_high_precision_timer tm;
	tm.start();

	ns_image_server_results_subject results_subject;
	results_subject.region_id = job.region_id;


	const bool skip_inferred_worm_analysis(image_server->get_cluster_constant_value("skip_inferred_worm_analysis", "false", &sql) != "false");

	ns_time_path_solution time_path_solution;
	if (skip_inferred_worm_analysis) {
		try {
			time_path_solution.load_from_db(job.region_id, sql, false);
		}
		catch (ns_ex & ex) {
			image_server->register_server_event(ex, &sql);
			ns_time_path_solver tp_solver;
			tp_solver.load(job.region_id, sql);
			ns_time_path_solver_parameters solver_parameters(ns_time_path_solver_parameters::default_parameters(job.region_id, sql));
			tp_solver.solve(solver_parameters, time_path_solution, &sql);
			time_path_solution.save_to_db(job.region_id, sql);
		}
	}
	else if (job.maintenance_task == ns_maintenance_rebuild_movement_from_stored_images ||
		job.maintenance_task == ns_maintenance_rebuild_movement_from_stored_image_quantification) {
		if (log_output)
			image_server->register_server_event(ns_image_server_event("Loading point cloud solution from disk."), &sql);
		time_path_solution.load_from_db(job.region_id, sql, false);
	}
	else {
		unsigned long count(0);
		while (true) {
			if (log_output)
				image_server->register_server_event(ns_image_server_event("Solving (x,y,t) point cloud."), &sql);
			ns_time_path_solver_parameters solver_parameters(ns_time_path_solver_parameters::default_parameters(job.region_id, sql));

			ns_time_path_solver tp_solver;
			tp_solver.load(job.region_id, sql);
			tp_solver.solve(solver_parameters, time_path_solution, &sql);
			if (log_output)
				image_server->register_server_event(ns_image_server_event("Filling gaps and adding path prefixes."), &sql);
			std::string prefix_length_str = image_server->get_cluster_constant_value("path_prefix_length_in_frames", ns_to_string(ns_time_path_solution::default_length_of_fast_moving_prefix()), &sql);
			unsigned long prefix_length(ns_time_path_solution::default_length_of_fast_moving_prefix());
			if (prefix_length_str.length() > 0) {
				prefix_length = atol(prefix_length_str.c_str());
			}

			time_path_solution.fill_gaps_and_add_path_prefixes(prefix_length);

			//unnecissary save, done for debug
			//	time_path_solution.save_to_db(job.region_id,sql);
			if (log_output)
				image_server->register_server_event(ns_image_server_event("Caching image data for inferred worm positions."), &sql);
			ns_image_server_time_path_inferred_worm_aggregator ag;
			bool problematic_image_encountered(ag.create_images_for_solution(job.region_id, time_path_solution, sql));
			//corrupt images are marked as problematic in the db and have their worm detection deleted. Thus, we need to rebuild the 
			//solution in order to account for these deletions.
			if (problematic_image_encountered) {

				bool fixed_something = time_path_solution.remove_invalidated_points(job.region_id, solver_parameters, sql);
				if (fixed_something)
					break;

				if (count > 2)
					throw ns_ex("Multiple attempts at analysis turned up corrupt images.  They all should have been caught in the first round, so we're giving up.");
				image_server->register_server_event(ns_image_server_event("Corrupt images were found and excluded.  Attempting to re-run analysis.."), &sql);
				count++;
			}
			else break;
		}

		//ns_worm_tracking_model_parameters tracking_parameters(ns_worm_tracking_model_parameters::default_parameters());
		//ns_worm_tracker worm_tracker;
		//worm_tracker.track_moving_worms(tracking_parameters,time_path_solution);
		time_path_solution.save_to_db(job.region_id, sql);

		ns_acquire_for_scope<std::ostream> position_3d_file_output(
			image_server->results_storage.animal_position_timeseries_3d(
				results_subject, sql, ns_image_server_results_storage::ns_3d_plot
			).output()
		);
		time_path_solution.output_visualization_csv(position_3d_file_output());
		position_3d_file_output.release();

		image_server->results_storage.write_animal_position_timeseries_3d_launcher(results_subject, ns_image_server_results_storage::ns_3d_plot, sql);
		//return true;
	}


	ns_time_path_image_movement_analyzer time_path_image_analyzer;
	ns_image_server::ns_posture_analysis_model_cache::const_handle_t posture_analysis_model_handle;
	image_server->get_posture_analysis_model_for_region(job.region_id, posture_analysis_model_handle, sql);
	ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
		ns_get_death_time_estimator_from_posture_analysis_model(posture_analysis_model_handle().model_specification));

	const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(job.region_id, sql));

	if (job.maintenance_task == ns_maintenance_rebuild_movement_data) {
		//load in worm images and analyze them for non-translating animals for posture changes
		if (log_output)
			image_server->register_server_event(ns_image_server_event("Analyzing images for animal posture changes."), &sql);
		unsigned long attempt_count(0);
		while (true) {
			try {
				time_path_image_analyzer.process_raw_images(job.region_id, time_path_solution, time_series_denoising_parameters, &death_time_estimator(), sql, -1, true);
				break;
			}
			catch (ns_ex & ex) {
				if (attempt_count > 1 || !time_path_image_analyzer.try_to_rebuild_after_failure())
					throw ex;
				else attempt_count++;
				
				//if a problem is encountered loading images from disk,
				//we may be able to recover simply by excluding those images.
				//In that case, we don't want to have to rebuild all the prefixes and inferred images, which is a very slow process.
				//so, we simply remove the broken timepoints and try again.
				ns_time_path_solver_parameters solver_parameters(ns_time_path_solver_parameters::default_parameters(job.region_id, sql));

				bool something_changed = time_path_solution.remove_invalidated_points(job.region_id, solver_parameters, sql);
				if (!something_changed)
					throw ns_ex("Could not fix the error in this region: ") << ex.text();

				time_path_solution.save_to_db(job.region_id, sql);
				ns_acquire_for_scope<std::ostream> position_3d_file_output(
					image_server->results_storage.animal_position_timeseries_3d(
						results_subject, sql, ns_image_server_results_storage::ns_3d_plot
					).output()
				);
				time_path_solution.output_visualization_csv(position_3d_file_output());
				position_3d_file_output.release();

				image_server->results_storage.write_animal_position_timeseries_3d_launcher(results_subject, ns_image_server_results_storage::ns_3d_plot, sql);
			}
		}
		time_path_image_analyzer.ns_time_path_image_movement_analyzer::obtain_analysis_id_and_save_movement_data(job.region_id, sql,
																			ns_time_path_image_movement_analyzer::ns_require_existing_record,
																			ns_time_path_image_movement_analyzer::ns_write_data );
	}
	else if (job.maintenance_task == ns_maintenance_rebuild_movement_from_stored_images) {
		//load in previously calculated image quantification from disk for re-analysis
		//This is good, for example, if changes have been made to the movement quantification analyzer
		//and it needs to be rerun though the actual images don't need to be requantified.
		if (log_output)
			image_server->register_server_event(ns_image_server_event("Quantifying stored animal posture images"), &sql);
		bool reanalyze_optical_flow(false);
#ifdef NS_CALCULATE_OPTICAL_FLOW
		reanalyze_optical_flow = true;
#endif
		time_path_image_analyzer.reanalyze_stored_aligned_images(job.region_id, time_path_solution, time_series_denoising_parameters, &death_time_estimator(), sql, true, reanalyze_optical_flow);
		time_path_image_analyzer.obtain_analysis_id_and_save_movement_data(job.region_id, sql,
			ns_time_path_image_movement_analyzer::ns_require_existing_record,
			ns_time_path_image_movement_analyzer::ns_write_data);
	}
	else {
		//load in previously calculated image quantification from disk for re-analysis
		//This is good, for example, if changes have been made to the movement quantification analyzer
		//and it needs to be rerun though the actual images don't need to be requantified.
		//or if by hand annotations have been performed and the alignment in the output files should be redone.
		if (log_output)
			image_server->register_server_event(ns_image_server_event("Analyzing stored animal posture quantification."), &sql);
		time_path_image_analyzer.load_completed_analysis(job.region_id, time_path_solution, time_series_denoising_parameters, &death_time_estimator(), sql);
		time_path_image_analyzer.obtain_analysis_id_and_save_movement_data(job.region_id, sql,
			ns_time_path_image_movement_analyzer::ns_require_existing_record,
			ns_time_path_image_movement_analyzer::ns_write_data);
	}
	death_time_estimator.release();

	ns_hand_annotation_loader by_hand_region_annotations;
	ns_region_metadata metadata;
	try {
		metadata = by_hand_region_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, job.region_id, sql);
	}
	catch (ns_ex & ex) {
		image_server->register_server_event(ex, &sql);
		metadata.load_from_db(job.region_id, "", sql);
	}

	ns_death_time_annotation_set set;
	time_path_image_analyzer.produce_death_time_annotations(set);
	ns_death_time_annotation_compiler compiler;
	compiler.add(set);
	compiler.add(by_hand_region_annotations.annotations);
	std::vector<ns_ex> censoring_file_io_problems;
	ns_ex censoring_problem;
	try {
		ns_death_time_annotation_set censoring_set;
		//calculate censoring events according to different censoring strategies
		ns_death_time_annotation::ns_by_hand_annotation_integration_strategy by_hand_annotation_integration_strategy[2] =
		{ ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_death_time_annotation::ns_only_machine_annotations };

		for (unsigned int bhais = 0; bhais < 2; bhais++) {
			for (unsigned int censoring_strategy = 0; censoring_strategy < (int)ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies; censoring_strategy++) {
				if (censoring_strategy == (int)ns_death_time_annotation::ns_by_hand_censoring)
					continue;

				ns_worm_movement_summary_series summary_series;


				if (censoring_strategy == ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor) {
					summary_series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
						(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
						ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_missing_times,
						compiler, ns_include_unchanged);
					summary_series.generate_censoring_annotations(metadata, time_path_image_analyzer.db_analysis_id(),censoring_set);
					try {
						ns_image_server_results_file movement_timeseries(image_server->results_storage.movement_timeseries_data(
							by_hand_annotation_integration_strategy[bhais],
							(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
							ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_missing_times,
							ns_include_unchanged,
							results_subject, "single_region", "movement_timeseries", sql));
						ns_acquire_for_scope<std::ostream> movement_out(movement_timeseries.output());
						ns_worm_movement_measurement_summary::out_header(movement_out());
						summary_series.to_file(metadata, movement_out());
						movement_out.release();
					}
					catch (ns_ex & ex) {
						censoring_file_io_problems.push_back(ex);
					}

				}
				else {
					summary_series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
						(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
						ns_death_time_annotation::default_missing_return_strategy(),
						compiler, ns_include_unchanged);
					summary_series.generate_censoring_annotations(metadata, time_path_image_analyzer.db_analysis_id(),censoring_set);

					try {
						ns_image_server_results_file movement_timeseries(image_server->results_storage.movement_timeseries_data(by_hand_annotation_integration_strategy[bhais],
							(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
							ns_death_time_annotation::ns_censoring_minimize_missing_times,
							ns_include_unchanged,
							results_subject, "single_region", "movement_timeseries", sql));
						ns_acquire_for_scope<std::ostream> movement_out(movement_timeseries.output());
						ns_worm_movement_measurement_summary::out_header(movement_out());
						summary_series.to_file(metadata, movement_out());
						movement_out.release();
					}
					catch (ns_ex & ex) {
						censoring_file_io_problems.push_back(ex);
					}


				}
			}
			set.add(censoring_set);
		}
	}
	catch (ns_ex & ex) {
		censoring_problem = ex;
	}


	//output worm movement and death time annotations to disk

	ns_image_server_results_file censoring_results(image_server->results_storage.machine_death_times(results_subject, ns_image_server_results_storage::ns_censoring_and_movement_transitions,
		"time_path_image_analysis", sql));
	ns_image_server_results_file state_results(image_server->results_storage.machine_death_times(results_subject, ns_image_server_results_storage::ns_worm_position_annotations,
		"time_path_image_analysis", sql));

	ns_acquire_for_scope<std::ostream> censoring_out(censoring_results.output());
	ns_acquire_for_scope<std::ostream> state_out(state_results.output());
	set.write_split_file_column_format(censoring_out(), state_out());
	censoring_out.release();
	state_out.release();
	//output worm movement path summaries to disk

	//when outputting analysis files for movement detection, we can use by-hand annotations of movement cessation times to align
	//movement quantification to gold-standard events.
	//ns_region_metadata metadata;

	time_path_image_analyzer.add_by_hand_annotations(by_hand_region_annotations.annotations);

	ns_image_server_results_subject sub;
	sub.region_id = job.region_id;
	try {
		ns_acquire_for_scope<std::ostream> o(image_server->results_storage.time_path_image_analysis_quantification(sub, "detailed", false, sql,false,false).output());
		if (time_path_image_analyzer.size() > 0) {
			time_path_image_analyzer.group(0).paths[0].write_detailed_movement_quantification_analysis_header(o());
			o() << "\n";
			time_path_image_analyzer.write_detailed_movement_quantification_analysis_data(metadata, o(), false);
		}
		else o() << "(No Paths in Solution)\n";

		o() << "\n";
		o.release();
	}
	catch (ns_ex & ex) {
		image_server->register_server_event(ex, &sql);
	}

	if (0) {
		//generate optimization training set 
		std:: vector<double> thresholds(25);
		//log scale between .1 and .0001
		for (unsigned int i = 0; i < 25; i++)
			thresholds[i] = pow(10, -1 - (3.0*i / 25));
		std::vector<unsigned long> hold_times(12);
		hold_times[0] = 0;
		hold_times[1] = 60 * 15;
		for (unsigned int i = 0; i < 10; i++)
			hold_times[i + 2] = i * 60 * 60;

		ns_acquire_for_scope < std:: ostream > o2(image_server->results_storage.time_path_image_analysis_quantification(sub, "optimization_stats", false, sql).output());
		ns_analyzed_image_time_path::write_posture_analysis_optimization_data_header(o2());
		o2() << "\n";
		ns_parameter_optimization_results res(thresholds.size(),hold_times.size());
		time_path_image_analyzer.write_posture_analysis_optimization_data(2,thresholds, hold_times, metadata, o2(),res);
	}

	//update db stats
	sql << "UPDATE sample_region_image_info SET "
		<< "latest_movement_rebuild_timestamp = UNIX_TIMESTAMP(NOW()),"
		<< "last_timepoint_in_latest_movement_rebuild = " << time_path_image_analyzer.last_timepoint_in_analysis() << ","
		<< "number_of_timepoints_in_latest_movement_rebuild = " << time_path_image_analyzer.number_of_timepoints_in_analysis()
		<< " WHERE id = " << job.region_id;
	sql.send_query();
	if (!censoring_file_io_problems.empty()) {
		ns_ex ex("One or more file i/o issues encountered during analysis:");
		for (unsigned long i = 0; i < censoring_file_io_problems.size(); i++)
			ex << "(1)" << censoring_file_io_problems[i].text();
		image_server->register_server_event(ex, &sql);
	}
	if (!censoring_problem.text().empty())
		throw ns_ex("A problem occurred when trying to generate censoring information: ") << censoring_problem.text() << ". Deaths were estimated but no censoring data written";

	//	image_server->performance_statistics.register_job_duration(ns_process_movement_paths_visualization,tm.stop());

}

void ns_refine_image_statistics(const ns_64_bit region_id, std::ostream & out,ns_sql & sql) {

	ns_time_path_solution solution;
	solution.load_from_db(region_id, sql, false);

	ns_time_path_image_movement_analyzer analyzer;
	ns_image_server::ns_posture_analysis_model_cache::const_handle_t posture_analysis_model_handle;
	image_server.get_posture_analysis_model_for_region(region_id, posture_analysis_model_handle, sql);

	ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
		ns_get_death_time_estimator_from_posture_analysis_model(posture_analysis_model_handle().model_specification));

	const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id, sql));

	analyzer.load_completed_analysis(region_id, solution, time_series_denoising_parameters, &death_time_estimator(), sql, true);

	ns_region_metadata metadata;	
	ns_hand_annotation_loader by_hand_region_annotations;
	metadata = by_hand_region_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, region_id, sql);

	analyzer.add_by_hand_annotations(by_hand_region_annotations.annotations);
	ns_detected_worm_stats::output_csv_header(out);
	out << ",";
	metadata.out_JMP_plate_identity_header_short(out);
	out << ",time spent included, time_spent_excluded,average_time,excluded by visual inspection, overlap with path match,Age (days), Stationary Worm ID, Movement State\n";



	sql << "SELECT id, worm_detection_results_id, capture_time FROM sample_region_images WHERE region_info_id = " << region_id << " AND worm_detection_results_id != 0 ORDER BY capture_time";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.empty())
		throw ns_ex("Could not find any valid timepoints in region ") << region_id;
	int r(-5);
	for (unsigned int i = 0; i < res.size(); i++) {
		int r1 = (100 * i) / res.size();
		if (r1 - r >= 5) {
			image_server.add_subtext_to_current_event(ns_to_string(r1) + "%...", &sql);
			r = r1;
		}
		try {

			unsigned long t = atol(res[i][2].c_str());

			ns_image_worm_detection_results results;
			results.detection_results_id = ns_atoi64(res[i][1].c_str());
			results.load_from_db(true, false, sql, false);
			ns_image_server_captured_image_region region;
			region.load_from_db(ns_atoi64(res[i][0].c_str()), &sql);
			results.load_images_from_db(region, sql, false, false);
			const std::vector<const ns_detected_worm_info *> worms = results.actual_worm_list();
			std::vector<ns_region_area> areas(worms.size());
			for (unsigned int j = 0; j < worms.size(); j++) {
				areas[j].pos = worms[j]->region_position_in_source_image;
				areas[j].size = worms[j]->region_size;
				areas[j].time = t;
			}

			//we need to do this so that the analyzer object can generate estimates of the movement state of objects over time
			const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id, sql));
			ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
				ns_get_death_time_estimator_from_posture_analysis_model(posture_analysis_model_handle().model_specification));
			analyzer.reanalyze_with_different_movement_estimator(time_series_denoising_parameters, &death_time_estimator());
			
			ns_object_hand_annotation_data hd;
			analyzer.guess_if_region_is_excluded_by_hand(areas);
			for (unsigned int j = 0; j < worms.size(); j++) {
				ns_detected_worm_stats worm_stats = worms[j]->generate_stats();
				worm_stats.output_csv_data(region_id, t, areas[j].pos, areas[j].size, hd, out);
				out << ",";
				metadata.out_JMP_plate_identity_data_short(out);
				out << "," << areas[j].total_inclusion_time_in_seconds << "," << areas[j].total_exclusion_time_in_seconds << "," << areas[j].average_annotation_time_for_region << ",";
				if (areas[j].total_exclusion_time_in_seconds == 0 || areas[j].total_exclusion_time_in_seconds < areas[j].total_inclusion_time_in_seconds)
					out << "no";
				else out << "yes";
				out << "," << areas[j].overlap_area_with_match << ",";
				out << ((t - metadata.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24) << ",";
				out << areas[j].worm_id << "," << ns_movement_state_to_string_short(areas[j].movement_state);
				out << "\n";
			}
		}
		catch (ns_ex & ex) {
			image_server.add_subtext_to_current_event(ex.text(), &sql);
		}
	}
}




struct ns_inferred_worm_aggregator_shared_state {
	ns_inferred_worm_aggregator_shared_state(std::map<unsigned long, ns_image_server_time_path_inferred_worm_aggregator_image_info> & region_images_by_time_, ns_sql & sql_) :needs_to_be_rebuilt(false), region_images_by_time(&region_images_by_time_),sql(&sql_), sql_lock("iwa_lock"), remove_list_lock("iwa_rlock") {}
	bool needs_to_be_rebuilt;
	ns_sql * sql;
	ns_lock sql_lock;
	ns_lock remove_list_lock;
	std::vector<ns_time_path_timepoint *> timepoints_to_remove;
	const std::map<unsigned long, ns_image_server_time_path_inferred_worm_aggregator_image_info> * region_images_by_time;
};



struct ns_inferred_worm_aggregator_thread_pool_persistant_data {
	ns_inferred_worm_aggregator_thread_pool_persistant_data() {
		unprocessed_image.use_more_memory_to_avoid_reallocations();
		spatial_image.use_more_memory_to_avoid_reallocations();
		thresholded_image.use_more_memory_to_avoid_reallocations();
	}

	ns_image_standard unprocessed_image,
		spatial_image,
		thresholded_image;
	ns_image_worm_detection_results results;
	ns_worm_collage_storage::ns_collage_image_pool collage_image_pool;
	std::vector<ns_time_path_element *> inferred_elements;
};


void ns_process_inferred_worms_for_timepoint(ns_time_path_timepoint * timepoint, ns_inferred_worm_aggregator_thread_pool_persistant_data & persistant_data, ns_inferred_worm_aggregator_shared_state & shared_state);

struct ns_inferred_worm_aggregator_thread_pool_job {
	ns_inferred_worm_aggregator_thread_pool_job() :timepoint(0), shared_state(0) {}
	ns_inferred_worm_aggregator_thread_pool_job(ns_time_path_timepoint * timepoint_, ns_inferred_worm_aggregator_shared_state * shared_state_) :timepoint(timepoint_), shared_state(shared_state_) {}

	ns_time_path_timepoint * timepoint;
	ns_inferred_worm_aggregator_shared_state * shared_state;

	void operator()(ns_inferred_worm_aggregator_thread_pool_persistant_data & persistant_data) {
		ns_process_inferred_worms_for_timepoint(timepoint, persistant_data, *shared_state);
	}
};

void throw_pool_errors(
	ns_thread_pool<ns_inferred_worm_aggregator_thread_pool_job,
	ns_inferred_worm_aggregator_thread_pool_persistant_data> & thread_pool, ns_sql & sql) {

	ns_inferred_worm_aggregator_thread_pool_job job;
	ns_ex ex;
	bool found_error(false);
	while (true) {
		long errors = thread_pool.get_next_error(job, ex);
		if (errors == 0)
			break;
		found_error = true;
		//register all but the last error
		if (errors > 1) image_server_const.add_subtext_to_current_event(ns_image_server_event(ex.text()) << "\n", &sql);
	}
	//throw the last error
	if (found_error)
		throw ex;
}


bool ns_image_server_time_path_inferred_worm_aggregator::create_images_for_solution(const ns_64_bit region_info_id, ns_time_path_solution & s, ns_sql & sql) {
	std::vector<ns_time_path_element *> inferred_elements;
	std::map<unsigned long, ns_image_server_time_path_inferred_worm_aggregator_image_info> region_images_by_time;
	{
		sql << "SELECT capture_time,id,"
			<< ns_processing_step_db_column_name(ns_unprocessed) << ","
			<< ns_processing_step_db_column_name(ns_process_spatial) << ","
			<< ns_processing_step_db_column_name(ns_process_threshold) << ","
			<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
			<< ", problem, censored, worm_detection_results_id,worm_interpolation_results_id FROM sample_region_images WHERE region_info_id = "
			<< region_info_id;

		ns_sql_result res;
		sql.get_rows(res);
		for (unsigned int i = 0; i < res.size(); i++) {
			ns_image_server_time_path_inferred_worm_aggregator_image_info in;
			in.time = atol(res[i][0].c_str());
			in.id = ns_atoi64(res[i][1].c_str());
			in.raw_image_id = ns_atoi64(res[i][2].c_str());
			in.spatial_id = ns_atoi64(res[i][3].c_str());
			in.threshold_id = ns_atoi64(res[i][4].c_str());
			in.region_interpolated_id = ns_atoi64(res[i][5].c_str());
			in.problem = ns_atoi64(res[i][6].c_str()) > 0;
			in.censored = ns_atoi64(res[i][7].c_str()) > 0;
			in.worm_detection_results_id = ns_atoi64(res[i][8].c_str());
			in.worm_interpolated_results_id = ns_atoi64(res[i][9].c_str());
			std::map<unsigned long, ns_image_server_time_path_inferred_worm_aggregator_image_info>::iterator p;
			p = region_images_by_time.find(in.time);
			if (p == region_images_by_time.end())
				region_images_by_time[in.time] = in;
			else {
				if (in.id == p->second.id)
					throw ns_ex("A duplicate sample_region_image **with the same region_id** was found!");
				//there's a duplicate sample_region_image at this time point.
				p->second.duplicates_of_this_time_point.push_back(in);
			}
		}
	}


	ns_inferred_worm_aggregator_shared_state shared_state(region_images_by_time,sql);

	ns_thread_pool<ns_inferred_worm_aggregator_thread_pool_job,
		ns_inferred_worm_aggregator_thread_pool_persistant_data> thread_pool;
	const unsigned long number_of_threads(image_server_const.maximum_number_of_processing_threads());
	thread_pool.set_number_of_threads(image_server_const.maximum_number_of_processing_threads());
	thread_pool.prepare_pool_to_run();


	//std::cerr << "Caching images for path gaps and prefixes...";
	long last_r(-5);

	ns_image_worm_detection_results results;
	ns_worm_collage_storage::ns_collage_image_pool collage_image_pool;
	for (unsigned int t = 0; t < s.timepoints.size(); t += number_of_threads) {
		long cur_r = (100 * t) / s.timepoints.size();
		if (cur_r - last_r >= 5) {
			image_server_const.add_subtext_to_current_event(ns_to_string(cur_r) + "%...", &sql);
			last_r = cur_r;
		}
		unsigned long num_to_run = number_of_threads;
		if (t + num_to_run >= s.timepoints.size())
			num_to_run = s.timepoints.size() - t - 1;

		for (unsigned int i = 0; i < num_to_run; i++)
			thread_pool.add_job_while_pool_is_not_running(
				ns_inferred_worm_aggregator_thread_pool_job(&s.timepoints[t + i], &shared_state));

		thread_pool.run_pool();
		thread_pool.wait_for_all_threads_to_become_idle();
		throw_pool_errors(thread_pool, sql);


	}
	thread_pool.shutdown();

	image_server_const.add_subtext_to_current_event("\n", &sql);

	for (unsigned int i = 0; i < shared_state.timepoints_to_remove.size(); i++) {

		//if there's an error, flag it, remove the problematic inferred animals from the solution, and remove any existing data from the db.
		ns_time_path_timepoint * timepoint(shared_state.timepoints_to_remove[i]);
		std::map<unsigned long, ns_image_server_time_path_inferred_worm_aggregator_image_info>::const_iterator current_region_image(shared_state.region_images_by_time->find(timepoint->time));
		unsigned int t = 0;
		for (; t < s.timepoints.size(); t++)
			if (timepoint == &s.timepoints[t])
				break;
		if (t == s.timepoints.size())
			throw ns_ex("Could not find timepoint to remove!");

		s.remove_inferred_animal_locations(t, true);

		current_region_image->second.remove_inferred_element_from_db(*shared_state.sql);
		for (unsigned int i = 0; i < current_region_image->second.duplicates_of_this_time_point.size(); i++)
			current_region_image->second.duplicates_of_this_time_point[i].remove_inferred_element_from_db(*shared_state.sql);
	}
	return shared_state.needs_to_be_rebuilt;
}

void ns_process_inferred_worms_for_timepoint(ns_time_path_timepoint * timepoint, ns_inferred_worm_aggregator_thread_pool_persistant_data & persistant_data, ns_inferred_worm_aggregator_shared_state & shared_state) {

	persistant_data.inferred_elements.resize(0);
	for (unsigned int i = 0; i < timepoint->elements.size(); i++)
		if (timepoint->elements[i].inferred_animal_location)
			persistant_data.inferred_elements.push_back(&timepoint->elements[i]);

	std::map<unsigned long, ns_image_server_time_path_inferred_worm_aggregator_image_info>::const_iterator current_region_image(shared_state.region_images_by_time->find(timepoint->time));

	if (current_region_image == shared_state.region_images_by_time->end())
		throw ns_ex("Could not find timepoint ") << timepoint->time << " in the database";

	ns_acquire_lock_for_scope lock(shared_state.sql_lock, __FILE__, __LINE__);
	if (persistant_data.inferred_elements.empty()) {
		current_region_image->second.remove_inferred_element_from_db(*shared_state.sql);
		for (unsigned int i = 0; i < current_region_image->second.duplicates_of_this_time_point.size(); i++) {
			image_server_const.add_subtext_to_current_event("Removing duplicate timepoint", shared_state.sql);
			current_region_image->second.duplicates_of_this_time_point[i].remove_inferred_element_from_db(*shared_state.sql);
		}
		lock.release();
		return;
	}
	lock.release();
	//load images
	ns_image_server_image im;
	ns_ex ex;
	bool unprocessed_problem(false), processed_problem(false);
	try {
		lock.get(__FILE__, __LINE__);
		im.load_from_db(current_region_image->second.raw_image_id, shared_state.sql);
		ns_image_storage_source_handle<ns_image_storage_handler::ns_component> h(image_server.image_storage.request_from_storage(im, shared_state.sql));
		lock.release();
		h.input_stream().pump(persistant_data.unprocessed_image, 1024);
	}
	catch (ns_ex & ex_) {
		ex = ex_;
		unprocessed_problem = true;
	}
	try {
		lock.get(__FILE__, __LINE__);
		im.load_from_db(current_region_image->second.spatial_id, shared_state.sql);
		ns_image_storage_source_handle<ns_image_storage_handler::ns_component> h(image_server.image_storage.request_from_storage(im, shared_state.sql));
		im.load_from_db(current_region_image->second.threshold_id, shared_state.sql);
		ns_image_storage_source_handle<ns_image_storage_handler::ns_component> h2(image_server.image_storage.request_from_storage(im, shared_state.sql));
		lock.release();
		h.input_stream().pump(persistant_data.spatial_image, 1024);
		h2.input_stream().pump(persistant_data.thresholded_image, 1024);

	}
	catch (ns_ex & ex_) {
		processed_problem = true;
		ex = ex_;
	}
	if (unprocessed_problem || processed_problem) {

		lock.get(__FILE__, __LINE__);
		ns_64_bit error_id = image_server.register_server_event(ns_ex("Skipping and flagging a corrupt or missing image:") << ex.text(),shared_state.sql);
		shared_state.needs_to_be_rebuilt = true;
		if (unprocessed_problem) {
			*shared_state.sql << "UPDATE sample_region_images SET problem=" << error_id << " WHERE id = " << current_region_image->second.id;
			shared_state.sql->send_query();
		}
		if (processed_problem) {
			*shared_state.sql << "UPDATE sample_region_images SET " << ns_processing_step_db_column_name(ns_process_spatial) << " = 0,"
				<< ns_processing_step_db_column_name(ns_process_threshold) << " = 0,"
				<< ns_processing_step_db_column_name(ns_process_lossy_stretch) << " = 0,"
				<< ns_processing_step_db_column_name(ns_process_worm_detection) << " = 0,"
				<< ns_processing_step_db_column_name(ns_process_region_vis) << " = 0,"
				<< ns_processing_step_db_column_name(ns_process_worm_detection_labels) << " = 0,"
				<< ns_processing_step_db_column_name(ns_process_interpolated_vis) << " = 0,"
				<< " worm_detection_results_id=0, worm_interpolation_results_id=0 "
				<< "WHERE id = " << current_region_image->second.id;
			shared_state.sql->send_query();
			*shared_state.sql << "DELETE FROM worm_detection_results WHERE id = " << current_region_image->second.worm_detection_results_id
				<< " OR id = " << current_region_image->second.worm_interpolated_results_id;
			shared_state.sql->send_query();
		}
		lock.release();
		ns_acquire_lock_for_scope rlock(shared_state.remove_list_lock, __FILE__, __LINE__);
		shared_state.timepoints_to_remove.push_back(timepoint);
		rlock.release();
		return;
	}
	persistant_data.results.clear(false);
	persistant_data.results.worm_collage.use_more_memory_to_avoid_reallocations(true);
	persistant_data.results.detection_results_id = 0;

	ns_image_properties prop(persistant_data.unprocessed_image.properties());
	prop.components = 1;
	std::vector<ns_detected_worm_info> & worms(persistant_data.results.replace_actual_worms_access());
	worms.resize(persistant_data.inferred_elements.size());

	//ns_font & f(font_server.default_font());
	//f.set_height(18);

	for (unsigned int i = 0; i < persistant_data.inferred_elements.size(); i++) {
		ns_detected_worm_info & worm(worms[i]);

		worm.region_size = persistant_data.inferred_elements[i]->region_size;
		worm.region_position_in_source_image = persistant_data.inferred_elements[i]->region_position;
		worm.context_image_size = persistant_data.inferred_elements[i]->context_image_size;
		worm.context_position_in_source_image = persistant_data.inferred_elements[i]->context_image_position;
		worm.movement_state = ns_movement_not_calculated;
		//			f.draw_grayscale(worm.region_position_in_source_image.x + worm.region_size.x/2,worm.region_position_in_source_image.y + worm.region_size.y/2,
		//						 255,std::string("w") + ns_to_string(t),spatial_image);

		//copy images over for new inferred detected worm
		prop.height = worm.region_size.y;
		prop.width = worm.region_size.x;
		worm.bitmap().prepare_to_recieve_image(prop);
		for (unsigned int y = 0; y < worm.region_size.y; y++)
			for (unsigned int x = 0; x < worm.region_size.x; x++)
				worm.bitmap()[y][x] = persistant_data.thresholded_image[y + worm.region_position_in_source_image.y][x + worm.region_position_in_source_image.x];
	}
	persistant_data.results.replace_actual_worms();
	const ns_image_standard & worm_collage(persistant_data.results.generate_region_collage(persistant_data.unprocessed_image, persistant_data.spatial_image, persistant_data.thresholded_image, &persistant_data.collage_image_pool));

	std::vector<ns_vector_2i> positions;
	persistant_data.results.worm_collage.info().image_locations_in_collage((unsigned long)worms.size(), positions);
	for (unsigned int i = 0; i < positions.size(); i++) {
		persistant_data.inferred_elements[i]->context_image_position_in_region_vis_image = positions[i];
	}
	lock.get( __FILE__, __LINE__);
	ns_image_server_captured_image_region reg_im;
	reg_im.load_from_db(current_region_image->second.id, shared_state.sql);
	persistant_data.results.save(reg_im, true, *shared_state.sql, false);

	bool had_to_use_volatile_storage;
	ns_image_server_image region_bitmap = reg_im.create_storage_for_processed_image(ns_process_region_interpolation_vis, ns_tiff, shared_state.sql);

	lock.release();

	unsigned long write_attempts = 0;
	while (true) {
		try {
			lock.get(__FILE__, __LINE__);
			ns_image_storage_reciever_handle<ns_8_bit> region_bitmap_o = image_server.image_storage.request_storage(
				region_bitmap,
				ns_tiff, 1.0, 1024, shared_state.sql,
				had_to_use_volatile_storage,
				false,
				false);
			lock.release();
			worm_collage.pump(region_bitmap_o.output_stream(), 1024);
			break;
		}
		catch (ns_ex & ex) {
			lock.get(__FILE__, __LINE__);
			image_server.register_server_event(ex, shared_state.sql);
			lock.release();
			if (write_attempts == 4)
				throw ex;
			write_attempts++;
		}
	}
	lock.get(__FILE__, __LINE__);
	for (unsigned int i = 0; i < current_region_image->second.duplicates_of_this_time_point.size(); i++) {
		ns_image_server_captured_image_region reg_im2;
		reg_im2.load_from_db(current_region_image->second.duplicates_of_this_time_point[i].id, shared_state.sql);
		persistant_data.results.save(reg_im2, true, *shared_state.sql, false);

		*shared_state.sql << "UPDATE " << ns_processing_step_db_table_name(ns_process_region_interpolation_vis) << " SET "
			<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
			<< " = " << region_bitmap.id << " WHERE id = " << reg_im2.region_images_id;
		shared_state.sql->send_query();
	}
	lock.release();
}