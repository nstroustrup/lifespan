#ifndef ns_animal_telemetry_h
#define ns_animal_telemetry_h
#include "ns_time_path_image_analyzer.h"
#include "ns_graph.h"


struct ns_animal_list_at_position {
	ns_stationary_path_id stationary_path_id;
	typedef std::vector<ns_death_timing_data> ns_animal_list;  //positions can hold multiple animals
	ns_animal_list animals;
};

class ns_death_time_posture_solo_annotater_region_data {
public:
 ns_death_time_posture_solo_annotater_region_data() :movement_data_loaded(false), annotation_file("","", ""), loading_time(0), loaded_path_data_successfully(false), movement_quantification_data_loaded(false){}
	
	void load_movement_analysis(const unsigned long region_id_, ns_sql & sql, const bool load_movement_quantification = false) {
		if (!movement_data_loaded) {
			solution.load_from_db(region_id_, sql, true);
		}
		if (movement_quantification_data_loaded)
			return;
		const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(region_id_, sql));
		ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
		image_server.get_posture_analysis_model_for_region(region_id_, handle, sql);
		ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
			ns_get_death_time_estimator_from_posture_analysis_model(handle().model_specification
			));
		handle.release();
		loaded_path_data_successfully = movement_analyzer.load_completed_analysis(region_id_, solution, time_series_denoising_parameters, &death_time_estimator(), sql, !load_movement_quantification);
		death_time_estimator.release();
		movement_data_loaded = true;
		if (load_movement_quantification)
			movement_quantification_data_loaded = true;
	}
	void load_images_for_worm(const ns_stationary_path_id & path_id, const unsigned long number_of_images_to_load, ns_sql & sql) {
		const unsigned long current_time(ns_current_time());
		const unsigned long max_number_of_cached_worms(2);
		//don't allow more than 2 paths to be loaded (to prevent memory overflow). Delete animals that have been in the cache for longest.
		if (image_loading_times_for_groups.size() >= max_number_of_cached_worms) {

			//delete animals that are older than 5 minutes.
			const unsigned long cutoff_time(current_time - 5 * 60);
			for (ns_loading_time_cache::iterator p = image_loading_times_for_groups.begin(); p != image_loading_times_for_groups.end(); ) {
				if (p->second < cutoff_time) {
					movement_analyzer.clear_images_for_group(p->first);
					image_loading_times_for_groups.erase(p++);
				}
				else p++;
			}
			//delete younger ones if necissary
			while (image_loading_times_for_groups.size() >= max_number_of_cached_worms) {
				ns_loading_time_cache::iterator youngest(image_loading_times_for_groups.begin());
				for (ns_loading_time_cache::iterator p = image_loading_times_for_groups.begin(); p != image_loading_times_for_groups.end(); p++) {
					if (youngest->second > p->second)
						youngest = p;
				}
				movement_analyzer.clear_images_for_group(youngest->first);
				image_loading_times_for_groups.erase(youngest);
			}
		}
		movement_analyzer.load_images_for_group(path_id.group_id, number_of_images_to_load, sql, false, false);
		image_loading_times_for_groups[path_id.group_id] = current_time;
	}
	void clear_images_for_worm(const ns_stationary_path_id & path_id) {
		movement_analyzer.clear_images_for_group(path_id.group_id);
		ns_loading_time_cache::iterator p = image_loading_times_for_groups.find(path_id.group_id);
		if (p != image_loading_times_for_groups.end())
			image_loading_times_for_groups.erase(p);
	}

	ns_time_path_image_movement_analyzer movement_analyzer;

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
			ns_acquire_for_scope<std::istream> in(annotation_file.input());
			if (!in.is_null()) {
				ns_death_time_annotation_set set;
				set.read(ns_death_time_annotation_set::ns_all_annotations, in());

				std::string error_message;
				if (matcher.load_timing_data_from_set(set, false, by_hand_timing_data, orphaned_events, error_message)) {
					if (error_message.size() != 0) {
						ns_update_information_bar(error_message);
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
			matcher.load_timing_data_from_set(machine_annotations.samples.begin()->regions.begin()->death_time_annotation_set, true,
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


		ns_acquire_for_scope<std::ostream> out(annotation_file.output());
		if (out.is_null())
			throw ns_ex("Could not open output file.");
		set.write(out());
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
	bool _show;
	ns_death_time_posture_solo_annotater_region_data *region_data;
	unsigned long group_id;
	ns_image_standard base_graph;
	ns_graph graph;
	ns_graph_specifics graph_specifics;
	ns_graph_object movement_vals, smoothed_movement_vals,size_vals, time_axes;
	ns_posture_analysis_model posture_analysis_model;
	void draw_base_graph() {
		graph.clear();
		ns_analyzed_image_time_path *path(&region_data->movement_analyzer.group(group_id).paths[0]);
		if (path->element_count() < 1)
			throw ns_ex("Time series is too short");
		movement_vals.y.resize(path->element_count());
		time_axes.x.resize(movement_vals.y.size());
		size_vals.y.resize(movement_vals.y.size());
		float min_score(FLT_MAX), max_score(-FLT_MAX);
		float min_intensity(FLT_MAX), max_intensity(-FLT_MAX);
		float min_time(FLT_MAX), max_time(-FLT_MAX);
		std::vector<float> scores(movement_vals.y.size());
		//find lowest movement score.
		for (unsigned int i = 0; i < movement_vals.y.size(); i++)
			scores[i] = path->element(i).measurements.death_time_posture_analysis_measure_v2();
		std::sort(scores.begin(), scores.end());
		float min_raw_score = scores[scores.size() / 20];
		float second_min_raw_score = min_raw_score *= 1.01; //default
		for (int i = scores.size() / 20 + 1; i < scores.size(); i++) {
			if (scores[i] > min_raw_score) {
				second_min_raw_score = scores[i];
				break;
			}
		}
		float max_raw_score = scores[scores.size()-scores.size() / 20 -1];
	
		for (unsigned int i = 0; i < movement_vals.y.size(); i++) {
			double d(path->element(i).measurements.death_time_posture_analysis_measure_v2());
			if (d >= max_raw_score) d = log(max_raw_score - min_raw_score);
			else if (d <= min_raw_score)
				d = log(second_min_raw_score - min_raw_score); //can't be zero before we take the log
			else d = log(d- min_raw_score);
			

			movement_vals.y[i] = d;
			double n(path->element(i).measurements.total_intensity_within_worm_area);
			double t(path->element(i).relative_time);
			if (d < min_score) min_score = d;
			if (d > max_score) max_score = d;
			if (n < min_intensity) min_intensity = n;
			if (n > max_intensity) max_intensity = n;
			if (t < min_time) min_time = t;
			if (t > max_time) max_time = t;
		}
		double threshold;
		if (posture_analysis_model.threshold_parameters.stationary_cutoff >= max_raw_score)
			threshold = max_raw_score - min_raw_score;
		else if (posture_analysis_model.threshold_parameters.stationary_cutoff <= min_raw_score)
			threshold = second_min_raw_score - min_raw_score;
		else threshold = posture_analysis_model.threshold_parameters.stationary_cutoff - min_raw_score;
		threshold = log(threshold);
		if (min_score > threshold)
			min_score = threshold;
		if (max_score < threshold)
			max_score = threshold;
		for (unsigned int i = 0; i < movement_vals.y.size(); i++) {
			time_axes.x[i] = floor(path->element(i).relative_time / 6.0 / 60 / 24) / 10;
			//scale denoised movement score to a value between 0 and 1
			//which on log scale becomes 0 and 1
			
			movement_vals.y[i] = (movement_vals.y[i] - min_score) / (max_score - min_score);
			if (movement_vals.y[i] < 0) movement_vals.y[i] = 0;
			if (movement_vals.y[i] > 1) movement_vals.y[i] = 1;

			//scale intensity to a value between 0 and 1.
			size_vals.y[i] = (path->element(i).measurements.total_intensity_within_worm_area - min_intensity) / (max_intensity-min_intensity) ;
			if (size_vals.y[i] < 0) size_vals.y[i] = 0;
			if (size_vals.y[i] > 1) size_vals.y[i] = 1;
		}

		ns_graph_object threshold_object(ns_graph_object::ns_graph_horizontal_line);
		double th((threshold- min_score)/(max_score - min_score));
		if (th< 0) th = 0;
		if (th > 1) th = 1;
		threshold_object.y.push_back(th);


		smoothed_movement_vals.y.resize(movement_vals.y.size());
		for ( int i = 0; i < movement_vals.y.size(); i++) {
			int di = i - 4;
			int ddi = i + 4;
			if (di < 0) di = 0;
			if (ddi >= movement_vals.y.size()) ddi = movement_vals.y.size()-1;
			float sum(0);
			for (int j = di; j <= ddi; j++) 
				sum += movement_vals.y[j];
			smoothed_movement_vals.y[i] = sum / (ddi - di + 1);
		}


		time_axes.x_label = "age (days)";
		time_axes.properties.text.color = ns_color_8(255, 255, 255);
		time_axes.properties.line.color = ns_color_8(255, 255, 255);
		smoothed_movement_vals.y_label = "Movement score";
		smoothed_movement_vals.properties.text.color = ns_color_8(255, 255, 255);
		smoothed_movement_vals.properties.line.color = ns_color_8(255,255,255);
		size_vals.properties.line.color = ns_color_8(125, 125, 255);
		smoothed_movement_vals.properties.line.draw = size_vals.properties.line.draw = true;
		smoothed_movement_vals.properties.line.width = size_vals.properties.line.width = 1;
		smoothed_movement_vals.properties.point.draw = size_vals.properties.point.draw = false;

		movement_vals.properties.line.draw = false;
		movement_vals.properties.point.draw = true;
		movement_vals.properties.point.width = 1;
		movement_vals.properties.point.color = ns_color_8(200, 200, 200);
		movement_vals.properties.point.edge_width = 1;



		threshold_object.properties.line.color = ns_color_8(100, 100, 100);
		threshold_object.properties.line.draw = true;


		graph.contents.push_back(movement_vals);
		graph.contents.push_back(smoothed_movement_vals);
		graph.contents.push_back(threshold_object);
		graph.contents.push_back(size_vals);
		graph.contents.push_back(time_axes);


		graph.x_axis_properties.line.color = graph.y_axis_properties.line.color = ns_color_8(255, 255, 255);
		graph.x_axis_properties.text.color = graph.y_axis_properties.text.color = ns_color_8(255, 255, 255);
		graph.x_axis_properties.text_size = graph.y_axis_properties.text_size = 10;
		graph.area_properties.area_fill.color = ns_color_8(0, 0, 0);

		ns_graph_axes axes;
		axes.boundary(0) = floor( min_time / 6.0 / 60 / 24) / 10;
		axes.boundary(1) = floor(max_time / 6.0 / 60 / 24) / 10;
		axes.boundary(2) = -.1;
		axes.boundary(3) = 1;
		ns_color_8 gray(50, 50, 50);
		graph.x_axis_properties.line.width = 
			graph.y_axis_properties.line.width = 1;
		graph.x_axis_properties.line.color =
			graph.y_axis_properties.line.color = gray;

		graph.set_graph_display_options("", axes, base_graph.properties().width/(float)base_graph.properties().height);
		graph_specifics = graph.draw(base_graph);
	}
	inline void map_value_from_graph_onto_image(const float &x, const float &y, unsigned long & x1, unsigned long & y1) {
		x1 = graph_specifics.boundary.x + (unsigned int)(graph_specifics.dx*(x - graph_specifics.axes.boundary(0) + graph_specifics.axes.axis_offset(0)));

		y1 = base_graph.properties().height - graph_specifics.boundary.y - (unsigned int)(graph_specifics.dy*(y - graph_specifics.axes.boundary(2) + graph_specifics.axes.axis_offset(1)));
	}
	inline unsigned long map_pixel_from_image_onto_buffer(const unsigned long &x, const unsigned long &y, const ns_vector_2i &position, const ns_vector_2i &buffer_size) {
		return 3 * ((buffer_size.y - y - position.y-1)*buffer_size.x + x + position.x);
	}
	void overlay_metadata(const unsigned long current_element, const ns_vector_2i & position, const ns_vector_2i & buffer_size, ns_8_bit * buffer) {
		unsigned long x_score, y_score, x_size, y_size;
		map_value_from_graph_onto_image(time_axes.x[current_element], smoothed_movement_vals.y[current_element], x_score, y_score);
		map_value_from_graph_onto_image(time_axes.x[current_element], size_vals.y[current_element], x_size, y_size);
		for (int y = -2; y <= 2; y++)
			for (int x = -2; x <= 2; x++) {
				unsigned long p(map_pixel_from_image_onto_buffer(x_score+x+border().x, y_score+y+ border().y, position, buffer_size));
				buffer[p] = 255;
				buffer[p+1] = 0;
				buffer[p+2] = 0;
				p = map_pixel_from_image_onto_buffer(x_size + x + border().x, y_size + y + border().y, position, buffer_size);
				buffer[p] = 255;
				buffer[p + 1] = 0;
				buffer[p + 2] = 0;
			}

	}
	
public:
	unsigned long get_graph_time_from_graph_position(const float x) { //x is in relative time
		ns_analyzed_image_time_path *path(&region_data->movement_analyzer.group(group_id).paths[0]);

		unsigned long dT(path->element(path->element_count()-1).absolute_time - path->element(0).absolute_time);
		float dt((path->element(path->element_count()-1).relative_time - path->element(0).relative_time) / 60.0 / 60.0 / 24.0);
		return ((x - path->element(0).relative_time / 60.0 / 60.0 / 24.0) / dt)*dT + path->element(0).absolute_time;

	}
	ns_vector_2i get_graph_value_from_click_position(const unsigned long &x, const unsigned long & y) const{
		ns_vector_2i res;
		res.x = ((long)x - (long)graph_specifics.boundary.x - (long)border().x) / (graph_specifics.dx) + graph_specifics.axes.boundary(0) - graph_specifics.axes.axis_offset(0);

		//y = base_graph.properties().height - graph_specifics.boundary.y - (unsigned int)(graph_specifics.dy*(y - graph_specifics.axes.boundary(2) + graph_specifics.axes.axis_offset(1)));
		res.y = -((long)y - base_graph.properties().height + graph_specifics.boundary.y + border().y) / graph_specifics.dy + graph_specifics.axes.boundary(2) - graph_specifics.axes.axis_offset(1);
		return res;
	}
	ns_vector_2i image_size() const { 
		if (!_show) return ns_vector_2i(0, 0);
		return ns_vector_2i(300, 300); 
	}
	ns_vector_2i border() const {
		return ns_vector_2i(25, 25);
	}
	ns_animal_telemetry() :_show(true), region_data(0), group_id(0),size_vals(ns_graph_object::ns_graph_dependant_variable), smoothed_movement_vals(ns_graph_object::ns_graph_dependant_variable),movement_vals(ns_graph_object::ns_graph_dependant_variable), time_axes(ns_graph_object::ns_graph_independant_variable){}
	void set_current_animal(const unsigned int & group_id_, ns_posture_analysis_model & mod,ns_death_time_posture_solo_annotater_region_data * region_data_) {
		group_id = group_id_;
		region_data = region_data_;
		base_graph.init(ns_image_properties(0, 0, 3));
		posture_analysis_model = mod;
	}
	void draw(const unsigned long element_id, const ns_vector_2i & position, const ns_vector_2i & graph_size, const ns_vector_2i & buffer_size, ns_8_bit * buffer) {
		if (base_graph.properties().height == 0) {
			base_graph.use_more_memory_to_avoid_reallocations();
			ns_image_properties prop;
			prop.components = 3;
			prop.width = graph_size.x- 2*border().x;
			prop.height = graph_size.y - 2*border().y;


			base_graph.init(prop);
			try {
				draw_base_graph();
			}
			catch (...) {
				base_graph.init(ns_image_properties(0, 0, 3));
				throw;
			}
		}
		//top margin
		for (unsigned int y = 0; y < border().y; y++)
			for (unsigned int x = 0; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++) 
					buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;
		
		for (unsigned int y = 0; y < base_graph.properties().height; y++) {
			//left margin
			for (unsigned int x = 0; x < border().x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + border().y, position, buffer_size) + c] = 0;
			//graph
			for (unsigned int x = 0; x < base_graph.properties().width; x++) {
				for (unsigned int c = 0; c < 3; c++) 
					buffer[map_pixel_from_image_onto_buffer(x + border().x, y + border().y, position, buffer_size) + c] = base_graph[y][3 * x + c];
			}
			overlay_metadata(element_id, position, buffer_size, buffer);

			//right margin
			for (unsigned int x = base_graph.properties().width + border().x; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y + border().y, position, buffer_size) + c] = 0;
		}
		//top margin
		for (unsigned int y = border().y+ base_graph.properties().height; y < graph_size.y; y++)
			for (unsigned int x = 0; x < graph_size.x; x++)
				for (unsigned int c = 0; c < 3; c++)
					buffer[map_pixel_from_image_onto_buffer(x, y, position, buffer_size) + c] = 0;
	}
	void show(bool s) { _show = s; }
	bool show() const { return _show;  }
};

#endif
