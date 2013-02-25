#ifndef NS_COMPLEX_SEGMENT_CLUSTER_SOLVER
#define NS_COMPLEX_SEGMENT_CLUSTER_SOLVER
#include "ns_segment_topology.h"



///clusters of contiguous worms need to be broken up into individual worm spines.  There are many possible
///divisions of the ns_node_topology_segment network that would represent solutions to the contiguous-worm disambiguation problem.
///ns_segment_path_possible_solution represents one possible division of the network into individual worms
class ns_segment_path_possible_solution{
public:
	ns_segment_path_possible_solution():worms(1){}
	ns_segment_path_possible_solution(const unsigned int number_of_worms):worms(number_of_worms){}
	///each path in worms[] represents a distinct worm found in the ns_node_topology_segment network
	std::vector<ns_segment_cluster_solution> worms;
	///the number of segment paths in the current solution that meet length criteria
	unsigned int number_of_worms;
			///The cartesian length of all paths in the solution
	double total_length_of_worms,
			///The varience cartesian length between all paths in the solution
		   variance_in_worm_length,
		   ///the smallest segment path in the solution
		   min_worm_length,
		   ///the largest segment path in the solution
		   max_worm_length,
		   //
		   shared_segment_length,
		   shared_segment_total_length,
		   
		   min_length_of_solution_with_shared_segment;
	double length_of_total_coverage()const {return total_length_of_worms-shared_segment_total_length+shared_segment_length;}
	void output_debug_label(std::ostream & o,const bool newline) const;
};


class ns_naive_complex_segment_cluster_solver{
public:
	static void run(ns_segment_cluster & segment_cluster,
					std::vector<ns_segment_cluster_solution_group> & mutually_exclusive_solutions);
};

class ns_smart_complex_segment_cluster_solver{
public:
	void run(	const int number_of_worms,const double source_image_resolution,
				ns_segment_cluster & segment_cluster,
				std::vector<ns_segment_cluster_solution_group> & mutually_exclusive_solutions){

			find_possible_paths(number_of_worms,segment_cluster);
			choose_best_solution_for_each_worm_number(number_of_worms,mutually_exclusive_solutions,source_image_resolution);
	}

private:
	///given a set of connected spine segments and their endpoints, calculate all possible
	///interpretations of the segment group as multiple touching worms.
	void find_possible_paths(const int number_of_worms,
							 ns_segment_cluster & segment_cluster);

	void find_possible_paths_fast(const int number_of_worms,
							 ns_segment_cluster & segment_cluster);

	///When provided a list of all possible solutions to a set of segments,
	///choose_clump_solution_set reduces the list a small set of mutually exclusive solutions.
	///A set of N segments can represent at most N worms; choose_clump_solution_set produces
	///N mutually exclusive solutions each representing the best solution if the cluster is assumed to contan 1...N worms.
	void choose_best_solution_for_each_worm_number(const int max_number_of_worms, std::vector<ns_segment_cluster_solution_group> & solutions, const double source_image_resolution);
	
	///during multiple-worm cluster disambiguation, a variety worm clump solutions are created.  Possible solutions
	///are stored in worm_clump_solutions
	std::vector<ns_segment_path_possible_solution> possible_solutions;
};



#endif
