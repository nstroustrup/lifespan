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
			bool solution_needs_to_be_rebuilt(ag.create_images_for_solution(job.region_id, time_path_solution, sql));
			//corrupt images are marked as problematic in the db and have their worm detection deleted. Thus, we need to rebuild the 
			//solution in order to account for these deletions.
			if (solution_needs_to_be_rebuilt) {
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

		time_path_image_analyzer.process_raw_images(job.region_id, time_path_solution, time_series_denoising_parameters, &death_time_estimator(), sql, -1, true);
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
		std::vector<double> hold_times(12);
		hold_times[0] = 0;
		hold_times[1] = 60 * 15;
		for (unsigned int i = 0; i < 10; i++)
			hold_times[i + 2] = i * 60 * 60;

		ns_acquire_for_scope < std:: ostream > o2(image_server->results_storage.time_path_image_analysis_quantification(sub, "optimization_stats", false, sql).output());
		ns_analyzed_image_time_path::write_analysis_optimization_data_header(o2());
		o2() << "\n";
		time_path_image_analyzer.write_analysis_optimization_data(2,thresholds, hold_times, metadata, o2());
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