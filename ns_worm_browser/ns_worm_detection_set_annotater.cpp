#include "ns_worm_detection_set_annotater.h"
#include "ns_worm_browser.h"

void ns_worm_detection_set_annotater::draw_metadata(ns_annotater_timepoint* tp_a, ns_image_standard& im, double external_rescale_factor) {

	for (unsigned int y = im.properties().height-bottom_border_height(); y < im.properties().height; y++) {
		for (unsigned int x = 0; x < 3 * im.properties().width; x++) {
			(im)[y][x] = 0;
		}
	}


	ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
	ns_font & font (font_server.get_default_font());
	font.set_height(bottom_text_size());
	unsigned long bottom_border_pos = im.properties().height - bottom_border_height()+ bottom_text_size();
	for (unsigned int i = 0; i < all_objects[current_timepoint_id].objects.size(); i++) {
		const  long x(all_objects[current_timepoint_id].object_positions[i].x);
		const ns_annotated_training_set_object& object = *all_objects[current_timepoint_id].objects[i];
		const unsigned long thickness(3);
		const unsigned long thickness_2(1);
		const unsigned long thickness_offset(1);

		ns_color_8 color;
		std::string text;
		if (object.hand_annotation_data.identified_as_a_worm_by_human) {
			color = ns_color_8(0, 0, 0);
			text = "worm";
		}
		else if (object.hand_annotation_data.identified_as_misdisambiguated_multiple_worms) {
			color = ns_color_8(0, 0, 255);
			text = "censored";
		}
		else {
			color = ns_color_8(255, 255, 255);
			text = "non-worm";
		}

		draw_box(ns_vector_2i(x,0), object.object.context_image_size, color, im, thickness);
		draw_box(ns_vector_2i(x, 0), object.object.context_image_size, ns_color_8(0, 0, 0), im, thickness_2);

	
	//	ns_font_output_dimension dim = font.draw_color(1, bottom_border_pos, ns_color_8(255, 255, 255), text, im,false);
	//	long centered_x = x - (long)dim.w / 2;
	//	if (centered_x < 0)
	//		centered_x = 0;
	//	font.draw_color(centered_x, bottom_border_pos, ns_color_8(255, 255, 255), text, im, false);

	}


	std::string pos;
	pos += ns_to_string(current_timepoint_id + 1) + " of " + ns_to_string(all_objects.size());
	font.draw_color(1, im.properties().height- bottom_text_size(), ns_color_8(255, 255, 255), pos, im);
		
}

void ns_worm_detection_set_annotater_object::load_image(const unsigned long bottom_height, ns_annotater_image_buffer_entry& im, ns_sql& sql, ns_simple_local_image_cache& image_cache, ns_annotater_memory_pool& memory_pool, const unsigned long resize_factor_) {

	im.loaded = false;
	ns_image_properties image_prop(300, 800, 3);
	const unsigned long spacing(4);
	object_positions.resize(objects.size(), ns_vector_2i(0, spacing));
	for (unsigned int i = 0; i < objects.size(); i++) {
		ns_image_properties p(objects[i]->object.context_image().absolute_grayscale.properties());
		if (p.height + ns_worm_detection_set_annotater::bottom_border_height() > image_prop.height)
			image_prop.height = p.height + ns_worm_detection_set_annotater::bottom_border_height();
		if (object_positions[i].x+p.width + ns_worm_detection_set_annotater::side_border_width() > image_prop.width)
			image_prop.width = object_positions[i].x + p.width + ns_worm_detection_set_annotater::side_border_width();
		if (i + 1 < objects.size())
			object_positions[i + 1] = object_positions[i] + ns_vector_2i(p.width+spacing, 0);
	}
	im.im->resize(image_prop);

	for (unsigned int i = 0; i < objects.size(); i++) {
		ns_image_properties p(objects[i]->object.context_image().absolute_grayscale.properties());
		for (unsigned int y = 0; y < p.height; y++) {
			for (unsigned int x = 0; x < p.width; x++) {
				char r, bg;
				switch (worm_detection_annotater->current_display_method) {
				case ns_show_highlighted_worm: {
					float m = objects[i]->object.context_image().absolute_grayscale[y][x];

					float f;
					if (x < objects[i]->object.get_region_offset_in_context_image().x ||
						y < objects[i]->object.get_region_offset_in_context_image().y ||
						x >= objects[i]->object.get_region_offset_in_context_image().x + objects[i]->object.region_size.x ||
						y >= objects[i]->object.get_region_offset_in_context_image().y + objects[i]->object.region_size.y)
						f = 0;
					else 
						f = (objects[i]->object.bitmap()[y-objects[i]->object.get_region_offset_in_context_image().y][x-objects[i]->object.get_region_offset_in_context_image().x] ? .3 : 0);
					r = m * (1 - f) + 255 * f;  //goes up to 255 the more movement there is
					bg =m * (1 - f);		   //goes down to zero the more movement there is.
				}
				break;
				case ns_show_absolute_grayscale: r = bg = objects[i]->object.context_image().absolute_grayscale[y][x];
					break;
				case ns_show_relative_grayscale: r = bg = objects[i]->object.context_image().relative_grayscale[y][x];
					break;
				}
				(*im.im)[y][3 * (object_positions[i].x + x) + 0] = r;
				(*im.im)[y][3 * (object_positions[i].x + x) + 1] = bg;
				(*im.im)[y][3 * (object_positions[i].x + x) + 2] = bg;
			}

			for (unsigned int x = 0; x < 3*spacing; x++) 
				(*im.im)[y][3 * (object_positions[i].x + p.width) + x] = 0;
		}
	
		for (unsigned int y = p.height; y < image_prop.height; y++) {
			for (unsigned int x = 0; x < p.width+spacing; x++) {
				(*im.im)[y][3 * (object_positions[i].x + x)] = 0;
				(*im.im)[y][3 * (object_positions[i].x + x) + 1] = 0;
				(*im.im)[y][3 * (object_positions[i].x + x) + 2] = 0;
			}
		}
	}
	for (unsigned int y = 0; y < image_prop.height; y++) {
		for (unsigned int x = object_positions.rbegin()->x + (*objects.rbegin())->object.context_image().absolute_grayscale.properties().width; x < image_prop.width; x++) {
			(*im.im)[y][3*x] = 0;
			(*im.im)[y][3 * x+1] = 0;
			(*im.im)[y][3 * x+2] = 0;
		}
	}
	im.loaded = true;
}

void ns_worm_detection_set_annotater::register_click(const ns_vector_2i& image_position, const ns_click_request& action, double external_rescale_factor) {
	if (all_objects.size() == 0)
		return;
	ns_acquire_lock_for_scope lock(image_buffer_access_lock, __FILE__, __LINE__);
	bool found_object(false);
	unsigned long selected_object(0);
	std::vector<ns_annotated_training_set_object*>& current_objects(all_objects[current_timepoint_id].objects);
	std::vector<ns_vector_2i>& object_positions(all_objects[current_timepoint_id].object_positions);
	for (unsigned int i = 0; i < current_objects.size(); i++) {
		if (object_positions[i].x > image_position.x || object_positions[i].y > image_position.y)
			continue;
		if (object_positions[i].x + current_objects[i]->object.context_image_size.x < image_position.x ||
			object_positions[i].y + current_objects[i]->object.context_image_size.y < image_position.y)
			continue;
		found_object = true;
		selected_object = i;
	}
	switch (action) {
	case ns_increase_contrast:
		worm_learner->main_window.dynamic_range_rescale_factor += .1;
		break;
	case ns_decrease_contrast:
		worm_learner->main_window.dynamic_range_rescale_factor -= .1;
		if (worm_learner->main_window.dynamic_range_rescale_factor < .1)
			worm_learner->main_window.dynamic_range_rescale_factor = .1;
		break;
	case ns_load_worm_details:
		break;
	case ns_censor_all:
	case ns_cycle_state:
	case ns_censor:
		if (!found_object) {
			switch (current_display_method) {
			case ns_worm_detection_set_annotater_object::ns_show_highlighted_worm:
				current_display_method = ns_worm_detection_set_annotater_object::ns_show_absolute_grayscale;
			case ns_worm_detection_set_annotater_object::ns_show_absolute_grayscale:
				current_display_method = ns_worm_detection_set_annotater_object::ns_show_relative_grayscale;
			case ns_worm_detection_set_annotater_object::ns_show_relative_grayscale:
				current_display_method = ns_worm_detection_set_annotater_object::ns_show_highlighted_worm;
			default:
				current_display_method = ns_worm_detection_set_annotater_object::ns_show_absolute_grayscale;
			}
			break;
		}
		current_objects[selected_object]->hand_annotation_data.identified_as_a_worm_by_human = !current_objects[selected_object]->hand_annotation_data.identified_as_a_worm_by_human;
		saved_ = false;
		break;
	case ns_annotate_extra_worm:
	case ns_cycle_flags:
		if (!found_object) break;
		current_objects[selected_object]->hand_annotation_data.identified_as_a_mangled_worm = !current_objects[selected_object]->hand_annotation_data.identified_as_a_worm_by_human;
		if (current_objects[selected_object]->hand_annotation_data.identified_as_a_mangled_worm)
			current_objects[selected_object]->hand_annotation_data.identified_as_a_worm_by_human = false;
		saved_ = false;
		break;

	default: throw ns_ex("ns_death_time_posture_annotater::Unknown click type");
	}
	
	saved_ = false;

	draw_metadata(&all_objects[current_timepoint_id], *current_image.im, external_rescale_factor);
	request_refresh();

	lock.release();
}
void ns_worm_detection_set_annotater::display_current_frame() {
		refresh_requested_ = false;
		ns_acquire_lock_for_scope lock(image_buffer_access_lock, __FILE__, __LINE__);
		if (current_image.im == 0)
			throw ns_ex("No frame loaded!");
		worm_learner->draw_image(-1, -1, *current_image.im);
		lock.release();
}

void ns_worm_detection_set_annotater::load_from_file(const std::string& path_, const std::string& filename_, ns_worm_learner* worm_learner_, double external_rescale_factor) {
	filename = filename_;
	path = path_;
	stop_fast_movement();
	clear();	
	if (sql.is_null())
		sql.attach(image_server.new_sql_connection(__FILE__, __LINE__));
	loading_temp.use_more_memory_to_avoid_reallocations(true);
	ns_load_image(path + DIR_CHAR_STR + filename, loading_temp);
	std::string extra_metadata;
	ns_worm_training_set_image::get_external_metadata(path, filename, extra_metadata);
	ns_worm_training_set_image::decode(loading_temp, objects, false, extra_metadata);

	const int number_of_objects_per_frame = 7;
	int n(objects.worms.size() + objects.non_worms.size() + objects.censored_worms.size());
	
	all_objects.resize((n% number_of_objects_per_frame == 0) ? n/number_of_objects_per_frame : (n/number_of_objects_per_frame+1));
	for (unsigned int i = 0; i < objects.worms.size(); i++)
		all_objects[i/ number_of_objects_per_frame].objects.push_back(objects.worms[i]);
	for (unsigned int i = 0; i < objects.non_worms.size(); i++)
		all_objects[(objects.worms.size() + i) / number_of_objects_per_frame].objects.push_back(objects.non_worms[i]);
	for (unsigned int i = 0; i < objects.censored_worms.size(); i++)
		all_objects[(objects.worms.size() + objects.non_worms.size() + i) / number_of_objects_per_frame].objects.push_back(objects.censored_worms[i]);
	for (unsigned int i = 0; i < all_objects.size(); i++)
		all_objects[i].worm_detection_annotater = this;

	for (unsigned long i = 0; i < all_objects.size(); i++) {
		for (unsigned long j = 0; j < all_objects[i].objects.size(); j++) {
			if (all_objects[i].objects[j]->hand_annotation_data.identified_as_a_worm_by_machine)
				all_objects[i].objects[j]->hand_annotation_data.identified_as_a_worm_by_human = true;
			all_objects[i].objects[j]->hand_annotation_data.identified_as_a_worm_by_machine = false;
		}
	}

	if (current_image.im == 0)
		current_image.im = new ns_image_standard();
	current_timepoint_id = 0;
	all_objects[current_timepoint_id].load_image(0, current_image, sql(), local_image_cache, memory_pool, resize_factor);
	draw_metadata(&all_objects[current_timepoint_id], *current_image.im, external_rescale_factor);

	ns_image_properties prop(current_image.im->properties());
	//allocate image buffer
	if (previous_images.size() != max_buffer_size || next_images.size() != max_buffer_size) {
		previous_images.resize(max_buffer_size);
		next_images.resize(max_buffer_size);
		for (unsigned int i = 0; i < max_buffer_size; i++) {
			previous_images[i].im = memory_pool.get(prop);
			next_images[i].im = memory_pool.get(prop);
			previous_images[i].im->use_more_memory_to_avoid_reallocations(true);
			next_images[i].im->use_more_memory_to_avoid_reallocations(true);
		}
	}

}

void ns_worm_detection_set_annotater::save_annotations(const ns_death_time_annotation_set& extra_annotations) const {

	ns_load_image(path + DIR_CHAR_STR + filename, loading_temp);
	for (unsigned int i = 0; i < all_objects.size(); i++) {
		for (unsigned int j = 0; j < all_objects[i].objects.size(); j++) {
			ns_vector_2i p(all_objects[i].objects[j]->collage_position.pos);
			const ns_image_standard& box = ns_worm_training_set_image::check_box(all_objects[i].objects[j]->hand_annotation_data.dominant_label());
			for (unsigned int y = 0; y < box.properties().height; y++)
				for (unsigned int x = 0; x < 3 * box.properties().width; x++)
					loading_temp[p.y+y][3*p.x+x] = box[y][x];
		}
	}
	std::string fname_mod = ns_dir::extract_filename_without_extension(filename);
	fname_mod += "_mod.tif";
	ns_save_image(path + DIR_CHAR_STR + fname_mod, loading_temp);
	ns_update_worm_information_bar("Annotations saved at " + ns_format_time_string_for_human(ns_current_time()));
	saved_ = true;
};