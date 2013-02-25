#ifndef NS_SEGMENT_TOPOLOGY
#define NS_SEGMENT_TOPOLOGY
#include "ns_node_topology.h"

#define NS_BSPLINE_SELECTION_DOWNSAMPLE_RATE 3
#define NS_BSPLINE_OUTPUT_DOWNSAMPLE_RATE 2
class ns_node{
public:
	ns_vector_2d position;
	ns_vector_2d tangent;
	ns_vector_2d normals[2];
	double width;
	double curvature;
};

class ns_segment;

class ns_segment_link{
public:
//	ns_segment_link(const ns_segment_link & seg):segment(seg.segment),start_neighbors(&seg.start_neighbors),end_neighbors(&seg.end_neighbors),polarity_(seg.polarity_){}
	ns_segment_link(ns_segment * seg,const bool pol);

	ns_segment * segment;
	inline std::vector<ns_segment_link> & start_neighbors(){return *start_neighbors_;}
	inline std::vector<ns_segment_link> & end_neighbors(){return *end_neighbors_;}

	
	inline const std::vector<ns_segment_link> & start_neighbors()const {return *start_neighbors_;}
	inline const std::vector<ns_segment_link> & end_neighbors()const {return *end_neighbors_;}

	//if true,the segment link is shared between multiple worm solutions
	bool is_shared;
	inline bool polarity()  const {return polarity_;}
	void merge(ns_segment_link & seg);

private:
	//0 is forward
	//1 is back
	bool polarity_;
	std::vector<ns_segment_link> * start_neighbors_;
	std::vector<ns_segment_link> * end_neighbors_;
};

class ns_segment{
public:
	std::vector<ns_node> nodes;
	std::vector<ns_segment_link> start_neighbors;
	std::vector<ns_segment_link> end_neighbors;

	ns_segment():number_of_times_shared(0),marked_for_deletion(false){
	//	init_token="SUPERDUPER5";
	//	init_token+="6";
	}
	/*void check_init(){
		std::string t = "SUPERDUPER5";
		t+= "6";
		if (init_token != t)
			throw ns_ex("UNINIT!");
	}
	std::string init_token;*/
	//used by complex segment solution algorithm
	unsigned long usage_count;
	bool can_be_shared;
	std::string debug_name;
	unsigned long number_of_times_shared;
	bool marked_for_deletion;

	inline float length(){return length_;}

	

	friend class ns_segment_cluster;
	friend class ns_segment_link;
	private:	
		float length_;
};

class ns_segment_cluster{
public:

	ns_segment_cluster(){}
	ns_segment_cluster(const ns_node_topology & node_topology){compile(node_topology);}
	void compile(const ns_node_topology & node_topology);
	std::vector<ns_segment *> segments;
	std::vector< ns_edge_2d> edges;

	void remove_short_segments(const double cutoff_length, const bool only_hanging=true);
	~ns_segment_cluster();
	void output_debug(std::ostream & out);
};

class ns_segment_cluster_solution{
	friend class ns_smart_complex_segment_cluster_solver;
	friend class ns_naive_complex_segment_cluster_solver;
	friend class ns_segment_cluster_solutions;
public:
	ns_segment_cluster_solution():length_(0){}
	ns_segment_cluster_solution(const std::vector<ns_segment_link> & seg):length_(0),segments(seg){}
	std::vector<ns_segment_link> segments;
	inline float length() const{return length_;}

private:
	//used during complex spline 
	float length_;
	mutable std::string label;
	void mark_path_as_used(const bool usage_count){
		for (unsigned int i = 0; i < segments.size(); i++){
			segments[i].segment->usage_count = usage_count;
			segments[i].segment->number_of_times_shared = usage_count;
		}
	}
	void increment_path_usage(){
		for (unsigned int i = 0; i < segments.size(); i++)
			segments[i].segment->usage_count++;
	}
	void generate_simple_path(std::vector< ns_node *> & simple_path);
};

typedef std::vector<ns_segment_cluster_solution> ns_segment_cluster_solution_group;

class ns_segment_cluster_solutions{
public:
	ns_segment_cluster_solutions(){}
	ns_segment_cluster_solutions(ns_segment_cluster & cluster,const float source_image_resolution, const bool do_multiple_worm_disambiguation){calculate(cluster,source_image_resolution,do_multiple_worm_disambiguation);}
	void calculate(ns_segment_cluster & cluster,const float source_image_resolution, const bool do_multiple_worm_disambiguation);
	std::vector<ns_segment_cluster_solution_group> mutually_exclusive_solution_groups;

};




class ns_worm_shape{
public:
	ns_worm_shape():length(0),/*center(0,0),*/spine_center(0,0){}
	std::vector<float> width,curvature;
	std::vector<ns_vector_2d> nodes;
	std::vector<ns_vector_2d> nodes_unsmoothed;
	std::vector<ns_vector_2d> normal_0;
	std::vector<ns_vector_2d> normal_1;

	std::vector<char> nodes_are_shared;
	std::vector<char> nodes_are_near_shared;

	mutable float length;
	//mutable ns_vector_2d center;
	mutable ns_vector_2d spine_center;
	mutable unsigned int spine_center_node_id;

	void from_segment_cluster_solution(const ns_segment_cluster_solution & solution);
	void finalize_worm_and_calculate_width(const std::vector<ns_edge_2d> & edges);

	void calculate_length() const;
	//find the angle of the center axis of the worm
	double angular_orientation() const;

	void write_to_buffer(float * buffer, const unsigned long node_offset);
	void read_from_buffer(const float * buffer, const unsigned long length, const unsigned int node_offset);

	void write_to_csv(std::ostream & out) const;
	void read_from_csv(std::istream & in, const unsigned long number_of_nodes);

	inline bool node_is_shared(const unsigned int & i) const {return nodes_are_shared[i]>0;}
	inline bool node_is_near_shared(const unsigned int & i) const {return nodes_are_near_shared[i]>0;}
	private:
};

bool ns_read_csv_value(std:: istream & i, std::string & str);
bool ns_read_csv_value(std:: istream & i, long & str);
bool ns_read_csv_value(std:: istream & i, int & str);
bool ns_read_csv_value(std:: istream & i, unsigned long & str);
bool ns_read_csv_value(std:: istream & i, double & str);
bool ns_read_csv_value(std:: istream & i,float & str);

void inline ns_extrude_normals_to_hull(const ns_vector_2d & pos, ns_vector_2d & normal_0, ns_vector_2d & normal_1, const std::vector<ns_edge_2d> & edges,long edge_start, long edge_end, bool only_consider_shorter_edges, bool zero_out_lost_segments);

#endif
