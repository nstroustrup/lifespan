#ifndef NS_death_time_posture_annotater_H
#define NS_death_time_posture_annotater_H
#include "ns_image.h"
#include "ns_image_server.h"
#include "ns_image_series_annotater.h"
#include "ns_death_time_annotation.h"
#include "ns_time_path_image_analyzer.h"

#include "ns_annotation_handling_for_visualization.h"

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
	ns_64_bit region_info_id,
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
	void load_image(const unsigned long buffer_height,ns_annotater_image_buffer_entry & im,ns_sql & sql,ns_simple_local_image_cache & image_cache, ns_annotater_memory_pool & pool,const unsigned long resize_factor_){
		ns_annotater_timepoint::load_image(buffer_height,im,sql,image_cache,pool,resize_factor_);
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
	void draw_metadata(ns_annotater_timepoint * tp_a,ns_image_standard & im, double external_resize_factor){
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
		ns_update_main_information_bar(ns_to_string(set.events.size()) + " events saved at " + ns_format_time_string_for_human(ns_current_time()));
		ns_update_worm_information_bar(ns_to_string(set.events.size()) + " events saved at " + ns_format_time_string_for_human(ns_current_time()));
		saved_=true;
	};

	void clear() {
		clear_base();
		orphaned_events.clear();
		timepoints.clear();
		timing_data.clear();
		saved_ = false;

	}
	void load_region(const unsigned long region_info_id_,const ns_alignment_type alignment_type_,ns_worm_learner * worm_learner_, double external_rescale_factor){
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
					std::cerr << "Error loading posture visualization image:" << ex.text();
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
					std::cerr << "Error loading posture visualization image:" << ex.text() << "\n";
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
		
		timepoints[current_timepoint_id].load_image(ns_death_time_posture_annotater_timepoint::ns_bottom_border_height,current_image,sql(),local_image_cache,memory_pool,resize_factor);

		//initialize input/output file and load known annotations from disk
		ns_image_server_results_subject sub;
		sub.region_id = region_info_id;
		annotation_file = image_server.results_storage.hand_curated_death_times(sub,sql());
		load_annotations();
		
		draw_metadata(&timepoints[current_timepoint_id],*current_image.im,external_rescale_factor);

		request_refresh();
		lock.release();
	}
	void register_click(const ns_vector_2i & image_position,const ns_click_request & action, double external_rescale_factor){
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
