#pragma once

//Data describing worms is processed in a variety of different ways and stored in several different formats
//This class groups together all image data and by hand annotations describing moving and stationary worms
//and organizes it together in a single object.

#include "ns_time_path_image_analyzer.h"

struct ns_worm_morphology_data_integrator_timepoint {
	ns_worm_morphology_data_integrator_timepoint() :time(0), region(0), worm_image_info(0), solution_path_info(0), solution_element(0), analyzed_image_time_path(0), analyzed_image_time_path_element(0),results(0), interpolated_results(0),image_data_identified(false){}
	unsigned long time;
	bool image_data_identified;
	ns_stationary_path_id id; //each worm that becomes stationationary is given an ID  

	const ns_death_time_annotation & by_hand_annotation()const { return solution_element->volatile_by_hand_annotated_properties; }
	//loads images for *all worms in the current timepoint*/
	void load_images_for_all_worms_at_current_timepoint(ns_sql & sql) {
		if (region == 0) throw ns_ex("Images have not yet been matched up.");
		if (results == 0) throw ns_ex("Results have not yet been loaded");
		results->load_images_from_db(*region, sql, false, false);
		if (interpolated_results != 0 && interpolated_results->detection_results_id != 0)
			interpolated_results->load_images_from_db(*region, sql, true, false);

	}
	void clear_images_for_all_worms_at_current_timepoint() {
		if (region == 0) throw ns_ex("Images have not yet been matched up."); if (results == 0) throw ns_ex("Results have not yet been loaded");
		results->clear_images();
		if (interpolated_results != 0 && interpolated_results->detection_results_id != 0)
			interpolated_results->clear_images();
	}
	ns_movement_state movement_state() const {
		if (id.group_id == -1)
			return ns_movement_fast;
		if (analyzed_image_time_path == 0)
			throw ns_ex("Encountered an unspecified image time path!");
		return analyzed_image_time_path->best_guess_movement_state(this->time);
	}

	//holds the image data for the worm
	const ns_detected_worm_info * worm_image_info;

	//holds information about the region image in which the worm was detected
	ns_image_server_captured_image_region * region;
	//holds information about the worm detection process that detected the worm
	ns_image_worm_detection_results * results;
	ns_image_worm_detection_results * interpolated_results;

	//holds information about the stationary worm detection process for all worms on the plate
	ns_time_path * solution_path_info;
	//holds information about this particular worm within the stationary worm detection process
	ns_time_path_element * solution_element;

	//holds information about the posture analysis of the current worm
	ns_analyzed_image_time_path * analyzed_image_time_path;
	//holds information about the posture analysis of the current worm at the current time
	ns_analyzed_image_time_path_element * analyzed_image_time_path_element;


	//contains diagnostic data from the worm image matching process
	ns_region_area region_area;

};
template<class allocator_T>
class ns_worm_morphology_data_integrator {
public:
	ns_worm_morphology_data_integrator(ns_time_path_image_movement_analysis_memory_pool<allocator_T> & memory_pool) :analyzer(memory_pool) {}
	std::vector<std::vector<ns_worm_morphology_data_integrator_timepoint> > timepoints;
	void load_data_and_annotations_from_db(const ns_64_bit & region_id, ns_sql & sql) {

		//first load in all the basic data
		region_info_id = region_id;
		solution.load_from_db(region_id, sql, false);

		/*
		ns_image_server::ns_posture_analysis_model_cache::const_handle_t posture_analysis_model_handle;
		image_server.get_posture_analysis_model_for_region(region_id, posture_analysis_model_handle, sql);
		ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
			ns_get_death_time_estimator_from_posture_analysis_model(posture_analysis_model_handle().model_specification));
		const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id, sql));
		*/
		analyzer.load_completed_analysis(region_id, solution,  sql, true);

		timepoints.resize(solution.timepoints.size());
		for (unsigned int i = 0; i < solution.timepoints.size(); i++) {
			timepoints[i].resize(solution.timepoints[i].elements.size());
			for (unsigned int j = 0; j < timepoints[i].size(); j++) {
				timepoints[i][j].solution_element = &solution.timepoints[i].elements[j];
				timepoints[i][j].time = solution.timepoints[i].time;
			}
		}
		for (unsigned int i = 0; i < solution.path_groups.size(); i++) {
			for (unsigned int j = 0; j < solution.path_groups[i].path_ids.size(); j++) {
				if (!solution.paths[solution.path_groups[i].path_ids[j]].moving_elements.empty())
					throw ns_ex("Path has moving elements for which handling is not implemented");


				if (i >= analyzer.groups.size() || j >= analyzer.groups[i].paths.size())
					throw ns_ex("Mismatch between time path solution and time path image analyzer!");
				if (solution.paths[solution.path_groups[i].path_ids[j]].stationary_elements.size() != analyzer.groups[i].paths[j].element_count())
					throw ns_ex("Mismatch in number of elements in time_path_solution and time_path_image_analyzer");

				for (unsigned int k = 0; k < solution.paths[solution.path_groups[i].path_ids[j]].stationary_elements.size(); k++) {
					const ns_time_element_link & link(solution.paths[solution.path_groups[i].path_ids[j]].stationary_elements[k]);
					timepoints[link.t_id][link.index].id.group_id = i + 1;
					timepoints[link.t_id][link.index].id.path_id = solution.path_groups[i].path_ids[j];
					timepoints[link.t_id][link.index].solution_path_info = &solution.paths[solution.path_groups[i].path_ids[j]];
					
					timepoints[link.t_id][link.index].analyzed_image_time_path = &analyzer.groups[i].paths[j];
					timepoints[link.t_id][link.index].analyzed_image_time_path_element = &analyzer.groups[i].paths[j].elements[k];

				}
			}
		}


		//now load in all the by-hand annotations.
		//all of the by-hand annotations are managed by the time_path_image_analyzer object.  So we need to load that in to get all the by-hand annotations

		ns_hand_annotation_loader by_hand_region_annotations;

		metadata = by_hand_region_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, region_id, sql);

		analyzer.add_by_hand_annotations(by_hand_region_annotations.annotations);

		//move the by hand annotations back to annotate each element on the time path solution
		//and output the time path solution visualiaztion complete with by hand annotations
		analyzer.back_port_by_hand_annotations_to_solution_elements(solution);

		//posture_analysis_model_handle.release();
	}
	void match_images_with_solution(ns_sql & sql) {

		sql << "SELECT id, worm_detection_results_id,worm_interpolation_results_id, capture_time FROM sample_region_images WHERE region_info_id = " << region_info_id << " AND worm_detection_results_id != 0 ORDER BY capture_time";
		ns_sql_result res;
		sql.get_rows(res);
		if (res.empty())
			throw ns_ex("Could not find any valid timepoints in region ") << region_info_id;
		int r(-5);
		worm_detection_results.resize(res.size());
		interpolated_worm_detection_results.resize(res.size());
		region_image_records.resize(res.size());
		if (res.size() != timepoints.size())
			throw ns_ex("Cannot load image data for all time path solution timepoints!");

		for (unsigned int i = 0; i < res.size(); i++) {
			int r1 = (100 * i) / res.size();
			if (r1 - r >= 5) {
				image_server.add_subtext_to_current_event(ns_to_string(r1) + "%...", &sql);
				r = r1;
			}

			unsigned long t = atol(res[i][3].c_str());

			ns_image_server_captured_image_region * region = &region_image_records[i];
			region->load_from_db(ns_atoi64(res[i][0].c_str()), &sql);


			ns_image_worm_detection_results & results = worm_detection_results[i];
			results.detection_results_id = ns_atoi64(res[i][1].c_str());
			results.load_from_db(true, false, sql, false);

			ns_image_worm_detection_results & interpolated_results = interpolated_worm_detection_results[i];
			interpolated_results.detection_results_id = ns_atoi64(res[i][2].c_str());
			if (interpolated_results.detection_results_id != 0)
				interpolated_results.load_from_db(true, true, sql, false);

			const std::vector<const ns_detected_worm_info *> real_worms = results.actual_worm_list();
			std::vector<const ns_detected_worm_info *> all_worms;
			all_worms.insert(all_worms.begin(),real_worms.begin(), real_worms.end());
			if (interpolated_results.detection_results_id != 0) {
				const std::vector<const ns_detected_worm_info *> interpolated_worms = interpolated_results.actual_worm_list();
				all_worms.insert(all_worms.end(), interpolated_worms.begin(), interpolated_worms.end());
			}

			std::vector<ns_region_area> areas(all_worms.size());
			for (unsigned int j = 0; j < all_worms.size(); j++) {
				areas[j].pos = all_worms[j]->context_position_in_source_image;
				areas[j].size = all_worms[j]->context_image_size;
				areas[j].time = t;
			}
			ns_image_server::ns_posture_analysis_model_cache::const_handle_t posture_analysis_model_handle;
			image_server.get_posture_analysis_model_for_region(region_info_id, posture_analysis_model_handle, sql);

			//we need to do this so that the analyzer object can generate estimates of the movement state of objects over time
			const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_info_id, sql));
			ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
				ns_get_death_time_estimator_from_posture_analysis_model(posture_analysis_model_handle().model_specification));
			analyzer.reanalyze_with_different_movement_estimator(time_series_denoising_parameters, &death_time_estimator());

			ns_object_hand_annotation_data hd;
			analyzer.match_plate_areas_to_paths(areas);
			//make path id lookup table

			//look for subregion info using time path solution
			unsigned long t1;
			for (t1 = 0; t1 < solution.timepoints.size(); t1++) {
				if (solution.timepoints[t1].time == t)
					break;
			}
			if (t1 == solution.timepoints.size())
				throw ns_ex("Could not match worm detection event ") << t << " with a timepoint in the time path solution!";

			if (t1 < solution.timepoints.size()) {
				for (unsigned int i = 0; i < areas.size(); i++) {
					bool match_found(false);
					unsigned long pos;
					double min_dist(DBL_MAX), min_size_diff(DBL_MAX);
					for (pos = 0; pos < solution.timepoints[t1].elements.size(); pos++) {
						const double dist = (solution.timepoints[t1].elements[pos].region_position - areas[i].pos).mag();
						const double size_diff = fabs(solution.timepoints[t1].elements[pos].region_size.mag() - areas[i].size.mag());
						if (dist < min_dist)
							min_dist = dist; 
						if (size_diff < min_size_diff)
							min_size_diff = size_diff;
						if (solution.timepoints[t1].elements[pos].context_image_position == areas[i].pos &&
							solution.timepoints[t1].elements[pos].context_image_size == areas[i].size) {
							areas[i].plate_subregion_info = solution.timepoints[t1].elements[pos].subregion_info;
							areas[i].explicitly_by_hand_excluded = solution.timepoints[t1].elements[pos].volatile_by_hand_annotated_properties.is_excluded();
							timepoints[t1][pos].region_area = areas[i];
							timepoints[t1][pos].worm_image_info = all_worms[i];
							timepoints[t1][pos].region = region;
							timepoints[t1][pos].results = &results;
							timepoints[t1][pos].interpolated_results = &interpolated_results;
							timepoints[t1][pos].image_data_identified = true;
							match_found = true;
							break;
						}
					}
					if (!match_found)
						std::cerr << "Could not match worm area. Min dist = " << min_dist << "; min size diff = " << min_size_diff << "\n";
				}
			}
		}
	}
	

	ns_time_path_solution solution;
	std::vector<ns_image_worm_detection_results> worm_detection_results, interpolated_worm_detection_results;
	std::vector<ns_image_server_captured_image_region> region_image_records;


	ns_time_path_image_movement_analyzer<allocator_T> analyzer;

	ns_region_metadata metadata;

	ns_64_bit region_info_id;
}; 
