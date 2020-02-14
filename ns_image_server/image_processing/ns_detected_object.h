#ifndef ns_detected_object_H
#define ns_detected_object_H

#include "ns_image.h"
#include <vector>
#include <map>
#include "ns_vector.h"
#include "ns_node_topology.h"
#include "ns_segment_topology.h"
#include "ns_progress_reporter.h"


///information about an area of an image that is known to contain a worm
///from temporal interpolation, but to which statistics cannot be calculated
struct ns_interpolated_worm_area{
	ns_interpolated_worm_area(){}
	ns_interpolated_worm_area(const ns_vector_2i & o, const ns_vector_2i & s):position_in_source_image(o),size(s){}
	ns_vector_2i position_in_source_image;
	ns_vector_2i size;
};


///ns_detected_object manages information about a contiguous region in an image, during and after it is located within a thresholded image.
///ns_detected_object objects are created and managed by an ns_detected_object_identifier object.
///Ultimately, ns_detected_object objects are translated into ns_detected_worm_info objects via
///the ns_detected_worm_info::from_region() function during worm detection by a ns_image_worm_detection_results object
class ns_detected_object{
public:
	ns_detected_object():area(0),size(0,0),offset_in_source_image(0,0),
						   _bitmap(new ns_image_bitmap()),_edge_bitmap(new ns_image_bitmap()),
						   must_be_a_worm(false),must_not_be_a_worm(false),is_a_merged_object(false){}

	//min_x,y and max_x,y are the coordinates of the
	//large, original image from which the region was identified.
	//avg_x,y is the center of mass, again in the coordinates of original image;
	ns_vector_2i size;
	ns_vector_2i center_in_source_image()const{return (offset_in_source_image+size)/2;}
	ns_vector_2i offset_in_source_image;
	

	//area, in square pixels, of white points in the bitmap
	unsigned int area,
		name;

	//if set to true, the specified region is always counted as a worm in later processing steps, pre-empting SVM categorization
	bool must_be_a_worm;
	//if set to true, the specified region is always counted as dirt (ie not a worm) in later processing steps, pre-empting SVM categorization
	bool must_not_be_a_worm;

	bool is_a_merged_object;

	///returns the bitmap corresponding to the contiguous region
	ns_image_bitmap & bitmap(){ if (_bitmap == 0)
		throw ns_ex("ns_detected_object::Accessing deleted bitmap!"); return *_bitmap;}
	///returns the bitmap corresponding to the contiguous region
	const ns_image_bitmap & bitmap() const{ if (_bitmap == 0)
		throw ns_ex("ns_detected_object::Accessing deleted bitmap!"); return *_bitmap;}

	///returns a bitmap whose white pixels corresponding to the outline of a contiguous region
	ns_image_bitmap & edge_bitmap() { if (_edge_bitmap == 0)
		throw ns_ex("ns_detected_object::Accessing deleted edge bitmap!"); return *_edge_bitmap;}


	///returns the region's bitmap while releasing it from the region.  Allows movement of
	///bitmap data without re-copy
	ns_image_bitmap * transfer_bitmap(){
		ns_image_bitmap * b = _bitmap;
		_bitmap = 0;
		return b;
	}

	//allows raw access to the region's bitmap pointer.  Hack alert.
	void accept_bitmap(ns_image_bitmap * b){
		if (_bitmap != 0)
			delete _bitmap;
		_bitmap = b;
	}
	///returns the region's edge bitmap while releasing it from the region.  Allows movement of
	///edge bitmap data without re-copy
	ns_image_bitmap * transfer_edge_bitmap(){
		ns_image_bitmap * b = _edge_bitmap;
		_edge_bitmap = 0;
		return b;
	}

	///accepts the edge bitmap and assigns it to the region's bitmap.  Allows movement
	///of edge bitmap data without re-copy.
	void accept_edge_bitmap(ns_image_bitmap * b){
		if (_edge_bitmap != 0)
			delete _edge_bitmap;

		_edge_bitmap = b;
	}
	///a 2d triangular mesh built from the region bitmap
	std::vector<ns_triangle_d> mesh;


	void calculate_edges();
	void remove_small_holes();

	///holds a single point for every hole (black area surrounded by white) in the bitmap
	std::vector<ns_vector_2d> holes;
	std::vector<ns_vector_2d> edge_coordinates;
	std::vector<ns_edge_ui> edge_list;
	std::vector<ns_edge_2d> edges;

	ns_node_topology node_topology;
	ns_segment_cluster segment_cluster;
	ns_segment_cluster_solutions segment_cluster_solutions;



	///used to hold information about the region (such as why it was rejected by the worm detection algorithm)
	///that can be displayed during visulalization
	std::string label;

	~ns_detected_object(){
		ns_safe_delete(_bitmap);
		ns_safe_delete(_edge_bitmap);
	}


private:

	///bitmaps that contains the region and edge data
	ns_image_bitmap * _bitmap;
	ns_image_bitmap * _edge_bitmap;



	///fill an image with specified value
	/*template<class ns_component, class whole_image>
	void fill_region_in_image(ns_component value, whole_image & image, unsigned int x_components = 1){
		unsigned int _width = _bitmap->properties().width;
		unsigned int _height = _bitmap->properties().height;

		for (unsigned int y = 0; y < _height; y++){
			for (unsigned int x = 0; x < _width; x++){
				if ((*_bitmap)[y][x]){
					//	cerr << "light";
						for (unsigned int i = 0; i < x_components; i++)
							image[y+min_y][(x+min_x)*x_components +i] = value;
				}
			}
		}
	}*/

	///Cross bounding box stats used during visualization
	int cross_bounding_box_b_x,
		cross_bounding_box_b_y,
		cross_bounding_box_t_x,
		cross_bounding_box_t_y,
		cross_half_length,
		cross_half_width;

	///used to draw a crosshair over the center of a worm
	inline void set_cross_size(const unsigned int half_length, const unsigned int half_width){
		ns_vector_2i c(center_in_source_image());
		cross_bounding_box_b_x = c.x - half_length;
		cross_bounding_box_b_y = c.y - half_length;
		cross_bounding_box_t_x = c.x + half_length;
		cross_bounding_box_t_y = c.y + half_length;
		cross_half_length = half_length;
		cross_half_width = half_width;

		if (cross_bounding_box_b_x < 0) cross_bounding_box_b_x = 0;
		if (cross_bounding_box_b_y < 0) cross_bounding_box_b_y = 0;
		if (cross_bounding_box_t_x < 0) cross_bounding_box_t_x = 0;
		if (cross_bounding_box_t_y < 0) cross_bounding_box_t_y = 0;
	}

	///used to draw a crosshair over the center of a worm in case where the entire image is not available.
	inline bool is_within_crosshair(const unsigned int x, const unsigned int y) const{
		//reject most points quickly
		if (x < (unsigned int)cross_bounding_box_b_x || x > (unsigned int)cross_bounding_box_t_x ||
			y < (unsigned int)cross_bounding_box_b_y || y > (unsigned int)cross_bounding_box_t_y)
			return false;
		ns_vector_2i c(center_in_source_image());
		//inside vertical line
		if ((long)x > c.x - cross_half_width && (long)x < c.x+ cross_half_width && (long)y > c.y- cross_half_length && (long)y <c.y + cross_half_length)
			return true;

		//inside horizontal line
		if ((long)x > (long)c.x - (long)cross_half_length && (long)x < (long)c.x + (long)cross_half_length && (long)y > (long)c.y- (long)cross_half_width && (long)y < (long)c.y + (long)cross_half_width)
			return true;
		return false;

	}

	friend class ns_detected_object_builder;
};

class ns_detected_object_manager{
public:
	~ns_detected_object_manager(){clear();}
	std::vector<ns_detected_object *> objects;
	std::vector<char> censor_object; // if 1, do not include that object in the dataset.

	std::vector<ns_interpolated_worm_area> interpolated_objects;

	void add_interpolated_worm_area(const ns_interpolated_worm_area & area){
		interpolated_objects.push_back(area);
	}

	///Goes through the regions std::vector and removes all regions do not fall within the specified size tolerences
	void constrain_region_area(unsigned int minimum_area, unsigned int maximum_area, const unsigned int max_diagonal);
	///Goes through the regions std::vector and removes the largest percent_cutoff fraction of regions.
	void remove_largest_images(double percent_cutoff);

	///Spine visualzation images are calculated at detection time.
	///This needs to be done immediately because following detection, mesh information
	///is discarded.  Visualzations are stored here.
	std::vector<ns_image_standard> spine_visualizations;
	//the provided image is interpreted as a list of pixels that are "static", ie that should not
	//be marked as part of putative worms.  Any object that has beyond the specifified threshold
	//of its pixels marked as static is eliminated from the list of putative worms.
	void remove_objects_found_in_static_mask(const ns_image_standard & static_mask);

	void clear(){
		for (unsigned int i = 0; i < objects.size(); i++)
			ns_safe_delete(objects[i]);
		objects.resize(0);
	}

	void convert_bitmaps_into_node_graphs(const float resolution, const std::string & debug_filename){
	//	unsigned long start_time = ns_current_time();
		if (resolution <= 1201){
		  std::string dbg_name;
			for (unsigned int i = 0; i < objects.size(); i++){
				objects[i]->calculate_edges();
				if (!debug_filename.empty())
				  dbg_name = debug_filename + "_" + ns_to_string(i);
				objects[i]->node_topology.build_via_delauny_method(objects[i]->edge_coordinates,objects[i]->edge_list,
																			objects[i]->holes,dbg_name);
			}
			remove_empty_spines();
		}
		else{
			for (unsigned int i = 0; i < objects.size(); i++){
				objects[i]->remove_small_holes();
				objects[i]->node_topology.build_via_zhang_thinning(objects[i]->bitmap());
			}
		}

	}

	void calculate_segment_topologies_from_node_graphs(const bool do_multiple_worm_disambiguation){
		//unsigned long start_time = ns_current_time();
		//std::cerr << "Reducing node graphs: ";
		std::cerr << "Fitting Splines...";
		ns_progress_reporter pr(objects.size(),5);
		for (unsigned int i = 0; i < objects.size(); i++){
			pr(i);
			//cerr << "WRITING DEBUG bitmap\n";
			//ns_save_image("c:\\tt\\current_bitmap.tif",objects[i]->bitmap());
			objects[i]->segment_cluster.compile(objects[i]->node_topology);
			objects[i]->segment_cluster_solutions.calculate(objects[i]->segment_cluster,objects[i]->bitmap().properties().resolution,do_multiple_worm_disambiguation);
		}
		pr(objects.size());
		//cerr << "Calculated segment topologies: " << ns_current_time() - start_time << "\n";
	}
private:
	///removes all regions whose delauny triangular mesh did not produce a valid spine
	void remove_empty_spines();

};

#endif
