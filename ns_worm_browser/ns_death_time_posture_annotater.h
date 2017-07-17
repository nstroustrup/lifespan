#ifndef NS_death_time_posture_annotater_H
#define NS_death_time_posture_annotater_H
#include "ns_image.h"
#include "ns_image_server.h"
#include "ns_image_series_annotater.h"
#include "ns_death_time_annotation.h"
#include "ns_time_path_image_analyzer.h"

void ns_update_information_bar(const std::string & status="-1");

class ns_death_time_posture_annotater_timepoint : public ns_annotater_timepoint{
private:

	void check_metadata(){
		if (vis_info.region_id != region_info_id)
			throw ns_ex("Incorrect time information found in desired frame: ") << vis_info.region_id << " (expected " << region_id << ")";
	/*	for (unsigned int i = 0; i < vis_info.worms.size(); i++){
			//if an animal wasn't measured at a certain time
			//the most recent image of it is included.
			//this means that images might contain worms at times before
			//the time of the current frame.  This is allowed, and is noted.
			if (vis_info.worms[i].absolute_time > absolute_time)
				throw ns_ex("Incorrect time information found in desired frame: ") << vis_info.worms[i].absolute_time << " (expected " << absolute_time << ")";
		}*/
	}

	 ns_image_storage_source_handle<ns_8_bit> get_image(ns_sql & sql){
		return image_server.image_storage.request_from_storage(vis_image,&sql);
	 }

public:
	bool output_absolute_times;
	unsigned long total_number_of_frames;
	ns_movement_posture_visualization_summary vis_info;
	ns_image_server_image vis_image;
	unsigned long region_info_id,
				  region_id,
				  frame_id;
	//returns true if a worm was picked and a change was made to the metadata
	const ns_movement_visualization_summary_entry * get_worm_at_visualization_position(const ns_vector_2i & c) const {
		//	ns_vector_2i rel_coords(image_coords*resize_factor);
		for (unsigned int i = 0; i < vis_info.worms.size(); i++){
			if (vis_info.worms[i].path_in_visualization.position.x <= c.x &&
				vis_info.worms[i].path_in_visualization.position.y <= c.y &&
				vis_info.worms[i].path_in_visualization.position.x + vis_info.worms[i].path_in_visualization.size.x > c.x &&
				vis_info.worms[i].path_in_visualization.position.y + vis_info.worms[i].path_in_visualization.size.y > c.y){
					return &vis_info.worms[i];
			}
		}
		return 0;
	}
	ns_movement_visualization_summary_entry * get_worm_at_visualization_position(const ns_vector_2i & c){
	//	ns_vector_2i rel_coords(image_coords*resize_factor);
		for (unsigned int i = 0; i < vis_info.worms.size(); i++){
			if (vis_info.worms[i].path_in_visualization.position.x <= c.x &&
				vis_info.worms[i].path_in_visualization.position.y <= c.y &&
				vis_info.worms[i].path_in_visualization.position.x + vis_info.worms[i].path_in_visualization.size.x > c.x &&
				vis_info.worms[i].path_in_visualization.position.y + vis_info.worms[i].path_in_visualization.size.y > c.y){
					return &vis_info.worms[i];
			}
		}
		return 0;
	}
	void load_metadata(ns_sql & sql){
		ns_image_storage_source_handle<ns_8_bit> h(image_server.image_storage.request_from_storage(vis_image,&sql));
		vis_info.from_xml(h.input_stream().properties().description);
	}
	enum {ns_bottom_border_height=25,ns_text_distance_from_bottom=17,ns_text_height=16};
	void load_image(const unsigned long buffer_height,ns_annotater_image_buffer_entry & im,ns_sql & sql,ns_image_standard & temp_buffer,const unsigned long resize_factor_){
		ns_annotater_timepoint::load_image(buffer_height,im,sql,temp_buffer,resize_factor_);
		//black out graph
		unsigned long max_time(0);
		for (unsigned int i = 0; i < vis_info.worms.size(); i++){
			if (max_time < vis_info.worms[i].image_time)
				max_time = vis_info.worms[i].image_time;

			ns_vector_2i graph_dimensions(vis_info.worms[i].metadata_in_visualizationA.size);
			ns_vector_2i metadata_position(vis_info.worms[i].metadata_in_visualizationA.position );
			
			graph_dimensions=graph_dimensions/resize_factor_;
			
			metadata_position = metadata_position/resize_factor_;

			//add an extra pixel to remove resize-related blurring
			if (metadata_position.y+graph_dimensions.y +1 < im.im->properties().height)
				graph_dimensions.y++;

			for (unsigned int y = 0; y < graph_dimensions.y; y++){
				for (unsigned int x = 0; x < graph_dimensions.x; x++){
					(*im.im)[metadata_position.y +y][3*(metadata_position.x+x)+0]=0;
					(*im.im)[metadata_position.y +y][3*(metadata_position.x+x)+1]=0;
					(*im.im)[metadata_position.y +y][3*(metadata_position.x+x)+2]=0;
				}
			}
		}
//		vis_info.from_xml(temp_buffer.properties().description);
		ns_acquire_lock_for_scope lock(font_server.default_font_lock, __FILE__, __LINE__);
		ns_font & font(font_server.get_default_font());
		font.set_height(ns_text_height);
		ns_vector_2i label_position(im.im->properties().width-3*ns_bottom_border_height,im.im->properties().height-ns_text_distance_from_bottom);
		font.draw_color(label_position.x,label_position.y,ns_color_8(255,255,255),ns_to_string(frame_id+1) + "/" + ns_to_string(total_number_of_frames),*im.im);
		if (output_absolute_times && max_time!=0){
			font.draw_color(ns_bottom_border_height,label_position.y,ns_color_8(255,255,255),ns_format_time_string_for_human(max_time),*im.im);
		}
		lock.release();
		im.loaded = true;
	}
	
};

std::ostream & operator << (std::ostream & o, const ns_death_time_annotation & a);
void ns_zero_death_interval(ns_death_time_annotation_time_interval & e);
struct ns_death_timing_data_step_event_specification{

	ns_death_timing_data_step_event_specification(const ns_death_time_annotation_time_interval & event_time_):event_time(event_time_),region_position(0,0),region_size(0,0),region_info_id(0),worm_id_in_path(0){}
	ns_death_timing_data_step_event_specification(const ns_death_time_annotation_time_interval & interval,
												  const ns_analyzed_image_time_path_element & e, 
													const unsigned long region_info_id_, 
													const ns_stationary_path_id & id, const unsigned long worm_id_in_path_):

												event_time(interval),
												region_position(e.region_offset_in_source_image()),
												region_size(e.worm_region_size()),
												region_info_id(region_info_id_),
												stationary_path_id(id),worm_id_in_path( worm_id_in_path_){}

	ns_death_time_annotation_time_interval event_time;
	ns_vector_2i region_position,
				 region_size;
	unsigned long region_info_id;
	ns_stationary_path_id stationary_path_id;
	unsigned long worm_id_in_path;

};

struct ns_death_timing_data{
	static ns_color_8 lighten(const ns_color_8 & v){
		return (v/3)*4;
	}
public:
	ns_death_timing_data():
		region_info_id(0),
			specified(false){
		animal_specific_sticky_properties.animal_id_at_position = 0;
		}

	ns_death_timing_data(const ns_movement_visualization_summary_entry & p,unsigned long region_id,const unsigned long worm_id_in_path_):position_data(p),
		region_info_id(0),
		source_region_id(region_id),specified(false){
			animal_specific_sticky_properties.animal_id_at_position = worm_id_in_path_;
	}
	
	ns_movement_visualization_summary_entry position_data;
	unsigned long source_region_id;
	unsigned long region_info_id;

	//unsigned long worm_id_in_path;

	bool specified;

	ns_death_time_annotation fast_movement_cessation; //occurs at the interval before the start of the path

	void set_fast_movement_cessation_time(const ns_death_timing_data_step_event_specification & e){
		apply_step_specification(fast_movement_cessation,e,ns_additional_worm_entry);
	}
	ns_death_time_annotation translation_cessation,
				  movement_cessation,
				  death_posture_relaxation_termination;
	
	void output_event_times(std::ostream & o){
		o << "Fast Movement Cessation: " << fast_movement_cessation << "\n"
		  << "Translation: " << translation_cessation << "\n"
		  << "Movement Cessation " << movement_cessation << "\n"
		  << "Death Posture Relaxation " << death_posture_relaxation_termination << "\n";
	}

	ns_death_time_annotation animal_specific_sticky_properties;

	void generate_event_timing_data(std::vector<ns_death_time_annotation> & annotations){
		
		if (animal_specific_sticky_properties.animal_id_at_position != 0)
			annotations.push_back(fast_movement_cessation);
		if (translation_cessation.time.period_end != 0)
			annotations.push_back(translation_cessation);
		if (movement_cessation.time.period_end != 0)
			annotations.push_back(movement_cessation);
		if (death_posture_relaxation_termination.time.period_end != 0)
			annotations.push_back(death_posture_relaxation_termination);
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
	static ns_8_bit lighten(const ns_8_bit & a){
		if (a*1.75>=255)
			return 255;
		return (ns_8_bit)(a*1.75);
	};

	unsigned long get_time_from_movement_diagram_position(const unsigned long x_pos,const ns_vector_2i & pos, const ns_vector_2i & size, const ns_time_path_limits & path_observation_limits){
		const unsigned long path_start_time(path_observation_limits.interval_before_first_observation.period_end);
		const unsigned long last_path_frame_time(path_observation_limits.interval_after_last_observation.period_start);
		const unsigned long total_time = last_path_frame_time - path_start_time;
		
		const float dt(size.x/(float)total_time);
		return x_pos/dt+path_start_time;
	}
	void draw_movement_diagram(const ns_vector_2i & pos, const ns_vector_2i & size, const ns_time_path_limits & path_observation_limits, const ns_death_time_annotation_time_interval & current_interval, ns_image_standard & im, const float & scaling){
		if (path_observation_limits.last_obsevation_of_plate.period_end <= path_observation_limits.first_obsevation_of_plate.period_start)
			throw ns_ex("draw_movement_diagram()::Invalid path duration");

		const unsigned long path_start_time(path_observation_limits.interval_before_first_observation.period_end);
		const unsigned long last_path_frame_time(path_observation_limits.interval_after_last_observation.period_start);
		const unsigned long total_time = last_path_frame_time - path_start_time;
		
		const float dt(size.x/(float)total_time);
		
		const unsigned long worm_arrival_time(fast_movement_cessation.time.period_end);
		for (unsigned int y = 0; y < size.y; y++){
			unsigned int x;
			ns_color_8 c;
			c = ns_movement_colors::color(ns_movement_fast)*scaling;
			if (worm_arrival_time < path_start_time)
				throw ns_ex("Invalid time of additional worm entry into cluster");
			for (x = 0; x < (worm_arrival_time-path_start_time)*dt; x++){
				if(y+pos.y >= im.properties().height || x+pos.x >= im.properties().width || im.properties().components != 3)
					throw ns_ex("Yikes! 1");
				im[y+pos.y][3*(x+pos.x)+0] = c.x;
				im[y+pos.y][3*(x+pos.x)+1] = c.y;
				im[y+pos.y][3*(x+pos.x)+2] = c.z;
			}
			c = ns_movement_colors::color(ns_movement_slow)*scaling;
			
			if (translation_cessation.time.period_end != 0 && translation_cessation.time.period_end >= path_start_time){
				if (translation_cessation.time.period_end > last_path_frame_time)
					throw ns_ex("Invalid translation cessation time");
				for (; x < (int)((translation_cessation.time.period_end - path_start_time)*dt); x++){
					if(y+pos.y >= im.properties().height || x+pos.x >= im.properties().width || im.properties().components != 3)
						throw ns_ex("Yikes! 2");
					im[y+pos.y][3*(x+pos.x)+0] = c.x;
					im[y+pos.y][3*(x+pos.x)+1] = c.y;
					im[y+pos.y][3*(x+pos.x)+2] = c.z;
				}
				c = ns_movement_colors::color(ns_movement_posture)*scaling;
			}
			if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end >= path_start_time){
				if (movement_cessation.time.period_end > last_path_frame_time)
					throw ns_ex("Invalid Movement Cessation Time");
				for (; x < (int)((movement_cessation.time.period_end- path_start_time)*dt); x++){
					if(y+pos.y >= im.properties().height || x+pos.x >= im.properties().width || im.properties().components != 3)
						throw ns_ex("Yikes! 3");
					im[y+pos.y][3*(x+pos.x)+0] = c.x;
					im[y+pos.y][3*(x+pos.x)+1] = c.y;
					im[y+pos.y][3*(x+pos.x)+2] = c.z;
				}
				c = ns_movement_colors::color(ns_movement_stationary)*scaling;			
				if (movement_cessation.excluded == ns_death_time_annotation::ns_censored_at_end_of_experiment)
					c = ns_movement_colors::color(ns_movement_machine_excluded)*scaling;
	
			}
			if (death_posture_relaxation_termination.time.period_end != 0 && death_posture_relaxation_termination.time.period_end >= path_start_time){

				if (dt*(death_posture_relaxation_termination.time.period_start - path_start_time >= x))
					x = dt*(death_posture_relaxation_termination.time.period_start - path_start_time);


				if (death_posture_relaxation_termination.time.period_end > last_path_frame_time)
					throw ns_ex("Invalid Death Posture Relaxation Termination Time!");
				c = ns_movement_colors::color(ns_movement_death_posture_relaxation);
				for (; x < (int)((death_posture_relaxation_termination.time.period_end - path_start_time)*dt); x++){
					
					if(y+pos.y >= im.properties().height || x+pos.x >= im.properties().width || im.properties().components != 3)
						throw ns_ex("Yikes! 4");
					im[y+pos.y][3*(x+pos.x)+0] = c.x;
					im[y+pos.y][3*(x+pos.x)+1] = c.y;
					im[y+pos.y][3*(x+pos.x)+2] = c.z;
				}
				
				c = ns_movement_colors::color(ns_movement_stationary)*scaling;
			}
			for (; x < size.x; x++){
				
					if(y+pos.y >= im.properties().height || x+pos.x >= im.properties().width || im.properties().components != 3)
						throw ns_ex("Yikes! 5");
					im[y+pos.y][3*(x+pos.x)+0] = c.x;
					im[y+pos.y][3*(x+pos.x)+1] = c.y;
					im[y+pos.y][3*(x+pos.x)+2] = c.z;
			}
		}
		unsigned long interval_start((current_interval.period_start-path_start_time)*dt),
					  interval_end((current_interval.period_end-path_start_time)*dt);
		//cerr << "Interval duration: " << current_interval.period_end - current_interval.period_start << " seconds; " 
		//	<< (current_interval.period_end - current_interval.period_start)*dt << " est pixels; " << interval_end-interval_start << " actual pixels\n";

		for (unsigned int y = 0; y < size.y; y++){
			if (!current_interval.period_start_was_not_observed){
				for (unsigned long x = interval_start; x < interval_end; x++){
					if(y+pos.y >= im.properties().height || x+pos.x >= im.properties().width || im.properties().components != 3)
						throw ns_ex("Yikes! 6");
					
					im[pos.y+y][3*(x+pos.x)] = (1)?lighten(im[pos.y+y][3*(x+pos.x)]):im[pos.y+y][3*(x+pos.x)];
					im[pos.y+y][3*(x+pos.x)+1] = (1)?lighten(im[pos.y+y][3*(x+pos.x)+1]):im[pos.y+y][3*(x+pos.x)+1];
					im[pos.y+y][3*(x+pos.x)+2] = (1)?lighten(im[pos.y+y][3*(x+pos.x)+2]):im[pos.y+y][3*(x+pos.x)+2];
				}
			}
			im[pos.y+y][3*(interval_end+pos.x)] = (y%2)?255:0;
			im[pos.y+y][3*(interval_end+pos.x)+1] = (y%2)?255:0;
			im[pos.y+y][3*(interval_end+pos.x)+2] = (y%2)?255:0;
		}
	}

	ns_movement_state movement_state(const unsigned long time) const{
		if (death_posture_relaxation_termination.time.period_end != 0 && death_posture_relaxation_termination.time.period_end <= time)
			return ns_movement_stationary;
		
		if (movement_cessation.time.period_end != 0 && movement_cessation.time.period_end <= time){
			if (death_posture_relaxation_termination.time.period_end != 0)
				return ns_movement_death_posture_relaxation;
			else return ns_movement_stationary;
		}
		if (translation_cessation.time.period_end != 0 && translation_cessation.time.period_end <= time)
			return ns_movement_posture;
		return ns_movement_slow;
	}

	typedef std::pair<ns_color_8,unsigned long> ns_box_style;
	ns_box_style style(const unsigned long time) const{
		const unsigned long thick_line(3);
		const unsigned long thin_line(1);

		return ns_box_style(lighten(ns_movement_colors::color(movement_state(time))),thin_line);
	}

	int add_annotation(const ns_death_time_annotation & e, bool ignore_unhelpful_annotations){
		int duplication_events (0);
		specified = true;
				
		animal_specific_sticky_properties.annotation_time = e.annotation_time;
				
	/*	if (e.flag.specified())
			animal_specific_sticky_properties.flag = e.flag;
		if (e.is_excluded())
			animal_specific_sticky_properties.excluded = e.excluded;
		if (animal_specific_sticky_properties.number_of_worms_at_location_marked_by_hand < e.number_of_worms_at_location_marked_by_hand)
			animal_specific_sticky_properties.number_of_worms_at_location_marked_by_hand = e.number_of_worms_at_location_marked_by_hand;
				*/
		switch(e.type){
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
				if (death_posture_relaxation_termination.time.period_end == 0)
					death_posture_relaxation_termination = e;
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
	static void apply_step_specification(ns_death_time_annotation & a,const ns_death_timing_data_step_event_specification & e, const ns_movement_event & movement_type){
		a.time = e.event_time;
		a.position = e.region_position;
		a.size = e.region_size;
		a.region_info_id = e.region_info_id;
		a.stationary_path_id = e.stationary_path_id;
		a.annotation_source = ns_death_time_annotation::ns_posture_image;
		a.animal_id_at_position = e.worm_id_in_path;
		a.type = movement_type;
	}
	
		//given the user has selected the specified time path element, update the annotations apropriately
	void step_event(const ns_death_timing_data_step_event_specification & e,const ns_time_path_limits & observation_limits){
		//cerr << "V-------------------V\n";
		//output_event_times(cerr);
		if (e.event_time.period_end < fast_movement_cessation.time.period_end){
			set_fast_movement_cessation_time(e);
		}
		if (e.event_time.period_end == fast_movement_cessation.time.period_end){
			ns_zero_death_interval(death_posture_relaxation_termination.time);
			//clear all data to indicate the first time point is fast moving
			if (movement_cessation.time.period_end == fast_movement_cessation.time.period_end){
				ns_zero_death_interval(movement_cessation.time);
				ns_zero_death_interval(translation_cessation.time);
				this->fast_movement_cessation.time = observation_limits.last_obsevation_of_plate;

			}
			//indicate the first timepoint is stationary
			else if (translation_cessation.time.period_end == fast_movement_cessation.time.period_end){
				apply_step_specification(movement_cessation, e, ns_movement_cessation);
				movement_cessation.time.period_start_was_not_observed = true;
				ns_zero_death_interval(translation_cessation.time);
			}
			else{
				//indicate the first timepoint is slow moving
				apply_step_specification(translation_cessation, e, ns_translation_cessation);
				translation_cessation.time.period_start_was_not_observed = true;
			}
		}
		else if (movement_cessation.time.period_end == 0 &&
			death_posture_relaxation_termination.time.period_end == 0 &&
			(translation_cessation.time.period_end == 0 || e.event_time.period_end < translation_cessation.time.period_end)){
			apply_step_specification(translation_cessation, e, ns_translation_cessation);
		}
		else if (e.event_time.period_end == translation_cessation.time.period_end){
				ns_zero_death_interval(translation_cessation.time);
				ns_zero_death_interval(movement_cessation.time);
				ns_zero_death_interval(death_posture_relaxation_termination.time);
		}
		else if (death_posture_relaxation_termination.time.period_end == 0 &&
			(movement_cessation.time.period_end == 0 || e.event_time.period_end < movement_cessation.time.period_end)){
			apply_step_specification(movement_cessation, e, ns_movement_cessation);
		}
		else if (e.event_time.period_end == movement_cessation.time.period_end){
			ns_zero_death_interval(movement_cessation.time);
			ns_zero_death_interval(death_posture_relaxation_termination.time);
		}
		else if (death_posture_relaxation_termination.time.period_end == 0 && 
			e.event_time.period_end != death_posture_relaxation_termination.time.period_end){
			apply_step_specification(death_posture_relaxation_termination,e,ns_death_posture_relaxation_termination);
		}
		else if (e.event_time.period_end == death_posture_relaxation_termination.time.period_end){
			ns_zero_death_interval(death_posture_relaxation_termination.time);
		}

	//	output_event_times(cerr);
	//	cerr << "^-------------------^\n";
	}
};



template <class timing_data_container>
class ns_timing_data_and_death_time_annotation_matcher{
public:
	bool load_timing_data_from_set(const ns_death_time_annotation_set & set, const bool ignore_unuseful_annotations, timing_data_container & timing_data, std::vector<ns_death_time_annotation> & orphaned_events, std::string & error_message){
		//use the compiler to recognize all the stationary worms and put all the annotations together.
		ns_death_time_annotation_compiler c;
		c.add(set,ns_region_metadata());
		if (c.regions.size() > 1)
			throw ns_ex("ns_timing_data_and_death_time_annotation_matcher()::Found multiple regions in death time annotation set!");
		if (timing_data.size() == 0)
			throw ns_ex("Empty Data provided to annotation matcher");
		std::map<unsigned long,typename timing_data_container::value_type::ns_animal_list *> group_lookup;
		for (unsigned int i = 0; i < timing_data.size(); i++)
			group_lookup[timing_data[i].stationary_path_id.group_id] = &timing_data[i].animals;
		
		//resize(c.regions.begin()->second.locations.size());
		unsigned int location_id(0);
		for (ns_death_time_annotation_compiler_region::ns_location_list::iterator p = c.regions.begin()->second.locations.begin(); p != c.regions.begin()->second.locations.end(); p++){
			if (!p->properties.stationary_path_id.specified())
				continue;
			//find the correct entry in the timing data structure for the current location
			typename std::map<unsigned long,typename timing_data_container::value_type::ns_animal_list *>::iterator q = group_lookup.find(p->properties.stationary_path_id.group_id);
			if (q == group_lookup.end())
				continue;

			//identify how many worms were located at the current position
			std::map<unsigned long,ns_death_time_annotation_set> events_by_animal_id;
			for (unsigned int i = 0; i < p->annotations.size(); i++){
				events_by_animal_id[p->annotations[i].animal_id_at_position].add(p->annotations[i]);
			}
			//create space for those worms
			q->second->resize(events_by_animal_id.size());
			unsigned long animal_id(0);
			for (std::map<unsigned long,ns_death_time_annotation_set>::iterator r = events_by_animal_id.begin();r != events_by_animal_id.end(); r++){
				for (unsigned int i = 0; i < r->second.events.size(); i++){
					(*q->second)[animal_id].add_annotation(r->second.events[i],ignore_unuseful_annotations);
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

	void save_death_timing_data_to_set(const timing_data_container & timing_data, const std::vector<ns_death_time_annotation> & orphaned_events, ns_death_time_annotation_set & set, const bool include_flags_and_censoring){
		unsigned long current_time(ns_current_time());
		if (set.size() > 20)
			cout << "Many annotations specified!\n";
		for (typename timing_data_container::const_iterator p = timing_data.begin() ; p != timing_data.end(); ++p){
			if (p->specified == false)
				continue;
			if (p->translation_cessation.time.period_end != 0)
				set.add(p->translation_cessation);
			
			if (p->movement_cessation.time.period_end != 0)
				set.add(p->movement_cessation);
			
			if (p->death_posture_relaxation_termination.time.period_end != 0)
				set.add(p->death_posture_relaxation_termination);
		
		}

		for (unsigned long i = 0; i < orphaned_events.size(); i++)
			set.add(orphaned_events[i]);
	}
		
};

class ns_worm_learner;
class ns_death_time_posture_annotater : public ns_image_series_annotater{
private:
	inline ns_annotater_timepoint * timepoint(const unsigned long i){
		return &timepoints[i];
	}
	inline unsigned long number_of_timepoints(){
		return timepoints.size();
	}


	unsigned long region_info_id;
	std::vector<ns_death_time_posture_annotater_timepoint> timepoints;
	
	static void draw_box(const ns_vector_2i & p, const ns_vector_2i & s, const ns_color_8 & c,ns_image_standard & im, const unsigned long thickness){
		im.draw_line_color(p,p+ns_vector_2i(s.x,0),c,thickness);
		im.draw_line_color(p,p+ns_vector_2i(0,s.y),c,thickness);
		im.draw_line_color(p+s,p+ns_vector_2i(s.x,0),c,thickness);
		im.draw_line_color(p+s,p+ns_vector_2i(0,s.y),c,thickness);
	}
	void draw_metadata(ns_annotater_timepoint * tp_a,ns_image_standard & im){
		throw ns_ex("Not implemented");
		/*
	//	cerr << resize_factor << "\n";
		const ns_death_time_posture_annotater_timepoint * tp(static_cast<const ns_death_time_posture_annotater_timepoint * >(tp_a));
		const unsigned long clear_thickness(5);
		const unsigned long thickness_offset(3);
		ns_font & font(font_server.default_font());
		unsigned long small_label_height(im.properties().height/80);
		font.set_height(small_label_height);

		//clear out old box
		for (ns_timing_data::iterator p = timing_data.begin(); p != timing_data.end(); p++){
			ns_vector_2i box_pos((p->position_data.path_in_visualization.position-ns_vector_2i(thickness_offset,thickness_offset))/resize_factor),
				box_size((p->position_data.path_in_visualization.size+ns_vector_2i(thickness_offset,thickness_offset))/resize_factor);
			if (box_pos.x < 0){
				box_size.x+=box_pos.x;
				box_pos.x=0;
			}
			if (box_pos.y < 0){
				box_size.y+=box_pos.y;
				box_pos.y=0;
			}

			draw_box(box_pos,box_size,
					ns_color_8(0,0,0),
					im,
					clear_thickness);
			for (unsigned int y = 0; y < small_label_height; y++)
				for (unsigned int x = 0; x < 2*3*small_label_height; x++){
	//				cerr << p->position_data.image_position_in_visualization.y/resize_factor + small_label_height + y << "," << 3*(p->position_data.image_position_in_visualization.x/resize_factor+clear_thickness/2)+x << "\n";
					im[p->position_data.path_in_visualization.position.y/resize_factor + y][3*(p->position_data.path_in_visualization.position.x/resize_factor+clear_thickness/2)+x] = 0;
				}

			
			const ns_movement_visualization_summary_entry * w(tp->get_worm_at_visualization_position(p->position_data.path_in_visualization.position));

			if (w != 0){
				if (p->sticky_properties.number_of_worms_at_location_marked_by_machine > 1 || p->sticky_properties.number_of_worms_at_location_marked_by_hand > 0){
					ns_vector_2i pos(box_pos.x+clear_thickness/2,box_pos.y+small_label_height);
					ns_font_output_dimension d = font.draw_color(pos.x,pos.y,ns_color_8(200,200,255),
									string("+") + ns_to_string(p->sticky_properties.number_of_worms_at_location_marked_by_machine),im);

					font.draw_color(pos.x+d.w,pos.y,ns_color_8(255,200,200),
									string(" +") + ns_to_string(p->sticky_properties.number_of_worms_at_location_marked_by_hand),im);
				}

				if (p->sticky_properties.is_excluded()){
					draw_box(box_pos,box_size,
						ns_color_8(125,125,125),
						im,
						clear_thickness);
				}
				//draw new box
				ns_death_timing_data::ns_box_style b(p->style(w->image_time));
				draw_box(box_pos,box_size,
						b.first,
						im,
						b.second);
			}
		}
//		font.set_height(im.properties().height/40);
	//	font.draw_color(1,im.properties().height/39,ns_color_8(255,255,255),ns_format_time_string_for_human(tp->absolute_time),im);
	*/
	}
	
	typedef std::vector<ns_death_timing_data> ns_timing_data;
	ns_timing_data timing_data;

	
	std::vector<ns_death_time_annotation> orphaned_events;

	ns_worm_learner * worm_learner;


	mutable ns_image_server_results_file annotation_file;
	enum {default_resize_factor=2,max_buffer_size = 6};

	mutable bool saved_;
public:
	
	void set_resize_factor(const unsigned long resize_factor_){resize_factor = resize_factor_;}
	bool data_saved()const{return saved_;}
 ns_death_time_posture_annotater():ns_image_series_annotater(default_resize_factor, ns_death_time_posture_annotater_timepoint::ns_bottom_border_height),annotation_file("","",""),saved_(true){}
	typedef enum {ns_time_aligned_images,ns_death_aligned_images} ns_alignment_type;

	std::string image_label(const unsigned long frame_id){
		return ns_to_string(frame_id) + "/" + ns_to_string(timepoints.size());	
	}

/*	bool fix_censored_events_with_no_time_specified(){
		bool data_changed = false;
		for (ns_timing_data::iterator p =timing_data.begin() ; p != timing_data.end(); ++p){
			if (!p->sticky_properties.is_excluded())
				continue;
			bool found_matching_timepoint(false);
			for (unsigned int i = 0; i < timepoints.size(); i++){
				ns_movement_visualization_summary_entry * w(timepoints[i].get_worm_at_visualization_position(p->position_data.path_in_visualization.position));
				if (w != 0){
					p->sticky_properties.annotation_time = ns_current_time();
					found_matching_timepoint = true;
					data_changed = true;
					break;
				}
			}
			if (!found_matching_timepoint)
				throw ns_ex("fix_censored_events_with_no_time_specified()::Could not find matching timepoint for censored animal.");
		}
		return data_changed;
	}
	*/
	bool load_annotations(){
		orphaned_events.resize(0);
		throw ns_ex("Not Implemented");
		/*
		unsigned long duplication_events(0);
		if (timepoints.size() == 0) return false;
		ns_acquire_for_scope<std::istream> in(annotation_file.input());
		if (in.is_null())
			return false;
		ns_death_time_annotation_set set;
		set.read(ns_death_time_annotation_set::ns_all_annotations,in());

		ns_timing_data_and_death_time_annotation_matcher<ns_timing_data> matcher;
		string error_message;
		
		
		if (!matcher.load_timing_data_from_set(set,false,timing_data,orphaned_events,error_message))
			return false;
		if (error_message.size() != 0)
			ns_update_information_bar(error_message);
		saved_=true;
		saved_ = !fix_censored_events_with_no_time_specified();
		return true;*/
	}
	void save_annotations(const ns_death_time_annotation_set & extra_annotations) const{
		ns_death_time_annotation_set set;
		unsigned long current_time(ns_current_time());
		
		ns_timing_data_and_death_time_annotation_matcher<ns_timing_data> matcher;
		matcher.save_death_timing_data_to_set(timing_data,orphaned_events,set,true);
		set.add(extra_annotations);
		ns_acquire_for_scope<std::ostream> out(annotation_file.output());
		if (out.is_null())
			throw ns_ex("Could not open output file.");
		set.write(out());
		out.release();
		ns_update_information_bar(ns_to_string(set.events.size()) + " events saved at " + ns_format_time_string_for_human(ns_current_time()));
		saved_=true;
	};

	void clear() {
		clear_base();
		orphaned_events.clear();
		timepoints.clear();
		timing_data.clear();
		saved_ = false;

	}
	void load_region(const unsigned long region_info_id_,const ns_alignment_type alignment_type_,ns_worm_learner * worm_learner_){
		stop_fast_movement();
		clear();
		ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__,__LINE__);
		worm_learner = worm_learner_;
		if (sql.is_null())
			sql.attach(image_server.new_sql_connection(__FILE__,__LINE__));

		alignment_type = alignment_type_;

		timing_data.clear();
		timepoints.resize(0);
		
		region_info_id = region_info_id_;

		if (alignment_type == ns_time_aligned_images){
			//load timepoint data
			sql() << "SELECT i.id,i.`partition`, i.path, i.filename,r.id, r.capture_time FROM images as i, sample_region_images as r WHERE "
				<< "r.region_info_id = " << region_info_id
				<< " AND r.problem = 0 "
				<< " AND r.censored = 0 "
				<< " AND r." << ns_processing_step_db_column_name(ns_process_movement_posture_visualization) << " != 0"
				<< " AND r." << ns_processing_step_db_column_name(ns_process_movement_posture_visualization) << "= i.id "
				<< " ORDER BY r.capture_time ASC";
			ns_sql_result res;
			sql().get_rows(res);
			if (res.size() == 0)
				throw ns_ex("The specified region has no abosolute-time aligned posture visualization images created.");

			timepoints.reserve(res.size());
			for (unsigned int i = 0; i < res.size(); i++){
				unsigned long s(timepoints.size());
				try{
					timepoints.resize(s+1);
					timepoints[s].vis_image.id = ns_atoi64(res[i][0].c_str());
					timepoints[s].vis_image.partition = res[i][1];
					timepoints[s].vis_image.path = res[i][2];
					timepoints[s].vis_image.filename = res[i][3];
					timepoints[s].region_id = ns_atoi64(res[i][4].c_str());
					timepoints[s].region_info_id = region_info_id;
				//	timepoints[s].absolute_time = atol(res[i][5].c_str());
					timepoints[s].frame_id=s;
					timepoints[s].output_absolute_times = true;
					timepoints[s].load_metadata(sql());
				}
				catch(ns_ex & ex){
					timepoints.resize(s);
					cerr << "Error loading posture visualization image:" << ex.text();
				}
			}
		}
		else if (alignment_type == ns_death_aligned_images){
			sql() << "SELECT i.id,i.`partition`,i.path,i.filename,r.id FROM images as i, sample_region_image_aligned_path_images as r WHERE "
				    "r.region_info_id = " << region_info_id << " AND r.image_id = i.id";
			ns_sql_result res;
			sql().get_rows(res);
			if (res.size() == 0)
				throw ns_ex("The specified region has no death aligned posture visualization images.");
			timepoints.reserve(res.size());
			for (unsigned int i = 0; i < res.size(); i++){
				unsigned long s(timepoints.size());
				try{
					timepoints.resize(s+1);
					timepoints[s].vis_image.id = ns_atoi64(res[i][0].c_str());
					timepoints[s].vis_image.partition = res[i][1];
					timepoints[s].vis_image.path = res[i][2];
					timepoints[s].vis_image.filename = res[i][3];
					timepoints[s].region_id = 0;
					timepoints[s].region_info_id = region_info_id;
				//	timepoints[s].absolute_time = ns_atoi64(res[i][5].c_str());
					timepoints[s].frame_id = s;
					timepoints[s].output_absolute_times =false;
					timepoints[s].load_metadata(sql());
				}
				catch(ns_ex & ex){
					timepoints.resize(s);
					cerr << "Error loading posture visualization image:" << ex.text() << "\n";
				}
			}
		}
		if (timepoints.size() == 0)
			throw ns_ex("No timepoints could be loaded!");
		
	


		//initialize worm timing data
		for (unsigned int i = 0; i < timepoints.size(); i++){
			timepoints[i].total_number_of_frames = timepoints.size();
			for (unsigned int j = 0; j < timepoints[i].vis_info.worms.size(); j++){
				ns_timing_data::iterator p;
				for (p = timing_data.begin(); p != timing_data.end(); ++p){
					if (p->position_data.path_in_visualization.position == timepoints[i].vis_info.worms[j].path_in_visualization.position)
						break;
				}
			//	cerr << timepoints[i].vis_info.worms[j].visualization_position;
				if (p == timing_data.end()){
					timing_data.push_back(ns_death_timing_data(timepoints[i].vis_info.worms[j],timepoints[i].region_id,0));
				//	cerr << "I";
				}
		//		cerr << ",";
			}
	//		cerr << "\n";
		}

		//allocate image buffer
		if (previous_images.size() != max_buffer_size || next_images.size() != max_buffer_size){
			previous_images.resize(max_buffer_size);
			next_images.resize(max_buffer_size);
			for (unsigned int i = 0; i < max_buffer_size; i++){
				previous_images[i].im = new ns_image_standard();
				next_images[i].im = new ns_image_standard();
			}
		}
		if (current_image.im == 0)
			current_image.im = new ns_image_standard();
		if (alignment_type == ns_death_aligned_images){
			if (timepoints[0].vis_info.alignment_frame_number != 0){
				//navigate to the frame at which all the animals died
				for (current_timepoint_id = 0; current_timepoint_id < timepoints.size()-1; current_timepoint_id++){
					if (timepoints[current_timepoint_id+1].vis_info.alignment_frame_number 
						== timepoints[current_timepoint_id].vis_info.frame_number) break;
				}
			}
		}
		else{
			//navigate to first timepoint containing non-translating worms
		/*	unsigned long earliest_death_time(ULONG_MAX);
			long earliest_path_id(-1),earliest_group_id(-1);
			for (unsigned int i = 0; i < timing_data.size(); i++){
				if (timing_data[i].time_of_movement_cessation > 0 && 
					timing_data[i].time_of_movement_cessation < earliest_death_time){
					earliest_death_time = timing_data[i].time_of_movement_cessation;
					earliest_path_id = timing_data[i].position_data.path_id;
					earliest_group_id = timing_data[i].position_data.group_id;
				}
			}*/
			
			for (current_timepoint_id = 0; current_timepoint_id < timepoints.size()-1; current_timepoint_id++){
				if(timepoints[current_timepoint_id].vis_info.worms.size() > 0){
						break;
					}
			}
		}
		
		timepoints[current_timepoint_id].load_image(ns_death_time_posture_annotater_timepoint::ns_bottom_border_height,current_image,sql(),asynch_load_specification.temp_buffer,resize_factor);

		//initialize input/output file and load known annotations from disk
		ns_image_server_results_subject sub;
		sub.region_id = region_info_id;
		annotation_file = image_server.results_storage.hand_curated_death_times(sub,sql());
		load_annotations();
		
		draw_metadata(&timepoints[current_timepoint_id],*current_image.im);

		request_refresh();
		lock.release();
	}
	void register_click(const ns_vector_2i & image_position,const ns_click_request & action){
		throw ns_ex("not implemented");
		/*
		ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__,__LINE__);
		ns_movement_visualization_summary_entry * w(timepoints[current_timepoint_id].get_worm_at_visualization_position(image_position*resize_factor));
		if (w==0) return;
		ns_timing_data::iterator p;
		for (p = timing_data.begin(); p != timing_data.end(); ++p)
			if (p->position_data.path_in_visualization.position == w->path_in_visualization.position)
				break;
		if (p == timing_data.end())
			throw ns_ex("Could not find worm in timing data!");
		switch(action){
		case ns_cycle_state:  p->step_event(ns_death_timing_data_step_event_specification(w->image_time));break;
			case ns_censor: 
				p->sticky_properties.excluded = (p->sticky_properties.is_excluded()?(ns_death_time_annotation::ns_not_excluded):(ns_death_time_annotation::ns_by_hand_excluded));
				break;
			case ns_annotate_extra_worm: p->annotate_extra_worm(); break;
			default: throw ns_ex("ns_death_time_posture_annotater::Unknown click type");
		}
		p->sticky_properties.annotation_time = ns_current_time();
		p->specified = true;
		saved_ = false;
		draw_metadata(&timepoints[current_timepoint_id],*current_image.im);
		request_refresh();

		lock.release();*/
	}

	void display_current_frame();
	private:
		
	ns_alignment_type alignment_type;
};

#endif
