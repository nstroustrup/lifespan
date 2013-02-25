#include "ns_segment_topology.h"
#include "ns_complex_segment_cluster_solver.h"
//#define NS_USE_BSPLINE
#ifdef NS_USE_BSPLINE
#include "ns_bspline.h"
#else
#include "ns_vector_bitmap_interface.h"
#endif

using namespace std;

ns_segment_link::ns_segment_link(ns_segment * seg,const bool pol):segment(seg),
		polarity_(pol),
		start_neighbors_(pol ? &seg->end_neighbors: &seg->start_neighbors),
		end_neighbors_(pol ? &seg->start_neighbors: &seg->end_neighbors),is_shared(false){}


ns_segment_cluster::~ns_segment_cluster(){
	for (unsigned int i = 0; i < segments.size(); i++)
		delete segments[i];
}

void ns_segment_link::merge(ns_segment_link & seg){
	//cerr << "Merging " << seg.segment->debug_name << " into " << segment->debug_name << "\n";
	segment->length_ += seg.segment->length();
	bool joined_at_start(false),
		 seg_is_reverse(false);

	for(std::vector<ns_segment_link>::iterator p = start_neighbors().begin(); p != start_neighbors().end();p++){
		if (p->segment == seg.segment){
			joined_at_start = true;
			///?
			seg_is_reverse = polarity() != p->polarity();
			start_neighbors().erase(p);
			break;
		}
	}
	if (!joined_at_start){
		bool found(false);
		for(std::vector<ns_segment_link>::iterator p = end_neighbors().begin(); p != end_neighbors().end();p++){
				if (p->segment == seg.segment){
					found = true;
					seg_is_reverse = p->polarity() == polarity();
					end_neighbors().erase(p);
					break;
				}
			}
	}
	bool seg_polarity = true;
	for(std::vector<ns_segment_link>::iterator p = seg.start_neighbors().begin(); p != seg.start_neighbors().end();p++){
		if (p->segment == segment){
			seg_polarity = false;
			break;
		}
	}

	std::vector<ns_node>::iterator node_insertion_location = segment->nodes.begin();
	//merge nodes
	if (joined_at_start == polarity())
		node_insertion_location = segment->nodes.end();
	if (seg_is_reverse)
		segment->nodes.insert(node_insertion_location,seg.segment->nodes.begin(),seg.segment->nodes.end());
	else segment->nodes.insert(node_insertion_location,seg.segment->nodes.rbegin(),seg.segment->nodes.rend());
	

	//update neighbor of neighbors
	std::vector<ns_segment_link> * to_change(&seg.start_neighbors());
	if (!seg_polarity)
		to_change = &seg.end_neighbors();
	for (std::vector<ns_segment_link>::iterator neighbor = to_change->begin(); neighbor != to_change->end(); neighbor++){
		bool neighbor_is_reversed;
		for (std::vector<ns_segment_link>::iterator neighbor_neighbor = neighbor->start_neighbors().begin(); neighbor_neighbor != neighbor->start_neighbors().end();){
			if (neighbor_neighbor->segment == seg.segment){
				neighbor_is_reversed = seg_is_reverse != neighbor_neighbor->polarity();
				neighbor_neighbor = neighbor->start_neighbors().erase(neighbor_neighbor);
			}
			else neighbor_neighbor++;
		}
		neighbor->start_neighbors().push_back(ns_segment_link(segment,!neighbor_is_reversed));
	}
	//add neighbor of neighbors
	std::vector<ns_segment_link> * neighbor_insertion_location = &start_neighbors();
	if (!joined_at_start)
		neighbor_insertion_location = &end_neighbors();
	for (std::vector<ns_segment_link>::iterator neighbor = to_change->begin(); neighbor != to_change->end(); neighbor++){
		neighbor_insertion_location->push_back(*neighbor);
	}
	
	seg.segment->marked_for_deletion = true;
}
void ns_segment_cluster::remove_short_segments(const double cutoff_length, const bool only_hanging){
	const unsigned int min_number_segments(3);
	unsigned int segments_left = (unsigned int)segments.size();
	if (segments_left < min_number_segments)
		return;

	for (std::vector<ns_segment *>::iterator p = segments.begin(); p != segments.end() && segments_left >= min_number_segments;){
		if ((*p)->length() >= cutoff_length ||
			only_hanging && 
			((*p)->end_neighbors.size() != 0 &&
			 (*p)->start_neighbors.size() != 0)){
				p++;
				//cerr << segments_left  << "\n";
				continue;
		}
	//	cerr << "Removing " << (*p)->debug_name << " ";
		//disconnect deleted segment from its neighbors
		for (std::vector<ns_segment_link>::iterator n = (*p)->start_neighbors.begin(); n != (*p)->start_neighbors.end(); n++){
			for (std::vector<ns_segment_link>::iterator q = n->start_neighbors().begin(); q != n->start_neighbors().end();q++){
				if (q->segment == (*p)){
					n->start_neighbors().erase(q);
					break;
				}
			}
		}
		for (std::vector<ns_segment_link>::iterator n = (*p)->end_neighbors.begin(); n != (*p)->end_neighbors.end(); n++){
			for (std::vector<ns_segment_link>::iterator q = n->start_neighbors().begin(); q != n->start_neighbors().end();q++){
				if (q->segment == (*p)){
					n->start_neighbors().erase(q);
					break;
				}
			}
		}
		if ((*p)->start_neighbors.size() > 1){
		//	cerr << "merging " << (*p)->start_neighbors[0].segment->debug_name << " and " << (*p)->start_neighbors[1].segment->debug_name << ",";
			(*p)->start_neighbors[0].merge((*p)->start_neighbors[1]);
			segments_left--;
		}
		if ((*p)->end_neighbors.size() > 1){
			
			//cerr << "merging " << (*p)->end_neighbors[0].segment->debug_name << " and " << (*p)->end_neighbors[1].segment->debug_name << ",";
			(*p)->end_neighbors[0].merge((*p)->end_neighbors[1]);
			segments_left--;
		}
	//	cerr << "\n";
		(*p)->marked_for_deletion = true;
		p++;
		segments_left--;
		//cerr << segments_left  << "\n";
		//p++;
	}
	for (std::vector<ns_segment *>::iterator p = segments.begin(); p != segments.end();){
		if ((*p)->marked_for_deletion){
			//XXX
			delete *p;
			p = segments.erase(p);
		}
		else p++;
	}

}
inline char ns_end_label(bool backward){
	if (backward)return 'e';
	else return 's';
}

void ns_segment_cluster::output_debug(ostream & out){
	cerr << "WRITING SEGMENT DEBUG INFO\n";
	if (segments.size() == 0) return;
	if (segments[0]->debug_name.size() == 0){
		for (unsigned int i = 0; i < segments.size(); i++)
			segments[i]->debug_name += char('a'+i);
	}
	out << "graph segment_cluster {\n";
	out << "\t resolution = 72\n";
	for (unsigned int i = 0; i < segments.size(); i++){
		out << "\t\"" << segments[i]->debug_name <<  "s\" [label=\"" << segments[i]->debug_name << "s\"]\n";
		out << "\t\"" << segments[i]->debug_name <<  "e\" [label=\"" << segments[i]->debug_name << "e\"]\n";
	}
	for (unsigned int i = 0; i < segments.size(); i++){
		out << "\t" << segments[i]->debug_name << "s--" << segments[i]->debug_name << "e [label=\"" << segments[i]->length() << "\" style=\"setlinewidth(2)\"]\n";;
	//	out << "\t" << segments[i]->debug_name << "e--" << segments[i]->debug_name << "s \n";
		
		for (unsigned int j = 0; j < segments[i]->start_neighbors.size(); j++)
			out << "\t" << segments[i]->debug_name << "s--" << segments[i]->start_neighbors[j].segment->debug_name << ns_end_label(segments[i]->start_neighbors[j].polarity()) << " [dir=forward,style=\"dashed\"]\n";
		for (unsigned int j = 0; j < segments[i]->end_neighbors.size(); j++)
				out << "\t" << segments[i]->debug_name << "e--" << segments[i]->end_neighbors[j].segment->debug_name << ns_end_label(segments[i]->end_neighbors[j].polarity()) << " [dir=forward,style=\"dashed\"]\n";
	}
	out << "}\n";
}

unsigned long debug_id(0);
void ns_segment_cluster::compile(const ns_node_topology & n){
	segments.resize(n.segments.size());
	for (unsigned int i = 0; i < segments.size(); i++)
		segments[i] = new ns_segment();

	map<ns_node_topology_segment *,unsigned int> neighbor_mappings;

	float total_graph_length(0);
	//populate node list and set up neighbor mappings
	for (unsigned int i = 0; i < n.segments.size(); i++){
		neighbor_mappings[n.segments[i]] = i;
		segments[i]->length_ = 0;

		segments[i]->nodes.resize(n.segments[i]->nodes.size()+2);
		segments[i]->nodes[0].position = n.segments[i]->end[0]->node->coord;
		ns_vector_2d prev = n.segments[i]->end[0]->node->coord;
		segments[i]->nodes[0].width = n.segments[i]->end[0]->node->width;
		for (unsigned int j = 0; j < n.segments[i]->nodes.size(); j++){
			segments[i]->nodes[j+1].position = n.segments[i]->nodes[j]->coord;
			segments[i]->nodes[j+1].width = n.segments[i]->nodes[j]->width;
			segments[i]->length_ += (float)(n.segments[i]->nodes[j]->coord - prev).mag();
			prev = n.segments[i]->nodes[j]->coord;
		}
		segments[i]->nodes[segments[i]->nodes.size()-1].position = n.segments[i]->end[1]->node->coord;
		segments[i]->nodes[segments[i]->nodes.size()-1].width = n.segments[i]->end[1]->node->width;
		segments[i]->length_ += (float)(n.segments[i]->end[1]->node->coord - prev).mag();
		total_graph_length+=segments[i]->length_;
	}
	//populate neighbor list using mappings
	for (unsigned int i = 0; i < n.segments.size(); i++){
		for (unsigned int j = 0; j < 3; j++){
			//start neighbors
			if (n.segments[i]->end[0]->neighbor[j] != 0 &&
				n.segments[i]->end[0]->neighbor[j] != n.segments[i]){
					map<ns_node_topology_segment *,unsigned int>::iterator p = neighbor_mappings.find(n.segments[i]->end[0]->neighbor[j]);
					if (p == neighbor_mappings.end()) 
						throw ns_ex("ns_segment_cluster::Could not link up segment mappings when building from node topology");
					segments[i]->start_neighbors.push_back(ns_segment_link(segments[p->second],n.segments[i]->end[0]->neighbor_attachment_end[j]));
			}	
			//end neighbors
			if (n.segments[i]->end[1]->neighbor[j] != 0 &&
				n.segments[i]->end[1]->neighbor[j] != n.segments[i]){
					map<ns_node_topology_segment *,unsigned int>::iterator p = neighbor_mappings.find(n.segments[i]->end[1]->neighbor[j]);
					if (p == neighbor_mappings.end()) 
						throw ns_ex("ns_segment_cluster::Could not link up segment mappings when building from node topology");
					segments[i]->end_neighbors.push_back(ns_segment_link(segments[p->second],n.segments[i]->end[1]->neighbor_attachment_end[j]));
			}
		}
	}
	double average_graph_length = total_graph_length/segments.size();
	
	double maximum_size_of_irrelevant_segments = total_graph_length/5;
	/*std::string prefix("c:\\tt\\object");
	prefix+=ns_to_string(debug_id) + "_";
	std::string f = prefix + "nodes_before.txt";
	ofstream o1(f.c_str());
	n.dump_node_graph(o1);
	o1.close();
	std::string filename = prefix + "segments.txt";
	ofstream o(filename.c_str());
	output_debug(o);
	o.close();*/
	//cerr << "Removing segments shorter than " << average_graph_length/2 << "\n";
	double cutoff = .75*average_graph_length;
	if (cutoff > maximum_size_of_irrelevant_segments)
		cutoff = maximum_size_of_irrelevant_segments;
	remove_short_segments(cutoff);
	/*filename = prefix + "segments_later.txt";
	o.open(filename.c_str());
	output_debug(o);
	o.close();*/
//	debug_id++;
	//cerr << segments.size() << "\n";
}




void ns_segment_cluster_solutions::calculate(ns_segment_cluster & cluster,const float source_image_resolution, const bool do_multiple_worm_disambiguation){

	if (cluster.segments.size() == 0)
		return;

	//if there is only one skeleton segment, add it to the ouput and we're finished!
	if (cluster.segments.size() == 1 && cluster.segments[0]->nodes.size() != 0){
		mutually_exclusive_solution_groups.resize(1,
			ns_segment_cluster_solution_group(1,
			ns_segment_cluster_solution(std::vector<ns_segment_link>(1,ns_segment_link(cluster.segments[0],false)))));

				ns_vector_2d prev = mutually_exclusive_solution_groups[0][0].segments[0].segment->nodes[0].position;
		for (unsigned int i = 1; i < mutually_exclusive_solution_groups[0][0].segments[0].segment->nodes.size(); i++){
			mutually_exclusive_solution_groups[0][0].length_+= (float)(mutually_exclusive_solution_groups[0][0].segments[0].segment->nodes[i].position-prev).mag();
			prev= mutually_exclusive_solution_groups[0][0].segments[0].segment->nodes[i].position;
		}
		return;
	}

	//If a region breaks down into multiple connected skeleton segments,
	//it could represent multiple worms touching one another.
	//We need to analyze the topology so we can break it individual worms.
	//We can do this two ways:
	//1.  With a clever algorithm that identifies multiple worms, but whose run-time 
	//    scales alarmingly with the number of segments
	//2.  With a naive algorithm that just finds the longest path through the skeleton network
	//    and assums that is the worm.
	//We use #1 if there are a reasonable number of segments, #2 if there are tons of them.
	
	//unsigned long time = ns_current_time();
	if (do_multiple_worm_disambiguation && cluster.segments.size() < 7){
		ns_smart_complex_segment_cluster_solver smart_solver;
		smart_solver.run(ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_worms_per_multiple_worm_cluster),source_image_resolution,cluster,mutually_exclusive_solution_groups);
		//cerr << "Complex solver calculation: " << ns_current_time() - time << "\n";
	}
	else{
		ns_naive_complex_segment_cluster_solver::run(cluster,mutually_exclusive_solution_groups);
	}
}

double ns_worm_shape::angular_orientation() const{
	double angle = 0;
	ns_vector_2d top(0,0),
				bottom(0,0);
	for (unsigned int i = 0; i < spine_center_node_id; i++)
		bottom = bottom + nodes[i];
	for (unsigned int i = spine_center_node_id; i < nodes.size(); i++)
		top = top + nodes[i];

	return (bottom-top).angle();

}

void ns_worm_shape::from_segment_cluster_solution(const ns_segment_cluster_solution & sn){

	int node_count(0);

	for (unsigned int i = 0; i < sn.segments.size(); i++){
		if (sn.segments[i].segment->nodes.size() == 0) 
			throw ns_ex("ns_worm_shape::from_segment_cluster_solution()::Encountered a segment containing no nodes.");
		node_count+=(int)sn.segments[i].segment->nodes.size()-1;
	}
	node_count++;

	nodes.resize(node_count);	
	nodes_unsmoothed.resize(node_count);	
	width.resize(node_count);	
	normal_0.resize(nodes.size(),ns_vector_2d(0,0));
	normal_1.resize(nodes.size(),ns_vector_2d(0,0));
	curvature.resize(nodes.size(),0);
	nodes_are_shared.resize(nodes.size());
	nodes_are_near_shared.resize(nodes.size());
	unsigned int cur_node(0);
	for (unsigned int i = 0; i < sn.segments.size(); i++){
		bool segment_is_shared = sn.segments[i].is_shared;
		if (!sn.segments[i].polarity())
			for (unsigned int j = 0; j < sn.segments[i].segment->nodes.size()-1; j++){
				nodes[cur_node] = sn.segments[i].segment->nodes[j].position;
				nodes_unsmoothed[cur_node] = nodes[cur_node];
				width[cur_node] = (float)sn.segments[i].segment->nodes[j].width;
				nodes_are_shared[cur_node] = segment_is_shared;
				cur_node++;
			}
		else
			for (unsigned int j = (unsigned int)sn.segments[i].segment->nodes.size()-1; j>0; j--){
				nodes[cur_node] = sn.segments[i].segment->nodes[j].position;
				nodes_unsmoothed[cur_node] = nodes[cur_node];
				width[cur_node] = (float)sn.segments[i].segment->nodes[j].width;
				nodes_are_shared[cur_node] = segment_is_shared;
				cur_node++;
			}
	}
	if (sn.segments[sn.segments.size()-1].polarity()){
		nodes[cur_node] = sn.segments[sn.segments.size()-1].segment->nodes[0].position;
		nodes_unsmoothed[cur_node] = nodes[cur_node];
	}
	else{
		nodes[cur_node] = sn.segments[sn.segments.size()-1].segment->nodes[sn.segments[sn.segments.size()-1].segment->nodes.size()-1].position;
		nodes_unsmoothed[cur_node] = nodes[cur_node];
	}


	unsigned long shared_blur = (unsigned long)nodes_are_shared.size()/5;
	unsigned long cur_blur_count(0);
	for (unsigned long i = 0; i < shared_blur; i++)
		cur_blur_count+=nodes_are_shared[i];
	
	for (unsigned long i = 0; i < shared_blur; i++)
		nodes_are_near_shared[i] = cur_blur_count > 0;
	for (unsigned long i = shared_blur; i < nodes_are_near_shared.size()-shared_blur; i++){
		cur_blur_count-=nodes_are_shared[i-shared_blur];
		cur_blur_count+=nodes_are_shared[i+shared_blur];
		nodes_are_near_shared[i] = cur_blur_count > 0;
	}
	for (unsigned long i = (unsigned long)nodes_are_near_shared.size()-shared_blur; i < (unsigned long)nodes_are_near_shared.size(); i++)
		nodes_are_near_shared[i] = cur_blur_count > 0;


	#ifdef NS_USE_BSPLINE
	//resample worm as a bspline
	ns_bspline bspline;
	if (nodes.size() == 0)
		throw ns_ex("ns_worm_shape::from_segment_cluster_solution::Found an empty object while preparing to spline segment");
	if (nodes.size() == 1)
		throw ns_ex("ns_worm_shape::from_segment_cluster_solution::Found an object with only one node while preparing to spline segment");

	unsigned long output_size(((unsigned long)nodes.size())/NS_BSPLINE_SELECTION_DOWNSAMPLE_RATE);
	if (output_size < 2)
		output_size = 2;
	bspline.calculate_with_standard_params(nodes,output_size,false);	
	
	const double frac_dist=((double)ns_worm_detection_constants::percent_distance_to_retract_spine_from_ends/(double)100);
	unsigned long start_crop = bspline.crop_ends(frac_dist);
	ns_crop_vector(nodes_are_shared,start_crop,(unsigned long)bspline.positions.size());
	ns_crop_vector(nodes_are_near_shared,start_crop,(unsigned long)bspline.positions.size());
	ns_crop_vector(nodes_unsmoothed,start_crop,(unsigned long)bspline.positions.size());

	nodes.resize(bspline.positions.size());
	width.resize(bspline.positions.size());
	curvature.resize(bspline.positions.size());
	normal_0.resize(bspline.positions.size());
	normal_1.resize(bspline.positions.size());

	for (unsigned int i = 0; i < (unsigned int)bspline.positions.size(); i++){
		nodes[i] = bspline.positions[i];
		curvature[i] = (float)bspline.curvature[i];
		normal_0[i] = ns_vector_2d(-bspline.tangents[i].y,bspline.tangents[i].x);
		normal_1[i] = ns_vector_2d(bspline.tangents[i].y,-bspline.tangents[i].x);
	}
	#else

		if (bitmap().properties().resolution <= 1201)
			ns_calculate_curvature<ns_worm_detection_constants::spine_smoothing_radius_1200>(nodes,curvature);
		else
			ns_calculate_curvature<ns_worm_detection_constants::spine_smoothing_radius_3200>(nodes,curvature);

	#endif


}
	
void inline ns_extrude_normals_to_hull(const ns_vector_2d & pos, ns_vector_2d & normal_0, ns_vector_2d & normal_1, const std::vector<ns_edge_2d> & edges, long edge_start=0, long edge_end=-1, bool only_consider_shorter_edges=false, bool zero_out_lost_segments=false){

			if (edge_end == -1) edge_end = (long)edges.size();

			ns_edge_2d norm_trans(pos,pos+normal_0);

			double pos_min,
				   neg_min;
			ns_vector_2d intersect,
						 pos_i,
						 neg_i;

			if (zero_out_lost_segments){
				pos_min = DBL_MAX;
				neg_min = DBL_MAX;
				pos_i=ns_vector_2d(0,0);
				neg_i=ns_vector_2d(0,0);
			}
			else{
				pos_min = normal_0.squared();
				neg_min = normal_1.squared();
				pos_i =normal_0;
				neg_i=normal_1;
			}

			double tmp;

			ns_vector_2d u;
			if (only_consider_shorter_edges){
				double my_len = (normal_0 - normal_1).squared();
				for (unsigned int k = (unsigned int)edge_start; k < (unsigned int)edge_end; k++){
					if ((edges[k].vertex[0] - edges[k].vertex[1]).squared() > my_len) continue;
					if (ns_intersect_ls<double>(norm_trans,edges[k],intersect,u)){
						tmp = (intersect - pos).squared();
						if (u.x >= 0){
							if (tmp < pos_min) {pos_min = tmp; pos_i = intersect- pos;}
						}
						else {
							if (tmp < neg_min) {neg_min = tmp; neg_i = intersect- pos;}
						}
					}
				}
			}
			else 
				for (unsigned int k = (unsigned int)edge_start; k < (unsigned int)edge_end; k++){
							if (ns_intersect_ls<double>(norm_trans,edges[k],intersect,u)){
								tmp = (intersect - pos).squared();
								if (u.x >= 0){
									if (tmp < pos_min) {pos_min = tmp; pos_i = intersect- pos;}
								}
								else {
									if (tmp < neg_min) {neg_min = tmp; neg_i = intersect- pos;}
								}
							}
						}
			if (pos_min == DBL_MAX){
					normal_0 = ns_vector_2d(0,0);
					normal_1 = ns_vector_2d(0,0);
					cerr << "MISS";
			}
			else{
				normal_0 = pos_i;
				normal_1 = neg_i;
			}
}

void ns_worm_shape::finalize_worm_and_calculate_width(const std::vector<ns_edge_2d> & edges){

	
	//now we need to find the width by extending the normals until they hit the hull of the object
	for (unsigned int i = 0; i < (unsigned int)nodes.size(); i++)
		ns_extrude_normals_to_hull(nodes[i],normal_0[i],normal_1[i],edges,0,(long)edges.size(),false,false);

	std::vector<ns_edge_2d> width_edges(normal_0.size());
	for (unsigned int i = 0; i < normal_0.size(); i++){
		width_edges[i].vertex[0] = normal_0[i]+nodes[i];
		width_edges[i].vertex[1] = normal_1[i]+nodes[i];
	}
	const unsigned int neighbors_to_check(10);
	for (unsigned long i = 0; i < (unsigned long)nodes.size(); i++){
		unsigned start = i > neighbors_to_check?i-neighbors_to_check:0;
		unsigned stop = i + neighbors_to_check <= nodes.size()?i+neighbors_to_check : nodes.size();
		ns_extrude_normals_to_hull(nodes[i],normal_0[i],normal_1[i],width_edges,start,stop,true);
	}

	//ends are noisy, clean them out.
	unsigned int d(4);
	if (d+2 < curvature.size()){
		for (unsigned int i = 0; i < d; i++){
			curvature[i] = curvature[d];
			curvature[nodes.size()-1-i] = curvature[curvature.size()-d];
			normal_0[i] = normal_0[d]*(1.0-(d-i)/(float)d);
			normal_1[i] = normal_1[d]*(1.0-(d-i)/(float)d);
			normal_0[nodes.size()-1-i] = normal_0[curvature.size()-d]*(1.0-(d-i)/(float)d);
			normal_1[nodes.size()-1-i] = normal_1[curvature.size()-d]*(1.0-(d-i)/(float)d);
		}
	}
	width.resize(normal_0.size());
	for (unsigned int i = 0; i < width.size(); i++)
		width[i] = (float)(normal_0[i]-normal_1[i]).mag();
	
	calculate_length();
}

void ns_worm_shape::calculate_length() const{
		if (length != 0)
			return;
//	  length = 0;
	//  center = ns_vector_2d(0,0);

	  for (unsigned int i = 0; (int)i < (int)nodes.size()-1; i++){
			length+= (float)sqrt((nodes[i] - nodes[i+1]).squared());
//			center+=nodes[i];
	  }
//	  if (nodes.size() != 0)
	//	center+=nodes[nodes.size()-1];
	//  center/=nodes.size();
	  float distance = 0;
	  for (unsigned int i = 0; (int)i < (int)nodes.size()-1; i++){
			distance+= (float)sqrt((nodes[i] - nodes[i+1]).squared());
			if (distance > length/2){
				spine_center = nodes[i];
				spine_center_node_id = i;
				break;
			}
	  }
	}
bool ns_read_csv_value(std:: istream & i, std::string & str){
	char a(0);
	str.resize(0);
	while(true){
		a = i.get();
		if (i.fail())
			return false;
		if (a==',' || a=='\n')
			return true;
		str+=a;
	}
}
bool ns_read_csv_value(std:: istream & i, long & str){
	string a;
	if (!ns_read_csv_value(i,a))
		return false;
	str = atol(a.c_str());
	return true;
}
bool ns_read_csv_value(std:: istream & i, unsigned long  & str){
	string a;
	if (!ns_read_csv_value(i,a))
		return false;
	str = atol(a.c_str());
	return true;
}
bool ns_read_csv_value(std:: istream & i, int  & str){
	string a;
	if (!ns_read_csv_value(i,a))
		return false;
	str = atoi(a.c_str());
	return true;
}
bool ns_read_csv_value(std:: istream & i, double & str){
	string a;
	if (!ns_read_csv_value(i,a))
		return false;
	str = atof(a.c_str());
	return true;
}
bool ns_read_csv_value(std:: istream & i, float & str){
	string a;
	if (!ns_read_csv_value(i,a))
		return false;
	str = atof(a.c_str());
	return true;
}

void ns_worm_shape::read_from_csv(std::istream & in, const unsigned long number_of_nodes){
		width.resize(number_of_nodes);
		nodes.resize(number_of_nodes);
		nodes_unsmoothed.resize(number_of_nodes);
		curvature.resize(number_of_nodes);
		for (unsigned int i = 0; i < number_of_nodes; i++){	
			if (!ns_read_csv_value(in,nodes[i].x)) 
				throw ns_ex("ns_worm_shape::read_from_csv()::Malformed csv file");	
			if (!ns_read_csv_value(in,nodes[i].y)) 
				throw ns_ex("ns_worm_shape::read_from_csv()::Malformed csv file");		
			if (!ns_read_csv_value(in,nodes_unsmoothed[i].x)) 
				throw ns_ex("ns_worm_shape::read_from_csv()::Malformed csv file");	 
			if (!ns_read_csv_value(in,nodes_unsmoothed[i].y)) 
				throw ns_ex("ns_worm_shape::read_from_csv()::Malformed csv file");	
			if (!ns_read_csv_value(in,width[i])) 
				throw ns_ex("ns_worm_shape::read_from_csv()::Malformed csv file");				  
			if (!ns_read_csv_value(in,curvature[i])) 
				throw ns_ex("ns_worm_shape::read_from_csv()::Malformed csv file");			  
		}
		length = 0;
		calculate_length();
}
void ns_worm_shape::read_from_buffer(const float * buffer, const unsigned long number_of_nodes, const unsigned int node_offset){
		width.resize(number_of_nodes);
		nodes.resize(number_of_nodes);
		nodes_unsmoothed.resize(number_of_nodes);
		curvature.resize(number_of_nodes);
		for (unsigned int i = 0; i < number_of_nodes; i++){
			nodes[i].x			  = buffer[6*(node_offset + i)    ];
			nodes[i].y			  = buffer[6*(node_offset + i) + 1];
			nodes_unsmoothed[i].x = buffer[6*(node_offset + i) + 2];
			nodes_unsmoothed[i].y = buffer[6*(node_offset + i) + 3];
			width[i]			  = buffer[6*(node_offset + i) + 4];
			curvature[i]		  = buffer[6*(node_offset + i) + 5];
		}
		length = 0;
		calculate_length();
	}

void ns_worm_shape::write_to_csv(std::ostream & out) const{
	for (unsigned int i = 0; i < nodes.size(); i++){
		out << nodes[i].x << "," << nodes[i].y << ","
			<< nodes_unsmoothed[i].x << "," << nodes_unsmoothed[i].y << ","
			<< width[i] << "," << curvature[i] << ",";
	}
}

void ns_worm_shape::write_to_buffer(float * buffer, const unsigned long node_offset){
	for (unsigned int i = 0; i < nodes.size(); i++){
		buffer[6*(node_offset + i)    ] = (float)nodes[i].x;
		buffer[6*(node_offset + i) + 1] =(float)nodes[i].y;
		buffer[6*(node_offset + i) + 2] = (float)nodes_unsmoothed[i].x;
		buffer[6*(node_offset + i) + 3] =(float)nodes_unsmoothed[i].y;
		buffer[6*(node_offset + i) + 4] = (float)width[i];
		buffer[6*(node_offset + i) + 5] = (float)curvature[i];
	}
}
