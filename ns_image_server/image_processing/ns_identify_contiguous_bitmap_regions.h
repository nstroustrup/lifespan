#ifndef NS_PROCESS_CONTIGUOUS_REGIONS
#define NS_PROCESS_CONTIGUOUS_REGIONS
#include <vector>
#include <string>
#include "ns_image.h"
#include <math.h>
#include <algorithm>
#include <fstream>
#include <limits.h>
#include <map>

#include "ns_vector.h"
#include "ns_vector_bitmap_interface.h"
#include "ns_detected_object.h"
#include "ns_detected_worm_info.h"

#include "ns_tiff.h"


#define TIME_RESOLUTION 10

//#define ID_DEBUG_OUTPUT

#ifdef ID_DEBUG_OUTPUT
std::ofstream deb;
#endif

///ns_detected_object_indentier detects all regions of a bitmap containing contiguous pixels.  During this process
///each contiguous region is assigned a pseudonym.  As the image is processed, seperate regions are often discovered to contain
///contiguous pixels.  Rather than going back and merging the two regions, their ids are simply registered as pseudonyms for the same
//region.  ns_pseudonym_maintainer maintains the pseudonym std::mappings of various regions

/*
class ns_pseudonym_equivalency_set{
public:
	std::set<unsigned long> equal_ids;
	unsigned long x,y,w,h;
	std::vector<ns_pseudonym_equivalency_set *> parents;
	std::vector<ns_pseudonym_equivalency_set *> children;


	void insert(const ns_pseudonym_equivalency_set & s);
	void recursive_collapse();
	inline bool already_merged() const{return equal_ids.size() == 0;}
};

struct ns_pseudonym_equivalency_calculator{
	ns_pseudonym_equivalency_calculator():start_of_last_active_set_row(0),start_of_this_active_set_row(0),estimated_number_of_objects(0){}
	void add_pixel_to_equivalency_set(const unsigned long value,const unsigned long x_start,const unsigned long length,const unsigned long y);
	void increment_start_of_active_sets();
	void collapse_equality_sets();

	template<class whole_image>
	void produce_detected_objects(std::vector<ns_detected_object *> & objects, const whole_image & im){

		objects.reserve(estimated_number_of_objects);
		for (unsigned int i = 0; i < equivalency_sets.size(); i++){
			if (equivalency_sets[i].already_merged()) continue;
			unsigned long s = (unsigned long)objects.size();
			objects.resize(s+1,new ns_detected_object);
			ns_detected_object & object(*objects[s]);
			object.min_x = equivalency_sets[i].x;
			object.min_y = equivalency_sets[i].y;
			object.max_x = equivalency_sets[i].x+equivalency_sets[i].w-1;
			object.max_y = equivalency_sets[i].y+equivalency_sets[i].h-1;

			//initialize object bitmap
			ns_image_properties prop(im.properties());
			prop.width = equivalency_sets[i].w;
			prop.height = equivalency_sets[i].h;
			ns_image_bitmap &bmp(object.bitmap());
			bmp.prepare_to_recieve_image(prop);
			for (unsigned int y = 0; y < prop.height; y++)
				for (unsigned int x = 0; x < prop.width; x++)
					bmp[y][x] = 0;

			//go through each of the specified sets of pixels and fill them in in the object bitmap
			for (std::set<unsigned long>::iterator p = equivalency_sets[i].equal_ids.begin(); p != equivalency_sets[i].equal_ids.end(); p++){
				unsigned long y = *p/im.properties().width;
				unsigned long start_x = *p-y*im.properties().width;
				for (unsigned long x = start_x; x < im.properties().width && bmp[y][x]; x++)
					bmp[y-equivalency_sets[i].y][x] = 1;
			}


			object.avg_y = 0;
			object.avg_x = 0;
			object.area = 0;
			for (unsigned int y = 0; y < prop.height; y++){
				for (unsigned int x = 0; x < prop.width; x++){
					if (bmp[y][x]){
						object.avg_y += y;
						object.avg_x += x;
						object.area++;
					}
				}
			}
			object.avg_y = object.avg_y/object.area + object.min_y;
			object.avg_x = object.avg_x/object.area + object.min_x;
		}
	}

private:
	//pair<pixel_group_id,equivalency_set>
	std::vector<ns_pseudonym_equivalency_set> equivalency_sets;
	unsigned long start_of_last_active_set_row;
	unsigned long start_of_this_active_set_row;
	unsigned long estimated_number_of_objects;
};


///After a threshold is applied to an image, contiguous regions are detected by ns_detected_object.  ns_line_store holds
///pixel information for detected contiguous images as they are loaded in line by line.  ns_line_store collects lines dynamically
//as they are found.  When all lines are loaded and the dimentions of the object bitmap are known, the pixel information is transferred to
///a ns_image_bitmap object.
class ns_line_store{
public:
	ns_line_store(){}
	unsigned int absolute_x_start;
	unsigned int absolute_y_start;
	std::vector< bool > vals;
};*/
struct ns_rle_run{
	ns_rle_run(){}
	ns_rle_run(const unsigned long r, const unsigned long s, const unsigned long e, const unsigned long l):row(r),start_col(s),end_col(e),perm_label(l){}
	unsigned long row,start_col,end_col,perm_label;
};
struct ns_label_entry{
	unsigned long label,next;
};
struct ns_row_boundaries{
	ns_row_boundaries(){}
	ns_row_boundaries(const unsigned long start_,const unsigned long stop_):start(start_),stop(stop_){}
	unsigned long start,stop;
};

class ns_connected_component_analyzer{
public:
	ns_connected_component_analyzer():labels(1),eq_classes(1){}//indicies start at 1 to allow 0 to indicate null

	template<class ns_component>
	void detect_objects(const ns_image_whole<ns_component> & im, std::vector<ns_detected_object *> & output){
		populate_rle_table_from_image(im);
		calculate_equivalency_table();
		generate_objects_from_equivalency_table(im.properties(),output);
	}

	//find all pixels in the bitmap im belonging to the object at the specified location pos, and highlight them in output/
	template<class ns_component>
	void find_object_around_pixel(const ns_image_whole<ns_component>& im, const ns_vector_2i & pos,ns_image_bitmap & output) {
		populate_rle_table_from_image(im);
		calculate_equivalency_table();
		output.prepare_to_recieve_image(im.properties());
		expand_bitmap_from_point(pos,output);
	}

private:
	std::vector<ns_row_boundaries> row_boundaries;
	std::vector<ns_rle_run> runs;
	std::vector<ns_label_entry> labels;
	std::vector<unsigned long> eq_classes;

	unsigned long get_new_plabel();
	void generate_objects_from_equivalency_table(const ns_image_properties & prop, std::vector<ns_detected_object *> & output);
	void calculate_equivalency_table();
	void make_equivalent(const unsigned long l1,const unsigned long l2);

	template<class ns_component>
	void populate_rle_table_from_image(const ns_image_whole<ns_component> & im){
		runs.reserve(im.properties().height/2);
		labels.reserve(im.properties().height/2);
		eq_classes.reserve(im.properties().height/2);

		row_boundaries.resize(im.properties().height);

		//do the run length encoding
		for (unsigned int y = 0; y < im.properties().height; y++){
			bool state(false);
			unsigned long start_x;

			unsigned long start_index_of_this_row((unsigned long)runs.size());

			for (unsigned int x = 0; x < im.properties().width; x++){
				if (!state && im[y][x]){
					//we've found the start of a thresholded region
					state = true;
					start_x = x;
				}
				if (state && !im[y][x]){
					//we've found the end of a thresholded region
					state = false;
					runs.push_back(ns_rle_run(y,start_x,x-1,0));
				}
			}
			if (state)
				runs.push_back(ns_rle_run(y,start_x,im.properties().width-1,0));
			//record where each row of the image starts and stops in the run table
			if (start_index_of_this_row == runs.size()){
				row_boundaries[y].start = 0;
				row_boundaries[y].stop = 0;
			}
			else{
				row_boundaries[y].start = start_index_of_this_row;
				row_boundaries[y].stop = (unsigned long)runs.size();
			}
		}
	}


	void expand_bitmap_from_point(const ns_vector_2i& pos, ns_image_bitmap& output);
};


///Provided with a bitmap image (usually obtained via thresholding a grayscale image), ns_contiguous_identifier generates
///a list of regions each representign a set of contiguous white pixels surrounded on all sides by black pixels.
///Contrary to its name, ns_detected_object_identifier not only locates regions but also analyzes them
///to determine whether they correspond to worm or non-worm objects.


template<class ns_component>
void ns_identify_contiguous_bitmap_regions(const ns_image_whole<ns_component> & im,std::vector<ns_detected_object *> & output){

	if  (im.properties().width*im.properties().height == 0)
		return;

	ns_connected_component_analyzer cc;
	cc.detect_objects(im,output);

	//And we're done!
}


/*
class ns_detected_object_builder{
public:
	ns_detected_object_builder():min_x(UINT_MAX), min_y(UINT_MAX), max_x(UINT_MAX),max_y(UINT_MAX),avg_x(0),avg_y(0){}
	///temporary storage used to build the bitmap during region detection
	std::map<unsigned int, ns_line_store> line_storage;

	unsigned int min_x, max_x, min_y, max_y, avg_x, avg_y;

	///function used to add points to a contiguous region during region detection
	///points must be registerd in order , downward and to the right, otherwise
	///the algorithm breaks down.
	inline void register_point(const unsigned int & x, const unsigned int & y){
		if (x < min_x)
			min_x = x;
		if (y < min_y)
			min_y = y;
		if (x > max_x || max_x==UINT_MAX)
			max_x = x;
		//we have a new line; initialize it
		if (y > max_y || max_y==UINT_MAX){
			line_storage[y].absolute_x_start = x;
			max_y = y;
		}
		ns_line_store &ls = line_storage[y];
		//if the current line needs to be bigger, make it so
		if (ls.vals.size() <= (x - ls.absolute_x_start))
			ls.vals.resize(x - ls.absolute_x_start + 1,false);
		ls.vals[x-ls.absolute_x_start] = true;
	}

	inline unsigned int width() const{return max_x - min_x + 1;}
	inline unsigned int height() const{return max_y - min_y + 1;}


	///after region detection is completed
	///finalize_bitmaps() should be called to transfer the
	///temporary data structures in line_storage into the final bitmap.
	void output_detected_object(const float bitmap_resolution,ns_detected_object & object){

		ns_image_properties bprop;
		bprop.height = height();
		bprop.width = width();
		bprop.resolution = bitmap_resolution;
		bprop.components = 1;
		object._bitmap->prepare_to_recieve_image(bprop);
	//	cerr << "Early bitmap size = " << _bitmap->properties().resolution << "\n";
		for(unsigned int y = 0; y < bprop.height; y++)
			for (unsigned int x =0; x < bprop.width; x++)
				(*object._bitmap)[y][x] = false;

		for (std::map<unsigned int, ns_line_store>::iterator p = line_storage.begin(); p != line_storage.end(); p++){
			for (unsigned int x = 0; x < p->second.vals.size(); x++)
				(*object._bitmap)[p->first - min_y][(x + p->second.absolute_x_start) - min_x] = p->second.vals[x];
		}
		line_storage.clear();

		object.min_x=min_x;
		object.max_x=max_x;
		object.min_y=min_y;
		object.max_y=max_y;


		//calculate area and center of mass
		object.avg_y = 0;
		object.avg_x = 0;
		object.area = 0;
		for (unsigned int y = 0; y < bprop.height; y++){
			for (unsigned int x = 0; x < bprop.width; x++){
				if ((*object._bitmap)[y][x]){
					object.avg_y += y;
					object.avg_x += x;
					object.area++;
				}
			}
		}
		object.avg_y = object.avg_y/object.area + object.min_y;
		object.avg_x = object.avg_x/object.area + object.min_x;


	}
};*/

/*template<class whole_image>
void ns_identify_contiguous_bitmap_regions(const whole_image & im,std::vector<ns_detected_object *> & output){

	if  (im.properties().width*im.properties().height == 0)
		return;

	ns_pseudonym_equivalency_calculator eq;
	for (unsigned int y = 0; y < im.properties().height; y++){
		bool state(0);
		unsigned long start_x;
		for (unsigned int x = 0; x < im.properties().width; x++){
			if (!state && im[y][x]){
				//we've found the start of a thresholded region
				state = true;
				start_x = x;
			}
			if (state && !im[y][x]){
				//we've found the end of a thresholded region
				state = false;
				eq.add_pixel_to_equivalency_set(im.properties().width*y+start_x,start_x,x-start_x,y);
			}
		}
		//we're done with the line
		eq.increment_start_of_active_sets();
	}
	eq.collapse_equality_sets();
	eq.produce_detected_objects(output,im);

}*/

/*

///ns_detected_object_indentier detects all regions of a bitmap containing contiguous pixels.  During this process
///each contiguous region is assigned a pseudonym.  As the image is processed, seperate regions are often discovered to contain
///contiguous pixels.  Rather than going back and merging the two regions, their ids are simply registered as pseudonyms for the same
//region.  ns_pseudonym_maintainer maintains the pseudonym std::mappings of various regions
class ns_pseudonym_maintainer{
public:
	ns_pseudonym_maintainer():region_count(0){}
	void set(const unsigned long & name, const unsigned long & pseudonym);

	inline const unsigned long & operator[](const unsigned long &pseudonym) const{
		std::map<unsigned long, unsigned long>::const_iterator p = pseudonyms.find(pseudonym);
		if (p == pseudonyms.end())
			return pseudonym;
		return p->second;
	}

	void dump(std::ostream & out){
		for (std::map<unsigned long, unsigned long>::const_iterator p = pseudonyms.begin(); p!= pseudonyms.end(); p++)
			out << p->first << "->" << p->second << "\n";
	}

	unsigned long number_of_unique_names(){
		return region_count;
	}

	~ns_pseudonym_maintainer(){
		for (std::map<unsigned long, std::vector<unsigned long *> *>::iterator p = reverse_pseudonym_lookup.begin(); p != reverse_pseudonym_lookup.end(); p++)
			delete p->second;
	}
private:
	std::map<unsigned long, unsigned long> pseudonyms;
	std::map<unsigned long, std::vector<unsigned long *> *> reverse_pseudonym_lookup;
	unsigned long region_count;
};




///Provided with a bitmap image (usually obtained via thresholding a grayscale image), ns_contiguous_identifier generates
///a list of regions each representign a set of contiguous white pixels surrounded on all sides by black pixels.
///Contrary to its name, ns_detected_object_identifier not only locates regions but also analyzes them
///to determine whether they correspond to worm or non-worm objects.

template<class whole_image>
void ns_identify_contiguous_bitmap_regions(const whole_image & im,std::vector<ns_detected_object *> & output){

	///This buffer assigns each pixel to a numbered region
	///0 means a pixel is not in the current image
	///1 or higher means the pixel is in a region with that pseudonym.
	///An example of two objects is given below.
	//000000000000000000
	//000011110000220000
	//000001110022222000
	//000000110022222000
	//000000010002220000
	//000000000000000000
	//Up to (2^16)-1 regions can be held
	ns_image_standard_32_bit ids;


	if  (im.properties().width*im.properties().height == 0)
		return;

	//In each line, each contiguous run of 1 pixels is given its own name.
	//Many of these runs will actually belong to the same region, connected
	//by 1 pixels on the next line.  the pseudonym std::map keeps track of which names
	//actually point to the same region. (pseudonym, actual identity)
	std::map<unsigned long, unsigned long> area_pseudonyms;
	ns_pseudonym_maintainer pseudonyms;
	//cerr << "Allocating Swap space...";
	ids.resize(im.properties());
	//cerr << "\nDetecting regions...";
	//Go through the entire image and mark regions and annotate pseudonyms
	bool in_object = false;
	unsigned long next_available_id= 1;
	unsigned long cur_id = 1;

	#ifdef ID_DEBUG_OUTPUT
	deb.open("c:/worm_debug.txt");
	#endif

	for (unsigned y = 0; y < im.properties().height; y++){
		for (unsigned x = 0; x < im.properties().width; x++){
			if (!in_object){
				//if you have entered a new region
				if (im[y][x] != 0){
					in_object = true;
					//check to see if the new region has an existing region directly above it.
					//if this is true, no need to make a new pseudonym, just grab its id.
					if (y != 0 && ids[y-1][x] != 0)
						cur_id = ids[y-1][x];
					//otherwise, make a new id for the region.
					else{
						cur_id = next_available_id;
						//area_pseudonyms[cur_id] = cur_id;  //register the id so it appears when you look for pseudonyms for it
						//reverse_pseudonyms[cur_id] = cur_id;
						next_available_id++;
					}
					ids[y][x] = cur_id;  //mark the current region in the std::map, so that lower lines can look to see
										 //if they are contiguous with existing regions.
				}
				//mark the current pixel as not in a region, so that lower lines will see that there is no
				//contiguous region above them
				else ids[y][x] = 0;
			}
			//if the cursor is currently inside a contiguous run on a line:
			else{
				//A false value indicates you've hit the end of the region.
				if ( im[y][x] == 0){
					in_object = false;
					ids[y][x] = 0;
				}
				else {
					//Check to see if the region is now contiguous with another region directly above it
					if (y != 0 && ids[y-1][x] != 0){
						//if the current region isn't already rooted to the same location as
						//region above, root the current region (and the current region's children) there.
						unsigned long pseud = pseudonyms[ids[y-1][x]];
						if (pseud != cur_id){
							pseudonyms.set(pseud,cur_id);
							//unsigned long pseud = area_pseudonyms[ids[y-1][x]];
							//if (pseud != cur_id){	//you are the pseudonym!
							//	area_pseudonyms[cur_id] = pseud;
							//}
							#ifdef ID_DEBUG_OUTPUT
							deb << cur_id << "->" << pseud << "\n";
							#endif
						}
						cur_id = pseud;
					}

					ids[y][x] = cur_id;  //register the current pixel in the id image so that lower lines
										 //will see that there is a contiguous region above.
				}
			}
		}
		//you've reached the end of a line
		in_object = false;
	}

	#ifdef ID_DEBUG_OUTPUT
		for (unsigned int y = 0; y < ids.properties().height; y++){
			for (unsigned int x = 0; x < ids.properties().width; x++)
				deb << ids[y][x] << "\t";
			deb << "\n";
		}
	#endif






	std::vector<ns_detected_object_builder> regions;
	//cerr << "\nCollecting Region information...";

	std::map<const unsigned int, unsigned int> region_stats;

	//walk through the image that holds all contiguous regions (under pseudonyms)
	//and register all the pixels of each region.
	regions.reserve(10000);
	for (unsigned y = 0; y < ids.properties().height; y++){

		//cerr << y << "\n";
		//cerr << "y: " << y << "\n";
		for (unsigned x = 0; x < ids.properties().width; x++){
			if (ids[y][x] == 0)
				continue;
		//	cerr << "Looking for pseudonym" << ids[y][x] << "\n";

			//unsigned int id = p->second;
			unsigned int id = pseudonyms[ids[y][x]];
			ids[y][x] = id;
		//	cerr << "Looking for region " << id << "\n";
			//If the region has not been encountered before, create a new record for it.
			//cerr << "1";
			//cerr << "id=" << id << ":";
			if (region_stats.find(id) == region_stats.end()){
		//		cerr << "Creating the new region...\n";

				//what we're doing here is creating a std::map address table into the region[] std::vector
				unsigned int cs = (unsigned int)region_stats.size();
				regions.resize(cs+1);
				//cerr << "2";
				region_stats[id] = cs;
				//debucerr << "3";
//					cerr << "curi = " << cur_i << "/" << s << "\n";
				regions[cs].register_point(x,y);
//					cur_i++;
			}
			//otherwise, grab a reference to the existing record.
			else {
		//		cerr << "Region already exists.\n";

			//	cerr << ".";
				regions[region_stats[id]].register_point(x,y);
			}
			//cerr << "\n";

		}
	}

	#ifdef ID_DEBUG_OUTPUT
	for (unsigned int y = 0; y < ids.properties().height; y++){
		for (unsigned int x = 0; x < ids.properties().width; x++)
			deb << ids[y][x] << "\t";
		deb << "\n";
	}
	pseudonyms.dump(deb);
	deb.close();
	#endif
	output.resize(regions.size());
	for (unsigned int i = 0; i < output.size(); i++)
		output[i] = new ns_detected_object;
	for (unsigned int i = 0; i < regions.size(); i++)
		regions[i].output_detected_object(im.properties().resolution,*output[i]);

	ids.clear();
}
*/
#endif
