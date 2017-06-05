#ifndef NS_EXPERIMENT_STORYBOARD_H
#define NS_EXPERIMENT_STORYBOARD_H

#include "ns_image.h"
#include "ns_sql.h"
#include "ns_survival_curve.h"
#include "ns_death_time_annotation_set.h"
#include "ns_image_server_results_storage.h"
#include "ns_xml.h"
#include "ns_detected_worm_info.h"

struct ns_by_hand_movement_annotation{
	ns_by_hand_movement_annotation():matched(false),loaded_from_disk(false){}
	ns_by_hand_movement_annotation(const ns_death_time_annotation & a, const bool loaded_from_disk_):annotation(a),matched(false),
		loaded_from_disk(loaded_from_disk_){}

	ns_death_time_annotation annotation;
	bool loaded_from_disk,
		 matched;
};
struct ns_experiment_storyboard_timepoint_element{
	ns_experiment_storyboard_timepoint_element():storyboard_time(0),storyboard_absolute_time(0),neighbor_group_id(0),neighbor_group_size(0),neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate(0),drawn_worm_bottom_right_overlay(false),drawn_worm_top_right_overlay(false){}

	ns_death_time_annotation event_annotation;

	ns_death_time_annotation annotation_whose_image_should_be_used;

	typedef std::vector<ns_by_hand_movement_annotation> ns_by_hand_movement_annotation_list;
	ns_by_hand_movement_annotation_list by_hand_movement_annotations_for_element;

	void specify_by_hand_annotations(const ns_death_time_annotation & sticky_properties, const std::vector<ns_death_time_annotation> & movement_events);
	
	void simplify_and_condense_by_hand_movement_annotations();

	ns_vector_2i position_on_time_point;
	ns_vector_2i event_object_size()const{return event_annotation.size;}
	ns_vector_2i event_image_size()const{return event_annotation.size+ns_worm_collage_storage::context_border_size()*2;}
	ns_vector_2i image_object_size()const{return annotation_whose_image_should_be_used.size;}
	ns_vector_2i image_image_size()const{return annotation_whose_image_should_be_used.size+ns_worm_collage_storage::context_border_size()*2;}
	ns_image_standard image;

	unsigned long storyboard_absolute_time;
	unsigned long storyboard_time;

	bool annotation_was_censored_on_loading; //temporary storage during annotaiton
	
	void write_metadata(const ns_64_bit division_id,ns_xml_simple_writer & xml) const;
	ns_64_bit from_xml_group(ns_xml_simple_object & group);
	unsigned long neighbor_group_id;
	unsigned long neighbor_group_size;
	unsigned long neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate;

	
	bool drawn_worm_bottom_right_overlay,
		 drawn_worm_top_right_overlay;

};

struct ns_experiment_storyboard_timepoint{
	unsigned long time;
	std::vector<ns_experiment_storyboard_timepoint_element> events;
	unsigned long sub_image_id;
	ns_vector_2i position_on_storyboard,
				  size;

	void load_images(bool use_color,ns_sql & sql);
	void clear_images();
	void calculate_worm_positions();
};

struct ns_division_count{
	unsigned long group_number,
				 group_population;

	unsigned long start_index,
				  size;

	unsigned long division_time_length;
};

struct ns_experiment_storyboard_spec{
	ns_experiment_storyboard_spec():region_id(0),sample_id(0),experiment_id(0),use_by_hand_annotations(false),use_absolute_time(false),delay_time_after_event(0),minimum_distance_to_juxtipose_neighbors(50){}
	ns_experiment_storyboard_spec(const unsigned long & r,const unsigned long & s, const unsigned long & e, 
								  const bool & use_by_hand,const ns_movement_event & etype,const ns_region_metadata &strain, const bool & use_abs, const bool & delay_time_after_ev,const unsigned long minimum_distance_to_juxtipose_neighbors_):
	region_id(r),sample_id(s),experiment_id(e),strain_to_use(strain),delay_time_after_event(delay_time_after_ev),
	use_absolute_time(use_abs),event_to_mark(etype),use_by_hand_annotations(use_by_hand),minimum_distance_to_juxtipose_neighbors(minimum_distance_to_juxtipose_neighbors_){}
	

	
	typedef enum {ns_inspect_for_non_worms,ns_inspect_for_multiworm_clumps, ns_number_of_flavors} ns_storyboard_flavor;

	void set_flavor(const ns_storyboard_flavor & f);

	ns_64_bit region_id;
	ns_64_bit sample_id;
	ns_64_bit experiment_id;  
	bool use_by_hand_annotations;
	ns_movement_event event_to_mark;
	ns_region_metadata strain_to_use;
	bool use_absolute_time;
	unsigned long delay_time_after_event;
	bool choose_images_from_time_of_last_death;

	unsigned long minimum_distance_to_juxtipose_neighbors;

	void add_to_xml(ns_xml_simple_writer & xml) const;
	void from_xml_group(ns_xml_simple_object & group);

	void clear(){
		region_id = sample_id = experiment_id = 0;
		use_by_hand_annotations = use_absolute_time = false;
		event_to_mark = ns_no_movement_event;
		strain_to_use.clear();
	}
};
class ns_experiment_storyboard_manager;

struct ns_reg_info{
	ns_reg_info(){}
	ns_reg_info(ns_64_bit wd, ns_64_bit regid):worm_detection_results_id(wd),region_image_id(regid){}
	ns_64_bit worm_detection_results_id,
				region_image_id;
};

class ns_experiment_storyboard_compiled_event_set {
	public:
		ns_experiment_storyboard_compiled_event_set();
		ns_death_time_annotation_compiler all_events;	
		unsigned long last_timepoint_in_storyboard;
		unsigned long number_of_regions_in_storyboard;
		void load(const ns_experiment_storyboard_spec & spec, ns_sql & sql);
		bool need_to_reload_for_new_spec(const ns_experiment_storyboard_spec & new_spec);
	private:
		ns_experiment_storyboard_spec spec;
};

class ns_experiment_storyboard{

public:
	ns_experiment_storyboard(){clear();}
	typedef enum {ns_creating_from_machine_annotations,ns_loading_from_storyboard_file} ns_loading_type;
	bool load_events_from_annotation_compiler(const ns_loading_type & loading_type,const ns_death_time_annotation_compiler & all_events,const bool use_absolute_time,const bool state_annotations_available_in_loaded_annotations,const unsigned long minimum_distance_to_juxtipose_neighbors, ns_sql & sql);

	void draw(const unsigned long sub_image_id,ns_image_standard & im,bool use_color,ns_sql & sql);

	bool create_storyboard_metadata_from_machine_annotations(ns_experiment_storyboard_spec spec, const ns_experiment_storyboard_compiled_event_set & compiled_event_set, ns_sql & sql);
	void check_that_all_time_path_information_is_valid(ns_sql & sql);
	void save_by_hand_annotations(ns_sql & sql,const ns_death_time_annotation_set & extra_annotations) const;

	static std::string image_suffix(const ns_experiment_storyboard_spec & spec);

	void write_metadata(std::ostream & o) const;
	bool read_metadata(std::istream & i, ns_sql & sql);

	ns_ex compare(const ns_experiment_storyboard & s);
	void clear(){
		first_time = last_time = 0;
		time_of_last_death = 0;
		last_timepoint_in_storyboard = 0;
		number_of_regions_in_storyboard = 0;
		worm_images_size.resize(0);
		subject_specification.clear();
		divisions.resize(0);
		orphan_by_hand_annotations.clear();
		region_annotation_file_cache.clear();
	};

	const ns_experiment_storyboard_spec & subject() const{
		return subject_specification;
	}

	const ns_experiment_storyboard_timepoint_element & find_animal(const ns_64_bit region_info_id,const ns_stationary_path_id & id) const;
	
	std::vector<ns_experiment_storyboard_timepoint> divisions;
	std::vector<ns_death_time_annotation> orphan_by_hand_annotations;
	unsigned long  number_of_sub_images()const{return worm_images_size.size();}

	unsigned long last_timepoint_in_storyboard;
	unsigned long number_of_regions_in_storyboard;
	unsigned long time_of_last_death;

	private:

		void build_worm_detection_id_lookup_table(ns_sql & sql);
		void prepare_to_draw(ns_sql & sql);
		//worm_detection_id_lookup[region_info_id][time] = info
		std::map<ns_64_bit, std::map<ns_64_bit,  ns_reg_info> > worm_detection_id_lookup;
		unsigned long first_time,
				  last_time;
		std::vector<ns_vector_2i> worm_images_size;
		std::vector<ns_vector_2i>::size_type number_of_images_in_storyboard()const{return worm_images_size.size();}

	typedef std::map<ns_64_bit,ns_image_server_results_file> ns_region_annotation_file_cache_type;	
	mutable ns_region_annotation_file_cache_type  region_annotation_file_cache;


	ns_experiment_storyboard_spec subject_specification;

	void calculate_worm_positions();

	static ns_division_count count_of_animals_in_division(const unsigned long number_of_divisions,const std::vector<unsigned long > & values,unsigned long start_index,unsigned long size);
	unsigned long label_margin_height();
	void draw_label_margin(const unsigned long sub_image_id,bool use_color,ns_image_standard & im);
};


class ns_experiment_storyboard_manager{
public:
	bool load_metadata_from_db(const ns_experiment_storyboard_spec & spec, ns_experiment_storyboard & storyboard, ns_sql & sql);
	bool load_image_from_db(const unsigned long sub_image_id,const ns_experiment_storyboard_spec & spec, ns_image_standard & im,ns_sql & sql);
	void save_metadata_to_db(const ns_experiment_storyboard_spec & spec, const ns_experiment_storyboard & storyboard, const ns_image_type & metadata_suffix,ns_sql & sql);
	void save_image_to_db(const unsigned long sub_image_id,const ns_experiment_storyboard_spec & spec, const ns_image_standard & im, ns_sql & sql);
	void delete_metadata_from_db(const ns_experiment_storyboard_spec & spec, ns_sql & sql);
	unsigned long number_of_sub_images() const{return sub_images.size();}

private:
	std::vector<ns_image_server_image> sub_images;
	ns_image_server_image xml_metadata_database_record;
	void load_metadata_from_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	void save_metadata_to_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	bool load_subimages_from_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	void create_records_and_storage_for_subimages(const unsigned long number_of_subimages,const ns_experiment_storyboard_spec & spec,ns_sql & sql, const bool create_if_missing);

	static std::string generate_sql_query_where_clause_for_specification(const ns_experiment_storyboard_spec & spec);

	void get_default_storage_base_filenames(const unsigned long subimage_id,ns_image_server_image & image, const ns_image_type & type,
		const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	
	ns_image_server_image & get_storage_for_storyboard(const unsigned long subimage_id, const ns_experiment_storyboard_spec & spec,ns_sql & sql);
};



#endif
