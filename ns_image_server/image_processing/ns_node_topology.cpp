#include "ns_image_tools.h"
#include "ns_vector_bitmap_interface.h"
#include "ns_worm_detection_constants.h"
#include "ns_complex_segment_cluster_solver.h"

#include "ns_image_easy_io.h"

using namespace std;


#define ALLOW_ALL_PATHS 0



struct ns_delaunay_mesh_data{
	std::vector<ns_triangle_d> mesh;
	std::vector<char> deleted_triangles;
	int * neighborlist;
	std::vector< ns_edge_2d> edges;
	std::vector<int> neighbor_list;
};

void ns_build_delaunay_mesh(const std::vector<ns_vector_2d> & points, const std::vector<ns_edge_ui> &edge_list, const std::vector<ns_vector_2d> & holes,ns_delaunay_mesh_data & output_data);

ns_node_topology::~ns_node_topology(){
	delete_nodes();
	for (unsigned int i = 0; i < segments.size(); i++)
		delete segments[i];
	for (unsigned int i = 0; i < segment_ends.size(); i++)
		delete segment_ends[i];
}

unsigned int pspsp;
bool ns_node_topology::build_via_delauny_method(const std::vector<ns_vector_2d> & edge_coordinates, const std::vector<ns_edge_ui> & edge_list, const std::vector<ns_vector_2d> & holes, const std::string & debug_output_filename){

	ns_delaunay_mesh_data mesh_data;
	ns_build_delaunay_mesh(edge_coordinates, edge_list, holes, mesh_data);
	for (unsigned int i = 0; i < edge_list.size(); i++){
		if (edge_list[i].vertex[0] >= edge_coordinates.size())
			throw ns_ex("YIKES");
		if (edge_list[i].vertex[1] >= edge_coordinates.size())
			throw ns_ex("YIKES");
}
	//label each node as an island (no connectivity), and endpoint (connected to one neighbor), or a branchpoint (connected to two neighbors).
	populate_nodes_from_mesh(mesh_data.mesh, mesh_data.neighbor_list);

	if (debug_output_filename.size() != 0){
		ofstream post((debug_output_filename + "_raw.txt").c_str());
		if (post.fail())
			throw ns_ex("ns_node_topology::Could not open file \"") << debug_output_filename << "\" for debug spine output.";
		dump_node_graph(post);
		post.close();
	}

	//Multiple triangles in the mesh may posess the same circumcenter,
	//these should be removed as well.
	remove_duplicate_nodes(false);

	sort_node_neighbor_links();	//set up ns_previous, ns_next, and ns_branch references.
							    //note ns_branch contains a node reference only if ns_previous and ns_next also do.
								//ns_next only contains a reference is ns_previous does as well.

	//Many objects shapes, when fed to triangulate(), produce spurious skeleton "arms" containing only a few node
	//Due to extra triangles that do not span the center of the worm
	//these should be removed before doing anything else.
	remove_short_arms(1);
	remove_short_arms(2);
	remove_short_arms(3);
	remove_short_arms(4);

	//Arm removal can produce odd topologies, and place identical nodes next to each other.
	//Go through the skeleton again and remove such spurious instances.
	remove_duplicate_nodes(true);
	sort_node_neighbor_links();	

	remove_short_arms(1);	
	remove_short_arms(2);
	remove_short_arms(3);
	remove_short_arms(4);
	//Now we have removed "noise" from the skeleton, we 
	//go through all nodes and classify them as 
	//segment end points, branch points, or nodes in the middle of a segment
	classify_nodes();			

	cout << "WARNING: ENCOUNTERED A LOW-RESOLUTION IMAGE!\n";

	std::string fn =  debug_output_filename;
	//fn = "c:/deb" + ns_to_string(pspsp);
	//pspsp++;
	if (fn.size() != 0){
		ofstream post((fn + "_poc.txt").c_str());
		if (post.fail())
			throw ns_ex("ns_node_topology::Could not open file \"") << fn << "\" for debug spine output.";
		dump_node_graph(post);
		post.close();
	}

	//now generate the super graph of connected skeleton segment endpoins
	//and assign nodes to their corresponding segment
	generate_segments();

	break_segment_loops();
	
	//check for a variety of topological errors that may have cropped up
	//in processing
	look_for_duplicate_segment_nodes();

	return true;
}

bool ns_node_topology::build_via_zhang_thinning(const ns_image_bitmap & image){

	ns_image_whole<unsigned long> skeleton;
	ns_zhang_thinning(image,skeleton);


		
	ns_image_standard skeleton_2;
	
	/*std::string r = ns_to_string(rand());
	skeleton_2.prepare_to_recieve_image(skeleton.properties());
	for (unsigned int y = 0; y < skeleton.properties().height; y++)
		for (unsigned int x = 0; x < skeleton.properties().width; x++)
			skeleton_2[y][x] = 200*(skeleton[y][x]!=0)+55*(image[y][x]!=0);
	ns_save_image(std::string("c:\\worm_terminal_debug_output\\image_") + r + ".tif",skeleton_2);*/

	for (unsigned int y = 0; y < skeleton.properties().height; y++){
		skeleton[y][0] = 0;
		skeleton[y][skeleton.properties().width-1] = 0;
	}
	for (unsigned int x = 0; x < skeleton.properties().width; x++){
		skeleton[0][x] = 0;
		skeleton[skeleton.properties().height-1][x] = 0;
	}
	unsigned int number_of_nodes = 1;
	//go through all light pixels and give them an id
	//corresponding to their index in this->nodes[]
	for (int y = 1; y < (int)skeleton.properties().height-1; y++){
		for (int x = 1; x < (int)skeleton.properties().width-1; x++){ //all non-zero pixels get a unique id.
				unsigned int t= (skeleton[y][x]!=0);
				skeleton[y][x]= t * number_of_nodes;
				number_of_nodes+=t;
		}
	}

	nodes.resize(number_of_nodes-1,0);
	for (unsigned int i = 0; i < nodes.size(); i++)
		nodes[i] = new ns_node_topology_node;

	//go through each pixel and mark its neighbors
	std::vector<unsigned long> neighbors;
	neighbors.reserve(3);
	for (int y = 1; y < (int)skeleton.properties().height-1; y++){
		for (int x = 1; x < (int)skeleton.properties().width-1; x++){
			unsigned long  &p1 = skeleton[y]  [x]  , &p2 = skeleton[y-1][x]  , &p3 = skeleton[y-1][x+1],
						   &p4 = skeleton[y]  [x+1], &p5 = skeleton[y+1][x+1], &p6 = skeleton[y+1][x]  ,
						   &p7 = skeleton[y+1][x-1], &p8 = skeleton[y]  [x-1], &p9 = skeleton[y-1][x-1];
		
			if (skeleton[y][x] != 0){	
				neighbors.resize(0);
				//a pixel has eight neighbors, but in our triangle-based system
				//nodes only have three.  Fortunatley this tri-connectivity falls out of the 
				//zhang thinning algorithm cleanly.
				//If the local neighborhood is a tic-tac-toe board with the current pixel in the center
				//then neighbors sharing a side are automatically neighbors.
				//neighbors sharing a corner are only pixels if neither of the adjacent pixels sharing a side
				//are neighbors.  This makes better sense drawn out on paper.
				if (p2) neighbors.push_back(p2);
				if (p6)neighbors.push_back(p6);

				if (p4 &&(!p6 || !p5))neighbors.push_back(p4);	//if p1 is the bottom left hand corner of four contiguous pixels (forming a square), don't connect the two bottom ones
				if (p8 &&(!p7 || !p6))neighbors.push_back(p8);  //if p1 is the bottom right hand corner of four contiguous pixels (forming a square), don't connect the two bottom ones
																//this way we prevent making spurious loops
				if (!p2&&p3&&!p4)neighbors.push_back(p3);
				if (!p4&&p5&&!p6)neighbors.push_back(p5);
				if (!p6&&p7&&!p8)neighbors.push_back(p7);
				if (!p8&&p9&&!p2)neighbors.push_back(p9);
				unsigned i;
				for (i = 0; i < neighbors.size(); i++)
					nodes[p1-1]->neighbors[i] = nodes[neighbors[i]-1];//randomly assign each neighbor as next, prev, and branch.  
																  //We'll sort through these later.
				/*for (; i < 3; i++)
					nodes[p1-1]->neighbors[i] = 0;*/

				nodes[p1-1]->coord = ns_vector_2d(x,y);
			}
		}
	}

	

	/*std::string fname = "c:\\worm_terminal_debug_output\\graph" + r + "pre.txt";
	ofstream post(fname.c_str());
	dump_node_graph(post);
	post.close();*/

	//sort_node_neighbor_links();	

	//go through all nodes and classify them as 
	//segment end points, branch points, or nodes in the middle of a segment
	classify_nodes();		



	//cerr << fname << "\n";

	remove_short_arms(1);
	remove_short_arms(2);
	remove_short_arms(3);
	remove_short_arms(4);

	//Arm removal can produce odd topologies, and place identical nodes next to each other.
	//Go through the skeleton again and remove such spurious instances.
	remove_duplicate_nodes(true);
	sort_node_neighbor_links();	

	remove_short_arms(1);	
	remove_short_arms(2);
	remove_short_arms(3);
	remove_short_arms(4);

	//now generate the super graph of connected skeleton segment endpoints
	//and assign nodes to their corresponding segment
	generate_segments();

	/*ofstream post;
	std::string fname = "c:\\tt\\node_graph.txt";
	post.open(fname.c_str());
	dump_node_graph(post);
	post.close();*/

	//look_for_duplicate_segment_nodes();

	//if the analysis produces no valid segments, return false.
	if (segments.size() == 0)
		return false;

	return true;
}


//finds segments who start and end in the same place (the same segment_end node)
//and breaks the loop
//if the loop is connected to another segment, the loop is broken such
//that the angle of meeting between the broken loop and third segment is minimized
void ns_node_topology::break_segment_loops(){
	for (unsigned int i = 0; i < segments.size(); i++){
	
		if (segments[i]->is_a_loop()){
			//cerr << "Found a loop with length" << segments[i]->length << "\n";
			unsigned int break_end;
			//if its a circle, the direction of breakage doesn't matter.
			if (segments[i]->end[0]->neighbor[2] == 0)
				break_end = 0;
			else{
				int end_neighbor_index[3] = {-1,-1, -1};  //the loop has two ends, both connected to the same segment_end object.
													  //this array holds the index in the segment_end object's neighbor array
													  //that corresponds to the segment's end[] array entry
													  //[0] = end[0], [1] = end[1], [2] = the branch segment not part of the loop
				for (unsigned int j = 0; j < 3; j++){
					if (segments[i]->end[0]->neighbor[j] == segments[i])
						end_neighbor_index[segments[i]->end[0]->neighbor_attachment_end[j]] = j;
					if (segments[i]->end[1]->neighbor[j] == segments[i])
						end_neighbor_index[segments[i]->end[1]->neighbor_attachment_end[j]] = j;
					else if (end_neighbor_index[2] == -1) end_neighbor_index[2] = j;
				}
				if (end_neighbor_index[0] == -1 || end_neighbor_index[1] == -1 || end_neighbor_index[2] == -1)
					throw ns_ex("ns_node_topology:: Could not resolve end_neighbor_index during loop breaking!");

				//calculate the slopes of each segment entering the branchpoint
				ns_vector_2d downstream_coord[3];
				double slope_entering_branchpoint[3];
				for (unsigned int j = 0; j < 3; j++){
					downstream_coord[j] = find_downstream_coordinate(*segments[i]->end[0]->neighbor[ end_neighbor_index[j]],
																 segments[i]->end[0]->neighbor_attachment_end[ end_neighbor_index[j]],
																 5);
					ns_vector_2d dif = segments[i]->end[0]->node->coord - downstream_coord[j];
					slope_entering_branchpoint[j] = dif.angle();
				}
				//detach the loop end that departs the most from the incomming slope (makes the tightest bend)
				bool end_to_detach = 0;
				if ( fabs(slope_entering_branchpoint[2] - slope_entering_branchpoint[0]) > fabs(slope_entering_branchpoint[2] - slope_entering_branchpoint[1]))
					end_to_detach = 1;

				ns_node_topology_segment_end * new_end = new ns_node_topology_segment_end;
				segment_ends.push_back(new_end);

				new_end->neighbor[0] = segments[i];	//set new endpoint values
				new_end->neighbor_attachment_end[0] = end_to_detach;
				new_end->node = segments[i]->end[0]->node;
				
				new_end->neighbor[1] = new_end->neighbor[2] = 0;
				new_end->neighbor_attachment_end[1] = new_end->neighbor_attachment_end[2] = 0;

				segments[i]->end[!end_to_detach]->neighbor[ end_neighbor_index[end_to_detach] ] = 0; //update the non-detached endpoint

				segments[i]->end[end_to_detach] = new_end;  //attach the new node to the detached end.

				for (unsigned int j = 0; j < 2; j++)  //rebuild data structures (perhaps not needed)
					for (unsigned int k = 0; k < 3; k++)
						if (segments[i]->end[j]->neighbor[k] != 0)
							segments[i]->end[j]->neighbor[k]->build_neighbor_list();
			}
		}
	}
}

void ns_node_topology::merge_segments(){
	bool segments_changed;
	for (unsigned int i = 0; i < segments.size(); i++){
		//cerr << i << "\n";
		segments_changed = false;

		bool current_is_a_loop = segments[i]->is_a_loop();
		continue;
		//look at both ends of the segment
		for (unsigned end_i = 0; end_i < 2; end_i++){
			double cur_length = segments[i]->max_downstream_length(!end_i);

			

			//if current end of a segment is a branchpoint...
			if (segments[i]->neighbor[2*end_i] != 0 && segments[i]->neighbor[2*end_i+1] != 0){

				//handle special case of loops
				bool n0_is_a_loop = segments[i]->neighbor[2*end_i]->is_a_loop(),
					 n1_is_a_loop = segments[i]->neighbor[2*end_i+1]->is_a_loop();

				if (current_is_a_loop || n0_is_a_loop || n1_is_a_loop){
					
					if (current_is_a_loop && n0_is_a_loop)
						segments[i]->merge_neighbor(2*end_i+1);
					else if (current_is_a_loop && n1_is_a_loop)
						segments[i]->merge_neighbor(2*end_i);
					else if (n0_is_a_loop && n1_is_a_loop){
						unsigned int k;
						for (k = 0; k <= 4; k++){  //find which position the neighbor registers the other neighbor
							if (segments[i]->neighbor[2*end_i] != 0 && segments[i]->neighbor[2*end_i+1] != 0 &&
								segments[i]->neighbor[2*end_i]->neighbor[k] == segments[i]->neighbor[2*end_i+1]){
								segments[i]->neighbor[2*end_i]->merge_neighbor(k);	
								segments_changed = true;
								break;
							}
						}
						if (k == 4)
							throw ns_ex("ns_node_topology::Could not find second largest segment in largest's neighbor list during loop removal.");
					}
					else
						throw ns_ex("ns_node_topology::Only one branch of a branchpoint is a loop.");
				}

				
				double n0_length = segments[i]->neighbor[2*end_i  ]->max_downstream_length(!segments[i]->neighbor_attachment_end[2*end_i  ]),
				       n1_length = segments[i]->neighbor[2*end_i+1]->max_downstream_length(!segments[i]->neighbor_attachment_end[2*end_i+1]);
			
				//the current segment isn't the smallest of the three
				if (cur_length > n0_length || 
					cur_length > n1_length){
					
					//end_i is bigger. merge with it.
					if (n0_length > n1_length)
						segments[i]->merge_neighbor(2*end_i);
					else //end_i+1 is bigger.  merge with it.
						segments[i]->merge_neighbor(2*end_i+1);
				segments_changed = true;
				}
				else{ //the current segment is the smallest.  we need to merge the other two.
					unsigned int k;
					for (k = 0; k <= 4; k++){  //find which position the neighbor registers the other neighbor
						if (segments[i]->neighbor[2*end_i] != 0 && segments[i]->neighbor[2*end_i+1] != 0 &&
							segments[i]->neighbor[2*end_i]->neighbor[k] == segments[i]->neighbor[2*end_i+1]){
							segments[i]->neighbor[2*end_i]->merge_neighbor(k);	
							segments_changed = true;
							break;
						}
					}
					if (k == 4)
						throw ns_ex("Could not find second largest segment in largest's neighbor list during segment merging.");

				}
			}
			
		}
		if (segments_changed)
			//we've modified the segment std::vector.  Start over from the beginning!
				i = 0;
	}

	//remove spurious segments eliminated by the merge
	for (std::vector<ns_node_topology_segment *>::iterator p = segments.begin(); p!= segments.end(); ){
		if ((*p)->end[0] == 0 || (*p)->end[1]==0){
			delete (*p);
			p = segments.erase(p);
		}
		else p++;
	}
}


void ns_node_topology::recurse_segments(ns_node_topology_segment_end *end, const unsigned int neighbor_to_take){
	ns_swap<ns_node_topology_node *> swap;

	unsigned int number_of_start_end_neighbors = 0;
	//the end could potentially have neighbors already, added by the last recursion
	if (end->neighbor[0] != 0){
		number_of_start_end_neighbors++;
		if (end->neighbor[1] != 0) number_of_start_end_neighbors++;
	}

	/*if (number_of_start_end_neighbors > 2)
		throw ns_ex("Overflow in number_of_start_end_neighbors");//xxx

	if (end->node == 0)
		throw ns_ex("End has an invalid node pointer");  //xxx

	 //from the end, build segments in any, extant, yet unprocessed direction
	if (end->node->neighbors[neighbor_to_take] == 0)
		throw ns_ex("Attempting to recurse down null path!");
	*/
	#ifdef SPINE_DEBUG_OUTPUT 
	cerr << "\nLooking from " << end->node << end->node->coord<< "(";
	for (unsigned int l = 0; l < 3; l++)
		cerr << end->node->neighbors[l] << ",";
	cerr << ") toward " << end->node->neighbors[neighbor_to_take] << end->node->neighbors[neighbor_to_take]->coord << "(";
	for (unsigned int l = 0; l < 3; l++)
		cerr << end->node->neighbors[neighbor_to_take]->neighbors[l] << ",";
	cerr << ")";
	#endif

	end->node->polarity_fixed = true;

	//go through and compile the current segment.
	ns_node_topology_node * previous = end->node,
				  * cur = end->node->neighbors[neighbor_to_take];

	//create a new segment and link it to the current endpoint.
	segments.resize(segments.size()+1, new ns_node_topology_segment); 
	ns_node_topology_segment * current_segment = segments[segments.size()-1];
	end->neighbor[number_of_start_end_neighbors] = current_segment;
	end->neighbor_attachment_end[number_of_start_end_neighbors] = 0;
	current_segment->end[0] = end;
	number_of_start_end_neighbors++;


	while(true){
		if (cur == 0)
			throw ns_ex("Walked off the end of node_list!");

		current_segment->length += sqrt((cur->coord - previous->coord).squared());
		//make sure that the current node's "previous" pointer is set correctly
		if ((*cur)(ns_node_topology_node::ns_next) == previous)
			swap((*cur)(ns_node_topology_node::ns_next), (*cur)(ns_node_topology_node::ns_previous));
		else if ((*cur)(ns_node_topology_node::ns_branch) == previous)
			swap((*cur)(ns_node_topology_node::ns_branch), (*cur)(ns_node_topology_node::ns_previous));
		//else if ( (*cur)(ns_node_topology_node::ns_next) == previous || (*cur)(ns_node_topology_node::ns_branch) == previous)
		//	cerr << "\nYikes!\n";
		previous->polarity_fixed = true;  //mark the current node as handled.
		/*#ifdef SPINE_DEBUG_OUTPUT 
		cerr << "Updated " << cur << "(";
		for (unsigned int l = 0; l < 3; l++)
			cerr << cur->neighbors[l] << ",";
		cerr << ")";
		#endif*/
		#ifdef SPINE_DEBUG_OUTPUT 
		cerr << cur->coord << ",";
		#endif
	
		//if we're at a branchpoint or an endpoint, prepare to recurse.
		if ((*cur)(ns_node_topology_node::ns_branch) != 0  || (*cur)(ns_node_topology_node::ns_next) == 0){
			#ifdef SPINE_DEBUG_OUTPUT 
			if ((*cur)(ns_node_topology_node::ns_branch) != 0) cerr << "\nMoving toward a branchpoint.\n";
			#endif
			if ( (*cur)(ns_node_topology_node::ns_branch) != 0 && cur->polarity_fixed){
				#ifdef SPINE_DEBUG_OUTPUT 
				cerr << "We've seen the branchpoint before!\n";
				#endif
				//we've found a loop!  Find the corresponding segment end and link it up
				unsigned int i;
				for (i = 0; i < segment_ends.size(); i++){
					if (segment_ends[i]->node == cur){
						unsigned int j;
						for (j = 0; j < 3; j++){
							if (segment_ends[i]->neighbor[j] == 0){
								segment_ends[i]->neighbor[j] = current_segment;
								segment_ends[i]->neighbor_attachment_end[j] = 1;
								current_segment->end[1] = segment_ends[i];
								//current_segment->build_neighbor_list();
								break;
							}
						}
						if (j == 3)
							throw ns_ex("Found segment_end but all neighbor spots were filled");
						else break;
					}
				}
				if (i == segment_ends.size())
					throw ns_ex("Could not find matching segment end for loop point.");
				cur->polarity_fixed = true;
				break;
			}
			
			segment_ends.resize(segment_ends.size()+1, new ns_node_topology_segment_end);
			ns_node_topology_segment_end * new_end = segment_ends[segment_ends.size()-1];
			new_end->node = cur;
			new_end->neighbor[0] = current_segment;
			current_segment->end[1] = new_end;
			new_end->neighbor_attachment_end[0] = 1;
			
			if ((*cur)(ns_node_topology_node::ns_branch) == 0){  //if we've hit an endpoint, we're done.
				#ifdef SPINE_DEBUG_OUTPUT 
				cerr << "\nHit an endpoint.\n";
				#endif
				cur->polarity_fixed = true;
				break;
			}
			
			for (unsigned int k = 0; k < 3; k++)//if we've hit a branchpoint, recurse down it, but don't head backwards to previous
				if (new_end->node->neighbors[k]!= 0 && new_end->node->neighbors[k] != previous ){
					#ifdef SPINE_DEBUG_OUTPUT 
					cerr << "\nRecursing from " << cur << "\n";
					#endif
					recurse_segments(new_end,k);  
					#ifdef SPINE_DEBUG_OUTPUT 
					cerr << "\nCompleted recursive ofshoot from from " << cur << "\n";
					#endif
				}
				#ifdef SPINE_DEBUG_OUTPUT 
				cerr << "\nFinished All recursion based from " << cur << ".\n";
				#endif
				break;
		}
		else{
			//otherwise, add the current node to the segment and try to continue.
			current_segment->nodes.push_back(cur);
			previous = cur;
			cur = (*cur)(ns_node_topology_node::ns_next);
			
		}
	}
}


void ns_node_topology::look_for_duplicate_segment_nodes(){
	for (std::vector<ns_node_topology_segment *>::iterator p = segments.begin(); p != segments.end(); ){
		if ((*p) == 0){
			cerr << "Uninitialized segment!\n";
			p = segments.erase(p);
			continue;
		}
		if ((*p)->end[0] == 0){
			cerr << "Segment without beginning end!\n";
			//p = segments.erase(p);
			p++;
			continue;
		}			
		if ((*p)->end[1] == 0){
			cerr << "Segment without end end!\n";
			//p = segments.erase(p);
			p++;
			continue;
		}
		if ((*p)->nodes.size() == 0 && (*p)->end[0]->node == (*p)->end[1]->node){
			cerr << "Segment with identical ends!\n";
			p = segments.erase(p);
			//p++;
			continue;
		}
		if ((*p)->nodes.size() != 0 && (*p)->end[0]->node == (*p)->nodes[0])
			cerr <<"Duplicate at beginning\n!!!";
		for (unsigned int j = 1; j < (*p)->nodes.size(); j++)
			if ((*p)->nodes[j-1] == (*p)->nodes[j])
				cerr << "Duplicates at position i" << "\n";
		if ((*p)->nodes.size() != 0 && (*p)->end[1]->node == (*p)->nodes[(*p)->nodes.size()-1])
			cerr << "Duplicate at end\n!!!";
		p++;
	}
}

void ns_node_topology::remove_duplicate_nodes(const bool move_nodes){
	for (unsigned int i = 0; i < nodes.size(); i++){
		//look at all neighbors of the node
		for (unsigned int j = 0; j < 3;){
			if (nodes[i]->neighbors[j] != 0 && nodes[i]->neighbors[j] == nodes[i]){  //remove spurious self references
				nodes[i]->neighbors[j] = 0;
				j++;
				continue;
			}

			//if the neighbor exists and has the same coordinate as this one
			if (nodes[i]->neighbors[j] != 0 && nodes[i]->neighbors[j]->coord == nodes[i]->coord){
				
				ns_node_topology_node * node_to_be_removed = nodes[i]->neighbors[j];
				/*cerr << "Editing " << nodes[i] << ":\n";
				cerr << "\t" << nodes[i] << "(";
				for (unsigned int k = 0; k < 3; k++)
					cerr << nodes[i]->neighbors[k] << ",";
				cerr << ")->" << node_to_be_removed << "(";

				for (unsigned int k = 0; k < 3; k++)
					cerr << node_to_be_removed->neighbors[k] << ",";
				cerr << ")\n";*/
				if (node_to_be_removed->neighbors[ns_node_topology_node::ns_branch] != 0 ){
					if (nodes[i]->neighbors[ns_node_topology_node::ns_branch] == 0){
						j++;
						continue; //if the current node isn't a branch, then the current node will get deleted
						//later on... ie when the neighbor deletes *its* neighbors.
					}

					if (nodes[i] > node_to_be_removed || !move_nodes){  //only move identical neighbors once
						j++;
						continue;
					}
					
					if (node_to_be_removed->neighbors[ns_node_topology_node::ns_branch]->coord == node_to_be_removed->coord){
						//cerr << "\tMultiple, deep, node similarity problems!!!\n\n";
						node_to_be_removed->coord = node_to_be_removed->coord + ns_vector_2d(ns_spine_search_distance_epsilon,ns_spine_search_distance_epsilon)*8;
					}
					else{
						//cerr << "\tMoving halfway.";
						node_to_be_removed->coord = (node_to_be_removed->coord + node_to_be_removed->neighbors[ns_node_topology_node::ns_branch]->coord)/2;
					}
				}
				else{
					
					//the neighbor has two neighbors, find the far neighbor.

					//cerr << "Found duplicate, neighboring nodes:";
					ns_node_topology_node * node_being_added = node_to_be_removed->neighbors[ns_node_topology_node::ns_next];
					if (node_to_be_removed->neighbors[ns_node_topology_node::ns_next] == nodes[i])
						node_being_added = node_to_be_removed->neighbors[ns_node_topology_node::ns_previous];

					//link to neighbor of node_to_be_removed
					nodes[i]->neighbors[j] = node_being_added;

					if (node_being_added != 0){
					/*	cerr <<"->" <<  node_being_added << "(";
						for (unsigned int k = 0; k < 3; k++)
							cerr <<  node_being_added->neighbors[k] << ",";
						cerr << ")\n";*/
						//update back reference from node being conneted to
						bool found = false;
						unsigned int k;
						for (k = 0; k < 3; k++){
							if (node_being_added->neighbors[k] == node_to_be_removed){
								node_being_added->neighbors[k] = nodes[i];
								found = true;
							}
						}			

						if (!found)
							throw ns_ex("ns_node_topology::Duplicate node removal: Could not find neighbor of neighbor!");

					}
					else{
						//cerr << "No Neighbor\n";
						if (j == ns_node_topology_node::ns_previous){  //if we're effectively removing a neighbor, we must make sure to fill ns_previous with the remaining link
							nodes[i]->neighbors[ns_node_topology_node::ns_previous] = nodes[i]->neighbors[ns_node_topology_node::ns_next];
							nodes[i]->neighbors[ns_node_topology_node::ns_next] = 0;
							if (nodes[i]->neighbors[ns_node_topology_node::ns_previous] == 0){
								nodes[i]->neighbors[ns_node_topology_node::ns_previous] = nodes[i]->neighbors[ns_node_topology_node::ns_branch];
								nodes[i]->neighbors[ns_node_topology_node::ns_branch] = 0;
							}
						}
						j++;
					}



					//update the removed node
					node_to_be_removed->neighbors[ns_node_topology_node::ns_previous] = 0;
					node_to_be_removed->neighbors[ns_node_topology_node::ns_next] = 0;
					node_to_be_removed->neighbors[ns_node_topology_node::ns_branch] = 0;

					//cerr << "Done.";
					//link neighbor of node_to_be_removed back to this one.
				}
				/*cerr << "\tResult:" << nodes[i] << "(";
				for (unsigned int k = 0; k < 3; k++)
					cerr << nodes[i]->neighbors[k] << ",";
				cerr << ")->" << node_to_be_removed << "(";

				for (unsigned int k = 0; k < 3; k++)
					cerr << nodes[i]->neighbors[j]->neighbors[k] << ",";
				cerr << ")\n";*/
			}
			else j++;
			
		/*	unsigned int number_of_neighbors = 0;
			for (unsigned int k = 0; k < 3; k++){
				if (nodes[i]->neighbors[k] != 0) 
					number_of_neighbors++;
			}

			if(number_of_neighbors > 0 && nodes[i]->neighbors[ns_node_topology_node::ns_previous] == 0){
				
				if (nodes[i]->neighbors[ns_node_topology_node::ns_branch] != 0)
					nodes[i]->neighbors[ns_node_topology_node::ns_previous] = nodes[i]->neighbors[ns_node_topology_node::ns_branch];
				else if (nodes[i]->neighbors[ns_node_topology_node::ns_next] != 0)
					nodes[i]->neighbors[ns_node_topology_node::ns_previous] = nodes[i]->neighbors[ns_node_topology_node::ns_next];
				else throw ns_ex("number_of_neighbors = ") << number_of_neighbors << " and ns_previous == ns_next ==  ns_branch == 0";
			}
			if (number_of_neighbors > 1 && nodes[i]->neighbors[ns_node_topology_node::ns_next] == 0){
				if (nodes[i]->neighbors[ns_node_topology_node::ns_branch] != 0)
					nodes[i]->neighbors[ns_node_topology_node::ns_next] = nodes[i]->neighbors[ns_node_topology_node::ns_branch];
				else throw ns_ex("number_of_neighbors = ") << number_of_neighbors << " and ns_next == ns_branch == 0";
			}*/
		}
	}
}

void ns_node_topology::delete_nodes(){
	for (unsigned int i = 0; i < nodes.size(); i++){
		delete nodes[i];
		nodes[i] = 0;
	}
}
//fills the ends, islands, and branchpoints arrays from raw mesh data.
//deleted triangles are discarded in the new arrays.
void ns_node_topology::populate_nodes_from_mesh(const std::vector<ns_triangle_d> & mesh, const std::vector<int> & neighborlist){
	delete_nodes();
	unsigned int number_of_nodes = (unsigned int)mesh.size();


	nodes.resize(number_of_nodes,0);

	for (unsigned int i = 0; i < mesh.size(); i++)
		nodes[i] = new ns_node_topology_node;
	unsigned int cur_node = 0;
	//sort through mesh, locating islands, ends, and branchpoints.
	for (unsigned int i = 0; i < mesh.size(); i++){
		//count collect the neighbors
		unsigned int number_of_neighbors = 0;
		for (unsigned int k = 0; k < 3; k++){
			int neighbor_id = neighborlist[3*cur_node+k];
			if (neighbor_id!= -1){
				nodes[cur_node]->neighbors[number_of_neighbors] = nodes[neighbor_id];  //randomly assign each neighbor as next, prev, and branch.  
																				//We'll sort through these later.
				number_of_neighbors++;
			}
		}

		nodes[cur_node]->coord = mesh[i].circumcenter();
		if (nodes[cur_node]->coord.x < 0 || nodes[cur_node]->coord.y < 0 || 
			nodes[cur_node]->coord.x > 1000 || nodes[cur_node]->coord.y > 1000) 
			throw ns_ex("YIKES");
		//the width at this point is the distance between the smallest base
		//of the triangle to its opposite point.
		nodes[cur_node]->width = (float)mesh[i].maximum_height();
		cur_node++;
	}		
}


void ns_node_topology::sort_node_neighbor_links(){

	unsigned char number_of_neighbors;
	for (unsigned int i = 0; i < nodes.size(); i++){
		number_of_neighbors = 0;
		ns_node_topology_node neighbors[3];
		for (unsigned int k = 0; k < 3; k++){
			if (nodes[i]->neighbors[k] != 0) {
				nodes[i]->neighbors[number_of_neighbors] = nodes[i]->neighbors[k];  //shift neighbors into lower positions in the neighbors[] array
				number_of_neighbors++;
			}
		}
		for (unsigned int k = number_of_neighbors; k < 3; k++)
			nodes[i]->neighbors[k] = 0;
	}
}

void ns_node_topology::classify_nodes(){
	for (unsigned int i = 0; i < nodes.size(); i++){
		if (nodes[i]->neighbors[ns_node_topology_node::ns_branch] != 0){
			 branchpoints.push_back(nodes[i]); continue;
		}
		if (nodes[i]->neighbors[ns_node_topology_node::ns_next] == 0 && nodes[i]->neighbors[ns_node_topology_node::ns_previous] != 0){
			ends.push_back(nodes[i]); continue;
		}
	}
}

//should be reimplemented recursively,
void ns_node_topology::remove_short_arms(const unsigned int min_node_length){
	
	//remove arms that contain only one end and one branch,
	//and remove the branch.
	for (std::vector<ns_node_topology_node *>::iterator p = nodes.begin(); p != nodes.end(); p++){
		if ((*(*p))(ns_node_topology_node::ns_next) != 0) continue;  //only look at ends
		
		ns_node_topology_node * neighbor = (*(*p))(ns_node_topology_node::ns_previous);

		if (neighbor == 0)  //ignore nodes that are islands (ie they;ve been deleted by previous remove_short_arm calls.
			continue;

		//if the neighbor has a branch, since the current node is an end
		//we implicitly know the end's segment is only one node long (and thus should be deleted)

		if ( (*neighbor)(ns_node_topology_node::ns_branch) != 0 ){
			//mark the current node as its neigbor's branchpoint,
			//rather than its neighbor's next node or previous node.
			
			//(*p)->width+=(float)sqrt((neighbor->coord - (*p)->coord).squared());
			if ((*p) == (*neighbor)(ns_node_topology_node::ns_previous))
				(*neighbor)(ns_node_topology_node::ns_previous) = (*neighbor)(ns_node_topology_node::ns_branch);
			else if ((*p) == (*neighbor)(ns_node_topology_node::ns_next))
				(*neighbor)(ns_node_topology_node::ns_next) = (*neighbor)(ns_node_topology_node::ns_branch);

//				if ((*neighbor)(ns_node_topology_node::ns_previous) == 0)
//					(*neighbor)(ns_node_topology_node::ns_previous) == (*neighbor)(ns_node_topology_node::ns_next) ;

			//if we are the neighbor's branch, we can just delete it.
			(*neighbor)(ns_node_topology_node::ns_branch) = 0;
			(*(*p))(ns_node_topology_node::ns_previous) = 0;
			continue;
		}
		//remove length 2
		ns_node_topology_node * n2 = (*neighbor)(ns_node_topology_node::ns_previous);
		bool f=false;
		if (n2 == neighbor || n2 == *p || n2 == 0){
			f = true;
			n2 = (*neighbor)(ns_node_topology_node::ns_next);
		}
		if (min_node_length >1 && n2 != 0){
			if ((*n2)(ns_node_topology_node::ns_branch) != 0){
			//	(*p)->width+=(float)sqrt((n2->coord - (*p)->coord).squared());

				if (neighbor == (*n2)(ns_node_topology_node::ns_previous))  //remove link from n2->neighbor
					(*n2)(ns_node_topology_node::ns_previous) = (*n2)(ns_node_topology_node::ns_branch);
				else if (neighbor == (*n2)(ns_node_topology_node::ns_next))
					(*n2)(ns_node_topology_node::ns_next) = (*n2)(ns_node_topology_node::ns_branch);
				//else if (n2 == (*n2)(ns_node_topology_node::ns_branch) all we need to do is delete the branch.
				(*n2)(ns_node_topology_node::ns_branch) = 0;
				

				/*
				//Correctly detach the segment.
				if (f) (*neighbor)(ns_node_topology_node::ns_next) = 0;			//remove link from neighbor->n2
				else{  
					(*neighbor)(ns_node_topology_node::ns_previous) = (*neighbor)(ns_node_topology_node::ns_next);  //If a node only has one neighbor, it should be stored in ns_previous
					(*neighbor)(ns_node_topology_node::ns_next) = 0;
				}*/
				//Dissolve the segment entirely.
				(*p)->neighbors[0] = (*p)->neighbors[1] = (*p)->neighbors[2] = 0;
				neighbor->neighbors[0] = neighbor->neighbors[1] = neighbor->neighbors[2] = 0;
				continue;
	
			}
			//remove length 3;
			ns_node_topology_node * n3 = (*n2)(ns_node_topology_node::ns_previous);
			bool g=false;
			if (n3 == 0 || n3 == neighbor || n3 == n2){
				g = true;
				n3 = (*n2)(ns_node_topology_node::ns_next);
			}
			if (min_node_length >2 &&  n3 != 0){
				//cerr << "l3:";
				if((*n3)(ns_node_topology_node::ns_branch) != 0){
					//(*p)->width+=(float)sqrt((n3->coord - (*p)->coord).squared());
					if (n2 == (*n3)(ns_node_topology_node::ns_previous))
						(*n3)(ns_node_topology_node::ns_previous) = (*n3)(ns_node_topology_node::ns_branch);
					else if (n2 == (*n3)(ns_node_topology_node::ns_next))
						(*n3)(ns_node_topology_node::ns_next) = (*n3)(ns_node_topology_node::ns_branch);
					(*n3)(ns_node_topology_node::ns_branch) = 0;
				/*
				//Correctly detach the segment.
					if (g) (*n2)(ns_node_topology_node::ns_next) = 0;
					else{ 
						(*n2)(ns_node_topology_node::ns_previous) = (*n2)(ns_node_topology_node::ns_next);
						(*n2)(ns_node_topology_node::ns_next) = 0;
					}
				*/
				//Dissolve the segment entirely.
				(*p)->neighbors[0] = (*p)->neighbors[1] = (*p)->neighbors[2] = 0;
				neighbor->neighbors[0] = neighbor->neighbors[1] = neighbor->neighbors[2] = 0;
				n2->neighbors[0] = n2->neighbors[1] = n2->neighbors[2] = 0;
					continue;
				}
					
				//remove length 4;
				ns_node_topology_node * n4 = (*n3)(ns_node_topology_node::ns_previous);
				bool h=false;
				if (n4 == n3 || n4 == n2 || n4 == 0){
					h = true;
					n4 = (*n3)(ns_node_topology_node::ns_next);
				}
				if (min_node_length >3 && n4 != 0){
					
				//	cerr << "l4:";
					if((*n4)(ns_node_topology_node::ns_branch) != 0){
						//(*p)->width+=(float)sqrt((n4->coord - (*p)->coord).squared());
						if (n3 == (*n4)(ns_node_topology_node::ns_previous))
							(*n4)(ns_node_topology_node::ns_previous) = (*n4)(ns_node_topology_node::ns_branch);
						else if (n3 == (*n4)(ns_node_topology_node::ns_next))
							(*n4)(ns_node_topology_node::ns_next) = (*n4)(ns_node_topology_node::ns_branch);
						(*n4)(ns_node_topology_node::ns_branch) = 0;
						/*
						//Correctly detach the segment.
						if (h) (*n3)(ns_node_topology_node::ns_next) = 0;
						else{
							(*n3)(ns_node_topology_node::ns_previous) = (*n3)(ns_node_topology_node::ns_next);
							(*n3)(ns_node_topology_node::ns_next) = 0;
						}
						*/
						//Dissolve the segment entirely.
						(*p)->neighbors[0] = (*p)->neighbors[1] = (*p)->neighbors[2] = 0;
						neighbor->neighbors[0] = neighbor->neighbors[1] = neighbor->neighbors[2] = 0;
						n2->neighbors[0] = n2->neighbors[1] = n2->neighbors[2] = 0;
						n3->neighbors[0] = n3->neighbors[1] = n3->neighbors[2] = 0;
					}
				}
			}
		}
	}
}

//go through all the points and organize them into segments
void ns_node_topology::generate_segments(){
	for (unsigned int i = 0; i < ends.size(); i++)
		if (ends[i]->polarity_fixed)
			throw ns_ex("This node graph has already been calclulated once before--reset all flags before crawling it again!");

	segment_ends.reserve(ends.size());
	for (unsigned int i = 0; i < ends.size(); i++){
		if (!ends[i]->polarity_fixed){
			/*if (ends[i]->neighbors[ns_node_topology_node::ns_previous] == 0){
				throw ns_ex("Unbound end!");
				continue; //XXX this should be an impossible precondition!  Look back and see where such objects are comming from
			}
			if (ends[i]->neighbors[ns_node_topology_node::ns_branch] != 0){
				throw ns_ex("Branching end!");
				continue; //XXX this should be an impossible precondition!  Look back and see where such objects are comming from
			}*/
			segment_ends.resize(segment_ends.size()+1, new ns_node_topology_segment_end);
			ns_node_topology_segment_end * end = segment_ends[segment_ends.size()-1];
			end->node = ends[i];
			if (end->node->neighbors[0]!= 0 && end->node->neighbors[0] != end->node){
				#ifdef SPINE_DEBUG_OUTPUT 
				cerr << "Starting Root Branch " << 0 << " \n";
				#endif
				recurse_segments(end,0);
			}
		}
	}
	//delete spine segments that contain no nodes
	for (std::vector<ns_node_topology_segment *>::iterator p = segments.begin(); p != segments.end();){
//		if ((*p)->nodes.size() == 0)
//			p = segments.erase(p);
//		else{
			(*p)->build_neighbor_list();
			p++;
//		}
	}
}

void ns_node_topology_segment::merge_neighbor(const unsigned int neighbor_id){
	ns_node_topology_segment * output_ptr = neighbor[neighbor_id];

	bool current_connected_at_end0 = (neighbor_id < 2), //true: the neighbor to be merged is comming off of this segment's edge[0]
														//false: the neighbor to be merged is comming off of it's own edge[0]
	neighbor_connected_at_end0 = !neighbor_attachment_end[neighbor_id];



	//Determine connectivity
	
	ns_node_topology_segment_end * far_end,
						 * shared_end;

	if (neighbor_connected_at_end0)
			far_end = neighbor[neighbor_id]->end[1];
	else far_end = neighbor[neighbor_id]->end[0];

	if (current_connected_at_end0)
		shared_end = end[0];
	else shared_end = end[1];

	
	//merge neighbor lists.
	
	unsigned int this_size = static_cast<unsigned int>(nodes.size());
	unsigned int neighbor_size = static_cast<unsigned int>(neighbor[neighbor_id]->nodes.size());
	nodes.resize(nodes.size() + 1 + neighbor[neighbor_id]->nodes.size());

	if (!current_connected_at_end0 && neighbor_connected_at_end0){  //  >this> - end - >neighbor>
		//  node orientation:  this + end + neighbor
		nodes[this_size] = shared_end->node;
		for (unsigned int i = 0; i < neighbor_size; i++)
			nodes[this_size+1+i] = neighbor[neighbor_id]->nodes[i];
	}
	else if (!current_connected_at_end0 && !neighbor_connected_at_end0){  //  >this>  - end - <neighbor<
		//  node orientation:  this + end reverse(neighbor);
		nodes[this_size] = shared_end->node;
		for (unsigned int i = 0; i < neighbor_size ; i++)
			nodes[this_size+1+i] = neighbor[neighbor_id]->nodes[neighbor_size - 1 - i];
	}
	else if (current_connected_at_end0 && neighbor_connected_at_end0){  //  <this< - end - >neighbor>
	//  node orientation:  reverse(neighbor) + end + this
	for (int i = this_size-1; i >= 0 ; i--)
		nodes[neighbor_size + 1 + i] = nodes[i];
	nodes[neighbor_size] = shared_end->node;
	for (unsigned int i = 0; i < neighbor_size ; i++)
		nodes[i] = neighbor[neighbor_id]->nodes[neighbor_size - 1 - i];

	}
	else{	//  <this<  - end -  <neighbor< 
		// node orientation: neighbor + end + this
	for (int i = this_size-1; i >= 0 ; i--)
			nodes[neighbor_size + 1 + i] = nodes[i];
		nodes[neighbor_size] = shared_end->node;
		for (unsigned int i = 0; i < neighbor_size; i++)
			nodes[i] = neighbor[neighbor_id]->nodes[i];
	}

	neighbor[neighbor_id]->nodes.clear();
	
	//Combine lengths
	length += neighbor[neighbor_id]->length;
	neighbor[neighbor_id]->length = 0;
	
	//break off the shared endpoint.  If it has any segments attached, they are now separated!
	unsigned int i;
	for (i = 0; i < 3; i++)
		if (shared_end->neighbor[i] == this || shared_end->neighbor[i] == neighbor[neighbor_id]){
			shared_end->neighbor[i] = 0;
		}
	//if (i == 3) throw ns_ex("Could not find self on shared endpoint!");


	//reassign endpoint of segment being merged.	

	for (i = 0; i < 3; i++){
		//cerr << "Far End Neighbor = " << (unsigned int)far_end->neighbor[i] << ", this = " << (unsigned int)&neighbor[neighbor_id] << "\n";
		if (far_end->neighbor[i] == neighbor[neighbor_id]){
			far_end->neighbor[i] = this;
			break;
		}
	}
	if (i == 3) 
		throw ns_ex("Could not find neighbor on neighbor's endpoint!");

	//now mark the far end as the end of this segment.
	if (neighbor_id < 2)
		end[0] = far_end;
	else end[1] = far_end;

	neighbor[neighbor_id]->end[0] = 0;
	neighbor[neighbor_id]->end[1] = 0;
	neighbor[neighbor_id]->build_neighbor_list();

	//rebuild neighbors for everybody involved.
	for (unsigned int i = 0; i < 3; i++)
		if (far_end->neighbor[i] != 0)
			far_end->neighbor[i]->build_neighbor_list();
	for (unsigned int i = 0; i < 3; i++)
		if (shared_end->neighbor[i] != 0)
			shared_end->neighbor[i]->build_neighbor_list();

}

void ns_node_topology_segment::build_neighbor_list(){
	for (unsigned int i = 0; i < 4; i++)  //clear list
		neighbor[i] = 0;

	for (unsigned int end_i = 0; end_i < 2; end_i++){
		if (end[end_i] == 0)  //if no end is assigned, clear bindings.
			continue;
	
		unsigned int n = 0; //either 0 or 1
		for (unsigned int neighbor_i = 0; neighbor_i < 3; neighbor_i++){
			if (end[end_i]->neighbor[neighbor_i] != 0 && end[end_i]->neighbor[neighbor_i] != this){
				neighbor[2*end_i+n%2] =				   end[end_i]->neighbor[neighbor_i];
				neighbor_attachment_end[2*end_i+n%2] = end[end_i]->neighbor_attachment_end[neighbor_i];
				n++;
			}
		}
	}
}

double ns_node_topology_segment::max_downstream_length(const bool follow_end0){
	double a=0,
		b=0;

	if (neighbor[2*follow_end0] != 0)
		a = neighbor[2*follow_end0]->max_downstream_length(!neighbor_attachment_end[2*follow_end0]);
	if (neighbor[2*follow_end0+1] != 0)
		b = neighbor[2*follow_end0+1]->max_downstream_length(!neighbor_attachment_end[2*follow_end0+1]);

	if (a > b)
		return length + a;
	return length + b;
}




void ns_build_delaunay_mesh(const std::vector<ns_vector_2d> & points, const std::vector<ns_edge_ui> &edge_list, const std::vector<ns_vector_2d> & holes,ns_delaunay_mesh_data & output_data){

	if (points.size() == 0)
		return;
	
	triangulateio input, output;

	input.pointlist = new REAL[points.size()*2];
	for (unsigned int i = 0; i < points.size(); i++){
		input.pointlist[2*i] = points[i].x;
		input.pointlist[2*i+1] = points[i].y;
		//cerr << "(" << points[i].x << "," << points[i].y << ") ";
	}
	input.numberofholes = (int)holes.size();
	if (input.numberofholes != 0){
		input.holelist = new REAL[holes.size()*2];
		for (unsigned int i = 0; i < holes.size(); i++){
			input.holelist[2*i] = holes[i].x;
			input.holelist[2*i+1] = holes[i].y;
		}
	}

	//cerr <<"\n";
	input.numberofpoints = static_cast<unsigned int>(points.size());
	input.numberofpointattributes = 0;
	input.pointmarkerlist = NULL;
	input.pointattributelist = NULL;
	input.numberofsegments= static_cast<int>(edge_list.size());
	input.segmentlist = new int[edge_list.size()*2];
	for (unsigned int i = 0; i < edge_list.size(); i++){
		input.segmentlist[2*i] = edge_list[i].vertex[0];
		input.segmentlist[2*i+1] = edge_list[i].vertex[1];
	}
	input.segmentmarkerlist = NULL;
	//new int[edges.size()];
	//for (unsigned int i = 0; i < edges.size(); i++)
	//	input.segmentmarkerlist[i]=i;

	input.numberofregions = 0;

	if (input.numberofregions != 0)
		input.regionlist = new REAL[input.numberofregions * 4];
	else input.regionlist = NULL;

	for (unsigned int i = 0; i < static_cast<unsigned int>(input.numberofregions); i++){
		input.regionlist[4*i] = 0.5;
		input.regionlist[4*i+1] = 5.0;
		input.regionlist[4*i+2] = 7.0;            /* Regional attribute (for whole mesh). */
		input.regionlist[4*i+3] = 0.1;          /* Area constraint that will not be used. */
	}

	input.numberoftriangles = 0;
	input.trianglelist = NULL;


	output.numberoftriangles = 0;
	output.pointlist = NULL;
	output.pointmarkerlist = NULL;
	output.pointattributelist = NULL;
	output.trianglelist = NULL;
	output.neighborlist = NULL;
	output.segmentlist = NULL;
	output.segmentmarkerlist = NULL;
	output.edgelist = NULL;
	output.edgemarkerlist = NULL;

	//z: start indicies at 0
	//p: use a planar straight line graph (ie use edges)
	//D: generate conforming delaunay mesh (not just constrained delaunay)
	//n: generate neighbors list for each triangle
	//Q: be quiet

	std::string options = "zpDnQ";
	//cerr << "Launching triangulate\n";
	triangulate(options.c_str(),&input,&output,NULL);
	//cerr << "Done.\n";

	output_data.mesh.resize(output.numberoftriangles);
	
	/*double a[3];
	for (unsigned int i = 0; i < output.numberoftriangles; i++){
		a[0] = output.trianglelist[3*i];
		a[1] = output.trianglelist[3*i+1];
		a[2] = output.trianglelist[3*i+2];
	}*/

	for (int i = 0; i < output.numberoftriangles; i++){
		//cerr << "Triangle " << i << " has vertexes: " << output.trianglelist[3*i] << "," << output.trianglelist[3*i+1] << "," << output.trianglelist[3*i+2] <<")\n";
		output_data.mesh[i].vertex[0] = ns_vector_2d(output.pointlist[2*output.trianglelist[3*i  ]   ],
												output.pointlist[2*output.trianglelist[3*i  ] +1]);
		output_data.mesh[i].vertex[1] = ns_vector_2d(output.pointlist[2*output.trianglelist[3*i+1]   ],
												output.pointlist[2*output.trianglelist[3*i+1] +1]);
		output_data.mesh[i].vertex[2] = ns_vector_2d(output.pointlist[2*output.trianglelist[3*i+2]   ],
												output.pointlist[2*output.trianglelist[3*i+2] +1]);
	}

	output_data.edges.resize(edge_list.size());
	for (unsigned int i = 0; i < edge_list.size(); i++)
		output_data.edges[i] = ns_edge_2d(points[edge_list[i].vertex[0]],points[edge_list[i].vertex[1]]);
	if (output.neighborlist != NULL){
		output_data.neighbor_list.resize(3*output.numberoftriangles);
		for (unsigned int i = 0; i < (unsigned int)3*output.numberoftriangles; i++)
			output_data.neighbor_list[i] = output.neighborlist[i];
	}

	if (input.numberofpoints!=0)
		delete input.pointlist;
	if (input.segmentlist!=0)
		delete input.segmentlist;
	if (input.numberofregions != 0)
		delete input.regionlist;
	if (input.numberofholes != 0)
		delete input.holelist;
	
	
	if (output.numberofpoints !=0){
		free(output.pointlist);
		free(output.pointmarkerlist);
	}
	if (output.numberofedges != 0)
		free(output.regionlist);

	if (output.numberofpointattributes !=0)
		free(output.pointattributelist);

	if (output.numberofsegments !=0 || output.segmentlist != NULL)
		free(output.segmentlist);
	if (output.numberofsegments !=0 || output.segmentmarkerlist != NULL)
		free(output.segmentmarkerlist);
	
	if (output.numberoftriangleattributes !=0)
		free(output.triangleattributelist);
	if (output.numberoftriangles !=0 || output.trianglelist != NULL)
		free(output.trianglelist);
	if (output.numberoftriangles !=0 || output.neighborlist != NULL)
		free(output.neighborlist);

}


void ns_node_topology::dump_node_graph(ostream & out) const{
	
	cerr << "DUMPING NODE GRAPH\n";
	out << "graph worm_spine {\n";
	out << "\t resolution = 72\n";
	for (unsigned int i = 0; i < nodes.size(); i++)
		out << "\t\"" << nodes[i] << "\" [label=\"" << nodes[i]->coord << "\"]\n";

	for (unsigned int i = 0; i < nodes.size(); i++){
		for (unsigned int k = 0; k < 3; k++)
			if (nodes[i]->neighbors[k] != 0){
				out << "\t\"" << nodes[i] << "\" -- \"" << nodes[i]->neighbors[k] << "\" [dir=forward, label=";
				switch(k){
					case 0: out << "p"; break;
					case 1: out << "n"; break;
					case 2: out << "b"; break;
				}
				out << "];\n";
				}
		if (nodes[i]->neighbors[0] == 0 && nodes[i]->neighbors[1] == 0 && nodes[i]->neighbors[2] == 0)  //unconnected nodes
			out << "\t\"" << nodes[i] << "\"\n";
	}
	out << "}\n";
}

ns_vector_2d ns_node_topology::find_downstream_coordinate(ns_node_topology_segment & segment, bool start_end, const unsigned int steps_to_take){
	if (segment.nodes.size() == 0){
		if (steps_to_take > 0)
			return segment.end[!start_end]->node->coord;
		else return segment.end[start_end]->node->coord;
	}
	unsigned int i = steps_to_take;
	if (i >= (unsigned int)segment.nodes.size())
		i = (unsigned int)segment.nodes.size()-1;
	if (!start_end)	return segment.nodes[i]->coord;
	else			return segment.nodes[ segment.nodes.size() - i - 1]->coord;
}


void ns_node_topology::remove_empty_segments(){
	for (std::vector<ns_node_topology_segment *>::iterator p = segments.begin(); p!= segments.end(); ){			
			if ((*p)->nodes.size() <= 1){
				delete (*p);
				p = segments.erase(p);
			}
			else p++;
		}
}
void ns_node_topology::remove_small_segments(const double percent_cutoff){
	double max_segment_length = 0;
	for (unsigned int i = 0; i < segments.size(); i++){
		if (segments[i]->length >= max_segment_length)
			max_segment_length = segments[i]->length;
	}

	for (std::vector<ns_node_topology_segment *>::iterator p = segments.begin(); p!= segments.end(); ){			
		if ((*p)->length < percent_cutoff*max_segment_length){
			delete (*p);
			p = segments.erase(p);
		}
		else p++;
	}
}
