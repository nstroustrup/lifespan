#include "ns_complex_segment_cluster_solver.h"
#include "ns_vector_bitmap_interface.h"
#include <stack>

#ifdef NS_USE_BSPLINE
#include "ns_bspline.h"
#endif

using namespace std;

#define ALLOW_SHARED_SEGMENTS

#define ALLOW_ALL_SPINE_PERMUTATIONS
//#undef ALLOW_ALL_SPINE_PERMUTATIONS

//#define  NS_DISPLAY_SOLUTION_COMPARRISONS

///clusters of contiguous worms need to be broken up into individual worm spines.  ns_segment_path_search_branch_point is an data structure
///used in the ns_node_topology_segment network search algorithm looking for distinct solutions to the multiple-worm-disambiguation problem.
class ns_segment_path_search_branch_point{
public:
	//create a new branch point from scratch, starting off at the specified point
	ns_segment_path_search_branch_point(const unsigned int number_of_worms,const unsigned int cur_worm_id,const ns_segment_link & first_step,const bool do_not_add_base_)
		:current_solution(number_of_worms),current_worm_id(cur_worm_id){
		current_solution.worms[current_worm_id].segments.push_back(first_step);
		do_not_add_base = do_not_add_base_;
	}
	//branch off a new branch point from the specified solution starting at the specified point.
	ns_segment_path_search_branch_point(const ns_segment_path_possible_solution & cur_solution, const unsigned int cur_worm_id,const ns_segment_link & next_step,const bool do_not_add_base_)
		:current_solution(0),current_worm_id(cur_worm_id){
		current_solution.worms.insert(current_solution.worms.begin(),cur_solution.worms.begin(),cur_solution.worms.end());
		current_solution.worms[cur_worm_id].segments.push_back(next_step);
		do_not_add_base = do_not_add_base_;
	}
	unsigned int current_worm_id;
	ns_segment_path_possible_solution current_solution;
	bool do_not_add_base;
};


#define ns_spine_arm 0
#define ns_spine_loop 1
#define ns_spine_handle 2

typedef vector<vector<ns_node_topology_node *> >ns_spine_solution_group;

struct ns_segment_path_possible_solution_group{
	//solutions[1..N] lists each possible solution set
	//solutions[i][1..M] lists each worm in each solution set
	//solutions[i][j][1..O] lists each node of each worm in each solution set
	vector<vector<vector< ns_node *> > > solutions;
	vector<ns_segment_path_possible_solution *> solution_source;
	double min_total_curvature;
	int winning_solution_id;
};

void ns_segment_path_possible_solution::output_debug_label(ostream & o, const bool newline = true) const{
		for (unsigned int i = 0; i < worms.size(); i++){
			if ( worms[i].segments.size() == 0) continue;
			for (unsigned int j = 0; j < worms[i].segments.size(); j++)
				o << worms[i].segments[j].segment->debug_name;
			o << "; ";
		}
		if (newline) o << "\n";
	}


//go through the worm segment paths and create a simple, ordered list of nodes along the path.
void ns_segment_cluster_solution::generate_simple_path(vector< ns_node *> & simple_path){
	//the first node of one segment of the path is identical to the last node of the previous segment; we remember to skip them here.
	simple_path.resize(0);
	if (segments.size() == 0) return;
	simple_path.reserve(segments.size()*10);
	for (unsigned int i = 0; i < segments.size(); i++){
	
		if (!segments[i].polarity())
			for (unsigned int j = 0; j < segments[i].segment->nodes.size()-1; j++)
				simple_path.push_back(&segments[i].segment->nodes[j]);
		else 
			for (int j = (int)segments[i].segment->nodes.size()-1; j > 0 ; j--)
				simple_path.push_back(&segments[i].segment->nodes[j]);
	}
	ns_segment_link & last_segment = segments[segments.size()-1];
	if (!last_segment.polarity())
		simple_path.push_back(&last_segment.segment->nodes[last_segment.segment->nodes.size()-1]);
	else simple_path.push_back(&last_segment.segment->nodes[0]);
}


struct ns_segment_path_solution_fast{
	ns_segment_path_solution_fast(const unsigned long & max_length){segments.reserve(max_length);}
	ns_segment_path_solution_fast(const unsigned long & max_length, const vector<vector<unsigned char> > & seg){
		segments.reserve(max_length); 
		segments.insert(segments.begin(),seg.begin(),seg.end());
	}
	//unsigned char segments[16][16];
	vector<vector<unsigned char> > segments;  //segments[i]::worm i
											  //segments[i][j]::segment j of worm i
};

struct ns_segment_path_search_branch_point_fast{
	ns_segment_path_search_branch_point_fast(const unsigned long & max_solution_length):solution(max_solution_length){}
	ns_segment_path_search_branch_point_fast(const unsigned int cur_w_id,const ns_segment_path_solution_fast & sol):current_worm_id(cur_w_id),solution(sol){}
	unsigned char current_worm_id;
	ns_segment_path_solution_fast solution;
	inline unsigned char end() const {return solution.segments[current_worm_id].back()>>1;}
	inline unsigned char end_polarity() const {return solution.segments[current_worm_id].back() & 1;}
};

class ns_segment_path_possible_solution_set_sorter{
public:
	bool operator()(const ns_segment_cluster_solution * l, const ns_segment_cluster_solution * r){
		//shorter solutions < than longer ones
		if (l->segments.size() != r->segments.size())
			return l->segments.size() > r->segments.size();
		//sort by "alphabetical" order, using the segment pointer location as each letter.
		for (unsigned int i = 0; i < l->segments.size(); i++)
			if (l->segments[i].segment != r->segments[i].segment)
				return l->segments[i].segment > r->segments[i].segment;

		return false;
	}
};

class ns_segment_path_possible_solution_hash{
public:
	ns_segment_path_possible_solution_hash(){}
	ns_segment_path_possible_solution_hash(const ns_segment_path_possible_solution & sol, std::vector<const ns_segment_cluster_solution * > & sorted_solutions_temp){
		unsigned int pos(0);
		sorted_solutions_temp.resize(sol.worms.size());
		for (unsigned int i = 0; i < sol.worms.size(); i++)
			sorted_solutions_temp[i] = &sol.worms[i];
		//cerr << "*";
		//sol.output_debug_label(cerr);

		std::sort(sorted_solutions_temp.begin(),sorted_solutions_temp.end(),ns_segment_path_possible_solution_set_sorter());
		/*ns_segment_path_possible_solution p;
		p.worms.resize(solutions.size());
		for (unsigned int i = 0; i < solutions.size(); i++)
			p.worms[i] = *solutions[i];
		cerr << "!";
		p.output_debug_label(cerr);
*/

		for (unsigned int i = 0; i < sorted_solutions_temp.size(); i++){
			if (sorted_solutions_temp[i]->segments.size() == 0) {path[pos] = 0; pos++; continue;}
			for (unsigned int j = 0; j < sorted_solutions_temp[i]->segments.size(); j++)
				path[pos+j] = sorted_solutions_temp[i]->segments[j].segment;
			pos+= (unsigned int)sorted_solutions_temp[i]->segments.size() + 1;
			path[pos-1] = 0;
		}
		path[pos] = 0;
	}
	void out_debug(ostream & o) const{
		for (unsigned int i = 0; i < 511; i++){
			if (path[i] == 0){
				if(path[i+1] == 0)
					return;
				o << ";";
			}
			else 
				o << path[i]->debug_name;
		}
	}
	ns_segment * path[512];
};

ostream & operator << (ostream & o, const ns_segment_path_possible_solution_hash & h){
	h.out_debug(o);
	return o;
}
/*
bool operator<(const ns_segment_path_possible_solution_hash & l, const ns_segment_path_possible_solution_hash & r){
	for (unsigned int i = 0; i < 511; i++){
		if (l.path[i] != r.path[i]){
			return l.path[i] < r.path[i];
		}
		if (l.path[i]== 0){
			if (l.path[i+1] == 0 || r.path[i+1] == 0)  //we've reached the end of one of the solutions
				return l.path[i+1] < r.path[i+1];  //if the other h hasn't ended, this one is shorter.
		}
	}
	return false;
}
*/
//#define NS_DISPLAY_SOLUTION_COMPARRISONS
#ifdef NS_DISPLAY_SOLUTION_COMPARRISONS
ofstream cmp_tst("c:\\tt\\cmp_tst.txt");
bool operator<(const ns_segment_path_possible_solution_hash & l, const ns_segment_path_possible_solution_hash & r){
	char cur_worm_vis_l[512],
		 cur_worm_vis_r[512];
	unsigned int cur_s_l(0),cur_s_r(0);
	unsigned int len_l(0),len_r(0);
	cmp_tst << "Comparing " << l <<  " and " << r << ":\n";

	for (unsigned int i = 0; i < 511; i++){
	
		if (l.path[i] != 0) cur_worm_vis_l[len_l] = l.path[i]->debug_name[0];
		else cur_worm_vis_l[len_l] = 0;
		if (r.path[i] != 0) cur_worm_vis_r[len_r] = r.path[i]->debug_name[0];
		else cur_worm_vis_r[len_r] = 0;

		if (l.path[i] != 0) len_l++;
		if (r.path[i] != 0) len_r++;
		if (len_l != len_r){
			if (len_l < len_r)
			cmp_tst << "*\tWorm end: " << len_l << " < " << len_r << " @ " << i << "\n";
			else 
			cmp_tst << "*\tWorm end: " << len_l << " > " << len_r << " @ " << i << "\n";
			return len_l < len_r; //if the current paths are different lengths, we're done.
		}
		if (l.path[i] == 0 || r.path[i] == 0){  //we've finished the current worm
			
			bool end_of_solution_reached(false);
			if (l.path[i+1] == 0 || r.path[i+1] == 0){  //we've reached the end of one of the solutions
				//if we reached the end of one of the solutiosn before the other, they aren't the same length
				if (l.path[i+1] != r.path[i+1]){
					if (l.path[i+1] < r.path[i+1])
						cmp_tst << "*\tSolution end: " << len_l << " < " << len_r << " @ " << i << "\n";
					else 
						cmp_tst << "*\tSolution end: " << len_l << " > " << len_r << " @ " << i << "\n";
					return l.path[i+1] < r.path[i+1];  //if the other h hasn't ended, this one is shorter
				}
				//if both solutions are the same length, we test to see if their last worm is the same, and then stop
				end_of_solution_reached = true;
			}

			for (unsigned int j = 0; j < len_l; j++){
				cur_worm_vis_l[j] = l.path[cur_s_l+j]->debug_name[0];
				cur_worm_vis_r[j] = r.path[cur_s_r + j]->debug_name[0];
			}
			cur_worm_vis_l[len_l] = 0;
			cur_worm_vis_r[len_l] = 0;


			int forward_comp = 0;
			for (unsigned int j = 0; j < len_l; j++){
				if (l.path[cur_s_l+j] > r.path[cur_s_r + j]){forward_comp =  1;break;}
				if (l.path[cur_s_l+j] < r.path[cur_s_r + j])   {forward_comp = -1;break;}
			}
			int reverse_comp = 0;
			if (forward_comp != 0){
				for (unsigned int j = 0; j < len_l; j++){
					if (l.path[cur_s_l+j] > r.path[cur_s_r+len_l-j-1]){reverse_comp =  1;break;}
					if (l.path[cur_s_l+j] < r.path[cur_s_r+len_l-j-1])   {reverse_comp = -1;break;}
				}
			}
			if (forward_comp == 0 || reverse_comp == 0){ //either the forward or the reverse matched.

				bool end_of_solution_reached(false);
				if (l.path[i+1] == 0 || r.path[i+1] == 0){  //we've reached the end of one of the solutions
					//if we reached the end of one of the solutiosn before the other, they aren't the same length
					if (l.path[i+1] != r.path[i+1])
						return l.path[i+1] < r.path[i+1];  //if the other h hasn't ended, this one is shorter
					//if both solutions are the same length, we test to see if their last worm is the same, and then stop
					end_of_solution_reached = true;
				}

				cmp_tst << "\t" << cur_worm_vis_l << " == " << cur_worm_vis_r;
				if (forward_comp == 0) cmp_tst << "(f)";
				else cmp_tst << "(r)";
				cmp_tst << "\n";
			
				len_l = 0;
				len_r = 0;
				cur_s_l = i+1;
				cur_s_r = i+1;
				if (end_of_solution_reached) 
					return false;  //we're at the end of the solution and no differences were found
			}
			else{
				cmp_tst << "*\t" <<cur_worm_vis_l << " != " << cur_worm_vis_r;
				cmp_tst << "\n";
				return forward_comp == -1;
			}
		}
	}
	throw ns_ex("ns_segment_path_possible_solution_hash::Comparrison Overflow");
}
#else
inline bool operator<(const ns_segment_path_possible_solution_hash & l, const ns_segment_path_possible_solution_hash & r){
	char cur_worm_vis_l[512],
		 cur_worm_vis_r[512];
	unsigned int cur_s_l(0),cur_s_r(0);
	unsigned int len_l(0),len_r(0);
	
	
	for (unsigned int i = 0; i < 511; i++){
		char l_name('?'),
			 r_name('?');
		if (l.path[i] != 0 && l.path[i]->debug_name.size() > 0)
			l_name = l.path[i]->debug_name[0];
		if (r.path[i] != 0 && r.path[i]->debug_name.size() > 0)
			r_name = r.path[i]->debug_name[0];

		if (l.path[i] != 0) 
			cur_worm_vis_l[len_l] = l_name;
		else cur_worm_vis_l[len_l] = 0;
		if (r.path[i] != 0) 
			cur_worm_vis_r[len_r] = r_name;
		else cur_worm_vis_r[len_r] = 0;

		if (l.path[i] != 0) len_l++;
		if (r.path[i] != 0) len_r++;
		if (len_l != len_r)
			return len_l < len_r; //if the current paths are different lengths, we're done.

		if (l.path[i] == 0 || r.path[i] == 0){  //we've finished the current worm
			
			bool end_of_solution_reached(false);
			if (l.path[i+1] == 0 || r.path[i+1] == 0){  //we've reached the end of one of the solutions
				//if we reached the end of one of the solutiosn before the other, they aren't the same length
				if (l.path[i+1] != r.path[i+1])
					return l.path[i+1] < r.path[i+1];  //if the other h hasn't ended, this one is shorter
				//if both solutions are the same length, we test to see if their last worm is the same, and then stop
				end_of_solution_reached = true;
			}

				for (unsigned int j = 0; j < len_l; j++){
					cur_worm_vis_l[j] = l_name;
					cur_worm_vis_r[j] = r_name;
				}
				cur_worm_vis_l[len_l] = 0;
				cur_worm_vis_r[len_l] = 0;
			

			int forward_comp = 0;
			for (unsigned int j = 0; j < len_l; j++){
				if (l.path[cur_s_l+j] > r.path[cur_s_r + j]){forward_comp =  1;break;}
				if (l.path[cur_s_l+j] < r.path[cur_s_r + j])   {forward_comp = -1;break;}
			}
			int reverse_comp = 0;
			if (forward_comp != 0){
				for (unsigned int j = 0; j < len_l; j++){
					if (l.path[cur_s_l+j] > r.path[cur_s_r+len_l-j-1]){reverse_comp =  1;break;}
					if (l.path[cur_s_l+j] < r.path[cur_s_r+len_l-j-1])   {reverse_comp = -1;break;}
				}
			}
			if (forward_comp == 0 || reverse_comp == 0){ //either the forward or the reverse matched.
				
				//we need to check for loops.
				//Generally, we ignore the polarity of segments because such polarity can be deduced by 
				//the identity of the segment's neighbors.  However, with loops, a change in polarity
				//is masked because both ends of the loop are connected to the same neighbor in both cases.
				//Thus, we identify loops and let both forward and back
				for (unsigned int j = 0; j < len_l; j++){
					if (l.path[cur_s_l+j]->nodes[0].position == l.path[cur_s_l+j]->nodes[l.path[cur_s_l+j]->nodes.size()-1].position)
						return &l < &r;  //we want to let both solutions through, that is to mark them as not equal,
										//but we still need be consistant about which of the "identical" solutions we mark as less
										//otherwise the algorithm will find that l < r and l > r and panic.  Thus we use the
										//pointer locations of each solution as a consistant comparrison.
				}

				len_l = 0;
				len_r = 0;
				cur_s_l = i+1;
				cur_s_r = i+1;
				if (end_of_solution_reached)
					return false;  //we're at the end of the solution and no differences were found
			}
			else
				return forward_comp == -1;
		}
	}
	throw ns_ex("ns_segment_path_possible_solution_hash::Comparrison overflow");
}
#endif



bool ns_can_be_shared(const ns_segment_link & s){
	unsigned complex_neighbors = 0;
	if (s.start_neighbors().size() == 0 || s.end_neighbors().size() == 0)
		return true;
	for (unsigned int i = 0; i < s.start_neighbors().size(); i++)
		complex_neighbors += (s.start_neighbors()[i].end_neighbors().size() > 0);
	for (unsigned int i = 0; i < s.end_neighbors().size(); i++)
		complex_neighbors += (s.end_neighbors()[i].end_neighbors().size() > 0);
	if (complex_neighbors > 0)
		return false;
	if (s.segment->nodes[0].position == s.segment->nodes[s.segment->nodes.size()-1].position)  //loops can't be shared
		return false;
	return true;
}

//depth first search for loops
bool ns_cluster_contains_a_loop(ns_segment_cluster & segment_cluster){
	if (segment_cluster.segments.size() == 0)
		return false;
	//refresh path map for current solution
	for (unsigned int i = 0; i < segment_cluster.segments.size(); i++)
		segment_cluster.segments[i]->usage_count=0;
	
	std::stack<ns_segment_path_search_branch_point> branch_points;
	branch_points.push(ns_segment_path_search_branch_point(1,0,ns_segment_link(segment_cluster.segments[0],false),true));
	branch_points.push(ns_segment_path_search_branch_point(1,0,ns_segment_link(segment_cluster.segments[0],true),false));

	while(!branch_points.empty()){
		ns_segment_path_search_branch_point bp = branch_points.top();
		branch_points.pop();
		
		ns_segment_cluster_solution & cur_path = bp.current_solution.worms[0];
		ns_segment_link & cur_segment = cur_path.segments[cur_path.segments.size()-1];
		for (unsigned int i = 0; i < cur_segment.end_neighbors().size(); i++){
			if (cur_segment.end_neighbors()[i].segment->usage_count != 0)
				return true;
			cur_segment.end_neighbors()[i].segment->usage_count++;
			branch_points.push(ns_segment_path_search_branch_point(bp.current_solution,bp.current_worm_id,cur_segment.end_neighbors()[i],false));
		}
	}
	return false;
}

#ifdef NS_DISPLAY_SOLUTION_COMPARRISONS
long worm_id=0;
ofstream mmake("c:\\tt\\make2.bat");
#endif
void ns_smart_complex_segment_cluster_solver::find_possible_paths(const int number_of_worms,
								 ns_segment_cluster & segment_cluster){

	#ifdef ALLOW_SHARED_SEGMENTS
	//if the graph has a loop, don't share segments (because in this case the combinations to be tested explode)
	if (0&&ns_cluster_contains_a_loop(segment_cluster)){
	//	cerr << "Found a loop\n";
		for (unsigned int i = 0; i < segment_cluster.segments.size(); i++)
			segment_cluster.segments[i]->can_be_shared = false;
	}
	else{
	//	cerr << "No Loop\n";
		for (unsigned int i = 0; i < segment_cluster.segments.size(); i++)
			segment_cluster.segments[i]->can_be_shared = ns_can_be_shared(ns_segment_link(segment_cluster.segments[i],false));
	}
	#endif
	std::stack<ns_segment_path_search_branch_point> branch_points;


	//start a worm tree at each segment end in the graph
	for (unsigned int i = 0; i < segment_cluster.segments.size(); i++){
		branch_points.push(ns_segment_path_search_branch_point(number_of_worms,0,ns_segment_link(segment_cluster.segments[i],false),true));
		branch_points.push(ns_segment_path_search_branch_point(number_of_worms,0,ns_segment_link(segment_cluster.segments[i],true),false));
	}
	set<ns_segment_path_possible_solution_hash> recorded_paths;

	
	//ofstream o("c:\\tt\\dbg.txt");
	//if (o.fail())
	//	throw ns_ex("COuld not open f");	
		


	#ifdef NS_DISPLAY_SOLUTION_COMPARRISONS
		string filename = "c:\\tt\\object_";
		filename+=ns_to_string(worm_id);
		filename += "_dbg_lbs_";
		mmake << "c:\\work\\graphviz\\bin\\neato.exe -Tpng " << filename << ".txt > " << filename << ".png\n";
		filename+=".txt";
		ofstream o2(filename.c_str());
		segment_cluster.output_debug(o2);
		o2 << "}\n";
		o2.close();
	#endif



	//o2.open("c:\\tt\\adding.txt");
	vector<const ns_segment_cluster_solution * > sorted_solutions_temp;
	while(!branch_points.empty()){
		ns_segment_path_search_branch_point bp = branch_points.top();
		branch_points.pop();
	

		//refresh path map for current solution
		for (unsigned int i = 0; i < segment_cluster.segments.size(); i++)
			segment_cluster.segments[i]->usage_count=0;

		for (unsigned int i = 0; i < bp.current_solution.worms.size(); i++)
			bp.current_solution.worms[i].increment_path_usage();

		//set up some variables
		ns_segment_cluster_solution & cur_path = bp.current_solution.worms[bp.current_worm_id];
		if (cur_path.segments.size() == 0)
			throw ns_ex("ns_spine::Empty segment path search branch point path encountered!");
		ns_segment_link & cur_segment = cur_path.segments[cur_path.segments.size()-1];
		
		bp.current_solution.worms[bp.current_worm_id].length_+=cur_segment.segment->length();
		//create branch points that continue to explore the graph with the current worm
		for (unsigned int i = 0; i < cur_segment.end_neighbors().size(); i++){
			//cur_segment.end_neighbors()[i].segment->check_init();
			//char tmp = 0;
			bool a = cur_segment.end_neighbors()[i].segment->usage_count ==0;
			//if (a)
		//		tmp++;
			bool b1 = cur_segment.end_neighbors()[i].segment->usage_count < (unsigned long)number_of_worms;
			//if (b1) tmp+=2;
			bool b2 = cur_segment.end_neighbors()[i].segment->can_be_shared;
			//if (b2) tmp+=3;
			//if (tmp==7) cerr << "WOW\n";
			if (a
				#ifdef ALLOW_SHARED_SEGMENTS
				||(b1&& b2)
				#endif
				)
				branch_points.push(ns_segment_path_search_branch_point(bp.current_solution,bp.current_worm_id,cur_segment.end_neighbors()[i],false));
		}
		
		//consider the current worm as finished, and create branch points that start exploration of the graph with the next worm
		if ((int)bp.current_worm_id < number_of_worms-1){
			for (unsigned int i = 0; i < segment_cluster.segments.size(); i++){
				if (segment_cluster.segments[i]->usage_count == 0){ //stop at segments that have already been calculated 
					branch_points.push(ns_segment_path_search_branch_point(bp.current_solution,bp.current_worm_id+1,ns_segment_link(segment_cluster.segments[i],false),true));
					branch_points.push(ns_segment_path_search_branch_point(bp.current_solution,bp.current_worm_id+1,ns_segment_link(segment_cluster.segments[i],true),false));
				}
			}
		}
		if (!bp.do_not_add_base){
			#ifdef NS_DISPLAY_SOLUTION_COMPARRISONS
			cmp_tst << ">";
			bp.current_solution.output_debug_label(cmp_tst);
			#endif

			//bp.current_solution.output_debug_label(o2,false);
			//o2 << ":";
			std::pair<set<ns_segment_path_possible_solution_hash>::iterator, bool> p = recorded_paths.insert(ns_segment_path_possible_solution_hash(bp.current_solution, sorted_solutions_temp));
			if (p.second){
		//		o2 << " Added\n";
				
		//		#ifdef NS_DISPLAY_SOLUTION_COMPARRISONS
		//			cmp_tst << ">Added\n";
		//		#endif*/
				possible_solutions.push_back(bp.current_solution);
			}
		//	else{
		//		o2 << " Rejected\n";
		//		#ifdef NS_DISPLAY_SOLUTION_COMPARRISONS
		//			cmp_tst << ">Rejected\n";
		//		#endif
		//	}
		}
	}

	//o2.close();
	//exit(0);
}

#include "ns_image_easy_io.h"
void ns_smart_complex_segment_cluster_solver::choose_best_solution_for_each_worm_number(const int max_number_of_worms, vector<ns_segment_cluster_solution_group> & solutions, const double source_image_resolution){
	//	worm_segment_paths.resize(0);
	if (possible_solutions.size() == 0)
		return;
	#ifdef NS_DISPLAY_SOLUTION_COMPARRISONS
	worm_id++;
	#endif
	vector<ns_segment_path_possible_solution_group> solution_groups(max_number_of_worms);
	vector<double> length_of_largest_coverage_for_worm_number(max_number_of_worms,0);

	//ofstream lt("c:\\tt\\lengths.txt");
	//first, calculate a few statistics of each solution
	double length_of_largest_coverage_for_any_worm_number(0);
	for (unsigned int i = 0; i < possible_solutions.size(); i++){
		for (possible_solutions[i].number_of_worms = 0; possible_solutions[i].number_of_worms < possible_solutions[i].worms.size() &&
											  possible_solutions[i].worms[possible_solutions[i].number_of_worms].segments.size() != 0;
											  possible_solutions[i].number_of_worms++);
		possible_solutions[i].worms.resize(possible_solutions[i].number_of_worms);
		if (possible_solutions[i].number_of_worms == 0)
			continue;


		vector<char> worm_contains_shared_segments(possible_solutions[i].number_of_worms,0);
		//calculate shared segment length
		possible_solutions[i].shared_segment_length = 0;	
		possible_solutions[i].shared_segment_total_length = 0;
		#ifdef ALLOW_SHARED_SEGMENTS
		for (unsigned int j = 0; j < possible_solutions[i].number_of_worms; j++)
			possible_solutions[i].worms[j].mark_path_as_used(0);
		for (unsigned int j = 0; j < possible_solutions[i].number_of_worms; j++)
			possible_solutions[i].worms[j].increment_path_usage();

		for (unsigned int j = 0; j < possible_solutions[i].number_of_worms; j++){
			for (unsigned int k = 0; k < possible_solutions[i].worms[j].segments.size(); k++){
				if (possible_solutions[i].worms[j].segments[k].segment->usage_count > 1){
					worm_contains_shared_segments[j] = 1;
					possible_solutions[i].worms[j].segments[k].is_shared = true;  //this flag is used later for bitmap disambiguation
					if (possible_solutions[i].worms[j].segments[k].segment->number_of_times_shared == 0){
					//	cerr << "shared segment " << possible_solutions[i].worms[j].segments[k].segment->debug_name << " has length of " << possible_solutions[i].worms[j].segments[k].segment->length() << ": ";
						possible_solutions[i].shared_segment_length += possible_solutions[i].worms[j].segments[k].segment->length();
					}
					possible_solutions[i].shared_segment_total_length+= possible_solutions[i].worms[j].segments[k].segment->length();
					//else 
					//	cerr << "Repeat\n";
					possible_solutions[i].worms[j].segments[k].segment->number_of_times_shared++;
				}
			}
		}
		#endif
		//cerr << "Shared length: " << possible_solutions[i].shared_segment_length  << "\n";


		//calculate max and min
		possible_solutions[i].min_worm_length = FLT_MAX;
		possible_solutions[i].max_worm_length = 0;
		possible_solutions[i].total_length_of_worms = 0;
		possible_solutions[i].min_length_of_solution_with_shared_segment = FLT_MAX;
		
	//	possible_solutions[i].output_debug_label(lt,false);
		for (unsigned int j = 0; j < possible_solutions[i].number_of_worms; j++){
//			lt << possible_solutions[i].worms[j].length() << ",";
			possible_solutions[i].total_length_of_worms+=possible_solutions[i].worms[j].length();
			if (possible_solutions[i].min_worm_length > possible_solutions[i].worms[j].length())
				possible_solutions[i].min_worm_length = possible_solutions[i].worms[j].length();
			if (possible_solutions[i].max_worm_length < possible_solutions[i].worms[j].length())
				possible_solutions[i].max_worm_length = possible_solutions[i].worms[j].length();
			if (worm_contains_shared_segments[j] && possible_solutions[i].min_length_of_solution_with_shared_segment > possible_solutions[i].worms[j].length())
				possible_solutions[i].min_length_of_solution_with_shared_segment = possible_solutions[i].worms[j].length();
		}		
		//lt << "\t\tTotal: " << possible_solutions[i].total_length_of_worms << ",\tshared: " << possible_solutions[i].shared_segment_total_length  <<",\t" << possible_solutions[i].shared_segment_length << "\n";
		double length_of_coverage = possible_solutions[i].length_of_total_coverage();
	//	if (length_of_coverage > length_of_largest_coverage_for_any_worm_number)
	//		lt << "***\n";
		if (length_of_coverage > length_of_largest_coverage_for_any_worm_number)
			length_of_largest_coverage_for_any_worm_number = length_of_coverage;
		//if there is a large difference in the sizes of the worms detected, discard.
		//if (possible_solutions[i].min_worm_length < possible_solutions[i].max_worm_length/2)
		//	continue;
		if (length_of_largest_coverage_for_worm_number[possible_solutions[i].number_of_worms-1] < length_of_coverage)
			length_of_largest_coverage_for_worm_number[possible_solutions[i].number_of_worms-1] = length_of_coverage;
	}
	//string pf = "c:\\tt\\object_";
	//pf+=ns_to_string(worm_id);
	//pf+="_picker.txt";
	//ofstream o("c:\\tt\\picker.txt");

	//go through all solutions, discard some outright, and convert the rest into an easier format for further processing.
//	cerr << possible_solutions.size() << " possible solutions\n";
	for (unsigned int i = 0; i < possible_solutions.size(); i++){		
		//if (possible_solutions[i].number_of_worms != 2)
		//	continue;
		//cerr << possible_solutions[i].number_of_worms << " worm solution found:\n";

			if (possible_solutions[i].length_of_total_coverage() < .85*length_of_largest_coverage_for_worm_number[possible_solutions[i].number_of_worms-1]){
				//o<< "CWTotal(" << possible_solutions[i].total_length_of_worms << "vs" << length_of_largest_coverage_for_worm_number[possible_solutions[i].number_of_worms-1] << ")\n";
				continue;
			}
		//	possible_solutions[i].output_debug_label(o,false);
			
			if (possible_solutions[i].number_of_worms != 1 &&  //always allow a one worm solution
				possible_solutions[i].length_of_total_coverage() < .65*length_of_largest_coverage_for_any_worm_number){
		//		o << "AWLength(" << possible_solutions[i].length_of_total_coverage() << "vs" << length_of_largest_coverage_for_any_worm_number << ")\n";
			//	if (possible_solutions[i].shared_segment_total_length != 0) cerr << "TOTAL LENGTH\n";
				continue;
			}		
		

			if (possible_solutions[i].min_worm_length < .75*possible_solutions[i].max_worm_length){
		//		o << "Uneven(" << possible_solutions[i].min_worm_length << "vs"  << possible_solutions[i].max_worm_length << ")\n";
			//	if (possible_solutions[i].shared_segment_total_length != 0) cerr << "UNEVEN\n";
				continue;
			}


			//cerr << "Shared: (" << possible_solutions[i].shared_segment_length << ") vs min:(" << possible_solutions[i].min_length_of_solution_with_shared_segment << ")\n";
				
			if (possible_solutions[i].shared_segment_length > .5*possible_solutions[i].min_length_of_solution_with_shared_segment){
	  //			o << "Shared(" << possible_solutions[i].shared_segment_length << " vs " << possible_solutions[i].min_length_of_solution_with_shared_segment << ")\n";
				continue;
			}
		//	o << "OK\n";

		//Ok!  The path has passed the first set of criterea, now we convert it to a simpler format for further processing.
		unsigned int p = possible_solutions[i].number_of_worms-1;
		unsigned int q = (unsigned int)solution_groups[p].solutions.size();
		solution_groups[p].solutions.resize(q+1,vector<vector<ns_node *> >(possible_solutions[i].number_of_worms));
		solution_groups[p].solution_source.resize(q+1,&possible_solutions[i]);
		//cerr << "Adding a simple path with " << possible_solutions[i].number_of_worms << " worms with lengths ";
		for (unsigned int j = 0; j < possible_solutions[i].number_of_worms; j++){
			//cerr << possible_solutions[i].worms[j].length() << ",";
			possible_solutions[i].worms[j].generate_simple_path(solution_groups[p].solutions[q][j]);
		}
		//cerr << "\n";
	}
	//o.close();
	//ofstream o;
	//o.open("c:\\tt\\curvature.txt");
	solutions.resize(0);
	const int distance_from_joint_to_search = 10;
	#ifndef NS_USE_BSPLINE
		double (*calc_curvature) (const vector<ns_vector_2d> & );
		if (source_image_resolution <= 1201)
			calc_curvature=ns_calculate_average_curvature<ns_worm_detection_constants::spine_smoothing_radius_1200>;
		else 
			calc_curvature=ns_calculate_average_curvature<ns_worm_detection_constants::spine_smoothing_radius_1200>;
	#endif
	const double end_crop_fraction=((double)ns_worm_detection_constants::percent_distance_to_retract_spine_from_ends/(double)100);
	ns_bspline bspline;
	//now we can choose set winners based on curvature minimization.
//	ofstream cdbg("c:\\tt\\curvature_debug.txt");
	for (unsigned int i = 0; i < solution_groups.size(); i++){
	//	cerr << "Group " << i << " has " << solution_groups[i].solutions.size() << " possibilities.\n";
	//o << "**Solution group " << i << "**\n";

		if (solution_groups[i].solutions.size() == 1){
			solution_groups[i].winning_solution_id = 0;
		}
		//choose based on curvature
		else{
			solution_groups[i].min_total_curvature = FLT_MAX;
			solution_groups[i].winning_solution_id = -1;
			for (unsigned int s = 0; s < solution_groups[i].solutions.size(); s++){
				if (solution_groups[i].solutions[s].size() == 0)
					continue;
			//	double max_curvature = 0;
				vector<double> mean_curvature(solution_groups[i].solutions[s].size(),0);
				vector<double> max_curvature(solution_groups[i].solutions[s].size(),0);
				vector<double> variance_in_curvature(solution_groups[i].solutions[s].size(),0);
			//	vector<double> skew_in_curvature(solution_groups[i].solutions[s].size(),0);
				vector<double> worm_length(solution_groups[i].solutions[s].size(),0);
				
		
	//			solution_groups[i].solution_source[s]->output_debug_label(cdbg,false);
				for (unsigned int w = 0; w < solution_groups[i].solutions[s].size(); w++){
					
					vector<ns_vector_2d> nodes(solution_groups[i].solutions[s][w].size());
					for (unsigned int j = 0; j < solution_groups[i].solutions[s][w].size(); j++)
						nodes[j] = solution_groups[i].solutions[s][w][j]->position;

					
					bspline.calculate_with_standard_params(nodes,(unsigned int)nodes.size()/NS_BSPLINE_OUTPUT_DOWNSAMPLE_RATE,true);
					bspline.crop_ends(end_crop_fraction);
					
					//vis
					/*ns_image_properties prop(0,0,1,3200);
					
					for (unsigned int j = 0; j < bspline.positions.size(); j++){
						if (bspline.positions[j].x > prop.width)
							prop.width = ceil(bspline.positions[j].x);
						if (bspline.positions[j].y > prop.height)
							prop.height = ceil(bspline.positions[j].y);
					}
					for (unsigned int j = 0; j < nodes.size(); j++){
						if (nodes[j].x > prop.width)
							prop.width = ceil(nodes[j].x);
						if (nodes[j].y > prop.height)
							prop.height = ceil(nodes[j].y);
					}
					ns_image_standard vis;
					prop.width++;prop.height++;
					prop.width*=2; prop.height*=2;
					vis.init(prop);
					for (unsigned int y = 0; y < prop.height; y++)
						for (unsigned int x = 0; x < prop.width; x++)
							vis[y][x] = 0;
					for (unsigned int j = 0; j < bspline.positions.size(); j++)
						vis[2*(int)bspline.positions[j].y][2*(int)bspline.positions[j].x] = 255;
					for (unsigned int j = 0; j < nodes.size(); j++)
						vis[2*(int)nodes[j].y][2*(int)nodes[j].x] = 127;
					string filename = "c:\\tt\\object_";
					filename += ns_to_string(worm_id);
					filename +="_spline_count_";
					filename += ns_to_string(i);
					filename += "=option";
					filename += ns_to_string(s);
					filename += "=worm";
					filename += ns_to_string(w);
					filename += "of";
					filename += ns_to_string(solution_groups[i].solutions[s].size());
					filename += ".tif";
					ns_save_image(filename,vis);
					// /vis
	*/
					for (unsigned int j = 0; j < bspline.curvature.size(); j++){
						mean_curvature[w]+=bspline.curvature[j];
						if (bspline.curvature[j] > max_curvature[w])
							max_curvature[w] = bspline.curvature[j];
					}
					
				
					mean_curvature[w]/=bspline.curvature.size();
					for (unsigned int j = 0; j < bspline.curvature.size(); j++)
						variance_in_curvature[w]+=(bspline.curvature[j]-mean_curvature[w])*(bspline.curvature[j]-mean_curvature[w]);
					variance_in_curvature[w]/=bspline.curvature.size();
					//for (unsigned int j = 0; j < bspline.curvature.size(); j++)
					//	skew_in_curvature[w]+=(bspline.curvature[j]-mean_curvature[w])*(bspline.curvature[j]-mean_curvature[w])*(bspline.curvature[j]-mean_curvature[w]);
					//skew_in_curvature[w]/=pow(variance_in_curvature[w],1/5);

					worm_length[w] = bspline.length;
			//		cdbg << "[" << mean_curvature[w] << "," << variance_in_curvature[w] << "," << skew_in_curvature[w] << "," << bspline.length << "]";
				}
				double mean_mean_curvature = 0;
				double mean_variance_in_curvature = 0;
			//	double mean_skew_in_curvature = 0;
				double total_length = 0;
				double max_max_curvature = 0;
				for (unsigned int k = 0; k < mean_curvature.size(); k++){
				//	mean_mean_curvature+=mean_curvature[k]*worm_length[k];
					mean_variance_in_curvature+=variance_in_curvature[k]*worm_length[k];
				//	mean_skew_in_curvature+=skew_in_curvature[k]*worm_length[k];
					total_length+=worm_length[k];
					if (max_curvature[k] >  max_max_curvature)
						 max_max_curvature = max_curvature[k];
				}
				//mean_mean_curvature/=total_length;
				mean_variance_in_curvature/=total_length;
			//	mean_skew_in_curvature/=total_length;
				//cdbg << "\n\t::M(" << mean_mean_curvature<<"),V("<<mean_variance_in_curvature << "),S("<<mean_skew_in_curvature << ")\n";
				
	//			solution_groups[i].solution_source[s]->output_debug_label(o,false);
				//o << " (Curvature == " << max_curvature << ")";
				if (mean_variance_in_curvature< solution_groups[i].min_total_curvature){
				//	o << "Current Winner";
					solution_groups[i].min_total_curvature = mean_variance_in_curvature;
					solution_groups[i].winning_solution_id = s;
				}
			
			}
		}
		
//		if (solution_groups[i].winning_solution_id != -1){
	//		cdbg << "\nWinner: ";
//			solution_groups[i].solution_source[solution_groups[i].winning_solution_id ]->output_debug_label(cdbg,false) ;
	//		cdbg << " " << solution_groups[i].min_total_curvature <<"\n";
//		}
	//	else cdbg << "\nNo Winner.\n";
		#ifdef ALLOW_ALL_SPINE_PERMUTATIONS
		for (unsigned int j = 0; j< solution_groups[i].solutions.size(); j++){
			solution_groups[i].winning_solution_id = j;
		#endif
		//if a winner is found, add it to the winning group.
		if (solution_groups[i].winning_solution_id != -1){
		//	cerr << "From solution group " << i << ", " << solution_groups[i].winning_solution_id << " wins ";;
			unsigned int s = (unsigned int)solutions.size();
			//s = 0;
			solutions.resize(s+1);
			//if (i!=1) continue;
			//the winning solution:
			ns_segment_path_possible_solution & winner(*solution_groups[i].solution_source[solution_groups[i].winning_solution_id]);
			//cerr << " with length " << winner.total_length_of_worms << "\n";
			ns_segment_cluster_solution_group & best_solution_for_worm_number(solutions[s]);
			//add a group of ns_segment_cluster_solution
			best_solution_for_worm_number.insert(best_solution_for_worm_number.end(),winner.worms.begin(),winner.worms.end());
		}
	//	else cerr << "From solution group " << i << " nobody wins.\n";
		#ifdef ALLOW_ALL_SPINE_PERMUTATIONS
		}
		#endif
	}
	
}



void ns_naive_complex_segment_cluster_solver::run(ns_segment_cluster & segment_cluster,
												  vector<ns_segment_cluster_solution_group> & mutually_exclusive_solutions){

	vector<ns_segment_path_possible_solution> longest_solutions(segment_cluster.segments.size());
	for (unsigned int s = 0; s < segment_cluster.segments.size(); s++){
		ns_segment_path_possible_solution & longest_solution = longest_solutions[s];

		std::stack<ns_segment_path_search_branch_point> branch_points;

		//start in all directions from the current end
		for (unsigned int i = 0; i < segment_cluster.segments.size(); i++){
			branch_points.push(ns_segment_path_search_branch_point(1,0,ns_segment_link(segment_cluster.segments[i],false),false));
			branch_points.push(ns_segment_path_search_branch_point(1,0,ns_segment_link(segment_cluster.segments[i],true),true));
		}
		//xxx
		//build hash table for end names
		/*map<ns_spine_segment_end *,unsigned int> end_names;
		for (unsigned int i = 0; i < segment_ends.size(); i++)
			end_names[segment_ends[i]] = i;
		vector< vector< vector<bool> > > used_worm_segments_table;
		used_worm_segments_table.resize( 0, 
											vector<vector<bool> >(segment_ends.size(), 
											vector<bool>(segment_ends.size(),
											false)));
*/

		while(!branch_points.empty()){
			ns_segment_path_search_branch_point bp = branch_points.top();
			branch_points.pop();

			//refresh path map for current solution
			for (unsigned int i = 0; i < segment_cluster.segments.size(); i++)
				segment_cluster.segments[i]->usage_count=0;
			for (unsigned int i = 0; i < bp.current_solution.worms.size(); i++)
				bp.current_solution.worms[i].mark_path_as_used(1);

			//set up some variables
			ns_segment_cluster_solution & cur_solution = bp.current_solution.worms[bp.current_worm_id];
			if (cur_solution.segments.size() == 0)
				throw ns_ex("ns_naive_complex_segment_cluster_solver::Empty segment path search branch point path encountered!");
			ns_segment_link & cur_segment = cur_solution.segments[cur_solution.segments.size()-1];

			
			cur_solution.length_+=(float)cur_segment.segment->length();
		
		
			//create branch points that continue to explore the graph with the current worm
			bool reached_segment_end = true;
			for (unsigned int i = 0; i < cur_segment.end_neighbors().size(); i++){
				if (cur_segment.end_neighbors()[i].segment->usage_count != 0) //stop at segments that have already been used
					continue;
				if (cur_segment.end_neighbors()[i].segment == cur_segment.segment) //don't double back on the segment used in the current iteration
						continue;
				reached_segment_end = false;
				branch_points.push(ns_segment_path_search_branch_point(bp.current_solution,bp.current_worm_id,cur_segment.end_neighbors()[i],false));
			}
			if (reached_segment_end){
			
				if (cur_solution.length() > longest_solution.worms[0].length())
					longest_solution = (bp.current_solution);
				cur_solution.mark_path_as_used(0);
			}
		}
	}

	double longest_path = 0;
	unsigned int longest_path_id = 0;
	for (unsigned int i = 0; i < longest_solutions.size(); i++){
		if (longest_solutions[i].worms[0].length() > longest_path){
			longest_path = longest_solutions[i].worms[0].length();
			longest_path_id = i;
		}
	}

	mutually_exclusive_solutions.resize(1);
	mutually_exclusive_solutions[0].resize(1);
	mutually_exclusive_solutions[0][0].segments.insert(mutually_exclusive_solutions[0][0].segments.end(),
														longest_solutions[longest_path_id].worms[0].segments.begin(),
														longest_solutions[longest_path_id].worms[0].segments.end());	
}
