#ifndef NS_death_time_solo_posture_annotater_H
#define NS_death_time_solo_posture_annotater_H
#include "ns_image.h"
#include "ns_image_server.h"
#include "ns_image_series_annotater.h"
#include "ns_death_time_annotation.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_death_time_posture_annotater.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include <functional> 
#include "ns_subpixel_image_alignment.h"
#include "ns_fl_modal_dialogs.h"
#include "ns_animal_telemetry.h"
void ns_hide_worm_window();

void ns_specify_worm_details(const ns_64_bit region_info_id,const ns_stationary_path_id & worm, const ns_death_time_annotation & sticky_properties, std::vector<ns_death_time_annotation> & event_times);

class ns_death_time_solo_posture_annotater_timepoint : public ns_annotater_timepoint{
public:
	typedef enum { ns_image, ns_movement, ns_movement_threshold, ns_movement_and_image, ns_movement_threshold_and_image } ns_visualization_type;

	const ns_analyzed_image_time_path_element * path_timepoint_element;
	ns_time_path_image_movement_analyzer * movement_analyzer;
	unsigned long element_id;
	unsigned long group_id;
	ns_vector_2i image_pane_area;

	enum {ns_movement_bar_height=15,ns_bottom_border_height_minus_hand_bars=115,ns_side_border_width=4, ns_minimum_width = 350};
	
	static int bottom_border_height(){return (int)ns_bottom_border_height_minus_hand_bars + ns_movement_bar_height*(int)ns_death_time_annotation::maximum_number_of_worms_at_position;}


	ns_image_storage_source_handle<ns_8_bit> get_image(ns_sql & sql){
		throw ns_ex("N/A");
	}
	
	static void render_image(const ns_visualization_type vis_type, const ns_registered_image_set & registered_images, ns_image_standard & output, const std::string & debug_label) {
		const ns_image_standard_signed & movement_image(registered_images.movement_image_);
		const ns_image_standard &image(registered_images.image);

		int min_mov(INT_MAX), max_mov(0);
		float mov_r;
		if (vis_type != ns_image) {
			for (int y = 0; y < movement_image.properties().height; y++)
				for (int x = 0; x < movement_image.properties().width; x++) {
					if (!registered_images.get_stabilized_worm_neighborhood_threshold(y, x))
						continue;
					int t(abs(movement_image[y][x]));
					if (t < min_mov)
						min_mov = t;
					if (t > max_mov)
						max_mov = t;
				}
			if (min_mov == max_mov) {
				if (min_mov != 0)
					min_mov--;
				if (max_mov != 255)
					max_mov++;
			}
			mov_r = max_mov - min_mov;
		}
		//cout << debug_label <<  "(" << max_mov << "," << min_mov << "; ";
		float movement_sum(0);
		//top border
		for (unsigned int y = 0; y < ns_side_border_width; y++) {
			for (unsigned int x = 0; x < 3 * output.properties().width; x++) {
				output[y][x] = 0;
			}
		}
		for (unsigned int y = 0; y < image.properties().height; y++) {
			//left border
			for (unsigned int x = 0; x < ns_side_border_width; x++) {
				output[y + ns_side_border_width][3 * x] =
					output[y + ns_side_border_width][3 * x + 1] =
					output[y + ns_side_border_width][3 * x + 2] = 0;
			}
			//middle
			switch (vis_type) {
			case ns_image:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					output[y + ns_side_border_width][3 * (x + ns_side_border_width)] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 1] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 2] = image[y][x];
				}
				break;
			case ns_movement:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					ns_8_bit t;

					if (registered_images.get_stabilized_worm_neighborhood_threshold(y, x)) {
						long sum, count;
						ns_analyzed_image_time_path::spatially_average_movement(y, x, ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);

						if (count == 0) t = 0;
						else {
							t = 255 * ((abs(sum / count) - min_mov) / mov_r);
							movement_sum += abs(sum) / (float)count;
						}
					}
					else t = 0;


					output[y + ns_side_border_width][3 * (x + ns_side_border_width)] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 1] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 2] = t;
				}
				break;
			case ns_movement_threshold:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					ns_8_bit t;
					if (registered_images.get_stabilized_worm_neighborhood_threshold(y, x)) {
						long sum, count;
						ns_analyzed_image_time_path::spatially_average_movement(y, x, ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);

						if (abs(sum) < count*ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_threshold)
							t = 0;
						else {
							t = 255 * ((abs(sum / count) - min_mov) / mov_r);
							movement_sum += abs(sum) / (float)count;
						}
					}
					else t = 0;
					output[y][3 * (x + ns_side_border_width)] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 1] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 2] = t;
				}
				break;
			case ns_movement_and_image:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					float f;
					if (registered_images.get_stabilized_worm_neighborhood_threshold(y, x)) {
						long sum, count;
						ns_analyzed_image_time_path::spatially_average_movement(y, x, ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);

						if (count == 0)
							f = 0;
						else {
							f = ((abs(sum / count) - min_mov) / mov_r);
							movement_sum += abs(sum) / (float)count;
						}
					}
					else f = 0;

					const int r = image[y][x] * (1 - f) + 255 * f,  //goes up to 255 the more movement there is
						bg = image[y][x] * (1 - f);  //goes down to zero the more movement there is.
					output[y + ns_side_border_width][3 * (x + ns_side_border_width)] = r;
					output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 1] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 2] = bg;
				}
				break;
			case ns_movement_threshold_and_image:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					float f;
					if (registered_images.get_stabilized_worm_neighborhood_threshold(y, x)) {
						long sum, count;
						ns_analyzed_image_time_path::spatially_average_movement(y, x, ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);
						if (count == 0 || abs(sum) < count* ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_threshold)
							f = 0;
						else {
							f = ((abs(sum / count) - min_mov) / mov_r);
							movement_sum += abs(sum) / (float)count;
						}
					}
					else f = 0;

					int r = image[y][x] * (1 - f) + 255 * f,  //goes up to 255 the more movement there is
						bg = image[y][x] * (1 - f);  //goes down to zero the more movement there is.
					output[y + ns_side_border_width][3 * (x + ns_side_border_width)] = r;
					output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 1] =
						output[y + ns_side_border_width][3 * (x + ns_side_border_width) + 2] = bg;
				}
				break;

			}
			//right border
			for (unsigned int x = image.properties().width + ns_side_border_width; x < output.properties().width; x++) {
				output[y+ ns_side_border_width][3 * x] =
					output[y + ns_side_border_width][3 * x + 1] =
					output[y + ns_side_border_width][3 * x + 2] = 0;
			}
		}
		for (unsigned int y = image.properties().height + ns_side_border_width; y < output.properties().height; y++) {
			for (unsigned int x = 0; x < output.properties().width; x++) {
				output[y][3 * x] =
					output[y][3 * x + 1] =
					output[y][3 * x + 2] = 0;
			}
		}
		//cerr << movement_sum << ")\n";

	}
	
	void load_image(const unsigned long buffer_height,ns_annotater_image_buffer_entry & im,ns_sql & sql,ns_image_standard & temp_buffer, const unsigned long resize_factor_=1){
		movement_analyzer->load_images_for_group(group_id,element_id+10,sql,false,false);
	//	ns_annotater_timepoint::load_image(buffer_height,im,sql,temp_buffer,resize_factor_);
		if (path_timepoint_element == 0){
		  cerr << "Path timepoint element not specified";
		  return;
		}
		if (!path_timepoint_element->registered_image_is_loaded()){
			cerr << "No image is loaded for current frame";
			return;
		}
		//size allocated here xxx
		ns_image_properties prop(path_timepoint_element->image().properties());
		if (prop.width < ns_minimum_width)
			prop.width = ns_minimum_width;
		prop.width+=2*ns_side_border_width;
		prop.height+=bottom_border_height();
		prop.components = 3;
		im.loaded = true;
		im.im->init(prop);

		render_image(vis_type, path_timepoint_element->registered_image_set(), *im.im,"precomputed");
		
		image_pane_area.y = path_timepoint_element->image().properties().height;
		image_pane_area.x = path_timepoint_element->image().properties().width;
		im.loaded = true;
	}
	void set_visulaization_type(const ns_visualization_type t) { vis_type = t; }
	private:
		 ns_visualization_type vis_type;
};


class ns_death_time_posture_solo_annotater_data_cache{
	
	//region_info_id , data
	typedef std::map<unsigned long,ns_death_time_posture_solo_annotater_region_data *> ns_movement_data_list;
	ns_movement_data_list region_movement_data;
	//ns_death_time_posture_solo_annotater_region_data data;

public:
	void clear(){
		region_movement_data.clear();
	}
	ns_death_time_posture_solo_annotater_region_data * get_region_movement_data(const unsigned int region_id, ns_sql & sql, bool load_movement_quantification_data) {

		unsigned long current_time(ns_current_time());
		ns_movement_data_list::iterator p = region_movement_data.find(region_id);
		if (p == region_movement_data.end()) {
			//clear out old cache entries to prevent large cumulative memory allocation
			unsigned long cleared(0);
			unsigned long count(region_movement_data.size());
			unsigned long cutoff_time;
			unsigned long max_buffer_size(6);
			if (region_movement_data.size() < max_buffer_size) {
				//delete all cached data older than 3 minutes
				cutoff_time = current_time - 60 * 60 * 3;
			}
			else {
				std::vector<unsigned long> times(region_movement_data.size());
				unsigned long i(0);
				for (ns_movement_data_list::iterator q = region_movement_data.begin(); q != region_movement_data.end(); q++) {
					times[i] = q->second->loading_time;
					i++;
				}
				std::sort(times.begin(), times.end(), std::greater<unsigned long>());
				cutoff_time = times[max_buffer_size - 1];
			}
			for (ns_movement_data_list::iterator q = region_movement_data.begin(); q != region_movement_data.end();) {
				if (q->second->loading_time <= cutoff_time) {
					delete q->second;
					region_movement_data.erase(q++);
					cleared++;
				}
				else q++;
			}
			if (cleared > 0)
				cout << "Cleared " << cleared << " out of " << count << " entries in path cache.\n";
			p = region_movement_data.insert(ns_movement_data_list::value_type(region_id, new ns_death_time_posture_solo_annotater_region_data())).first;
		}
		ns_death_time_posture_solo_annotater_region_data & data = *(p->second);
		try {
			data.metadata.load_from_db(region_id, "", sql);
			ns_image_server_results_subject sub;
			sub.region_id = region_id;
			data.loading_time = current_time;
			data.annotation_file = image_server.results_storage.hand_curated_death_times(sub, sql);
			//this will work even if no path data can be loaded, but 
			//p->loaded_path_data_successfully will be set to false

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading movement quantification data"));
			data.load_movement_analysis(region_id, sql, load_movement_quantification_data);
			
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting timing"));
			data.by_hand_timing_data.resize(0);
			data.machine_timing_data.resize(0);
			data.by_hand_timing_data.resize(data.movement_analyzer.size());
			data.machine_timing_data.resize(data.movement_analyzer.size());
			for (unsigned int i = 0; i < data.movement_analyzer.size(); i++){
				ns_stationary_path_id path_id;
				path_id.detection_set_id = data.movement_analyzer.db_analysis_id();
				path_id.group_id = i;
				path_id.path_id = 0;
				data.by_hand_timing_data[i].stationary_path_id = path_id;
				data.machine_timing_data[i].stationary_path_id = path_id;

				data.by_hand_timing_data[i].animals.resize(1);
				data.by_hand_timing_data[i].animals[0].set_fast_movement_cessation_time(
					ns_death_timing_data_step_event_specification(
					data.movement_analyzer[i].paths[0].cessation_of_fast_movement_interval(),
					data.movement_analyzer[i].paths[0].element(data.movement_analyzer[i].paths[0].first_stationary_timepoint()),
										region_id,path_id,0));
				data.by_hand_timing_data[i].animals[0].animal_specific_sticky_properties.animal_id_at_position = 0;
				data.machine_timing_data[i].animals.resize(1);
				data.machine_timing_data[i].animals[0].set_fast_movement_cessation_time(
					ns_death_timing_data_step_event_specification(
					data.movement_analyzer[i].paths[0].cessation_of_fast_movement_interval(),
					data.movement_analyzer[i].paths[0].element(data.movement_analyzer[i].paths[0].first_stationary_timepoint()),
										region_id,path_id,0));
				data.machine_timing_data[i].animals[0].animal_specific_sticky_properties.animal_id_at_position = 0;
				data.by_hand_timing_data[i].animals[0].position_data.stationary_path_id = path_id;
				data.by_hand_timing_data[i].animals[0].position_data.path_in_source_image.position = data.movement_analyzer[i].paths[0].path_region_position;
				data.by_hand_timing_data[i].animals[0].position_data.path_in_source_image.size = data.movement_analyzer[i].paths[0].path_region_size;
				data.by_hand_timing_data[i].animals[0].position_data.worm_in_source_image.position = data.movement_analyzer[i].paths[0].element(data.movement_analyzer[i].paths[0].first_stationary_timepoint()).region_offset_in_source_image();
				data.by_hand_timing_data[i].animals[0].position_data.worm_in_source_image.size = data.movement_analyzer[i].paths[0].element(data.movement_analyzer[i].paths[0].first_stationary_timepoint()).worm_region_size();

				data.by_hand_timing_data[i].animals[0].animal_specific_sticky_properties.stationary_path_id = data.by_hand_timing_data[i].animals[0].position_data.stationary_path_id;
				//data.by_hand_timing_data[i].animals[0].worm_id_in_path = 0;
					//data.by_hand_timing_data[i].sticky_properties.annotation_source = ns_death_time_annotation::ns_lifespan_machine;
				//data.by_hand_timing_data[i].sticky_properties.position = data.movement_analyzer[i].paths[0].path_region_position;
			//	data.by_hand_timing_data[i].sticky_properties.size = data.movement_analyzer[i].paths[0].path_region_size;

			//	data.by_hand_timing_data[i].stationary_path_id = data.by_hand_timing_data[i].position_data.stationary_path_id;

				data.by_hand_timing_data[i].animals[0].region_info_id = region_id;
				data.machine_timing_data[i].animals[0] =
					data.by_hand_timing_data[i].animals[0];

				//by default specify the beginnnig of the path as the translation cessation time.
				if (data.by_hand_timing_data[i].animals[0].translation_cessation.time.period_end==0)
		//			for (int k = 0; k < 2; k++)
					data.by_hand_timing_data[i].animals[0].step_event(
						ns_death_timing_data_step_event_specification(
									data.movement_analyzer[i].paths[0].cessation_of_fast_movement_interval(),
									data.movement_analyzer[i].paths[0].element(data.movement_analyzer[i].paths[0].first_stationary_timepoint()),region_id,
									data.by_hand_timing_data[i].animals[0].position_data.stationary_path_id,0),data.movement_analyzer[i].paths[0].observation_limits(),false);
			}

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading annotations 2"));
			data.load_annotations(sql,false);
		
		}
		catch (...) {
			delete p->second;
			region_movement_data.erase(p);
			throw;
		}

		return p->second;
	}


	void add_annotations_to_set(ns_death_time_annotation_set & set, std::vector<ns_death_time_annotation> & orphaned_events){
		for (ns_death_time_posture_solo_annotater_data_cache::ns_movement_data_list::iterator p = region_movement_data.begin(); p != region_movement_data.end(); p++)
			p->second->add_annotations_to_set(set,orphaned_events);
	}
	bool save_annotations(const ns_death_time_annotation_set & a){
		throw ns_ex("Solo posture annotater annotations should be mixed in with storyboard");
		for (ns_death_time_posture_solo_annotater_data_cache::ns_movement_data_list::iterator p = region_movement_data.begin(); p != region_movement_data.end(); p++)
			p->second->save_annotations(a);
		return true;
	}
};
void ns_crop_time(const ns_time_path_limits & limits, const ns_death_time_annotation_time_interval & first_observation_in_path, const ns_death_time_annotation_time_interval & last_observation_in_path, ns_death_time_annotation_time_interval & target);

class ns_worm_learner;
class ns_death_time_solo_posture_annotater : public ns_image_series_annotater {
private:

	ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type current_visualization_type;
	std::vector<ns_death_time_solo_posture_annotater_timepoint> timepoints;
	static ns_death_time_posture_solo_annotater_data_cache data_cache;
	ns_death_time_posture_solo_annotater_region_data * current_region_data;
	ns_analyzed_image_time_path * current_worm;
	ns_death_time_annotation properties_for_all_animals;

	unsigned long current_element_id()const { return current_timepoint_id; }
	ns_death_time_annotation_time_interval current_time_interval() {
		if (current_timepoint_id == 0)
			return current_worm->observation_limits().interval_before_first_observation;
		return ns_death_time_annotation_time_interval(current_worm->element(current_timepoint_id - 1).absolute_time,
			current_worm->element(current_timepoint_id).absolute_time);

	}
	ns_animal_list_at_position & current_by_hand_timing_data() { return current_region_data->by_hand_timing_data[properties_for_all_animals.stationary_path_id.group_id]; }
	ns_animal_list_at_position * current_machine_timing_data;
	unsigned long current_animal_id;

	static void draw_box(const ns_vector_2i & p, const ns_vector_2i & s, const ns_color_8 & c, ns_image_standard & im, const unsigned long thickness) {
		im.draw_line_color(p, p + ns_vector_2i(s.x, 0), c, thickness);
		im.draw_line_color(p, p + ns_vector_2i(0, s.y), c, thickness);
		im.draw_line_color(p + s, p + ns_vector_2i(s.x, 0), c, thickness);
		im.draw_line_color(p + s, p + ns_vector_2i(0, s.y), c, thickness);
	}
	static std::string state_label(const ns_movement_state cur_state) {
		switch (cur_state) {
		case ns_movement_stationary:return "Dead";
		case ns_movement_posture: return "Changing Posture";
		case ns_movement_slow: return "Slow Moving";
		case ns_movement_fast: return "Fast Moving";
		case ns_movement_death_posture_relaxation: return "Death Posture Relaxation";
		default: return std::string("Unknown:") + ns_to_string((int)cur_state);
		}
	}
	enum { bottom_offset = 5 };

	void draw_metadata(ns_annotater_timepoint * tp_a, ns_image_standard & im) {
		if (current_worm == 0) {
			cerr << "worm not loaded/n";
			return;
		}
		if (current_element_id() >= current_worm->element_count()) {
			cerr << "Element " << current_element_id() << " does not exist/n";
			return;
		}

		if (im.properties().height == 0 || im.properties().width == 0)
			throw ns_ex("No worm image is loaded");
		const ns_death_time_solo_posture_annotater_timepoint * tp(static_cast<const ns_death_time_solo_posture_annotater_timepoint *>(tp_a));
		const unsigned long clear_thickness(5);
		const unsigned long thickness_offset(3);
		ns_acquire_lock_for_scope lock(font_server.default_font_lock, __FILE__, __LINE__);
		ns_font & font(font_server.get_default_font());
		const unsigned long text_height(14);
		font.set_height(text_height);
		ns_vector_2i bottom_margin_bottom(bottom_margin_position());
		ns_vector_2i bottom_margin_top(bottom_margin_bottom.x + current_worm->element(current_element_id()).image().properties().width + ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width,
			bottom_margin_bottom.y + ns_death_time_solo_posture_annotater_timepoint::bottom_border_height() - bottom_offset);

		for (unsigned int y = bottom_margin_bottom.y; y < bottom_margin_top.y; y++)
			for (unsigned int x = bottom_margin_bottom.x; x < bottom_margin_top.x; x++) {
				//if (y>=im.properties().height||x>=im.properties().width) {
				//cerr << "out of bounds: " << x << ","<< y << " " << im.properties().width << ","<< im.properties().height << "\n";
				//continue;
				//}
				im[y][3 * x] =
					im[y][3 * x + 1] =
					im[y][3 * x + 2] = 0;
			}


		unsigned long vis_height(10);
		const ns_time_path_limits observation_limit(current_worm->observation_limits());
		ns_death_time_annotation_time_interval first_path_obs, last_path_obs;
		last_path_obs.period_end = current_worm->element(current_worm->element_count() - 1).absolute_time;
		last_path_obs.period_start = current_worm->element(current_worm->element_count() - 2).absolute_time;
		first_path_obs.period_start = current_worm->element(0).absolute_time;
		first_path_obs.period_end = current_worm->element(1).absolute_time;

		//handle out of bound values
		for (ns_animal_list_at_position::ns_animal_list::iterator p = current_by_hand_timing_data().animals.begin(); p != current_by_hand_timing_data().animals.end(); p++) {
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->fast_movement_cessation.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->death_posture_relaxation_termination_.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->death_posture_relaxation_start.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->movement_cessation.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->translation_cessation.time);
		}

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing timing."));
		current_machine_timing_data->animals[0].draw_movement_diagram(bottom_margin_bottom,
			ns_vector_2i(current_worm->element(current_element_id()).image().properties().width, vis_height),
			observation_limit,
			current_time_interval(),
			im, 0.8);
		const unsigned long hand_bar_height(current_by_hand_timing_data().animals.size()*
			ns_death_time_solo_posture_annotater_timepoint::ns_movement_bar_height);

		for (unsigned int i = 0; i < current_by_hand_timing_data().animals.size(); i++) {
			current_by_hand_timing_data().animals[i].draw_movement_diagram(bottom_margin_bottom
				+ ns_vector_2i(0, (i + 1)*ns_death_time_solo_posture_annotater_timepoint::ns_movement_bar_height),
				ns_vector_2i(current_worm->element(current_element_id()).image().properties().width, vis_height),
				observation_limit,
				current_time_interval(),
				im, (i == current_animal_id) ? 1.0 : 0.8);
		}

		const int num_lines(6);
		std::string lines[num_lines];
		ns_color_8 line_color[num_lines];
		const ns_movement_state cur_by_hand_state(current_by_hand_timing_data().animals[current_animal_id].movement_state(current_worm->element(current_element_id()).absolute_time));
		const ns_movement_state cur_machine_state(current_machine_timing_data->animals[0].movement_state(current_worm->element(current_element_id()).absolute_time));
		const bool by_hand_death_contracting(current_by_hand_timing_data().animals[current_animal_id].is_death_time_contracting(current_worm->element(current_element_id()).absolute_time));
		const bool machine_death_contracting(current_machine_timing_data->animals[0].is_death_time_contracting(current_worm->element(current_element_id()).absolute_time));
		lines[0] = "Frame " + ns_to_string(current_element_id() + 1) + " of " + ns_to_string(timepoints.size()) + " ";
		lines[0] += current_worm->element(current_element_id()).element_before_fast_movement_cessation ? "(B)" : "(A)";
		lines[0] += current_worm->element(current_element_id()).inferred_animal_location ? "(I)" : "";
		if (current_region_data->metadata.time_at_which_animals_had_zero_age != 0) {
			lines[0] += "Day ";
			lines[0] += ns_to_string_short((float)
				(current_worm->element(current_element_id()).absolute_time -
					current_region_data->metadata.time_at_which_animals_had_zero_age) / 24.0 / 60 / 60, 2);
			lines[0] += " ";
		}
		lines[0] += "Date: ";
		lines[0] += ns_format_time_string_for_human(current_worm->element(current_element_id()).absolute_time);

		lines[1] = (current_region_data->metadata.plate_type_summary());


		lines[2] = current_region_data->metadata.sample_name + "::" + current_region_data->metadata.region_name + "::worm #" + ns_to_string(properties_for_all_animals.stationary_path_id.group_id);

		ns_vector_2i p(current_worm->path_region_position + current_worm->path_region_size / 2);
		lines[2] += " (" + ns_to_string(p.x) + "," + ns_to_string(p.y) + ")";

		lines[3] = "Machine:" + state_label(cur_machine_state) + (machine_death_contracting ? "(Contr/Expr)" : "");
		if (current_by_hand_timing_data().animals[current_animal_id].specified)
			lines[4] += "Human: " + state_label(cur_by_hand_state) + (by_hand_death_contracting ? "(Contr/Expr)" : "");
		else lines[4] += "Human: Not Specified";
		if (properties_for_all_animals.number_of_worms_at_location_marked_by_hand > 1) {
			lines[4] += " +";
			lines[4] += ns_to_string(properties_for_all_animals.number_of_worms_at_location_marked_by_hand);
		}

		if (properties_for_all_animals.is_excluded())
			lines[5] = "Worm Excluded ";
		if (properties_for_all_animals.flag.specified())
			lines[5] += properties_for_all_animals.flag.label();


		line_color[0] = line_color[1] = line_color[2] = line_color[3] = line_color[4] =
			ns_color_8(255, 255, 255);
		line_color[5] = ns_annotation_flag_color(properties_for_all_animals);

		ns_vector_2i text_pos(bottom_margin_bottom.x, bottom_margin_bottom.y + hand_bar_height + 15 + text_height);
		for (unsigned int i = 0; i < num_lines; i++)
			font.draw_color(text_pos.x, text_pos.y + i*(text_height + 1), line_color[i], lines[i], im);

		//unsigned long small_label_height(im.properties().height/80);
		//font.set_height(small_label_height);

		//clear out old box
		/*ns_vector_2d off(ns_analyzed_image_time_path::maximum_alignment_offset() + current_worm->element(current_element_id()).offset_from_path);
		draw_box(ns_vector_2i(off.x+ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width,off.y),
			current_worm->element(current_element_id()).worm_context_size(),
				  ns_color_8(60,60,60),im,2);

		draw_box(ns_vector_2i(ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width,0),
				 ns_vector_2i(timepoints[current_element_id()].path_timepoint_element->image().properties().width,
				 timepoints[current_element_id()].path_timepoint_element->image().properties().height),
				ns_color_8(60,60,60),im,2);

		draw_box(ns_vector_2i(ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width,0),
				 ns_vector_2i(timepoints[current_element_id()].path_timepoint_element->image().properties().width,
				 timepoints[current_element_id()].path_timepoint_element->image().properties().height),
				ns_color_8(60,60,60),im,2);
				*/
				//	draw_box(timepoints[current_element_id()].path_timepoint_element->measurements.local_maximum_position, 
				//			 timepoints[current_element_id()].path_timepoint_element->measurements.local_maximum_area,
				//			ns_color_8(0,0,60),im,1);

			//		font.set_height(im.properties().height/40);
				//	font.draw_color(1,im.properties().height/39,ns_color_8(255,255,255),ns_format_time_string_for_human(tp->absolute_time),im);
		lock.release();
	}



	ns_worm_learner * worm_learner;


	enum { default_resize_factor = 1, max_buffer_size = 15 };

	mutable bool saved_;
public:


	ns_animal_telemetry::ns_graph_contents step_graph_type() {
		graph_contents = (ns_animal_telemetry::ns_graph_contents)((int)graph_contents + 1);
		if (graph_contents == ns_animal_telemetry::ns_number_of_graph_types) {
			graph_contents = ns_animal_telemetry::ns_none;
			telemetry.show(false);
		}
		else
			telemetry.show(true);
		request_refresh();
		return graph_contents;
	}
	ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type step_visualization_type() {

		if (current_visualization_type == ns_death_time_solo_posture_annotater_timepoint::ns_movement_threshold_and_image)
			current_visualization_type = ns_death_time_solo_posture_annotater_timepoint::ns_image;
		else current_visualization_type = (ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type)((int)current_visualization_type + 1);

		ns_acquire_lock_for_scope lock(image_buffer_access_lock, __FILE__, __LINE__);
		for (unsigned int i = 0; i < timepoints.size(); i++) {
			timepoints[i].set_visulaization_type(current_visualization_type);
		}
		for (unsigned int i = 0; i < previous_images.size(); i++) {
			previous_images[i].loaded = false;
			next_images[i].loaded = false;
		}

		ns_image_standard temp_buffer;
		timepoints[current_timepoint_id].load_image(1024, current_image, sql(), temp_buffer, 1);

		draw_metadata(&timepoints[current_timepoint_id], *current_image.im);
		lock.release();
		return current_visualization_type;
	}

	ns_animal_telemetry telemetry;
	ns_animal_telemetry::ns_graph_contents graph_contents;

	typedef enum { ns_none, ns_forward, ns_back, ns_fast_forward, ns_fast_back, ns_stop, ns_save, ns_rewind_to_zero, ns_write_quantification_to_disk, ns_step_visualization, ns_step_graph, ns_number_of_annotater_actions } ns_image_series_annotater_action;

	inline ns_annotater_timepoint * timepoint(const unsigned long i) { return &timepoints[i]; }
	inline unsigned long number_of_timepoints() { return timepoints.size(); }

	void set_resize_factor(const unsigned long resize_factor_) { resize_factor = resize_factor_; }
	bool data_saved()const { return saved_; }
	ns_death_time_solo_posture_annotater() :ns_image_series_annotater(default_resize_factor, ns_death_time_posture_annotater_timepoint::ns_bottom_border_height),
		saved_(true), graph_contents(ns_animal_telemetry::ns_none), current_visualization_type(ns_death_time_solo_posture_annotater_timepoint::ns_image), current_region_data(0), current_worm(0), current_machine_timing_data(0) {}

	typedef enum { ns_time_aligned_images, ns_death_aligned_images } ns_alignment_type;

	bool load_annotations() {
		//	for (u
		saved_ = true;
		//	saved_ = !fix_censored_events_with_no_time_specified();
		return true;
	}

	void add_annotations_to_set(ns_death_time_annotation_set & set, std::vector<ns_death_time_annotation> & orphaned_events) {
		data_cache.add_annotations_to_set(set, orphaned_events);
	}

	void save_annotations(const ns_death_time_annotation_set & set) const {
		data_cache.save_annotations(set);

		ns_update_information_bar(string("Annotations saved at ") + ns_format_time_string_for_human(ns_current_time()));
		saved_ = true;
	};
	bool current_region_path_data_loaded() {
		return current_region_data->loaded_path_data_successfully;
	}
	void close_worm() {
		properties_for_all_animals = ns_death_time_annotation();
		properties_for_all_animals.region_info_id = 0;
		current_worm = 0;
		current_machine_timing_data = 0;
		current_timepoint_id = 0;
		current_animal_id = 0;
	}
	void clear_data_cache() {
		data_cache.clear();
	}

	static bool ns_fix_annotation(ns_death_time_annotation & a,ns_analyzed_image_time_path & p);
	void set_current_timepoint(const unsigned long current_time,const bool require_exact=true,const bool give_up_if_too_long=false){
		for (current_timepoint_id = 0; current_timepoint_id < current_worm->element_count(); current_timepoint_id++){
				if (current_worm->element(current_timepoint_id).absolute_time >= current_time)
					break;
			}	
		if (current_timepoint_id >= current_worm->element_count())
			current_timepoint_id = current_worm->element_count() - 1;
		bool give_up_on_preload(false);
		if (current_timepoint_id > 50 && give_up_if_too_long){
			give_up_on_preload = true;
			current_timepoint_id = 0;
			for (unsigned int k = 0; k < current_worm->element_count(); k++)
				if (!current_worm->element(k).element_before_fast_movement_cessation){
					current_timepoint_id = k;
					break;
				}
		}

		//load images for the worm.

		//data_cache.load_images_for_worm(properties_for_all_animals.stationary_path_id,current_timepoint_id+10,sql);
		current_region_data->clear_images_for_worm(properties_for_all_animals.stationary_path_id);

		current_region_data->load_images_for_worm(properties_for_all_animals.stationary_path_id,current_timepoint_id+10,sql());


		if (require_exact && current_worm->element(current_timepoint_id).absolute_time != current_time && !give_up_on_preload)
			throw ns_ex("Could not find requested element time:") << current_time;
		if (current_timepoint_id+1 < current_worm->element_count())
			current_timepoint_id++;

	}
	void draw_telemetry(const ns_vector_2i & position, const ns_vector_2i & graph_size, const ns_vector_2i & buffer_size, ns_8_bit * buffer) {
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing telemetry."));
		telemetry.draw(graph_contents,current_timepoint_id,  position, graph_size, buffer_size, buffer);
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Done with telemetry."));
	}
	void draw_registration_debug(const ns_vector_2i & position, const ns_vector_2i & buffer_size, ns_8_bit * buffer) {
		if (current_timepoint_id == 0)
			return;
		const ns_image_standard & im1(timepoints[current_timepoint_id].path_timepoint_element->registered_image_set().image);
		const ns_image_standard & im2(timepoints[current_timepoint_id - 1].path_timepoint_element->registered_image_set().image);
		const ns_image_standard & thresh(timepoints[current_timepoint_id].path_timepoint_element->registered_image_set().worm_region_threshold);

		ns_stretch_registration_line_offsets offsets;
		ns_stretch_registration reg;

		float histogram_matching_factors_r[256], histogram_matching_factors_s[256];
		ns_match_histograms(im1, im2, histogram_matching_factors_s);
		float avg_x = reg.calculate(im1, im2, ns_vector_2i(0, 0), ns_vector_2i(im1.properties().width, im1.properties().height), ns_vector_2i(0, 0), offsets, histogram_matching_factors_s);
		ns_stretch_source_mappings mappings;
		ns_stretch_registration::convert_offsets_to_source_positions(offsets, mappings);
		ns_image_standard imr;
		ofstream foo("C:\\Users\\ns89\\Dropbox\\foo.csv");
		foo << "pos,offset,npos,inverse\n";
		for (unsigned int i = 0; i < mappings.p.size(); i++) {
			foo << i << "," << offsets.p[i] << "," << (offsets.p[i] + i) << "," << mappings.p[i] << "\n";
		}
		foo.close();
		ns_stretch_registration::register_image(mappings, 0, im2, imr);
		//cerr << avg_x << " ";
		ns_registered_image_set im_set_r, im_set_s;
		im1.pump(im_set_r.image, 1024);
		im1.pump(im_set_s.image, 1024);
		im_set_r.movement_image_.init(im1.properties());
		im_set_s.movement_image_.init(im2.properties());
	//	ns_match_histograms(im1, imr, histogram_matching_factors_r);
		const int n(2);
		const long kernel_area((2 * n + 1)*(2 * n + 1));
		for ( int y = n; y < imr.properties().height-n; y++) {
			for ( int x = n; x < imr.properties().width-n; x++) {
				double d_r(0);
				double d_s(0);
				for (long dy = -n; dy <= n; dy++) {
					for (long dx = -n; dx <= n; dx++) {
						d_r += (short)(im1[y][x] - histogram_matching_factors_r[imr[y][x]]);
						d_s += (short)(im1[y][x] - histogram_matching_factors_s[im2[y][x]]);
					}
				}
				im_set_r.movement_image_[y][x] = d_r /= kernel_area;
				im_set_s.movement_image_[y][x] = d_s /= kernel_area;
			}
		}
		thresh.pump(im_set_r.worm_region_threshold, 1024);
		thresh.pump(im_set_s.worm_region_threshold, 1024);
		ns_image_properties prop(im1.properties());
		prop.components = 3;
		ns_image_standard out;
		prop.width += 2 * ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width; 
		prop.height += 2 * ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width;
		out.init(prop);
		ns_death_time_solo_posture_annotater_timepoint::render_image(current_visualization_type, im_set_s, out, "linear");
		ns_death_time_solo_posture_annotater_timepoint::render_image(current_visualization_type, im_set_r, out,"nonlinear");


		for (unsigned int y = 0; y < out.properties().height; y++) {
			const int y_(buffer_size.y - y-position.y);
			if (y_ >= buffer_size.y || y_ < 0)
				continue;
			for (unsigned int x = 0; x < 3*out.properties().width; x++) {
				const int x_(3*position.x + x);
				if (x_ >= 3*buffer_size.x)
					break;
				buffer[3 * buffer_size.x*(y_) + x_ ] = (out[y][x]<64)?4* out[y][x]:255;
			}

		}

	}

	ns_vector_2i bottom_margin_position() {
	  if (current_worm == 0 || current_worm->element_count() <= current_element_id()){
	    cerr << "Worm/element not loaded";
	    return ns_vector_2d(0,0);
	    }
	if (!current_worm->element(current_element_id()).registered_image_is_loaded()) {
			cerr << "No image is loaded for current element;";
			return ns_vector_2i(0, 0);
		}
		return ns_vector_2i(ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width,
			current_worm->element(current_element_id()).image().properties().height + bottom_offset);
	}


	void load_worm(const ns_64_bit region_info_id_, const ns_stationary_path_id & worm, const unsigned long current_time, const ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type visualization_type, const ns_experiment_storyboard  * storyboard,ns_worm_learner * worm_learner_){
		

		if (sql.is_null())
			sql.attach(image_server.new_sql_connection(__FILE__, __LINE__));

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Clearing self."));
		clear();
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Clearing annotator."));
	    ns_image_series_annotater::clear();
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Closing previous worm."));
		this->close_worm();
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Stopping movement."));
		stop_fast_movement();
		current_visualization_type = visualization_type;
		
		graph_contents = ns_animal_telemetry::ns_movement_intensity;

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Showing telemetry"));
		telemetry.show(true);
	
		ns_region_metadata metadata;
		metadata.region_id = region_info_id_;
		try{

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Finding Animal"));
			const ns_experiment_storyboard_timepoint_element & e(storyboard->find_animal(region_info_id_,worm));
			ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__,__LINE__);
			worm_learner = worm_learner_;
		
			timepoints.resize(0);
			if (current_region_data != 0) {

				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Clearing images"));
				current_region_data->clear_images_for_worm(properties_for_all_animals.stationary_path_id);
			}
	
			properties_for_all_animals.region_info_id = region_info_id_;
			//get movement information for current worm
		try{
				current_region_data = 0;
				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading movement data"));
				current_region_data = data_cache.get_region_movement_data(region_info_id_,sql(),telemetry.show());
				
			}
			catch(...){
				metadata.load_from_db(region_info_id_,"",sql()); 
				current_region_data = 0;

				throw;
			}
			if (!current_region_data->loaded_path_data_successfully) 
				throw ns_ex("The movement anaylsis data required to inspect this worm is no longer in the database.  You might need to re-analyze movement for this region.");
			
			metadata = current_region_data->metadata;
			if (worm.detection_set_id != 0 && current_region_data->movement_analyzer.db_analysis_id() != worm.detection_set_id)
				throw ns_ex("This storyboard was built using an out-of-date movement analysis result.  Please rebuild the storyboard, so that it will reflect the most recent analysis.");
			if (current_region_data->movement_analyzer.size() <= worm.group_id)
				throw ns_ex("Looking at the movement analysis result for this region, this worm's ID appears to be invalid.");
			if (worm.path_id != 0)
				throw ns_ex("The specified group has multiple paths! This should be impossible");
			
			current_worm = &current_region_data->movement_analyzer[worm.group_id].paths[worm.path_id]; 
		       
			current_animal_id = 0;
			

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading posture analysis model"));
			ns_image_server::ns_posture_analysis_model_cache::const_handle_t handle;
			image_server.get_posture_analysis_model_for_region(region_info_id_, handle, sql());
			ns_posture_analysis_model mod(handle().model_specification);
			mod.threshold_parameters.use_v1_movement_score = false;
			handle.release();
			
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting animal for telemetry"));
			telemetry.set_current_animal(worm.group_id,mod, current_region_data);



			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting fast movement cessation times and adding annotations"));
			for (unsigned int i = 0; i < e.by_hand_movement_annotations_for_element.size(); i++){
				unsigned long animal_id = e.by_hand_movement_annotations_for_element[i].annotation.animal_id_at_position;
				const unsigned long current_number_of_animals(current_region_data->by_hand_timing_data[worm.group_id].animals.size());
				if (animal_id >= current_number_of_animals){
					current_region_data->by_hand_timing_data[worm.group_id].animals.resize(animal_id+1);
					for (unsigned int i = current_number_of_animals; i < animal_id; i++){
						current_region_data->by_hand_timing_data[worm.group_id].animals[i].set_fast_movement_cessation_time(
							ns_death_timing_data_step_event_specification(current_worm->cessation_of_fast_movement_interval(),
							current_worm->element(current_worm->first_stationary_timepoint()),
							properties_for_all_animals.region_info_id,properties_for_all_animals.stationary_path_id,i));
					}
					
				}
				current_region_data->by_hand_timing_data[worm.group_id].animals[animal_id].add_annotation(e.by_hand_movement_annotations_for_element[i].annotation,true); 
				//if (animal_id == 0)
				//if (e.by_hand_movement_annotations_for_element[i].type == ns_additional_worm_entry)
				//	curre nt_region_data->by_hand_timing_data[worm.group_id].animals[animal_id].first_frame_time = e.by_hand_movement_annotations_for_element[i].time.end_time;
				//current_region_data->by_hand_timing_data[worm.group_id].animals[animal_id].add_annotation(e.by_hand_movement_annotations_for_element[i],true);
			}
			
			//current_region_data->metadata
			//current_by_hand_timing_data = &current_region_data->by_hand_timing_data[worm.group_id];
			//xxxx
			if (worm.group_id >= current_region_data->machine_timing_data.size())
			throw ns_ex("Group ID ") << worm.group_id << " is not present in machine timing data (" << current_region_data->machine_timing_data.size() << ") for region " << region_info_id_;
			current_machine_timing_data = &current_region_data->machine_timing_data[worm.group_id];
			current_animal_id = 0;

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Transferring sticky properties"));
			//current_by_hand_timing_data->animals[current_animal_id].sticky_properties = current_machine_timing_data->animals[0].sticky_properties;
			e.event_annotation.transfer_sticky_properties(properties_for_all_animals);
			//current_by_hand_timing_data->animals[current_animal_id].sticky_propertes.annotation_source = ns_death_time_annotation::ns_posture_image;
			properties_for_all_animals.stationary_path_id = worm;
			this->saved_ = true;
			for (unsigned int j = 0; j < current_region_data->by_hand_timing_data[worm.group_id].animals.size(); j++){
				if (ns_fix_annotation(current_region_data->by_hand_timing_data[worm.group_id].animals[j].fast_movement_cessation,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(current_region_data->by_hand_timing_data[worm.group_id].animals[j].translation_cessation,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(current_region_data->by_hand_timing_data[worm.group_id].animals[j].movement_cessation,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(current_region_data->by_hand_timing_data[worm.group_id].animals[j].death_posture_relaxation_termination_,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(current_region_data->by_hand_timing_data[worm.group_id].animals[j].death_posture_relaxation_start, *current_worm))
					saved_ = false;
			}

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Done setting by hand annotations"));
			if (!saved_){

				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Updating events to storyboard"));
				update_events_to_storyboard();
				saved_ = true;
			}
			//initialize worm timing data
			unsigned long number_of_valid_elements;
			for ( number_of_valid_elements = 0;  number_of_valid_elements < current_worm->element_count();  number_of_valid_elements++)
				if ( current_worm->element( number_of_valid_elements).excluded)
					break;

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading timepoints"));
			timepoints.resize(number_of_valid_elements);
			if (number_of_valid_elements == 0)
				throw ns_ex("This path has no valid elements!");
			for (unsigned int i = 0; i < timepoints.size(); i++){
				timepoints[i].path_timepoint_element = &current_worm->element(i);
				timepoints[i].element_id = i;
				timepoints[i].movement_analyzer = & current_region_data->movement_analyzer;  //*
				timepoints[i].group_id = properties_for_all_animals.stationary_path_id.group_id;
				timepoints[i].set_visulaization_type(current_visualization_type);
			}
//			current_by_hand_timing_data->animals[current_animal_id].first_frame_time = timepoints[0].path_timepoint_element->absolute_time;


			//allocate image buffer
			
			if (current_image.im == 0)
				current_image.im = new ns_image_standard();


			set_current_timepoint(current_time,true,true);
			{
				ns_image_standard temp_buffer;

				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading first image."));
				timepoints[current_timepoint_id].load_image(1024,current_image,sql(),temp_buffer,1);
			}
			if (previous_images.size() != max_buffer_size || next_images.size() != max_buffer_size) {
				previous_images.resize(max_buffer_size);
				next_images.resize(max_buffer_size);
				for (unsigned int i = 0; i < max_buffer_size; i++) {
					previous_images[i].im = new ns_image_standard();
					next_images[i].im = new ns_image_standard();
				}
				for (unsigned int i = 0; i < max_buffer_size; i++)
					previous_images[i].im->init(current_image.im->properties());
				for (unsigned int i = 0; i < max_buffer_size; i++)
					next_images[i].im->init(current_image.im->properties());
			}

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing Metadata."));
			draw_metadata(&timepoints[current_timepoint_id],*current_image.im);

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Requesting Refresh."));
			request_refresh();
			lock.release();
		       

		}
		catch(ns_ex & ex){

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Closing worm on error."));
			close_worm();
			cerr << "Error loading worm from region " << metadata.plate_name() << " : " << ex.text() << "\n";
			ns_hide_worm_window();
			throw;
		}
	}

	void clear() {
		clear_base();
		saved_ = false;
		timepoints.resize(0);
		telemetry.clear();
		graph_contents = ns_animal_telemetry::ns_none;
	//	data_cache.clear();
		close_worm();
	//	data_cache.clear();
		current_region_data = 0;
		properties_for_all_animals = ns_death_time_annotation();
	}

	static void step_error_label(ns_death_time_annotation &  sticky_properties){
		//first clear errors where event is both excluded and a flag specified
		if (sticky_properties.is_excluded() && sticky_properties.flag.specified()){
			sticky_properties.excluded = ns_death_time_annotation::ns_not_excluded;
			sticky_properties.flag = ns_death_time_annotation_flag::none();
			return;
		}
		//first click excludes the animal
		if (!sticky_properties.is_excluded() && !sticky_properties.flag.specified()){
			sticky_properties.excluded = ns_death_time_annotation::ns_by_hand_excluded;
			return;
		}
		//second click triggers a flag.
		if (sticky_properties.is_excluded())
			sticky_properties.excluded = ns_death_time_annotation::ns_not_excluded;
		sticky_properties.flag.step_event();
		
	}
	static std::string pad_zeros(const std::string & str, int len){
		int add = len-(int)str.size();
		if (add <= 0)
			return str;
		std::string ret;
		for (int i = 0; i < add; i++){
			ret+='0';
		}
		return ret+str;
	}
	bool movement_quantification_data_loaded() {
		if (current_region_data == 0)
			return false;
		return current_region_data->movement_quantification_data_is_loaded();
	}
	void load_movement_analysis(ns_sql & sql) {
		if (current_region_data == 0)
			return;
		
		ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__, __LINE__);
		current_region_data->load_movement_analysis(current_region_data->metadata.region_id, sql, true);
		
		//we now need to update all fields that referred to the previous movement analysis object
		current_worm = &current_region_data->movement_analyzer[	properties_for_all_animals.stationary_path_id.group_id].paths[properties_for_all_animals.stationary_path_id.path_id]; 
	      
			for (unsigned int i = 0; i < timepoints.size(); i++){
				timepoints[i].path_timepoint_element = &current_worm->element(i);
				timepoints[i].movement_analyzer = & current_region_data->movement_analyzer;  //*
			       
			}
			for (unsigned int i = 0; i < previous_images.size(); i++) {
			  previous_images[i].loaded = false;
			  next_images[i].loaded = false;
			}
			
			ns_image_standard temp_buffer;
			timepoints[current_timepoint_id].load_image(1024, current_image, sql, temp_buffer, 1);
		 
			lock.release();
	}
	void output_worm_frames(const string &base_dir,const std::string & filename, ns_sql & sql){
		const string bd(base_dir);
		const string base_directory(bd + DIR_CHAR_STR + filename);
		ns_dir::create_directory_recursive(base_directory);
		const string gray_directory(base_directory + DIR_CHAR_STR + "gray");
		const string movement_directory(base_directory + DIR_CHAR_STR + "movement");
		ns_dir::create_directory_recursive(gray_directory);
		ns_dir::create_directory_recursive(movement_directory);
	
		current_region_data->load_movement_analysis(current_region_data->metadata.region_id,sql,true);
		current_worm = &current_region_data->movement_analyzer[properties_for_all_animals.stationary_path_id.group_id].paths[properties_for_all_animals.stationary_path_id.path_id];
		
		//add in by hand annotations so they are outputted correctly.
		std::vector<ns_death_time_annotation> by_hand_annotations;
		for (unsigned long i = 0; i < current_region_data->by_hand_timing_data.size(); i++){
				current_region_data->by_hand_timing_data[i].animals.begin()->generate_event_timing_data(by_hand_annotations);
				ns_death_time_annotation_compiler c;
				for (unsigned int j = 0; j < by_hand_annotations.size(); j++)
					c.add(by_hand_annotations[j],current_region_data->metadata);
				current_region_data->movement_analyzer.add_by_hand_annotations(c);
		}
		
		
		//we're reallocating all the elements so we need to link them in.
		for (unsigned int i = 0; i < timepoints.size(); i++){
				timepoints[i].path_timepoint_element = &current_worm->element(i);
		}
		const std::string outfile(base_directory + DIR_CHAR_STR + filename + "_movement_quantification.csv");
		ofstream o(outfile.c_str());
		current_region_data->movement_analyzer.group(properties_for_all_animals.stationary_path_id.group_id).paths.begin()->write_detailed_movement_quantification_analysis_header(o);
		o << "\n";
		current_region_data->movement_analyzer.write_detailed_movement_quantification_analysis_data(current_region_data->metadata,o,false,properties_for_all_animals.stationary_path_id.group_id);
		o.close();


	//	string base_filename(base_directory + DIR_CHAR_STR + filename);
		ns_image_standard mvt_tmp;
		current_region_data->load_images_for_worm(properties_for_all_animals.stationary_path_id,current_element_id()+1,sql);
		unsigned long num_dig = ceil(log10((double)this->current_element_id()));

		for (unsigned int i = 0; i <= this->current_element_id(); i++){
			if (timepoints[i].path_timepoint_element->excluded)
				continue;
			const ns_image_standard * im(current_region_data->movement_analyzer.group(properties_for_all_animals.stationary_path_id.group_id).paths[properties_for_all_animals.stationary_path_id.path_id].element(i).image_p());
			if (im == 0)
				continue;
			//ns_image_properties prop(im.properties());
			ns_save_image(gray_directory + DIR_CHAR_STR + filename + "_grayscale_" + pad_zeros(ns_to_string(i+1),num_dig) + ".tif",*im);
			current_region_data->movement_analyzer.group(properties_for_all_animals.stationary_path_id.group_id).paths[properties_for_all_animals.stationary_path_id.path_id].element(i).generate_movement_visualization(mvt_tmp);
			//ns_image_properties prop(im.properties()
			ns_save_image(movement_directory + DIR_CHAR_STR + filename +  "_" + pad_zeros(ns_to_string(i+1),num_dig) + ".tif",mvt_tmp);
		}
		
		
	}
	
	std::vector<ns_death_time_annotation> click_event_cache;
	

	void register_click(const ns_vector_2i & image_position, const ns_click_request & action);

	void display_current_frame();
	private:
	void update_events_to_storyboard(){
		properties_for_all_animals.annotation_time = ns_current_time();
		current_by_hand_timing_data().animals[current_animal_id].specified = true;
		click_event_cache.resize(0);
		for (unsigned int i = 0; i < current_by_hand_timing_data().animals.size(); i++){
			current_by_hand_timing_data().animals[i].generate_event_timing_data(click_event_cache);
			current_by_hand_timing_data().animals[i].specified = true;
		}
		ns_specify_worm_details(properties_for_all_animals.region_info_id,properties_for_all_animals.stationary_path_id,properties_for_all_animals,click_event_cache);
	}
		
	ns_alignment_type alignment_type;
};

#endif
