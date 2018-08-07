#ifndef NS_ANNOTATION_HANDLING_FOR_VISUALIZATION
#define NS_ANNOTATION_HANDLING_FOR_VISUALIZATION

#include "ns_time_path_image_analyzer.h"



void ns_crop_time(const ns_time_path_limits & limits, const ns_death_time_annotation_time_interval & first_observation_in_path, const ns_death_time_annotation_time_interval & last_observation_in_path, ns_death_time_annotation_time_interval & target);

void ns_zero_death_interval(ns_death_time_annotation_time_interval & e);
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
		source_region_id(region_id), specified(false), tentative_death_posture_relaxation_start_spec(false) {
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
		death_posture_relaxation_start,
		death_posture_relaxation_termination_;
	bool tentative_death_posture_relaxation_start_spec;


	void clear_annotations() {
		fast_movement_cessation.clear_movement_properties();
		fast_movement_cessation.clear_sticky_properties();
		translation_cessation.clear_movement_properties();
		translation_cessation.clear_sticky_properties();
		movement_cessation.clear_movement_properties();
		movement_cessation.clear_sticky_properties();
		death_posture_relaxation_start.clear_movement_properties();
		death_posture_relaxation_start.clear_sticky_properties();
		death_posture_relaxation_termination_.clear_movement_properties();
		death_posture_relaxation_termination_.clear_sticky_properties();
	}
	void output_event_times(std::ostream & o) {
		o << "Fast Movement Cessation: " << fast_movement_cessation << "\n"
			<< "Translation: " << translation_cessation << "\n"
			<< "Movement Cessation " << movement_cessation << "\n"
			<< "Death Posture Relaxation Start" << death_posture_relaxation_start << "\n"
			<< "Death Posture Relaxation End" << death_posture_relaxation_termination_ << "\n";
	}

	ns_death_time_annotation animal_specific_sticky_properties;

	void generate_event_timing_data(std::vector<ns_death_time_annotation> & annotations) {

		if (animal_specific_sticky_properties.animal_id_at_position != 0)
			annotations.push_back(fast_movement_cessation);
		if (translation_cessation.time.period_end != 0)
			annotations.push_back(translation_cessation);
		if (movement_cessation.time.period_end != 0)
			annotations.push_back(movement_cessation);
		if (death_posture_relaxation_termination_.time.period_end != 0)
			annotations.push_back(death_posture_relaxation_termination_);
		if (death_posture_relaxation_start.time.period_end != 0)
			annotations.push_back(death_posture_relaxation_start);
	}

	/*void annotate_extra_worm(){
	if (sticky_properties.number_of_worms_at_location_marked_by_hand == 0)
	sticky_properties.number_of_worms_at_location_marked_by_hand = 2;
	else if (sticky_properties.number_of_worms_at_location_marked_by_hand == ns_death_time_annotation::maximum_number_of_worms_at_position)
	sticky_properties.number_of_worms_at_location_marked_by_hand = 1;
	else if (sticky_properties.number_of_worms_at_location_marked_by_hand == 1)
	sticky_properties.number_of_worms_at_location_marked_by_hand = 0;
	else sticky_properties.number_of_worms_at_location_marked_by_hand++;
	}*/
	static ns_8_bit lighten(const ns_8_bit & a) {
		if (a*1.75 >= 255)
			return 255;
		return (ns_8_bit)(a*1.75);
	};

	unsigned long get_time_from_movement_diagram_position(const unsigned long x_pos, const ns_vector_2i & pos, const ns_vector_2i & size, const ns_time_path_limits & path_observation_limits) {
		const unsigned long path_start_time(path_observation_limits.interval_before_first_observation.period_end);
		const unsigned long last_path_frame_time(path_observation_limits.interval_after_last_observation.period_start);
		const unsigned long total_time = last_path_frame_time - path_start_time;

		const float dt(size.x / (float)total_time);
		return x_pos / dt + path_start_time;
	}
	void draw_movement_diagram(const ns_vector_2i & pos, const ns_vector_2i & size, const ns_time_path_limits & path_observation_limits, const ns_death_time_annotation_time_interval & current_interval, ns_image_standard & im, const float & scaling, const int current_interval_marker_min_width=0,const bool dotted_marker=true) {
		if (path_observation_limits.last_obsevation_of_plate.period_end <= path_observation_limits.first_obsevation_of_plate.period_start)
			throw ns_ex("draw_movement_diagram()::Invalid path duration");

		const unsigned long path_start_time(path_observation_limits.interval_before_first_observation.period_end);
		const unsigned long last_path_frame_time(path_observation_limits.interval_after_last_observation.period_start);
		const unsigned long total_time = last_path_frame_time - path_start_time;

		const float dt(size.x / (float)total_time);
		const unsigned long worm_arrival_time(fast_movement_cessation.time.period_end);
		//we push death relaxation back in time as we find more data
		unsigned long death_relaxation_start_time = worm_arrival_time;
		for (unsigned int y = 0; y < size.y; y++) {
			unsigned int x;
			ns_color_8 c;
			c = ns_movement_colors::color(ns_movement_fast)*scaling;
			if (worm_arrival_time < path_start_time)
				throw ns_ex("Invalid time of additional worm entry into cluster");
			for (x = 0; x < (worm_arrival_time - path_start_time)*dt; x++) {
				if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
					throw ns_ex("Yikes! 1");
				im[y + pos.y][3 * (x + pos.x) + 0] = c.x;
				im[y + pos.y][3 * (x + pos.x) + 1] = c.y;
				im[y + pos.y][3 * (x + pos.x) + 2] = c.z;
			}

			c = ns_movement_colors::color(ns_movement_slow)*scaling;
			if (translation_cessation.time.period_end != 0 && translation_cessation.time.period_end >= path_start_time) {

				death_relaxation_start_time = translation_cessation.time.period_end;
				if (translation_cessation.time.period_end > last_path_frame_time)
					throw ns_ex("Invalid translation cessation time");
				for (; x < (int)((translation_cessation.time.period_end - path_start_time)*dt); x++) {
					if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
						throw ns_ex("Yikes! 2");
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z;
				}
				c = ns_movement_colors::color(ns_movement_posture)*scaling;
			}
			if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end >= path_start_time) {

				death_relaxation_start_time = movement_cessation.time.period_end;
				if (movement_cessation.time.period_end > last_path_frame_time)
					throw ns_ex("Invalid Movement Cessation Time");
				for (; x < (int)((movement_cessation.time.period_end - path_start_time)*dt); x++) {
					if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
						throw ns_ex("Yikes! 3");
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z;
				}

				c = ns_movement_colors::color(ns_movement_stationary)*scaling;
				if (movement_cessation.excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment)
					c = ns_movement_colors::color(ns_movement_machine_excluded)*scaling;
			}
			//here the color is set by the value of the last specified event
			for (; x < size.x; x++) {

				if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3)
					throw ns_ex("Yikes! 6");
				im[y + pos.y][3 * (x + pos.x) + 0] = c.x;
				im[y + pos.y][3 * (x + pos.x) + 1] = c.y;
				im[y + pos.y][3 * (x + pos.x) + 2] = c.z;
			}

			if (y < size.y / 2 &&
				death_posture_relaxation_termination_.time.period_end != 0 && death_posture_relaxation_termination_.time.period_end >= path_start_time) {

				if (death_posture_relaxation_start.time.period_end != 0)
					death_relaxation_start_time = death_posture_relaxation_start.time.period_end;
				//draw animal as dead until death relaxation begins
				if (death_relaxation_start_time > last_path_frame_time)
					throw ns_ex("Invalid Death Posture Relaxation Start Time!");
				c = ns_movement_colors::color(ns_movement_death_posture_relaxation)*scaling;
				for (int x = (death_relaxation_start_time - (ns_s64_bit)path_start_time)*dt; x < (death_posture_relaxation_termination_.time.period_start - (ns_s64_bit)path_start_time)*dt; x++) {
					if (y + pos.y >= im.properties().height || x + pos.x >= im.properties().width || im.properties().components != 3) {
						std::cout << "Out of bounds death relaxation time interval draw (" << x + pos.x << "," << y + pos.y << ") in an image (" << im.properties().width << "," << im.properties().height << "\n";
						break;
					}
					im[y + pos.y][3 * (x + pos.x) + 0] = c.x;
					im[y + pos.y][3 * (x + pos.x) + 1] = c.y;
					im[y + pos.y][3 * (x + pos.x) + 2] = c.z;
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
				if (!dotted_marker) {
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
					im[pos.y + y][3 * (interval_end + pos.x)] = (y % 2) ? 255 : 0;
					im[pos.y + y][3 * (interval_end + pos.x) + 1] = (y % 2) ? 255 : 0;
					im[pos.y + y][3 * (interval_end + pos.x) + 2] = (y % 2) ? 255 : 0;
				}
			}
		}
	}

	bool is_death_time_contracting(const unsigned long time) const {
		return death_posture_relaxation_termination_.time.period_end != 0 && death_posture_relaxation_start.time.period_end <= time && death_posture_relaxation_termination_.time.period_end >= time;
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

		/*	if (e.flag.specified())
		animal_specific_sticky_properties.flag = e.flag;
		if (e.is_excluded())
		animal_specific_sticky_properties.excluded = e.excluded;
		if (animal_specific_sticky_properties.number_of_worms_at_location_marked_by_hand < e.number_of_worms_at_location_marked_by_hand)
		animal_specific_sticky_properties.number_of_worms_at_location_marked_by_hand = e.number_of_worms_at_location_marked_by_hand;
		*/
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
		case  ns_death_posture_relaxation_termination:
			if (death_posture_relaxation_termination_.time.period_end == 0)
				death_posture_relaxation_termination_ = e;
			else
				duplication_events++;
			break;
		case  ns_death_posture_relaxation_start:
			if (death_posture_relaxation_start.time.period_end == 0)
				death_posture_relaxation_start = e;
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
	}

	ns_death_time_annotation_time_interval last_specified_event_before_time(const unsigned long t, const ns_time_path_limits & path_observation_limits) {
		if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end < t)
			return movement_cessation.time;
		if (translation_cessation.time.period_end != 0 && movement_cessation.time.period_end < t)
			return translation_cessation.time;
		if (fast_movement_cessation.time.period_end != 0)
			return fast_movement_cessation.time;
		return path_observation_limits.interval_before_first_observation;
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
					ns_zero_death_interval(movement_cessation.time);
					ns_zero_death_interval(translation_cessation.time);
					this->fast_movement_cessation.time = observation_limits.last_obsevation_of_plate;

				}
				//indicate the first timepoint is stationary
				else if (translation_cessation.time.period_end == fast_movement_cessation.time.period_end) {
					apply_step_specification(movement_cessation, e, ns_movement_cessation);
					movement_cessation.time.period_start_was_not_observed = true;
					ns_zero_death_interval(translation_cessation.time);
				}
				else {
					//indicate the first timepoint is slow moving
					apply_step_specification(translation_cessation, e, ns_translation_cessation);
					translation_cessation.time.period_start_was_not_observed = true;
				}
			}
			else if (movement_cessation.time.period_end == 0 &&
				//death_posture_relaxation_termination.time.period_end == 0 &&
				(translation_cessation.time.period_end == 0 || e.event_time.period_end < translation_cessation.time.period_end)) {
				apply_step_specification(translation_cessation, e, ns_translation_cessation);
			}
			else if (e.event_time.period_end == translation_cessation.time.period_end) {
				ns_zero_death_interval(translation_cessation.time);
				ns_zero_death_interval(movement_cessation.time);
				//	ns_zero_death_interval(death_posture_relaxation_termination.time);
			}
			else if (//death_posture_relaxation_termination.time.period_end == 0 &&
				(movement_cessation.time.period_end == 0 || e.event_time.period_end < movement_cessation.time.period_end)) {
				apply_step_specification(movement_cessation, e, ns_movement_cessation);
			}
			else if (e.event_time.period_end == movement_cessation.time.period_end) {
				ns_zero_death_interval(movement_cessation.time);
				//	ns_zero_death_interval(death_posture_relaxation_termination.time);
			}
		}
		else {
			//the user is specifying the death time relaxation duration
			if (death_posture_relaxation_termination_.time.period_end == 0 ||
				e.event_time.period_end > death_posture_relaxation_termination_.time.period_end) {
				apply_step_specification(death_posture_relaxation_termination_, e, ns_death_posture_relaxation_termination);
				if (death_posture_relaxation_start.time.period_end == 0) {
					ns_death_timing_data_step_event_specification stp(e);
					stp.event_time = last_specified_event_before_time(e.event_time.period_end, observation_limits);
					apply_step_specification(death_posture_relaxation_start, stp, ns_death_posture_relaxation_start);
					tentative_death_posture_relaxation_start_spec = true;
				}
			}
			else  if (e.event_time.period_end == death_posture_relaxation_termination_.time.period_end ||
				e.event_time.period_end == death_posture_relaxation_start.time.period_end) {
				ns_zero_death_interval(death_posture_relaxation_termination_.time);
				ns_zero_death_interval(death_posture_relaxation_start.time);
				tentative_death_posture_relaxation_start_spec = false;
			}
			else if (death_posture_relaxation_start.time.period_end == 0 ||
				e.event_time.period_end < death_posture_relaxation_start.time.period_end) {
				apply_step_specification(death_posture_relaxation_start, e, ns_death_posture_relaxation_start);
				tentative_death_posture_relaxation_start_spec = false;
			}
			else {
				long end_dist = death_posture_relaxation_termination_.time.period_end - e.event_time.period_end;
				long start_dist = e.event_time.period_end - death_posture_relaxation_start.time.period_end;
				if (end_dist > start_dist || tentative_death_posture_relaxation_start_spec)
					apply_step_specification(death_posture_relaxation_start, e, ns_death_posture_relaxation_start);
				else
					apply_step_specification(death_posture_relaxation_termination_, e, ns_death_posture_relaxation_termination);
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
		/*
		unsigned long duplication_events(0);
		for (unsigned int i = 0; i < set.events.size(); i++){

		const unsigned long permitted_offset_distance(10);
		const unsigned long pod_sq(permitted_offset_distance*permitted_offset_distance);
		unsigned long min_dist(UINT_MAX);
		ns_vector_2i event_center((set.events[i].position+set.events[i].size/2));

		timing_data_container::iterator best_match(timing_data.end());
		for (timing_data_container::iterator p =timing_data.begin() ; p != timing_data.end(); ++p){

		//first try to search by explictly specified path id
		if (set.events[i].stationary_path_id.specified()
		&& set.events[i].stationary_path_id == p->position_data.stationary_path_id){
		best_match = p;
		break;
		}
		else{
		//otherwise search by position
		unsigned long dist((p->position_data.worm_in_source_image.position-set.events[i].position).squared());
		if (dist < min_dist){
		min_dist = dist;
		if (dist < pod_sq)
		best_match = p;
		}
		}
		}
		if (best_match != timing_data.end()){
		duplication_events+=best_match->add_annotation(set.events[i],ignore_unuseful_annotations);

		}

		else{
		orphaned_events.push_back(set.events[i]);
		}
		}

		//		in.release();
		if (orphaned_events.size() > 0 || duplication_events){
		string error_message("During load, ");
		error_message += ns_to_string(orphaned_events.size()) + " orphaned and ";
		error_message += ns_to_string(duplication_events) + " duplicates found.";
		unsigned long censored_count(0),extra_count(0);
		for (unsigned int i = 0; i < orphaned_events.size(); i++){
		if (orphaned_events[i].is_excluded())
		censored_count++;
		if (orphaned_events[i].number_of_worms_at_location_marked_by_hand > 0)
		extra_count++;
		}
		error_message+= ns_to_string(censored_count) + " of the orphans are censored annotations and " + ns_to_string(extra_count) + " are of multiple worm annotations";

		}*/
		return true;
	}

	void save_death_timing_data_to_set(const timing_data_container & timing_data, const std::vector<ns_death_time_annotation> & orphaned_events, ns_death_time_annotation_set & set, const bool include_flags_and_censoring) {
		unsigned long current_time(ns_current_time());
		if (set.size() > 20)
			cout << "Many annotations specified!\n";
		for (typename timing_data_container::const_iterator p = timing_data.begin(); p != timing_data.end(); ++p) {
			if (p->specified == false)
				continue;
			if (p->translation_cessation.time.period_end != 0)
				set.add(p->translation_cessation);

			if (p->movement_cessation.time.period_end != 0)
				set.add(p->movement_cessation);

			if (p->death_posture_relaxation_start.time.period_end != 0)
				set.add(p->death_posture_relaxation_start);

			if (p->death_posture_relaxation_termination_.time.period_end != 0)
				set.add(p->death_posture_relaxation_termination_);

		}

		for (unsigned long i = 0; i < orphaned_events.size(); i++)
			set.add(orphaned_events[i]);
	}

};
class ns_timing_data_configurator{
public:
	void operator()(const unsigned long region_id,ns_time_path_image_movement_analyzer & movement_analyzer, std::vector<ns_animal_list_at_position> & machine_timing_data, std::vector < ns_animal_list_at_position> & by_hand_timing_data) {
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
			//by_hand_timing_data[i].animals[0].worm_id_in_path = 0;
			//by_hand_timing_data[i].sticky_properties.annotation_source = ns_death_time_annotation::ns_lifespan_machine;
			//by_hand_timing_data[i].sticky_properties.position = movement_analyzer[i].paths[0].path_region_position;
			//	by_hand_timing_data[i].sticky_properties.size = movement_analyzer[i].paths[0].path_region_size;

			//	by_hand_timing_data[i].stationary_path_id = by_hand_timing_data[i].position_stationary_path_id;

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