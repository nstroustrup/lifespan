#ifndef NS_SPINE_DRAWER_H
#define NS_SPINE_DRAWER_H

#include "ns_segment_topology.h"
#include "ns_image.h"
#include "ns_svg.h"

#define NS_DRAW_SPINE_NORMALS
#undef NS_DRAW_BITMAP_EDGES

//setting worm_spine_segment to zero will output all worm spine segments
class ns_spine_drawer{
public:
	static void draw_mesh(const std::vector<ns_triangle_d> & mesh, const ns_color_8 & color, const unsigned int resize_factor,ns_image_standard & output);

	void draw_normals(const ns_worm_shape & worm, ns_image_standard & output, const unsigned int resize_factor);

	void draw_normals(const ns_worm_shape & worm, ns_svg & svg);

	void draw_spine(const ns_image_standard & img, const ns_segment_cluster & seg, const ns_worm_shape & worm, ns_svg & output);

	void draw_spine(const ns_image_standard & img, const ns_segment_cluster & seg, const ns_worm_shape & worm, ns_image_standard & output, const unsigned int resize_factor);

	void draw_edges(ns_svg & svg,const std::vector<ns_vector_2d> & edge_coordinates);
};
#endif
