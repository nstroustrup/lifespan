#ifndef ns_animal_telemetry_h
#define ns_animal_telemetry_h
#include "ns_time_path_image_analyzer.h"
#include "ns_graph.h"

void ns_update_worm_information_bar(const std::string & status);
void ns_update_main_information_bar(const std::string & status);

#include "ns_bspline.h"

void ns_gaussian_kernel_smoother(const unsigned long time_step_resample_factor, const unsigned long kernel_width_in_fraction_of_all_points,const std::vector<ns_graph_object> & source, std::vector<ns_graph_object> & dest);
typedef enum{ns_band,ns_connect_to_bottom} ns_extrema_plot_type;
void ns_calculate_running_extrema(const ns_extrema_plot_type & plot_type, const unsigned long time_step_resample_factor, const unsigned long kernel_width_in_fraction_of_all_points, const std::vector<ns_graph_object> & source, std::vector<ns_graph_object> & dest, std::vector<double> temp1, std::vector<double> temp2);
class ns_death_time_posture_solo_annotater_region_data {
private:
	//disable copy constructor
	ns_death_time_posture_solo_annotater_region_data(const ns_death_time_posture_solo_annotater_region_data &);
public:
 ns_death_time_posture_solo_annotater_region_data(ns_time_path_image_movement_analysis_memory_pool<ns_wasteful_overallocation_resizer> & memory_pool) :movement_analyzer(memory_pool),movement_data_loaded(false), annotation_file("","", ""), loading_time(0), loaded_path_data_successfully(false), movement_quantification_data_loaded(false){}
	
	void load_movement_analysis(const ns_64_bit region_id_, ns_sql & sql, const bool load_movement_quantification = false) {
		if (!movement_data_loaded) {
			solution.load_from_db(region_id_, sql, true);
		}
		if (movement_quantification_data_loaded)
			return;

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading time series denoising parameters"));
		const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id_, sql));
		ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
		try{
		  image_server.get_posture_analysis_model_for_region(region_id_, handle, sql);
		  //cout << "Using model " << handle().name << "\n";
		  ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
													      ns_get_death_time_estimator_from_posture_analysis_model(handle().model_specification
																				      ));
		  handle.release();
		  
		  if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading completed analysis"));
		  loaded_path_data_successfully = movement_analyzer.load_completed_analysis_(region_id_, solution, time_series_denoising_parameters, &death_time_estimator(), sql, !load_movement_quantification);
		  death_time_estimator.release();
		}
		catch(ns_ex & ex){
		  if (handle.is_valid())
		    handle.release();
		  throw ns_ex("Error while loading worm movement information: ") << ex.text();

		}
	      
		movement_data_loaded = true;
		if (load_movement_quantification)
			movement_quantification_data_loaded = true;
	}
	void load_images_for_worm(const ns_stationary_path_id & path_id, const unsigned long number_of_images_to_load, ns_sql & sql,ns_simple_local_image_cache & image_cache) {
		const unsigned long current_time(ns_current_time());
		const unsigned long max_number_of_cached_worms(4);
		//don't allow more than 2 paths to be loaded (to prevent memory overflow). Delete animals that have been in the cache for longest.
		if (image_loading_times_for_groups.size() >= max_number_of_cached_worms) {

			//delete animals that are older than 5 minutes.
			const unsigned long cutoff_time(current_time - 5 * 60);
			for (ns_loading_time_cache::iterator p = image_loading_times_for_groups.begin(); p != image_loading_times_for_groups.end(); ) {
				if (p->second < cutoff_time) {
					movement_analyzer.clear_images_for_group(p->first,image_cache);
					image_loading_times_for_groups.erase(p++);
				}
				else p++;
			}
			//delete younger ones if necessary
			while (image_loading_times_for_groups.size() >= max_number_of_cached_worms) {
				ns_loading_time_cache::iterator youngest(image_loading_times_for_groups.begin());
				for (ns_loading_time_cache::iterator p = image_loading_times_for_groups.begin(); p != image_loading_times_for_groups.end(); p++) {
					if (youngest->second > p->second)
						youngest = p;
				}
				movement_analyzer.clear_images_for_group(youngest->first,image_cache);
				image_loading_times_for_groups.erase(youngest);
			}
		}
		movement_analyzer.load_images_for_group(path_id.group_id, number_of_images_to_load, sql, false, false,image_cache);
		image_loading_times_for_groups[path_id.group_id] = current_time;
	}
	void clear_images_for_worm(const ns_stationary_path_id & path_id, ns_simple_local_image_cache & image_cache) {
		movement_analyzer.clear_images_for_group(path_id.group_id,image_cache);
		ns_loading_time_cache::iterator p = image_loading_times_for_groups.find(path_id.group_id);
		if (p != image_loading_times_for_groups.end())
			image_loading_times_for_groups.erase(p);
	}

	ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer> movement_analyzer;

	std::vector<ns_animal_list_at_position> by_hand_timing_data; //organized by group.  that is movement_analyzer.group(4) is timing_data(4)
	std::vector<ns_animal_list_at_position> machine_timing_data; //organized by group.  that is movement_analyzer.group(4) is timing_data(4)
	std::vector<ns_death_time_annotation> orphaned_events;
	ns_region_metadata metadata;

	//group id,time of loading
	typedef std::map<long, unsigned long> ns_loading_time_cache;
	ns_loading_time_cache image_loading_times_for_groups;

	mutable ns_image_server_results_file annotation_file;
	unsigned long loading_time;
	bool load_annotations(ns_sql & sql, const bool load_by_hand = true) {
		orphaned_events.resize(0);
		ns_timing_data_and_death_time_annotation_matcher <std::vector<ns_animal_list_at_position> > matcher;
		bool could_load_by_hand(false);

		if (load_by_hand) {
			//load by hand annotations
			ns_acquire_for_scope<ns_istream> in(annotation_file.input());
			if (!in.is_null()) {
				ns_death_time_annotation_set set;
				set.read(ns_death_time_annotation_set::ns_all_annotations, in()());
				
				std::string error_message;
				if (matcher.load_timing_data_from_set(set, false, by_hand_timing_data, orphaned_events, error_message)) {
					if (error_message.size() != 0) {
						ns_update_worm_information_bar(error_message);
						ns_update_main_information_bar(error_message);
						could_load_by_hand = true;
					}

				}
			}
		}
		//load machine annotations
		ns_machine_analysis_data_loader machine_annotations;
		machine_annotations.load(ns_death_time_annotation_set::ns_censoring_and_movement_transitions, metadata.region_id, 0, 0, sql, true);
		if (machine_annotations.samples.size() != 0 && machine_annotations.samples.begin()->regions.size() != 0) {
			std::vector<ns_death_time_annotation> _unused_orphaned_events;
			std::string _unused_error_message;
			matcher.load_timing_data_from_set((*machine_annotations.samples.begin()->regions.begin())->death_time_annotation_set, true,
				machine_timing_data, _unused_orphaned_events, _unused_error_message);
			return true;
		}



		return could_load_by_hand;
	}
	void add_annotations_to_set(ns_death_time_annotation_set & set, std::vector<ns_death_time_annotation> & orphaned_events) {
		ns_death_time_annotation_set t_set;
		std::vector<ns_death_time_annotation> t_orphaned_events;
		ns_timing_data_and_death_time_annotation_matcher<std::vector<ns_death_timing_data> > matcher;
		for (unsigned int i = 0; i < by_hand_timing_data.size(); i++)
			matcher.save_death_timing_data_to_set(by_hand_timing_data[i].animals, t_orphaned_events, t_set, false);
		orphaned_events.insert(orphaned_events.end(), t_orphaned_events.begin(), t_orphaned_events.end());
		set.add(t_set);
	}

	void save_annotations(const ns_death_time_annotation_set & extra_annotations) const {
		throw ns_ex("The solo posture annotations should be mixed in with storyboard annotations!");
		ns_death_time_annotation_set set;


		ns_acquire_for_scope<ns_ostream> out(annotation_file.output());
		if (out.is_null())
			throw ns_ex("Could not open output file.");
		set.write(out()());
		out.release();
		//	ns_update_information_bar(ns_to_string(set.events.size()) + " events saved at " + ns_format_time_string_for_human(ns_current_time()));
		//	saved_=true;
	};
	bool movement_quantification_data_is_loaded() { return movement_quantification_data_loaded; }
	bool loaded_path_data_successfully;
private:
	ns_time_path_solution solution;
	bool movement_data_loaded,
		movement_quantification_data_loaded;
};

class ns_animal_telemetry {
public:
	typedef enum { ns_none, ns_movement, ns_movement_intensity, ns_movement_intensity_slope_1x, ns_movement_intensity_slope_2x, ns_movement_intensity_slope_4x, ns_all,ns_number_of_graph_types } ns_graph_contents;
	static std::string graph_type_string(const ns_graph_contents c) {
		switch (c) {
		case ns_none: return "no data";
		case ns_movement: return "movement scores (white)";
		case ns_movement_intensity: "movement scores (white),total intensity (blue)";
		case ns_movement_intensity_slope_1x: return "movement scores (white), total intensity (blue), change in total intensity (green)";
		case ns_movement_intensity_slope_2x: return "movement scores (white), total intensity (blue), change in total intensity (green)";
		case ns_movement_intensity_slope_4x: return "movement scores (white), total intensity (blue), change in total intensity (green)";
		case ns_all: return "all data";
		default: throw ns_ex("ns_animal_telemetry::graph_type_string()::Unknown graph type");
		}
	}
	void get_current_time_limits(unsigned long & start, unsigned long & stop) {
		if (region_data == 0)
			throw ns_ex("get_current_time_limits::No region data loaded");
		ns_analyzed_image_time_path *path(&region_data->movement_analyzer.group(group_id).paths[0]);
		start = path->element(0).absolute_time;
		stop = path->element(path->element_count() - 1).absolute_time;
	}
private:
	bool _show;
	ns_death_time_posture_solo_annotater_region_data *region_data;
	unsigned long group_id;
	ns_image_standard base_graph_top, base_graph_bottom;
	ns_graph graph_top, graph_bottom;
	ns_graph_specifics graph_top_specifics, graph_bottom_specifics;
	vector<ns_graph_object> movement_vals, smoothed_movement_vals, size_vals, smoothed_size_vals,slope_vals;
	vector <double> time_axis;
	vector<long> segment_ids; //mapping from the current time in time_axis to the specicif segment (element of movement_vals, smoothed_movement_vals, etc)
	vector<long> segment_offsets; //mapping from the current time in time_axis to the specicif segment (element of movement_vals, smoothed_movement_vals, etc)
	ns_posture_analysis_model posture_analysis_model;
	std::vector<double> temp1, temp2;
	
	unsigned long number_of_valid_elements, first_element;
	
	void draw_base_graph(const ns_graph_contents & graph_contents, const long marker_resize_factor, unsigned long start_time = 0, unsigned long stop_time = UINT_MAX) {

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing telemetry base graph."));
		if (graph_contents == ns_none)
			return;
		graph_top.clear();
		graph_bottom.clear();
		ns_analyzed_image_time_path *path(&region_data->movement_analyzer.group(group_id).paths[0]);
		if (path->element_count() < 1)
			throw ns_ex("Time series is too short");


		//calculate the average time step duration, properly handling excluded time steps
		ns_64_bit time_step(0);
		unsigned long time_step_count(0);
		unsigned long last_valid_time(0);
		unsigned long last_valid_element = 0;
		unsigned long first_valid_element = path->element_count();
		for (unsigned int i = 0; i < path->element_count(); i++) {
			if (path->element(i).absolute_time > stop_time)
				break;
			if (!path->element(i).excluded) {
				if (last_valid_time > 0) {
					time_step += (path->element(i).absolute_time - last_valid_time);
					time_step_count++;
				}
				if (i < first_valid_element && path->element(i).absolute_time >= start_time) {
					first_valid_element = i;
				}
				last_valid_time = path->element(i).absolute_time;
				last_valid_element = i;
			}
		}
		if (last_valid_element == 0 || first_valid_element == path->element_count())
			throw ns_ex("No data to plot");
		unsigned long number_of_valid_elements_ = last_valid_element - first_valid_element +1;
		number_of_valid_elements = number_of_valid_elements_;
		first_element = first_valid_element;
		time_axis.resize(number_of_valid_elements_);
		segment_ids.resize(number_of_valid_elements_, -1);

		const unsigned long max_time_step_interval_to_connect_with_lines(4 * time_step / time_step_count);

		//count how many connected line segments we'll need to draw.
		long number_of_separate_segments(1);
		last_valid_time = 0;
		for (unsigned int i = first_valid_element; i <= last_valid_element; i++) {
			if (path->element(i).excluded)
				continue;
			unsigned long current_time_step = (path->element(i).absolute_time - last_valid_time);
			bool step_was_too_long(last_valid_time > 0 && current_time_step > max_time_step_interval_to_connect_with_lines);
			if (step_was_too_long)
				number_of_separate_segments++;
			segment_ids[i-first_valid_element] = number_of_separate_segments - 1;
			last_valid_time = path->element(i).absolute_time;
		}
		segment_offsets.resize(number_of_separate_segments);
		if (segment_offsets.size() > 0)
			segment_offsets[0] = 0;
		unsigned long cur_seg_id(0);
		for (unsigned int i = first_valid_element; i <= last_valid_element; i++) {
			if (path->element(i).excluded)
				continue;
			if (segment_ids[i - first_valid_element] != cur_seg_id) {
				cur_seg_id++;
				if (cur_seg_id >= segment_offsets.size())
					throw ns_ex("Encountered an invalid segment id");
				segment_offsets[cur_seg_id] = i - first_valid_element;
			}
		}

		movement_vals.resize(number_of_separate_segments, ns_graph_object::ns_graph_dependant_variable);
		slope_vals.resize(number_of_separate_segments, ns_graph_object::ns_graph_dependant_variable);
		size_vals.resize(number_of_separate_segments, ns_graph_object::ns_graph_dependant_variable);
		for (unsigned int i = 0; i < number_of_separate_segments; i++) {
			movement_vals[i].x.resize(0);
			movement_vals[i].x.reserve(number_of_valid_elements_);
			movement_vals[i].y.resize(0);
			movement_vals[i].y.reserve(number_of_valid_elements_);

			slope_vals[i].x.resize(0);
			slope_vals[i].x.reserve(number_of_valid_elements_);
			slope_vals[i].y.resize(0);
			slope_vals[i].y.reserve(number_of_valid_elements_);
			size_vals[i].x.resize(0);
			size_vals[i].x.reserve(number_of_valid_elements_);
			size_vals[i].y.resize(0);
			size_vals[i].y.reserve(number_of_valid_elements_);
		}

		float min_score(FLT_MAX), max_score(-FLT_MAX);
		float min_intensity(FLT_MAX), max_intensity(-FLT_MAX);
		ns_s64_bit min_intensity_slope(INT32_MAX),  //we want to include a zero slope
			max_intensity_slope(-INT32_MAX);

		float min_raw_score, max_raw_score, second_smallest_raw_score;
		float min_time(FLT_MAX), max_time(-FLT_MAX);

		std::vector<double> scores;
		scores.reserve(number_of_valid_elements_);

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Calculating telemetry scores."));
		//find lowest movement score.
		for (unsigned int i = first_valid_element; i <= last_valid_element; i++) {
			if (path->element(i).excluded)
				continue;
			const double d(path->element(i).measurements.death_time_posture_analysis_measure_v2_uncropped());
			scores.push_back(d);
		}
		//add in threshold
		///if thresholding model!
		//scores.push_back(posture_analysis_model.threshold_parameters.stationary_cutoff);

		std::sort(scores.begin(), scores.end());
		min_raw_score = scores[scores.size() / 50];
		//we now want to find the second smallest score in the data set.  
		//All data points whose value is equal to the smallest data point will be set to this value (as they are not allowed to be zero on log scale)
		second_smallest_raw_score = min_raw_score *= 1.01; //default
		for (int i = scores.size() / 50; i < scores.size(); i++) {
			if (scores[i] > min_raw_score) {
				second_smallest_raw_score = scores[i];
				break;
			}
		}
		max_raw_score = scores[scores.size() - scores.size() / 50 - 1];

		
		//make sure the axis minimum and maximum are not equal.
		if (second_smallest_raw_score == max_raw_score || min_raw_score == max_raw_score) {
			max_raw_score += .01;
			second_smallest_raw_score = max_raw_score * .01;
			min_raw_score = max_raw_score * .001;
		}


		//now reuse scores memory for another purpose--storing normalized movement scores.
		scores.resize(number_of_valid_elements_);
		//calculate normalized movement scores and find their min and max
		for (unsigned int i = first_valid_element; i < last_valid_element; i++) {
			if (path->element(i).excluded)
				continue;
			double d(path->element(i).measurements.death_time_posture_analysis_measure_v2_uncropped());
			if (d >= max_raw_score) d = log(max_raw_score - min_raw_score);
			else if (d <= min_raw_score)
				d = log(second_smallest_raw_score - min_raw_score); //can't be zero before we take the log
			else d = log(d - min_raw_score);

			scores[i-first_valid_element] = d;

			if (d < min_score) min_score = d;
			if (d > max_score) max_score = d;
		}
		if (max_score == min_score)
			max_score += .01;
		//find min max of other statistics

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Calculating extrema."));
		for (unsigned int i = first_valid_element; i <= last_valid_element; i++) {
			if (path->element(i).excluded)
				continue;
			const double t(path->element(i).relative_time);
			if (t < min_time) min_time = t;
			if (t > max_time) max_time = t;
			const double n(path->element(i).measurements.total_intensity_within_stabilized);
			double s;
			switch (graph_contents){
				case ns_movement_intensity_slope_1x: s = path->element(i).measurements.change_in_total_stabilized_intensity_1x; break;
				case ns_movement_intensity_slope_2x: s = path->element(i).measurements.change_in_total_stabilized_intensity_2x; break;
				case ns_movement_intensity_slope_4x: s = path->element(i).measurements.change_in_total_stabilized_intensity_4x; break;
				default: s = 0;
			}
			//cout << t / (60.0 * 60 * 24) << "," << s << "\n";
			if (i < path->first_stationary_timepoint())
				continue;
			if (n < min_intensity) min_intensity = n;
			if (n > max_intensity) max_intensity = n;
			if (s < min_intensity_slope) min_intensity_slope = s;
			if (s > max_intensity_slope) max_intensity_slope = s;
		}
		//cout << "\n";

		ns_s64_bit intensity_slope_largest = (max_intensity_slope > -min_intensity_slope) ? max_intensity_slope: -min_intensity_slope;

		///xxx only for threshold!
		double threshold = 0;
		/*if (posture_analysis_model.threshold_parameters.stationary_cutoff >= max_raw_score)
			threshold = log(max_raw_score - min_raw_score);
		else if (posture_analysis_model.threshold_parameters.stationary_cutoff <= min_raw_score)
			threshold = log(second_smallest_raw_score - min_raw_score);
		else threshold = log(posture_analysis_model.threshold_parameters.stationary_cutoff - min_raw_score);
		*/

		double min_rounded_time(DBL_MAX), max_rounded_time(DBL_MIN);

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Normalizing scores."));
		//calculate normalized scores and break up into independantly plotted segments
		for (unsigned int i = first_valid_element; i <= last_valid_element; i++) {
			const long & current_segment = segment_ids[i-first_valid_element];
			const double time = path->element(i).relative_time / 60.0 / 60 / 24;
			if (time < min_rounded_time) min_rounded_time = time;
			if (time > max_rounded_time) max_rounded_time = time;
			time_axis[i- first_valid_element] = time;
			if (path->element(i).excluded || segment_ids[i-first_valid_element] == -1)
				continue;
			//scale denoised movement score to a value between 0 and 1
			//which on log scale becomes 0 and 1

			movement_vals[current_segment].x.push_back(time);
			movement_vals[current_segment].y.push_back((scores[i-first_valid_element] - min_score) / (max_score - min_score));
			if (*movement_vals[current_segment].y.rbegin() < 0) *movement_vals[current_segment].y.rbegin() = 0;
			if (*movement_vals[current_segment].y.rbegin() > 1) *movement_vals[current_segment].y.rbegin() = 1;

			//scale intensity to a value between 0 and 1.
			size_vals[current_segment].x.push_back(time);
			size_vals[current_segment].y.push_back((path->element(i).measurements.total_intensity_within_stabilized - min_intensity) / (max_intensity - min_intensity));
			if (*size_vals[current_segment].y.rbegin() < 0) *size_vals[current_segment].y.rbegin() = 0;
			if (*size_vals[current_segment].y.rbegin() > 1) *size_vals[current_segment].y.rbegin() = 1;

			//scale slope so that a slope of 0 is at .5 and the max devation from zero is either at -1 or 1
			slope_vals[current_segment].x.push_back(time);
			double s;
			switch (graph_contents) {
				case ns_movement_intensity_slope_1x: s = path->element(i).measurements.change_in_total_stabilized_intensity_1x; break;
				case ns_movement_intensity_slope_2x: s = path->element(i).measurements.change_in_total_stabilized_intensity_2x; break;
				case ns_movement_intensity_slope_4x: s = path->element(i).measurements.change_in_total_stabilized_intensity_4x; break;
				default: s = 0;
			}
			if (intensity_slope_largest != 0)
			slope_vals[current_segment].y.push_back(.5*(s  / (float)(intensity_slope_largest)+1));
			else slope_vals[current_segment].y.push_back(0);
			if (*slope_vals[current_segment].y.rbegin() < 0) *slope_vals[current_segment].y.rbegin() = 0;
			if (*slope_vals[current_segment].y.rbegin() > 1) *slope_vals[current_segment].y.rbegin() = 1;
		}

		ns_graph_object threshold_object(ns_graph_object::ns_graph_horizontal_line);
		double th((threshold - min_score) / (max_score - min_score));
		if (th < 0) th = 0;
		if (th > 1) th = 1;
		threshold_object.y.push_back(th);


		double zero_slope_value = .5;

		ns_graph_object zero_slope_object(ns_graph_object::ns_graph_horizontal_line);
		zero_slope_object.y.push_back(zero_slope_value);




		const unsigned long resample_factor(4);
		const unsigned long kernel_absolute_width(5);

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Calculate running extrema"));
		ns_calculate_running_extrema(ns_connect_to_bottom, resample_factor, kernel_absolute_width, movement_vals, smoothed_movement_vals, temp1, temp2);
		ns_calculate_running_extrema(ns_band,resample_factor, kernel_absolute_width, size_vals, smoothed_size_vals, temp1, temp2);
		for (unsigned int i = 0; i < smoothed_movement_vals.size(); i++){
			for (unsigned int j = 0; j < smoothed_movement_vals[i].y.size(); j++) {
				if (smoothed_movement_vals[i].y[j] > 1)
					smoothed_movement_vals[i].y[j] = 1;
				if (smoothed_movement_vals[i].y[j] < 0)
					smoothed_movement_vals[i].y[j] = 0;
			}
			for (unsigned int j = 0; j < smoothed_movement_vals[i].y_min.size(); j++) {
				if (smoothed_movement_vals[i].y_min[j] < 0)
					smoothed_movement_vals[i].y_min[j] = 0;
				if (smoothed_movement_vals[i].y_min[j] > 1)
					smoothed_movement_vals[i].y_min[j] = 1;
			}
		}	
		for (unsigned int i = 0; i < smoothed_size_vals.size(); i++) {
			for (unsigned int j = 0; j < smoothed_size_vals[i].y.size(); j++) {
				if (smoothed_size_vals[i].y[j] > 1)
					smoothed_size_vals[i].y[j] = 1;
				if (smoothed_size_vals[i].y[j] < 0)
					smoothed_size_vals[i].y[j] = 0;
			}
			for (unsigned int j = 0; j < smoothed_size_vals[i].y_min.size(); j++) {
				if (smoothed_size_vals[i].y_min[j] > 1)
					smoothed_size_vals[i].y_min[j] = 1;
				if (smoothed_size_vals[i].y_min[j] < 0)
					smoothed_size_vals[i].y_min[j] = 0;
			}
		}

		threshold_object.properties.line.color = ns_color_8(150, 150, 150);
		threshold_object.properties.line.draw = true;

		zero_slope_object.properties.line.color = ns_color_8(150, 150, 150);
		zero_slope_object.properties.line.draw = true;
		const unsigned long desired_screen_marker_size(1);
		unsigned long marker_size = desired_screen_marker_size /marker_resize_factor;
		if (marker_size <desired_screen_marker_size)
			marker_size = desired_screen_marker_size* marker_resize_factor;

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Building graph objects"));
		for (unsigned int i = 0; i < number_of_separate_segments; i++) {

			/*smoothed_movement_vals[i].properties.line.draw = size_vals[i].properties.line.draw = true;
			smoothed_movement_vals[i].properties.line.width = size_vals[i].properties.line.width = 1;
			smoothed_movement_vals[i].properties.point.draw = size_vals[i].properties.point.draw = false;
			smoothed_size_vals[i].properties = smoothed_movement_vals[i].properties;

			smoothed_movement_vals[i].properties.line.color = ns_color_8(200, 200, 200);
			smoothed_size_vals[i].properties.line.color = ns_color_8(100, 100, 225);
			*/
			smoothed_movement_vals[i].properties.fill_between_y_and_ymin = true;
			smoothed_movement_vals[i].properties.area_fill.draw = true;
			smoothed_movement_vals[i].properties.area_fill.opacity = .5;
			smoothed_movement_vals[i].properties.line.draw = false;
			smoothed_movement_vals[i].properties.point.draw = false;
			smoothed_size_vals[i].properties = smoothed_movement_vals[i].properties;
			smoothed_movement_vals[i].properties.area_fill.color = ns_color_8(100, 100, 100);
			smoothed_size_vals[i].properties.area_fill.color = ns_color_8(75, 75, 150);

			movement_vals[i].properties.line.draw = false;
			movement_vals[i].properties.point.draw = true;
			size_vals[i].properties = movement_vals[i].properties;

			movement_vals[i].properties.point.color = ns_color_8(225, 225, 225);
			//movement_vals[i].properties.point.edge_color = ns_color_8(200, 200, 200);
			movement_vals[i].properties.point.width = marker_size;
			movement_vals[i].properties.point.edge_width = 0;

			size_vals[i].properties.point.color = ns_color_8(125, 125, 255);
			//size_vals[i].properties.point.edge_color = ns_color_8(125, 125, 255);
			size_vals[i].properties.point.width = marker_size;
			size_vals[i].properties.point.edge_width = 0;

			slope_vals[i].properties.point.draw = true;
			slope_vals[i].properties.line.draw = true;
			slope_vals[i].properties.line.color = ns_color_8(150, 250, 200);
			slope_vals[i].properties.point.color = ns_color_8(150, 250, 200);

			switch (graph_contents) {	//deliberate read-through between plots
				case ns_all:
				case ns_movement_intensity_slope_1x:
				case ns_movement_intensity_slope_2x:
				case ns_movement_intensity_slope_4x:
				case ns_movement_intensity:
					graph_bottom.add_reference(&smoothed_size_vals[i]);
				case ns_movement:
					graph_top.add_reference(&smoothed_movement_vals[i]);
				}
			switch (graph_contents) {	//deliberate read-through between plots
				case ns_all:
				case ns_movement_intensity_slope_1x:
				case ns_movement_intensity_slope_2x:
				case ns_movement_intensity_slope_4x:
					graph_bottom.add_reference(&slope_vals[i]);
				case ns_movement_intensity:
					graph_bottom.add_reference(&size_vals[i]);
				case ns_movement:	
					graph_top.add_reference(&movement_vals[i]);
			}
		}
		if (graph_contents == ns_movement_intensity_slope_1x ||
			graph_contents == ns_movement_intensity_slope_2x ||
			graph_contents == ns_movement_intensity_slope_4x ||
			graph_contents == ns_all)
			graph_bottom.add_reference(&zero_slope_object);
	
		graph_top.add_reference(&threshold_object);

		graph_top.x_axis_properties.line.color = graph_top.y_axis_properties.line.color = ns_color_8(255, 255, 255);
		graph_top.x_axis_properties.text.color = graph_top.y_axis_properties.text.color = ns_color_8(255, 255, 255);
		graph_top.x_axis_properties.text_size = graph_top.y_axis_properties.text_size = 10;
		graph_top.area_properties.area_fill.color = ns_color_8(0, 0, 0);

		ns_graph_axes axes;
		axes.boundary(0) = min_rounded_time;
		axes.boundary(1) = max_rounded_time;
		axes.boundary(2) = 0;
		axes.boundary(3) = 1;
		axes.tick(2) = .25;
		axes.tick(3) = .125;
		if (max_rounded_time - min_rounded_time > 10) {
			axes.tick(1) = floor((max_rounded_time - min_rounded_time)/10.0);
			axes.tick(0) = axes.tick(1) * 2;
		}
		else if (max_rounded_time - min_rounded_time < 10) {
			axes.tick(0) = 1;
			axes.tick(1) = .5;
		}
		else if (max_rounded_time - min_rounded_time > 5) {
			axes.tick(0) = 1;
			axes.tick(1) = .25;
		}
		else if (max_rounded_time - min_rounded_time > 2) {
			axes.tick(0) = .5;
			axes.tick(1) = .1;
		}
		else  {
			axes.tick(0) = .25;
			axes.tick(1) = .125;
		}
		for (int i = 0; i < 3; i++)
		axes.tick_interval_specified(i) = true;

		ns_color_8 gray(50, 50, 50);
		graph_top.x_axis_properties.line.width = 
			graph_top.y_axis_properties.line.width = 1;
		graph_top.x_axis_properties.line.color =
			graph_top.y_axis_properties.line.color = gray;
		graph_top.x_axis_properties.text_size *= 2;
		graph_top.y_axis_properties.text_size *= 2;

		graph_bottom.x_axis_properties = graph_top.x_axis_properties;
		graph_bottom.y_axis_properties = graph_top.y_axis_properties;
		graph_bottom.area_properties = graph_top.area_properties;


		graph_top.set_graph_display_options("", axes, base_graph_top.properties().width/(float)base_graph_top.properties().height);
		graph_bottom.set_graph_display_options("", axes, base_graph_bottom.properties().width / (float)base_graph_bottom.properties().height);

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Rendering movement graph"));
		graph_top_specifics = graph_top.draw(base_graph_top);
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Rendering size graph"));
		graph_bottom_specifics = graph_bottom.draw(base_graph_bottom);
	}
	inline void map_value_from_top_graph_onto_image(const float &x, const float &y, unsigned long & x1, unsigned long & y1) {
		x1 = graph_top_specifics.boundary_bottom_and_left.x + (unsigned int)(graph_top_specifics.dx*(x - graph_top_specifics.axes.boundary(0) + graph_top_specifics.axes.axis_offset(0)));
		y1 = base_graph_top.properties().height - graph_top_specifics.boundary_bottom_and_left.y - (unsigned int)(graph_top_specifics.dy*(y - graph_top_specifics.axes.boundary(2) + graph_top_specifics.axes.axis_offset(1)));
	}

	inline void map_value_from_bottom_graph_onto_image(const float &x, const float &y, unsigned long & x1, unsigned long & y1) {
		x1 = graph_bottom_specifics.boundary_bottom_and_left.x + (unsigned int)(graph_bottom_specifics.dx*(x - graph_bottom_specifics.axes.boundary(0) + graph_bottom_specifics.axes.axis_offset(0)));
		y1 = base_graph_top.properties().height + border().y + base_graph_bottom.properties().height - graph_bottom_specifics.boundary_bottom_and_left.y - (unsigned int)(graph_bottom_specifics.dy*(y - graph_bottom_specifics.axes.boundary(2) + graph_bottom_specifics.axes.axis_offset(1)));
	}
	inline unsigned long map_pixel_from_image_onto_buffer(const unsigned long &x, const unsigned long &y, const ns_vector_2i &position, const ns_vector_2i &buffer_size) {
		return 3 * ((buffer_size.y - y - position.y-1)*buffer_size.x + x + position.x);
	}
	ns_vector_2i border() const { return ns_vector_2i(25, 25); }
	void overlay_metadata(const ns_animal_telemetry::ns_graph_contents graph_contents,const unsigned long current_element, const ns_vector_2i & position, const ns_vector_2i & buffer_size, const long marker_resize_factor,ns_8_bit * buffer) {
		unsigned long x_score, y_score, x_size, y_size, x_slope, y_slope;
		long segment_id = segment_ids[current_element];
		if (segment_id == -1)
			return;
		if (segment_id > movement_vals.size())
			throw ns_ex("Invalid segment id!");
		long segment_element_id = current_element - segment_offsets[segment_id];
		if (segment_element_id >= movement_vals[segment_id].y.size())
			throw ns_ex("Out of element id");
		map_value_from_top_graph_onto_image(time_axis[current_element], movement_vals[segment_id].y[segment_element_id], x_score, y_score);
		map_value_from_bottom_graph_onto_image(time_axis[current_element], size_vals[segment_id].y[segment_element_id], x_size, y_size);
		map_value_from_bottom_graph_onto_image(time_axis[current_element], slope_vals[segment_id].y[segment_element_id], x_slope, y_slope);
		for (int y = -2* marker_resize_factor; y <= 2 * marker_resize_factor; y++)
			for (int x = -2 * marker_resize_factor; x <= 2 * marker_resize_factor; x++) {
				unsigned long p(map_pixel_from_image_onto_buffer(x_score+x+border().x, y_score+y+ border().y, position, buffer_size));
				buffer[p] = 255;
				buffer[p+1] = 0;
				buffer[p+2] = 0;
				if (graph_contents == ns_animal_telemetry::ns_all || graph_contents == ns_animal_telemetry::ns_movement_intensity || graph_contents == ns_animal_telemetry::ns_movement_intensity_slope_1x
					|| graph_contents == ns_animal_telemetry::ns_movement_intensity_slope_2x
					|| graph_contents == ns_animal_telemetry::ns_movement_intensity_slope_4x) {
					p = map_pixel_from_image_onto_buffer(x_size + x + border().x, y_size + y + border().y, position, buffer_size);
					buffer[p] = 255;
					buffer[p + 1] = 0;
					buffer[p + 2] = 0;
				}
				if (graph_contents == ns_animal_telemetry::ns_movement_intensity_slope_1x
					|| graph_contents == ns_animal_telemetry::ns_movement_intensity_slope_2x
					|| graph_contents == ns_animal_telemetry::ns_movement_intensity_slope_4x) {
					p = map_pixel_from_image_onto_buffer(x_slope + x + border().x, y_slope + y + border().y, position, buffer_size);
					buffer[p] = 255;
					buffer[p + 1] = 0;
					buffer[p + 2] = 0;
				}
			}

	}
	ns_graph_contents last_graph_contents;
	unsigned long last_start_time, last_stop_time;
	float last_rescale_factor;
public:
	void clear() {
		_show = false;
		region_data = 0;
		group_id = 0;
		base_graph_top.clear();
		base_graph_bottom.clear();
		graph_top.clear();
		graph_bottom.clear();
		last_graph_contents = ns_none;
		movement_vals.clear();
		smoothed_movement_vals.clear();
		size_vals.clear();
		slope_vals.clear();
		time_axis.clear();
		last_start_time = 0;
		last_stop_time = 0;
	}
	unsigned long get_graph_time_from_graph_position(const float x) { //x is in relative time
		ns_analyzed_image_time_path *path(&region_data->movement_analyzer.group(group_id).paths[0]);
		long min_i(0);
		float min_dt(100000);
		unsigned long x_seconds = x * 60 * 60 * 24 + path->element(0).absolute_time;
		for (unsigned int i = 0; i < path->element_count(); i++) {
			const bool g(path->element(i).absolute_time >= x_seconds);
			const float dt = (path->element(i).absolute_time >= x_seconds) ? (path->element(i).absolute_time - x_seconds) : (x_seconds - path->element(i).absolute_time);
			if (dt < min_dt) {
				min_dt = dt;
				min_i = i;
			}
			if (g) break;
		}
		return path->element(min_i).absolute_time;
		//unsigned long dT(path->element(first_element+number_of_valid_elements-1).absolute_time - path->element(first_element).absolute_time);
		//float dt((path->element(first_element+number_of_valid_elements-1).relative_time - path->element(first_element).relative_time) / 60.0 / 60.0 / 24.0);
		//return ((x - path->element(first_element).relative_time / 60.0 / 60.0 / 24.0) / dt)*dT + path->element(first_element).absolute_time;

	}
	//currently does not return a correct y value.
	ns_vector_2d get_graph_value_from_click_position_(const unsigned long &x, const unsigned long & y) const{
		ns_vector_2d res;
		res.x = ((long)x - (long)graph_top_specifics.boundary_bottom_and_left.x - (long)border().x) / (graph_top_specifics.dx) + graph_top_specifics.axes.boundary(0) - graph_top_specifics.axes.axis_offset(0);

		//cout << x << " " << res.x << "\n";
		//y = base_graph.properties().height - graph_specifics.boundary.y - (unsigned int)(graph_specifics.dy*(y - graph_specifics.axes.boundary(2) + graph_specifics.axes.axis_offset(1)));
		res.y = 0;// -((long)((long)y - (long)base_graph.properties().height + graph_specifics.boundary.y + border().y) / graph_specifics.dy + graph_specifics.axes.boundary(2) - graph_specifics.axes.axis_offset(1));
		return res;
	}
	
	ns_animal_telemetry() :_show(false), last_graph_contents(ns_none), region_data(0), group_id(0){}
	void set_current_animal(const unsigned int & group_id_, ns_posture_analysis_model & mod,ns_death_time_posture_solo_annotater_region_data * region_data_) {
		group_id = group_id_;
		region_data = region_data_;
		base_graph_top.init(ns_image_properties(0, 0, 3));
		base_graph_bottom.init(ns_image_properties(0, 0, 3));
		posture_analysis_model = mod;
	}
	void draw(const ns_graph_contents graph_contents, const unsigned long element_id, const ns_vector_2i & position, const ns_vector_2i & graph_size, const ns_vector_2i & buffer_size, const float marker_resize_factor, ns_8_bit * buffer, const unsigned long start_time=0, const unsigned long stop_time=UINT_MAX) {
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Starting to draw telemetry."));
		if (base_graph_top.properties().height == 0 || graph_contents != last_graph_contents || last_start_time != start_time || last_stop_time != stop_time || last_rescale_factor != marker_resize_factor) {
			base_graph_top.use_more_memory_to_avoid_reallocations();
			base_graph_bottom.use_more_memory_to_avoid_reallocations();
			ns_image_properties prop;
			prop.components = 3;
			prop.width = graph_size.x- 3*border().x;
			prop.height = graph_size.y/2 - 3*border().y;


			base_graph_top.init(prop);
			base_graph_bottom.init(prop);
			try {

				draw_base_graph(graph_contents, marker_resize_factor, start_time,stop_time);
				last_graph_contents = graph_contents;
				last_start_time = start_time;
				last_stop_time = stop_time;
				last_rescale_factor = marker_resize_factor;
			}
			catch (...) {
				base_graph_top.init(ns_image_properties(0, 0, 3));
				base_graph_bottom.init(ns_image_properties(0, 0, 3));
				throw;
			}
		}

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing everything"));
		//top margin
		for (unsigned int y = 0; y < border().y; y++)
			for (unsigned int x = 0; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++) 
					buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;
		//TOP GRAPH
		for (unsigned int y = 0; y < base_graph_top.properties().height; y++) {
			//left margin
			for (unsigned int x = 0; x < border().x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + border().y, position, buffer_size) + c] = 0;
			//graph
			for (unsigned int x = 0; x < base_graph_top.properties().width; x++) {
				for (unsigned int c = 0; c < 3; c++) 
					buffer[map_pixel_from_image_onto_buffer(x + border().x, y + border().y, position, buffer_size) + c] = base_graph_top[y][3 * x + c];
			}

			//right margin
			for (unsigned int x = base_graph_top.properties().width + border().x; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + border().y, position, buffer_size) + c] = 0;
		}
		//middle margin
		for (unsigned int y = border().y + base_graph_bottom.properties().height; y < 2*border().y + base_graph_bottom.properties().height; y++)
			for (unsigned int x = 0; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;
		const long bottom_graph_y_offset(2 * border().y + base_graph_bottom.properties().height);

		//BOTTOM GRAPH
		for (unsigned int y = 0; y < base_graph_bottom.properties().height; y++) {
			//left margin
			for (unsigned int x = 0; x < border().x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + bottom_graph_y_offset, position, buffer_size) + c] = 0;
			//graph
			for (unsigned int x = 0; x < base_graph_bottom.properties().width; x++) {
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x + border().x, y + bottom_graph_y_offset, position, buffer_size) + c] = base_graph_bottom[y][3 * x + c];
			}

			//right margin
			for (unsigned int x = base_graph_bottom.properties().width + border().x; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + bottom_graph_y_offset, position, buffer_size) + c] = 0;
		}
		//top margin
		for (unsigned int y = bottom_graph_y_offset + base_graph_bottom.properties().height; y < graph_size.y; y++)
			for (unsigned int x = 0; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;

		overlay_metadata(graph_contents, element_id - first_element, position, buffer_size, marker_resize_factor, buffer);
	}
	void show(bool s) { _show = s; }
	bool show() const { return _show;  }
};

#endif
