#ifndef NS_NODE_TOPOLOGY
#define NS_NODE_TOPOLOGY
#include "ns_vector.h"
#include <math.h>
#include <stack>
#include <float.h>
#include <algorithm>
#include <vector>
#include <fstream>
#include "ns_worm_detection_constants.h"
#include <limits>
#include "ns_image.h"
#include "ns_node_topology_primitives.h"


#define NS_USE_BSPLINE



//#define NS_USE_BSPLINE

#undef SPINE_DEBUG_OUTPUT
//#define SPINE_DEBUG_OUTPUT

///Each object in an image is represented by a Delauny triangular mesh.
///This mesh is then used to calculate a set of connected line segments that run down the anterior-posterior axis of the object.
///ns_spine takes an Delauny mesh and extracts the anterior-posterior axis.  Objects with complex shapes do not have a single axis,
///And so ns_spine creates a set of possible sub-spines that would correspond to multiple contiguous worms.
class ns_node_topology{
	friend class ns_segment_cluster;
public:

	///Given a triangular mesh, build set of possible spine subdivisions corresponding to one or more contiguous worms
	bool build_via_delauny_method(const std::vector<ns_vector_2d> & edge_coordinates, const std::vector<ns_edge_ui> & edge_list, const std::vector<ns_vector_2d> & holes, const std::string & debug_output_filename);
	bool build_via_zhang_thinning(const ns_image_bitmap & bitmap);

	~ns_node_topology();

private:

	///nodes are grouped into an articulated skeleton of ns_spine_segments connected by ns_spine_segment_ends joints.
	///segments holds the ns_spine_segement elements in no particular order.
	std::vector<ns_node_topology_segment *> segments;
	///nodes are grouped into an articulated skeleton of ns_spine_segments connected by ns_spine_segment_ends joints.
	///segments holds the spine joints in no particular order.
	std::vector<ns_node_topology_segment_end *> segment_ends;



	float _source_image_resolution;
	///All nodes in the ns_node_topology_node directed graph can be classified into islands (no neighbors), ends (only one neighbor), branchpoints (three neighbors).
	std::vector<ns_node_topology_node *> ends;
	///All nodes in the ns_node_topology_node directed graph can be classified into islands (no neighbors), ends (only one neighbor), branchpoints (three neighbors).
	std::vector<ns_node_topology_node *> islands;
	///All nodes in the ns_node_topology_node directed graph can be classified into islands (no neighbors), ends (only one neighbor), branchpoints (three neighbors).
	std::vector<ns_node_topology_node *> branchpoints;
	//All nodes in the network, used to allow quick deallocation of all node objects.
	std::vector<ns_node_topology_node *> nodes;

	//segments are collected into segment paths, ie collections of connected segments.
	//segments corresponding to worms are stored in worm_segment_paths.
	//std::vector<ns_segment_path *> worm_segment_paths;

	///spine building produces of possible multiple-worm-clump worm disambiguaotion solutions

	void dump_node_graph(std::ostream & out) const;

	//finds the position of the steps_to_take node of a segment, starting from the start_end end.
	ns_vector_2d find_downstream_coordinate(ns_node_topology_segment & segment, bool start_end, const unsigned int steps_to_take);

	//void find_possible_paths(const int number_of_worms,std::vector<ns_segment_path_possible_solution> & solutions);


	//finds segments who start and end in the same place (the same segment_end node)
	//and breaks the loop
	//if the loop is connected to another segment, the loop is broken such
	//that the angle of meeting between the broken loop and third segment is minimized
	void break_segment_loops();

	void generate_all_segment_paths();

	void look_for_duplicate_segment_nodes();

	void remove_duplicate_nodes(const bool move_nodes);

	void delete_nodes();
	//fills the ends, islands, and branchpoints arrays from raw mesh data.
	//deleted triangles are discarded in the new arrays.
	void populate_nodes_from_mesh(const std::vector<ns_triangle_d> & mesh, const std::vector<int> & neighborlist);

	void sort_node_neighbor_links();

	void classify_nodes();
	//should be reimplemented recursively,
	void remove_short_arms(const unsigned int min_node_length);

	//go through all the points and organize them into segments
	void generate_segments();

	//from a given end, generate (recursively) segments for all neighbors of the node it represents.
	void recurse_segments(ns_node_topology_segment_end *end, const unsigned int neighbor_to_take);

	void merge_segments();

	void remove_empty_segments();

	void remove_small_segments(const double percent_cutoff);

};

#endif
