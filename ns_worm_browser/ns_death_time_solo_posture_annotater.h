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
#include "ns_annotation_handling_for_visualization.h"
void ns_hide_worm_window();

//must have worm browser storyboard lock before calling!
void ns_specify_worm_details(const ns_64_bit region_info_id,const ns_stationary_path_id & worm, const ns_death_time_annotation & sticky_properties, std::vector<ns_death_time_annotation> & event_times,double external_rescale_factor);

class ns_death_time_solo_posture_annotater_timepoint : public ns_annotater_timepoint{
public:
	typedef enum { ns_image, ns_movement, ns_movement_threshold, ns_movement_and_image, ns_movement_threshold_and_image } ns_visualization_type;
	
	static std::string visulazation_type_string(const ns_visualization_type & t) {
		switch (t) {
			case ns_image: return "brightfield image";
			case ns_movement: return "movement quantification";
			case ns_movement_threshold:return "thresholded movement quantification";
			case ns_movement_and_image: return "movement quantification, superimposed on brightfield image";
			case ns_movement_threshold_and_image: return "thresholded movement quantification, superimposed on brightfield image";
			default: throw ns_ex("ns_death_time_solo_posture_annotater_timepoint::visulazation_type_string()::Unknown visualization type");
		}
	}
	const ns_analyzed_image_time_path_element * path_timepoint_element;
	ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer> * movement_analyzer;
	unsigned long element_id;
	unsigned long group_id;
	ns_vector_2i image_pane_area;

	enum {ns_movement_bar_height=15,ns_bottom_border_height_minus_hand_bars=115,ns_side_border_width=4, ns_minimum_width = 350,ns_resolution_increase_factor=2};
	
	static int bottom_border_height(){return ((int)ns_bottom_border_height_minus_hand_bars + ns_movement_bar_height*(int)ns_death_time_annotation::maximum_number_of_worms_at_position);}


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
		const unsigned long side_border(ns_side_border_width*ns_resolution_increase_factor);
		//top border
		for (unsigned int y = 0; y < side_border; y++) {
			for (unsigned int x = 0; x < 3 * output.properties().width; x++) {
				output[y][x] = 0;
			}
		}
		for (unsigned int y = 0; y < ns_resolution_increase_factor*image.properties().height; y+= ns_resolution_increase_factor) {

			//left border
			for (unsigned int x = 0; x < side_border; x++) {
				output[y + side_border][3 * x] =
					output[y + side_border][3 * x + 1] =
					output[y + side_border][3 * x + 2] = 0;
			}
			//middle
			switch (vis_type) {
			case ns_image:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					const ns_8_bit a = image[y / ns_resolution_increase_factor][x];
					for (unsigned int xx = 0; xx < ns_resolution_increase_factor; xx++)
					output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] =
						output[y + side_border][3 * (ns_resolution_increase_factor*x + xx+ side_border) + 1] =
						output[y + side_border][3 * (ns_resolution_increase_factor*x + xx+ side_border) + 2] = a;
				}
				break;
			case ns_movement:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					ns_8_bit t;
					if (registered_images.get_stabilized_worm_neighborhood_threshold(y / ns_resolution_increase_factor, x)) {
						long sum, count;

						ns_analyzed_image_time_path::spatially_average_movement(y / ns_resolution_increase_factor, x, ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);

						if (count == 0) t = 0;
						else {
							t = 255 * ((abs(sum / count) - min_mov) / mov_r);
						}
					}
					else t = 0;

					for (unsigned int xx = 0; xx < ns_resolution_increase_factor; xx++) {
						output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] =
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] =
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = t;

						if (registered_images.get_stabilized_worm_neighborhood_threshold_edge(y / ns_resolution_increase_factor, x)) {
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] = 125;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] = 125;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = 125;
						}
					}
				}
				break;
			case ns_movement_threshold:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					ns_8_bit t;
					if (registered_images.get_stabilized_worm_neighborhood_threshold(y / ns_resolution_increase_factor, x)) {
						long sum, count;
						ns_analyzed_image_time_path::spatially_average_movement(y / ns_resolution_increase_factor, x, ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);

						if (abs(sum) < count*ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>::ns_spatially_averaged_movement_threshold)
							t = 0;
						else {
							t = 255 * ((abs(sum / count) - min_mov) / mov_r);
						}
					}
					else t = 0;

					for (unsigned int xx = 0; xx < ns_resolution_increase_factor; xx++) {
						output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] =
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] =
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = t;

						if (registered_images.get_stabilized_worm_neighborhood_threshold_edge(y / ns_resolution_increase_factor, x)) {
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] = 125;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] = 125;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = 125;
						}
					}
				}
				break;
			case ns_movement_and_image:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					float f;
					if (registered_images.get_stabilized_worm_neighborhood_threshold(y/ ns_resolution_increase_factor, x)) {
						long sum, count;
						ns_analyzed_image_time_path::spatially_average_movement(y/ ns_resolution_increase_factor, x, ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);

						if (count == 0)
							f = 0;
						else {
							f = ((abs(sum / count) - min_mov) / mov_r);
						}
					}
					else f = 0;

					const int r = image[y / ns_resolution_increase_factor][x] * (1 - f) + 255 * f,  //goes up to 255 the more movement there is
						bg = image[y / ns_resolution_increase_factor][x] * (1 - f);  //goes down to zero the more movement there is.
					for (unsigned int xx = 0; xx < ns_resolution_increase_factor; xx++) {
						output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] = r;
						output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] =
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = bg;

						if (registered_images.get_stabilized_worm_neighborhood_threshold_edge(y / ns_resolution_increase_factor, x)) {
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] = 125;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] = 0;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = 0;
						}
					}
				}
				break;
			case ns_movement_threshold_and_image:
				for (unsigned int x = 0; x < image.properties().width; x++) {
					float f;
					if (registered_images.get_stabilized_worm_neighborhood_threshold(y/ ns_resolution_increase_factor, x)) {
						long sum, count;
						ns_analyzed_image_time_path::spatially_average_movement(y/ ns_resolution_increase_factor, x, ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>::ns_spatially_averaged_movement_kernal_half_size, movement_image, sum, count);
						if (count == 0 || abs(sum) < count* ns_time_path_image_movement_analyzer<ns_wasteful_overallocation_resizer>::ns_spatially_averaged_movement_threshold)
							f = 0;
						else {
							f = ((abs(sum / count) - min_mov) / mov_r);
						}
					}
					else f = 0;

					int r = image[y / ns_resolution_increase_factor][x] * (1 - f) + 255 * f,  //goes up to 255 the more movement there is
						bg = image[y / ns_resolution_increase_factor][x] * (1 - f);  //goes down to zero the more movement there is.
					for (unsigned int xx = 0; xx < ns_resolution_increase_factor; xx++) {
						output[y + side_border][3 * (ns_resolution_increase_factor*x+xx + side_border)] = r;
						output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] =
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = bg;

						if (registered_images.get_stabilized_worm_neighborhood_threshold_edge(y / ns_resolution_increase_factor, x)) {
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border)] = 125;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 1] = 0;
							output[y + side_border][3 * (ns_resolution_increase_factor*x + xx + side_border) + 2] = 0;
						}
					}
				}
				break;

			}
			//right border
			for (unsigned int x = ns_resolution_increase_factor*image.properties().width; x < output.properties().width; x++) {
				output[y+ side_border][3 * x] =
					output[y + side_border][3 * x + 1] =
					output[y + side_border][3 * x + 2] = 0;
			}

			//now copy repeated lines
			for (unsigned int yy = 1; yy < ns_resolution_increase_factor; yy++) {
				for (unsigned int x = 0; x < 3*output.properties().width; x++) 
					output[y + side_border + yy][x] = output[y + side_border][x];
			}
		}

		for (unsigned int y = ns_resolution_increase_factor*image.properties().height + side_border; y < output.properties().height; y++) {
			for (unsigned int x = 0; x < output.properties().width; x++) {
				output[y][3 * x] =
					output[y][3 * x + 1] =
					output[y][3 * x + 2] = 0;
			}
		}
		//cerr << movement_sum << ")\n";

	}
	
	void load_image(const unsigned long buffer_height,ns_annotater_image_buffer_entry & im,
		ns_sql & sql,
		ns_simple_local_image_cache & image_cache,
		ns_annotater_memory_pool & memory_pool, const unsigned long resize_factor_=1){
		//all images are being loaded asynchronosuly.  Wait until the requested element is loaded
		movement_analyzer->wait_until_element_is_loaded(group_id, element_id);
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
		prop.width+=2* ns_side_border_width;
		prop.height+=bottom_border_height();
		prop.width *= ns_resolution_increase_factor;
		prop.height *= ns_resolution_increase_factor;
		prop.components = 3;
		if (im.im == 0) {
			im.im = memory_pool.get(prop);
			im.im->use_more_memory_to_avoid_reallocations(true);
		}
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
struct ns_death_time_posture_solo_annotater_data_cache_data_source {
	ns_death_time_posture_solo_annotater_data_cache_data_source() :sql(0), memory_pool(0){}
	ns_death_time_posture_solo_annotater_data_cache_data_source(ns_sql * s, ns_time_path_image_movement_analysis_memory_pool<ns_wasteful_overallocation_resizer> * pool) :sql(s), memory_pool(pool) {}
	ns_sql * sql;
	ns_time_path_image_movement_analysis_memory_pool<ns_wasteful_overallocation_resizer> * memory_pool;
};

struct ns_death_time_posture_solo_annotater_data_cache_data_spec {
	ns_death_time_posture_solo_annotater_data_cache_data_spec() :region_id(0), load_movement_quantification_data(false) {}
	ns_death_time_posture_solo_annotater_data_cache_data_spec(ns_64_bit id, bool r) :region_id(id), load_movement_quantification_data(r) {}
	ns_64_bit region_id;
	bool load_movement_quantification_data;
};

template<class ns_component>
class ns_death_time_posture_solo_annotater_data_cache_data : public ns_simple_cache_data<ns_death_time_posture_solo_annotater_data_cache_data_spec, ns_death_time_posture_solo_annotater_data_cache_data_source, ns_64_bit> {
	ns_death_time_posture_solo_annotater_data_cache_data_spec region_info_int;
public:
	ns_death_time_posture_solo_annotater_data_cache_data() :data(0) {}
	ns_image_storage_source_handle<ns_component> source;
	ns_image_server_image image_record;
	ns_death_time_posture_solo_annotater_region_data *data;
	ns_64_bit size_in_memory_in_kbytes() const {
		return 1;
	}
	void load_from_external_source(const ns_death_time_posture_solo_annotater_data_cache_data_spec & region_info, ns_death_time_posture_solo_annotater_data_cache_data_source & source_) {
		region_info_int = region_info;
		data = new ns_death_time_posture_solo_annotater_region_data(*source_.memory_pool);
		data->metadata.load_from_db(region_info.region_id, "", *source_.sql);
		ns_image_server_results_subject sub;
		sub.region_id = region_info.region_id;
		data->loading_time = ns_current_time();
		data->annotation_file = image_server.results_storage.hand_curated_death_times(sub, *source_.sql);
		//this will work even if no path data can be loaded, but 
		//p->loaded_path_data_successfully will be set to false

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading movement quantification data"));
		data->load_movement_analysis(region_info.region_id, *source_.sql, region_info.load_movement_quantification_data);

		ns_timing_data_configurator configurator;
		configurator(region_info.region_id, data->movement_analyzer, data->machine_timing_data, data->by_hand_timing_data);

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading annotations 2"));
		data->load_annotations(*source_.sql, false);
	}
	void clean_up(ns_death_time_posture_solo_annotater_data_cache_data_source & source) { 
		ns_safe_delete(data); 
	}

	const ns_64_bit & id() const { return region_info_int.region_id; }
	static ns_64_bit to_id(const ns_death_time_posture_solo_annotater_data_cache_data_spec & region_info) { return region_info.region_id; }
};

typedef ns_simple_cache< ns_death_time_posture_solo_annotater_data_cache_data<ns_8_bit>, ns_64_bit,true> ns_death_time_posture_solo_annotater_data_cache_storage;


class ns_death_time_posture_solo_annotater_data_cache{
	ns_death_time_posture_solo_annotater_data_cache_storage cache;
	//ns_death_time_posture_solo_annotater_region_data data;

public:
	ns_death_time_posture_solo_annotater_data_cache():cache(2)	//allow two regions to be loaded simultaneously.
	{}
	void clear(){
		ns_death_time_posture_solo_annotater_data_cache_data_source source(0, &memory_pool);
		cache.clear_cache(source);
	}

	bool is_loaded(const ns_64_bit region_id) {
		ns_death_time_posture_solo_annotater_data_cache_data_spec spec(region_id, false);
		return cache.is_cached(spec);
	}
	void get_region_movement_data(const ns_64_bit region_id, ns_sql & sql, const bool load_movement_quantification_data, ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) {

		ns_death_time_posture_solo_annotater_data_cache_data_source source(&sql, &memory_pool);
		ns_death_time_posture_solo_annotater_data_cache_data_spec spec(region_id, load_movement_quantification_data);
		//cerr << "Loading get\n";
		cache.get_for_write(spec, handle, source);
	}
	void get_region_movement_data_no_create(const ns_64_bit region_id,  ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle){
		ns_death_time_posture_solo_annotater_data_cache_data_spec spec(region_id, false);
		//cerr << "no create get\n";
		cache.get_for_write_no_create(spec, handle);
	}
	void get_region_movement_data_no_create(const ns_64_bit region_id, ns_death_time_posture_solo_annotater_data_cache_storage::const_handle_t & handle) {
		ns_death_time_posture_solo_annotater_data_cache_data_spec spec(region_id, false);
		//cerr << "no create const get\n";
		cache.get_for_read_no_create(spec, handle);
	}

	
	void add_annotations_to_set(ns_death_time_annotation_set & set, ns_sql & sql,std::vector<ns_death_time_annotation> & orphaned_events){

		ns_death_time_posture_solo_annotater_data_cache_data_source source(&sql, &memory_pool);

		ns_64_bit cur_region_id = 0;
		bool first = true;
		//go through region by region adding everything in.
		ns_death_time_posture_solo_annotater_data_cache_storage::handle_t handle;
		for (auto p = set.begin(); p != set.end(); p++) {
			if (first || p->region_info_id != cur_region_id) {
				if (!first)
					handle.release();
				first = false;
				cur_region_id = p->region_info_id;
				ns_death_time_posture_solo_annotater_data_cache_data_spec spec(cur_region_id, false);
				cache.get_for_write(spec, handle, source);
			}
			handle().data->add_annotations_to_set(set, orphaned_events);
		}
		if (!first)
			handle.release();
	}
	bool save_annotations(const ns_death_time_annotation_set & a){
		throw ns_ex("Solo posture annotater annotations should be mixed in with storyboard");
	}
	ns_time_path_image_movement_analysis_memory_pool<ns_wasteful_overallocation_resizer> memory_pool;
};

class ns_worm_learner;
class ns_death_time_solo_posture_annotater : public ns_image_series_annotater {
private:

	ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type current_visualization_type;
	std::vector<ns_death_time_solo_posture_annotater_timepoint> timepoints;
	static ns_death_time_posture_solo_annotater_data_cache data_cache;

	ns_analyzed_image_time_path * current_worm(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) {
		if (current_region_id == 0)
			return 0;
		return &handle().data->movement_analyzer[current_worm_id.group_id].paths[current_worm_id.path_id];
	}
	const ns_analyzed_image_time_path * current_worm(ns_death_time_posture_solo_annotater_data_cache_storage::const_handle_t & handle) const {
		if (current_region_id == 0)
			return 0;
		return &handle().data->movement_analyzer[current_worm_id.group_id].paths[current_worm_id.path_id];
	}

	const ns_analyzed_image_time_path * current_worm(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) const {
		if (current_region_id == 0)
			return 0;
		return &handle().data->movement_analyzer[current_worm_id.group_id].paths[current_worm_id.path_id];
	}
	ns_death_time_annotation properties_for_all_animals;

	unsigned long current_element_id()const { return current_timepoint_id; }
	ns_death_time_annotation_time_interval current_time_interval(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) {
		ns_analyzed_image_time_path * cur_worm = current_worm(handle);

		if (current_timepoint_id == 0)
			return cur_worm->observation_limits().interval_before_first_observation;
		return ns_death_time_annotation_time_interval(cur_worm->element(current_timepoint_id - 1).absolute_time,
			cur_worm->element(current_timepoint_id).absolute_time);

	}
	ns_animal_list_at_position * current_by_hand_timing_data(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) {
		return & handle().data->by_hand_timing_data[properties_for_all_animals.stationary_path_id.group_id]; 
	}
	ns_animal_list_at_position * current_machine_timing_data(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) {
		return & handle().data->machine_timing_data[current_worm_id.group_id];
	}
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
		case ns_movement_death_associated_expansion: return "Death-Associated Expansion";
		case ns_movement_death_associated_post_expansion_contraction: return "Death-Associated post-expansion Contraction";
		default: return std::string("Unknown:") + ns_to_string((int)cur_state);
		}
	}
	enum { bottom_offset = 5, movement_vis_bar_height = 10 * ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor};
	void draw_metadata(ns_annotater_timepoint * tp_a, ns_image_standard & im, double external_window_rescale_factor) {

		ns_death_time_posture_solo_annotater_data_cache_storage::handle_t local_handle;
		data_cache.get_region_movement_data_no_create(current_region_id, local_handle);
		draw_metadata(tp_a, im, local_handle, external_window_rescale_factor);
		local_handle.release();
		
	}
	void draw_metadata(ns_annotater_timepoint * tp_a, ns_image_standard & im, ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle,double external_window_rescale_factor) {
		
		ns_analyzed_image_time_path * cur_worm = current_worm(handle);

		if (cur_worm == 0) {
			cerr << "worm not loaded/n";
			return;
		}
		if (current_element_id() >= cur_worm->element_count()) {
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
		font.set_height(text_height*ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor);
		ns_vector_2i bottom_margin_bottom(bottom_margin_position(&handle));
		ns_vector_2i bottom_margin_top(bottom_margin_bottom.x + cur_worm->element(current_element_id()).image().properties().width + ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width,
			bottom_margin_bottom.y + ns_death_time_solo_posture_annotater_timepoint::bottom_border_height() - bottom_offset);


		bottom_margin_bottom *= ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor;
		bottom_margin_top *= ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor;

		for (unsigned int y = bottom_margin_bottom.y; y < bottom_margin_top.y; y++)
			for (unsigned int x = bottom_margin_bottom.x; x < bottom_margin_top.x; x++) {
	
				im[y][3 * x] =
					im[y][3 * x + 1] =
					im[y][3 * x + 2] = 0;
			}


		
		const ns_time_path_limits observation_limit(cur_worm->observation_limits());
		ns_death_time_annotation_time_interval first_path_obs, last_path_obs;
		last_path_obs.period_end = cur_worm->element(cur_worm->element_count() - 1).absolute_time;
		last_path_obs.period_start = cur_worm->element(cur_worm->element_count() - 2).absolute_time;
		first_path_obs.period_start = cur_worm->element(0).absolute_time;
		first_path_obs.period_end = cur_worm->element(1).absolute_time;

		ns_animal_list_at_position * cur_hand_timing(current_by_hand_timing_data(handle));
		ns_animal_list_at_position * cur_machine_timing(current_machine_timing_data(handle));
		//handle out of bound values
		for (ns_animal_list_at_position::ns_animal_list::iterator p = cur_hand_timing->animals.begin(); p != cur_hand_timing->animals.end(); p++) {
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->fast_movement_cessation.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->death_associated_expansion_stop.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->death_associated_expansion_start.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->death_associated_post_expansion_contraction_stop.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->death_associated_post_expansion_contraction_start.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->movement_cessation.time);
			ns_crop_time(observation_limit, first_path_obs, last_path_obs, p->translation_cessation.time);
		}

		const unsigned long hand_bar_height(cur_hand_timing->animals.size() *
			ns_death_time_solo_posture_annotater_timepoint::ns_movement_bar_height);

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing timing."));
		try {
			cur_machine_timing->animals[0].draw_movement_diagram(bottom_margin_bottom,
				ns_vector_2i(cur_worm->element(current_element_id()).image().properties().width * ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor, movement_vis_bar_height),
				observation_limit,
				current_time_interval(handle),
				im, 0.8, ceil(1 / external_window_rescale_factor));
			for (unsigned int i = 0; i < cur_hand_timing->animals.size(); i++) {
				cur_hand_timing->animals[i].draw_movement_diagram(bottom_margin_bottom + (
					ns_vector_2i(0, (i + 1) * ns_death_time_solo_posture_annotater_timepoint::ns_movement_bar_height)) * ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor,
					ns_vector_2i(cur_worm->element(current_element_id()).image().properties().width * ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor, movement_vis_bar_height),
					observation_limit,
					current_time_interval(handle),
					im, (i == current_animal_id) ? 1.0 : 0.8, ceil(1 / external_window_rescale_factor));
			}
			//throw ns_ex("TEST");
		}
		catch (ns_ex & ex) {

			cout << ex.text() << "\n";
			/*class ns_choice_dialog dialog;
			dialog.title = "An error is present in this animal's data:";
			dialog.option_1 = "Ignore";
			dialog.option_2 = "Clear all annotations";
			ns_run_in_main_thread<ns_choice_dialog> b(&dialog);
			switch (dialog.result) {
			case 1:
				break;
			case 2:*/
			cur_hand_timing->animals.resize(1);
			current_animal_id = 0;
			cur_hand_timing->animals[0].clear_annotations();

			cur_hand_timing->animals[0].set_fast_movement_cessation_time(
				ns_death_timing_data_step_event_specification(cur_worm->cessation_of_fast_movement_interval(),
					cur_worm->element(cur_worm->first_stationary_timepoint()),
					properties_for_all_animals.region_info_id, properties_for_all_animals.stationary_path_id, 0));
				//	}

		}

		const int num_lines(6);
		std::string lines[num_lines];
		ns_color_8 line_color[num_lines];
		const ns_movement_state cur_by_hand_state(cur_hand_timing->animals[current_animal_id].movement_state(cur_worm->element(current_element_id()).absolute_time));
		const ns_movement_state cur_machine_state(cur_machine_timing->animals[0].movement_state(cur_worm->element(current_element_id()).absolute_time));
		const bool by_hand_death_expanding(cur_hand_timing->animals[current_animal_id].is_death_time_expanding(cur_worm->element(current_element_id()).absolute_time));
		const bool machine_death_expanding(cur_machine_timing->animals[0].is_death_time_expanding(cur_worm->element(current_element_id()).absolute_time));
		const bool by_hand_death_post_expansion_contracting(cur_hand_timing->animals[current_animal_id].is_death_time_post_expansion_contracting(cur_worm->element(current_element_id()).absolute_time));
		const bool machine_death_post_expansion_contracting(cur_machine_timing->animals[0].is_death_time_post_expansion_contracting(cur_worm->element(current_element_id()).absolute_time));
		lines[0] = "Frame " + ns_to_string(current_element_id() + 1) + " of " + ns_to_string(timepoints.size()) + " ";
		lines[0] += cur_worm->element(current_element_id()).element_before_fast_movement_cessation ? "(B)" : "(A)";
		lines[0] += cur_worm->element(current_element_id()).inferred_animal_location ? "(I)" : "";
		if (handle().data->metadata.time_at_which_animals_had_zero_age != 0) {
			lines[0] += "Day ";
			lines[0] += ns_to_string_short((float)
				(cur_worm->element(current_element_id()).absolute_time -
					handle().data->metadata.time_at_which_animals_had_zero_age) / 24.0 / 60 / 60, 2);
			lines[0] += " ";
		}
		lines[0] += "Date: ";
		lines[0] += ns_format_time_string_for_human(cur_worm->element(current_element_id()).absolute_time);

		lines[1] = (handle().data->metadata.plate_type_summary());


		lines[2] = handle().data->metadata.sample_name + "::" + handle().data->metadata.region_name + "::worm #" + ns_to_string(properties_for_all_animals.stationary_path_id.group_id);

		ns_vector_2i p(cur_worm->path_region_position + cur_worm->path_region_size / 2);
		lines[2] += " (" + ns_to_string(p.x) + "," + ns_to_string(p.y) + ")";

		lines[3] = "Machine:" + state_label(cur_machine_state) + (machine_death_expanding ? "(Expanding)" : "") + (machine_death_post_expansion_contracting ? "(Contracting)" : "");
		if (cur_hand_timing->animals[current_animal_id].specified)
			lines[4] += "Human: " + state_label(cur_by_hand_state) + (by_hand_death_expanding ? "(Expanding)" : "") + (by_hand_death_post_expansion_contracting ? "(Contracting)" : "");
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

		ns_vector_2i text_pos(bottom_margin_bottom.x, bottom_margin_bottom.y + (hand_bar_height + 15 + text_height)*ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor);
		for (unsigned int i = 0; i < num_lines; i++)
			font.draw_color(text_pos.x, text_pos.y + i*(text_height + 1)*ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor, line_color[i], lines[i], im);
		
		lock.release();
	}

	ns_worm_learner * worm_learner;

	mutable bool saved_;

public:

	enum { default_resize_factor = 1, max_buffer_size = 15, max_zoom_factor = 40 };
	float telemetry_zoom_factor;
	ns_vector_2i telemetry_size() { return ns_vector_2i(500, 500)*ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor; }

	std::string graph_type_string() const {
		return ns_animal_telemetry::graph_type_string(graph_contents);
	}
	ns_animal_telemetry::ns_graph_contents step_graph_type() {
		graph_contents = (ns_animal_telemetry::ns_graph_contents)((int)graph_contents + 1);
		if (graph_contents == ns_animal_telemetry::ns_all) {
			graph_contents = ns_animal_telemetry::ns_none;
			telemetry.show(false);
		}
		else
			telemetry.show(true);
		request_refresh();
		return graph_contents;
	}

	ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type step_visualization_type(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t * handle,double external_rescale_factor) {

		ns_death_time_posture_solo_annotater_data_cache_storage::handle_t local_handle;
		if (handle == 0){
			data_cache.get_region_movement_data_no_create(current_region_id, local_handle);
			handle = &local_handle;
		}

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

		timepoints[current_timepoint_id].load_image(1024, current_image, sql(),local_image_cache, memory_pool, 1);

		draw_metadata(&timepoints[current_timepoint_id], *current_image.im, *handle, external_rescale_factor);
		lock.release();
		return current_visualization_type;
	}

	ns_animal_telemetry telemetry;
	ns_animal_telemetry::ns_graph_contents graph_contents;

	typedef enum { ns_none, ns_forward, ns_back, ns_fast_forward, ns_fast_back, ns_stop, ns_save, ns_rewind_to_zero, ns_write_quantification_to_disk, ns_step_visualization, ns_step_graph, ns_time_zoom_in_step,ns_time_zoom_out_step, ns_number_of_annotater_actions } ns_image_series_annotater_action;

	inline ns_annotater_timepoint * timepoint(const unsigned long i) { return &timepoints[i]; }
	inline unsigned long number_of_timepoints() { return timepoints.size(); }

	void set_resize_factor(const unsigned long resize_factor_) { resize_factor = resize_factor_; }
	bool data_saved()const { return saved_; }
	ns_death_time_solo_posture_annotater() :ns_image_series_annotater(default_resize_factor, ns_death_time_posture_annotater_timepoint::ns_bottom_border_height),
		saved_(true), graph_contents(ns_animal_telemetry::ns_movement_intensity), worm_image_offset_due_to_telemetry_graph_spacing(0,0),current_visualization_type(ns_death_time_solo_posture_annotater_timepoint::ns_image),
		region_loaded(false), telemetry_zoom_factor(1), current_region_id(0){	}
	bool region_loaded;
	typedef enum { ns_time_aligned_images, ns_death_aligned_images } ns_alignment_type;

	bool load_annotations() {
		//	for (u
		saved_ = true;
		//	saved_ = !fix_censored_events_with_no_time_specified();
		return true;
	}

	void add_annotations_to_set(ns_death_time_annotation_set & set, std::vector<ns_death_time_annotation> & orphaned_events) {
		data_cache.add_annotations_to_set(set, sql(),orphaned_events);
	}

	void save_annotations(const ns_death_time_annotation_set & set) const {
		throw ns_ex("This shouldn't run");
		data_cache.save_annotations(set);

		ns_update_worm_information_bar(string("Annotations saved at ") + ns_format_time_string_for_human(ns_current_time()));
		ns_update_main_information_bar(string("Annotations saved at ") + ns_format_time_string_for_human(ns_current_time()));
		saved_ = true;
	};
	bool current_region_path_data_loaded(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) {
		if (current_region_id == 0)
			return 0;
		return handle().data->loaded_path_data_successfully;
	}
	void close_worm() {
		if (current_region_id == 0 || !data_cache.is_loaded(current_region_id)) {
			current_region_id = 0;
			return;
		}
		ns_death_time_posture_solo_annotater_data_cache_storage::handle_t handle;
		try{
			data_cache.get_region_movement_data_no_create(current_region_id, handle);
				}
		catch(ns_ex & ex){
		  //cerr << ex.text() << "\n";
		}
		try {
			if (handle.is_valid()) {
				handle().data->movement_analyzer.stop_asynch_group_load();
				handle().data->clear_images_for_worm(properties_for_all_animals.stationary_path_id, local_image_cache);
			}

		}
		catch (ns_ex & ex) {
			cerr << ex.text() << "\n";
		}
		properties_for_all_animals = ns_death_time_annotation();
		properties_for_all_animals.region_info_id = 0;
		current_worm_id.group_id = 0;
		current_worm_id.path_id = 0;
		current_timepoint_id = 0;
		current_animal_id = 0;
	}
	void clear_data_cache() {
		data_cache.clear();
	}

	static bool ns_fix_annotation(ns_death_time_annotation & a,ns_analyzed_image_time_path & p);
	void set_current_timepoint(const unsigned long current_time, ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle, const bool require_exact=true,const bool give_up_if_too_long=false){
		auto cur_worm = current_worm(handle);
		for (current_timepoint_id = 0; current_timepoint_id < cur_worm->element_count(); current_timepoint_id++){
				if (cur_worm->element(current_timepoint_id).absolute_time >= current_time)
					break;
			}	
		if (current_timepoint_id >= cur_worm->element_count())
			current_timepoint_id = cur_worm->element_count() - 1;
		bool give_up_on_preload(false);
		if (current_timepoint_id > 50 && give_up_if_too_long){
			give_up_on_preload = true;
			current_timepoint_id = 0;
			for (unsigned int k = 0; k < cur_worm->element_count(); k++)
				if (!cur_worm->element(k).element_before_fast_movement_cessation){
					current_timepoint_id = k;
					break;
				}
		}

		//load images for the worm.

		//data_cache.load_images_for_worm(properties_for_all_animals.stationary_path_id,current_timepoint_id+10,sql);
		//current_region_data->clear_images_for_worm(properties_for_all_animals.stationary_path_id,local_image_cache);

		handle().data->load_images_for_worm(properties_for_all_animals.stationary_path_id,current_timepoint_id+10,sql(),local_image_cache);


		if (require_exact && cur_worm->element(current_timepoint_id).absolute_time != current_time && !give_up_on_preload)
			throw ns_ex("ns_death_time_solo_posture_annotater::set_current_timepoint()::Could not find requested element time:") << current_time;
		if (current_timepoint_id+1 < cur_worm->element_count())
			current_timepoint_id++;

	}

	unsigned long last_time_at_current_telementry_zoom(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) const;
	void draw_telemetry(const ns_vector_2i & position, const ns_vector_2i & graph_size, const ns_vector_2i & buffer_size, const float rescale_factor,ns_8_bit * buffer);
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

	ns_vector_2i bottom_margin_position(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t * handle) {

		ns_death_time_posture_solo_annotater_data_cache_storage::const_handle_t local_handle;
		const ns_analyzed_image_time_path * cur_worm(0);
		if (handle != 0)
			cur_worm = current_worm(*handle);
		else {
			data_cache.get_region_movement_data_no_create(current_region_id, local_handle);
			cur_worm = current_worm(local_handle);
		}
	

	  if (cur_worm->element_count() <= current_element_id()){
	    cerr << "Worm/element not loaded";
	    return ns_vector_2d(0,0);
	    }
	if (!cur_worm->element(current_element_id()).registered_image_is_loaded()) {
			cerr << "No image is loaded for current element;";
			return ns_vector_2i(0, 0);
		}
		return ns_vector_2i(ns_death_time_solo_posture_annotater_timepoint::ns_side_border_width,
			cur_worm->element(current_element_id()).image().properties().height + bottom_offset);
	}

	void precache_images(const ns_64_bit region_info_id, const ns_stationary_path_id & worm,ns_sql & sql) {
		ns_death_time_posture_solo_annotater_data_cache_storage::handle_t handle;
		data_cache.get_region_movement_data(region_info_id, sql, true, handle);
		handle().data->movement_analyzer.precache_group_images_locally(worm.group_id, worm.path_id, &handle,sql,false);

	}
	ns_64_bit current_region_id;
	ns_stationary_path_id current_worm_id;
	
	void load_worm(const ns_64_bit region_info_id_, const ns_stationary_path_id& worm, const unsigned long current_time, const ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type visualization_type, const ns_experiment_storyboard* storyboard, ns_worm_learner* worm_learner_, const double external_rescale_factor) {

		if (sql.is_null()) {

			if (image_server.verbose_debug_output())
				image_server_const.register_server_event_no_db(ns_image_server_event("Connecting to sql db"));
			sql.attach(image_server.new_sql_connection(__FILE__, __LINE__));
		}
		if (asynch_loading_sql.is_null()) {
			if (image_server.verbose_debug_output())
				image_server_const.register_server_event_no_db(ns_image_server_event("Connecting to sql db 2"));
			asynch_loading_sql.attach(image_server.new_sql_connection(__FILE__, __LINE__));
		}
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Clearing self."));
		clear();
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Clearing annotator."));
	    ns_image_series_annotater::clear();
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Closing previous worm."));
		this->close_worm();
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Stopping movement."));
		stop_fast_movement();
		current_visualization_type = visualization_type;
		
	
		telemetry.show(graph_contents != ns_animal_telemetry::ns_none);
	
		ns_region_metadata metadata;
		metadata.region_id = region_info_id_;
		current_region_id = region_info_id_; 
		current_worm_id = worm;
		try{

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Finding Animal"));
			const ns_experiment_storyboard_timepoint_element & e(storyboard->find_animal(region_info_id_,worm));
			ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__,__LINE__);
			worm_learner = worm_learner_;

			
			ns_death_time_posture_solo_annotater_data_cache_storage::handle_t handle;
			try {

				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading movement data"));
				data_cache.get_region_movement_data(region_info_id_, sql(),telemetry.show(),handle);

				//stop any previously started image loading
				handle().data->movement_analyzer.stop_asynch_group_load();

				if (!handle().data->loaded_path_data_successfully)
					throw ns_ex("The movement anaylsis data required to inspect this worm is no longer in the database.  You might need to re-analyze movement for this region.");

			}
			catch (...) {
				metadata.load_from_db(region_info_id_, "", sql());
				region_loaded = false;

				throw;
			}
			timepoints.resize(0);
			if (region_loaded) {
				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Clearing images"));
				handle().data->clear_images_for_worm(properties_for_all_animals.stationary_path_id,local_image_cache);
			}
			region_loaded = true;
	
			properties_for_all_animals.region_info_id = region_info_id_;
			//get movement information for current worm
		
			
			metadata = handle().data->metadata;
			if (worm.detection_set_id != 0 && handle().data->movement_analyzer.db_analysis_id() != worm.detection_set_id)
				throw ns_ex("This storyboard was built using an out-of-date movement analysis result.  Please rebuild the storyboard, so that it will reflect the most recent analysis.");
			if (handle().data->movement_analyzer.size() <= worm.group_id)
				throw ns_ex("Looking at the movement analysis result for this region, this worm's ID appears to be invalid.");
			if (worm.path_id != 0)
				throw ns_ex("The specified group has multiple paths! This should be impossible");
			
			ns_analyzed_image_time_path * current_worm = &handle().data->movement_analyzer[worm.group_id].paths[worm.path_id];
		       
			current_animal_id = 0;
			

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading posture analysis model"));
			ns_image_server::ns_posture_analysis_model_cache::const_handle_t model_handle;
			image_server.get_posture_analysis_model_for_region(region_info_id_, model_handle, sql());
			ns_posture_analysis_model mod(model_handle().model_specification);
			mod.threshold_parameters.use_v1_movement_score = false;
			model_handle.release();
			
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting animal for telemetry"));
			telemetry.set_current_animal(worm.group_id,mod, handle().data);



			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting fast movement cessation times and adding annotations"));
			for (unsigned int i = 0; i < e.by_hand_movement_annotations_for_element.size(); i++){
				unsigned long animal_id = e.by_hand_movement_annotations_for_element[i].annotation.animal_id_at_position;
				const unsigned long current_number_of_animals(handle().data->by_hand_timing_data[worm.group_id].animals.size());
				if (animal_id >= current_number_of_animals){
					handle().data->by_hand_timing_data[worm.group_id].animals.resize(animal_id+1);
					for (unsigned int i = current_number_of_animals; i < animal_id; i++){
						handle().data->by_hand_timing_data[worm.group_id].animals[i].set_fast_movement_cessation_time(
							ns_death_timing_data_step_event_specification(current_worm->cessation_of_fast_movement_interval(),
							current_worm->element(current_worm->first_stationary_timepoint()),
							properties_for_all_animals.region_info_id,properties_for_all_animals.stationary_path_id,i));
					}
					
				}
				handle().data->by_hand_timing_data[worm.group_id].animals[animal_id].add_annotation(e.by_hand_movement_annotations_for_element[i].annotation,true);
				//if (animal_id == 0)
				//if (e.by_hand_movement_annotations_for_element[i].type == ns_additional_worm_entry)
				//	curre nt_region_data->by_hand_timing_data[worm.group_id].animals[animal_id].first_frame_time = e.by_hand_movement_annotations_for_element[i].time.end_time;
				//current_region_data->by_hand_timing_data[worm.group_id].animals[animal_id].add_annotation(e.by_hand_movement_annotations_for_element[i],true);
			}
			
			//current_region_data->metadata
			//current_by_hand_timing_data = &current_region_data->by_hand_timing_data[worm.group_id];
			//xxxx
			if (worm.group_id >= handle().data->machine_timing_data.size())
			throw ns_ex("Group ID ") << worm.group_id << " is not present in machine timing data (" << handle().data->machine_timing_data.size() << ") for region " << region_info_id_;
			current_animal_id = 0;

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Transferring sticky properties"));
			//current_by_hand_timing_data->animals[current_animal_id].sticky_properties = current_machine_timing_data->animals[0].sticky_properties;
			e.event_annotation.transfer_sticky_properties(properties_for_all_animals);
			//current_by_hand_timing_data->animals[current_animal_id].sticky_propertes.annotation_source = ns_death_time_annotation::ns_posture_image;
			properties_for_all_animals.stationary_path_id = worm;
			this->saved_ = true;
			for (unsigned int j = 0; j < handle().data->by_hand_timing_data[worm.group_id].animals.size(); j++){
				if (ns_fix_annotation(handle().data->by_hand_timing_data[worm.group_id].animals[j].fast_movement_cessation,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(handle().data->by_hand_timing_data[worm.group_id].animals[j].translation_cessation,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(handle().data->by_hand_timing_data[worm.group_id].animals[j].movement_cessation,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(handle().data->by_hand_timing_data[worm.group_id].animals[j].death_associated_expansion_stop,*current_worm))
					saved_ = false;
				if (ns_fix_annotation(handle().data->by_hand_timing_data[worm.group_id].animals[j].death_associated_expansion_start, *current_worm))
					saved_ = false; 
				if (ns_fix_annotation(handle().data->by_hand_timing_data[worm.group_id].animals[j].death_associated_post_expansion_contraction_stop, *current_worm))
					saved_ = false;
				if (ns_fix_annotation(handle().data->by_hand_timing_data[worm.group_id].animals[j].death_associated_post_expansion_contraction_start, *current_worm))
					saved_ = false;
			}

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Done setting by hand annotations"));
			if (!saved_){

				if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Updating events to storyboard"));
				update_events_to_storyboard(external_rescale_factor,handle);
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
				timepoints[i].movement_analyzer = &handle().data->movement_analyzer;  //*
				timepoints[i].group_id = properties_for_all_animals.stationary_path_id.group_id;
				timepoints[i].set_visulaization_type(current_visualization_type);
			}


			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Starting asynch image load"));
		
			handle().data->movement_analyzer.load_images_for_group_asynch(worm.group_id, number_of_valid_elements, asynch_loading_sql(), false, false, local_image_cache);
			
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting current timepoint"));
			set_current_timepoint(current_time,handle,current_time!=0, true);
			telemetry_zoom_factor = timepoints.size() / 250;
			if (telemetry_zoom_factor < 1)
				telemetry_zoom_factor = 1;
			
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting telemetry zoom"));
			{
				unsigned long latest_death_time = 0;
				//make sure the death time is shown on the graph
				for (unsigned int i = 0; i < handle().data->machine_timing_data[worm.group_id].animals.size(); i++) {
					if (handle().data->machine_timing_data[worm.group_id].animals[i].movement_cessation.time.period_end > latest_death_time) {
						latest_death_time = handle().data->machine_timing_data[worm.group_id].animals[i].movement_cessation.time.period_end;
						//cerr << "m:" << latest_death_time << "\n";
					}
				}
				for (unsigned int i = 0; i < handle().data->by_hand_timing_data[worm.group_id].animals.size(); i++) {
					if (handle().data->by_hand_timing_data[worm.group_id].animals[i].movement_cessation.time.period_end > latest_death_time) {
						latest_death_time = handle().data->by_hand_timing_data[worm.group_id].animals[i].movement_cessation.time.period_end;
						//cerr << "h:" << latest_death_time << "\n";
					}
				}
				//cerr << "h:" << latest_death_time << "\n";
				if (last_time_at_current_telementry_zoom(handle) < latest_death_time) {
					unsigned zoom_timepoint(0);
					for (unsigned int zoom_timepoint = 0; zoom_timepoint < timepoints.size(); zoom_timepoint++) {
						if (timepoints[zoom_timepoint].path_timepoint_element->absolute_time >= latest_death_time)
							break;
					}
					zoom_timepoint = zoom_timepoint + 0.25*(timepoints.size() - zoom_timepoint);
					//cout << zoom_timepoint << "\n";
					if (zoom_timepoint != 0)
						telemetry_zoom_factor = timepoints.size() / zoom_timepoint;
				}

				if (telemetry_zoom_factor > timepoints.size() / 250)
					telemetry_zoom_factor = timepoints.size() / 250;
				if (telemetry_zoom_factor < 1)
					telemetry_zoom_factor = 1;
			}
			
			
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Loading first image."));
			timepoints[current_timepoint_id].load_image(1024,current_image,sql(),local_image_cache, memory_pool,1);
			
			const ns_image_properties current_prop = current_image.im->properties();
			if (previous_images.size() != max_buffer_size || next_images.size() != max_buffer_size) {
				previous_images.resize(max_buffer_size);
				next_images.resize(max_buffer_size);
				for (unsigned int i = 0; i < max_buffer_size; i++) {
					if (previous_images[i].im == 0)
						previous_images[i].im = memory_pool.get(current_prop);
					previous_images[i].im->use_more_memory_to_avoid_reallocations(true);
					if (next_images[i].im == 0)
						next_images[i].im = memory_pool.get(current_prop);
					next_images[i].im->use_more_memory_to_avoid_reallocations(true);
				}
				for (unsigned int i = 0; i < max_buffer_size; i++)
					previous_images[i].im->init(current_image.im->properties());
				for (unsigned int i = 0; i < max_buffer_size; i++)
					next_images[i].im->init(current_image.im->properties());
			}

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Drawing Metadata."));
			draw_metadata(&timepoints[current_timepoint_id],*current_image.im,handle, external_rescale_factor);

			handle.release();
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Requesting Refresh."));
			request_refresh();
			lock.release();
		       

		}
		catch(ns_ex & ex){
			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Closing worm on error."));
			close_worm();
			ns_ex ex2("Error loading worm from region ");
			ex2 << metadata.plate_name() << " : " << ex.text();
			ns_hide_worm_window();
			throw ex;
		}
	}

	void clear() {

		telemetry_zoom_factor = 1;
		clear_base();
		saved_ = false;
		timepoints.resize(0);
		telemetry.clear();
	//	data_cache.clear();
		close_worm();
	//	data_cache.clear();
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
	bool movement_quantification_data_loaded(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle) {
		if (current_region_id == 0 || !data_cache.is_loaded(current_region_id))
			return false;		
		return handle().data->movement_quantification_data_is_loaded();
	}
	void load_movement_quantification_if_needed(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t * handle) {
		ns_death_time_posture_solo_annotater_data_cache_storage::handle_t local_handle;
		if (handle == 0) {
		  try{
			data_cache.get_region_movement_data_no_create(current_region_id, local_handle);
			handle = &local_handle;
		  }
		  catch(ns_ex & ex){
		    throw ns_ex("Error while accessing previously loaded movement data:") << ex.text();
		  }
		}

		if (!movement_quantification_data_loaded(*handle)) 
			load_movement_analysis(*handle,sql());
	}

	//XXX THIS BREAKS LOCKS ENCAPSULATION
	void load_movement_analysis(ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle, ns_sql & sql) {
		if (current_region_id != 0 )
			return;

		handle().data->load_movement_analysis(current_region_id, sql, true);
		
		//we now need to update all fields that referred to the previous movement analysis object
		auto cur_worm = &handle().data->movement_analyzer.group(current_worm_id.group_id).paths[current_worm_id.path_id];
	      
		for (unsigned int i = 0; i < timepoints.size(); i++){
			timepoints[i].path_timepoint_element = &cur_worm->element(i);
			timepoints[i].movement_analyzer = &handle().data->movement_analyzer;  
			       
		}
		for (unsigned int i = 0; i < previous_images.size(); i++) {
			previous_images[i].loaded = false;
			next_images[i].loaded = false;
		}
			
		timepoints[current_timepoint_id].load_image(1024, current_image, sql,local_image_cache, memory_pool, 1);
		handle.release();
	}
	//this breaks locking encapsulation
	void output_worm_frames(const string &base_dir,const std::string & filename, ns_death_time_posture_solo_annotater_data_cache_storage::handle_t & handle,ns_sql & sql){
		const string bd(base_dir);
		const string base_directory(bd + DIR_CHAR_STR + filename);
		ns_dir::create_directory_recursive(base_directory);
		const string gray_directory(base_directory + DIR_CHAR_STR + "gray");
		const string movement_directory(base_directory + DIR_CHAR_STR + "movement");
		ns_dir::create_directory_recursive(gray_directory);
		ns_dir::create_directory_recursive(movement_directory);

		handle().data->load_movement_analysis(current_region_id,sql,true);
		
		//add in by hand annotations so they are outputted correctly.
		std::vector<ns_death_time_annotation> by_hand_annotations;
		for (unsigned long i = 0; i < handle().data->by_hand_timing_data.size(); i++){
			handle().data->by_hand_timing_data[i].animals.begin()->generate_event_timing_data(by_hand_annotations);
				ns_death_time_annotation_compiler c;
				for (unsigned int j = 0; j < by_hand_annotations.size(); j++)
					c.add(by_hand_annotations[j], handle().data->metadata);
				handle().data->movement_analyzer.add_by_hand_annotations(c);
		}
		
		auto cur_worm = &handle().data->movement_analyzer.group(current_worm_id.group_id).paths[current_worm_id.path_id];

		//we're reallocating all the elements so we need to link them in.
		for (unsigned int i = 0; i < timepoints.size(); i++){
				timepoints[i].path_timepoint_element = &cur_worm->element(i);
		}
		const std::string outfile(base_directory + DIR_CHAR_STR + filename + "_movement_quantification.csv");
		ofstream o(outfile.c_str());
		handle().data->movement_analyzer.group(properties_for_all_animals.stationary_path_id.group_id).paths.begin()->write_detailed_movement_quantification_analysis_header(o);
		o << "\n";
		handle().data->movement_analyzer.write_detailed_movement_quantification_analysis_data(handle().data->metadata,o,false,properties_for_all_animals.stationary_path_id.group_id);
		o.close();


	//	string base_filename(base_directory + DIR_CHAR_STR + filename);
		ns_image_standard mvt_tmp;
		handle().data->load_images_for_worm(properties_for_all_animals.stationary_path_id,current_element_id()+1,sql,local_image_cache);
		unsigned long num_dig = ceil(log10((double)this->current_element_id()));

		for (unsigned int i = 0; i <= this->current_element_id(); i++){
			if (timepoints[i].path_timepoint_element->excluded)
				continue;
			const ns_image_standard * im(handle().data->movement_analyzer.group(properties_for_all_animals.stationary_path_id.group_id).paths[properties_for_all_animals.stationary_path_id.path_id].element(i).image_p());
			if (im == 0)
				continue;
			//ns_image_properties prop(im.properties());
			ns_save_image(gray_directory + DIR_CHAR_STR + filename + "_grayscale_" + pad_zeros(ns_to_string(i+1),num_dig) + ".tif",*im);
			handle().data->movement_analyzer.group(properties_for_all_animals.stationary_path_id.group_id).paths[properties_for_all_animals.stationary_path_id.path_id].element(i).generate_movement_visualization(mvt_tmp);
			//ns_image_properties prop(im.properties()
			ns_save_image(movement_directory + DIR_CHAR_STR + filename +  "_" + pad_zeros(ns_to_string(i+1),num_dig) + ".tif",mvt_tmp);
		}
		handle.release();
		
	}
	
	std::vector<ns_death_time_annotation> click_event_cache;
	

	void register_click(const ns_vector_2i & image_position, const ns_click_request & action, double external_rescale_factor);

	void display_current_frame();
	ns_vector_2i worm_image_offset_due_to_telemetry_graph_spacing;
	private:
	void update_events_to_storyboard(double external_rescale_factor, ns_death_time_posture_solo_annotater_data_cache_storage::handle_t &handle){
		auto cur_hand_data = current_by_hand_timing_data(handle);
		properties_for_all_animals.annotation_time = ns_current_time();
		cur_hand_data->animals[current_animal_id].specified = true;
		click_event_cache.resize(0);
		for (unsigned int i = 0; i < cur_hand_data->animals.size(); i++){
			cur_hand_data->animals[i].generate_event_timing_data(click_event_cache);
			cur_hand_data->animals[i].specified = true;
		}
		//anyone who is working with the solo annotater already must have the storyboard lock
		ns_specify_worm_details(properties_for_all_animals.region_info_id,properties_for_all_animals.stationary_path_id,properties_for_all_animals,click_event_cache,external_rescale_factor);
	}
		
	ns_alignment_type alignment_type;

};

#endif
