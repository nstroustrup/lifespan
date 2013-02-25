#ifndef NS_REGION_GROWER_SEGMENTER
#include "ns_image.h"
#include <map>

typedef enum { ns_horizontal, ns_vertical} ns_region_grower_edge_direction;
class ns_pixel_coord{
public:
	ns_pixel_coord(){};
	ns_pixel_coord(const unsigned long _x, const unsigned long _y, const ns_region_grower_edge_direction _edge_direction=ns_horizontal):x(_x),y(_y),edge_direction(_edge_direction){}
	unsigned long  x,y;
	//only used on edges
	ns_region_grower_edge_direction edge_direction;
};

typedef std::vector<ns_pixel_coord> ns_pixel_set;

class ns_region_pair{
public:
	ns_region_pair (const unsigned int a, const unsigned int b){
		if (a > b){
			first = a;
			second = b;
		}
		else{
			first = b;
			second = a;
		}
	}

private:
	unsigned int first,second;
};

struct ns_region_grower_edge{
	ns_region_grower_edge():number_of_weak_pixels(0){}
	std::vector<ns_pixel_set *> pixels;
	unsigned long number_of_weak_pixels;
	unsigned int length(){
		unsigned int len = 0;
		for (unsigned int i = 0; i < pixels.size(); i++)
			len+=(unsigned long)pixels[i]->size();
		return len;
	}
	void calc_weak_pixels(const ns_image_standard & image, const unsigned int weak_edge_threshold){
		number_of_weak_pixels = 0;
		for(unsigned int i = 0; i < pixels.size(); i++){
			for (unsigned int j = 0; j < pixels[i]->size(); j++){
				int pix1 = image[((*pixels[i])[j]).y][(*(pixels[i]))[j].x],
					pix2;

				if ( (*(pixels[i]))[j].edge_direction == ns_vertical )
					pix2 = image[(*(pixels[i]))[j].y][(*(pixels[i]))[j].x-1];
				else 
					pix2 = image[(*(pixels[i]))[j].y-1][(*(pixels[i]))[j].x];
				unsigned int pix_strength = abs(pix1 - pix2);
			//	cerr << pix_strength << ",";
				number_of_weak_pixels += (unsigned int)(pix_strength < weak_edge_threshold);
			}
		}
	//	cerr << number_of_weak_pixels  << "\n";
	}

};

//(region id, edge information)
typedef std::map<unsigned int, ns_region_grower_edge> ns_region_edges_list;

class ns_region{
public:
	ns_region():outside_image_edge_perimeter(0),deleted(false){}
	std::vector <ns_pixel_set *> pixels;
	ns_region_edges_list edges;
	unsigned int id;

	bool deleted;

 	void merge(ns_region & reg, std::vector<ns_region> & all_regions){
		if (this->edges.find(reg.id) == this->edges.end())
			throw ns_ex("Merger does not border with mergee");
		if (reg.edges.find(this->id) == reg.edges.end())
			throw ns_ex("Mergee does not border with merger");

		//update perimeter length
		perimeter_length += reg.perimeter_length - this->edges[reg.id].length();
		area += reg.area;
		total_intensity += reg.total_intensity;
		//remove common edge from merger's and mergee's list.
		reg.edges.erase(id);
		this->edges.erase(reg.id);

		//update adjacencies	
		for (ns_region_edges_list::iterator p = reg.edges.begin(); p!= reg.edges.end(); p++){
			if (p->first == id)
				continue;
			
			//merge mergee's and the merger's edges.
			this->edges[p->first].pixels.insert(this->edges[p->first].pixels.end(),p->second.pixels.begin(),p->second.pixels.end());
			this->edges[p->first].number_of_weak_pixels+=p->second.number_of_weak_pixels;

			//notify the mergee's neighbor that they now border the merge, not the mergee
			all_regions[p->first].edges.erase(reg.id);  //delete mergee's record in mergee's neighbors edge list

			all_regions[p->first].edges[this->id].pixels.insert(all_regions[p->first].edges[this->id].pixels.end(), p->second.pixels.begin(),p->second.pixels.end());
		}
		//take all mergee's pixels.
		this->pixels.insert(this->pixels.begin(),reg.pixels.begin(), reg.pixels.end());
		reg.deleted = true;
		
	}
	unsigned int perimeter_length;
	unsigned long area;
	unsigned long total_intensity;
	void calculate_perimeter_length(){
		perimeter_length = outside_image_edge_perimeter;

		for (unsigned int i = 0; i < edges.size(); i++){
			for (unsigned int j = 0; j < edges[i].pixels.size(); j++)
				perimeter_length += (unsigned int)edges[i].pixels[j]->size();
		}
	}

	//the number of pixels in the region that border the outside of the image
	unsigned int outside_image_edge_perimeter;

	bool merged; //whether or not the region was merged in the last round
};

void ns_region_growing_segmenter(const ns_image_standard & input, ns_image_standard & output, const unsigned int weak_edge_threshold, const double common_edge_threshold1, const double common_edge_threshold2);

#endif