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
	ns_experiment_storyboard_timepoint_element():storyboard_time(0),storyboard_absolute_time(0),neighbor_group_id(0),neighbor_group_size(0),neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate(0){}

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
	
	void write_metadata(const unsigned long division_id,ns_xml_simple_writer & xml) const;
	unsigned long from_xml_group(ns_xml_simple_object & group);
	unsigned long neighbor_group_id;
	unsigned long neighbor_group_size;
	unsigned long neighbor_group_id_of_which_this_element_is_an_in_situ_duplicate;
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

	unsigned long region_id;
	unsigned long sample_id;
	unsigned long experiment_id;  
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
	ns_reg_info(unsigned long wd, unsigned long regid):worm_detection_results_id(wd),region_image_id(regid){}
	unsigned long worm_detection_results_id,
				region_image_id;
};

class ns_experiment_storyboard{

public:
	typedef enum {ns_creating_from_machine_annotations,ns_loading_from_storyboard_file} ns_loading_type;
	bool load_events_from_annotation_compiler(const ns_loading_type & loading_type,ns_death_time_annotation_compiler & all_events,const bool use_absolute_time,const unsigned long minimum_distance_to_juxtipose_neighbors, ns_sql & sql);

	void draw(const unsigned long sub_image_id,ns_image_standard & im,bool use_color,ns_sql & sql);

	bool create_storyboard_metadata_from_machine_annotations(ns_experiment_storyboard_spec spec, ns_sql & sql);
	void save_by_hand_annotations(ns_sql & sql,const ns_death_time_annotation_set & extra_annotations) const;

	static std::string image_suffix(const ns_experiment_storyboard_spec & spec);

	void write_metadata(std::ostream & o) const;
	void read_metadata(std::istream & i, ns_sql & sql);

	void clear(){
		first_time = last_time = 0;
		worm_images_size.resize(0);
		subject_specification.clear();
		divisions.resize(0);
		orphan_by_hand_annotations.clear();
		region_annotation_file_cache.clear();
	};

	const ns_experiment_storyboard_spec & subject() const{
		return subject_specification;
	}

	const ns_experiment_storyboard_timepoint_element & find_animal(const unsigned long region_info_id,const ns_stationary_path_id & id) const;
	
	std::vector<ns_experiment_storyboard_timepoint> divisions;
	std::vector<ns_death_time_annotation> orphan_by_hand_annotations;
	unsigned long  number_of_sub_images()const{return worm_images_size.size();}

	unsigned long last_timepoint_in_storyboard;
	unsigned long number_of_regions_in_storyboard;

	private:
		void prepare_to_draw(ns_sql & sql);
		std::map<unsigned long, std::map<unsigned long,  ns_reg_info> > worm_detection_id_lookup;
		unsigned long first_time,
				  last_time;
		std::vector<ns_vector_2i> worm_images_size;
		std::vector<ns_vector_2i>::size_type number_of_images_in_storyboard()const{return worm_images_size.size();}

	typedef std::map<unsigned long,ns_image_server_results_file> ns_region_annotation_file_cache_type;	
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
	void save_metadata_to_db(const ns_experiment_storyboard_spec & spec, const ns_experiment_storyboard & storyboard, const std::string & metadata_suffix,ns_sql & sql);
	void save_image_to_db(const unsigned long sub_image_id,const ns_experiment_storyboard_spec & spec, const ns_image_standard & im, ns_sql & sql);
	void delete_metadata_from_db(const ns_experiment_storyboard_spec & spec, ns_sql & sql);
	unsigned long number_of_sub_images() const{return sub_images.size();}

private:
	std::vector<ns_image_server_image> sub_images;
	ns_image_server_image metadata;
	void load_metadata_from_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	void save_metadata_to_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	bool load_subimages_from_db(const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	void create_records_and_storage_for_subimages(const unsigned long number_of_subimages,const ns_experiment_storyboard_spec & spec,ns_sql & sql, const bool create_if_missing);
	void get_default_storage_base_filenames(const unsigned long subimage_id,ns_image_server_image & image, 
		const ns_experiment_storyboard_spec & spec,ns_sql & sql);
	
	ns_image_server_image & get_storage_for_storyboard(const unsigned long subimage_id, const ns_experiment_storyboard_spec & spec,ns_sql & sql);
};



#endif
