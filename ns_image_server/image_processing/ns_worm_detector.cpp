#include "ns_worm_detector.h"
#include "ns_image_easy_io.h"
#include "ns_node_topology.h"


using namespace std;
void ns_detected_object_manager::remove_largest_images(double percent_cutoff){

	//remove unusally large objects
	ns_vector_2i max;
	max.x = 0; 
	max.y = 0;
	ns_vector_2i average(0,0);
	for (unsigned int i = 0; i < objects.size(); i++){
		if (max.x < (int)objects[i]->bitmap().properties().width)
			max.x = (int)objects[i]->bitmap().properties().width;

		if (max.y < (int)objects[i]->bitmap().properties().height)
			max.y = (int)objects[i]->bitmap().properties().height;

		average.x += objects[i]->bitmap().properties().width;
		average.y += objects[i]->bitmap().properties().height;
	}

	//average = average/spine_visualizations.size();
	ns_vector_2i cutoff = max*.98;

	for (vector<ns_detected_object *>::iterator p = objects.begin(); p != objects.end();){
		if ((*p)->bitmap().properties().height > (unsigned int)cutoff.y || (*p)->bitmap().properties().width > (unsigned int)cutoff.x){
			delete *p;
			p = objects.erase(p);
		}
		else p++;
	}
}

void ns_detected_object_manager::remove_empty_spines(){
	/*for (vector<ns_detected_object *>:: iterator p = objects.begin(); p != objects.end();){
		if ((*p)->segment_cluster_solutions.mutually_exclusive_solution_groups..size() == 0){
			//add explanation as to why region was removed
//				region_labels.push_back(ns_text_label(ns_vector_2i((*p)->max_x,(*p)->min_y),"empty_spine"));

			delete *p;
			p = objects.erase(p);
		}
		else 
			p++;
	}		*/
}
void ns_detected_object_manager::remove_objects_found_in_static_mask(const ns_image_standard & static_mask){
	
	vector<ns_detected_object *> objects_to_sort;
	objects_to_sort.assign(objects.begin(),objects.end());
	objects.resize(0);

	vector<ns_detected_object *> not_in_static_mask;
	not_in_static_mask.reserve(objects.size());
	try{

		for (unsigned int i = 0; i < objects_to_sort.size(); i++){
			unsigned int mask_overlay(0),
						 total_area(0);

			if (objects_to_sort[i]->size.x > objects_to_sort[i]->bitmap().properties().width
				|| objects_to_sort[i]->size.y > objects_to_sort[i]->bitmap().properties().height)
				throw ns_ex("Specified object: (position:") << objects_to_sort[i]->offset_in_source_image.x <<  "," << objects_to_sort[i]->offset_in_source_image.y << "; size: " 
								<< objects_to_sort[i]->size.x << "," << objects_to_sort[i]->size.y << ") is larger than it's bitmap " <<
								objects_to_sort[i]->bitmap().properties().width << "," << objects_to_sort[i]->bitmap().properties().height;

				if (objects_to_sort[i]->offset_in_source_image.x + objects_to_sort[i]->size.x > static_mask.properties().width
					|| objects_to_sort[i]->offset_in_source_image.y + objects_to_sort[i]->size.y > static_mask.properties().height)
						throw ns_ex("Specified object: (position:") << objects_to_sort[i]->offset_in_source_image.x <<  "," << objects_to_sort[i]->offset_in_source_image.y << "; size: " 
								<< objects_to_sort[i]->size.x << "," << objects_to_sort[i]->size.y << ") lies outside the static mask " <<
								objects_to_sort[i]->bitmap().properties().width << "," << objects_to_sort[i]->bitmap().properties().height;

			for (unsigned int y = 0; y < objects_to_sort[i]->size.y; y++){
				for (unsigned int x = 0; x < objects_to_sort[i]->size.x; x++){
					total_area+=objects_to_sort[i]->bitmap()[y][x];
					mask_overlay+= static_mask[y+objects_to_sort[i]->offset_in_source_image.y][x+objects_to_sort[i]->offset_in_source_image.x] 
									&& objects_to_sort[i]->bitmap()[y][x];
				}
			}
			if (10*mask_overlay > 5*total_area){
				//object is in the static mask.
				ns_safe_delete(objects_to_sort[i]);
			}
			else{
				not_in_static_mask.push_back(objects_to_sort[i]);
				objects_to_sort[i] = 0;
			}
		}
		objects_to_sort.clear();
		//put all the objects we want to keep back in the objects vector.
		objects.assign(not_in_static_mask.begin(),not_in_static_mask.end());
	}
	catch(...){
		//if we encounter a problem, put all the elements we've kept and all those we haven't looked at yet back into the object vector.
		objects.assign(not_in_static_mask.begin(),not_in_static_mask.end());
		for (vector<ns_detected_object *>::iterator p = objects_to_sort.begin(); p != objects_to_sort.end(); ++p){
			if (*p == 0) continue;
			objects.push_back(*p);
		}
	}

}
void ns_detected_object_manager::constrain_region_area(unsigned int minimum_area, unsigned int maximum_area, const unsigned int max_diagonal){
	if (minimum_area >= maximum_area)
		throw ns_ex("ns_bitmap_region_classifier::Nonsensical region area constraints: (min,max) = (") << minimum_area << "," << maximum_area << ")";
	vector<ns_detected_object *> size_ok;
	size_ok.reserve(objects.size());
	for (unsigned int i = 0; i < objects.size(); i++){
		if ( objects[i]->area < minimum_area){
		//	cerr << "SMALL AREA\n";
			delete objects[i];
		}
		else if (objects[i]->area > maximum_area){
		//	cerr << "LARGE AREA\n";
			delete objects[i];
		}
		else if (objects[i]->bitmap().properties().width * objects[i]->bitmap().properties().width + 
		objects[i]->bitmap().properties().height* objects[i]->bitmap().properties().height > max_diagonal*max_diagonal){
			/*cerr << sqrt((double)(objects[i]->bitmap().properties().width * objects[i]->bitmap().properties().width + 
				objects[i]->bitmap().properties().height* objects[i]->bitmap().properties().height)) << ">" << max_diagonal << " ";
			cerr << "DIAG\n";*/
			delete objects[i];
		}
		else
			size_ok.push_back(objects[i]);
	}
	objects.assign(size_ok.begin(),size_ok.end());
	/*for (vector<ns_detected_object *>:: iterator p = regions.begin(); p != regions.end();){
		if ( (*p)->area < minimum_area || (*p)->area > maximum_area || ( (*p)->width()*(*p)->width() + (*p)->height()*(*p)->height() > max_diagonal*max_diagonal)){

			//if ((*p)->area > maximum_area) //add explanation as to why region was removed
			//	region_labels.push_back(ns_text_label(ns_vector_2i((*p)->max_x,(*p)->min_y),"Too Large"));
				
			//(*p)->fill_region_in_image(false,im);
			delete *p;
			p = regions.erase(p);
		}
		else 
			p++;
	}*/
}
void ns_detected_object::remove_small_holes(){
	if (bitmap().properties().resolution > 1201)
		ns_remove_small_holes(bitmap(),ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_hole_size,bitmap().properties().resolution));
}
void ns_detected_object::calculate_edges(){
	
	ns_calculate_res_aware_edges(bitmap(),edge_bitmap(),edge_coordinates,holes,edge_list,edges);
	
	
}

