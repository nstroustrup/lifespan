#ifndef NS_worm_detection_set_annotater_H
#define NS_worm_detection_set_annotater_H
#include "ns_image.h"
#include "ns_image_server.h"
#include "ns_image_series_annotater.h"
#include "ns_worm_training_set_image.h"

class ns_worm_detection_set_annotater;
class ns_worm_detection_set_annotater_object : public ns_annotater_timepoint {
private:
	ns_image_storage_source_handle<ns_8_bit> get_image(ns_sql& sql) {
		throw ns_ex("Data is stored in memory");
	}
public:
	void init() {}
	typedef enum { ns_show_highlighted_worm, ns_show_absolute_grayscale, ns_show_relative_grayscale } ns_display_method;
	ns_worm_detection_set_annotater* worm_detection_annotater;
	std::vector<ns_annotated_training_set_object*> objects;
	std::vector< ns_vector_2i > object_positions;
	void load_image(const unsigned long bottom_height, ns_annotater_image_buffer_entry& im, ns_sql& sql, ns_simple_local_image_cache& image_cache, ns_annotater_memory_pool& memory_pool, const unsigned long resize_factor_);
};

class ns_worm_learner;
class ns_worm_detection_set_annotater : public ns_image_series_annotater{
public:
	
	ns_worm_detection_set_annotater_object::ns_display_method current_display_method;

private:
	std::string filename, path;
	ns_annotated_training_set objects;
	std::vector<ns_worm_detection_set_annotater_object> all_objects;
	
	void draw_metadata(ns_annotater_timepoint* tp_a, ns_image_standard& im, double external_rescale_factor);



	enum {default_resize_factor=2,max_buffer_size = 6};

	mutable bool saved_;

	mutable ns_image_standard loading_temp;
public:
	static long side_border_width() { return 25; }
	static long bottom_border_height() { return 50; }
	static long bottom_text_offset_from_bottom() { return 6; }
	static long bottom_text_size() { return 16; }

	ns_worm_learner* worm_learner;
	inline ns_annotater_timepoint* timepoint(const unsigned long i) {
		return &all_objects[i];
	}
	inline unsigned long number_of_timepoints() {
		return all_objects.size();
	}

	void set_resize_factor(const unsigned long resize_factor_){
		resize_factor = resize_factor_;
	}
	bool data_saved()const{return saved_;}
	ns_worm_detection_set_annotater(const unsigned long res):ns_image_series_annotater(res,0), saved_(true){}
	~ns_worm_detection_set_annotater() {
	}

	std::string image_label(const unsigned long frame_id){
		return ns_to_string(frame_id) + "/" + ns_to_string(all_objects.size());	
	}

	bool load_annotations(){
		//annotations are loaded automatically when storyboard is loaded.
		return true;
	}
	void save_annotations(const ns_death_time_annotation_set& extra_annotations) const;
	void redraw_current_metadata(double external_resize_factor){
		draw_metadata(&all_objects[current_timepoint_id],*current_image.im, external_resize_factor);
	}

	static void draw_box(const ns_vector_2i& p, const ns_vector_2i& s, const ns_color_8& c, ns_image_standard& im, const unsigned long thickness) {
		im.draw_line_color_thick(p, p + ns_vector_2i(s.x, 0), c, thickness);
		im.draw_line_color_thick(p, p + ns_vector_2i(0, s.y), c, thickness);
		im.draw_line_color_thick(p + s, p + ns_vector_2i(s.x, 0), c, thickness);
		im.draw_line_color_thick(p + s, p + ns_vector_2i(0, s.y), c, thickness);
	}
	void load_from_file(const std::string& path, const std::string& filename, ns_worm_learner* worm_learner_, double external_rescale_factor);
	void redraw_all(const double& external_rescale_factor);
	void register_click(const ns_vector_2i& image_position, const ns_click_request& action, double external_rescale_factor);

	void display_current_frame();
	
	void clear() {
		clear_base();
		saved_ = false;
		all_objects.resize(0);
		objects.clear();
	}

};
#endif