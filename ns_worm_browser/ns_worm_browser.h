#ifndef NS_WORM_TERMINAL
#define NS_WORM_TERMINAL

#include "ns_ex.h"

#include <FL/Fl.H>
#include <FL/Fl_Window.H>
#include <FL/Fl_Gl_Window.H>
#include <FL/Fl_Menu_.H>
#include <FL/Fl_Menu_Bar.H>
#include <FL/gl.h>

#include "ns_fl_modal_dialogs.h"
#include "ns_image.h"
#include "ns_image_server.h"
#include "ns_image_processing_pipeline.h"
#include "ns_jpeg.h"
#include "ns_tiff.h"


#include "ns_image_stream_buffers.h"
#include "ns_spatial_avg.h"
#include "ns_image_tools.h"
#include "ns_process_mask_regions.h"
#include "ns_worm_detector.h"
#include "ns_font.h"
#include "ns_socket.h"
#include "ns_image_socket.h"
#include "ns_image_server_message.h"
#include "ns_sql.h"
#include "ns_region_growing_segmenter.h"
#include "ns_difference_thresholder.h"
#include <string>
#include "ns_font.h"
#include "ns_image_registration.h"
#include "ns_heat_map_interpolation.h"
#include "ns_resampler.h"
#include "ns_detected_worm_info.h"
#include "ns_progress_reporter.h"
#include "ns_machine_learning_training_set.h"
#include "ns_xml.h"
#include "ns_time_path_solver.h"
//#include "ns_time_path_analyzer.h"
#include "ns_capture_schedule.h"
#include "ns_buffered_random_access_image.h"
#include "ns_mask_management.h"
#include "ns_lifespan_statistics.h"
#include "ns_machine_analysis_data_loader.h"
//#include "ns_worm_tracker.h"
using namespace std;

#include "ns_death_time_posture_annotater.h"
#ifdef _WIN32
#include "resource.h"
#endif
#include "ns_experiment_storyboard_annotater.h"
#include "ns_death_time_solo_posture_annotater.h"

extern bool output_debug_messages;
extern std::ofstream debug_output;

#include "ns_process_16_bit_images.h"
void ns_run_first_thermometer_experiment();

void ns_to_lower(std::string & s);

void ns_update_main_information_bar(const std::string & status);
void ns_update_worm_information_bar(const std::string & status);
void update_region_choice_menu();
void update_strain_choice_menu();
void update_exclusion_choice_menu();
ns_vector_2i main_image_window_size_difference();
ns_vector_2i worm_image_window_size_difference();

void ns_worm_browser_output_debug(const unsigned long line_number,const std::string & source, const std::string & message);


void report_changes_made_to_screen();

void ns_set_main_window_annotation_controls_activity(const bool active);

std::string ns_extract_scanner_name_from_filename(const std::string & filename);

//unsigned int ask_modal_question(const std::string & question,const string & response_1, const string & response_2, const string & response_3);

void ask_if_schedule_should_be_submitted_to_db(bool & write_to_disk, bool & write_to_db);
//typedef ns_image_whole<ns_8_bit,ns_image_stream_static_offset_buffer<ns_8_bit>,ns_image_stream_static_buffer<ns_8_bit> > ns_image_standard;

typedef enum {ns_movement_area_plot,ns_movement_scatter_proportion_plot,ns_movement_3d_path_plot,ns_movement_3d_movement_plot,ns_survival_curve} ns_region_visualization;

struct ns_box{
	typedef enum {ns_none,ns_top_left,ns_top_right,ns_bottom_left,ns_bottom_right,ns_whole_box} ns_box_location;
	ns_vector_2i top_left,
				 bottom_right;
	ns_box_location corner_contact(const ns_vector_2i & v) const{
		const long sensitivity(8*8);
		if ((top_left - v).squared() < sensitivity)
			return ns_box::ns_top_left;
		else if (bottom_right == ns_vector_2i(-1,-1))
			return ns_box::ns_none;
		else if ((bottom_right - v).squared() < sensitivity)
			return ns_box::ns_bottom_right;
		else if ( (ns_vector_2i(bottom_right.x,top_left.y) - v).squared() < sensitivity)
			return ns_box::ns_top_right;
		else if ( (ns_vector_2i(top_left.x,bottom_right.y) - v).squared() < sensitivity)
			return ns_box::ns_bottom_left;
		return ns_box::ns_none;
	}
	
};

struct ns_area_box{
	ns_box screen_coords,
		   image_coords;

	void assign_and_correct_inversions(const ns_box::ns_box_location & handle,const ns_vector_2i & new_screen_position, const ns_vector_2i & new_image_position){
		ns_area_box final_result(*this);
		if (screen_coords.bottom_right == ns_vector_2i(-1,-1)){
			if (handle!=ns_box::ns_top_left)
				throw ns_ex("Non-existant corner selected!");
			screen_coords.top_left = new_screen_position;
			image_coords.top_left = new_image_position;
			return;
		}

		switch(handle){
			case ns_box::ns_top_left:
				final_result.screen_coords.top_left=new_screen_position;
				final_result.image_coords.top_left=new_image_position;
				break;
			case ns_box::ns_top_right:
				final_result.screen_coords.top_left.y=new_screen_position.y;
				final_result.image_coords.top_left.y=new_image_position.y;
				final_result.screen_coords.bottom_right.x=new_screen_position.x;
				final_result.image_coords.bottom_right.x=new_image_position.x;
				break;
			case ns_box::ns_bottom_left:
				final_result.screen_coords.top_left.x=new_screen_position.x;
				final_result.image_coords.top_left.x=new_image_position.x;
				final_result.screen_coords.bottom_right.y=new_screen_position.y;
				final_result.image_coords.bottom_right.y=new_image_position.y;
				break;
			case ns_box::ns_bottom_right:
				final_result.screen_coords.bottom_right=new_screen_position;
				final_result.image_coords.bottom_right=new_image_position;
				break;
		}
		//now resolve any inversion
		if (handle == ns_box::ns_bottom_right || handle == ns_box::ns_top_right){
			if (final_result.screen_coords.top_left.x +4 > final_result.screen_coords.bottom_right.x){
				final_result.screen_coords.bottom_right.x = screen_coords.top_left.x+4;
				final_result.image_coords.bottom_right.x = image_coords.top_left.x+4;

			}
		}
		if (handle == ns_box::ns_bottom_left || handle == ns_box::ns_top_left){
			if (final_result.screen_coords.top_left.x +4 > final_result.screen_coords.bottom_right.x){
				final_result.screen_coords.top_left.x = screen_coords.bottom_right.x-4;
				final_result.image_coords.top_left.x = image_coords.bottom_right.x-4;

			}
		}	
		if (handle == ns_box::ns_bottom_left || handle == ns_box::ns_bottom_right){
			if (final_result.screen_coords.top_left.y +4 > final_result.screen_coords.bottom_right.y){
				final_result.screen_coords.bottom_right.y = screen_coords.top_left.y+4;
				final_result.image_coords.bottom_right.y = image_coords.top_left.y+4;

			}
		}
		if (handle == ns_box::ns_top_left || handle == ns_box::ns_top_right){
			if (final_result.screen_coords.top_left.y +4 > final_result.screen_coords.bottom_right.y){
				final_result.screen_coords.top_left.y = screen_coords.bottom_right.y-4;
				final_result.image_coords.top_left.y = image_coords.bottom_right.y-4;

			}
		}
		screen_coords = final_result.screen_coords;
		image_coords = final_result.image_coords;
	}
};


typedef enum { ns_none, ns_activate, ns_deactivate } ns_menu_bar_request;

class ns_area_handler{
public:
	ns_area_handler():unfinished_box_exists(false),selected_box_exists(false),moved_since_last_click(false),created_new_box_in_current_click(false){current_unfinished_box = boxes.end(); selected_box = boxes.end();}
	typedef enum {ns_select_handle,ns_move_handle,ns_deselect_handle,ns_move_all_boxes} ns_handle_action;

	void click(const ns_handle_action & action,const ns_vector_2i & image_pos, const ns_vector_2i & screen_pos,ns_8_bit * screen_buffer, const ns_image_standard & background,const unsigned long scaling, const double pixel_scaling);
	void output_boxes(std::ostream & out, const std::string & device_name,const float & resolution,const std::string & units);
	void clear_boxes();
	void draw_boxes(ns_8_bit * screen_buffer,const ns_image_properties & buffer_size,const unsigned long image_scaling, const double pixel_scaling) const;

	void register_size_change(ns_image_properties & prop){
		bool detete_current_unfinished(false);
		for (std::vector<ns_area_box>::iterator p = boxes.begin(); p != boxes.end(); ){
			if ((!(p->image_coords.bottom_right == ns_vector_2i(-1,-1) &&
				(p->image_coords.bottom_right.x >=  prop.width || p->image_coords.bottom_right.y >=  prop.height))||
				p->image_coords.top_left.x >=  prop.width || p->image_coords.top_left.y >=  prop.height)){
				if (unfinished_box_exists && current_unfinished_box == p)
					detete_current_unfinished = true;
				p = boxes.erase(p);
			}
			else ++p;
		}
		if (detete_current_unfinished)
			current_unfinished_box = boxes.end();
		selected_box = boxes.end();
		
	}
private:
	void remove_box_from_screen_buffer(const std::vector<ns_area_box>::iterator b, ns_8_bit * screen_buffer, const ns_image_standard & background,const unsigned long scaling, const double pixel_scaling) const;
	std::vector<ns_area_box>::iterator current_unfinished_box;
	std::vector<ns_area_box>::iterator selected_box;
	ns_box::ns_box_location cur_box_handle;
	std::vector<ns_area_box> boxes;

	bool moved_since_last_click;
	bool created_new_box_in_current_click;

	bool unfinished_box_exists,
		selected_box_exists;

};



struct ns_experiment_region_chooser_region{
	std::string region_name,
				display_name;
	ns_region_metadata * region_metadata;
	bool excluded,
		 censored;
	unsigned long region_id;
};
struct ns_experiment_region_chooser_sample{
	std::string sample_name;
	unsigned long sample_id;
	std::vector<ns_experiment_region_chooser_region> regions;
	std::string device;
};
struct ns_experiment_region_selector_experiment_info{
	ns_experiment_region_selector_experiment_info(){}
	ns_experiment_region_selector_experiment_info(const unsigned long id, const std::string & name_, const unsigned long group_id_):
	experiment_id(id),name(name_),experiment_group_id(group_id_){}
	std::string name;
	unsigned long experiment_id;
	unsigned long experiment_group_id;
	std::string experiment_group_name;
};
bool ns_load_image_from_resource(int resource_id,const std::string &filename);
class ns_experiment_region_selector{
public:
	typedef enum {ns_show_all,ns_hide_censored,ns_hide_uncensored} ns_censor_masking;
	
	static ns_censor_masking censor_masking_from_string(const std::string & s){
		for (unsigned int i = 0; i < 3; i++)
			if (s == censor_masking_string((ns_censor_masking)i))
				return (ns_censor_masking)i;
		throw ns_ex("Unknown censor masking string ") << s;
	}
	static std::string censor_masking_string(const ns_censor_masking & c){
		switch(c){
			case ns_show_all:
				return "All";
			case ns_hide_censored:
				return "Only Verified";
			case ns_hide_uncensored:
				return "Only Excluded";
		}
		throw ns_ex("Unknown censor type");
	}
private:
	ns_experiment_region_chooser_sample * cur_sample;
	ns_experiment_region_chooser_region * cur_region;
	ns_region_metadata * cur_strain;
	unsigned long experiment_id;
	ns_censor_masking current_censor_masking;
public:
	ns_censor_masking censor_masking() const{return current_censor_masking;}
	void set_censor_masking(const ns_censor_masking & censor_masking ){current_censor_masking = censor_masking;}

	ns_experiment_region_selector():experiment_id(0),cur_sample(0),cur_region(0), cur_strain(0){}
	void set_current_experiment(const std::string & experiment_name,ns_sql & sql){
		for (unsigned int i = 0; i < experiment_groups.size(); i++){
			for (unsigned int j = 0; j < experiment_groups[i].size(); j++){
				if (experiment_groups[i][j].name == experiment_name){
					set_current_experiment(experiment_groups[i][j].experiment_id,sql);
					return;
				}
			}
		}
			throw ns_ex("Could not locate experiment name in cache: ") << experiment_name;
	}

	void set_current_experiment(long experiment_id_,ns_sql & sql){
		//select default experiment
		if (experiment_id_==-1){
			ns_worm_browser_output_debug(__LINE__,__FILE__,"Default experiment requested");
			if (experiment_groups.size() == 0)
				throw ns_ex("Default experiment requested while no experiment groups exist in cache.");
			experiment_id_ = 0;
			ns_worm_browser_output_debug(__LINE__,__FILE__,std::string(ns_to_string(experiment_groups.size())+ " experiment groups found"));
			for (unsigned int i = 0; i < experiment_groups.size(); i++){
				if (experiment_groups.size() > 0){
					for (unsigned int j = 0; j < experiment_groups[i].size(); j++)
						ns_worm_browser_output_debug(__LINE__,__FILE__,ns_to_string(i) + "," + ns_to_string(j) + ":" + ns_to_string(experiment_groups[i][j].experiment_id));
					if (experiment_groups[i].size() > 0)
						experiment_id_ = experiment_groups[i][0].experiment_id;
					break;
				}
			}
			//if (experiment_id_ == 0){
				//throw ns_ex("Default experiment requested while no experiments exist in database.");
			//}
		}
		else{
			string experiment_name;
			if (!get_experiment_name(experiment_id_,experiment_name))
				throw ns_ex("Could not find experiment in cached experiment information.");
		}
		if (experiment_id_ != 0){
			load(experiment_id_,sql);
			experiment_id = experiment_id_;
		}
	}

	bool select_default_sample_and_region(){
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Slecting default sample and region");
		cur_strain = 0;
		//if (!experiment_strains.empty())
		//	cur_strain = &experiment_strains.begin()->second;
		//else cur_strain = 0;
		if (samples.size() == 0) 
			return false;
		for (unsigned int i = 0; i < samples.size(); i++){
			if (samples[i].regions.size() != 0){
				cur_sample = &samples[i];
				cur_region = &samples[i].regions[0];
				return true;
			}
		}
		return false;
	}

	void select_region(const string & sample_region_name){
		if (experiment_id == 0)
			return;
		string sample_name,region_name;
		std::string::size_type p(sample_region_name.find("::"));
		if (p == std::string::npos)
			throw ns_ex("Could not find \"::\" in region name");
		sample_name = sample_region_name.substr(0,p);
		region_name = sample_region_name.substr(p+2,std::string::npos);
		for (unsigned int i = 0; i < samples.size(); i++){
			if (samples[i].sample_name == sample_name){
				for (unsigned int j = 0; j < samples[i].regions.size(); ++j){
					if (samples[i].regions[j].region_name == region_name){
						cur_sample = &samples[i];
						cur_region = &samples[i].regions[j];
						return;
					}
				}
				throw ns_ex("Could not identify region named ") << region_name;
			}
		}
		throw ns_ex("Could not identify sample named ") << sample_name;
	}
	void select_strain(const string & strain_name){
		if (experiment_id == 0)
			return;

		if (strain_name == "All Strains"){
			cur_strain = 0;
			return;
		}
		if (experiment_strains.find(strain_name) == experiment_strains.end())
			throw ns_ex("Could not find ") << strain_name  << " in the strain list.";
		cur_strain = &(experiment_strains.find(strain_name)->second);
	}

	const unsigned long & current_experiment_id(){return experiment_id;}
	const std::string current_experiment_name(){
		if (experiment_id == 0)
			return "";
		std::string ret; 
		get_experiment_name(current_experiment_id(),ret);
		return ret;
	}
	ns_region_metadata & current_strain(){if (cur_strain == 0) throw ns_ex("No Strain Selected"); else return *cur_strain;}
	bool strain_selected() const{return cur_strain!=0;}
	
	bool region_selected() const{return cur_region!=0;}
	bool sample_selected() const{return cur_sample!=0;}
	bool experiment_selected() const{return experiment_id!=0;}
	const ns_experiment_region_chooser_sample & current_sample(){if (cur_sample==0)throw ns_ex("No sample selected!"); else return *cur_sample;}
	const ns_experiment_region_chooser_region & current_region(){if (cur_region==0)throw ns_ex("No region selected!"); else return *cur_region;}

	void load(const unsigned long experiment_id,ns_sql & sql){
		if (experiment_id == 0)
			return;
		ns_worm_browser_output_debug(__LINE__,__FILE__,std::string("Loading experiment data for " + ns_to_string(experiment_id)));
		
		sql << "SELECT id,name,device_name FROM capture_samples WHERE experiment_id=" << experiment_id << " AND problem=0 AND censored=0 ORDER BY name ASC";
		ns_sql_result res;
		sql.get_rows(res);
		cur_region = 0;
		cur_sample = 0;
		samples.resize(res.size());
		experiment_strains.clear();
		for (unsigned int i = 0; i < res.size(); i++){
			samples[i].sample_id = atol(res[i][0].c_str());
			samples[i].sample_name = res[i][1];
			sql << "SELECT id,name, strain,excluded_from_analysis,censored FROM sample_region_image_info WHERE sample_id= " << res[i][0] << " AND censored=0 ORDER BY name ASC";
			ns_sql_result res2;
			sql.get_rows(res2);
			samples[i].regions.resize(res2.size());
			samples[i].device = res[i][2];
			for (unsigned int j = 0; j < res2.size(); j++){
				samples[i].regions[j].region_id = atol(res2[j][0].c_str());
				samples[i].regions[j].region_name = res2[j][1];
				samples[i].regions[j].display_name = res[i][1] + "::" + res2[j][1];
				samples[i].regions[j].excluded = res2[j][3]!="0";
				samples[i].regions[j].censored = res2[j][4]!="0";
				ns_region_metadata m;
				m.load_from_db(samples[i].regions[j].region_id,"",sql);
				std::string d(m.device_regression_match_description());
				if (experiment_strains.find(d) == experiment_strains.end())
					experiment_strains[d] = m;
				samples[i].regions[j].region_metadata = &experiment_strains[d];
			}
		}
	}
	std::vector<ns_experiment_region_chooser_sample> samples;
	std::vector<std::vector<ns_experiment_region_selector_experiment_info> > experiment_groups;
	typedef std::map<std::string,ns_region_metadata> ns_experiment_strain_list;
	ns_experiment_strain_list experiment_strains;

	void load_experiment_names(ns_sql & sql){
		ns_worm_browser_output_debug(__LINE__,__FILE__,std::string("Loading experiment names from db"));
		
		std::map<unsigned long,vector<ns_experiment_region_selector_experiment_info> > experiments_by_group;
		experiment_groups.resize(0);
		sql << "SELECT id,name,group_id FROM experiments WHERE hidden = 0 ORDER BY first_time_point DESC";
		ns_sql_result res;
		sql.get_rows(res);
		for (unsigned int i = 0; i < res.size(); i++)
			experiments_by_group[atol(res[i][2].c_str())].push_back(ns_experiment_region_selector_experiment_info(atol(res[i][0].c_str()),res[i][1],atol(res[i][2].c_str())));
		sql << "SELECT group_id,group_name,hidden FROM experiment_groups WHERE hidden=0 ORDER BY group_order ASC";
		sql.get_rows(res);
		experiment_groups.resize(res.size()+1);
		for (unsigned int i = 0; i < res.size(); i++){
			const unsigned long cur_group_id(atol(res[i][0].c_str()));
			experiment_groups[i].reserve(experiments_by_group[cur_group_id].size());
			for (unsigned int j = 0; j < experiments_by_group[cur_group_id].size(); j++){
				experiment_groups[i].push_back(experiments_by_group[cur_group_id][j]);
				experiment_groups[i].rbegin()->experiment_group_name = res[i][1];
			}
		}
		experiment_groups.rbegin()->reserve(experiments_by_group[0].size());	
		for (unsigned int j = 0; j < experiments_by_group[0].size(); j++){
				experiment_groups.rbegin()->push_back(experiments_by_group[0][j]);
				experiment_groups.rbegin()->rbegin()->experiment_group_name = "No Group";
			}
	}

	bool get_experiment_info(const unsigned long id, ns_experiment_region_selector_experiment_info & info) const{
		for (unsigned int i = 0; i < experiment_groups.size(); i++)
			for (unsigned int j = 0; j < experiment_groups[i].size(); j++){
				if (id == experiment_groups[i][j].experiment_id){
					info = experiment_groups[i][j];
					return true;
				}
			}
			info.experiment_group_id = 0;
			info.experiment_id = 0;
			info.experiment_group_name.clear();
			info.name = std::string("Unknown Experiment ID:") + ns_to_string(id);
		return false;
	}
	bool get_experiment_name(const unsigned long id,std::string & name) const {
		ns_experiment_region_selector_experiment_info info;
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Getting experiment info");
		bool res(get_experiment_info(id, info));
		name = info.name;
		return res;
	}
	
};


struct ns_button_press{
	typedef enum{ns_down,ns_up,ns_drag} ns_click_type;
	ns_click_type click_type;
	bool right_button,
		 shift_key_held,
		 control_key_held;
	ns_vector_2i screen_position,
				 image_position,
				 screen_distance_from_click_location,
				 image_distance_from_click_location;
};

class ns_gl_window_data{
public:
	ns_gl_window_data(const string & window_name):gl_buffer(0),display_lock(string("ns_lock::display_") + window_name),redraw_requested(false),display_rescale_factor(1 / ns_death_time_solo_posture_annotater_timepoint::ns_resolution_increase_factor),
		dynamic_range_rescale_factor(1),worm_image_size(10,10),telemetry_size(10,10), gl_image_size(10,10),image_zoom(1), pre_gl_downsample(1){}
	ns_vector_2i worm_image_size,
		telemetry_size,
		gl_image_size;
	float image_zoom;
	unsigned int pre_gl_downsample;
	ns_8_bit * gl_buffer;
	ns_image_properties gl_buffer_properties;
	ns_lock display_lock;
	float display_rescale_factor;
	float dynamic_range_rescale_factor;

	bool redraw_requested;
	void redraw_screen(){
		redraw_requested = true;
	}

	//void redraw_screen(const unsigned long w, const unsigned long h,const bool resize=true){
	//	redraw_requested=true;
	//}	
};

bool ns_set_animation_state(bool state);
ns_thread_return_type ns_asynch_handle_file_request(void * fname);

struct ns_mask_info{
	ns_64_bit image_id,mask_id;
};
class ns_worm_learner{
public:
	typedef enum{ns_draw_boxes,
		ns_annotate_death_times_in_time_aligned_posture,
		ns_annotate_death_times_in_death_aligned_posture,
		ns_annotate_death_times_in_region,
		ns_annotate_storyboard_region,
		ns_annotate_storyboard_sample,
		ns_annotate_storyboard_experiment
	} ns_behavior_mode;

	

	void set_behavior_mode(const ns_behavior_mode t){
		behavior_mode = t;
		//if (t == ns_draw_boxes)
		//	ns_set_main_window_annotation_controls_activity(false);
		//else 
			ns_set_main_window_annotation_controls_activity(true);
	}
	ns_behavior_mode current_behavior_mode(){
		return behavior_mode;
	}
	ns_worm_learner():behavior_mode(ns_draw_boxes),mask_analyzer(4096), process_mask_menu_displayed(false),
		worm_detection_results(0),model_specification(&default_model),last_annotation_type_loaded(ns_death_time_annotation_set::ns_no_annotations),
		current_image_lock("ns_worm_learner::current_image"),
		movement_data_is_strictly_decreasing_(false),overwrite_existing_mask_when_submitting(false),output_svg_spines(false),static_mask(0),generate_mp4_(false),
		/*submit_capture_specification_to_db_when_recieved(false),*/overwrite_submitted_capture_specification(false),maximum_window_size(1024,768),
		current_annotater(&death_time_annotater),storyboard_annotater(2),main_window("Main Window"), persistant_sql_connection(0), persistant_sql_lock("psl"), show_testing_menus(false),
				worm_window("Worm Window"){
		storyboard_annotater.set_resize_factor(2);
	}

	~ns_worm_learner();
	std::vector<std::string> databases_available;
	void load_databases(ns_sql & sql){
		sql << "SHOW DATABASES";
		ns_sql_result database;
		sql.get_rows(database);
		for (unsigned int i = 0; i < database.size(); i++){
			if (database[i][0] == "test" ||
				database[i][0] == "information_schema" ||
				database[i][0] == "image_server_buffer" ||
				database[i][0] == "mysql")
				continue;
			databases_available.push_back(database[i][0]);
		}
	}
	void calculate_image_statistics_for_experiment_sample(unsigned long experiment_id,ns_sql & sql,bool overwrite_false=false);
	void generate_scanner_lifespan_statistics(bool use_by_hand_censoring,const std::vector<unsigned long> & experiment_ids, const std::string & output_filname);

	ns_image_properties current_image_properties() const{return current_image.properties();}
	//mask
	void produce_mask_file(const ns_bulk_experiment_mask_manager::ns_mask_type mask_type,const std::string & filename);
	void decode_mask_file(const std::string & filename, const std::string & output_vis_filename="");
	void submit_mask_file_to_cluster(const ns_bulk_experiment_mask_manager::ns_mask_type mask_type);
	void view_current_mask();
	void apply_mask_on_current_image();
	ns_mask_info send_mask_to_server(const ns_64_bit &sample_id);
	void load_mask(const std::string & filename,bool draw_to_screen=true);
	bool mask_loaded();
	void generate_scanner_report(unsigned long first_experiment_time, unsigned long last_experiment_time);

	//void get_ip_and_port_for_mask_upload(std::string & ip_address,unsigned long & port);
	//image transformations
	void resize_image();
	void zhang_thinning();
	void compress_dark_noise();
	void two_stage_threshold(const bool & make_vis=false);
	void set_svm_model_specification(ns_worm_detection_model_cache::const_handle_t & spec);
	void run_binary_morpholgical_manipulations();
	void to_bw();
	void to_color();
	void show_edges();
	void difference_threshold();void apply_threshold();
	void apply_spatial_average();
	void remove_large_objects();
	void stretch_levels();
	void sharpen();
	void diff_encode(const std::string & f1, const std::string & f2);

	void stretch_levels_approx();

	
	void calculate_erosion_gradient();

	//movement analysis
	void generate_survival_curve_from_hand_annotations();
	void compare_machine_and_by_hand_annotations();
	void simulate_multiple_worm_clumps(const bool use_waiting_time_cropping,const bool require_nearly_slow_moving);
	void calculate_heatmap_overlay();
	void calculate_movement_threshold(const std::string & filename, const bool & visulization=false);
	void test_time_path_analysis_parameters(unsigned long region_id);

	//void run_temporal_inference();
	void calculate_vertical_offset(const std::string & filename);
	void characterize_movement(const std::string & filename, const std::string & filename_2="");
	void characterize_precomputed_movement(const unsigned long short_1_id, const unsigned long short_2_id, const unsigned long long_id);
	void analyze_time_path(const unsigned long region_id);

	//data analysis
	void generate_single_frame_posture_image_pixel_data(const bool single_Region);
	void compile_experiment_survival_and_movement_data(bool use_by_hand_censoring,const ns_region_visualization & vis,const  ns_movement_data_source_type::type & type);
	void load_current_experiment_movement_results(const ns_death_time_annotation_set::ns_annotation_type_to_load & annotations_to_load, const unsigned long experiment_id);
	void output_experiment_movement_graph_wrapper_files(ns_machine_analysis_region_data & r, const std::string & filename);
	bool movement_data_is_strictly_decreasing(){return movement_data_is_strictly_decreasing_;}
	void output_device_timing_data(const unsigned long experiment_id,const unsigned long experiment_group_id);
	void output_region_statistics(const unsigned long experiment_id,const unsigned long experiment_group_id);

	void generate_morphology_statistics(const ns_64_bit & experiment_id);

	void create_feature_time_series(const std::string & directory);

	
	//void analyze_image_path(const unsigned long region_id);

	
	void export_experiment_data(const unsigned long experiment_id);
	bool import_experiment_data(const std::string & database_name,const std::string & directory, const bool reuse_database);
	
	typedef enum{ns_lifespan,ns_thermotolerance,ns_quiecent ,ns_v2} ns_parameter_set_range;

	void output_movement_analysis_optimization_data(int software_version_number,const ns_parameter_set_range & range, bool run_posture, bool run_expansion);

	typedef enum{ns_quantification_summary,ns_quantification_detailed,ns_quantification_detailed_with_by_hand, ns_build_worm_markov_posture_model_from_by_hand_annotations,ns_quantification_abbreviated_detailed} ns_movement_quantification_type;
	void generate_experiment_movement_image_quantification_analysis_data(ns_movement_quantification_type  detail_level);

	void generate_training_set_from_by_hand_annotation();
	void generate_detailed_animal_data_file();


	void generate_worm_storyboard(const std::string & filename,const bool use_by_hand_timings,const ns_movement_event & event_to_mark,unsigned long region_id, unsigned long sample_id, unsigned long experiment_id,ns_sql & sql, const ns_region_metadata & strain_to_use=ns_region_metadata(),bool use_absolute_time=true);
	

	//etc
	void generate_mp4(const bool & g){
		cout << (g?"Generating MP4 Videos":"Generating WMV Videos");
		generate_mp4_ = g;
	}
	void overwrite_submitted_specification(const bool & g){
		cout << (g?"Existing experiments will be overwritten if necessary.":"Existing experiments will never be overwritten.");
		overwrite_submitted_capture_specification = g;
	}
	void overwrite_existing_masks(const bool & g){
		cout << (g?"Existing masks will be overwritten if necessary.":"Existing masks will not be overwritten.");
		overwrite_existing_mask_when_submitting = g;
	}
	
	void output_lots_of_worms(const std::string & path);
	void test_image_metatadata();
	
	ns_64_bit create_experiment_from_directory_structure(const std::string & directory_name,const bool process_masks_locally);
	void rebuild_experiment_samples_from_disk(const ns_64_bit experiment_id);
	void rebuild_experiment_regions_from_disk(const ns_64_bit experiment_id);

	bool show_testing_menus;
	//machine learning
	void grayscale_from_blue();
	bool start_death_time_annotation(const ns_behavior_mode m,const ns_experiment_storyboard_spec::ns_storyboard_flavor & f);
	void stop_death_time_annotation();
	void display_splash_image();
	//void repair_image_headers(const std::string & filename);
	void generate_training_set_image();
	void process_training_set_image();
	void output_learning_set(const std::string & directory, const bool & process = false);
	ns_training_file_generator training_file_generator;
	void train_from_data(const std::string & base_dir);
	void decimate_folder(const std::string & filename);
	void translate_f_score_file(const std::string & filename);

	void concatenate_time_series(const std::string & directory,const float resize_factor);

	void output_subregion_as_test_set(const std::string & old_info);
	void input_subregion_as_new_experiment(const std::string &new_info);

	//worm detection

	void detect_and_output_objects();
	void make_object_collage();

	void make_spine_collage(const bool svg=false,const std::string & svg_directory="");
	void make_spine_collage_with_stats(const bool svg=false,const std::string & svg_directory="");
	void make_reject_spine_collage(const bool svg=false,const std::string & svg_directory="");
	void make_reject_spine_collage_with_stats(const bool svg=false,const std::string & svg_directory="");

	void process_contiguous_regions();
	void output_distributions_of_detected_objects(const std::string &directory);

	//Region area identification
	void output_area_info(const std::string & filename);
	void clear_areas();

	//GUI
	const std::string & get_experiment_name(const unsigned long id);
	std::vector<ns_worm_detection_model_cache::const_handle_t>  model_specifications;
	const ns_svm_model_specification & get_svm_model_specification();

	void draw_image(const double x, const double y, ns_image_standard & image);
	void draw_line_on_overlay(const ns_vector_2i & a, const ns_vector_2i & b);
	void update_main_window_display();
	void draw();
	void touch_main_window_pixel(const ns_button_press & press);
	bool register_main_window_key_press(int key, const bool shift_key_held,const bool control_key_held,const bool alt_key_held);
	
	void draw_worm_window_image(ns_image_standard & image);
	void update_worm_window_display();
	void touch_worm_window_pixel(const ns_button_press & press);
	bool register_worm_window_key_press(int key, const bool shift_key_held,const bool control_key_held,const bool alt_key_held);

	//file i/o
	void load_file(const std::string & filename);
	void save_current_image(const std::string & filename);
	void paste_from_clipboard();
	void copy_to_clipboard();
	bool load_threshold_if_possible(const std::string & filename);
	template<ns_feature_intensity features>
	void load_16_bit(const std::string & filename){
		load_16_bit<features>(filename,current_image);
		draw();
	}
	template<ns_feature_intensity features>
	void load_16_bit(const std::string & filename,ns_image_standard & image){

		std::string extension = ns_dir::extract_extension(filename);
		//open jpeg
		if (extension == "jpg"){
			throw ns_ex("Cannot open 16 bit jpegs.");
		}
		//open tiff
		else if (extension == "tif" || extension == "tiff"){
			ns_tiff_image_input_file<ns_16_bit> tiff_in;
			tiff_in.open_file(filename);
			ns_image_stream_file_source<ns_16_bit > file_source(tiff_in);
			ns_image_process_16_bit<features, ns_image_stream_static_offset_buffer<ns_16_bit> > processor(512);
		//	ns_image_standard small_image;
		//	processor.set_small_image_output(small_image);
			ns_image_stream_binding< ns_image_process_16_bit<features, ns_image_stream_static_offset_buffer<ns_16_bit> >,
									 ns_image_standard > binding(processor,image,512);
			if (features == ns_features_are_light)
				processor.set_crop_value(200);

			file_source.pump(binding,512);
		}

	}
	void navigate_death_time_annotation(ns_image_series_annotater::ns_image_series_annotater_action action,bool asynch=false);
	void navigate_solo_worm_annotation(ns_death_time_solo_posture_annotater::ns_image_series_annotater_action action, bool asynch=false);
	void save_death_time_annotations(ns_sql & sql);
	bool prompt_to_save_death_time_annotations();
	void load_strain_metadata_into_database(const std::string filename);
	void upgrade_tables();
	template<class whole_image>
	void load_file(const std::string & filename, whole_image & image){
		thresholded_image.clear();
		detection_spatial_median.clear();
		detection_brightfield.clear();
		std::string extension = ns_dir::extract_extension(filename);
		//open jpeg
		if (extension == "jpg"){
			ns_jpeg_image_input_file<ns_8_bit> jpeg_in;
			jpeg_in.open_file(filename);
			ns_image_stream_file_source<ns_8_bit> file_source(jpeg_in);
			file_source.pump(image,128);
			current_filename = filename;
		}
		//open tiff
		else if (extension == "tif" || extension == "tiff"){
			ns_tiff_image_input_file<ns_8_bit> tiff_in;
			tiff_in.open_file(filename);
			ns_image_stream_file_source<ns_8_bit > file_source(tiff_in);
			file_source.pump(image,128);
			current_filename = filename;
		}
	}
	std::string worm_visualization_directory(){
		return std::string("e:\\worm_results\\results") + DIR_CHAR_STR + "spines";
	}
	bool output_svg_spines;

	void set_static_mask(ns_image_standard & im);

	void handle_file_request(const std::string & filename);

	const std::string & get_current_clipboard_filename() const {return current_clipboard_filename;}
	
	ns_experiment_region_selector data_selector;

	ns_death_time_posture_annotater death_time_annotater;
	ns_experiment_storyboard_annotater storyboard_annotater;
	ns_image_series_annotater * current_annotater;
	ns_death_time_solo_posture_annotater death_time_solo_annotater;
	bool worm_launch_finished;
	void save_current_area_selections();

	
	ns_image_standard animation;
	void draw_animation(const double & t);

	ns_vector_2i maximum_window_size;

	ns_gl_window_data main_window,
				worm_window;

	ns_experiment_storyboard_spec::ns_storyboard_flavor current_storyboard_flavor;
	ns_worm_detection_model_cache::const_handle_t default_model;
	std::string current_mask_filename;
	float dynamic_range_rescale;
	ns_sql & get_sql_connection();
	ns_lock persistant_sql_lock;

	ns_death_time_solo_posture_annotater_timepoint::ns_visualization_type solo_annotation_visualization_type;
private:
	ns_image_standard animation_temp;
	ns_death_time_annotation_set::ns_annotation_type_to_load last_annotation_type_loaded;
	void touch_main_window_pixel_internal(const ns_button_press & p);
	void touch_worm_window_pixel_internal(const ns_button_press & p);

	ns_behavior_mode behavior_mode;
	bool generate_mp4_;
	//bool submit_capture_specification_to_db_when_recieved;
	bool overwrite_submitted_capture_specification;
	bool overwrite_existing_mask_when_submitting;
	ns_image_standard * static_mask;

	std::string current_filename;
	ns_image_standard current_image;
	ns_image_standard detection_spatial_median;
	ns_image_standard detection_brightfield;
	ns_image_standard current_mask;
	ns_image_standard thresholded_image;

	ns_area_handler area_handler;

	ns_worm_detection_model_cache::const_handle_t * model_specification;

	ns_image_mask_analyzer<ns_8_bit > mask_analyzer;

	ns_worm_detector<ns_image_standard> worm_detector;

	ns_image_worm_detection_results * worm_detection_results;

	ns_bulk_experiment_mask_manager mask_manager;

	bool process_mask_menu_displayed;

	ns_lock current_image_lock;

	ns_machine_analysis_data_loader movement_results;

	bool movement_data_is_strictly_decreasing_;

	string current_clipboard_filename;
	ns_sql * persistant_sql_connection;

};


#endif
