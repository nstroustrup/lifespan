#ifndef NS_SPINE_PRIMITIVES
#define NS_SPINE_PRIMITIVES
#include "ns_worm_detection_constants.h"
#include <limits>
#include "ns_image.h"
#include <set>


#define ns_spine_search_distance_epsilon .001


///Each node along the anterior-posterior axis is generated from the circumcenter of a triange within a triangular mesh
///As such, it can have three neighbors corresponding to the circumcenters of neighboring triangles.
///Nodes are chained together in a directed graph, ie n1->n2->n3->n4
//Directionality is specified by a neighbor's position in the ns_node_topology_node::neighbors array
//neighbors[ns_previous] is the neighbor lying closest to the start of the spine
//neighbors[ns_next] is the neighbor lying closest to the end of the spine
//neighbors[ns_branch] represents a third neighbor that connects another spine branch to the current branch
//If a node does not have one of these neighbors, neighbors[i] is set to zero.
//This means that spine junctions can be found by searching for nodes with defined neighbors[ns_branch] != 0


///A node along the anterior-posterior axis spine of an object
struct ns_node_topology_node{
	ns_node_topology_node():polarity_fixed(false),is_a_segment_pivot(false){neighbors[0]=0;neighbors[1]=0;neighbors[2]=0;}
	
	typedef enum{ns_previous, ns_next,  ns_branch, ns_no_neighbor} ns_neighbor_name;
	///Each node along the anterior-posterior axis is generated from the circumcenter of a triange within a triangular mesh
	///As such, it can have three neighbors corresponding to the circumcenters of neighboring triangles.
	///Nodes are chained together in a directed graph, ie n1->n2->n3->n4
	//Directionality is specified by a neighbor's position in the ns_node_topology_node::neighbors array
	//neighbors[ns_previous] is the neighbor lying closest to the start of the spine
	//neighbors[ns_next] is the neighbor lying closest to the end of the spine
	//neighbors[ns_branch] represents a third neighbor that connects another spine branch to the current branch
	//If a node does not have one of these neighbors, neighbors[i] is set to zero.
	//This means that spine junctions can be found by searching for nodes with defined neighbors[ns_branch] != 0
	ns_node_topology_node * neighbors[3];

	///returns the specified neighbor (0,1,2)  of the spine node
	inline ns_node_topology_node * & operator()(const ns_neighbor_name & n){
		return neighbors[n];
	}	
	///returns the specified neighbor (0,1,2)  of the spine node
	inline const ns_node_topology_node * operator()(const ns_neighbor_name & n) const{
		return neighbors[n];
	}

	///the cartesian coordinates of the node.
	ns_vector_2d coord;		
	double width;

	///Vectors normal to the spine in both directions.  Calculated by calculating the 
	///the line perpendicular to the spine and finding its intersection with the hull of 
	ns_vector_2d normal_vectors[2]; 

	///if true, a branchpoint has been finalized and
	///doesn't need to be reshuffled
	bool polarity_fixed;

	///asks that, from the perspective of neighbor[arm_direction], what direction is the current node?
	///Is the current node it's neighbor's ns_previous neighbor, ns_next_neighbor, or ns_branch?
	ns_neighbor_name direction_polarity(ns_neighbor_name & arm_direction){
		if (neighbors[arm_direction] == 0)								return ns_no_neighbor;
		if (neighbors[arm_direction]->neighbors[ns_previous] == this)	return ns_next;
		if (neighbors[arm_direction]->neighbors[ns_next] == this)		return ns_previous;
		if (neighbors[arm_direction]->neighbors[ns_branch] == this)		return ns_branch;
		throw ns_ex("direction_polarity()::Invalid arm specified");
	}
	
	///A flag set as true during spine topology analysis, representing an endpoing of a node segment
	bool is_a_segment_pivot;
};

struct ns_node_topology_segment;

///nodes are grouped into segments, the end of which are nodes that either 1) only have one neighbor or 2) are stationed at a branchpoint.
///ns_node_topology_segment_end contains references to any segment that has the specified node as one of its endpoints.
struct ns_node_topology_segment_end{
	ns_node_topology_segment_end():node(0){neighbor[0] = 0;neighbor[1] = 0;neighbor[2] = 0;}

	///at most three segments can be attached to a single segment endpoint
	ns_node_topology_segment * neighbor[3];

	///if true, the neighbor is attached at its end[0] end.
	///if false, the neighbor is attached at its end[1] end.
	bool neighbor_attachment_end[3];
	ns_node_topology_node * node;

};

///Nodes are grouped into segments, the end of which are nodes that either 1) only have one neighbor or 2) are stationed at a branchpoint.
///Whereas ns_nodes depict a directed graph of nodes, ns_spine_segments represent the same data as an "articulated skeleton" with ns_node_topology_segment_end 
///objects at joints
///Network topology is much easier to analyze using an articulated skeleton rather than having to directly traverse the underlying ns_node network
///Each segment can have two neighboring segments attached at each end. 
struct ns_node_topology_segment{
	ns_node_topology_segment():length(0){end[0] = 0; end[1] = 0; for (int i = 0; i < 4; i++) neighbor[i] = 0;}
	///All nodes within the segment (not including the endpoints)
	std::vector<ns_node_topology_node * > nodes;
	///The total length of the spine in cartesian distance (ie the sum of the distance between all nodes including the endpoints)
	double length;
	///The two endpoints of the spine
	ns_node_topology_segment_end * end[2];
	///Segments can form a loop where two ends connect to each other.  Returns true if this is the case
	bool is_a_loop(){
		if (end[0] == 0 || end[1] == 0) return false;
		return (end[0]->node == end[1]->node);
	}

	///Each segment can have a total of four neighbors,
	///two on one end and two on the other.
	ns_node_topology_segment * neighbor[4];

	///Which end of neighboring segment neighbor[i], is attached to the shared ns_node_topology_segment end? true == end[1], false == end[0]
	bool neighbor_attachment_end[4];

	//given segment end points, fill in the neighbor[] and neighbor_attachment_endp[] structures
	void build_neighbor_list();


	//#define SPINE_DEBUG_OUTPUT
	///Merges the specified neighbor to the current segment.
	///If another segment exists at the same branchpoint as the segment and that being merged, it is orphaned (ie the shared end-point is assigned
	///to both segments but no longer acts as a joint in the articulated ns_node_topology_segment skeleton
	void merge_neighbor(const unsigned int neighbor_id);

	//the number of times a given segment is used in any ns_segment_path.
	//Used as temporary storage in the worm clump disambiguation algorithm
	unsigned int usage_count;  

	
	double max_downstream_length(const bool follow_end0);

		
};
#endif
