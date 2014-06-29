#include "ns_heat_map_interpolation.h"
#include "ns_identify_contiguous_bitmap_regions.h"
#include "ns_worm_detector.h"
#include "ns_image_easy_io.h"
using namespace std;

#include "ns_xml.h"

void ns_worm_interpolation_timepoint::load_threshold(ns_sql & sql){
	ns_image_storage_source_handle<ns_8_bit> t(image_server.image_storage.request_from_storage(threshold_image,&sql));
	t.input_stream().pump(threshold,1024);
	//ns_identify_contiguous_bitmap_regions(threshold,object_manager.objects);
	//allow big objects but not small ones.
	//object_manager.constrain_region_area(
	//	ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,threshold.properties().resolution),
	//	100*ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,threshold.properties().resolution),
	//	100*ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,threshold.properties().resolution));
	//mark all objects to be disregarded in worm detection (we'll explicitly allow the ones we do want)
	//for (unsigned int k = 0; k < object_manager.objects.size(); k++)
	//	object_manager.objects[k]->must_not_be_a_worm = true;
	threshold_loaded = true;
}

void ns_worm_interpolation_timepoint::clear_threshold(){
	//object_manager.clear();
	threshold.clear();
	threshold_loaded = false;
}
/*
bool ns_worm_interpolation_timepoint::load_region_images(ns_sql & sql){
	try{
		detected_worms.load_images_from_db(region,sql);
		if (interpolated_worms.id != 0)
			interpolated_worms.load_images_from_db(region,sql,true);
		loaded_images = true;
		return true;
	}
	catch(ns_ex & ex){
		detected_worms.clear();
		interpolated_worms.clear();
		cerr << ex.text();
		return false;
	}
}
*/
/*
void ns_worm_interpolation_timepoint::clear_images(){
	detected_worms.clear();
	interpolated_worms.clear();
	loaded_images = false;
}*/
/*
void ns_worm_interpolation_timepoint::commit_interpolated_regions_to_db(const ns_image_standard * static_mask, std::vector<ns_reverse_worm_lookup> & corresponding_reverse_lookup_table_entry, ns_sql & sql){
	//remove all non-worms from record to speed things up
	for (std::vector<ns_detected_object *>::iterator p = object_manager.objects.begin(); p != object_manager.objects.end();)
		if ((*p)->must_not_be_a_worm){
			delete (*p);
			p = object_manager.objects.erase(p);
		}
		else p++;
	cerr << object_manager.objects.size() << " interpolated worms were found.\n";

	ns_worm_detector<ns_image_standard> detector;
	ns_image_standard dummy;
	throw ns_ex("Intensity not implemented");
	//now update database with newly found neighbors
	ns_image_worm_detection_results * results = detector.run(region.region_info_id,region.capture_time,object_manager,dummy, threshold,static_mask,ns_model_specification(),"",ns_detected_worm_info::ns_vis_raster,ns_whole_image_region_stats::null(),false);
	try{
		//save newly found interpolated worms to the db
		region.register_worm_detection(results,true,sql);
		//save region bitmap
		ns_image_standard collage;
	//	results->create_spine_visualizations(collage);
	//	ns_save_image("c:\\tt\\merged.tif",collage);
		throw ns_ex("NEXT LINE IS COMMENTED");
//		results->create_grayscale_object_collage(collage);
		ns_image_server_image region_bitmap = region.create_storage_for_processed_image(ns_process_region_interpolation_vis,ns_tiff,&sql);
		bool had_to_use_volatile_storage;
		ns_image_storage_reciever_handle<ns_8_bit> region_bitmap_o(image_server.image_storage.request_storage(region_bitmap, ns_tiff, 512,&sql,had_to_use_volatile_storage,false,false));
		collage.pump(region_bitmap_o.output_stream(),512);
		region_bitmap.mark_as_finished_processing(&sql);

		//since we have re-interpolated the frame, delete previous interpolated records in preparation
		//for the new ones.
		for (std::vector<ns_reverse_worm_lookup>::iterator p = corresponding_reverse_lookup_table_entry.begin(); 
													  p!=corresponding_reverse_lookup_table_entry.end();)
			if (p->interpolated) p = corresponding_reverse_lookup_table_entry.erase(p);
			else p++;

		//and load newly interpolated points into db.
		interpolated_worms.clear();
		interpolated_worms.id = results->id;
		interpolated_worms.load_from_db(sql);
		try{
			interpolated_worms.load_images_from_db(region,sql,true);
		}
		catch(ns_ex & ex){
			cerr << "Error refreshing interpolated worm record: " << ex.text();
			throw;
		}
		
		//add newly interpolated worms to local record of next time point
		const std::vector<ns_detected_worm_info *> & iworms =  interpolated_worms.actual_worm_list();
		corresponding_reverse_lookup_table_entry.reserve(corresponding_reverse_lookup_table_entry.size() + iworms.size());
		for (unsigned int j = 0; j <iworms.size(); j++)
			corresponding_reverse_lookup_table_entry.push_back(ns_reverse_worm_lookup(iworms[j]->worm_center() + iworms[j]->region_position_in_source_image,*iworms[j],this,true));
	
		delete results;
	}
	catch(...){
		delete results;
		throw;
	}
}

void ns_worm_interpolation_decision_maker::identify_interpolated_worm(const ns_detected_worm_info & worm, std::vector<ns_detected_object *> & neighbors, std::vector<ns_detected_object *> & matching_objects){
	int best_id = -1;
	unsigned long best_area_overlap = 0;
	ns_bitmap_overlap_calculation_results best_overlap;
	matching_objects.resize(0);

	unsigned long total_overlap_area(0);
	const unsigned int small_overlap_min(9);//ns_worm_detection_constants::get(ns_worm_detection_constant::overthresholded_object_overlap_required_to_merge_into_seed));
	const unsigned int large_overlap_min(6);//ns_worm_detection_constants::get(ns_worm_detection_constant::seed_coverage_required_to_allow_merge_overthresholded_objects));
	
	//cerr << worm.area << ": " << overlap_min << "\n";
	unsigned long worm_area(0);
	for (unsigned int i = 0; i < neighbors.size(); i++){		
	//	if (neighbors[i]->must_not_be_a_worm) continue;
		ns_vector_2i offset_2(neighbors[i]->offset_in_source_image);
		ns_bitmap_overlap_calculation_results overlap = ns_calculate_bitmap_overlap(worm.bitmap(),worm.region_position_in_source_image,neighbors[i]->bitmap(),offset_2);
		if (overlap.image_1_absolute_area != 0)
			worm_area = overlap.image_1_absolute_area;
		if (overlap.image_2_absolute_area == 0) continue;
		//cerr <<overlap.overlap_area << "(" << (double)overlap.overlap_area/(double)overlap.image_2_absolute_area  << ")\n";
		if (10*overlap.overlap_area > small_overlap_min*overlap.image_2_absolute_area){
			matching_objects.push_back(neighbors[i]);
			total_overlap_area+=overlap.overlap_area;
		}
	}
	if (worm_area == 0 || 10*total_overlap_area < large_overlap_min*worm_area){
		matching_objects.resize(0);
		if (worm_area != 0 && total_overlap_area != 0){
			//cerr << "Not enough coverage: " << total_overlap_area << "vs " << worm_area << ":" << (double)total_overlap_area/(double)worm_area;
			//cerr << "\n";
		}
	}
	else{ 
		//cerr << "Total Overlap area: " << total_overlap_area << " out of " << worm_area << "\n";
		//cerr << "\n";
	}
	
}


ns_detected_object * ns_worm_interpolation_decision_maker::create_merged_object(const ns_detected_worm_info & worm, std::vector<ns_detected_object *> & neighbors, const std::vector<ns_detected_object *> & matching_objects){
	if (matching_objects.size() == 0)
		throw ns_ex("ns_worm_interpolation_decision_maker::create_merged_object::No objects specified to merge!");
	ns_detected_object * merged = new ns_detected_object;
	std::vector<ns_vector_2i> offsets(matching_objects.size());
	ns_vector_2i new_bl(INT_MAX,INT_MAX),new_tr(0,0);
	for (unsigned int i = 0; i < matching_objects.size(); i++){
		if (matching_objects[i]->offset_in_source_image.x < (unsigned int)new_bl.x)
			new_bl.x = matching_objects[i]->offset_in_source_image.x;
		if (matching_objects[i]->offset_in_source_image.y < (unsigned int)new_bl.y)
			new_bl.y = matching_objects[i]->offset_in_source_image.y;
		if (matching_objects[i]->offset_in_source_image.x +  matching_objects[i]->size.x > (unsigned int)new_tr.x)
			new_tr.x = matching_objects[i]->offset_in_source_image.x +  matching_objects[i]->size.x;
		if (matching_objects[i]->offset_in_source_image.y +  matching_objects[i]->size.y > (unsigned int)new_tr.y)
			new_tr.x = matching_objects[i]->offset_in_source_image.y +  matching_objects[i]->size.y;
	}
	if (worm.region_position_in_source_image.x < new_bl.x)
		new_bl.x = worm.region_position_in_source_image.x;
	if (worm.region_position_in_source_image.y < new_bl.y)
		new_bl.y = worm.region_position_in_source_image.y;
	if (worm.region_position_in_source_image.x + worm.region_size.x-1 > new_tr.x)
		new_tr.x = worm.region_position_in_source_image.x + worm.region_size.x-1;
	if (worm.region_position_in_source_image.y + worm.region_size.y-1 > new_tr.y)
		new_tr.y = worm.region_position_in_source_image.y + worm.region_size.y-1;

	ns_image_properties prop(matching_objects[0]->bitmap().properties());
	prop.width = new_tr.x-new_bl.x+1;
	prop.height = new_tr.y-new_bl.y+1;
	merged->bitmap().prepare_to_recieve_image(prop);
	merged->offset_in_source_image = new_bl;
	merged->size = new_tr - new_bl + ns_vector_2i(1,1);
	for (unsigned int y = 0; y < prop.height; y++)
		for (unsigned int x = 0; x < prop.width; x++)
			merged->bitmap()[y][x] = 0;
	for (unsigned int i = 0; i < matching_objects.size(); i++){
		ns_vector_2i offset(matching_objects[i]->offset_in_source_image - merged->offset_in_source_image);
		for (unsigned int y = 0; y < matching_objects[i]->bitmap().properties().height; y++){
			for (unsigned int x = 0; x < matching_objects[i]->bitmap().properties().width; x++)
				if (matching_objects[i]->bitmap()[y][x])
					merged->bitmap()[y+offset.y][x+offset.x] = 1;
		}
	}	
	
	ns_vector_2i worm_offset(worm.region_position_in_source_image-merged->offset_in_source_image);
	for (unsigned int y = 0; y < worm.bitmap().properties().height; y++){
			for (unsigned int x = 0; x < worm.bitmap().properties().width; x++)
				if (worm.bitmap()[y][x])
					merged->bitmap()[y+worm_offset.y][x+worm_offset.x] = 1;
	}

	for (unsigned int y = 0; y < prop.height; y++){
		for (unsigned int x = 0; x < prop.width; x++)
			merged->area+=merged->bitmap()[y][x]?1:0;
	}
		
	return merged;
}


bool ns_output_tight_box(const ns_image_standard & in, ns_image_standard & out){
	ns_vector_2i tl(999,999), br(0,0);
	for (unsigned int y = 0; y < in.properties().height; y++)
		for (unsigned int x = 0; x < in.properties().width; x++){
			if (in[y][x] == 0) continue;
			if (y < (unsigned int)tl.y) tl.y = y;
			if (x < (unsigned int)tl.x) tl.x = x;
			if (y > (unsigned int)br.y) br.y = y;
			if (x > (unsigned int)br.x) br.x = x;
		}
	ns_image_properties prop(in.properties());
	prop.width = br.x - tl.x;
	prop.height = br.y - tl.y;
	if (tl.x >= br.x || tl.y >= br.y) 
		return false;
	out.prepare_to_recieve_image(prop);
	for (unsigned int y = (unsigned int)tl.y; y < (unsigned int)br.y; y++)
		for (unsigned int x = (unsigned int)tl.x; x < (unsigned int)br.x; x++)
			out[y-tl.y][x-tl.x] = in[y][x];
	return true;
}
void ns_worm_multi_frame_interpolation::output_worm_images_to_disk(const std::string & path, const std::string & prefix, ns_sql & sql, const unsigned int duration_of_time_to_use){
	ns_dir::create_directory_recursive(path);
	if (!ns_dir::file_is_writeable(path + DIR_CHAR_STR + "a.tif"))
		throw ns_ex("Cannot write to ") << path;
	if (time_points.size() == 0)
		throw ns_ex("No time points found.\n");
	unsigned long start_time = time_points[0]->time;
	unsigned int stop_time = time_points[0]->time + 24*60*60*duration_of_time_to_use;
	unsigned int stop;
	for (stop = 0; stop < time_points.size() && time_points[stop]->time < stop_time; stop++);
	
	cerr << "Using " << stop << " time points\n";
	for (unsigned int i = 0; i < stop; i++){
		time_points[i]->load_region_images(sql);
		const std::vector<ns_detected_worm_info *> worms = time_points[i]->detected_worms.actual_worm_list();
		cerr << "Found " << worms.size() << " worms\n";
		for (unsigned int j = 0; j < worms.size(); j++){
			std::string filename(path);
			filename += "\\";
			filename += prefix + ns_to_string(i) + "_" + ns_to_string(j) + ".tif";
			ns_image_standard tight;
			throw ns_ex("Relative grayscale not written to disk!");
			if (!ns_output_tight_box(worms[j]->absolute_grayscale(),tight)) continue;
			cerr << "Writing " << filename << "\n";
			ns_save_image(filename,tight);
		}

	}

}
void ns_worm_multi_frame_interpolation::clear_previous_interpolation_results(const unsigned int region_info_id, ns_sql & sql){
	
	std::string riv = ns_processing_step_db_column_name(ns_process_region_interpolation_vis);
	sql << "DELETE worm_detection_results FROM worm_detection_results, sample_region_images WHERE "
		<< "worm_detection_results.id = sample_region_images.worm_interpolation_results_id AND "
		<< "sample_region_images.worm_interpolation_results_id != 0 AND "
		<< "sample_region_images.region_info_id = " << region_info_id;
	sql.send_query();
	sql << "DELETE images FROM images, sample_region_images WHERE "
		<< "images.id = sample_region_images." << riv <<" AND "
		<< "sample_region_images." << riv <<" != 0 AND "
		<< "sample_region_images.region_info_id = " << region_info_id;
	sql.send_query();
	sql << "UPDATE sample_region_images SET worm_interpolation_results_id = 0, " << riv <<" = 0 WHERE "
		<< "sample_region_images.region_info_id = " << region_info_id;
	sql.send_query();
	sql << "UPDATE sample_region_image_info SET temporal_interpolation_performed = 0 WHERE id = " << region_info_id;
	sql.send_query();
}*/

void ns_worm_multi_frame_interpolation::load_all_region_worms(const unsigned int region_info_id, ns_sql & sql, bool only_use_processed_frames){

	cerr << "Downloading Dataset...\n";
	sql << "SELECT capture_time, id, worm_detection_results_id, worm_interpolation_results_id, worm_movement_id, "
		<< "op" << (unsigned int)ns_process_threshold << "_image_id FROM sample_region_images "
		<< "WHERE region_info_id = " << region_info_id;
	if (only_use_processed_frames)
		sql << " AND worm_detection_results_id != 0 AND op" << (unsigned int)ns_process_region_vis << "_image_id!=0";
	sql	<< " AND op" << (unsigned int)ns_process_threshold << "_image_id!=0"
		<< " AND problem = 0 AND censored = 0 ";
	sql << " ORDER BY capture_time ASC";
	ns_sql_result res;
	sql.get_rows(res);
	time_points_storage.resize(res.size());
	if (time_points_storage.size() == 0)
		return;
	ns_progress_reporter pr(time_points_storage.size(),10);
	for (unsigned int i = 0; i < time_points_storage.size(); i++){
		pr(i);
		time_points_storage[i].time = atol(res[i][0].c_str());
		//time_points_storage[i].region.region_images_id = atol(res[i][1].c_str());
		//time_points_storage[i].detected_worms.id =  atol(res[i][2].c_str());
		//time_points_storage[i].interpolated_worms.id =  atol(res[i][3].c_str());
		time_points_storage[i].threshold_image.id = atol(res[i][5].c_str());

		//if (only_use_processed_frames){
		//	time_points_storage[i].detected_worms.load_from_db(sql);
		//	
		//	if (time_points_storage[i].interpolated_worms.id != 0){
		//		time_points_storage[i].interpolated_worms.load_from_db(sql);
			//	cerr << time_points_storage[i].interpolated_worms.interpolated_worm_area_list().size() << " interpolated worms loaded from disk\n";
		//	}
		//}
		
		time_points.push_back(&time_points_storage[i]);
	}
	pr(time_points_storage.size());
	//step = (unsigned int)time_points_storage.size()/10;
	//percent = 0;
	//load all region images, deleting time points that do not have valid region images assigned.
	/*cerr << "Loading region images\n";
	for (unsigned int i = 0; i < time_points_storage.size(); i++){
		try{
			time_points_storage[i].detected_worms.load_images_from_db(time_points_storage[i].region,sql);
			if (time_points_storage[i].interpolated_worms.id != 0)
				time_points_storage[i].detected_worms.load_images_from_db(time_points_storage[i].region,sql,true);
		}
		catch(ns_ex & ex){
			cerr << ex.text() << "\n";
		}
		if (i%step == 0){
			cerr << percent << "%...";
			percent+=10;
		}
	}*/
}

void ns_out_frame_count(ns_image_standard & im, const unsigned long number_of_frames_used_to_find_stationary_objects, const unsigned long frame_count){
	ns_xml_simple_writer xml;
	xml.add_header();
	xml.add_tag("frame_count",frame_count);
	xml.add_tag("number_of_frames_used_to_find_stationary_objects",number_of_frames_used_to_find_stationary_objects);
	xml.add_footer();
	im.set_description(xml.result());
	/*
	ns_32_bit time_points_l = (ns_32_bit)frame_count;
	if ((ns_32_bit)frame_count > ((ns_32_bit)0)-1)
		frame_count = ((ns_32_bit)0)-1;
	ns_8_bit * tp = reinterpret_cast<ns_8_bit *>(&time_points_l);
	//place number of frames used to calculate heat map in bitmap
	im[0][0] = tp[0];
	im[0][1] = tp[1];
	im[0][2] = tp[2];
	im[0][3] = tp[3];
	im[0][4] = tp[0];
	im[0][5] = tp[1];*/
}


inline char ns_get_division_from_frame_number(const unsigned int i,const unsigned long number_of_frames_used_to_find_stationary_objects,const unsigned int total){

	//if there are very few frames, just assume all are early.
	//otherwise, any individual step would be too noisy due to low sample size.
	if (total <= 30){
		if (i < number_of_frames_used_to_find_stationary_objects)
			return 0;
		else return 1;
	}
	if (number_of_frames_used_to_find_stationary_objects > total)
		throw ns_ex("Invalid maximum number_of_frames_used_to_find_stationary_objects specified: ") << number_of_frames_used_to_find_stationary_objects << " in a set with only " << total << " frames.";

	//if there aren't many frames, use an even division between the three time divisions
	if (total <= 90){
		if (number_of_frames_used_to_find_stationary_objects == 0)
			return (char)((3*(long)i)/(total+1));
		else{
			if (i < number_of_frames_used_to_find_stationary_objects)
				return 0;
			else
				return (char)((2*(long)i)/(total-number_of_frames_used_to_find_stationary_objects+1)); // TODO: Verify this is the correct return value in this case
		}
	}

	//otherwise shorten the "beginning" time division to only a seventh of the total span
	unsigned int frac = ((9*(long)i)/(total-number_of_frames_used_to_find_stationary_objects+1));
	if ( frac >=5)	return 2;
	if ( i > number_of_frames_used_to_find_stationary_objects)	return 1;
	return 0;
}

struct ns_number_of_frames_in_heatmap{
	ns_number_of_frames_in_heatmap(const unsigned long number_of_frames_used_to_find_stationary_objects, const unsigned int total){
		early = middle = late = 0;
		for (unsigned int i = 0; i < total; i++){
			switch(ns_get_division_from_frame_number(i,number_of_frames_used_to_find_stationary_objects,total)){
				case 0:early++;break;
				case 1:middle++;break;
				case 2:late++;break;
			}
		}
	}
	unsigned int early,
				 middle,
				 late;
};



void ns_worm_multi_frame_interpolation::generate_heat_map(ns_image_standard & heat_map_out,const unsigned int number_of_frames_used_to_find_stationary_objects_,ns_sql & sql){
	if (time_points.size() == 0)
		throw ns_ex("ns_worm_multi_frame_interpolation::generate_heat_map::No time points available!");
	unsigned long number_of_frames_used_to_find_stationary_objects(number_of_frames_used_to_find_stationary_objects_);
	//0 indicates that the algorithm should choose the best number of points to use

	if (time_points.size() <= 2 && number_of_frames_used_to_find_stationary_objects != 1)
			throw ns_ex("Cannot calculate static mask for such a small number of frames:" ) << time_points.size();

	//if not stationary object frame count is specified, determine one automatically
	if (number_of_frames_used_to_find_stationary_objects_ == 0){
		//for very short experiments (ie heat shock) use a short time interval
		if (time_points.size() < 60)
			number_of_frames_used_to_find_stationary_objects = 3;
		else{
			if (number_of_frames_used_to_find_stationary_objects_ > time_points.size())
				number_of_frames_used_to_find_stationary_objects = time_points.size()/5;
		
			if (number_of_frames_used_to_find_stationary_objects==0)
				number_of_frames_used_to_find_stationary_objects = 2;
		}
	}

	//get image size for heatmap from first threshold
	bool size_loaded = false;
	ns_image_properties heat_map_size;
	for (unsigned int i = 0; i < time_points.size(); i++){
		try{
			//cerr << "Loading heat_map size from time_point " << i << "\n";
			time_points[i]->load_threshold(sql);
			heat_map_size = time_points[i]->threshold.properties();
			size_loaded = true;
			break;
		}
		catch(ns_ex & ex){
			cerr << "Could not open threshold frame for size info: " << ex.text() << "\n";
		}
	}
	if (!size_loaded)
		throw ns_ex("ns_worm_multi_frame_interpolation::generate_heat_map()::Sample region has no usable images!");

	heat_map_size.components = 3;
	heat_map_out.prepare_to_recieve_image(heat_map_size);
	
	for (unsigned int y = 0; y < heat_map_size.height; y++)
			for (unsigned int x = 0; x < 3*heat_map_size.width; x++)
				heat_map_out[y][x] = 0;
	unsigned int i = 0;
	for (std::vector<ns_worm_interpolation_timepoint *>::iterator p = time_points.begin();;){
		//stop at one element before the end.
		std::vector<ns_worm_interpolation_timepoint *>::iterator end_p = time_points.end();
		end_p--;
		if (p == end_p) break;
		try{

			cerr << "Processing frame " << i+1 << "/" << time_points.size() << "\n";
			if (i != 0)
				(*p)->load_threshold(sql);
		}
		catch(ns_ex & ex){
			cerr << ex.text() << "\n";
			p = time_points.erase(p);
			continue;
		}
		for (long y = 0; y < (long)(*p)->threshold.properties().height; y++){
			for (long  x = 0; x < (long)(*p)->threshold.properties().width; x++){
				long color = ns_get_division_from_frame_number(i,number_of_frames_used_to_find_stationary_objects,(unsigned int)time_points.size());
				if ((*p)->threshold[y][x] && heat_map_out[y][3*x+color] < 255)
					heat_map_out[y][3*x+color]++;
			}
		}
		(*p)->clear_threshold();
		p++;
		i++;
	}
	ns_out_frame_count(heat_map_out,number_of_frames_used_to_find_stationary_objects,(unsigned long)time_points.size());
	
}
/*
void ns_worm_multi_frame_interpolation::interpolate_worms(const ns_image_standard * static_mask, ns_sql & sql){;
	run_analysis(static_mask,sql);
}

void ns_worm_multi_frame_interpolation::run_analysis(const ns_image_standard * static_mask,ns_sql & sql){
	const unsigned int distance_to_look_in_future(ns_worm_detection_constants::get(ns_worm_detection_constant::number_of_time_points_to_use_for_interpolation));
	const unsigned long max_distance(ns_worm_detection_constants::get(ns_worm_detection_constant::allowed_drift_distance_for_objects_during_interpolation));

	

	unsigned long heat_map_threshold(ns_worm_detection_constants::get(ns_worm_detection_constant::number_of_time_points_required_to_infer_an_object_during_interpolation));
	unsigned int heat_map_overlap_threshold(ns_worm_detection_constants::get(ns_worm_detection_constant::overlap_required_to_infer_an_object_during_interpolation));


	std::vector<unsigned int> prev_discovery_time_freq_distribution(distance_to_look_in_future,0);
	std::vector<unsigned int> thresh_discovery_time_freq_distribution(distance_to_look_in_future,0);
	std::vector<unsigned int> heat_map_discovery_time_freq_distribution(distance_to_look_in_future,0);

	std::vector< std::vector< ns_reverse_worm_lookup> > worm_locations(time_points.size());
	//aggregate all worms, interpolated and otherwise.
	if (time_points.size() == 0){
		cerr << "No time points exist.\n";
		return;
	}
	for (unsigned int i = 0; i < time_points.size(); i++){
		const std::vector<ns_detected_worm_info *> &worms =  time_points[i]->detected_worms.actual_worm_list();
		const std::vector<ns_detected_worm_info *> &iworms =  time_points[i]->interpolated_worms.actual_worm_list();
		worm_locations[i].reserve(worms.size()+ iworms.size());
		for (unsigned int j = 0; j <worms.size(); j++)
			worm_locations[i].push_back(ns_reverse_worm_lookup(worms[j]->worm_center() + worms[j]->region_position_in_source_image,*worms[j],time_points[i],false));
		//cerr << iworms.size() << " interpolated worms\n";
		for (unsigned int j = 0; j <iworms.size(); j++)
			worm_locations[i].push_back(ns_reverse_worm_lookup(iworms[j]->worm_center() + iworms[j]->region_position_in_source_image,*iworms[j],time_points[i],true));
	}
	if (time_points.size() == 0) return;

	//get image size for heatmap from first threshold
	time_points[0]->load_threshold(sql);
	ns_image_whole<ns_8_bit> heat_map;
	ns_image_properties heat_map_size(time_points[0]->threshold.properties());
	time_points[0]->clear_threshold();
	heat_map.prepare_to_recieve_image(heat_map_size);
	heat_map_size.components = 3;
	//heat_map_out.prepare_to_recieve_image(heat_map_size);
	


	//look at future time points to find matches
	std::vector<ns_worm_interpolation_timepoint *>::iterator cur_tp = time_points.begin();
	std::vector< std::vector< ns_reverse_worm_lookup> >::iterator cur_wl = worm_locations.begin();
	const unsigned int start(0);
	cur_tp+=start;
	cur_wl+=start;
	try{
		for (unsigned int i = start; i < time_points.size()-1;){


			//build heat map of worm locations
			for (unsigned int y = 0; y < heat_map_size.height; y++)
				for (unsigned int x = 0; x < heat_map_size.width; x++)
					heat_map[y][x] = 0;

			//use all worms in the current timepoint as bait
			std::vector< ns_reverse_worm_lookup *> bait_worms(worm_locations[i].size());
			for (unsigned int k = 0; k < bait_worms.size(); k++)
				bait_worms[k] = &worm_locations[i][k];
			cerr << "Current Frame: " << ns_format_time_string(time_points[i]->time)  << " (" << i << "/" << worm_locations.size() << ")\n";
			
			if (time_points[i]->time == 1225656002) 
				cerr << "OK\n";
			bool could_not_load_current_frame = false;
			std::vector<ns_worm_interpolation_timepoint *>::iterator future_tp = cur_tp;
			std::vector< std::vector< ns_reverse_worm_lookup> >::iterator future_wl = cur_wl;
			//load current and future region images
			for (unsigned int fi = i; fi < worm_locations.size() && fi-i < distance_to_look_in_future;){
				//cerr << "Loading" <<  fi << "/" << worm_locations.size() << " ";
				bool found_error = false;


				if (!time_points[fi]->loaded_images)
					found_error = !time_points[fi]->load_region_images(sql);
				if (!found_error && !time_points[fi]->threshold_loaded)
						time_points[fi]->load_threshold(sql);
				if (!found_error && ! (time_points[fi]->threshold.properties() == heat_map.properties())){
					cerr << "Current frame thresholded image is " << time_points[fi]->threshold.properties().width << "x" << 
																	 time_points[fi]->threshold.properties().height <<
							" wheras the heat map is " <<  heat_map.properties().width << "x" <<  heat_map.properties().height << "\n";
					found_error = true;
				}
				
				if(!found_error){
					//build heat map
					for (unsigned int y = 0; y < heat_map_size.height; y++)
						for (unsigned int x = 0; x < heat_map_size.width; x++)
							heat_map[y][x] += (time_points[fi]->threshold[y][x] > 0);
					fi++;
					future_tp++;
					future_wl++;

				}
				else{
					
					cerr << "Erasing" <<  fi << "/" << worm_locations.size() << " ";
					if (fi == worm_locations.size()-2) break;
					future_tp = time_points.erase(future_tp);
					future_wl->clear();
					future_wl = worm_locations.erase(future_wl);
					if (fi == i){
						cur_tp = future_tp;
						cur_wl = future_wl;
						could_not_load_current_frame = true;
						break;
					}
				}
			}
			cerr << "\n";
			if (could_not_load_current_frame)
				continue;
			


		
			//search future frames for neighbors to the bait worm
			for (unsigned int fi = i+1; fi < worm_locations.size() && fi-i < distance_to_look_in_future && bait_worms.size() != 0; fi++){
	
				

				//for each bait worm, look in the future for matches, either previously-detected or interpolated
				std::vector<ns_reverse_worm_lookup *>::iterator bait_worm = bait_worms.begin();
				for (unsigned int bait_worm_id = 0; bait_worm_id < bait_worms.size(); ){

					
					unsigned long worm_heat_map_overlap_area,
								  worm_area;

					int future_neighbor_id= -1;
					
					for (unsigned int future_worm_id = 0; future_worm_id < worm_locations[fi].size(); future_worm_id++){
						if ((bait_worms[bait_worm_id]->pos - worm_locations[fi][future_worm_id].pos).mag() <= max_distance){
							future_neighbor_id= future_worm_id;
							break;
						}
					}
				
					//if we find a previously-detected match for the bait worm in the current future frame, we can stop searching
					//and delete it from the list.
					if (future_neighbor_id != -1){
						prev_discovery_time_freq_distribution[fi-i]++;
						bait_worm = bait_worms.erase(bait_worm);
				//		cerr << "Worm found found in pre-detected object\n";
						continue;
					}



					
					//if we didn't find an already-detected neighbor, look for a non-worm object in the threshold that might actually be a worm

					//find regions in the future frame that overlap with the worm,
					//and test to see if they show significant overlap
					ns_worm_interpolation_decision_maker d;
					std::vector<ns_detected_object *> matching_objects;
					d.identify_interpolated_worm(*bait_worms[bait_worm_id]->worm, time_points[fi]->object_manager.objects,matching_objects);

					//retrieve the best neighbor's id in the next_threshold_objects object

					//if a good neighbor is found, mark it for inclusion.
					if (matching_objects.size() > 0){
						cerr << "Found a worm in the threshold " << fi-i << " frames in the future " << "\n";
						//if we only find one object, we just need to set the correct flags.
						if (matching_objects.size() == 1){
							time_points[fi]->object_manager.objects[0]->must_be_a_worm = true;
							time_points[fi]->object_manager.objects[0]->must_not_be_a_worm = false;
						}
						//if we find multiple objects, they are problably remenants of one object that via thresholding
						//got broken into pieces.  We need to reassemble them.
						else{
							cerr << "Merging " << matching_objects.size() << "worms...\n";
							ns_detected_object * merged = d.create_merged_object(*bait_worms[bait_worm_id]->worm, time_points[fi]->object_manager.objects,matching_objects);
							for (unsigned long i = 0; i < matching_objects.size(); i++)
								matching_objects[i]->must_not_be_a_worm = true;
							time_points[fi]->object_manager.objects.push_back(merged);
							merged->must_be_a_worm = true;
							merged->must_not_be_a_worm = false;
							merged->is_a_merged_object = true;
						}
						time_points[fi]->interpolated_worms_identified = true;
						thresh_discovery_time_freq_distribution[fi-i]++;
						bait_worm = bait_worms.erase(bait_worm);
						continue;
					}
				
					

					//if we didn't find an non-worm object in the threshold that might actually be a worm
					//look for one in the heat map
					const ns_detected_worm_info * worm(bait_worms[bait_worm_id]->worm);

					ns_image_standard vis;
					ns_image_properties p(worm->bitmap().properties());
					p.components = 3;
					vis.prepare_to_recieve_image(p);

					worm_heat_map_overlap_area = 0;
					worm_area = 0;
					
					for (unsigned int y = 0; y < worm->bitmap().properties().height; y++)
						for (unsigned int x = 0; x < worm->bitmap().properties().width; x++){
							worm_heat_map_overlap_area+= worm->bitmap()[y][x] && (10*(unsigned long)heat_map[worm->region_position_in_source_image.y+y][worm->region_position_in_source_image.x+x] >= heat_map_threshold*distance_to_look_in_future);
							worm_area += worm->bitmap()[y][x];
							vis[y][3*x + 0] = heat_map[worm->region_position_in_source_image.y+y][worm->region_position_in_source_image.x+x];
							vis[y][3*x + 1] = 255*worm->bitmap()[y][x];
							vis[y][3*x + 2] = 0;
						}
					//cerr << "Found an overlap of " << worm_heat_map_overlap_area << " from a worm with area " << worm_area << "\n";
				
					vis[0][0] = 255;
					vis[0][2] = 0;
					vis[0][3] = 0;
					//std::string det = "no";
					if (10*worm_heat_map_overlap_area > worm_area*heat_map_overlap_threshold){
					//	det = "yes";
						vis[0][0] = 0;
						vis[0][2] = 255;
						vis[0][3] = 0;
						ns_vector_2i top_left(heat_map_size.width,heat_map_size.height),
									 bottom_right(0,0);
						int start_y = worm->region_position_in_source_image.y - 50;
						int stop_y = worm->region_position_in_source_image.y + worm->region_size.y + 50;
						int start_x = worm->region_position_in_source_image.x - 50;
						int stop_x = worm->region_position_in_source_image.x + worm->region_size.x + 50;
						if (start_y < 0) start_y = 0;
						if (start_x < 0) start_x = 0;
						if (stop_y >= (int)heat_map_size.height) stop_y = (int)heat_map_size.height;
						if (stop_x >= (int)heat_map_size.width) stop_x = (int)heat_map_size.width;

						for (int y = start_y; y < stop_y; y++){
							for (int x = start_x; x < stop_x; x++){
								if (heat_map[y][x] >= heat_map_threshold){
									if (y < top_left.y) top_left.y = y;
									if (x < top_left.x) top_left.x = x;
									if (y > bottom_right.y) bottom_right.y = y;
									if (x > bottom_right.x) bottom_right.x = x;
								}
							}
						}
						ns_image_properties prop;
						prop.width = bottom_right.x - top_left.x;
						prop.height = bottom_right.y - top_left.y;
						prop.components = 1;
						prop.resolution = heat_map_size.resolution;

						//cerr << "Found a worm in the heat map " << fi-i << " frames in the future " << "\n";
						
						time_points[fi]->object_manager.add_interpolated_worm_area(ns_interpolated_worm_area(top_left,bottom_right - top_left));
						time_points[fi]->interpolated_worms_identified = true;
						//We do *not* remove the worm as bait
						//as we want to infer all regions between detected worms
						//bait_worm = bait_worms.erase(bait_worm);
						heat_map_discovery_time_freq_distribution[fi-i]++;
					}
		
					bait_worm++;
					bait_worm_id++;
				}
					
			}
			//time_points[fi]->clear_images();
			//time_points[fi]->clear_threshold();

			//Now we have located/interpolated/failed to find neighbors for all bait worms in the current frame.
			//We can now update the database, output a visualization if necissary, and deallocate memory we're finished with.
			for (unsigned int fi = i+1; fi < worm_locations.size() && fi-i < distance_to_look_in_future; fi++){			
				if (time_points[fi]->interpolated_worms_identified){
					time_points[fi]->commit_interpolated_regions_to_db(static_mask,worm_locations[fi],sql);
					time_points[fi]->interpolated_worms_identified = false;
				}
			}

			
			//draw heat plot for current frame


			//deallocate images that will no-longer be used 
			time_points[i]->clear_images();
			time_points[i]->clear_threshold();
			worm_locations[i].resize(0);
			i++;
			cur_tp++;
			cur_wl++;
			//exit(0);
		}
		//place number of frames used to calculate heat map in bitmap
		//if (generate_heat_map)
		//	ns_out_frame_count(heat_map_out,(unsigned long)worm_locations.size());

	
		cerr << "Done\n";
	}	
	catch(ns_ex & ex){
		cerr << ex.text();
	}
}*/

//A heat map is generated by summing all thresholded frames of a time series.  For the first
//third of the time series, treshold values are added to the red channel of the heat map.
//for the second third of the time series, threshold values are added to the green channel
//for the final, blue.
//This means we can roughly inspect which pixels are constantly bright for the biggining, middle, or end
//of the experiment.
//This function looks for pixels that were bright for a specified fraction of each third of the experiment.
//Such bright pixels, if situated in a region where at least one pixel was bright in the two other thirds
//of the experiment, are added to the output image.
//The output image can then be used to mask out unwanted "static" pixels that represent
//features such as the plate edge or dust.
void ns_worm_multi_frame_interpolation::generate_static_mask_from_heatmap(const ns_image_standard & im, ns_image_standard & out){
	const unsigned int	spatial_smudge_distance(ns_worm_detection_constants::get(ns_worm_detection_constant::allowed_drift_distance_for_objects_during_static_mask_creation)),
						early_strong_threshold(ns_worm_detection_constants::get(ns_worm_detection_constant::proportion_of_early_time_points_present_required_during_static_mask_creation)),  //out of 10
						middle_threshold(ns_worm_detection_constants::get(ns_worm_detection_constant::proportion_of_middle_time_points_present_required_during_static_mask_creation)), //out of 10
						late_threshold(ns_worm_detection_constants::get(ns_worm_detection_constant::proportion_of_late_time_points_present_required_during_static_mask_creation));  //out of 10
	if (im.properties().width < 2 || im.properties().height == 0)
		throw ns_ex("ns_worm_multi_frame_interpolation::Empty heat map was provided");

	
	ns_32_bit number_of_frames_used_to_make_heatmap(0);
	ns_32_bit number_of_frames_used_to_find_stationary_objects(0);

	ns_32_bit time_points_l = 0;
	if (im.properties().description.size() != 0){
		//load frame information from xml spec
		bool found_total(false),
			 found_begin_count(false);
		ns_xml_simple_object_reader xml;
		xml.from_string(im.properties().description);
		for (unsigned int i = 0; i < xml.objects.size(); i++){
			if (xml.objects[i].name == "frame_count"){
				number_of_frames_used_to_make_heatmap = atol(xml.objects[i].value.c_str());
				found_total = true;
			}
			else if (xml.objects[i].name =="number_of_frames_used_to_find_stationary_objects"){
				number_of_frames_used_to_find_stationary_objects = atol(xml.objects[i].value.c_str());
				found_begin_count = true;
			}
		}
		if (!found_total)
			throw ns_ex("ns_worm_multi_frame_interpolation::generate_static_mask_from_heatmap::Could not find total frame count specification in heat map");
		if (!found_begin_count)
			throw ns_ex("ns_worm_multi_frame_interpolation::generate_static_mask_from_heatmap::Could not find number_of_frames_used_to_find_stationary_objects specification in heat map");
	}
	else{
		throw ns_ex("Old style heat maps no longer supported!");
		//old style heatmaps stored pixel information in the first five pixels of the image.
		ns_8_bit a=im[0][0],
			b=im[0][1],
			c=im[0][2],
			d=im[0][3],
			e=im[0][4],
			f=im[0][5];
		if (im[0][0] != im[0][4] || im[0][1] != im[0][5])
			throw ns_ex("ns_worm_multi_frame_interpolation::Could not find watermark specifiying source frame count");	
		ns_8_bit * fr = reinterpret_cast<ns_8_bit *>(&number_of_frames_used_to_make_heatmap);
		fr[0] = im[0][0];
		fr[1] = im[0][1];
		fr[2] = im[0][2];
		fr[3] = im[0][3];
		if (number_of_frames_used_to_make_heatmap < 30)
			 number_of_frames_used_to_find_stationary_objects = number_of_frames_used_to_make_heatmap;
		else if (number_of_frames_used_to_make_heatmap < 90)
			number_of_frames_used_to_find_stationary_objects = number_of_frames_used_to_make_heatmap/3;
		number_of_frames_used_to_find_stationary_objects = number_of_frames_used_to_make_heatmap/9;
	}



	//number_of_frames_used_to_make_heatmap = 306;
	ns_number_of_frames_in_heatmap number_of_frames(number_of_frames_used_to_find_stationary_objects,number_of_frames_used_to_make_heatmap);

	/*cerr << number_of_frames_used_to_make_heatmap << " frames used in heat map, divided into " 
		<< number_of_frames.early << ", "<< number_of_frames.middle << ", and " << number_of_frames.late << "\n";
*/
	if (im.properties().components != 3)
		throw ns_ex("ns_image_server_calculate_heatmap_overlay::Heatmaps must be in color");
	ns_image_properties p(im.properties());
	p.components = 1;
	out.prepare_to_recieve_image(p);

	for (int y = 0; y < (int)p.height; y++){
		for (int x = 0; x < (int)p.width; x++){
			const bool	found_early(10*((unsigned int)im[y][3*x  ]) >= early_strong_threshold*number_of_frames.early),
						found_middle(10*((unsigned int)im[y][3*x+1]) >= middle_threshold*number_of_frames.middle),
						found_late(10*((unsigned int)im[y][3*x+2]) >= late_threshold*number_of_frames.late);
			if(!found_early &&!found_middle &&!found_late){
				out[y][x] = 0;
				continue;
			}

			int x0(x-spatial_smudge_distance),
				x1(x+spatial_smudge_distance),
				y0(y-spatial_smudge_distance),
				y1(y+spatial_smudge_distance);
			if (x0 < 0) x0 = 0;
			if (x1 >= (int)p.width) x1 = (int)p.width-1;
			if (y0 < 0) y0 = 0;
			if (y1 >= (int)p.height) y1 = (int)p.height-1;
			bool cont(true);
			out[y][x] = 0;

			for (int _y = y0; _y < y1 && cont; _y++){
				for (int _x = x0; _x < x1 && cont; _x++){
					if(	10*((unsigned int)im[_y][3*_x  ]) >= early_strong_threshold*number_of_frames.early ||
						(found_early && 
							(10*((unsigned int)im[_y][3*_x+1]) >= middle_threshold*number_of_frames.middle ||
							10*((unsigned int)im[_y][3*_x+2]) >= late_threshold*number_of_frames.late)
						)
					){

						out[y][x] = 255;
						cont = false;
					}
				}
			}
		}
	}
}
