#include "ns_vector_bitmap_interface.h"
#include "ns_identify_contiguous_bitmap_regions.h"
#ifndef NS_MINIMAL_SERVER_BUILD
using namespace std;


/*
std::string ns_color_to_hex_string(const ns_color_8 & c){
	throw ns_ex("ns_color_to_hex_string()::Not Implemented");
}

ns_color_8 ns_hex_string_to_color(const std::string & c){
	if (c.empty())
		return ns_color_8(0,0,0);
	if (c.size() != 6)
		throw ns_ex("ns_hex_string_to_color::Invalid color!");
	char a[3][3];
	a[0][0] = c[0];
	a[0][1] = c[1];
	a[0][2] = 0;
	a[1][0] = c[2];
	a[1][1] = c[3];
	a[1][2] = 0;
	a[2][0] = c[4];
	a[2][1] = c[5];
	a[2][2] = 0;

	ns_color_8 col(strtol(a[0],0,16),strtol(a[1],0,16),strtol(a[2],0,16));
	return col;
}*/


//we are removing triangles from the mesh whose neighbors are invalid triangles.
//to remove the triangle at index i in mesh[], set flag at index i in remove[]
void ns_clear_neighboring_triangles(const int cur_triangle, const int cur_triangle_offending_side,
								 const int neighbor_triangle, 
								 const std::vector<ns_triangle_d> & mesh,
								 const int * neighbors,
								 std::vector<char> & remove){

	if (cur_triangle == -1)
		return;
	//mark current triangle for removal
	remove[cur_triangle] = true;

	//if an error has occurred, stop
	if (neighbor_triangle == -1 || remove[neighbor_triangle])
		return;
	

	//now, we collect enough information to see if there is an invalid neighbor

	//the problem here is that the numbering between the two triangles aren't consistant, so 
	//we have to rebuild the topology of the three, figuring out the shared side, the shared vertex,
	//and which of the two shared vertex, along with the unique vertex of the current triangle,
	//generated the offending side.

    //determine which vertexes are shared between the two triangles, and which vertex of each is not shared (is "unique");
	std::vector<int> neighbor_shared_vertex;
	std::vector<int> cur_shared_vertex;
	int neighbor_unique_vertex,
		cur_unique_vertex;
	bool is_new_vertex_neighbor[3] = {true, true, true};  //used to find new vertex in neighbor
	bool is_new_vertex_cur[3] = {true, true, true};  //used to find new vertex in neighbor
	for (unsigned int i = 0; i < 3; i++){ //look for shared values
		for (unsigned int j = 0; j < 3; j++)
			if (mesh[neighbor_triangle].vertex[i] == mesh[cur_triangle].vertex[j]){
				is_new_vertex_neighbor[i] = false;
				is_new_vertex_cur[j] = false;
			}
	}
	for (unsigned int i = 0; i < 3; i++){  //use shared values to populate neighbor/unique information
		if (!is_new_vertex_neighbor[i])
			neighbor_shared_vertex.push_back(i);
		else neighbor_unique_vertex = i;
		if (!is_new_vertex_cur[i])
			cur_shared_vertex.push_back(i);
		else cur_unique_vertex = i;
	}
	//make sure exactly two vertexes are shared between the triangles. 
	//If this isn't the case, then the two triangles weren't really neighbors and we're sunk.
	if (neighbor_shared_vertex.size() != 2 || cur_shared_vertex.size() != 2)
		throw ns_ex("ns_clear_neighboring_triangles::Incorrect neighboring triangle!");


	//now we have al the correct information.  A few more calculations and we'll be there.

	//on the current triangle, find the shared point that generated the offending side with the unqiue current pixel
	int cur_vertex_to_check = ns_third_point(cur_triangle_offending_side,cur_unique_vertex);
	

	//find the correspoinding point (the poitn that generated the offending side) on the neighbor triangle
	int neighbor_vertex_to_check = -1;
	for (unsigned int i = 0; i < 3; i++)
		if (mesh[neighbor_triangle].vertex[i] == mesh[cur_triangle].vertex[cur_vertex_to_check])
			neighbor_vertex_to_check = i;
	if (neighbor_vertex_to_check == -1)
		throw ns_ex("ns_clear_neighboring_triangles::Vertex to check is not shared between current and neighbor.");
	//now we have all the info we need.

	//find the side on the neighbor that could possibly be incorrect as well
	int putative_bad_neighbor_side = ns_third_point(neighbor_vertex_to_check,neighbor_unique_vertex);
	int putuative_bad_neighbor_id = neighbors[3*neighbor_triangle+neighbor_vertex_to_check];

	//if the side is indeed a bad side, move on to it.
	if (ns_neighboring_vertex(mesh[neighbor_triangle].vertex[neighbor_vertex_to_check],mesh[neighbor_triangle].vertex[neighbor_unique_vertex]))
		ns_clear_neighboring_triangles(neighbor_triangle,
										putative_bad_neighbor_side,
										putuative_bad_neighbor_id , 
										mesh,neighbors,remove);
}

void ns_process_hole_finding(ns_image_bitmap & temp, std::vector<ns_vector_2d> & holes){
	
	//at this point all holes should be white on a black background.
	std::vector<ns_detected_object *> hole_objects;
	ns_identify_contiguous_bitmap_regions(temp,hole_objects);
	try{
		//now we note the center of each hole in the hole list.
		for (unsigned int i = 0; i  < hole_objects.size(); i++)
			holes.push_back(ns_vector_2d(hole_objects[i]->center_in_source_image()));
	}
	catch(...){
		for (unsigned int i = 0; i < hole_objects.size(); i++)
			delete hole_objects[i];
		throw;
	}
	for (unsigned int i = 0; i < hole_objects.size(); i++)
		delete hole_objects[i];
}
#endif
//Let E be the set {0,1,2}
//Let S be the set {i,j}
//return the element of E that is not in S.
int ns_third_point(const int & i, const int & j){
	if(i + j == 1)  // 0,1 -> 2
		return 2;
	if (i+j == 2)	// 0,2 -> 1
		return 1;
	if (i+j == 3)	//1,2 -> 0
		return 0;
	return 0;
	throw ns_ex("ns_third_point: Passed ") << i << "," << j;
}

void ns_calculate_curvature_stats(const std::vector<float> & curvature, ns_curvature_stats & stats){
	stats.total = 0;
	stats.maximum = 0;
	stats.displacement_total = 0;
	for (unsigned int i = 0; i < curvature.size(); i++){
		if (fabs(curvature[i]) > stats.maximum)
			stats.maximum = fabs(curvature[i]);
		stats.total+=curvature[i];
		stats.displacement_total += fabs(curvature[i]);
	}
	if (curvature.size() == 0){
		stats.average = 0;
		stats.displacement_average = 0;
	}
	else{
		stats.average = stats.total/curvature.size();
		stats.displacement_average = stats.displacement_total/curvature.size();
	}
	stats.variance = 0;
	for (unsigned int i = 0; i < curvature.size(); i++)
		stats.variance+=(curvature[i]-stats.average)*(curvature[i]-stats.average);
	if (curvature.size() == 0)
		stats.variance = 0;
	else stats.variance = stats.variance/curvature.size();
	//calculate the number of x intercepts (curvature[i] == 0) present in the curvature
	const int span(3);
	stats.x_intercept_count = 0;
	bool last_point_was_span = false;
	for (unsigned int i = span; i < curvature.size() - span; i++){
		double a = curvature[i-span],
			   b = curvature[i+span];
		bool same_sign = ((int)(a/fabs(a)) == (int)(b/fabs(b)));
		if (!same_sign && !last_point_was_span)
			stats.x_intercept_count++;
		last_point_was_span = !same_sign;
	}
}

template<class T>
inline const T & ns_median_3(const T &a,const T &b,const T &c){
	if (a >= b){
		if (b > c)return b;
		if (c > a)return a;
	}
	if (b >= c){
		if (c > a)return c;
		return a;
	}
	if (a >= c) return c;
	return b;
}
template<class T>
inline void ns_mean_smooth(const std::vector<T> & in, std::vector<T> & out){
	out.resize(in.size());
	if (in.size() >= 3){
		out[0] = ns_median_3(out[0],out[1],out[2]);
		out[out.size()-1] = ns_median_3(in[out.size()-1],in[out.size()-2],in[out.size()-3]);
	}
	else if (out.size() == 2){
		out[0] = in[0];
		out[1] = in[1];
	}
	else out[0] = in[0];

	for (unsigned int i = 1; i < out.size()-1; i++)
		out[i] = ns_median_3(in[i-1],in[i],in[i+1]);
}

#ifndef NS_MINIMAL_SERVER_BUILD
template<class ns_component> 
void ns_remove_small_holes_template(ns_image_whole<ns_component> & source, unsigned long max_size_to_remove){
	ns_image_bitmap temp;//, temp2;
	temp.prepare_to_recieve_image(source.properties());
	const unsigned int w(temp.properties().width),
					   h(temp.properties().height);


   //invert the current region's bitmap
	for (unsigned int y = 0; y < h; y++){
		for (unsigned int x = 0; x < w; x++){
			temp[y][x] = !(source[y][x]);
		}
	}
	ns_flood_fill_from_outside(temp,false);

	//at this point all holes should be white on a black background.
	std::vector<ns_detected_object *> hole_objects;
	ns_identify_contiguous_bitmap_regions(temp,hole_objects);
	try{
		//fill in small holes
		for (unsigned int j = 0; j < hole_objects.size(); j++){
			if(hole_objects[j]->area <= max_size_to_remove){
				const ns_image_bitmap & bitmap = hole_objects[j]->bitmap();
				for (unsigned int y = 0; y < bitmap.properties().height; y++)
					for (unsigned int x = 0; x < bitmap.properties().width; x++)
						if (bitmap[y][x])
							source[y+hole_objects[j]->offset_in_source_image.y][x + hole_objects[j]->offset_in_source_image.x] = 1;
			}
		}
	}
	catch(...){
		for (unsigned int i = 0; i < hole_objects.size(); i++)
			ns_safe_delete(hole_objects[i]);
		hole_objects.resize(0);
		throw;
	}
	for (unsigned int i = 0; i < hole_objects.size(); i++)
		ns_safe_delete(hole_objects[i]);
	hole_objects.resize(0);

	//X shapes, ie
	// 0+0
	// +0+
	// 0+0
	//trip up zhang thinning.
	//remove them.
	for (unsigned int y = 1; y < h-1; y++)
		for (unsigned int x = 1; x < w-1; x++){
			if (source[y  ][x  ] &&
				!source[y  ][x-1] &&
				!source[y  ][x+1] &&
				!source[y-1][x  ] &&
				!source[y+1][x  ]
				)
				source[y  ][x  ] = false;
			if (!source[y  ][x  ] &&
				source[y  ][x-1] &&
				source[y  ][x+1] &&
				source[y-1][x  ] &&
				source[y+1][x  ]
				)
				source[y  ][x  ] = true;
		}
}


void ns_remove_small_holes(ns_image_whole<ns_8_bit> & source, unsigned long max_size_to_remove){
	ns_remove_small_holes_template(source,max_size_to_remove);
}
void ns_remove_small_holes(ns_image_whole<bool> & source, unsigned long max_size_to_remove){
	ns_remove_small_holes_template(source,max_size_to_remove);
}


std::stack<ns_vector_2i> ns_flood_fill_stack;
#endif
