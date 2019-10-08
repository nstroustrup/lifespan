#ifndef NS_WORM_TRAINING_SET_IMAGE_H
#define NS_WORM_TRAINING_SET_IMAGE_H
#include "ns_detected_worm_info.h"
#include "ns_death_time_annotation_set.h"
#include "ns_vector.h"
#include "ns_survival_curve.h"


struct ns_annotated_training_set_object{
	unsigned long  capture_time;
	ns_64_bit region_info_id;
	ns_detected_worm_info object;
	ns_whole_image_statistic_specification whole_image_stats;
	ns_packed_collage_position collage_position;
	ns_object_hand_annotation_data hand_annotation_data;
	long debug_unique_id_in_source_collage;
	ns_vector_2i debug_position_in_source_collage;
};
class ns_annotated_training_set{
public:
	std::vector<ns_annotated_training_set_object *> worms;
	std::vector<ns_annotated_training_set_object *> non_worms;
	std::vector<ns_annotated_training_set_object *> censored_worms;
	std::vector<std::vector<std::vector<ns_annotated_training_set_object *> > > objects_grouped_by_multiple_worm_cluster_origin;
	std::vector <ns_annotated_training_set_object> objects;
	//std::vector<ns_packed_collage_position> worm_positions;
	~ns_annotated_training_set();
	void clear();
};

class ns_worm_training_set_image{
public:
	static void generate(const ns_image_worm_detection_results & results, ns_image_standard & out);
	static void generate(const ns_death_time_annotation_compiler_region & result, ns_image_standard & out, ns_sql & sql, ns_simple_local_image_cache& image_cache);

	static void decode(const ns_image_standard & in,ns_annotated_training_set & training_set, const bool allow_malformed_metadata, const std::string & extra_metadata);

	static const ns_image_standard & check_box(const ns_object_hand_annotation_data::ns_object_hand_annotation &type){
		if (check_boxes.size() < (int)ns_object_hand_annotation_data:: ns_number_of_object_hand_annotation_types)
			generate_check_boxes();
		return check_boxes[(int)type];
	}

	static bool get_external_metadata(const std::string& path, const std::string& image_filename, std::string& extra_metadata);
private:
	
	
	static void generate_check_boxes();
	static void generate_check_box(const ns_color_8 & check_color, ns_image_standard & out);
	static std::vector<ns_image_standard> check_boxes;		

};

#include "ns_image.h"
void ns_transfer_annotations(const ns_image_standard & annotation_source,  const ns_image_standard & annotation_destination, ns_image_standard & output);
#endif
