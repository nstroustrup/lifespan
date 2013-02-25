#ifndef NS_VECTOR_BITMAP_INTERFACE
#define NS_VECTOR_BITMAP_INTERFACE

#include "triangle.h"
#include "ns_image.h"
#include <set>

#include "ns_font.h"
#include <stack>

//Tested for correctness 6/15/09
template<class C>
void ns_median_smoother(C & d, const long khw){
	typedef typename C::value_type T;
	std::vector<T> o(d.size());
	std::multiset<T> sorted;
	for (long i = 0; i <= 2*khw && i < (long)d.size(); i++){
		sorted.insert(d[i]);
	}
	long i;
	for (i = khw; (long)i < (long)(d.size())-khw-1; i++){
	
		typename std::multiset<T>::iterator mid(sorted.begin());
		for (long j = 0; j < khw; j++){
			mid++;
		}
		o[i] = *mid;
		typename std::multiset<T>::iterator p = sorted.find(d[i-khw]);
		sorted.erase(p);
		sorted.insert(d[i+khw+1]);
	}
	typename std::multiset<T>::iterator mid(sorted.begin());
	for (long j = 0; j < khw; j++){
		mid++;
	}
	o[i] = *mid;

	for (long i = khw; (long)i < (long)(d.size()-khw); i++)
		d[i] = o[i];
}


extern std::stack<ns_vector_2i> ns_flood_fill_stack;

//determines if two points "neighbor" each other, that is whether
//they are above, below, left, right, or at a diagonal from each other.
template<class vector_t>
inline bool ns_neighboring_vertex(const vector_t & start, const vector_t & stop){
	int dx = (int)fabs(start.x - stop.x),
		dy = (int)fabs(start.y - stop.y);
	return (dx == 1 && dy == 1) ||(dx == 0 && dy == 1) || (dx == 1 && dy == 0);
}
//we are removing triangles from the mesh whose neighbors are invalid triangles.
//to remove the triangle at index i in mesh[], set flag at index i in remove[]
void ns_clear_neighboring_triangles(const int cur_triangle, const int cur_triangle_offending_side,
								 const int neighbor_triangle, 
								 const std::vector<ns_triangle_d> & mesh,
								 const int * neighbors,
								 std::vector<char> & remove);
//Let E be the set {0,1,2}
//Let S be the set {i,j}
//return the element of E that is not in S.
int ns_third_point(const int & i, const int & j);
#include "ns_tiff.h"
//adapted from http://student.kuleuven.be/~m0216922/CG/floodfill.html
template<class binary_image>
void ns_flood_fill_from_outside(binary_image & im, const bool color){

	unsigned long w = im.properties().width,
				  h = im.properties().height;
	if (w == 0 || h == 0)
		return;
	//cerr << "Flood fill (" << w << "," << h <<")\n";
	//ns_tiff_image_output_file<ns_8_bit> tiff_out;
	//ns_image_stream_file_sink<ns_8_bit> file_sink("dbg.tif",tiff_out,128);
	//im.pump(file_sink,128);

	//add edges as start points.

	ns_flood_fill_stack.push(ns_vector_2i(0,0));
	ns_flood_fill_stack.push(ns_vector_2i(w-1,0));
	ns_flood_fill_stack.push(ns_vector_2i(0,h-1));
	ns_flood_fill_stack.push(ns_vector_2i(w-1,h-1));

	for (unsigned int x = 1; x < w; x++){
		if ((im[0][x] == color) && (im[0][x-1] != color))
				ns_flood_fill_stack.push(ns_vector_2i(x-1,0));
	}
	for (unsigned int y = 1; y< h; y++){
		if ((im[y][0] == color) && (im[y-1][0] != color))
				ns_flood_fill_stack.push(ns_vector_2i(0,y-1));
		if ((im[y][w-1] == color) && (im[y-1][w-1] != color))
				ns_flood_fill_stack.push(ns_vector_2i(w-1,y-1));
	}
	for (unsigned int x = 1; x < w; x++){
		if ((im[h-1][x] == color) && (im[h-1][x-1] != color))
				ns_flood_fill_stack.push(ns_vector_2i(x-1,h-1));
	}

	/*for (unsigned int y = 0; y < h; y++)
		pix.push(ns_vector_2i(w-1,y));
	
	for (unsigned int y = 0; y < h; y++)
		pix.push(ns_vector_2i(0,y));

	for (int x = (int)w-1; x >= 0; x--){
		pix.push(ns_vector_2i(x,h-1));
		pix.push(ns_vector_2i(x,0));
	}*/

	long _x;	
	//cerr << "{";
	while (!ns_flood_fill_stack.empty()){
		//cerr << pix.size() << ",";
		if (im[ns_flood_fill_stack.top().y][ns_flood_fill_stack.top().x] == color){
			ns_flood_fill_stack.pop();
			continue;
		}
		ns_vector_2i cur = ns_flood_fill_stack.top();
		ns_flood_fill_stack.pop();

		//if (cur.x >= (int)im.properties().width || cur.y >= (int)im.properties().height)
		//	throw ns_ex("ns_flood_fill::Invalid coordinate posted: ") << cur.x << "," << cur.y << "\n";
		
		//if (cur.x < 0 || cur.y < 0)
		//	throw ns_ex("ns_flood_fill::Invalid (low)coordinate posted: ") << cur.x << "," << cur.y << "\n";

		//scan to the left of the region at the current y
		_x = cur.x;
		while(_x > 0 && im[cur.y][_x-1] != color) 
			_x--;

		//move accross the region at the current y
		for(; _x < (long)w && im[cur.y][_x] != color; _x++){
		//	cerr << "(" << _x << "," << cur.y << ")";
			im[cur.y][_x] = color;

			if (cur.y > 0 && im[cur.y-1][_x] != color){
			//	cerr << "`";
				ns_flood_fill_stack.push(ns_vector_2i(_x,cur.y-1));
			//	cerr << "'";
			}
			if (cur.y < (int)h-1 && im[cur.y+1][_x] != color){
			//	cerr << "`";
				ns_flood_fill_stack.push(ns_vector_2i(_x,cur.y+1));
			//	cerr << "'";
			}
			//cerr << ";";
		}
		//cerr << ":";
	}
	//cerr << "}";
}

void ns_process_hole_finding(ns_image_bitmap & temp, std::vector<ns_vector_2d> & holes);


///if the object is laid out in white on a black background
///we flood fill starting from the outside, making the object
///blend in with the now-white background.
///any black pixels left are holes.
template<class binary_image>
void ns_find_holes(binary_image & source, std::vector<ns_vector_2d> & holes){
	ns_image_bitmap temp;//, temp2;
	temp.prepare_to_recieve_image(source.properties());
	//temp2.prepare_to_recieve_image(regions[i]->bitmap().properties());f
	const unsigned int w(temp.properties().width),
					   h(temp.properties().height);
   //invert the current region's bitmap
	for (unsigned int y = 0; y < h; y++)
		for (unsigned int x = 0; x < w; x++){
			temp[y][x] = !(source[y][x]);
			//temp2[y][x] = !(regions[i]->bitmap()[y][x]);
		}

	ns_flood_fill_from_outside(temp,false);
	ns_process_hole_finding(temp, holes);

}



void ns_remove_small_holes(ns_image_whole<ns_8_bit> & source, unsigned long max_size_to_remove);
void ns_remove_small_holes(ns_image_whole<bool> & source, unsigned long max_size_to_remove);

template<class ns_component>
class ns_erode_helper{
public:
	inline ns_component operator()(const ns_component &val, const ns_component &n1, const ns_component &n2, const ns_component &n3, const  ns_component &n4) const{
		return val && (((char)n1 + (char)n2 + (char)n3 + (char)n4) >= 3);
	}
};
template<class ns_component>
class ns_dilate_helper{
public:
	inline ns_component operator()(const ns_component &val, const ns_component &n1, const ns_component &n2, const ns_component &n3, const  ns_component &n4) const{
		return val || n1 ||n2 ||n3 ||n4;
	}
};
template<class ns_component>
class ns_edge_helper{
public:
	inline ns_component operator()(const ns_component &val, const ns_component &n1, const ns_component &n2, const ns_component &n3, const  ns_component &n4) const{
		return val && !(n1 && n2 && n3 && n4);
	}
};

template<class binary_image, class action_helper>
void ns_neighborhood_action(const binary_image & src,  binary_image & dst, const action_helper & helper){
	
	ns_image_properties p = src.properties();
	const long t((long)p.height-1);
	const long r((long)p.width-1);
	if (t < 0 || r < 0) return;

	//top edge
	dst[0][0] = helper(src[0][0],0,src[1][0],0,src[0][1]);
	for (long x = 1; x < r; x++)
		dst[0][x] = helper(src[0][x],0,src[1][x],src[0][x-1],src[0][x+1]);
	dst[0][r] = helper(src[0][r],0,src[1][r],src[0][r-1],0);

	//center
	for (long y = 1; y < t; y++){
		dst[y][0] = helper(src[y][0],src[y-1][0],src[y+1][0],0,src[y][1]);
		for (long x = 1; x < r; x++)
			dst[y][x] = helper(src[y][x],src[y-1][x],src[y+1][x],src[y][x-1],src[y][x+1]);
		dst[y][r] = helper(src[y][r],src[y-1][r],src[y+1][r],src[y][r-1],0);
	}
	//bottom edge
	dst[t][0] = helper(src[t][0],src[t-1][0],0,0,src[t][1]);
	for (long x = 1; x < r; x++)
		dst[t][x] = helper(src[t][x],src[t-1][x],0,src[t][x-1],src[t][x+1]);
	dst[t][r] = helper(src[t][r],src[t-1][r],0,src[t][r-1],0);
}

template<class binary_image>
void ns_create_edge_bitmap(const binary_image & input, ns_image_bitmap & output){

	binary_image eroded_bitmap;
	binary_image dialated_bitmap;

	eroded_bitmap.prepare_to_recieve_image(input.properties());	
	dialated_bitmap.prepare_to_recieve_image(input.properties());
	output.prepare_to_recieve_image(input.properties());


	ns_image_properties up = input.properties();
	//erode
	ns_neighborhood_action(input,eroded_bitmap,ns_erode_helper<ns_bit>());
	//dialate
	ns_neighborhood_action(eroded_bitmap,dialated_bitmap,ns_dilate_helper<ns_bit>());
	//find edges
	ns_neighborhood_action(dialated_bitmap,output,ns_edge_helper<ns_bit>());

	/*
	bool neighborhood[5]; //up, down, left, right, center
	//erode
	for (int y = 1; static_cast<unsigned int>(y) < up.height-1; y++){
		for (int x = 1; static_cast<unsigned int>(x) < up.width-1; x++){
			neighborhood[0] = input[y-1][x];
			neighborhood[1] = input[y+1][x];
			neighborhood[2] = input[y][x-1];
			neighborhood[3] = input[y][x+1];
			neighborhood[4] = input[y][x];
			eroded_bitmap[y][x] =  neighborhood[4] && ((char)neighborhood[0] + (char)neighborhood[1] + (char)neighborhood[2] + (char)neighborhood[3]) >= 3 ;
		}
	}	
	

	//dialate 
	for (int y = 0; static_cast<unsigned int>(y) < up.height; y++){
		for (int x = 0; static_cast<unsigned int>(x) < up.width; x++){

			neighborhood[0] = eroded_bitmap.slow_safe_access(y-1,x);
			neighborhood[1] = eroded_bitmap.slow_safe_access(y+1,x);
			neighborhood[2] = eroded_bitmap.slow_safe_access(y,x-1);
			neighborhood[3] = eroded_bitmap.slow_safe_access(y,x+1);
			neighborhood[4] = eroded_bitmap.slow_safe_access(y,x);
			dialated_bitmap[y][x] = (neighborhood[0] || neighborhood[1] || neighborhood[2] || neighborhood[3] || neighborhood[4]);
			
		}
	}	

	//find edges
	for (int y = 0; static_cast<unsigned int>(y) < up.height; y++){
		for (int x = 0; static_cast<unsigned int>(x) < up.width; x++){

			neighborhood[0] = dialated_bitmap.slow_safe_access(y-1,x);
			neighborhood[1] = dialated_bitmap.slow_safe_access(y+1,x);
			neighborhood[2] = dialated_bitmap.slow_safe_access(y,x-1);
			neighborhood[3] = dialated_bitmap.slow_safe_access(y,x+1);
			neighborhood[4] = dialated_bitmap.slow_safe_access(y,x);
			output[y][x] = dialated_bitmap[y][x] && !(neighborhood[0] && neighborhood[1] && neighborhood[2] && neighborhood[3]);
		}
	}*/

}

//produces the set of image pixels along the outside edge of an image.
//Subtracts the dialation of im with a plus-shaped kernal from the original image

template<class binary_image>
void ns_find_edge_coordinates(binary_image & im, ns_image_bitmap & edge_bitmap, std::vector<ns_vector_2d> & output_coordinates, std::vector<ns_vector_2d> & holes, std::vector<ns_edge_ui> & edges, const bool find_holes=true){
	output_coordinates.clear();
	edges.clear();

	//fill in some internal holes
	for (int y = 1; y < (int)im.properties().height-1; y++)
		for (int x = 1; x < (int)im.properties().width-1; x++){
			if (im[y-1][x-1] && im[y+1][x-1] && im[y-1][x+1] && im[y+1][x+1])
				im[y][x] = true;
			else if (im[y-1][x] && im[y+1][x] && im[y][x+1] && im[y][x-1])
				im[y][x] = true;
		}

	if (find_holes)
		ns_find_holes(im,holes);

	//first we upsample the image 2x
	ns_image_properties up = im.properties();
	up.width*=2;
	up.height*=2;

	binary_image start;
	start.prepare_to_recieve_image(up);
	for (unsigned int y = 0; y < up.height; y++)
		for (unsigned int x = 0; x < up.width; x++)
			start[y][x] = im[y/2][x/2];
	
		
	//create a large edge bitmap from which we will calculate
	//edge points
	ns_image_bitmap large_edge_bitmap;
	ns_create_edge_bitmap(start,large_edge_bitmap);

	//now subsample the edge bitmap
	edge_bitmap.init(im.properties());
	ns_create_edge_bitmap(im,edge_bitmap);
	/*for (unsigned int y = 0; y < edge_bitmap.properties().height; y++)
		for (unsigned int x = 0; x < edge_bitmap.properties().width; x++)
			edge_bitmap[y][x] = large_edge_bitmap[2*y][2*x];*/
	

	//each pixel on the edge of the region will be assigned an id
	//used to link them together as edges.  We need to remember these,
	//as is done in the vertex_ids image.
	ns_image_standard_32_bit vertex_ids;
	vertex_ids.init(large_edge_bitmap.properties());
	//in vertex_ids, all ids are off by one (ie the real vertex id is vertex_ids[y][x]-1)
	//this frees up 0 to represent a point where no vertex exists.
	output_coordinates.reserve(up.width*up.height/8);
	for (unsigned int y = 0; y < vertex_ids.properties().height; y++){
		for (unsigned int x = 0; x < vertex_ids.properties().width; x++){
			if (large_edge_bitmap[y][x]){
				vertex_ids[y][x] = static_cast<ns_32_bit>(output_coordinates.size()+1);  //assign the new vertex's id to the correct (x,y) position in the vertex_ids bitmap
				output_coordinates.push_back(ns_vector_2d(((float)x)/2.0,((float)y)/2.0));  //remember that the coordinates came from 2x enlargment of original bitmap, so we divide by 2 to get the
																		  //original coordinates.
			}
			else vertex_ids[y][x] = 0;
		}
	}
	edges.reserve(output_coordinates.size());
	for (int y = 0; static_cast<unsigned int>(y) < vertex_ids.properties().height; y++){
		for (int x = 0; static_cast<unsigned int>(x) < vertex_ids.properties().width; x++){
			ns_32_bit & cur = vertex_ids[y][x];	
			if (cur == 0)
				continue;
			//look through neighbors
			for (int _y = -1; _y <= 1; _y++){
				for (int _x = -1; _x <= 1; _x++){
					if (_x==0 && _y==0) continue;  //ignore self
					ns_32_bit neighbor = vertex_ids.slow_safe_access(y+_y,x+_x);
					if (neighbor != 0)  //if a neighboring pixel exist, make an edge to it.
						edges.push_back(ns_edge_ui(cur-1,neighbor-1));  //-1 because values of the vertex_ids are offset by one, as explained above.
				}
			}
		}
	}
}


#ifndef NS_MINIMAL_SERVER_BUILD
void ns_remove_holes(ns_image_bitmap & temp);
#endif

template<class T,unsigned int c>
inline void ns_smooth_series(const std::vector<T> & in, std::vector<T> & out,const T & zero){
	if (c == 0){
		for (unsigned int i = 0; i < in.size(); i++)
			out[i] = in[i];
		return;
	}
	for (unsigned int i = 0; i < in.size(); i++){
		T sum(zero);
		int start = i-c;
		if (start < 0) start = 0;
		int stop = i+c;
		if (stop > (int)in.size()-1)
			stop = (int)in.size()-1;
		for (int j = start; j <= stop; j++)
			sum = sum + in[j];
		out[i] = sum/(stop - start +1);
	}
}

struct ns_curvature_stats{
	double average, 
		total, 
		variance, 
		maximum,
		displacement_average,
		displacement_total;
	unsigned int x_intercept_count;
};


template<int smoothing_radius>
void ns_calculate_curvature(const std::vector<ns_vector_2d> & segment, std::vector<double> & curv){
	
	if (segment.size() < 2)
		throw ns_ex("calculate_average_curvature::Attempting to calculate curvature of a segment with less than three elements.");
	std::vector<ns_vector_2d> s(segment.size());
	ns_smooth_series<ns_vector_2d,smoothing_radius>(segment,s, ns_vector_2d(0,0));

	std::vector<ns_vector_2d> d(segment.size()),
						 ds(segment.size()),
						 d2(segment.size()),
						 d2s(segment.size());

	for (unsigned int i = 1; i < d.size()-1; i++)
		d[i] = (s[i+1] - s[i-1])/2;
	d[0] = d[1];
	d[d.size()-1] = d[d.size()-2];

	ns_smooth_series<ns_vector_2d,smoothing_radius>(d,ds, ns_vector_2d(0,0));

	for (unsigned int i = 1; i < d2.size()-1; i++)
		d2[i] = (ds[i+1] - ds[i-1])/2;
	d2[0] = d2[1];
	d2[d2.size()-1] = d2[d2.size()-2];
	ns_smooth_series<ns_vector_2d,smoothing_radius>(d2,d2s, ns_vector_2d(0,0));
	
	curv.resize(d2s.size());
	for (unsigned int i = 0; i < d2s.size(); i++){
		if (ds[i] == ns_vector_2d(0,0))
			curv[i] = 0;
		
		else {
			double p = ds[i].squared();
			curv[i] = (ds[i].x*d2s[i].y-ds[i].y*d2s[i].x)/sqrt(p*p*p);
		}
	}
	/*std::vector<double> curvature_smoothed;
	ns_mean_smooth(curv,curvature_smoothed);
	ns_mean_smooth(curvature_smoothed,curv);*/

}

template<int smoothing_radius>
double ns_calculate_average_curvature(const std::vector<ns_vector_2d> & segment){
	std::vector<double> curvature;
	ns_calculate_curvature<smoothing_radius>(segment,curvature);
	double total = 0;
	for (unsigned int i = 0; i < curvature.size(); i++)
		total+=curvature[i];
	if (curvature.size() == 0)
		return 0;
	return total/curvature.size();
}

void ns_calculate_curvature_stats(const std::vector<float> & curvature, ns_curvature_stats & stats);

template<class ns_vector>
//two rectangles a and b
//a1 is top left of a, a2 is bottom right
bool ns_rectangle_intersect(const ns_vector &a1, const ns_vector &a2, const ns_vector & b1,const ns_vector & b2){
	if(a1.x < b1.x && a2.x < b1.x) return false;
	if(a1.x > b2.x && a2.x > b2.x) return false;
	if(a1.y < b1.y && a2.y < b1.y) return false;
	if(a1.y > b2.y && a2.y > b2.y) return false;
	return true;
}
#endif
