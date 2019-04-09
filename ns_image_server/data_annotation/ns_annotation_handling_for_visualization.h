#ifndef NS_ANNOTATION_HANDLING_FOR_VISUALIZATION
#define NS_ANNOTATION_HANDLING_FOR_VISUALIZATION

#include "ns_time_path_image_analyzer.h"



void ns_crop_time(const ns_time_path_limits & limits, const ns_death_time_annotation_time_interval & first_observation_in_path, const ns_death_time_annotation_time_interval & last_observation_in_path, ns_death_time_annotation_time_interval & target);

void ns_zero_death_interval(ns_death_time_annotation & e);
std::ostream & operator << (std::ostream & o, const ns_death_time_annotation & a);

struct ns_death_timing_data_step_event_specification {

	ns_death_timing_data_step_event_specification(const ns_death_time_annotation_time_interval & event_time_) :event_time(event_time_), region_position(0, 0), region_size(0, 0), region_info_id(0), worm_id_in_path(0) {}
	ns_death_timing_data_step_event_specification(const ns_death_time_annotation_time_interval & interval,
		const ns_analyzed_image_time_path_element & e,
		const ns_64_bit region_info_id_,
		const ns_stationary_path_id & id, const unsigned long worm_id_in_path_) :

		event_time(interval),
		region_position(e.region_offset_in_source_image()),
		region_size(e.worm_region_size()),
		region_info_id(region_info_id_),
		stationary_path_id(id), worm_id_in_path(worm_id_in_path_) {}

	ns_death_time_annotation_time_interval event_time;
	ns_vector_2i region_position,
		region_size;
	ns_64_bit region_info_id;
	ns_stationary_path_id stationary_path_id;
	unsigned long worm_id_in_path;

};


struct ns_death_timing_data {
	static ns_color_8 lighten(const ns_color_8 & v) {
		return (v / 3) * 4;
	}
public:
	ns_death_timing_data() :
		region_info_id(0),
		specified(false) {
		animal_specific_sticky_properties.animal_id_at_position = 0;
	}

	ns_death_timing_data(const ns_movement_visualization_summary_entry & p, unsigned long region_id, const unsigned long worm_id_in_path_) :position_data(p),
		region_info_id(0),
		source_region_id(region_id), specified(false), tentative_death_associated_expansion_start_spec(false), tentative_death_associated_post_expansion_contraction_start_spec(false) {
		animal_specific_sticky_properties.animal_id_at_position = worm_id_in_path_;
	}

	ns_movement_visualization_summary_entry position_data;
	unsigned long source_region_id;
	unsigned long region_info_id;

	//unsigned long worm_id_in_path;

	bool specified;

	ns_death_time_annotation fast_movement_cessation; //occurs at the interval before the start of the path

	void set_fast_movement_cessation_time(const ns_death_timing_data_step_event_specification & e) {
		apply_step_specification(fast_movement_cessation, e, ns_additional_worm_entry);
	}
	ns_death_time_annotation translation_cessation,
		movement_cessation,
		death_associated_expansion_start,
		death_associated_expansion_stop,
		death_associated_post_expansion_contraction_start,
		death_associated_post_expansion_contraction_stop;
	bool tentative_death_associated_expansion_start_spec,
		tentative_death_associated_post_expansion_contraction_start_spec;


	void clear_annotations() {
		fast_movement_cessation.clear_movement_properties();
		fast_movement_cessation.clear_sticky_properties();
		translation_cessation.clear_movement_properties();
		translation_cessation.clear_sticky_properties();
		movement_cessation.clear_movement_properties();
		movement_cessation.clear_sticky_properties();
		death_associated_expansion_start.clear_movement_properties();
		death_associated_expansion_start.clear_sticky_properties();
		death_associated_expansion_stop.clear_movement_properties();
		death_associated_expansion_stop.clear_sticky_properties();
		death_associated_post_expansion_contraction_start.clear_movement_properties();
		death_associated_post_expansion_contraction_start.clear_sticky_properties();
		death_associated_post_expansion_contraction_stop.clear_movement_properties();
		death_associated_post_expansion_contraction_stop.clear_sticky_properties();
	}
	void output_event_times(std::ostream & o) {
		o << "Fast Movement Cessation: " << fast_movement_cessation << "\n"
			<< "Translation: " << translation_cessation << "\n"
			<< "Movement Cessation " << movement_cessation << "\n"
			<< "Death-Associated Expansion Start" << death_associated_expansion_start << "\n"
			<< "Death-Associated Expansion End" << death_associated_expansion_stop << "\n"
			<< "Death-Associated post-Expansion Contraction Start" << death_associated_post_expansion_contraction_start << "\n"
			<< "Death-Associated post-Expansion Contraction End" << death_associated_post_expansion_contraction_stop << "\n";
	}

	ns_death_time_annotation animal_specific_sticky_properties;

	static bool useful_information_in_annotation(const ns_death_time_annotation & a) {
		return a.time.period_end != 0 || a.event_explicitness != ns_death_time_annotation::ns_unknown_explicitness;
	}
	void generate_event_timing_data(std::vector<ns_death_time_annotation> & annotations) {

		if (animal_specific_sticky_properties.animal_id_at_position != 0)
			annotations.push_back(fast_movement_cessation);
		if (useful_information_in_annotation(translation_cessation))
			annotations.push_back(translation_cessation);
		if (useful_information_in_annotation(movement_cessation))
			annotations.push_back(movement_cessation);
		if (useful_information_in_annotation(death_associated_expansion_stop))
			annotations.push_back(death_associated_expansion_stop);
		if (useful_information_in_annotation(death_associated_expansion_start))
			annotations.push_back(death_associated_expansion_start);
		if (useful_information_in_annotation(death_associated_post_expansion_contraction_stop))
			annotations.push_back(death_associated_post_expansion_contraction_stop);
		if (useful_information_in_annotation(death_associated_post_expansion_contraction_start))
			annotations.push_back(death_associated_post_expansion_contraction_start);
	}


	static ns_8_bit lighten(const ns_8_bit & a) {
		if (a*1.75 >= 255)
			return 255;
		return (ns_8_bit)(a*1.75);
	};

	typedef enum { no_expansion_button_border = 4 } ns_expansion_button_properties;
	unsigned long get_time_from_movement_diagram_position(const unsigned long x_pos, const ns_vector_2i & pos, const ns_vector_2i & size, const ns_time_path_limits & path_observation_limits, bool & expansion_button_pressed) {
		std::cerr << x_pos << " " << pos.x << " " << size.x << " " << size.y << " " << no_expansion_button_border << " " << (pos.x + size.x + no_expansion_button_border-size.y) << " " << (pos.x + size.x + no_expansion_button_border) << "\n";
		if (x_pos >= pos.x+( size.x - size.y)+ no_expansion_button_border &&
			x_pos <= pos.x + size.x + no_expansion_button_border ) { //the button has a width of size.y
			expansion_button_pressed = true;
			return 0;
		}
		else expansion_button_pressed = false;
		const unsigned long path_start_time(path_observation_limits.interval_before_first_observation.period_end);
		const unsigned long last_path_frame_time(path_observation_limits.interval_after_last_observation.period_start);
		const unsigned long total_time = last_path_frame_time - path_start_time;

		const unsigned long expansion_button_margin_size = (no_expansion_button_border + size.y);
		const float dt((size.x- expansion_button_margin_size) / (float)total_time);
		return (x_pos - pos.x) / dt + path_start_time;
	}
	typedef enum { ns_narrow_marker, ns_wide_dotted_marker,ns_highlight_up_until_current_time } ns_current_position_marker;
	typedef enum { ns_draw_relative_to_path, ns_draw_relative_to_plate} ns_draw_relative_spec;
	void draw_movement_diagram(const ns_vector_2i & pos, const ns_vector_2i & total_size, const ns_time_path_limits & path_observation_limits, const ns_death_time_annotation_time_interval & current_interval, ns_image_standard & im, const float & scaling, const int current_interval_marker_min_width = 0, const ns_current_position_marker marker_type = ns_narrow_marker, const ns_draw_relative_spec draw_spec = ns_draw_relative_to_path) {
		const unsigned long no_expansion_button_width(total_size.y);
		const unsigned long no_expansion_button_border(no_expansion_button_border);
		const unsigned long right_hand_margin(no_expansion_button_width + no_expansion_button_border);


		const ns_vector_2i size(total_size.x - right_hand_margin, total_size.y);

		if (path_observation_limits.last_obsevation_of_plate.period_end <= path_observation_limits.first_obsevation_of_plate.period_start)
			throw ns_ex("draw_movement_diagram()::Invalid path duration");

		ns_death_time_annotation_time_interval start_interval, stop_interval;
		unsigned long path_start_time_t, last_path_frame_time_t;
		if (draw_spec == ns_draw_relative_to_path) {
			start_interval = path_observation_limits.interval_before_first_observation;
			stop_interval = path_observation_limits.interval_after_last_observation;
			path_start_time_t = start_interval.period_end;
			last_path_frame_time_t = stop_interval.period_start;
		}
		else {
			start_interval = path_observation_limits.first_obsevation_of_plate;
			stop_interval = path_observation_limits.last_obsevation_of_plate;
			path_start_time_t = start_interval.period_start;
			last_path_frame_time_t = stop_interval.period_end;
		}
		const unsigned long path_start_time(path_start_time_t);
		const unsigned long last_path_frame_time(last_path_frame_time_t);
		const unsigned long total_time = last_path_frame_time - path_start_time;
		const float pre_current_time_scaling((marker_type == ns_highlight_up_until_current_time) ? 0.5 : 1);

		const float dt(size.x / (float)total_time);
		const unsigned long worm_arrival_time(fast_movement_cessation.time.period_end);
		//we push death relaxation start forward in time as we find more data
		//unsigned long death_relaxation_start_time = worm_arrival_time;

		unsigned int x;
		ns_color_8 c;
		c = ns_movement_colors::color(ns_movement_fast)*scaling;
		if (worm_arrival_time < path_start_time)
			throw ns_ex("Invalid time of additional worm entry into cluster");
		for (x = 0; x < (worm_arrival_time - path_start_time)*dt; x++) {
			if (x + pos.x >= im.properties().width || im.properties().components != 3)
				throw ns_ex("Yikes! 1");
			const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
			for (unsigned int y = 0; y < size.y; y++) {
				im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
			}
		}

		c = ns_movement_colors::color(ns_movement_slow)*scaling;
		if (translation_cessation.time.period_end != 0 && translation_cessation.time.period_end >= path_start_time) {

			//death_relaxation_start_time = translation_cessation.time.period_end;
			if (translation_cessation.time.period_end > last_path_frame_time)
				throw ns_ex("Invalid translation cessation time");
			for (; x < (int)((translation_cessation.time.period_end - path_start_time)*dt); x++) {
				if (pos.x >= im.properties().width || im.properties().components != 3)
					throw ns_ex("Yikes! 2");
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;

				for (unsigned int y = 0; y < size.y; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}
			c = ns_movement_colors::color(ns_movement_posture)*scaling;
		}
		if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end >= path_start_time) {

			//death_relaxation_start_time = movement_cessation.time.period_end;
			if (movement_cessation.time.period_end > last_path_frame_time)
				throw ns_ex("Invalid Movement Cessation Time");
			for (; x < (int)((movement_cessation.time.period_end - path_start_time)*dt); x++) {
				if (x + pos.x >= im.properties().width || im.properties().components != 3)
					throw ns_ex("Yikes! 3");
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
				for (unsigned int y = 0; y < size.y; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}

			c = ns_movement_colors::color(ns_movement_stationary)*scaling;
			if (movement_cessation.excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment)
				c = ns_movement_colors::color(ns_movement_machine_excluded)*scaling;
		}
		//here the color is set by the value of the last specified event
		for (; x < size.x; x++) {

			if (x + pos.x >= im.properties().width || im.properties().components != 3)
				throw ns_ex("Yikes! 6");
			const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
			for (unsigned int y = 0; y < size.y; y++) {
				im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
				im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
			}
		}

		if (death_associated_expansion_stop.time.period_end != 0 && death_associated_expansion_stop.time.period_end >= path_start_time) {
			unsigned long death_relaxation_start_time;
			if (death_associated_expansion_start.time.period_end != 0)
				death_relaxation_start_time = death_associated_expansion_start.time.period_end;
			else death_relaxation_start_time = death_associated_expansion_stop.time.period_end;
			//draw animal as dead until death relaxation begins
			if (death_relaxation_start_time > last_path_frame_time)
				throw ns_ex("Invalid Death Posture Relaxation Start Time!");
			c = ns_movement_colors::color(ns_movement_death_associated_expansion)*scaling;

			//make sure there is a minimum width to draw
			long start_pos((death_relaxation_start_time - (ns_s64_bit)path_start_time)*dt),
				stop_pos((death_associated_expansion_stop.time.period_end - (ns_s64_bit)path_start_time)*dt);
		//	std::cout << start_pos << "," << stop_pos << " -> ";
			if (stop_pos - start_pos < 4) {
				if (start_pos > 2)
					start_pos -= 2;
				if (stop_pos + 2 < size.x) {
					stop_pos += 2;
				}
			}
		//	std::cout << start_pos << "," << stop_pos << "\n";
			for (int x = start_pos; x < stop_pos; x++) {
				if (x + pos.x >= im.properties().width || im.properties().components != 3) {
					std::cout << "Out of bounds death relaxation time interval draw (" << x + pos.x << ") in an image (" << im.properties().width << "," << im.properties().height << "\n";
					break;
				}
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
				for (unsigned int y = 0; y < size.y / 2; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}
		}
		if (death_associated_post_expansion_contraction_stop.time.period_end != 0 && death_associated_post_expansion_contraction_stop.time.period_end >= path_start_time) {
			unsigned long ds;
			if (death_associated_post_expansion_contraction_start.time.period_end != 0)
				ds = death_associated_post_expansion_contraction_start.time.period_end;
			else ds = death_associated_post_expansion_contraction_stop.time.period_end;
			//draw animal as dead until death relaxation begins
			if (ds > last_path_frame_time)
				throw ns_ex("Invalid Death Posture Relaxation Start Time!");
			c = ns_movement_colors::color(ns_movement_death_associated_post_expansion_contraction)*scaling;

			//make sure there is a minimum width to draw
			long start_pos((ds - (ns_s64_bit)path_start_time)*dt),
				stop_pos((death_associated_post_expansion_contraction_stop.time.period_end - (ns_s64_bit)path_start_time)*dt);
			//	std::cout << start_pos << "," << stop_pos << " -> ";
			if (stop_pos - start_pos < 4) {
				if (start_pos > 2)
					start_pos -= 2;
				if (stop_pos + 2 < size.x) {
					stop_pos += 2;
				}
			}
			//	std::cout << start_pos << "," << stop_pos << "\n";
			for (int x = start_pos; x < stop_pos; x++) {
				if (x + pos.x >= im.properties().width || im.properties().components != 3) {
					std::cout << "Out of bounds death relaxation time interval draw (" << x + pos.x << ") in an image (" << im.properties().width << "," << im.properties().height << "\n";
					break;
				}
				const float cur_scale = (x / dt + path_start_time < current_interval.period_start) ? 1 : pre_current_time_scaling;
				for (unsigned int y = 0; y < size.y / 2; y++) {
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y*cur_scale;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z*cur_scale;
				}
			}
		}
		//draw death relaxation button (empty box if not specified, filled if specified)
		ns_color_8 edge_color = ns_movement_colors::color(ns_movement_death_associated_expansion)*scaling;
		ns_color_8 center_color;

		switch (death_associated_expansion_stop.event_explicitness) {
		case ns_death_time_annotation::ns_not_specified:
		case ns_death_time_annotation::ns_not_explicit:
		case ns_death_time_annotation::ns_explicitly_not_observed:
			center_color = ns_color_8(0, 0, 0)*scaling; break;
		case ns_death_time_annotation::ns_explicitly_observed:
			center_color = ns_movement_colors::color(ns_movement_death_associated_expansion)*scaling; break;
		default: throw ns_ex("Uknown event explicit state");
		}
		const unsigned int button_left = size.x + no_expansion_button_border + pos.x;
		for (unsigned long y = 0; y < no_expansion_button_width; y++) {
			for (unsigned long x = 0 ; x < no_expansion_button_width; x++) {
				if (y < 4 || y + 4 > no_expansion_button_width ||
					x < 4 || x + 4 > no_expansion_button_width) {
					im[y + pos.y][3 * (x + button_left) + 0] = edge_color.x;
					im[y + pos.y][3 * (x + button_left) + 1] = edge_color.y;
					im[y + pos.y][3 * (x + button_left) + 2] = edge_color.z;
				}
				//draw an x for explicitly not observed
				else if (death_associated_expansion_stop.event_explicitness == ns_death_time_annotation::ns_explicitly_not_observed &&
					(x  == y || x == no_expansion_button_width - y-1)) {
					im[y + pos.y][3 * (x + button_left) + 0] = edge_color.x;
					im[y + pos.y][3 * (x + button_left) + 1] = edge_color.y;
					im[y + pos.y][3 * (x + button_left) + 2] = edge_color.z;
				}
				else {
					im[y + pos.y][3 * (x + button_left) + 0] = center_color.x;
					im[y + pos.y][3 * (x + button_left) + 1] = center_color.y;
					im[y + pos.y][3 * (x + button_left) + 2] = center_color.z;
				}
			}
		}
		

		//highlight current time interval
		unsigned long interval_start((current_interval.period_start - path_start_time)*dt),
			interval_end((current_interval.period_end - path_start_time)*dt);
		//cerr << "Interval duration: " << current_interval.period_end - current_interval.period_start << " seconds; " 
		//	<< (current_interval.period_end - current_interval.period_start)*dt << " est pixels; " << interval_end-interval_start << " actual pixels\n";
		if (interval_end - interval_start < current_interval_marker_min_width) {
			interval_end = interval_start + current_interval_marker_min_width;
			if (interval_end >= size.x)
				interval_end = size.x - 1;
		}
		for (unsigned int y = 0; y < size.y; y++) {
			if (!current_interval.period_start_was_not_observed &&
				!current_interval.period_end_was_not_observed &&
				current_interval.period_start >= path_start_time &&
				current_interval.period_end <= last_path_frame_time) {

				if (marker_type == ns_highlight_up_until_current_time) {
					for (unsigned long x = interval_start; x < interval_end; x++) {
						if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
							throw ns_ex("Yikes! 6");

						im[pos.y + y][3 * (x + pos.x)] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x)]) : im[pos.y + y][3 * (x + pos.x)];
						im[pos.y + y][3 * (x + pos.x) + 1] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 1]) : im[pos.y + y][3 * (x + pos.x) + 1];
						im[pos.y + y][3 * (x + pos.x) + 2] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 2]) : im[pos.y + y][3 * (x + pos.x) + 2];
					}
				}
				else if (marker_type == ns_wide_dotted_marker) {
					for (unsigned long x = interval_start; x < interval_end; x++) {
						if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
							throw ns_ex("Yikes! 6");
						ns_8_bit cc = ((x - interval_start) % 2) ? ((y % 2) ? 255 : 0) : ((y % 2) ? 0 : 255);
						im[pos.y + y][3 * (x + pos.x)] = cc;
						im[pos.y + y][3 * (x + pos.x) + 1] = cc;
						im[pos.y + y][3 * (x + pos.x) + 2] = cc;
					}
				}
				else {
					for (unsigned long x = interval_start; x < interval_end; x++) {
						if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
							throw ns_ex("Yikes! 6");

						im[pos.y + y][3 * (x + pos.x)] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x)]) : im[pos.y + y][3 * (x + pos.x)];
						im[pos.y + y][3 * (x + pos.x) + 1] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 1]) : im[pos.y + y][3 * (x + pos.x) + 1];
						im[pos.y + y][3 * (x + pos.x) + 2] = (1) ? lighten(im[pos.y + y][3 * (x + pos.x) + 2]) : im[pos.y + y][3 * (x + pos.x) + 2];
					}

					int bar_start = interval_end;
					if (bar_start > current_interval_marker_min_width)
						bar_start -= current_interval_marker_min_width;
					else bar_start = 0;
					for (unsigned int x = bar_start; x < interval_end; x++) {
						im[pos.y + y][3 * (x + pos.x)] = 40;
						im[pos.y + y][3 * (x + pos.x) + 1] = 40;
						im[pos.y + y][3 * (x + pos.x) + 2] = 40;
					}
				}
			}
		}
	}
		

	bool is_death_time_expanding(const unsigned long time) const {
		return death_associated_expansion_stop.time.period_end != 0 && death_associated_expansion_stop.time.period_end >= time &&
			
				(
					death_associated_expansion_start.time.period_end != 0 &&
					death_associated_expansion_start.time.period_end < time
				) 
				||
					death_associated_expansion_start.time.period_end == 0 && 
				(
					movement_cessation.time.period_end != 0 && movement_cessation.time.period_end < time ||
					movement_cessation.time.period_end == 0 && translation_cessation.time.period_end != 0 && translation_cessation.time.period_end < time
				)
			
			;
	}
	bool is_death_time_post_expansion_contracting(const unsigned long time) const {
		return death_associated_post_expansion_contraction_stop.time.period_end != 0 && death_associated_post_expansion_contraction_stop.time.period_end >= time &&

			(
				death_associated_post_expansion_contraction_start.time.period_end != 0 &&
				death_associated_post_expansion_contraction_start.time.period_end < time
				)
			||
			death_associated_post_expansion_contraction_start.time.period_end == 0 &&
			(
				 death_associated_expansion_stop.time.period_end != 0 && death_associated_expansion_stop.time.period_end < time ||
				 death_associated_expansion_stop.time.period_end == 0 && movement_cessation.time.period_end != 0 && movement_cessation.time.period_end < time
				)

			;
	}

	ns_movement_state movement_state(const unsigned long time) const {

		if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end <= time) {
			return ns_movement_stationary;
		}
		if (translation_cessation.time.period_end != 0 && translation_cessation.time.period_end <= time)
			return ns_movement_posture;
		return ns_movement_slow;
	}

	typedef std::pair<ns_color_8, unsigned long> ns_box_style;
	ns_box_style style(const unsigned long time) const {
		const unsigned long thick_line(3);
		const unsigned long thin_line(1);

		return ns_box_style(lighten(ns_movement_colors::color(movement_state(time))), thin_line);
	}

	int add_annotation(const ns_death_time_annotation & e, bool ignore_unhelpful_annotations) {
		int duplication_events(0);
		specified = true;

		animal_specific_sticky_properties.annotation_time = e.annotation_time;

		switch (e.type) {
		case ns_fast_movement_cessation:
			fast_movement_cessation = e;
			break;
		case ns_translation_cessation:
			if (translation_cessation.time.period_end == 0)
				translation_cessation = e;
			else
				duplication_events++;
			break;
		case ns_movement_cessation:
			if (movement_cessation.time.period_end == 0)
				movement_cessation = e;
			else
				duplication_events++;
			break;
		case  ns_death_associated_expansion_stop:
			if (death_associated_expansion_stop.time.period_end == 0)
				death_associated_expansion_stop = e;
			else
				duplication_events++;
			break;
		case  ns_death_associated_expansion_start:
			if (death_associated_expansion_start.time.period_end == 0)
				death_associated_expansion_start = e;
			else
				duplication_events++;
			break;
		case  ns_death_associated_post_expansion_contraction_stop:
			if (death_associated_post_expansion_contraction_stop.time.period_end == 0)
				death_associated_post_expansion_contraction_stop = e;
			else
				duplication_events++;
			break;
		case  ns_death_associated_post_expansion_contraction_start:
			if (death_associated_post_expansion_contraction_start.time.period_end == 0)
				death_associated_post_expansion_contraction_start = e;
			else
				duplication_events++;
			break;
		case ns_no_movement_event:
			break;
		case ns_additional_worm_entry:
			fast_movement_cessation = e;
			break;
		default:
			if (!ignore_unhelpful_annotations)
				throw ns_ex("ns_timing_data_and_death_time_annotation_matcher::load_timing_data_from_set()::Do not know how to handle by hand annotation") << ns_movement_event_to_string(e.type);
		}
		return duplication_events;
	};
	static void apply_step_specification(ns_death_time_annotation & a, const ns_death_timing_data_step_event_specification & e, const ns_movement_event & movement_type) {
		a.time = e.event_time;
		a.position = e.region_position;
		a.size = e.region_size;
		a.region_info_id = e.region_info_id;
		a.stationary_path_id = e.stationary_path_id;
		a.annotation_source = ns_death_time_annotation::ns_posture_image;
		a.animal_id_at_position = e.worm_id_in_path;
		a.type = movement_type;
		a.event_explicitness = ns_death_time_annotation::ns_explicitly_observed;
	}

	ns_death_time_annotation_time_interval last_specified_event_before_time(const unsigned long t, const ns_time_path_limits & path_observation_limits) {
		if (death_associated_expansion_stop.time.period_end != 0 && death_associated_expansion_stop.time.period_end < t)
			return death_associated_expansion_stop.time;
		if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end < t)
			return movement_cessation.time;
		if (translation_cessation.time.period_end != 0 && movement_cessation.time.period_end < t)
			return translation_cessation.time;
		if (fast_movement_cessation.time.period_end != 0)
			return fast_movement_cessation.time;
		return path_observation_limits.interval_before_first_observation;
	}
	void step_death_posture_relaxation_explicitness(const ns_death_timing_data_step_event_specification & e) {
		const ns_death_time_annotation_time_interval start_time = death_associated_expansion_start.time;
		const ns_death_time_annotation_time_interval stop_time = death_associated_expansion_stop.time;
		const ns_death_time_annotation::ns_event_explicitness cur_exp(death_associated_expansion_stop.event_explicitness);
		apply_step_specification(death_associated_expansion_start, e, ns_death_associated_expansion_start);
		apply_step_specification(death_associated_expansion_stop, e, ns_death_associated_expansion_stop);
		death_associated_expansion_start.time = start_time;
		death_associated_expansion_stop.time = stop_time;

		switch (cur_exp) {
		case ns_death_time_annotation::ns_unknown_explicitness:  //deliberate pass-through
		case ns_death_time_annotation::ns_not_explicit:
			death_associated_expansion_stop.event_explicitness = ns_death_time_annotation::ns_explicitly_observed; break;
		case ns_death_time_annotation::ns_explicitly_observed:
			death_associated_expansion_stop.event_explicitness = ns_death_time_annotation::ns_explicitly_not_observed; break;
		case ns_death_time_annotation::ns_explicitly_not_observed: {
			death_associated_expansion_stop.event_explicitness = ns_death_time_annotation::ns_not_explicit; 
			ns_death_time_annotation_time_interval t(0, 0);
			death_associated_expansion_stop.time = t;
			death_associated_expansion_start.time = t;
			break;
		}
		default: throw ns_ex("step_death_posture_relaxation_explicitness()::Unkown state!");
		}
		death_associated_expansion_start.event_explicitness = death_associated_expansion_stop.event_explicitness;
	}
	//given the user has selected the specified time path element, update the annotations apropriately
	void step_event(const ns_death_timing_data_step_event_specification & e, const ns_time_path_limits & observation_limits, bool alternate_key_held) {
		//cerr << "V-------------------V\n";
		//output_event_times(cerr);
		if (!alternate_key_held) {
			if (e.event_time.period_end < fast_movement_cessation.time.period_end) {
				set_fast_movement_cessation_time(e);
			}
			if (e.event_time.period_end == fast_movement_cessation.time.period_end) {
				//	ns_zero_death_interval(death_posture_relaxation_termination.time);
				//clear all data to indicate the first time point is fast moving
				if (movement_cessation.time.period_end == fast_movement_cessation.time.period_end) {
					ns_zero_death_interval(movement_cessation);
					ns_zero_death_interval(translation_cessation);
					this->fast_movement_cessation.time = observation_limits.last_obsevation_of_plate;
					fast_movement_cessation.event_explicitness = ns_death_time_annotation::ns_explicitly_observed;

				}
				//indicate the first timepoint is stationary
				else if (translation_cessation.time.period_end == fast_movement_cessation.time.period_end) {
					apply_step_specification(movement_cessation, e, ns_movement_cessation);
					movement_cessation.time.period_start_was_not_observed = true;
					movement_cessation.event_explicitness = ns_death_time_annotation::ns_not_explicit;
					ns_zero_death_interval(translation_cessation);
				}
				else {
					//indicate the first timepoint is slow moving
					apply_step_specification(translation_cessation, e, ns_translation_cessation);
					translation_cessation.time.period_start_was_not_observed = true;
					translation_cessation.event_explicitness = ns_death_time_annotation::ns_not_explicit;
				}
			}
			else if (movement_cessation.time.period_end == 0 &&
				//death_posture_relaxation_termination.time.period_end == 0 &&
				(translation_cessation.time.period_end == 0 || e.event_time.period_end < translation_cessation.time.period_end)) {
				apply_step_specification(translation_cessation, e, ns_translation_cessation);
			}
			else if (e.event_time.period_end == translation_cessation.time.period_end) {
				ns_zero_death_interval(translation_cessation);
				ns_zero_death_interval(movement_cessation);
				//	ns_zero_death_interval(death_posture_relaxation_termination.time);
			}
			else if (//death_posture_relaxation_termination.time.period_end == 0 &&
				(movement_cessation.time.period_end == 0 || e.event_time.period_end < movement_cessation.time.period_end)) {
				apply_step_specification(movement_cessation, e, ns_movement_cessation);
			}
			else if (e.event_time.period_end == movement_cessation.time.period_end) {
				ns_zero_death_interval(movement_cessation);
				//	ns_zero_death_interval(death_posture_relaxation_termination.time);
			}
		}
		else {

			//the user is specifying the death time post-expansion contraction interval
			if (death_associated_expansion_stop.time.period_end != 0 && e.event_time.period_end > death_associated_expansion_stop.time.period_end) {

				if (death_associated_post_expansion_contraction_stop.time.period_end == 0) {
					apply_step_specification(death_associated_post_expansion_contraction_stop, e, ns_death_associated_post_expansion_contraction_stop);
					if (death_associated_post_expansion_contraction_start.time.period_end == 0) {
						ns_death_timing_data_step_event_specification stp(e);
						stp.event_time = last_specified_event_before_time(e.event_time.period_end, observation_limits);
						apply_step_specification(death_associated_post_expansion_contraction_start, stp, ns_death_associated_post_expansion_contraction_start);
						tentative_death_associated_post_expansion_contraction_start_spec = true;
					}
				}
				else  if (e.event_time.period_end == death_associated_post_expansion_contraction_stop.time.period_end) {
					ns_zero_death_interval(death_associated_post_expansion_contraction_stop);
					ns_zero_death_interval(death_associated_post_expansion_contraction_start);
					tentative_death_associated_post_expansion_contraction_start_spec = false;
				}
				else if (death_associated_post_expansion_contraction_start.time.period_end == 0 ||
					e.event_time.period_end < death_associated_post_expansion_contraction_start.time.period_end) {
					apply_step_specification(death_associated_post_expansion_contraction_start, e, ns_death_associated_post_expansion_contraction_start);
					tentative_death_associated_post_expansion_contraction_start_spec = false;
				}
				else {
					long end_dist = death_associated_post_expansion_contraction_stop.time.period_end - e.event_time.period_end;
					long start_dist = e.event_time.period_end - death_associated_post_expansion_contraction_start.time.period_end;
					if (end_dist > start_dist || tentative_death_associated_post_expansion_contraction_start_spec)
						apply_step_specification(death_associated_post_expansion_contraction_start, e, ns_death_associated_post_expansion_contraction_start);
					else
						apply_step_specification(death_associated_post_expansion_contraction_stop, e, ns_death_associated_post_expansion_contraction_stop);
				}
			}
			else {
				//the user is specifying the death time expansion interval
				if (death_associated_expansion_stop.time.period_end == 0) {
					apply_step_specification(death_associated_expansion_stop, e, ns_death_associated_expansion_stop);
					if (death_associated_expansion_start.time.period_end == 0) {
						ns_death_timing_data_step_event_specification stp(e);
						stp.event_time = last_specified_event_before_time(e.event_time.period_end, observation_limits);
						apply_step_specification(death_associated_expansion_start, stp, ns_death_associated_expansion_start);
						tentative_death_associated_expansion_start_spec = true;
					}
				}
				else  if (e.event_time.period_end == death_associated_expansion_stop.time.period_end) {
					ns_zero_death_interval(death_associated_expansion_stop);
					ns_zero_death_interval(death_associated_expansion_start);
					tentative_death_associated_expansion_start_spec = false;
				}
				else if (death_associated_expansion_start.time.period_end == 0 ||
					e.event_time.period_end < death_associated_expansion_start.time.period_end) {
					apply_step_specification(death_associated_expansion_start, e, ns_death_associated_expansion_start);
					tentative_death_associated_expansion_start_spec = false;
				}
				else {
					long end_dist = death_associated_expansion_stop.time.period_end - e.event_time.period_end;
					long start_dist = e.event_time.period_end - death_associated_expansion_start.time.period_end;
					if (end_dist > start_dist || tentative_death_associated_expansion_start_spec)
						apply_step_specification(death_associated_expansion_start, e, ns_death_associated_expansion_start);
					else
						apply_step_specification(death_associated_expansion_stop, e, ns_death_associated_expansion_stop);
				}
			}
		}

	}
};

struct ns_animal_list_at_position {
	ns_animal_list_at_position() :stationary_path_id(-1, -1, 0) {}
	ns_stationary_path_id stationary_path_id;
	typedef std::vector<ns_death_timing_data> ns_animal_list;  //positions can hold multiple animals
	ns_animal_list animals;
};

template <class timing_data_container>
class ns_timing_data_and_death_time_annotation_matcher {
public:
	bool load_timing_data_from_set(const ns_death_time_annotation_set & set, const bool ignore_unuseful_annotations, timing_data_container & timing_data, std::vector<ns_death_time_annotation> & orphaned_events, std::string & error_message) {
		//use the compiler to recognize all the stationary worms and put all the annotations together.
		ns_death_time_annotation_compiler c;
		c.add(set, ns_region_metadata());
		if (c.regions.size() > 1)
			throw ns_ex("ns_timing_data_and_death_time_annotation_matcher()::Found multiple regions in death time annotation set!");
		if (c.regions.empty())
			throw ns_ex("Region compilation yeilded an empty set");
		if (timing_data.size() == 0)
			throw ns_ex("Empty Data provided to annotation matcher");
		std::map<unsigned long, typename timing_data_container::value_type::ns_animal_list *> group_lookup;
		for (unsigned int i = 0; i < timing_data.size(); i++)
			group_lookup[timing_data[i].stationary_path_id.group_id] = &timing_data[i].animals;

		unsigned int location_id(0);
		for (ns_death_time_annotation_compiler_region::ns_location_list::iterator p = c.regions.begin()->second.locations.begin(); p != c.regions.begin()->second.locations.end(); p++) {
			if (!p->properties.stationary_path_id.specified())
				continue;
			//find the correct entry in the timing data structure for the current location
			typename std::map<unsigned long, typename timing_data_container::value_type::ns_animal_list *>::iterator q = group_lookup.find(p->properties.stationary_path_id.group_id);
			if (q == group_lookup.end())
				continue;

			//identify how many worms were located at the current position
			std::map<unsigned long, ns_death_time_annotation_set> events_by_animal_id;
			for (unsigned int i = 0; i < p->annotations.size(); i++) {
				events_by_animal_id[p->annotations[i].animal_id_at_position].add(p->annotations[i]);
			}
			//create space for those worms
			q->second->resize(events_by_animal_id.size());
			unsigned long animal_id(0);
			for (std::map<unsigned long, ns_death_time_annotation_set>::iterator r = events_by_animal_id.begin(); r != events_by_animal_id.end(); r++) {
				for (unsigned int i = 0; i < r->second.events.size(); i++) {
					(*q->second)[animal_id].add_annotation(r->second.events[i], ignore_unuseful_annotations);
				}
				animal_id++;
			}
			location_id++;
		}
		
		return true;
	}

	void save_death_timing_data_to_set(const timing_data_container & timing_data, const std::vector<ns_death_time_annotation> & orphaned_events, ns_death_time_annotation_set & set, const bool include_flags_and_censoring) {
		unsigned long current_time(ns_current_time());
		if (set.size() > 20)
		  std::cout << "Many annotations specified!\n";
		for (typename timing_data_container::const_iterator p = timing_data.begin(); p != timing_data.end(); ++p) {
			if (p->specified == false)
				continue;
			if (ns_death_timing_data::useful_information_in_annotation(p->translation_cessation))
				set.add(p->translation_cessation);

			if (ns_death_timing_data::useful_information_in_annotation(p->movement_cessation))
				set.add(p->movement_cessation);

			if (ns_death_timing_data::useful_information_in_annotation(p->death_associated_expansion_start))
				set.add(p->death_associated_expansion_start);

			if (ns_death_timing_data::useful_information_in_annotation(p->death_associated_expansion_stop))
				set.add(p->death_associated_expansion_stop);

			if (ns_death_timing_data::useful_information_in_annotation(p->death_associated_post_expansion_contraction_start))
				set.add(p->death_associated_post_expansion_contraction_start);

			if (ns_death_timing_data::useful_information_in_annotation(p->death_associated_post_expansion_contraction_stop))
				set.add(p->death_associated_post_expansion_contraction_stop);

		}

		for (unsigned long i = 0; i < orphaned_events.size(); i++)
			set.add(orphaned_events[i]);
	}

};
class ns_timing_data_configurator{
public:
	void operator()(const ns_64_bit region_id,ns_time_path_image_movement_analyzer & movement_analyzer, std::vector<ns_animal_list_at_position> & machine_timing_data, std::vector < ns_animal_list_at_position> & by_hand_timing_data) {
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Setting timing"));
		by_hand_timing_data.resize(0);
		machine_timing_data.resize(0);
		by_hand_timing_data.resize(movement_analyzer.size());
		machine_timing_data.resize(movement_analyzer.size());
		for (unsigned int i = 0; i < movement_analyzer.size(); i++) {
			ns_stationary_path_id path_id;
			path_id.detection_set_id = movement_analyzer.db_analysis_id();
			path_id.group_id = i;
			path_id.path_id = 0;
			by_hand_timing_data[i].stationary_path_id = path_id;
			machine_timing_data[i].stationary_path_id = path_id;

			by_hand_timing_data[i].animals.resize(1);
			by_hand_timing_data[i].animals[0].set_fast_movement_cessation_time(
				ns_death_timing_data_step_event_specification(
					movement_analyzer[i].paths[0].cessation_of_fast_movement_interval(),
					movement_analyzer[i].paths[0].element(movement_analyzer[i].paths[0].first_stationary_timepoint()),
					region_id, path_id, 0));
			by_hand_timing_data[i].animals[0].animal_specific_sticky_properties.animal_id_at_position = 0;
			machine_timing_data[i].animals.resize(1);
			machine_timing_data[i].animals[0].set_fast_movement_cessation_time(
				ns_death_timing_data_step_event_specification(
					movement_analyzer[i].paths[0].cessation_of_fast_movement_interval(),
					movement_analyzer[i].paths[0].element(movement_analyzer[i].paths[0].first_stationary_timepoint()),
					region_id, path_id, 0));
			machine_timing_data[i].animals[0].animal_specific_sticky_properties.animal_id_at_position = 0;
			by_hand_timing_data[i].animals[0].position_data.stationary_path_id = path_id;
			by_hand_timing_data[i].animals[0].position_data.path_in_source_image.position = movement_analyzer[i].paths[0].path_region_position;
			by_hand_timing_data[i].animals[0].position_data.path_in_source_image.size = movement_analyzer[i].paths[0].path_region_size;
			by_hand_timing_data[i].animals[0].position_data.worm_in_source_image.position = movement_analyzer[i].paths[0].element(movement_analyzer[i].paths[0].first_stationary_timepoint()).region_offset_in_source_image();
			by_hand_timing_data[i].animals[0].position_data.worm_in_source_image.size = movement_analyzer[i].paths[0].element(movement_analyzer[i].paths[0].first_stationary_timepoint()).worm_region_size();

			by_hand_timing_data[i].animals[0].animal_specific_sticky_properties.stationary_path_id = by_hand_timing_data[i].animals[0].position_data.stationary_path_id;

			by_hand_timing_data[i].animals[0].region_info_id = region_id;
			machine_timing_data[i].animals[0] =
				by_hand_timing_data[i].animals[0];

			//by default specify the beginnnig of the path as the translation cessation time.
			if (by_hand_timing_data[i].animals[0].translation_cessation.time.period_end == 0)
				//			for (int k = 0; k < 2; k++)
				by_hand_timing_data[i].animals[0].step_event(
					ns_death_timing_data_step_event_specification(
						movement_analyzer[i].paths[0].cessation_of_fast_movement_interval(),
						movement_analyzer[i].paths[0].element(movement_analyzer[i].paths[0].first_stationary_timepoint()), region_id,
						by_hand_timing_data[i].animals[0].position_data.stationary_path_id, 0), movement_analyzer[i].paths[0].observation_limits(), false);
		}
	}
};
#endif
