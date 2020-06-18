#include "ns_time_path_image_analyzer.h"
#include "ns_graph.h"
#include "ns_xml.h"
#include "ns_image_tools.h"
#include "ctmf.h"
#include "ns_thread_pool.h"
#include "ns_linear_regression_model.h"
#include "ns_annotation_handling_for_visualization.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_image_statistics.h"
#include <queue>

#ifdef NS_CALCULATE_OPTICAL_FLOW
#include "ns_optical_flow.h"

class ns_optical_flow_processor : public ns_optical_flow{};

#endif

#undef NS_DEBUG_FAST_IMAGE_REGISTRATION

using namespace std;

#define NS_MARGIN_BACKGROUND 0
#define NS_DO_SUBPIXEL_REGISTRATION
#define NS_SUBPIXEL_REGISTRATION_CORSE ns_vector_2d(.25,.25)
#define NS_SUBPIXEL_REGISTRATION_FINE ns_vector_2d(.125,.125)

const bool ns_normalize_individual_movement_timeseries_to_median(false);
const bool ns_skip_low_density_paths(false);

//#define NS_OUTPUT_ALGINMENT_DEBUG

ns_lock text_stream_debug_lock("tsdl");
void ns_output_asynchronous_image_loading_debug(ns_text_stream_t& s) {
	//only for debugging purposes
	return;
	/*ns_acquire_lock_for_scope lock(text_stream_debug_lock, __FILE__, __LINE__);
	cerr << ns_thread::current_thread_id() << ": " << s.text() << "\n";
	lock.release();*/
}

template<class allocator_T>
void ns_analyzed_image_time_path::release_images(ns_time_path_image_movement_analysis_memory_pool<allocator_T> & pool) {
	for (unsigned int i = 0; i < elements.size(); i++) {
		elements[i].clear_movement_images(pool);
		elements[i].clear_path_aligned_images(pool);
	}
}
ns_analyzed_image_time_path::~ns_analyzed_image_time_path(){
	movement_image_storage_lock.wait_to_acquire(__FILE__, __LINE__);
	movement_image_storage_lock.release();
		ns_safe_delete(output_reciever);
		ns_safe_delete(flow_output_reciever);
		#ifdef NS_CALCULATE_OPTICAL_FLOW
		ns_safe_delete(flow);
		#endif
	}


class ns_chunk_generator {
public:
	ns_chunk_generator(const unsigned long chunk_size_, const ns_analyzed_image_time_path & path_, const ns_64_bit group_id_) :path(&path_), chunk_size(chunk_size_), current_chunk(0, 0),group_id(group_id_) {}


	void setup_first_chuck_for_forwards_registration() {
		//we don't want to use the frames before stationarity to register, as the worm is most likely not there.
		current_chunk.stop_i = current_chunk.start_i = path->first_stationary_timepoint();
		/*
		//if the path is too small, we try to use the prefix to help with registration.
		//if the path is stil to small, current_chunk.start_i is negative and so no alignment is ever performed.
		if (current_chunk.start_i > -1 && (path->element_count() - current_chunk.start_i) < ns_analyzed_image_time_path::alignment_time_kernel_width) {
			current_chunk.start_i = current_chunk.stop_i = ((long)path->element_count() - ns_analyzed_image_time_path::alignment_time_kernel_width);
		}*/
		first_chunk_ = current_chunk;
		current_chunk.processing_direction = ns_analyzed_time_image_chunk::ns_forward;
	}
	void setup_first_chuck_for_backwards_registration() {
		//nothing to do backwards.
		if (this->path->first_stationary_timepoint() == 0) {
			current_chunk.start_i = current_chunk.stop_i = -1;
		}
		else {
			current_chunk.start_i = current_chunk.stop_i = this->path->first_stationary_timepoint() + ns_analyzed_image_time_path::alignment_time_kernel_width;
			if (current_chunk.start_i >= path->element_count())
				current_chunk.start_i = current_chunk.stop_i = path->element_count()-1;
		}
		//long chunk_length = current_chunk.start_i - current_chunk.stop_i;
		//if there isn't enough data to support the registration,
		//backwards from data collected before the onset of stationarity,
		//buffer it with data collected after the onset stationarity
		//we must be careful later not to output the data after onset of stationarity here.
		/*if (current_chunk.start_i > -1 && current_chunk.start_i < ns_analyzed_image_time_path::alignment_time_kernel_width) {

			current_chunk.start_i = current_chunk.stop_i = (ns_analyzed_image_time_path::alignment_time_kernel_width) - 1;
		}*/
		first_chunk_ = current_chunk;
		current_chunk.processing_direction = ns_analyzed_time_image_chunk::ns_backward;
	}
	///check to see if there is enough path aligned images loaded to support processing a chunk of data.
	//return true if there is and assign the chunk.
	bool update_and_check_for_new_chunk(ns_analyzed_time_image_chunk & new_chunk) {

		current_chunk.processing_direction = ns_analyzed_time_image_chunk::ns_forward;
		for (; current_chunk.stop_i < path->element_count() && path->element(current_chunk.stop_i).path_aligned_image_is_loaded();
			current_chunk.stop_i++);
		const unsigned long cur_size(current_chunk.stop_i - current_chunk.start_i);
		if (cur_size >= chunk_size ||
			(cur_size > 0 && current_chunk.stop_i == path->element_count())) {

			new_chunk = current_chunk;
			current_chunk.start_i = current_chunk.stop_i;
			return true;
		}
		return false;
	}
	///check to see if there is enough path aligned images loaded to support processing a chunk of data.
	//return true if there is and assign the chunk.
	bool backwards_update_and_check_for_new_chunk(ns_analyzed_time_image_chunk & new_chunk) {

		//current_chunk.start_i is the first unprocessed image encountered in previous rounds
		//we push current_chunk.stop_i along until we run out of unprocessed images, 
		//at which point current_chunk.start_i and current_chunk.stop_i now define the range of unprocessed images
		new_chunk.processing_direction = ns_analyzed_time_image_chunk::ns_backward;
		for (; current_chunk.stop_i >= 0 && path->element(current_chunk.stop_i).path_aligned_image_is_loaded(); current_chunk.stop_i--);

		const unsigned long cur_size(current_chunk.start_i - current_chunk.stop_i);
		//if ((this->group_id == 0 || this->group_id == 12) && cur_size != 0)
	//	cout << "g[" << group_id << "]=" << current_chunk.start_i << "," << current_chunk.stop_i;
		if (current_chunk.start_i > -1 &&
			(cur_size >= chunk_size ||
				current_chunk.stop_i == -1)) {
			new_chunk = current_chunk;
			//mark the current chunk (which will be referenced in the next iteration) as starting at the first unprocessed position (the stop of the new chunk)
			current_chunk.start_i = current_chunk.stop_i;
		//	cout << "!\n";
			return true;
		}
	//	if (group_id == 37) {
	//		cout << "[" << current_chunk.start_i << "," << current_chunk.stop_i << "]";
	//	}
		return false;
	}
	bool no_more_chunks_forward(){return current_chunk.stop_i == path->element_count();}
	bool no_more_chunks_backward() { return current_chunk.stop_i == -1; }
	
	const ns_analyzed_time_image_chunk & first_chunk() const {return first_chunk_;}
private:
	unsigned long chunk_size;
	
	ns_analyzed_time_image_chunk current_chunk;
	const ns_analyzed_image_time_path * path;
	ns_analyzed_time_image_chunk first_chunk_;
	const ns_64_bit group_id;

};
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::delete_from_db(const ns_64_bit region_id,ns_sql & sql){
	sql << "DELETE FROM path_data WHERE region_id = " << region_id;
	sql.send_query();
	sql << "UPDATE sample_region_image_info SET path_movement_images_are_cached=0 WHERE id = " << region_id;
	sql.send_query();
}

void ns_movement_posture_visualization_summary::to_xml(std::string & text){
	ns_xml_simple_writer xml;
	xml.add_tag("rid",region_id);
	xml.add_tag("fn",frame_number);
	xml.add_tag("afn",alignment_frame_number);
	for (unsigned int i = 0; i < worms.size(); i++){
		xml.start_group("w");
		xml.add_tag("swp",ns_xml_simple_writer::format_pair(worms[i].worm_in_source_image.position.x,worms[i].worm_in_source_image.position.y));
		xml.add_tag("swz",ns_xml_simple_writer::format_pair(worms[i].worm_in_source_image.size.x,worms[i].worm_in_source_image.size.y));

		xml.add_tag("spp",ns_xml_simple_writer::format_pair(worms[i].path_in_source_image.position.x,worms[i].path_in_source_image.position.y));
		xml.add_tag("spz",ns_xml_simple_writer::format_pair(worms[i].path_in_source_image.size.x,worms[i].path_in_source_image.size.y));

		xml.add_tag("vp",ns_xml_simple_writer::format_pair(worms[i].path_in_visualization.position.x,worms[i].path_in_visualization.position.y));
		xml.add_tag("vz",ns_xml_simple_writer::format_pair(worms[i].path_in_visualization.size.x,worms[i].path_in_visualization.size.y));

		xml.add_tag("mp",ns_xml_simple_writer::format_pair(worms[i].metadata_in_visualizationA.position.x,worms[i].metadata_in_visualizationA.position.y));
		xml.add_tag("mz",ns_xml_simple_writer::format_pair(worms[i].metadata_in_visualizationA.size.x,worms[i].metadata_in_visualizationA.size.y));

		xml.add_tag("pi",worms[i].stationary_path_id.path_id);
		xml.add_tag("gi",worms[i].stationary_path_id.group_id);
		xml.add_tag("gt",worms[i].stationary_path_id.detection_set_id);
		if (worms[i].path_time.period_start_was_not_observed)
			xml.add_tag("ps",-1);
		else xml.add_tag("ps",worms[i].path_time.period_start);
		if (worms[i].path_time.period_end_was_not_observed)
			xml.add_tag("pf",worms[i].path_time.period_end);
		else xml.add_tag("pf",worms[i].path_time.period_end);
		xml.add_tag("tt",worms[i].image_time);
		xml.end_group();
	}
	text = xml.result();
}
inline ns_vector_2i ns_get_integer_pair(const std::string & s){
	std::string::size_type t(s.find(","));
	if (t == std::string::npos)
		throw ns_ex("Could not find pair information in '") << s << "'";
	else return ns_vector_2i(atol(s.substr(0,t).c_str()),(atol(s.substr(t+1,std::string::npos).c_str())));
}
void ns_movement_posture_visualization_summary::from_xml(const std::string & text){
	ns_xml_simple_object_reader o;
	o.from_string(text);
	worms.resize(0);
	if (o.objects.size() == 0) return;
	worms.reserve(o.objects.size()-1);
	frame_number = 0;
	alignment_frame_number = 0;
	try{
		for (unsigned int i = 0; i < o.objects.size(); i++){
			if (o.objects[i].name == "rid"){
				region_id = ns_atoi64(o.objects[i].value.c_str());
				continue;
			}
			if (o.objects[i].name == "fn"){
				frame_number = atol(o.objects[i].value.c_str());
				continue;
			}
			if (o.objects[i].name == "afn"){
				alignment_frame_number = atol(o.objects[i].value.c_str());
				continue;
			}
			else if (o.objects[i].name!= "w")
				throw ns_ex("Unknown posture visualization summary tag: ") << o.objects[i].name;
			string::size_type s = worms.size();
			worms.resize(s+1);
			
			worms[s].worm_in_source_image.position = ns_get_integer_pair(o.objects[i].tag("swp"));
			worms[s].worm_in_source_image.size = ns_get_integer_pair(o.objects[i].tag("swz"));
			worms[s].path_in_source_image.position = ns_get_integer_pair(o.objects[i].tag("spp"));
			worms[s].path_in_source_image.size = ns_get_integer_pair(o.objects[i].tag("spz"));
			worms[s].path_in_visualization.position = ns_get_integer_pair(o.objects[i].tag("vp"));
			worms[s].path_in_visualization.size = ns_get_integer_pair(o.objects[i].tag("vz"));
			worms[s].metadata_in_visualizationA.position = ns_get_integer_pair(o.objects[i].tag("mp"));
			worms[s].metadata_in_visualizationA.size = ns_get_integer_pair(o.objects[i].tag("mz"));
			worms[s].stationary_path_id.path_id = atol(o.objects[i].tag("pi").c_str());
			worms[s].stationary_path_id.group_id = atol(o.objects[i].tag("gi").c_str());
			worms[s].stationary_path_id.detection_set_id = ns_atoi64(o.objects[i].tag("gt").c_str());
			if (o.objects[i].tag("ps") == "-1")
				worms[s].path_time.period_start_was_not_observed = true;
			else 
				worms[s].path_time.period_start = atol(o.objects[i].tag("ps").c_str());
			if (o.objects[i].tag("pf") == "-1")
				worms[s].path_time.period_end= atol(o.objects[i].tag("pf").c_str());
			else worms[s].path_time.period_end= atol(o.objects[i].tag("pf").c_str());
			worms[s].image_time = atol(o.objects[i].tag("tt").c_str());
		}
	}
	catch(ns_ex & ex){
		worms.clear();
		throw ex;
	}
}
ns_64_bit ns_largest_delta_subdivision(const std::vector<ns_64_bit> & v,const int sub_div){
	const unsigned long d_i(ceil(v.size()/(float)sub_div));
	ns_64_bit largest_delta(0);
	unsigned int i;
	for (i = 0; i < v.size(); i+=d_i){
		int s(i+d_i);
		if (i+d_i >= v.size())
			s = v.size()-1;
		if (v[s]-v[i] > largest_delta)
			largest_delta = v[s]-v[i];
	}
	return largest_delta;
}
template<class allocator_T>
ns_64_bit ns_time_path_image_movement_analyzer<allocator_T>::calculate_division_size_that_fits_in_specified_memory_size(const ns_64_bit & mem_size, const int multiplicity_of_images)const{
//	std::vector<ns_64_bit> image_size(groups.size());
	
	//generate the cumulative memory allocation needed to run the entire set;
	ns_64_bit total_area(0);
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path()){	
				continue;
			}
			ns_image_properties p;
			groups[i].paths[j].set_path_alignment_image_dimensions(p);
			total_area += p.width*p.height;
		}
	}
	//divide by two here because its unlikely all paths are simultaneously allocated
	const ns_64_bit total_mem_required_per_chunk(total_area/2);
	const ns_64_bit final_round_id((total_mem_required_per_chunk*(ns_64_bit)multiplicity_of_images)/mem_size);
	if (final_round_id*total_mem_required_per_chunk ==mem_size)
		return final_round_id;
	return final_round_id +1;
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::calculate_memory_pool_maximum_image_size(const unsigned int start_group,const unsigned int stop_group){
	
	bool largest_found(false);
	ns_image_properties largest_image_in_set(0,0,0,0);
	for (unsigned int i = start_group; i < stop_group; i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
				continue;
			ns_image_properties p;
			groups[i].paths[j].set_path_alignment_image_dimensions(p);
			if (!largest_found){
				largest_image_in_set = p;
				largest_found = true;
			}
			else if (largest_image_in_set.width < p.width)
				largest_image_in_set.width  = p.width;
			else if (largest_image_in_set.height < p.height)
				largest_image_in_set.height  = p.height;
		}
	}
	memory_pool.set_overallocation_size(largest_image_in_set);
}
void ns_alignment_state::clear() {
	for (unsigned int y = 0; y < consensus.properties().height; y++) 
		for (unsigned int x = 0; x < consensus.properties().width; x++)
			consensus[y][x] = 0;
	for (unsigned int y = 0; y < consensus_count.properties().height; y++)
		for (unsigned int x = 0; x < consensus_count.properties().width; x++)
			consensus_count[y][x] = 0;
	for (unsigned int y = 0; y < current_round_consensus.properties().height; y++)
		for (unsigned int x = 0; x < current_round_consensus.properties().width; x++)
			current_round_consensus[y][x] = 0;
	//xxx
	//consensus_internal_offset = ns_vector_2i(0, 0);
	registration_offset_sum = ns_vector_2d(0, 0);
	registration_offset_count = 0;
}

#ifdef NS_OUTPUT_ALGINMENT_DEBUG
string debug_path_name;
#endif
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::output_allocation_state(const std::string & stage, long timepoint,std::ostream & out) const {
	for (unsigned int i = 0; i < groups.size(); i++) {
		for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
			for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++) {
				out << stage << "," << timepoint << ","
					<< i << "," << j << "," << k << ","
					<< groups[i].paths[j].elements[k].absolute_time << ","
					<< (groups[i].paths[j].elements[k].path_aligned_image_is_loaded() ? "1" : "0") << ","
					<< (groups[i].paths[j].elements[k].registered_image_is_loaded() ? "1" : "0") << "\n";
			}
		}
	}
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::output_allocation_state_header(std::ostream & out) const {
	out << "stage,timepoint,group,path,element,absolute time,path aligned image loaded, registered image loaded\n";
}


struct ns_movement_analysis_shared_state {
	ns_movement_analysis_shared_state():open_object_count(0),sql_lock("ssl"), deallocated_aligned_count(0),
		registered_image_clear_lag(ns_analyzed_image_time_path::movement_time_kernel_width), 
		sql(0),
		path_aligned_image_clear_lag((ns_analyzed_image_time_path::movement_time_kernel_width > ns_analyzed_image_time_path::alignment_time_kernel_width) ?
			ns_analyzed_image_time_path::movement_time_kernel_width : ns_analyzed_image_time_path::alignment_time_kernel_width){}

	vector<vector<ns_chunk_generator> > chunk_generators;
	vector<vector<ns_alignment_state> > alignment_states;

	//std::vector<int> path_reset;

	unsigned long open_object_count;
	unsigned long deallocated_aligned_count;

	ns_sql * sql;
	ns_lock sql_lock;
	const ns_time_series_denoising_parameters * time_series_denoising_parameters;

	const  long path_aligned_image_clear_lag;
	const  long registered_image_clear_lag;
};



class ns_time_path_image_movement_analyzer_thread_pool_persistant_data {
public:
	ns_time_path_image_movement_analyzer_thread_pool_persistant_data() :
		fast_aligner(ns_analyzed_image_time_path::maximum_alignment_offset(),
			ns_vector_2i(0,0),
			ns_vector_2i(0, 0)) {}
	ns_calc_best_alignment_fast fast_aligner;

	std::vector<ns_image_standard> temporary_image;
	ns_image_bitmap temp_storage;
};

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::run_group_for_current_backwards_round(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data * persistant_data,ns_movement_analysis_shared_state * shared_state) {
	const unsigned int & i(group_id), &j(path_id);
	ns_analyzed_time_image_chunk chunk;

	if (!shared_state->chunk_generators[i][j].backwards_update_and_check_for_new_chunk(chunk)) {
		
		return;
	}
	//if (group_id == 37)
	//	cout << "(" << group_id << ")!";

	shared_state->open_object_count++;
	if (abs(chunk.start_i - chunk.stop_i) != 1)
		throw ns_ex("Running backwards, the chunk size has to be 1, otherwise reversing the order gets complicated!");
	//cerr << "R" << i << "(" << chunk.start_i << "-" << chunk.stop_i << ")";
	groups[i].paths[j].calculate_image_registration(chunk, shared_state->alignment_states[i][j], shared_state->chunk_generators[i][j].first_chunk(), persistant_data->fast_aligner);
	//cerr << "(";

	//in several case we need frames that occur after the onset of stationarity
	//to fill the alignment buffer.  We don't want to output these; we only want
	//to output those that occur /before/ the onset of stationarity.
	//we make that chunk here.
	if (chunk.start_i < groups[i].paths[j].first_stationary_timepoint()) {
		//	cerr << "T" << i << "(" << chunk.start_i << "-" << chunk.stop_i << ")";
		groups[i].paths[j].copy_aligned_path_to_registered_image(chunk, persistant_data->temporary_image,memory_pool);
	}

	//also, we /register/ backwards but calculate movement /forwards/
	//so we need to be one behind in the movement calculation at all times.
	ns_analyzed_time_image_chunk registration_chunk(chunk);
	registration_chunk.start_i += shared_state->registered_image_clear_lag;
	registration_chunk.stop_i += shared_state->registered_image_clear_lag;

	if (registration_chunk.start_i <= groups[i].paths[j].first_stationary_timepoint() - 1) {
		//	cerr << "M" << i << "(" << registration_chunk.start_i << "-" << registration_chunk.stop_i << ")";
		groups[i].paths[j].calculate_movement_images(registration_chunk);
		ns_acquire_lock_for_scope sql_lock(shared_state->sql_lock, __FILE__, __LINE__);
#ifdef NS_CALCULATE_OPTICAL_FLOW
		groups[i].paths[j].save_movement_images(registration_chunk, *shared_state->sql, ns_analyzed_image_time_path::ns_save_both, ns_analyzed_image_time_path::ns_only_output_backwards_images, ns_analyzed_image_time_path::ns_local_0);

#else
		groups[i].paths[j].save_movement_images(registration_chunk, *shared_state->sql, ns_analyzed_image_time_path::ns_save_simple, ns_analyzed_image_time_path::ns_only_output_backwards_images,ns_analyzed_image_time_path::ns_local_0);
#endif
		sql_lock.release();
	}
	//cerr << "Cp" << i << "(" << chunk.start_i + path_aligned_image_clear_lag << "-" << groups[i].paths[j].elements.size() << ")";

	for (long k = chunk.start_i + shared_state->path_aligned_image_clear_lag; k < groups[i].paths[j].elements.size(); k++) {
		if (groups[i].paths[j].elements[k].clear_path_aligned_images(memory_pool.aligned_image_pool))
			shared_state->deallocated_aligned_count++;
	}
	//cerr << "Cr" << i << "(" << chunk.start_i + registered_image_clear_lag << "-" << groups[i].paths[j].elements.size() << ")";
	for (long k = chunk.start_i + shared_state->registered_image_clear_lag; k < groups[i].paths[j].elements.size(); k++)
		groups[i].paths[j].elements[k].clear_movement_images(memory_pool.registered_image_pool);

	if (shared_state->chunk_generators[i][j].no_more_chunks_backward()) {
		//output the last (e.g first)
		ns_analyzed_time_image_chunk registration_chunk;
		registration_chunk.processing_direction = ns_analyzed_time_image_chunk::ns_backward;
		registration_chunk.start_i = 0;
		registration_chunk.stop_i = -1;
		groups[i].paths[j].calculate_movement_images(registration_chunk);
		ns_acquire_lock_for_scope sql_lock(shared_state->sql_lock, __FILE__, __LINE__);
#ifdef NS_CALCULATE_OPTICAL_FLOW
		groups[i].paths[j].save_movement_images(registration_chunk, *shared_state->sql, ns_analyzed_image_time_path::ns_save_both, ns_analyzed_image_time_path::ns_only_output_backwards_images, ns_analyzed_image_time_path::ns_local_0);
		ns_safe_delete(groups[i].paths[j].flow);
#else
		groups[i].paths[j].save_movement_images(registration_chunk, *shared_state->sql, ns_analyzed_image_time_path::ns_save_simple, ns_analyzed_image_time_path::ns_only_output_backwards_images, ns_analyzed_image_time_path::ns_local_0);
#endif
		sql_lock.release();

		//	cerr << "FCp" << i << "(0-" << groups[i].paths[j].elements.size() << ")";
		for (long k = 0; k < groups[i].paths[j].elements.size(); k++) {
			if (groups[i].paths[j].elements[k].clear_path_aligned_images(memory_pool.aligned_image_pool))
				shared_state->deallocated_aligned_count++;
		}
		//cerr << "FCr" << i << "(0-" << groups[i].paths[j].elements.size() << ")";
		for (long k = 0; k < groups[i].paths[j].elements.size(); k++)
			groups[i].paths[j].elements[k].clear_movement_images(memory_pool.registered_image_pool);

		groups[i].paths[j].reset_movement_image_saving();
		//shared_state->path_reset[i]++;
		//if (shared_state->path_reset[i] > 1)
			//cerr << "YIKES!";
	}
}

template<class allocator_T>
using ns_time_path_image_analysis_thread_job_pointer =  void (ns_time_path_image_movement_analyzer<allocator_T>::*)(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data *,ns_movement_analysis_shared_state *);

template<class allocator_T>
struct ns_time_path_image_movement_analyzer_thread_pool_job {
	ns_time_path_image_movement_analyzer_thread_pool_job() {}
	ns_time_path_image_movement_analyzer_thread_pool_job(unsigned long g_id,
		unsigned long p_id,
		ns_time_path_image_analysis_thread_job_pointer<allocator_T> f,
		ns_time_path_image_movement_analyzer<allocator_T> * m,
		ns_movement_analysis_shared_state * ss) :group_id(g_id), path_id(p_id), function_to_call(f), ma(m), shared_state(ss) {}
	unsigned long group_id, path_id;
	ns_time_path_image_analysis_thread_job_pointer<allocator_T> function_to_call;
	ns_time_path_image_movement_analyzer<allocator_T> * ma;
	ns_movement_analysis_shared_state * shared_state;

	void operator()(ns_time_path_image_movement_analyzer_thread_pool_persistant_data & persistant_data){
		(ma->*function_to_call)(group_id, path_id, &persistant_data,shared_state);
	}
};

typedef std::pair<unsigned int, unsigned int> ns_tpiatp_job;

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::finish_up_and_write_to_long_term_storage(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data * persistant_data, ns_movement_analysis_shared_state * shared_state) {

	persistant_data->temp_storage.use_more_memory_to_avoid_reallocations();
	//using the data stored in each path during the previous steps (specifically, when calculate_movement_images() is called
	//for each chunk
	//calculate the stabilized worm neighborhood
	groups[group_id].paths[path_id].calculate_stabilized_worm_neighborhood(persistant_data->temp_storage);
	
	//open the local cached image
	ns_image_storage_source_handle<ns_8_bit> in(image_server_const.image_storage.request_from_local_cache(groups[group_id].paths[path_id].volatile_storage_name(1, false), false));
	groups[group_id].paths[path_id].initialize_movement_image_loading_no_flow(in, false);

	//stream the data to long term storage while setting the stabilized worm neighborhood
	for (unsigned int i = 0; i < groups[group_id].paths[path_id].element_count(); i++) {
		ns_analyzed_time_image_chunk chunk;
		chunk.start_i = i;
		chunk.stop_i = i + 1;
		groups[group_id].paths[path_id].load_movement_images_no_flow(chunk, in, memory_pool);

		//set stabilized worm neighborhood region
		for (unsigned int j = chunk.start_i; j < chunk.stop_i; j++)
			for (unsigned long y = 0; y < persistant_data->temp_storage.properties().height; y++)
				for (unsigned long x = 0; x < persistant_data->temp_storage.properties().width; x++)
					groups[group_id].paths[path_id].elements[j].registered_images->set_just_stabilized_worm_neighborhood_threshold(y, x, persistant_data->temp_storage[y][x]);

		//write to long term storage
		ns_acquire_lock_for_scope lock(shared_state->sql_lock, __FILE__, __LINE__, false);
		bool lock_held(false);
		if (groups[group_id].paths[path_id].output_reciever == 0) { //save_movement_images() only uses sql during first round
			lock.get(__FILE__, __LINE__);
			lock_held = true;
		}
		if (shared_state->sql == 0) throw ns_ex("Found shared state == 0");

		groups[group_id].paths[path_id].save_movement_images(chunk, *shared_state->sql, ns_analyzed_image_time_path::ns_save_simple, ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_long_term);
		if (lock_held)
			lock.release();

		if (!ns_skip_low_density_paths || !groups[group_id].paths[path_id].is_low_density_path())
			groups[group_id].paths[path_id].quantify_movement(chunk);
		int s(chunk.start_i - (long)ns_analyzed_image_time_path::movement_time_kernel_width);
		if (s < 0) s = 0;
		for (long j = s; j < chunk.stop_i- (long)ns_analyzed_image_time_path::movement_time_kernel_width; j++)
			groups[group_id].paths[path_id].elements[j].clear_movement_images(memory_pool.registered_image_pool);
	}

	std::vector<ns_64_bit> tmp1, tmp2;
	groups[group_id].paths[path_id].denoise_movement_series_and_calculate_intensity_slopes(*shared_state->time_series_denoising_parameters,tmp1,tmp2);
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::run_group_for_current_forwards_round(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data * persistant_data,ns_movement_analysis_shared_state * shared_state) {
	const unsigned int &i(group_id), &j(path_id);
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
	if (i != 1)	//xxx
		continue;
#endif

	ns_analyzed_time_image_chunk chunk;
	if (!shared_state->chunk_generators[i][j].update_and_check_for_new_chunk(chunk))
		return;

	//			cerr << "Processing path " << i <<"." << j << ":(" << chunk.start_i << "-" << chunk.stop_i << ")\n";
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
	cerr << "PATH " << i << "," << j << "\n";
	debug_path_name = string("path") + ns_to_string(i) + "_" + ns_to_string(j);
#endif			
	//cerr << "R" << i << "(" << chunk.start_i << "-" << chunk.stop_i << ")";
	groups[i].paths[j].calculate_image_registration(chunk, shared_state->alignment_states[i][j], shared_state->chunk_generators[i][j].first_chunk(), persistant_data->fast_aligner);
	groups[i].paths[j].copy_aligned_path_to_registered_image(chunk, persistant_data->temporary_image,memory_pool);
	groups[i].paths[j].calculate_movement_images(chunk);

	///	cerr << total_flow_time / 1000;
	//	cerr << ")";
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
	for (unsigned int k = chunk.start_i; k < chunk.stop_i; k++) {
		oout << k << "," << groups[i].paths[j].element(k).registration_offset.x << "," <<
			groups[i].paths[j].element(k).registration_offset.y << "," <<
			groups[i].paths[j].element(k).registration_offset.mag() << "\n";
	}
	oout.flush();
#endif
	//groups[i].paths[j].quantify_movement(chunk);
	{
		ns_acquire_lock_for_scope sql_lock(shared_state->sql_lock, __FILE__, __LINE__);
#ifdef NS_CALCULATE_OPTICAL_FLOW
		groups[i].paths[j].save_movement_images(chunk, *shared_state->sql, ns_analyzed_image_time_path::ns_save_both, ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_local_1);
#else
		groups[i].paths[j].save_movement_images(chunk, *shared_state->sql, ns_analyzed_image_time_path::ns_save_simple, ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_local_1);
#endif
		sql_lock.release();
	}
	//cerr << "Cp" << i << "(0-" << chunk.start_i - (long)path_aligned_image_clear_lag+1 << ")";
	//cerr << "Cr" << i << "(0-" << chunk.start_i - (long)registered_image_clear_lag+1 << ")";

	for (long k = 0; k < (long)chunk.start_i - (long)shared_state->path_aligned_image_clear_lag + 1; k++)
		groups[i].paths[j].elements[k].clear_path_aligned_images(memory_pool.aligned_image_pool);
	for (long k = 0; k < (long)chunk.start_i - (long)shared_state->registered_image_clear_lag + 1; k++)
		groups[i].paths[j].elements[k].clear_movement_images(memory_pool.registered_image_pool);

	if (shared_state->chunk_generators[i][j].no_more_chunks_forward()) {

		for (long k = 0; k < groups[i].paths[j].elements.size(); k++) {
			groups[i].paths[j].elements[k].clear_path_aligned_images(memory_pool.aligned_image_pool);
			groups[i].paths[j].elements[k].clear_movement_images(memory_pool.registered_image_pool);
		}
		groups[i].paths[j].reset_movement_image_saving();
	//	shared_state->path_reset[i]++;
	//	if (shared_state->path_reset[i] > 1)
	//		cerr << "YIKES!";
	}
}
template<class allocator_T>
class ns_throw_pool_errors {
public:
	void operator()(
		ns_thread_pool<ns_time_path_image_movement_analyzer_thread_pool_job<allocator_T>,
			ns_time_path_image_movement_analyzer_thread_pool_persistant_data> & thread_pool, ns_sql & sql) {

			ns_time_path_image_movement_analyzer_thread_pool_job<allocator_T> job;
			ns_ex ex;
			bool found_error(false);
			while (true) {
				long errors = thread_pool.get_next_error(job, ex);
				if (errors == 0)
					break;
				found_error = true;
				//register all but the last error
				if (errors > 1) image_server_const.add_subtext_to_current_event(ns_image_server_event(ex.text()) << "\n", &sql);
			}
			//throw the last error
			if (found_error)
				throw ex;
		}
};

void ns_analyzed_image_time_path::calculate_stabilized_worm_neighborhood(ns_image_bitmap & stabilized_neighborhood) {

	unsigned long mmax(0);
	for (unsigned int y = 0; y < stabilized_worm_region_temp.properties().height; y++)
		for (unsigned int x = 0; x < stabilized_worm_region_temp.properties().width; x++)
			if (stabilized_worm_region_temp[y][x] > mmax)
				mmax = stabilized_worm_region_temp[y][x];
	stabilized_neighborhood.init(stabilized_worm_region_temp.properties());
	//set stabilized worm neighborhood mask as all pixels 
	unsigned long cutoff = stabilized_worm_region_total / 10;
	if (cutoff > mmax)
		cutoff = 0;
	for (unsigned int y = 0; y < stabilized_worm_region_temp.properties().height; y++)
		for (unsigned int x = 0; x < stabilized_worm_region_temp.properties().width; x++)
			stabilized_neighborhood[y][x] = stabilized_worm_region_temp[y][x] >= cutoff;
	stabilized_worm_region_temp.clear();
	stabilized_worm_region_total = 0;
}



template<class allocator_T>
bool ns_time_path_image_movement_analyzer<allocator_T>::try_to_rebuild_after_failure() const {return  _number_of_invalid_images_encountered*10 <= number_of_timepoints_in_analysis_; }


template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::process_raw_images(const ns_64_bit region_id,const ns_time_path_solution & solution_, const ns_time_series_denoising_parameters & times_series_denoising_parameters,
																const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql, const long group_number,const bool write_status_to_db,
																const ns_analysis_db_options &id_reanalysis_options){
	if (e->software_version_number() != NS_CURRENT_POSTURE_MODEL_VERSION)
	  throw ns_ex("This software, which is running threshold posture analysis version ") << ns_to_string(NS_CURRENT_POSTURE_MODEL_VERSION) << ", cannot use the incompatible posture analysis parameter set " << e->name << ", which is version " << e->software_version_number();
	region_info_id = region_id; 
	obtain_analysis_id_and_save_movement_data(region_id, sql, id_reanalysis_options,ns_do_not_write_data);
	if (analysis_id == 0)
		throw ns_ex("Could not obtain analysis id!");
	try{
		//std::ofstream debug_out("c:\\server\\path_alignment_debug.csv");
		//output_allocation_state_header(debug_out);
		externally_specified_plate_observation_interval = get_externally_specified_last_measurement(region_id,sql);
		load_from_solution(solution_);
		crop_path_observation_times(externally_specified_plate_observation_interval);

		acquire_region_image_specifications(region_id,sql);
		load_movement_image_db_info(region_id,sql);
		get_output_image_storage_locations(region_id,sql,false);

		unsigned long p(0);
	//	image_server_const.register_server_event(ns_image_server_event("Calculating Movement",false),&sql);
		
		ns_movement_analysis_shared_state shared_state;
		shared_state.sql = &sql;
		shared_state.time_series_denoising_parameters = &times_series_denoising_parameters;

		//we distribute the registration and image analysis tasks across multiple processors
		//to speed movement analysis up.  Doing it this way (within a process) is important because
		//rather than running multiple processes each with their own region to analyze
		//we can limit memory usage to just one region (and miminize paging and disk thrashing)
		ns_thread_pool<ns_time_path_image_movement_analyzer_thread_pool_job<allocator_T>,
					   ns_time_path_image_movement_analyzer_thread_pool_persistant_data> thread_pool;
		thread_pool.set_number_of_threads(image_server_const.maximum_number_of_processing_threads());
		thread_pool.prepare_pool_to_run();

		//xxx debug only
		//shared_state.path_reset.resize(groups.size(), 0);

		//initiate chunk generator and alignment states
		const unsigned long chunk_size(1);//minimize memory use
		
		//if we try to run too many worms simultaneously, we run out of memory on 32bit systems.
		//Most plates have fewer worms than this so usually the processing is done in one step.
		//But in situations with lots of worms we run through the region data files multiple times. 
		//This of course takes a lot longer.
		const unsigned long minimum_chunk_size(ns_analyzed_image_time_path::alignment_time_kernel_width);
		shared_state.chunk_generators.resize(groups.size());
		shared_state.alignment_states.resize(groups.size());
		//total_flow_time = 0;
		long number_of_paths_to_consider(0),
				number_of_paths_to_ignore(0);
		for (unsigned int i = 0; i < groups.size(); i++){
			shared_state.alignment_states[i].resize(groups[i].paths.size());
			shared_state.chunk_generators[i].reserve(groups[i].paths.size());
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (!ns_skip_low_density_paths || !groups[i].paths[j].is_low_density_path())
					number_of_paths_to_consider++;
				else number_of_paths_to_ignore++;
				shared_state.chunk_generators[i].push_back(ns_chunk_generator(chunk_size,groups[i].paths[j],i));
			}
		}
		if (number_of_paths_to_consider == 0)
			image_server_const.add_subtext_to_current_event(ns_image_server_event("No dead animals, or potentially dead animals, were identified in this region.") << "\n",&sql);
		else {
			image_server_const.add_subtext_to_current_event(ns_image_server_event("Registering and Analyzing ") << number_of_paths_to_consider << " objects; discarding " << number_of_paths_to_ignore << " as noise.\n", write_status_to_db ? &sql : 0);

			const bool system_is_64_bit(sizeof(void *) == 8);
			const int number_of_images_stored_in_memory_per_group(
				(1 + 1 + 2 + 4 + 4)*shared_state.registered_image_clear_lag //the registered images; two 8 bt images one 16 and two 32bit
				+
				3 * (2 * shared_state.path_aligned_image_clear_lag) //three chanels of two 8bit images
				+
				1 //intermediate buffers used during saves
			);

			ns_64_bit max_mem_per_node = (((ns_64_bit)image_server_const.maximum_memory_allocation_in_mb()) * 1024 * 1024) /
				image_server_const.maximum_number_of_processing_threads();
			ns_64_bit max_mem_on_32_bit = (((ns_64_bit)1) << 28) * 7;  //1.75GB

			//32 bit systems become unreliable if you allocate > 1.75 GB.
			//Yes you can set flags to get access to 3GB but this is finniky in practice
			//and we err on the side of stability.
			if (!system_is_64_bit && max_mem_per_node > max_mem_on_32_bit)
				max_mem_per_node = max_mem_on_32_bit;

			const ns_64_bit number_of_repeats_required(
				calculate_division_size_that_fits_in_specified_memory_size(
					max_mem_per_node,
					number_of_images_stored_in_memory_per_group));

			if (number_of_repeats_required > number_of_paths_to_consider)
				throw ns_ex("The specified worms are so big that they cannot be processed in memory");
			//const int ((int)(ceil(number_of_paths_to_consider/(float)maximum_number_of_worms_to_process_simultaneously)));
			const int number_of_worms_per_repeat((int)(ceil(number_of_paths_to_consider / (float)number_of_repeats_required)));
			if (number_of_repeats_required > 1)
				image_server_const.add_subtext_to_current_event(ns_image_server_event("To fit everything into memory, we're doing this in ") << number_of_repeats_required << " rounds", write_status_to_db ? &sql : 0);

			int current_round(0);
			for (unsigned int g = 0; g < groups.size(); ) {

				const unsigned long start_group(g);
				unsigned long stop_group;
				{
					int number_to_do_this_round(0);
					bool found_enough(false);
					//find the last worm that 
					for (stop_group = start_group; stop_group < groups.size() && !found_enough; stop_group++) {
						for (unsigned int j = 0; j < groups[stop_group].paths.size(); j++) {
							if (!ns_skip_low_density_paths || !groups[stop_group].paths[j].is_low_density_path()) {
								if (number_to_do_this_round == number_of_worms_per_repeat) {
									found_enough = true;
									stop_group--; //we've gone one too far
									break;
								}
								number_to_do_this_round++;
							}
						}
					}
				}
				if (start_group == stop_group)
					break;
				//cerr << "Running group " << start_group << "," << stop_group << "\n";
				if (number_of_repeats_required > 1)
					image_server_const.add_subtext_to_current_event(ns_image_server_event("Starting Round ") << current_round + 1 << "\n", write_status_to_db ? &sql : 0);

				calculate_memory_pool_maximum_image_size(start_group, stop_group);
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
				ofstream oout("c:\\tst_quant.csv");
				oout << "Frame,Offset X,Offset Y, Offset Magnitude\n";
				oout.flush();
#endif

				unsigned long debug_output_skip(0);

				//first, we register backwards from onset of stationarity
				for (unsigned int i = start_group; i < stop_group; i++) {
					for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
						groups[i].paths[j].find_first_labeled_stationary_timepoint();
						groups[i].paths[j].debug_number_images_written = -1;
						//	if (i == 24)
						//		cerr <<"SHA";
						shared_state.chunk_generators[i][j].setup_first_chuck_for_backwards_registration();
					}
				}

				image_server_const.add_subtext_to_current_event(ns_image_server_event("Running backwards..."), write_status_to_db ? &sql : 0);
				int last_r = 0;
				for (long t = (long)region_image_specifications.size() - 1; t >= 0; t--) {

					//output_allocation_state("bk",region_image_specifications.size() - 1 - t, debug_out);
					//cout << "t=" << t << " ";
					if (debug_output_skip == number_of_repeats_required || t == 0 && current_round == 0) {
						int r( (100 * (region_image_specifications.size() - 1 - t + region_image_specifications.size()*current_round)) / (region_image_specifications.size()*number_of_repeats_required) );
						if (r >= last_r + 5) {
							image_server_const.add_subtext_to_current_event(ns_to_string(r) + "%...", write_status_to_db ? &sql : 0);
							last_r = r;
						}
							debug_output_skip = 0;
					}
					else debug_output_skip++;
					long stop_t(t - (long)chunk_size);
					if (stop_t < -1)
						stop_t = -1;

					unsigned long allocated_count(this->memory_pool.aligned_image_pool.number_of_items_checked_out());

					try {
						//we obtain all the image data from disk that required to run the image analysis for the current chunk.
						//it is loaded into the groups[i].paths[j] data structures.
						//cout << "li(" << stop_t + 1 <<","<< t << ")";
						for (long t1 = stop_t + 1; t1 < t + 1; t1++) {

							//throw ns_ex("test");
							//cout << "Loading " << ns_format_time_string_for_human(region_image_specifications[t1].time) << "-" << ns_format_time_string_for_human(t1 + 1) << "\n";
							load_region_visualization_images(t1, t1 + 1, start_group, stop_group, sql, false, true, ns_analyzed_image_time_path::ns_lrv_flag_and_images);

						}
					}
					catch (ns_ex & ex) {
						//if we find an image error, we go back and go through /all/ images to root out all errors.  
						//Aftwerwards, the _number_of_invalid_images_encountered field can be inspected to see if an attempt to rebuild is worthwhile.
						ns_ex ex_f(ex);
						image_server_const.add_subtext_to_current_event(ns_image_server_event("Found an error; : ") << ex.text(),&sql);
						image_server_const.add_subtext_to_current_event(ns_image_server_event("Now performing a consistancy check on all images in the region.  All images with missing data will be marked with the (problem) flag, viewable in the web interface.  Often, deleting worm detection images and re-running worm detection will resolve this issue."), &sql);
						try {
							load_region_visualization_images(0, region_image_specifications.size(), 0, groups.size(), sql, true, true, ns_analyzed_image_time_path::ns_lrv_flag_and_images);
						}
						catch (ns_ex & ex2) {
							ex_f << ";" << ex2.text();
						}
						throw ex_f;
					}

					//now we have all the relevant image data loaded into path data structures for
					//the time points specified in the current chunk.
					//So, we can run image registration and analysis for all these images.

					//Note that each path is individually asked to figure out which data has been loaded and is ready
					//to analyze; we don't have to explicitly specify the job.
					for (unsigned int i = start_group; i < stop_group; i++) {
						for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
							if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
								continue;
							//this is a very paralellizeable step; we schedule each path to be analyzed individually and run them 
							//on all cores.
							thread_pool.add_job_while_pool_is_not_running(
								ns_time_path_image_movement_analyzer_thread_pool_job<allocator_T>(i, j,
									&ns_time_path_image_movement_analyzer<allocator_T>::run_group_for_current_backwards_round,
									this, &shared_state));

							//	run_group_for_current_backwards_round(i, j, &shared_state);


						}
						//	cerr << "\n";
					}

					thread_pool.run_pool();
					//ok!  here all the worker threads start handling all the schedule tasks.
					//this main thread blocks until the worker threads are all done
					thread_pool.wait_for_all_threads_to_become_idle();
					//now that all the worker threads are done, this thread holds the thread pool job lock again.
					//we keep it until we need to run more jobs in the next round.
					
					ns_throw_pool_errors<allocator_T> throw_errors;
					throw_errors(thread_pool, sql);
				}
				ns_ex still_open_errors("Paths still open: ");
				bool paths_still_open(false);
				for (unsigned int i = start_group; i < stop_group; i++) {
					for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
						if (groups[i].paths[j].output_reciever != 0) {
							paths_still_open = true;
							still_open_errors << i << ", ";
						}
					}
				}
				still_open_errors << "\n: ";
				for (unsigned int i = start_group; i < stop_group; i++) {
					for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
					
						if (groups[i].paths[j].debug_number_images_written != groups[i].paths[j].first_stationary_timepoint()) {
							if (groups[i].paths[j].debug_number_images_written != -1) {
								paths_still_open = true;
								still_open_errors << "Incorrect output length: group " << i << "(" << groups[i].paths[j].debug_number_images_written << "," << groups[i].paths[j].first_stationary_timepoint() << "),";

							}
							else {
								groups[i].paths[j].find_first_labeled_stationary_timepoint();
								if (groups[i].paths[j].first_stationary_timepoint_ != 0)
									image_server_const.add_subtext_to_current_event(std::string("Group ") + ns_to_string(i) + " was never opened.\n",&sql);
							//	cout << "defined across" << ns_format_time_string_for_human(groups[i].paths[j].elements[0].absolute_time) << "-" << ns_format_time_string_for_human(groups[i].paths[j].elements[groups[i].paths[j].first_stationary_timepoint()].absolute_time) << "\n";

							}
						}
					}
				}
				if (paths_still_open) {
					cerr << still_open_errors.text();
					throw still_open_errors;
				}

				//since we have been registered images backwards in time, we've been writing everything in reverse order to the local disk.
				//So, now we need to reload everything back in, reverse the order so that the earliest frame is first, and then 
				//write it all out to long term storage. 
				//image_server_const.add_subtext_to_current_event("\n", (write_status_to_db ? (&sql) : 0));
				//image_server_const.add_subtext_to_current_event(ns_image_server_event("Reversing backwards image..."), write_status_to_db ? &sql : 0);
				unsigned long debug_count(0);
				for (unsigned int i = start_group; i < stop_group; i++) {
					for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
					//	if (i % 5 == 0)
						//	image_server_const.add_subtext_to_current_event(ns_to_string((100 * i) / (stop_group - start_group)) + "%...", write_status_to_db ? &sql : 0);
						//output_allocation_state("rev", debug_count, debug_out);
						debug_count++;
						if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
							continue;
						//no backwards points to load!
						if (groups[i].paths[j].first_stationary_timepoint() == 0)
							continue;

						ns_image_storage_source_handle<ns_8_bit> storage(image_server_const.image_storage.request_from_local_cache(groups[i].paths[j].volatile_storage_name(0,false), false));
#ifdef NS_CALCULATE_OPTICAL_FLOW
						ns_image_storage_source_handle<float> flow_storage(image_server_const.image_storage.request_from_local_cache_float(groups[i].paths[j].volatile_storage_name(true), false));
						groups[i].paths[j].initialize_movement_image_loading(storage, flow_storage, true);
#else
						groups[i].paths[j].initialize_movement_image_loading_no_flow(storage, true);
#endif

						ns_analyzed_time_image_chunk chunk;
						chunk.direction = ns_analyzed_time_image_chunk::ns_forward;
						chunk.start_i = 0;
						chunk.stop_i = groups[i].paths[j].first_stationary_timepoint();
#ifdef NS_CALCULATE_OPTICAL_FLOW
						groups[i].paths[j].load_movement_images(chunk, storage, flow_storage);
#else
						groups[i].paths[j].load_movement_images_no_flow(chunk, storage,memory_pool);
#endif
						groups[i].paths[j].end_movement_image_loading();
						image_server_const.image_storage.delete_from_local_cache(groups[i].paths[j].volatile_storage_name(0,true));
						image_server_const.image_storage.delete_from_local_cache(groups[i].paths[j].volatile_storage_name(0,false));

						//reverse order;
						ns_registered_image_set * tmp;
						for (unsigned int k = 0; k < groups[i].paths[j].first_stationary_timepoint() / 2; k++) {
							tmp = groups[i].paths[j].elements[k].registered_images;
							groups[i].paths[j].elements[k].registered_images =
								groups[i].paths[j].elements[groups[i].paths[j].first_stationary_timepoint() - k - 1].registered_images;
							groups[i].paths[j].elements[groups[i].paths[j].first_stationary_timepoint() - k - 1].registered_images = tmp;
						}
#ifdef NS_CALCULATE_OPTICAL_FLOW
						groups[i].paths[j].save_movement_images(chunk, sql, ns_analyzed_image_time_path::ns_save_both, false);
#else
						groups[i].paths[j].save_movement_images(chunk, sql, ns_analyzed_image_time_path::ns_save_simple,ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_local_1);
#endif
						//we leave the image saving buffers open as we'll continue writing to them while moving forward.
	//					groups[i].paths[j].reset_movement_image_saving();

						//deallocate all before first time point.
						for (long k = 0; k <= groups[i].paths[j].first_stationary_timepoint(); k++) {
							groups[i].paths[j].elements[k].clear_movement_images(memory_pool.registered_image_pool);
						}
					}
				}


				debug_output_skip = 0;

				//OK; we've finished the backwards registration.
				//Now we go ahead and work forwards.
				//Running forwards is done essentially in the same way as running backwards
				//we're again caching to disk because we need to load everything in one final time
				//to calculate the stablizized worm region

			//	for (unsigned int i = 0; i < groups.size(); i++)
			//		shared_state.path_reset[i] = 0;

				for (unsigned int i = start_group; i < stop_group; i++) {
					for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
						shared_state.alignment_states[i][j].clear();
						shared_state.chunk_generators[i][j].setup_first_chuck_for_forwards_registration();
					}
				}
				image_server_const.add_subtext_to_current_event("\n", (write_status_to_db ? (&sql) : 0));

				image_server_const.add_subtext_to_current_event(ns_image_server_event("Running forwards..."), (write_status_to_db ? (&sql) : 0));

				last_r = 0;
				for (unsigned int t = 0; t < region_image_specifications.size(); t += chunk_size) {
					if (debug_output_skip == number_of_repeats_required || t == 0 && current_round == 0) {
						int r = (100 * (t + region_image_specifications.size()*current_round)) / (region_image_specifications.size()*number_of_repeats_required);
						//if (r > 100)
						//	cerr << "YIKES!";
						if (r >= last_r + 5) {
							image_server_const.add_subtext_to_current_event(ns_to_string(r) + "%...", write_status_to_db ? (&sql) : 0);
							last_r = r;
						}
						debug_output_skip = 0;
					}
					else debug_output_skip++;


					//load a chunk of images
					unsigned long stop_t = t + chunk_size;
					if (stop_t > region_image_specifications.size())
						stop_t = region_image_specifications.size();
					//	cerr << "Loading images " << t << "-" << stop_t << "\n";
					try {
						//load in image data for the current chunk
						load_region_visualization_images(t, stop_t, start_group, stop_group, sql, false, false, ns_analyzed_image_time_path::ns_lrv_flag_and_images);
					}
					catch (ns_ex & ex) {
						ns_ex ex_f(ex);
						image_server_const.add_subtext_to_current_event(ns_image_server_event("Found an error; doing a consistancy check on all images in the region: ") << ex.text() << "\n",&sql);
						try {
							load_region_visualization_images(0, region_image_specifications.size(), 0, groups.size(), sql, true, false, ns_analyzed_image_time_path::ns_lrv_flag_and_images);
						}
						catch (ns_ex & ex2) {
							ex_f << ";" << ex2.text();
						}
						throw ex_f;
					}

					//distribute the image registration and analysis across multiple cores
					for (unsigned int i = start_group; i < stop_group; i++) {
						for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
							if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
								continue;

							thread_pool.add_job_while_pool_is_not_running(
								ns_time_path_image_movement_analyzer_thread_pool_job<allocator_T>(i, j,
									&ns_time_path_image_movement_analyzer<allocator_T>::run_group_for_current_forwards_round,
									this, &shared_state));
							//run_group_for_current_backwards_round(i, j, &shared_state);
						}
					}

					thread_pool.run_pool();
					//ok!  here all the worker threads start handling all the schedule tasks.
					//this main thread blocks until the worker threads are all done
					thread_pool.wait_for_all_threads_to_become_idle();
					//now that all the worker threads are done, this thread holds the thread pool job lock again
					//and we keep it until we need to run more jobs, below.
					ns_throw_pool_errors<allocator_T> throw_errors;
					throw_errors(thread_pool, sql);
				}


				//ok! We've finished all the computationally expensive image registration and image analysis. 
				//Now we calculate some statistics and then we're done.
				image_loading_temp.use_more_memory_to_avoid_reallocations(false);
				image_loading_temp.clear();

				memory_pool.clear();
				g = stop_group;
				current_round++;
			}
		}
		thread_pool.shutdown();


		//now we have calculated all the movement images
		//and we have all the stabilized region data stored as well
		if (number_of_paths_to_consider > 0) {
			image_server_const.add_subtext_to_current_event("\n", (write_status_to_db ? (&sql) : 0));
			image_server_const.add_subtext_to_current_event(ns_image_server_event("Stabilizing regions of focus..."), (write_status_to_db ? (&sql) : 0));

			thread_pool.set_number_of_threads(image_server_const.maximum_number_of_processing_threads());
			thread_pool.prepare_pool_to_run();

			//we just need to do a few final calculations and then copy everything to long term storage
			for (unsigned int i = 0; i < groups.size(); i++) {
				for (unsigned int j = 0; j < groups[i].paths.size(); j++) {

					thread_pool.add_job_while_pool_is_not_running(
						ns_time_path_image_movement_analyzer_thread_pool_job<allocator_T>(i, j,
							&ns_time_path_image_movement_analyzer<allocator_T>::finish_up_and_write_to_long_term_storage,
							this, &shared_state));
				}
			}

			thread_pool.run_pool();

			//output progres information
			if (groups.size() > 0 && write_status_to_db) {
				int last_p(0);
				while (true) {
					const int n = thread_pool.number_of_jobs_pending();
					const int p = (100 * (groups.size() - n)) / groups.size();
					if (p - last_p >= 5) {
						ns_acquire_lock_for_scope lock(shared_state.sql_lock, __FILE__, __LINE__);
						image_server_const.add_subtext_to_current_event(ns_to_string(p) + "%...", (write_status_to_db ? (&sql) : 0));
						lock.release();
						last_p = p;
					}
					if (n == 0)
						break;
				}
				ns_thread::sleep(3);
			}

			thread_pool.wait_for_all_threads_to_become_idle();
			ns_throw_pool_errors<allocator_T> throw_errors;
			throw_errors(thread_pool, sql);
			thread_pool.shutdown();

			//OK! Now we have /everything/ finished with the images.
			//calculate some final stats and then we're done.
			normalize_movement_scores_over_all_paths(e->software_version_number(),times_series_denoising_parameters,sql);
			//xxx this could be paraellelized if it was worth it.
			for (unsigned int i = 0; i < groups.size(); i++){
				for (unsigned int j = 0; j < groups[i].paths.size(); j++){
							if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
									continue;
					//THIS IS WHERE QUANTIFICATION IS ANALYZED AND DEATH TIMES ARE IDENTIFIED AND ANNOTATIONS ARE GENERATED
					groups[i].paths[j].analyze_movement(e,ns_stationary_path_id(i,j,analysis_id),last_timepoint_in_analysis_);
					groups[i].paths[j].calculate_movement_quantification_summary(groups[i].paths[j].movement_analysis_result);
				}
			}

			//ofstream oo("c:\\out.csv");
			unsigned long total_groups(0);
			unsigned long total_skipped(0);
			for (unsigned int i = 0; i < groups.size(); i++){
				for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				//	for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++)
				//		oo << i << "," << j << "," << k << "," << (groups[i].paths[j].elements[k].element_was_processed?1:0) << "\n";
					const unsigned long c(groups[i].paths[j].number_of_elements_not_processed_correctly());
					//if (c > 0){
					//}
					total_groups+=(c>0)?1:0;
					total_skipped+=c;
				}
			}

			if (total_skipped > 0){
				throw ns_ex("") << total_skipped << " frames were missed among " << total_groups << " out of a total of " << groups.size() << " path groups.";
			}
		}
		generate_movement_description_series();


		mark_path_images_as_cached_in_db(region_id,sql);
		image_server_const.add_subtext_to_current_event("\n", (write_status_to_db ? (&sql) : 0));
		image_server_const.add_subtext_to_current_event(ns_image_server_event("Finished."), (write_status_to_db ? (&sql) : 0));

	}
	catch(...){
		delete_from_db(region_id,sql);
		throw;
	}
	if (write_status_to_db)
			image_server_const.add_subtext_to_current_event(ns_image_server_event("Done."),&sql);
	else cerr << "Done.\n";
}

unsigned long ns_analyzed_image_time_path::number_of_elements_not_processed_correctly() const{
	unsigned long not_processed_correctly(0);
	for (unsigned int i = 0; i < elements.size(); i++){
		if (!elements[i].excluded && !elements[i].element_was_processed)
			not_processed_correctly++;
	}
	return not_processed_correctly;
}
	
/*void ns_analyzed_image_time_path::out_histograms(std::ostream & o) const{
	o << "Value,Stationary Count, Movement Count\n";
	for (unsigned long i = 0; i < 256; i++)
		o << i << "," << (unsigned long)stationary_histogram[i] << "," << (unsigned long)movement_histogram[i] << "\n";
}*/

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::load_from_solution(const ns_time_path_solution & solution_, const long group_number){
	solution = &solution_;
	for (unsigned int i = 0; i < groups.size(); i++)
		groups[i].clear_images(memory_pool);
	groups.clear();
	extra_non_path_events.clear();
	_number_of_invalid_images_encountered = 0;
	if (region_info_id == 0)
		throw ns_ex("load_from_solution()::No Region ID Specified!");
	
	this->image_db_info_loaded = false;

	for (unsigned int i = 0; i < solution_.timepoints.size(); i++){
		if (solution_.timepoints[i].time > last_timepoint_in_analysis_)
			last_timepoint_in_analysis_ = solution_.timepoints[i].time;
	}
	number_of_timepoints_in_analysis_ = solution_.timepoints.size();
	groups.reserve(solution_.path_groups.size());
	if (group_number != -1){
		groups.emplace_back(ns_stationary_path_id(group_number,0,analysis_id),region_info_id,solution_,externally_specified_plate_observation_interval,extra_non_path_events,memory_pool);
		for (unsigned int i = 0; i < groups.rbegin()->paths.size(); i++){
			if (groups.rbegin()->paths[i].elements.size() < ns_analyzed_image_time_path::alignment_time_kernel_width)
				throw ns_ex("ns_time_path_image_movement_analyzer<allocator_T>::load_from_solution::Path loaded that is too short.");
		}
		if (groups.rbegin()->paths.size() == 0)
			groups.pop_back();
	}
	else{
		for (unsigned int i = 0; i < solution_.path_groups.size(); i++){
			
			groups.emplace_back(ns_stationary_path_id(i, 0, analysis_id),region_info_id,solution_,externally_specified_plate_observation_interval,extra_non_path_events,memory_pool);
			for (unsigned int j = 0; j < groups.rbegin()->paths.size(); j++){
				if (groups.rbegin()->paths[j].elements.size() < ns_analyzed_image_time_path::alignment_time_kernel_width)
					throw ns_ex("ns_time_path_image_movement_analyzer<allocator_T>::load_from_solution::Path loaded that is too short.");
			}
			if (groups.rbegin()->paths.size() == 0)
				groups.pop_back();
		}
	}
	if (solution_.timepoints.size() == 0)
		return;

	//add annotations for fast moving animals
	unsigned long last_time = solution_.timepoints.rbegin()->time;
	const unsigned long current_time(ns_current_time());
	for (unsigned int i = 0; i < solution_.unassigned_points.stationary_elements.size(); i++){
		const ns_time_path_element &e(solution_.element(solution_.unassigned_points.stationary_elements[i]));
		std::string expl("NP");
		ns_death_time_annotation::ns_exclusion_type excluded(ns_death_time_annotation::ns_not_excluded);
		if (e.low_temporal_resolution)
			excluded = ns_death_time_annotation::ns_machine_excluded;  //animals marked as low temperal resolution
																	   //should be excluded and not used
																	   //in censoring calculations
		extra_non_path_events.add(
				ns_death_time_annotation(ns_fast_moving_worm_observed,
				0,region_info_id,
				ns_death_time_annotation_time_interval(solution_.time(solution_.unassigned_points.stationary_elements[i]),solution_.time(solution_.unassigned_points.stationary_elements[i])),
				e.region_position,
				e.region_size,
				excluded,
				ns_death_time_annotation_event_count(1+e.number_of_extra_worms_identified_at_location,0),
				current_time,ns_death_time_annotation::ns_lifespan_machine,
				(e.part_of_a_multiple_worm_disambiguation_cluster)?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
				ns_stationary_path_id(0,0,analysis_id),false,e.inferred_animal_location, e.subregion_info, ns_death_time_annotation::ns_explicitly_observed,
				expl)
		);
	}

	
	get_processing_stats_from_solution(solution_);
}


template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::get_output_image_storage_locations(const ns_64_bit region_id,ns_sql & sql,const bool create_only_flow){

	string region_name,sample_name,experiment_name;
	ns_64_bit sample_id,experiment_id;
	ns_region_info_lookup::get_region_info(region_id,&sql,region_name,sample_name,sample_id, experiment_name, experiment_id);
	ns_file_location_specification region_info(image_server_const.image_storage.get_base_path_for_region(region_id, &sql));
	#ifndef NS_CALCULATE_OPTICAL_FLOW
	if (create_only_flow)
		throw ns_ex("Attempting to create flow image storage location in db with NS_CALCULATE_OPTICAL_FLOW set to false");
	#endif
	if (!create_only_flow) {
		//delete all old images (to prevent recurrent db corruption problems from propigating)
		sql << "SELECT id, image_id, flow_image_id, group_id FROM path_data WHERE region_id = " << region_id;
		ns_sql_result res;
		sql.get_rows(res);
		std::vector<ns_64_bit> images_to_delete;
		bool extra_groups_found(false);
		for (unsigned int i = 0; i < res.size(); i++) {

			//also delete from disk any path data that's no longer included in this analysis...eg a reanalysis that has less groups (worms).
			bool group_not_needed_in_this_analysis(atol(res[i][3].c_str()) >= groups.size());
			ns_image_server_image im1, im2;;
			if (group_not_needed_in_this_analysis) {
				extra_groups_found = true;
				im1.load_from_db(ns_atoi64(res[i][1].c_str()), &sql);
				im2.load_from_db(ns_atoi64(res[i][2].c_str()), &sql);
				image_server_const.image_storage.delete_from_storage(im1, ns_delete_both_volatile_and_long_term, &sql);
				image_server_const.image_storage.delete_from_storage(im2, ns_delete_both_volatile_and_long_term, &sql);
			}
			if (im1.id != 0)
				images_to_delete.push_back(im1.id);
			if (im2.id != 0)
				images_to_delete.push_back(im2.id);
		}
		if (extra_groups_found) {
			sql << "DELETE FROM path_data WHERE region_id = " << region_id << " AND group_id >= " << groups.size();
			image_server_const.add_subtext_to_current_event("Deleting extra paths...\n", &sql);
			sql.send_query();
		}
		if (!images_to_delete.empty()) {
			sql << "DELETE FROM images WHERE id = " << images_to_delete[0];
			for (unsigned int i = 1; i < images_to_delete.size(); i++)
				sql << " OR id = " << images_to_delete[i];
			sql.send_query();
		}
		
	}

	//now allocate images
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
				continue;
			ns_image_server_image & im(groups[i].paths[j].output_image);

			
			if (!create_only_flow){
				im = image_server_const.image_storage.get_storage_for_path(region_info, j, i,
																region_id, region_name,experiment_name, sample_name,false);
				im.save_to_db(0,&sql);
				if (im.id == 0)
					throw ns_ex("ns_time_path_image_movement_analyzer::get_output_image_storage_locations()::Could not generate a new path id image id.");
			}
			#ifdef NS_CALCULATE_OPTICAL_FLOW
			ns_image_server_image & im_flow(groups[i].paths[j].flow_output_image);

			
			im_flow = image_server_const.image_storage.get_storage_for_path(region_info, j, i,
															region_id, region_name,experiment_name, sample_name,true);
			im_flow.save_to_db(0,&sql);
			if (im_flow.id == 0)
				throw ns_ex("ns_time_path_image_movement_analyzer::get_output_image_storage_locations()::Could not generate a new path id flow image id.");
			
			#else
			ns_image_server_image  im_flow;
			im_flow.id = 0;
			#endif
			if (groups[i].paths[j].path_db_id == 0){
				if (create_only_flow)
					throw ns_ex("ns_time_path_image_movement_analyzer::get_output_image_storage_locations()::Attempting to create only a flow image even though the standard movement image doesn't exist");
				sql << "INSERT INTO path_data SET image_id = " << im.id << ",flow_image_id = " << im_flow.id << ", region_id = " << region_id << ",group_id = " << i << ",path_id = " << j;
				groups[i].paths[j].path_db_id = sql.send_query_get_id();
			}
			else{
				if(create_only_flow)
					sql << "UPDATE path_data SET flow_image_id = " << im_flow.id << " WHERE id = " << groups[i].paths[j].path_db_id;
				else
					sql << "UPDATE path_data SET image_id = " << im.id << ",flow_image_id = " << im_flow.id << " WHERE id = " << groups[i].paths[j].path_db_id;
				sql.send_query();
			}
		}
	}
}
template<class allocator_T>
ns_death_time_annotation_time_interval ns_time_path_image_movement_analyzer<allocator_T>::get_externally_specified_last_measurement(const ns_64_bit region_id, ns_sql & sql){
	sql << "SELECT time_of_last_valid_sample FROM sample_region_image_info WHERE id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_image_movement_analyzer::crop_paths_to_final_time()::Could not load time of last sample from database.");
	unsigned long stop_time(atol(res[0][0].c_str()));
	if (stop_time == 0)
		stop_time = UINT_MAX;
	return ns_death_time_annotation_time_interval(0,stop_time);
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::crop_path_observation_times(const ns_death_time_annotation_time_interval & val){
	
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			if(groups[i].paths[j].elements.size() == 0)
				continue;

			for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++){
				if (groups[i].paths[j].elements[k].absolute_time > val.period_end ||
					groups[i].paths[j].elements[j].absolute_time < val.period_start)
					groups[i].paths[j].elements[k].excluded = true;
			}
		/*	std::vector<ns_analyzed_image_time_path_element>::iterator p = groups[i].paths[j].elements.end();
			p--;
			while(true){
				bool is_first_element(p == groups[i].paths[j].elements.begin());
				if (p->absolute_time > last_time){
					p = groups[i].paths[j].elements.erase(p);
				}
				if ( groups[i].paths[j].elements.empty() || is_first_element)
					break;
				p--;
			}
		}*/
		}
	}
	for (std::vector<ns_death_time_annotation>::iterator p = extra_non_path_events.events.begin(); p != extra_non_path_events.events.end();){
		if (p->time.period_end > val.period_end ||
			p->time.period_start < val.period_start)
			p = extra_non_path_events.erase(p);
		else ++p;
	}
}
template<class allocator_T>
ns_time_path_movement_result_files ns_time_path_image_movement_analyzer<allocator_T>::get_movement_quantification_files(const ns_64_bit region_info_id,ns_sql & sql, const ns_analysis_db_options & analysis_options, const ns_record_style_options& record_options){
		sql << "SELECT movement_image_analysis_quantification_id FROM sample_region_image_info WHERE id = " << region_info_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_time_path_image_movement_analyzer::load_movement_data_from_db():Could not load info from db");
		ns_image_server_image im;
		im.id = ns_atoi64(res[0][0].c_str());
		ns_time_path_movement_result_files files;
		if (im.id == 0 || analysis_options == ns_force_creation_of_new_db_record){
			if (analysis_options == ns_require_existing_record)
				throw ns_ex("Movement quantification data has not been stored in db");
			files = image_server_const.image_storage.get_region_movement_quantification_metadata(region_info_id, sql);
			files.base_db_record.save_to_db(0, &sql);
			sql << "UPDATE sample_region_image_info SET movement_image_analysis_quantification_id = " << files.base_db_record.id << " WHERE id = " << region_info_id;
			sql.send_query();
			if (im.id != 0) {	//delete old record if it existed
				sql << "DELETE FROM images WHERE id = " << im.id;
				sql.send_query();
			}
			return files;
			
		}
		else im.load_from_db(im.id, &sql);
		files.only_quantification_specified = im.filename.find("_quantification") != im.filename.npos || im.filename.find(".csv") != im.filename.npos || im.filename == files.base_name();
		if (files.only_quantification_specified) {
			if (record_options == ns_force_new_record_format) {
				files = image_server_const.image_storage.get_region_movement_quantification_metadata(region_info_id, sql);
				files.base_db_record.save_to_db(im.id, &sql);
				return files;
			}
			files.movement_quantification = im;
			return files;
		}
		else {
			files.set_from_base_db_record(im);
			return files;
		}
	}

const ns_movement_event ns_hmm_movement_analysis_optimizatiom_stats_record::states[ns_hmm_movement_analysis_optimizatiom_stats_record::number_of_states] = { ns_translation_cessation,
															ns_fast_movement_cessation,
															ns_movement_cessation,
															ns_death_associated_expansion_start,
															ns_death_associated_post_expansion_contraction_start};


void ns_time_path_movement_result_files::set_from_base_db_record(const ns_image_server_image& im) {
	
	if (ns_dir::extract_filename_without_extension(im.filename) != im.filename)
		throw ns_ex("ns_time_path_movement_result_files::set_from_base_db_record::Base name has a file extension! : ") << im.filename;

	base_db_record = movement_quantification = annotation_events = intervals_data = im;
	movement_quantification.filename = base_db_record.filename + "_quantification.csv.gz";
	annotation_events.filename = base_db_record.filename + "_events.csv.gz";
	intervals_data.filename = base_db_record.filename + "_intervals.csv.gz";
	only_quantification_specified = false;
}


template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::load_stored_movement_analysis_results(ns_sql & sql, const ns_movement_analysis_results_to_load& movement_data_to_load){
	
	ns_time_path_movement_result_files analysis_files = get_movement_quantification_files(this->region_info_id,sql,ns_require_existing_record,ns_use_existing_format);
	if (analysis_files.only_quantification_specified && movement_data_to_load != ns_only_quantification)
		throw ns_ex("This plate was analyzed with a previous version of the analysis software.  Please re-run the job \"Rebuild Movement Analysis from cached image analysis\".");

	ns_acquire_for_scope<ns_istream> q_i(0),
			i_i(0), 
			 e_i(0);
	ns_ex additional_metadata_problem;
	try{

		if (movement_data_to_load == ns_all_results || movement_data_to_load == ns_all_results_no_movement) {
			try{
				e_i.attach(image_server_const.image_storage.request_metadata_from_disk(analysis_files.annotation_events,false, &sql,false));
				 i_i.attach(image_server_const.image_storage.request_metadata_from_disk(analysis_files.intervals_data, false, &sql,false));
			}
			catch (ns_ex & ex) {
				throw ns_ex("Some stored movement analysis could not be found.  Please re-run the job \"Rebuild Movement Analysis from cached image analysis\": ") << ex.text();
			}
		}
		q_i.attach(image_server_const.image_storage.request_metadata_from_disk(analysis_files.movement_quantification,false, &sql,false));
		load_movement_data_from_disk(q_i()(), movement_data_to_load == ns_only_quantification_no_movement || movement_data_to_load == ns_all_results_no_movement);
		q_i.release();
		if (movement_data_to_load == ns_all_results || movement_data_to_load == ns_all_results_no_movement) {
			read_internal_annotation_data(e_i()());
			e_i.release();
			read_internal_intervals_data(i_i()());
			i_i.release();
		}
		
	}
	catch (ns_ex & ex) {
		std::string data = "unknown";
		try {
			sql << "SELECT s.name,r.name FROM sample_region_image_info as r, capture_samples as s WHERE r.id = " << this->region_info_id << " and s.id = r.sample_id";
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() > 0)
				data = res[0][0] + "::" + res[0][1];
		}
		catch (ns_ex & ex) { cerr << ex.text() << "\n"; }
		catch (...) {}
		ns_ex ex2;
		ex2 << ex.type();
		ex2 << "Problem loading stored plate quantification for plate " << data << "(" << this->region_info_id << ").  Consider re-running worm movement analysis from cached images for this plate. " << ex.text();
		throw ex2;

	}
};


bool operator!=(const ns_movement_state_time_interval_indicies& a, const ns_movement_state_time_interval_indicies& b) {
	return !(a == b);
}
bool operator==(const ns_movement_state_time_interval_indicies& a, const ns_movement_state_time_interval_indicies& b) {
	if (a.interval_occurs_after_observation_interval != b.interval_occurs_after_observation_interval)
		return false;
	if (a.interval_occurs_before_observation_interval != b.interval_occurs_before_observation_interval)
		return false;
	if (a.period_end_index != b.period_end_index)
		return false;
	if (a.period_start_index != b.period_start_index)
		return false;
	return true;
}
bool operator!=(const ns_movement_state_observation_boundary_interval& a, const ns_movement_state_observation_boundary_interval& b) {
	return !(a == b);
}
bool operator==(const ns_movement_state_observation_boundary_interval& a, const ns_movement_state_observation_boundary_interval& b) {
	if (a.entrance_interval != b.entrance_interval)
		return false;
	if (a.exit_interval != b.exit_interval)
		return false;
	if (a.longest_observation_gap_within_interval != b.longest_observation_gap_within_interval)
		return false;
	if (a.skipped != b.skipped)
		return false;
	return true;
}

bool ns_movement_analysis_result::compare_set(const ns_death_time_annotation_set& a) const {
	return death_time_annotation_set.compare(a);
}
bool ns_movement_analysis_result::compare(const ns_movement_analysis_result& res) const {
	bool same = compare_set(res.death_time_annotation_set);
	bool same2 = compare_intervals(res);
	return same && same2;
}
bool ns_movement_analysis_result::compare_intervals(const ns_movement_analysis_result& a) const{
	bool same = true;
	if (state_intervals.size() != a.state_intervals.size()) {
		cout << "Different size!";
		same = false;
	}
	for (unsigned int i = 0; i < state_intervals.size(); i++)
		if (state_intervals[i] != a.state_intervals[i]){
			cout << "Different interval!";
			same = false;
		}
	if (first_valid_element_id != a.first_valid_element_id){
		cout << "Different first element";
		same = false;
	}
	if (last_valid_element_id != a.last_valid_element_id){
		cout << "different last ";
		same = false;
	}
	return same;
}

bool operator==(const ns_worm_movement_description_series_element& a, const ns_worm_movement_description_series_element& b) {
	if (a.animal_alive_at_end_of_observation != b.animal_alive_at_end_of_observation)
		return false;
	if (a.animal_death_was_observed != b.animal_death_was_observed)
		return false;
	if (!(a.final_image_size == b.final_image_size))
		return false;
	if (!(a.metadata_position_on_visualization_grid == b.metadata_position_on_visualization_grid))
		return false;
	if (!(a.path_id == b.path_id))
		return false;
	if (!(a.position_on_visualization_grid == b.position_on_visualization_grid))
		return false;
	if (a.should_be_displayed != b.should_be_displayed)
		return false;
	if (a.sticky_properties.to_string() != b.sticky_properties.to_string())
		return false;
	if (!(a.visulazation_border_to_crop == b.visulazation_border_to_crop))
		return false;
	return true;
}
bool operator!=(const ns_worm_movement_description_series_element& a, const ns_worm_movement_description_series_element& b) {
	return !(a == b);
}

bool operator==(const ns_worm_movement_description_series& a, const ns_worm_movement_description_series& b) {
	if (a.items.size() != b.items.size())
		return false;
	for (unsigned int i = 0; i < a.items.size(); i++) {
		if (a.items[i] != b.items[i])
			return false;
	}
	if (a.timepoints.size() != b.timepoints.size())
		return false;
	for (unsigned int i = 0; i < a.timepoints.size(); i++) {
		if (a.timepoints[i].time != b.timepoints[i].time)
			return false;
		if (a.timepoints[i].worms.size() != b.timepoints[i].worms.size())
			return false;
		for (unsigned int j = 0; j < a.timepoints[i].worms.size(); j++) {
			if (a.timepoints[i].worms[j] != b.timepoints[i].worms[j])
				return false;
		}
	}
	return true;

}
bool operator!=(const ns_worm_movement_description_series& a, const ns_worm_movement_description_series& b) {
	return !(a == b);
}

bool operator==(const ns_worm_movement_measurement_description& a, const ns_worm_movement_measurement_description& b) {
	if (!(a.context_position == b.context_position))
		return false;
	if (!(a.context_size == b.context_size))
		return false;
	if (!(a.movement == b.movement))
		return false;
	if (!(a.path_id == b.path_id))
		return false;
	if (!(a.region_position == b.region_position))
		return false;
	if (!(a.region_size == b.region_size))
		return false;
	return true;
}
bool operator!=(const ns_worm_movement_measurement_description& a, const ns_worm_movement_measurement_description& b) {
	return !(a == b);
}


bool operator==(const ns_analyzed_image_time_path& a, const ns_analyzed_image_time_path& b) {
	if (a.entirely_excluded != b.entirely_excluded)
		return false;
	if (!a.movement_analysis_result.compare(b.movement_analysis_result))
		return false;
	return true;
}
bool operator!=(const ns_analyzed_image_time_path& a, const ns_analyzed_image_time_path& b) {
	return !(a == b);
}

template<class allocator_T>
bool ns_time_path_image_movement_analyzer<allocator_T>::compare(const ns_time_path_image_movement_analyzer<allocator_T>& t) const {
	bool same = true;
	if (t.analysis_id != this->analysis_id) {
		cerr << "Analysis id";
		same = false;
	}
	if (t.denoising_parameters_used.to_string() != denoising_parameters_used.to_string()) {
		cerr << "Denoising";
		same = false;
	}
	if (t.description_series != this->description_series) {
		cerr << "Description series";
		same = false;
	}
	if (t.externally_specified_plate_observation_interval != this->externally_specified_plate_observation_interval) {
		cerr << "Observation interval";
		same = false;
	}
	if (t.extra_non_path_events.size() != this->extra_non_path_events.size()) {
		cerr << "extra non path events size";
		same = false;
	}
	else {
		for (unsigned int i = 0; i < t.extra_non_path_events.size(); i++) {
			if (t.extra_non_path_events[i].to_string(true) != extra_non_path_events[i].to_string(true)) {
				cerr << "extra non path:\n";
				cerr << t.extra_non_path_events[i].to_string(true) << "\n";
				cerr << extra_non_path_events[i].to_string(true) << "\n";
				return false;
			}
		}
	}
	if (t.groups.size() != this->groups.size()) {
		cerr << "group size";
		same = false;
	}
	else {
		for (unsigned int i = 0; i < groups.size(); i++) {
			if (t.groups[i].paths.size() != groups[i].paths.size()) {
				cerr << "path size";
				same = false;
			}
			else {
				for (unsigned int j = 0; j < t.groups[i].paths.size(); j++) {
					if (t.groups[i].paths[j] != groups[i].paths[j]) {
						cerr << "path" << i << " " << j;
						same = false;
					}
				}
			}
		}
	}
	if (t.last_timepoint_in_analysis_ != last_timepoint_in_analysis_) {
		cerr << "last time point";
		same = false;
	}
	
	if (t.number_of_timepoints_in_analysis_ != number_of_timepoints_in_analysis_) {
		cerr << "number_of_timepoints_in_analysis";
		same = false;
	}
	if (t.paths_loaded_from_solution != paths_loaded_from_solution) {
		cerr << "paths_loaded_from_solution";
		same = false;
	}
	if (t.posture_model_version_used != posture_model_version_used) {
		cerr << "posture_model_version_used";
		same = false;
	}
	if (t.region_image_specifications.size() != region_image_specifications.size()) {
		cerr << "region_image_specifications size";
		same = false;
	}
	else {
		for (unsigned int i = 0; i < t.region_image_specifications.size(); i++) {
			if (t.region_image_specifications[i] != region_image_specifications[i]) {
				cerr << "region image specification value";
				same = false;
			}
		}
	}
	if (t.region_info_id != region_info_id) {
		cerr << "region_info_id";
		same = false;
	}
	if (t._number_of_invalid_images_encountered != _number_of_invalid_images_encountered) {
		cerr << "_number_of_invalid_images_encountered";
		same = false;
	}
	return same;
}


bool operator==(const ns_analyzed_image_specification& a, const ns_analyzed_image_specification& b) {
	if (a.interpolated_region_vis_image.id != b.interpolated_region_vis_image.id)
		return false;
	if (a.interpolated_region_vis_required != b.interpolated_region_vis_required)
		return false;
	if (a.region_vis_image.id != b.region_vis_image.id)
		return false;
	if (a.region_vis_required != b.region_vis_required)
		return false;
	if (a.sample_region_image_id != b.sample_region_image_id)
		return false;
	if (a.time != b.time)
		return false;
	return true;
}
bool operator!=(const ns_analyzed_image_specification& a, const ns_analyzed_image_specification& b) {
	return !(a == b);
}


template<class allocator_T>
bool ns_time_path_image_movement_analyzer<allocator_T>::calculate_optimzation_stats_for_current_hmm_estimator(const std::string * database_name,ns_hmm_movement_analysis_optimizatiom_stats & output_stats, const ns_emperical_posture_quantification_value_estimator * e, std::set<ns_stationary_path_id> & paths_to_test, bool generate_path_info) {
	bool found_worm(false);
	output_stats.animals.reserve(groups.size());
	for (unsigned long g = 0; g < groups.size(); g++)
		for (unsigned long p = 0; p < groups[g].paths.size(); p++) {
			if (ns_skip_low_density_paths && groups[g].paths[p].is_low_density_path() || !groups[g].paths[p].by_hand_data_specified() ||
				groups[g].paths[p].sticky_properties().disambiguation_type != ns_death_time_annotation::ns_single_worm ||
				groups[g].paths[p].sticky_properties().excluded != ns_death_time_annotation::ns_not_excluded)
				
				continue;

		
			//record stats to evaluate the performance compared to by hand results

			if (!groups[g].paths[p].by_hand_data_specified())
				continue;

			if (!paths_to_test.empty() && paths_to_test.find(this->generate_stationary_path_id(g, p)) == paths_to_test.end())
				continue;
			found_worm = true;
			ns_time_path_posture_movement_solution by_hand_posture_movement_solution(groups[g].paths[p].reconstruct_movement_state_solution_from_annotations(groups[g].paths[p].movement_analysis_result.first_valid_element_id.period_start_index, groups[g].paths[p].movement_analysis_result.last_valid_element_id.period_end_index, e, groups[g].paths[p].by_hand_annotation_event_times));
			//if (by_hand_posture_movement_solution.moving.skipped) {
				//cout << "Encountered a by hand annotation in which the animal never slowed: " << g << "\n";
			//	continue;
			//}
			output_stats.animals.resize(output_stats.animals.size() + 1);
			ns_hmm_movement_analysis_optimizatiom_stats_record& stat(*output_stats.animals.rbegin());
			//set upstructures for debug output
			ns_hmm_solver hmm_solver;
			if (generate_path_info) {
				stat.state_info_times.resize(groups[g].paths[p].element_count());
				for (unsigned int i = 0; i < stat.state_info_times.size(); i++)
					stat.state_info_times[i] = groups[g].paths[p].element(i).absolute_time;
				e->provide_sub_probability_names(stat.state_info_variable_names);
			}

			/*std::ofstream o("c:\\server\\state_transitions.dot");
			if (!o.fail()) {
				std::vector<std::vector<double> > m;
				hmm_solver.build_state_transition_matrix(*e,m);
				hmm_solver.output_state_transition_matrix(m, o);
				o.close();
			}*/
			//first calculate the probabilities of the machine and by hand solutions
			hmm_solver.probability_of_path_solution(groups[g].paths[p], *e, groups[g].paths[p].movement_analysis_result.machine_movement_state_solution, stat.machine_state_info, generate_path_info);
	
			hmm_solver.probability_of_path_solution(groups[g].paths[p], *e, by_hand_posture_movement_solution, stat.by_hand_state_info, generate_path_info);
			
			stat.solution_loglikelihood = groups[g].paths[p].movement_analysis_result.machine_movement_state_solution.loglikelihood_of_solution;
		
			
			//now go through and calculate the errors betweeen the machine and by hand calculations
			for (unsigned int i = 0; i < ns_hmm_movement_analysis_optimizatiom_stats_record::number_of_states; i++) {
				const ns_movement_event st = ns_hmm_movement_analysis_optimizatiom_stats_record::states[i];
				bool machine_skipped;
				const ns_death_time_annotation_time_interval machine_result = groups[g].paths[p].machine_event_time(st, machine_skipped);
				const ns_death_time_annotation_time_interval by_hand_result = groups[g].paths[p].by_hand_annotation_event_times[st];

				stat.database_name = database_name;
				stat.properties = groups[g].paths[p].censoring_and_flag_details;
				stat.properties.stationary_path_id = ns_stationary_path_id(g, p, this->analysis_id);
				stat.path_id = ns_stationary_path_id(g, p, this->analysis_id);
				stat.properties.region_info_id = this->region_info_id;
				ns_hmm_movement_analysis_optimizatiom_stats_event_annotation & result = stat.measurements[st];

				result.by_hand_identified = !by_hand_result.fully_unbounded() && groups[g].paths[p].by_hand_annotation_event_explicitness[st] != ns_death_time_annotation::ns_explicitly_not_observed;
				
				if (result.by_hand_identified)
					result.by_hand = by_hand_result;
				if (result.by_hand_identified && result.by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval() == 0) {
					std::cerr << "The worm browser generated a spurious storyboard annotation that has been ignored.\n";
					//These enter the pipeline in
					//ns_analyzed_image_time_path::add_by_hand_annotations()
					result.by_hand_identified = false;
				}
				result.machine_identified = !machine_skipped;
				if (result.machine_identified)
					result.machine = machine_result;

			}

		}
	return found_worm;
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::reanalyze_with_different_movement_estimator(const ns_time_series_denoising_parameters &, const ns_analyzed_image_time_path_death_time_estimator * e) {
	if (region_info_id == 0)
		throw ns_ex("Attempting to reanalyze an unloaded image!");

	if (posture_model_version_used != e->software_version_number()){
	  //for (unsigned int i = 0; i <posture_model_version_used.size(); i++)
	  //cout << (int)posture_model_version_used[i];
	  throw ns_ex("This region's movement analysis was run using threshold posture analysis version \"") << ns_to_string(posture_model_version_used) << "\".  This is incompatible with the posture analysis file you have specified, \"" << e->name << "\" which is v " << e->software_version_number() << ".  You can fix this by running the job \"Analyze Worm Movement using Cached Images\" which will preserve all by hand annotations.";
	}


	for (unsigned long g = 0; g < groups.size(); g++) {
		for (unsigned long p = 0; p < groups[g].paths.size(); p++) {
			if (ns_skip_low_density_paths && groups[g].paths[p].is_low_density_path())
				continue;
			try {
				groups[g].paths[p].analyze_movement(e, ns_stationary_path_id(g, p, analysis_id), last_timepoint_in_analysis_);
				groups[g].paths[p].calculate_movement_quantification_summary(groups[g].paths[p].movement_analysis_result);
			}
			catch (ns_ex & ex) {
				throw ns_ex("Error analyzing group ") << g << " path " << p << ":" << ex.text();
			}
		}
	}
	generate_movement_description_series();
}

template<class allocator_T>
bool ns_time_path_image_movement_analyzer<allocator_T>::load_image_quantification_and_rerun_death_time_detection(const ns_64_bit region_id, const ns_time_path_solution& solution_, const ns_time_series_denoising_parameters& times_series_denoising_parameters,
	const ns_analyzed_image_time_path_death_time_estimator* e, ns_sql& sql, unsigned long debug_specific_worm) {
	region_info_id = region_id;
	externally_specified_plate_observation_interval = get_externally_specified_last_measurement(region_id, sql);
	load_from_solution(solution_);
	crop_path_observation_times(externally_specified_plate_observation_interval);
	bool found_path_info_in_db = load_movement_image_db_info(region_info_id, sql);



	for (unsigned long g = 0; g < groups.size(); g++) {
		for (unsigned long p = 0; p < groups[g].paths.size(); p++)
			for (unsigned int i = 0; i < groups[g].paths[p].death_time_annotations().events.size(); i++) {
				//	groups[g].paths[p].by_hand_annotation_event_times.resize((int)ns_number_of_movement_event_types,-1);
				groups[g].paths[p].movement_analysis_result.death_time_annotation_set.events[i].region_info_id = region_info_id;
			}
	}
	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Populating movement quantification from file"));
	load_stored_movement_analysis_results(sql, ns_only_quantification);

	if (posture_model_version_used != e->software_version_number()) {
		cout << posture_model_version_used << "\n";
		throw ns_ex("This region's movement analysis was run using threshold posture analysis version \"") << ns_to_string(posture_model_version_used) << "\".  This is incompatible with the movement analysis model you have specified, \"" << e->name << "\" which is v " << e->software_version_number() << ".  You can fix this by running the job \"Analyze Worm Movement using Cached Images\" which will preserve all by hand annotations.";
	}



	ns_64_bit file_specified_analysis_id = this->analysis_id;

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Getting analysis id"));
	obtain_analysis_id_and_save_movement_data(region_id, sql, ns_require_existing_record, ns_do_not_write_data);
	if (file_specified_analysis_id != this->analysis_id)
		throw ns_ex("Movement analysis ID specified on disk does not agree with the ID  specified in database.");
	if (analysis_id == 0)
		throw ns_ex("Could not obtain analysis id!");


	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Denoising movement series and calculating slopes"));
	for (unsigned long g = 0; g < groups.size(); g++)
		for (unsigned long p = 0; p < groups[g].paths.size(); p++) {
			unsigned long number_of_valid_points(0);
			for (unsigned int i = 0; i < groups[g].paths[p].element_count(); i++) {
				if (!groups[g].paths[p].element(i).censored &&
					!groups[g].paths[p].element(i).excluded)
					number_of_valid_points++;
			}
			if (ns_skip_low_density_paths && groups[g].paths[p].is_low_density_path())
				continue;
			//invalid paths can result from changing the last valid timepoint and reanalyzing saved movement analyses
			if (number_of_valid_points <= 1 || groups[g].paths[p].element_count() > 0 && groups[g].paths[p].element(0).absolute_time > this->last_timepoint_in_analysis()) {
				groups[g].paths[p].entirely_excluded = true;
				continue;
			}

			//groups[g].paths[p].denoise_movement_series_and_calculate_intensity_slopes(0,times_series_denoising_parameters);
		}

	//if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Normalizing scores"));
	normalize_movement_scores_over_all_paths(e->software_version_number(), times_series_denoising_parameters, sql);
	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Calculating quantifications"));
	std::vector< ns_64_bit> tmp1, tmp2;
	//ofstream o("c:\\server\\debug_" + ns_format_time_string(ns_current_time()));
	//groups[0].paths[0].write_detailed_movement_quantification_analysis_header(o);
	//o << "\n";
	for (unsigned long g = 0; g < groups.size(); g++){
		for (unsigned long p = 0; p < groups[g].paths.size(); p++) {
			unsigned long number_of_valid_points(0);
			for (unsigned int i = 0; i < groups[g].paths[p].element_count(); i++) {
				if (!groups[g].paths[p].element(i).censored &&
					!groups[g].paths[p].element(i).excluded)
					number_of_valid_points++;
			}
			if (ns_skip_low_density_paths && groups[g].paths[p].is_low_density_path())
				continue;
			//invalid paths can result from changing the last valid timepoint and reanalyzing saved movement analyses
			if (number_of_valid_points <= 1 || groups[g].paths[p].element_count() > 0 && groups[g].paths[p].element(0).absolute_time > this->last_timepoint_in_analysis()) {
				groups[g].paths[p].entirely_excluded = true;
				continue;
			}
			if (debug_specific_worm != -1) {
				if (debug_specific_worm != g)
					continue;
				//for debug, recalculate the smoothed data
				groups[g].paths[p].denoise_movement_series_and_calculate_intensity_slopes(times_series_denoising_parameters, tmp1, tmp2);
			}
			//xxx
			//ns_region_metadata m;
			//if (g == 17)
			//	groups[g].paths[p].write_detailed_movement_quantification_analysis_data(m, g, p, o, false, false);
			groups[g].paths[p].analyze_movement(e, ns_stationary_path_id(g, p, analysis_id), last_timepoint_in_analysis_);
			groups[g].paths[p].calculate_movement_quantification_summary(groups[g].paths[p].movement_analysis_result);
		}
	}
	//o.close();


	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Done"));



	//generate_movement_description_series();
	return found_path_info_in_db;
}
template<class allocator_T>
bool ns_time_path_image_movement_analyzer<allocator_T>::load_completed_analysis_(const ns_64_bit region_id,const ns_time_path_solution & solution_,  const ns_time_series_denoising_parameters & times_series_denoising_parameters, 
															const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql, bool exclude_movement_quantification){
	region_info_id = region_id;
	externally_specified_plate_observation_interval = get_externally_specified_last_measurement(region_id,sql);
	load_from_solution(solution_);
	crop_path_observation_times(externally_specified_plate_observation_interval);
	bool found_path_info_in_db = load_movement_image_db_info(region_info_id,sql);
	

	
	for (unsigned long g = 0; g < groups.size(); g++){
		for (unsigned long p = 0; p < groups[g].paths.size(); p++)
			for (unsigned int i = 0; i < groups[g].paths[p].death_time_annotations().events.size(); i++){
			//	groups[g].paths[p].by_hand_annotation_event_times.resize((int)ns_number_of_movement_event_types,-1);
				groups[g].paths[p].movement_analysis_result.death_time_annotation_set.events[i].region_info_id = region_info_id;
			}
	}
	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Populating movement quantification from file"));
	load_stored_movement_analysis_results(sql, exclude_movement_quantification? ns_all_results_no_movement:ns_all_results );

	if (posture_model_version_used != e->software_version_number()){
	  cout << posture_model_version_used << "\n";
	  throw ns_ex("This region's movement analysis was run using threshold posture analysis version \"") << ns_to_string(posture_model_version_used) << "\".  This is incompatible with the movement analysis model you have specified, \"" << e->name << "\" which is v " << e->software_version_number() << ".  You can fix this by running the job \"Analyze Worm Movement using Cached Images\" which will preserve all by hand annotations.";
	}



	ns_64_bit file_specified_analysis_id = this->analysis_id;

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Getting analysis id"));
	obtain_analysis_id_and_save_movement_data(region_id, sql, ns_require_existing_record, ns_do_not_write_data);
	if (file_specified_analysis_id != this->analysis_id)
		throw ns_ex("Movement analysis ID specified on disk does not agree with the ID  specified in database.");
	if (analysis_id == 0)
		throw ns_ex("Could not obtain analysis id!");

	if (exclude_movement_quantification)
		return found_path_info_in_db;

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Denoising movement series and calculating slopes"));
	for (unsigned long g = 0; g < groups.size(); g++)
		for (unsigned long p = 0; p < groups[g].paths.size(); p++){
			unsigned long number_of_valid_points(0);
			for (unsigned int i = 0; i < groups[g].paths[p].element_count(); i++){
				if (!groups[g].paths[p].element(i).censored && 
					!groups[g].paths[p].element(i).excluded)
					number_of_valid_points++;
			}
			if (ns_skip_low_density_paths && groups[g].paths[p].is_low_density_path())
				continue;
			//invalid paths can result from changing the last valid timepoint and reanalyzing saved movement analyses
			if (number_of_valid_points <= 1 || groups[g].paths[p].element_count() > 0 && groups[g].paths[p].element(0).absolute_time > this->last_timepoint_in_analysis()){
				groups[g].paths[p].entirely_excluded = true;
				continue;
			}
	
			//groups[g].paths[p].denoise_movement_series_and_calculate_intensity_slopes(times_series_denoising_parameters);
		}
	
	for (unsigned long g = 0; g < groups.size(); g++)
		for (unsigned long p = 0; p < groups[g].paths.size(); p++){
			unsigned long number_of_valid_points(0);
			for (unsigned int i = 0; i < groups[g].paths[p].element_count(); i++){
				if (!groups[g].paths[p].element(i).censored && 
					!groups[g].paths[p].element(i).excluded)
					number_of_valid_points++;
			}
			if (ns_skip_low_density_paths && groups[g].paths[p].is_low_density_path())
				continue;
			//invalid paths can result from changing the last valid timepoint and reanalyzing saved movement analyses
			if (number_of_valid_points <= 1 || groups[g].paths[p].element_count() > 0 && groups[g].paths[p].element(0).absolute_time > this->last_timepoint_in_analysis()){
				groups[g].paths[p].entirely_excluded = true;
				continue;
			}
			groups[g].paths[p].calculate_movement_quantification_summary(groups[g].paths[p].movement_analysis_result);
		}	

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Done"));

	return found_path_info_in_db;
}



template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::write_internal_annotation_data(std::ostream& o) const {
	for (unsigned int i = 0; i < groups.size(); i++) {
		for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
			for (unsigned int k = 0; k < groups[i].paths[j].movement_analysis_result.death_time_annotation_set.size(); k++) {
				o << i << "," << j << ",";
				groups[i].paths[j].movement_analysis_result.death_time_annotation_set[k].write_column_format_data(o);
				o << "\n";
			}
		}
	}
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::read_internal_annotation_data(std::istream& in) {
	ns_get_int get_int;
	while (true) {
		int group_id, path_id;
		get_int(in, group_id);
		if (in.fail())
			break;
		get_int(in, path_id);
		//cout << group_id << "," << path_id << "\n";
		if (group_id >= groups.size())
			throw ns_ex("Invalid group id");
		if (path_id >= groups[group_id].paths.size())
			throw ns_ex("Invalid path id");
		groups[group_id].paths[path_id].movement_analysis_result.death_time_annotation_set.read_column_format(ns_death_time_annotation_set::ns_all_annotations, in, false,true);
		if (in.fail())
			throw ns_ex("Malformed intervals file");
	}
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::write_internal_intervals_data(std::ostream & o) const {
	for (unsigned int i = 0; i < groups.size(); i++) {
		for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
			if (i != 0 || j != 0)
				o << "\n";
			o << i << "," << j << ",";
			groups[i].paths[j].movement_analysis_result.write_intervals(o);
		}
	}
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::read_internal_intervals_data(std::istream& in) {
	ns_get_int get_int;
	while (true) {
		if (in.fail())
			return;
		int group_id, path_id;
		get_int(in, group_id);
		if (in.fail())
			throw ns_ex("Malformed intervals file");
		get_int(in, path_id);
		if (in.fail())
			throw ns_ex("Malformed intervals file");
		//cout << group_id << " " << path_id << "\n";
		if (group_id >= groups.size())
			throw ns_ex("Invalid group id");
		if (path_id >= groups[group_id].paths.size())
			throw ns_ex("Invalid path id");
		groups[group_id].paths[path_id].movement_analysis_result.read_intervals(in);
		
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::obtain_analysis_id_and_save_movement_data(const ns_64_bit region_id, ns_sql& sql, ns_analysis_db_options id_options, ns_data_write_options write_options) {

	ns_time_path_movement_result_files movement_analysis_files = get_movement_quantification_files(region_id, sql, id_options, ns_force_new_record_format);

	//load and check for bad filenames
	if (movement_analysis_files.movement_quantification.filename.empty()) {
		if (id_options == ns_require_existing_record)
			throw ns_ex("obtain_analysis_id_and_save_movement_data()::Encountered an empty record");
		movement_analysis_files = get_movement_quantification_files(region_id, sql, ns_force_creation_of_new_db_record, ns_force_new_record_format);
	}

	ns_acquire_for_scope<ns_ostream> time_path_quantification_output(0), events_output(0), state_intervals_output(0);
	try {
		if (write_options == ns_write_data) {
			events_output.attach(image_server_const.image_storage.request_metadata_output(movement_analysis_files.annotation_events, ns_csv_gz, true, &sql));
			state_intervals_output.attach(image_server_const.image_storage.request_metadata_output(movement_analysis_files.intervals_data, ns_csv_gz, true, &sql));
			time_path_quantification_output.attach(image_server_const.image_storage.request_metadata_output(movement_analysis_files.movement_quantification, ns_csv_gz, true, &sql));
		}
	}
	catch (ns_ex & ex) {
		//if there's a problem create a new entry in the db to halt propigation of db errors
		if (id_options == ns_require_existing_record)
			throw ns_ex("ns_time_path_image_movement_analyzer::obtain_analysis_id_and_save_movement_data()::Cannot write to an existing movement analysis record, because that record could not be found");

		movement_analysis_files = get_movement_quantification_files(region_id, sql, ns_force_creation_of_new_db_record, ns_force_new_record_format);
		if (write_options == ns_write_data) {
			events_output.attach(image_server_const.image_storage.request_metadata_output(movement_analysis_files.annotation_events, ns_csv_gz, true, &sql));
			state_intervals_output.attach(image_server_const.image_storage.request_metadata_output(movement_analysis_files.intervals_data, ns_csv_gz, true, &sql));
			time_path_quantification_output.attach(image_server_const.image_storage.request_metadata_output(movement_analysis_files.movement_quantification, ns_csv_gz, true, &sql));
		}
	}
	//set analysis id that will uniquely identify all annotations generated by this analysis
	analysis_id = movement_analysis_files.base_db_record.id;
	
	if (write_options == ns_write_data) {
		save_movement_data_to_disk(time_path_quantification_output()());
		time_path_quantification_output.release();
		write_internal_annotation_data(events_output()());
		events_output.release();
		write_internal_intervals_data(state_intervals_output()());
		state_intervals_output.release();

		//std::map<int, ns_movement_analysis_result> saved_result;

		//for (unsigned int i = 0; i < groups.size(); i++) {
		//	saved_result[i] = groups[i].paths[0].movement_analysis_result;
		//}
		///xxx debug testing
		/*ns_istream* in = image_server_const.image_storage.request_metadata_from_disk(movement_analysis_files.intervals_data, true, &sql,false);
		this->read_internal_intervals_data((*in)());
		delete in;
		for (auto p = saved_result.begin(); p != saved_result.end(); p++) {
			if (!p->second.compare(groups[p->first].paths[0].movement_analysis_result))
				cerr << "Invalid write";
		}*/
	}

}


void ns_analyzed_image_time_path_element_measurements::read(istream & in, ns_vector_2d & registration_offset,bool & saturated_offset)  {
	ns_get_int get_int;
	ns_get_double get_double;
	ns_get_string get_string;
	get_int(in, interframe_time_scaled_movement_sum);//5										
	if (in.fail()) throw ns_ex("Invalid Specification 5");
	get_int(in, movement_alternate_worm_sum);//6												
	if (in.fail()) throw ns_ex("Invalid Specification 6");
	get_double(in, change_in_total_region_intensity);//7										
	if (in.fail()) throw ns_ex("Invalid Specification 7");
	get_double(in, change_in_total_foreground_intensity);//8								  
	if (in.fail()) throw ns_ex("Invalid Specification 8");
	get_int(in, total_foreground_area);//9		
	//cout << "fga: " << total_foreground_area << " ";
	if (in.fail()) throw ns_ex("Invalid Specification 9");
	get_int(in, total_intensity_within_foreground);//10										
	if (in.fail()) throw ns_ex("Invalid Specification 10");
	get_int(in, total_region_area);//11														
	if (in.fail()) throw ns_ex("Invalid Specification 11");
	get_int(in, total_intensity_within_region);//12
	if (in.fail()) throw ns_ex("Invalid Specification 12");
	get_int(in, total_alternate_worm_area);//13
	if (in.fail()) throw ns_ex("Invalid Specification 13");
	get_int(in, total_intensity_within_alternate_worm);//14
	if (in.fail()) throw ns_ex("Invalid Specification 14");
	int t;
	get_int(in, t);
	if (in.fail()) throw ns_ex("Invalid Specification 15");
	saturated_offset = (t != 0);//15

	get_double(in, registration_offset.x);//16
	if (in.fail())		throw ns_ex("Invalid Specification 15");
	get_double(in, registration_offset.y);//17
	if (in.fail())		throw ns_ex("Invalid Specification 17");
	get_int(in, movement_sum);//18
	if (in.fail())		throw ns_ex("Invalid Specification 18");
	get_double(in, denoised_movement_score);//19
	if (in.fail())		throw ns_ex("Invalid Specification 19");
	get_double(in, movement_score);//20
	if (in.fail())		throw ns_ex("Invalid Specification 20");
	get_int(in, total_intensity_within_stabilized);//21
	if (in.fail())		throw ns_ex("Invalid Specification 21");
	get_double(in, spatial_averaged_movement_sum_cropped);//22
	if (in.fail())		throw ns_ex("Invalid Specification 22");
	get_double(in, spatial_averaged_movement_sum_uncropped);//23
	if (in.fail())		throw ns_ex("Invalid Specification 23");
	get_double(in, spatial_averaged_movement_score_uncropped);//24
	if (in.fail())		throw ns_ex("Invalid Specification 24");
	get_double(in, spatial_averaged_movement_score_cropped);//25
	if (in.fail())		throw ns_ex("Invalid Specification 25");
	get_int(in, total_intensity_in_previous_frame_scaled_to_current_frames_histogram);//26
	if (in.fail())		throw ns_ex("Invalid Specification 26");
	get_int(in, total_stabilized_area);//27
	if (in.fail())		throw ns_ex("Invalid Specification 27");
	get_int(in, change_in_total_stabilized_intensity_1x);//28
	if (in.fail())	throw ns_ex("Invalid Specification 28");
	get_int(in, change_in_total_stabilized_intensity_2x);//29
	if (in.fail())	throw ns_ex("Invalid Specification 29");
	get_int(in, change_in_total_stabilized_intensity_4x);//30
	if (in.fail())		throw ns_ex("Invalid Specification 30");
	get_int(in, total_intensity_within_stabilized_denoised);//31	
	if (in.fail())	throw ns_ex("Invalid Specification 31"); 
	get_int(in, change_in_total_outside_stabilized_intensity_1x);//32	
	if (in.fail())	throw ns_ex("Invalid Specification 32"); 
	get_int(in, change_in_total_outside_stabilized_intensity_2x);//33	
	if (in.fail())	throw ns_ex("Invalid Specification 33");
	get_int(in, change_in_total_outside_stabilized_intensity_4x);//34	
	if (in.fail())	throw ns_ex("Invalid Specification 34");
	get_int(in, total_intensity_outside_stabilized_denoised);//35	
	if (in.fail()) throw ns_ex("Invalid Specification 35");
	string tmp;
	char a = get_int(in, tmp);
	if (in.fail())
	

	if (a == '\n') {
		//old style records
#ifdef NS_CALCULATE_OPTICAL_FLOW
		raw_flow_magnitude.zero();
		scaled_flow_dx.zero();
		scaled_flow_dy.zero();
		raw_flow_dx.zero();
		raw_flow_dy.zero();
#endif
		return;
	}
	if (!tmp.empty()) {
		//throw ns_ex("Badly formed input file!");
		cerr << "Badly formed input file: " << tmp << "\n";
	}
#ifdef NS_CALCULATE_OPTICAL_FLOW
	scaled_flow_magnitude.read(in);
	raw_flow_magnitude.read(in);
	scaled_flow_dx.read(in);
	scaled_flow_dy.read(in);
	raw_flow_dx.read(in);
	raw_flow_dy.read(in);
	/*
	for (unsigned int i = 0; i < 6; i++)
	for (unsigned int j = 0; j < 6; j++){
	get_int(in,tmp);
	if (in.fail()) throw ns_ex("Invalid Specification") << 27 +6*i+j;
	}*/
#endif

	//open for future use
	for (unsigned int i = 0; i < 2; i++) {
		char a = get_int(in, tmp);
		if (a == '\n')
			break;
		//		std::cerr << "E" << i << "'" << tmp << "' ";
		if (in.fail() || !tmp.empty())
			throw ns_ex("Invalid Specification");
	}
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::load_movement_data_from_disk(istream & in, bool skip_movement_data){

	ns_get_int get_int;
	ns_get_double get_double;
	ns_get_string get_string;
	get_int(in,this->analysis_id);
	if (in.fail())
		throw ns_ex("Empty Specification!");
	bool version_specified = false;
	std::string tmp;
	get_string(in, tmp);
	if (tmp == "v:") {
		get_string(in, tmp);
		posture_model_version_used.resize(0);
		posture_model_version_used.reserve(tmp.size());
		for (unsigned int i = 0; i < tmp.size(); i++)
		  if (!isspace(tmp[i]))
		    posture_model_version_used+=tmp[i];

		version_specified = true;
      
	} 
	if (!version_specified)
		posture_model_version_used = "2";
      
	if (skip_movement_data)
		return;
		
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++){
				groups[i].paths[j].elements[k].absolute_time = 0;
			}
		}
	}
	bool first_read = true;
	while(true){
		unsigned long group_id,path_id,element_id;
		if (first_read && !version_specified) 
			group_id = atoi(tmp.c_str());	//if version wasn't specified, we have already loaded the first integer while looking for the version
		else get_int(in, group_id); //1
		
		first_read = false;
		if(in.fail()) break;

		get_int(in,path_id);//2
		if(in.fail()) throw ns_ex("Invalid Specification 2");
		get_int(in,element_id);//3
		if(in.fail()) throw ns_ex("Invalid Specification 3");
	//	std::cerr << "(" << group_id << "," << path_id << "," << element_id << ",";
		if (group_id >= groups.size())
			throw ns_ex("ns_time_path_image_movement_analyzer::load_movement_data_from_disk()::Invalid group id ") << group_id;                                     
		if (path_id >= groups[group_id].paths.size())																												
			throw ns_ex("ns_time_path_image_movement_analyzer::load_movement_data_from_disk()::Invalid path id ") << path_id;										
		if (groups[group_id].paths[path_id].elements.size() == 0)																									
			throw ns_ex("ns_time_path_image_movement_analyzer::load_movement_data_from_disk()::Encountered an empty path");											
		if (element_id >= groups[group_id].paths[path_id].elements.size())																							
			throw ns_ex("ns_time_path_image_movement_analyzer::load_movement_data_from_disk()::Element is too large ") << path_id;		

		get_int(in,groups[group_id].paths[path_id].elements[element_id].absolute_time);//4		
		//cout << "Group id:" << group_id << "; Element id: " << element_id << "; time: " << groups[group_id].paths[path_id].elements[element_id].absolute_time << " ";
		if(in.fail()) throw ns_ex("Invalid Specification 4");																										
		groups[group_id].paths[path_id].elements[element_id].measurements.read(in, groups[group_id].paths[path_id].elements[element_id].registration_offset, groups[group_id].paths[path_id].elements[element_id].saturated_offset);
		//std::cout << "\n";

	}
	//check all data is loaded
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++){
				if (groups[i].paths[j].elements[k].absolute_time == 0)
					throw ns_ex(" ns_time_path_image_movement_analyzer::load_movement_data_from_disk():Not all data specified in file!");
			}
		}
	}
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::get_processing_stats_from_solution(const ns_time_path_solution & solution_){
	number_of_timepoints_in_analysis_ = solution_.timepoints.size();
	
	last_timepoint_in_analysis_ = 0;
	for (unsigned int i = 0; i < solution_.timepoints.size(); i++){
		if (solution_.timepoints[i].time > last_timepoint_in_analysis_)
			last_timepoint_in_analysis_ = solution_.timepoints[i].time;
	}

}





void ns_analyzed_image_time_path_element_measurements::write_header(ostream & out) {
	out << "interframe_time_scaled_movement_sum"
		",movement_alternate_worm_sum"
		",change_in_total_region_intensity"
		",change_in_total_foreground_intensity"
		",total_foreground_area"
		",total_intensity_within_foreground"
		",total_region_area "
		",total_intensity_within_region"
		",total_alternate_worm_area "
		",total_intensity_within_alternate_worm "
		",(saturated_offset"
		",offset.x "
		",offset.y"
		",movement_sum "
		",denoised_movement_score "
		",movement_score "
		",total_intensity_within_stabilized"
		",spatial_averaged_movement_sum_cropped "
		",spatial_averaged_movement_sum_uncropped "
		",spatial_averaged_movement_score_uncropped "
		",spatial_averaged_movement_score_cropped "
		",total_intensity_in_previous_frame_scaled_to_current_frames_histogram"
		",total_stabilized_area"
		",change_in_total_stabilized_intensity_1x"
		",change_in_total_stabilized_intensity_2x"
		",change_in_total_stabilized_intensity_4x"
		",total_intensity_within_stabilized_denoised"
		",change_in_total_outside_stabilized_intensity_1x"
		",change_in_total_outside_stabilized_intensity_2x"
		",change_in_total_outside_stabilized_intensity_4x"
		",total_intensity_outside_stabilized_denoised";

	for (unsigned int i = 0; i < 2; i++)
		out << ",blank";
}
void ns_analyzed_image_time_path_element_measurements::write(ostream & out,const ns_vector_2d & offset, const bool & saturated_offset) const {
	out << interframe_time_scaled_movement_sum << ","//5
		<< movement_alternate_worm_sum << ","//6
		<< change_in_total_region_intensity << ","//7
		<< change_in_total_foreground_intensity << ","//8
		<< total_foreground_area << ","//9
		<< total_intensity_within_foreground << ","//10
		<< total_region_area << ","//11
		<< total_intensity_within_region << ","//12
		<< total_alternate_worm_area << ","//13
		<< total_intensity_within_alternate_worm << ","//14
		<< (saturated_offset ? "1" : "0") << ","//15
		<< offset.x << ","//16
		<< offset.y << ","//17
		<< movement_sum << ","//18
		<< denoised_movement_score << ","//19
		<< movement_score << ","//20
		<< total_intensity_within_stabilized << ","//21
		<< spatial_averaged_movement_sum_cropped << ","//22
		<< spatial_averaged_movement_sum_uncropped << ","//23
		<< spatial_averaged_movement_score_uncropped << ","//24
		<< spatial_averaged_movement_score_cropped << ","//25
		<< total_intensity_in_previous_frame_scaled_to_current_frames_histogram << ","//26
		<< total_stabilized_area << ","//27
		<< change_in_total_stabilized_intensity_1x << ","//28
		<< change_in_total_stabilized_intensity_2x << ","//29
		<< change_in_total_stabilized_intensity_4x << ","//30
		<< total_intensity_within_stabilized_denoised << ","
		<< change_in_total_outside_stabilized_intensity_1x << ","//31
		<< change_in_total_outside_stabilized_intensity_2x << ","//32
		<< change_in_total_outside_stabilized_intensity_4x << ","//33
		<< total_intensity_outside_stabilized_denoised;
		#ifdef NS_CALCULATE_OPTICAL_FLOW
		o << ","; //deliberately left blank to mark old record format
		scaled_flow_magnitude.write(o); o << ",";
		raw_flow_magnitude.write(o); o << ",";
		scaled_flow_dx.write(o); o << ",";
		scaled_flow_dy.write(o); o << ",";
		raw_flow_dx.write(o); o << ",";
		raw_flow_dy.write(o);
		#endif

	for (unsigned int i = 0; i < 2; i++)
		out << ",";
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::save_movement_data_to_disk(ostream & o) const{
	o << this->analysis_id << ",v:,"<< posture_model_version_used<<"\n";
	for (unsigned long i = 0; i < groups.size(); i++){
		for (unsigned long j = 0; j < groups[i].paths.size(); j++){
			for (unsigned long k = 0; k < groups[i].paths[j].elements.size(); k++){
				o << i << ","//1
					<< j << ","//2
					<< k << ","//3
					<< groups[i].paths[j].elements[k].absolute_time << ",";//4
				groups[i].paths[j].elements[k].measurements.write(o,groups[i].paths[j].elements[k].registration_offset, groups[i].paths[j].elements[k].saturated_offset);
				o <<"\n";
			}
		}
	}
}


std::string ns_calc_rel_time_by_index(const unsigned long time, const ns_movement_state_observation_boundary_interval & i, const ns_analyzed_image_time_path & path){
	if (i.skipped)
		return "";
	return ns_to_string_short((((long)time)-(long)path.state_entrance_interval_time(i).best_estimate_event_time_for_possible_partially_unbounded_interval())/(60.0*60*24),3);
}


std::string ns_normalize_indexed_time(const ns_movement_state_observation_boundary_interval & i, const unsigned long time, const ns_analyzed_image_time_path & path){
	if (i.skipped)
		return "";
	return ns_to_string_short((((long)(long)path.state_entrance_interval_time(i).best_estimate_event_time_for_possible_partially_unbounded_interval()-(long)time))/(60.0*60*24),4);
}


std::string ns_normalize_abs_time(const long abs_time, const unsigned long time){
	if (abs_time == -1)
		return "";
	return ns_to_string_short((((long)abs_time-(long)time))/(60.0*60*24),4);
}

std::string ns_output_interval_difference(const unsigned long time, const ns_death_time_annotation_time_interval & b){
	if (b.period_end_was_not_observed)
		return "";
	return ns_to_string_short(((double)time-(double)b.period_end)/(60.0*60*24),3);
}

std::string ns_output_best_guess_interval_difference(const unsigned long time, const ns_movement_state_observation_boundary_interval& machine, const ns_death_time_annotation_time_interval & by_hand, const ns_analyzed_image_time_path& path) {
	if (by_hand.period_end_was_not_observed && machine.skipped)
		return "";
	if (!by_hand.period_end_was_not_observed)
		return ns_output_interval_difference(time, by_hand);
	return ns_calc_rel_time_by_index(time,machine,path);
}



void ns_analyzed_image_time_path::write_posture_analysis_optimization_data_header(std::ostream & o){
	o << "Experiment,Device,Plate Name,Animal Details,Group ID,Path ID,Excluded,Censored,Number of Worms in Clump,"
		"Movement Threshold, Min Hold Time (Hours), Denoising Technique Used, "
		"Visual Inspection Death Age (Days),Machine Death Age (Days), Visual Inspection Death Time (Date), Difference Between Machine and By Hand Death Times (Days), Sqrt(Difference Squared) (Days), Random Group";
}
std::vector< std::vector < unsigned long > > static_messy_death_time_matrix;
 void ns_analyzed_image_time_path::write_posture_analysis_optimization_data(const std::string & software_version_number,const ns_stationary_path_id & id, const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m,const ns_time_series_denoising_parameters & denoising_parameters,std::ostream & o, ns_parameter_optimization_results & results, ns_parameter_optimization_results * results_2) const{

	
	const unsigned long by_hand_death_time(by_hand_annotation_event_times[(int)ns_movement_cessation].period_end);
	if (by_hand_annotation_event_times[(int)ns_movement_cessation].fully_unbounded())
		return;
	results.number_valid_worms++;
	if (results_2 != 0)
		results_2->number_valid_worms++;
	calculate_posture_analysis_optimization_data(thresholds,hold_times,static_messy_death_time_matrix, software_version_number);
	const unsigned long random_group(rand()%2);
	for (unsigned int i = 0; i < thresholds.size(); i++)
		for (unsigned int j = 0; j < hold_times.size(); j++){

			const double err(((double)static_messy_death_time_matrix[i][j] - by_hand_death_time)/(60.0*60.0*24.0));
			const double err_sq = err*err;
			results.death_total_mean_square_error_in_hours[i][j] += err_sq;
			results.counts[i][j] ++;
			if (results_2 != 0) {
				results_2->death_total_mean_square_error_in_hours[i][j] += err_sq;
				results_2->counts[i][j] ++;
			}
			if (log(thresholds[i]) > 20) {
				cerr << "Invalid Threshold\n";
				continue;
			}
			o << m.experiment_name << "," << m.device << "," << m.plate_name() << "," << m.plate_type_summary() 
				<< "," << id.group_id << "," << id.path_id << ","
				<< (censoring_and_flag_details.is_excluded()?"1":"0") << ","
				<< (censoring_and_flag_details.is_censored()?"1":"0") << ","
				<< censoring_and_flag_details.number_of_worms() << ","
				<< thresholds[i] << "," << (hold_times[j])/60.0/60.0 << "," 
				<< denoising_parameters.to_string() << ","
				<< (by_hand_death_time - m.time_at_which_animals_had_zero_age)/(60.0*60.0*24)  << ","
				<< (static_messy_death_time_matrix[i][j] - m.time_at_which_animals_had_zero_age)/(60.0*60.0*24)  << ","
				<< by_hand_death_time << ","
				<< err << "," << sqrt(err_sq) << "," << random_group << "\n";
		}
}
 void ns_analyzed_image_time_path::write_expansion_analysis_optimization_data_header(std::ostream & o) {
	 o << "Experiment,Device,Plate Name,Animal Details,Group ID,Path ID,Excluded,Censored,Number of Worms in Clump,"
		 "Expansion Slope Threshold, Time Kernel Width (Days), "
		 "Visual Inspection Death Age (Days),"
		 "Visual Inspection Death Time (Date),"
		 "Expansion Start Difference (Days), Expansion Stop Difference (Days), Average Difference (Days), Average Difference Squared (Days), Random Group";
 }
 std::vector< ns_death_time_expansion_info> expansion_intervals;
 void ns_analyzed_image_time_path::write_expansion_analysis_optimization_data(const ns_stationary_path_id & id, const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m,  std::ostream & o, ns_parameter_optimization_results & results, ns_parameter_optimization_results * results_2) const {
	
	 if (by_hand_annotation_event_times[(int)ns_movement_cessation].fully_unbounded() ||
		 by_hand_annotation_event_times[(int)ns_death_associated_expansion_start].fully_unbounded() ||
		 by_hand_annotation_event_times[(int)ns_death_associated_expansion_stop].fully_unbounded())
		 return;

	const unsigned long death_time(by_hand_annotation_event_times[(int)ns_movement_cessation].period_end);
	
	const unsigned long time_at_which_death_time_expansion_started = by_hand_annotation_event_times[(int)ns_death_associated_expansion_start].period_end,
				   time_at_which_death_time_expansion_stopped = by_hand_annotation_event_times[(int)ns_death_associated_expansion_stop].period_end;
	 results.number_valid_worms++;
	 if (results_2 != 0)
		 results_2->number_valid_worms++;
	 expansion_intervals.resize(0);
	 //results.number_valid_worms++;
	 calculate_expansion_analysis_optimization_data(death_time,thresholds, hold_times, expansion_intervals);
	 const unsigned long random_group(rand() % 2);

	 for (unsigned int j = 0; j < hold_times.size(); j++) {
		 for (unsigned int i = 0; i < thresholds.size(); i++) {
			 ns_death_time_expansion_info & res(expansion_intervals[j*thresholds.size() + i]);
			 if (!res.found_death_time_expansion)
				 continue;
			 const unsigned long exp_start = elements[res.time_point_at_which_death_time_expansion_started].absolute_time;
			 const unsigned long exp_stop = elements[res.time_point_at_which_death_time_expansion_stopped].absolute_time;


			 const double start_err(((double)exp_start - time_at_which_death_time_expansion_started) / (60.0*60.0*24.0)),
				 stop_err(((double)exp_stop - time_at_which_death_time_expansion_stopped) / (60.0*60.0*24.0));

			 const double start_err_sq = start_err*start_err,
				 stop_err_sq = stop_err*stop_err;
			 const double avg_err_sq = (start_err_sq + stop_err_sq) / 2;
			 results.death_total_mean_square_error_in_hours[i][j] += avg_err_sq;
			 results.counts[i][j] ++;
			 if (results_2 != 0) {
				 results_2->death_total_mean_square_error_in_hours[i][j] += avg_err_sq;
				 results_2->counts[i][j] ++;
			 }

			 o << m.experiment_name << "," << m.device << "," << m.plate_name() << "," << m.plate_type_summary()
				 << "," << id.group_id << "," << id.path_id << ","
				 << (censoring_and_flag_details.is_excluded() ? "1" : "0") << ","
				 << (censoring_and_flag_details.is_censored() ? "1" : "0") << ","
				 << censoring_and_flag_details.number_of_worms() << ","
				 << thresholds[i] << "," << (hold_times[j]) / 60.0 / 60.0/24.0 << ","
				 << (death_time - m.time_at_which_animals_had_zero_age) / (60.0*60.0 * 24) << ","
				 << death_time << ","
				 << sqrt(start_err_sq) << "," << sqrt(stop_err_sq) << ","
				 << sqrt(avg_err_sq) << "," << avg_err_sq  << "," << random_group << "\n";
		 }
	 }
 }

 std::vector< std::vector < std::pair<unsigned long, unsigned long> > > static_messy_expansion_interval_matrix;
void ns_analyzed_image_time_path::calculate_posture_analysis_optimization_data(const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, std::vector< std::vector < unsigned long > > & death_times, const std::string & software_version) const{
	//find first valid observation
	unsigned long start_i(0);
	for (start_i = 0; start_i < elements.size(); start_i++){
		if (!elements[start_i].excluded && !elements[start_i].element_before_fast_movement_cessation)
			break;
	}
	if (start_i == elements.size())
		throw ns_ex("No valid measurements found in the time path.");
	std::vector<std::vector<unsigned long> > last_time_point_at_which_movement_was_found(
	thresholds.size(),
	std::vector<unsigned long>(hold_times.size(),elements[start_i].absolute_time));

	
	death_times.resize(thresholds.size());
	for (unsigned long i = 0; i < thresholds.size(); i++) {
		death_times[i].resize(0);
		death_times[i].resize(hold_times.size(), 0);
	}

	for (long t = start_i; t < elements.size(); t++){
		
		if (elements[t].excluded) continue;

		double r((software_version=="1")?elements[t].measurements.death_time_posture_analysis_measure_v1():
									   elements[t].measurements.death_time_posture_analysis_measure_v2_cropped());
		const unsigned long &cur_time (elements[t].absolute_time);
		//keep on pushing the last posture time and last sationary
		//times forward until we hit a low enough movement ratio
		//to meet the criteria.  At that point, the last posture 
		//and last stationary cutoffs stick
		
		for (unsigned int thresh = 0; thresh < thresholds.size(); thresh++){
			for (unsigned int hold_t = 0; hold_t < hold_times.size(); hold_t++){
				if (death_times[thresh][hold_t] != 0) continue;
				if (r >= thresholds[thresh])
					last_time_point_at_which_movement_was_found[thresh][hold_t] = cur_time;
				if (death_times[thresh][hold_t] == 0){
					unsigned long dt;
					if (last_time_point_at_which_movement_was_found[thresh][hold_t] == 0)
						dt = cur_time - elements[start_i].absolute_time;
					else dt = cur_time - last_time_point_at_which_movement_was_found[thresh][hold_t];
					if (dt >= hold_times[hold_t])
						death_times[thresh][hold_t] = last_time_point_at_which_movement_was_found[thresh][hold_t];
				}
			}
		}
	}
	for (unsigned int thresh = 0; thresh < thresholds.size(); thresh++){
			for (unsigned int hold_t = 0; hold_t < hold_times.size(); hold_t++){
				if (death_times[thresh][hold_t] == 0)
					death_times[thresh][hold_t] = elements.rbegin()->absolute_time;
			}
	}
}
void ns_analyzed_image_time_path::calculate_expansion_analysis_optimization_data(const unsigned long actual_death_time,const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, std::vector< ns_death_time_expansion_info>  & expansion_intervals) const {
	long min_time(INT_MAX);
	int death_index(0);
	for (unsigned int i = 0; i < this->elements.size(); i++) {
		long dt = abs((long)(elements[i].absolute_time - (long)actual_death_time));
		if (dt < min_time) {
			min_time = dt;
			death_index = i;
		}
	}
	std::vector<ns_s64_bit> tmp;
	identify_expansion_time(death_index, thresholds, hold_times, expansion_intervals, tmp);
}

void ns_analyzed_image_time_path::calculate_movement_quantification_summary(ns_movement_analysis_result& result) const {

	result.quantification_summary.mean_before_death.zero();
	result.quantification_summary.mean_after_death.zero();
	result.quantification_summary.variability_all.zero();
	result.quantification_summary.variability_before_death.zero();
	result.quantification_summary.variability_after_death.zero();
	result.quantification_summary.mean_all.zero();
	result.quantification_summary.count_after_death = 0;
	result.quantification_summary.count_before_death = 0;
	result.quantification_summary.count_all = 0;
	result.quantification_summary.number_of_registration_saturated_frames_before_death = 0;
	result.quantification_summary.number_of_registration_saturated_frames_after_death = 0;

	const long death_index((!result.state_intervals[(int)ns_movement_stationary].skipped)?
		result.state_intervals[(int)ns_movement_stationary].entrance_interval.period_end_index:(-1));
	if (death_index != -1){
		for (unsigned int i = 0; i <= death_index; i++){
			result.quantification_summary.count_before_death++;
		//	elements[i].measurements.calculate_means();
			result.quantification_summary.mean_before_death = result.quantification_summary.mean_before_death + elements[i].measurements;
			if (elements[i].saturated_offset)
				result.quantification_summary.number_of_registration_saturated_frames_before_death++;
		}
	}	
	for (unsigned int i = death_index+1; i < elements.size();i++){
	result.quantification_summary.count_after_death++;
	//	elements[i].measurements.calculate_means();
		result.quantification_summary.mean_after_death = result.quantification_summary.mean_after_death + elements[i].measurements;
		if (elements[i].saturated_offset)
			result.quantification_summary.number_of_registration_saturated_frames_after_death++;
	}
	
	result.quantification_summary.count_all = result.quantification_summary.count_before_death + result.quantification_summary.count_after_death;
	result.quantification_summary.mean_all = result.quantification_summary.mean_before_death + result.quantification_summary.mean_after_death;
	
	if (result.quantification_summary.count_all != 0) 
		result.quantification_summary.mean_all = result.quantification_summary.mean_all / result.quantification_summary.count_all;
	if (result.quantification_summary.count_before_death != 0) 
		result.quantification_summary.mean_before_death = result.quantification_summary.mean_before_death / result.quantification_summary.count_before_death;
	if (result.quantification_summary.count_after_death != 0) 
		result.quantification_summary.mean_after_death = result.quantification_summary.mean_after_death / result.quantification_summary.count_after_death;

	if (death_index != -1){
		for (unsigned int i = 0; i <= death_index; i++){
			
			ns_analyzed_image_time_path_element_measurements dif((elements[i].measurements + result.quantification_summary.mean_before_death/-1));
			dif.square();
			result.quantification_summary.variability_before_death = result.quantification_summary.variability_before_death + dif;

			dif = (elements[i].measurements + result.quantification_summary.mean_all/-1);
			dif.square();
			result.quantification_summary.variability_all = result.quantification_summary.variability_all + dif;
		}
	}	
	for (unsigned int i = death_index+1; i < elements.size();i++){
		ns_analyzed_image_time_path_element_measurements dif((elements[i].measurements + result.quantification_summary.mean_after_death/-1));
		dif.square();
		result.quantification_summary.variability_after_death = result.quantification_summary.variability_after_death + dif;

		dif = (elements[i].measurements + result.quantification_summary.mean_all/-1);
		dif.square();
		result.quantification_summary.variability_all = result.quantification_summary.variability_all + dif;
	}

	result.quantification_summary.variability_after_death.square_root();
	result.quantification_summary.variability_before_death.square_root();
	result.quantification_summary.variability_all.square_root();

	if (result.quantification_summary.count_all != 0) 
		result.quantification_summary.variability_all = result.quantification_summary.variability_all / result.quantification_summary.count_all;
	if (result.quantification_summary.count_before_death != 0) 
		result.quantification_summary.variability_before_death = result.quantification_summary.variability_before_death / result.quantification_summary.count_before_death;
	if (result.quantification_summary.count_after_death != 0) 
		result.quantification_summary.variability_after_death = result.quantification_summary.variability_after_death / result.quantification_summary.count_after_death;
}

void ns_analyzed_image_time_path::write_detailed_movement_quantification_analysis_header(std::ostream & o){
	ns_region_metadata::out_JMP_plate_identity_header_short(o);
	o << ",";
	o << "Group ID, Path ID, Excluded,Flag,Censored,Number of Animals In Clump, Extra Worm Count,"
		"Unregistered Object Center X, Unregistered Object Center Y,"
		// "Offset from Path Magnitude, Offset within Registered Image Magnitude,"
		"Registration Offset X, Registration Offset Y, Registration Offset Magnitude,"
		"Absolute Time, Age Relative Time,"
		"Machine-Annotated Movement State,By Hand Annotated Movement State,"
		"Machine Movement Cessation Relative Time, Machine Slow Movement Cessation Relative Time, Machine Fast Movement Cessation Relative Time,Machine Death-Associated Expansion Time,Machine Post-mortem Contraction Time,"
		"By Hand Movement Cessation Relative Time, By Hand Slow Movement Cessation Relative Time, By Hand Fast Movement Cessation Relative Time,By Hand Death-Associated Expansion Time,By Hand Post-mortem Contraction Time,"
		"Best Guess Movement Cessation Relative Time, Best Guess Slow Movement Cessation Relative Time, Best Guess Fast Movement Cessation Relative Time,Best Guess Death-Associated Expansion Time,Best Guess Post-mortem Contraction Time,"
		"Event-Aligned time,"
		"Movement Sum, Movement Score, Denoised Movement Score,"
		"Spatially Averaged Movement Sum Cropped,"
		"Spatially Averaged Movement Sum Uncropped,"
		"Spatially Averaged Movement Score Cropped,"
		"Spatially Averaged Movement Score Uncropped,"
		"Movement quantification used to identify threshold death,"
		"Movement quantification used to identify HMM death,"
		"Movement Alternate Worm Sum,"
		"Total Foreground Area, Total Stabilized Area, Total Region Area,Total Alternate Worm Area,"
		"Total Foreground Intensity, Total Stabilized Intensity,Total Region Intensity,Total Alternate Worm Intensity,"
		"Change in Foreground Intensity (pix/hour),"
		"Change in Stabilized Intensity 1x(pix/hour),Change in Stabilized Intensity 2x(pix/hour),Change in Stabilized Intensity 4x(pix/hour),"
		"Change in Region Intensity (pix/hour),"
		"Saturated Registration, Machine Error (Days ),"
		"Movement Cessation Time,"
		"Total Foreground Area at Movement Cessation,"
		"Total Stablilized Intensity at Movement Cessation,"
		"Death-Associated Expansion Cessation Time,"
		"Total Foreground Area at Death-Associated Expansion ,"
		"Total Stablilized Intensity at Death-Associated Expansion";

	#ifdef NS_CALCULATE_OPTICAL_FLOW
	ns_optical_flow_quantification::write_header("Scaled Flow Magnitude", o); o << ",";
	ns_optical_flow_quantification::write_header("Raw Flow Magnitude", o); o << ",";
	ns_optical_flow_quantification::write_header("Scaled Flow dx", o); o << ",";
	ns_optical_flow_quantification::write_header("Scaled Flow dy", o); o << ",";
	ns_optical_flow_quantification::write_header("Raw Flow dx", o); o << ",";
	ns_optical_flow_quantification::write_header("Raw Flow dy", o);
#endif
	for (unsigned int i = 0; i < this->posture_quantification_extra_debug_field_names.size(); i++){
		o << "," << posture_quantification_extra_debug_field_names[i];

	}
}
ns_death_time_annotation_time_interval ns_analyzed_image_time_path::machine_event_time(const ns_movement_event & e, bool & skipped) const {
	ns_movement_state_observation_boundary_interval interval;
	switch (e) {
	case ns_translation_cessation:
		interval = movement_analysis_result.state_intervals[(int)ns_movement_fast]; break;
	case ns_fast_movement_cessation:
		interval = (movement_analysis_result.state_intervals[(int)ns_movement_slow]); break;
	case ns_movement_cessation:
		interval = (movement_analysis_result.state_intervals[(int)ns_movement_stationary]); break;
	case ns_death_associated_expansion_start:
		interval = (movement_analysis_result.state_intervals[(int)ns_movement_death_associated_expansion]); break;
	case ns_death_associated_post_expansion_contraction_start:
		interval = (movement_analysis_result.state_intervals[(int)ns_movement_death_associated_post_expansion_contraction]); break;
	default: ns_ex("Unknown event type!");
	}
	skipped = interval.skipped;
	if (skipped) {
		ns_death_time_annotation_time_interval a;
		a.period_end_was_not_observed = true;
		a.period_start_was_not_observed = true;
		return a;
	}
	return state_entrance_interval_time(interval);
}

//used to time warp time series so that all the transitions between movement states match up.
class ns_time_series_event_aligner {
public:
	void calculate_alignment_dts(const std::vector<ns_death_time_annotation_time_interval>& intervals) {
		offsets.resize(0);
		offsets.resize(intervals.size(), 0);
		dt.resize(0);
		dt.resize(intervals.size(), 0);
		for (unsigned long int i = 0; i < intervals.size(); i++) {
			if (intervals[i].period_end_was_not_observed)
				continue;
			offsets[i] = intervals[i].period_end;
			if (i > 0 && offsets[i - 1] != 0)
				dt[i - 1] = offsets[i] - offsets[i - 1];
		}
	}
	static double event_reference_point(const ns_movement_state& state) {
		switch (state) {
		case ns_movement_fast: return 0;
		case ns_movement_slow: return 1;
		case ns_movement_posture: return 2;
		case ns_movement_stationary: return 3;
		case ns_movement_death_associated_expansion: return 4;
		case ns_movement_death_associated_post_expansion_contraction: return 5;
		default: throw ns_ex("Unknown reference point state");
		}
	}
	double rel_dt(const ns_movement_state& state, const unsigned long & time) const{
		if (offsets[state] == 0)
			return -1;
		return event_reference_point(state) + (time - offsets[(int)state]) / dt[(int)state];
	}
private:
	std::vector<unsigned long> offsets;
	std::vector<double> dt;
};
struct ns_event_time_to_use {
	ns_event_time_to_use() :time(0), event_index(0),before_index(0),after_index(0),found(false) {}
	ns_movement_event event_type;
	unsigned long time;
	unsigned long event_index,before_index, after_index;
	bool found;
};

struct ns_basic_stats {
	ns_basic_stats() :max(-DBL_MAX), min(DBL_MAX), sum(0), average(0), var(0) {}
	double max, min, sum, average, var;
	unsigned long N;
	template<class T>
	void reg(const T& t) {
		N++;
		if (t < min) min = t;
		if (t > max) max = t;
		average += t;
	}
	static void write_header(const std::string& label, std::ostream& o) {
		o << label << " Max," << label << " Min," << label << " Average," << label << " Variance";
	}
	void write(ostream& o) {
		o << max << "," << min << "," << average << "," << var;
	}
};
void ns_analyzed_image_time_path::write_path_classification_diagnostics_header(std::ostream& o) {
	ns_region_metadata::out_JMP_plate_identity_header_short(o);
	o << ",Group ID, Path ID, Excluded,Flag,Censored,Number of Animals In Clump, Number of Images,Solution loglikelihood,Animal died, Animal Expanded,Low Density,First Element Time (days), last Element time (days),";
	ns_basic_stats::write_header("Intensity Slope", o);
	o << ","; ns_basic_stats::write_header("Intensity Value", o);
	o << ","; ns_basic_stats::write_header("Movement", o);
}

void ns_analyzed_image_time_path::write_path_classification_diagnostics_data(const ns_region_metadata& m, const unsigned long group_id, const unsigned long path_id, std::ostream& o, const bool output_only_elements_with_hand) const {
	if (output_only_elements_with_hand && by_hand_annotation_event_times[(int)ns_movement_cessation].period_end_was_not_observed)
		return;
	m.out_JMP_plate_identity_data_short(o);
	o << ",";
	o << group_id << "," << path_id << ","
		<< ((censoring_and_flag_details.is_excluded() || censoring_and_flag_details.flag.event_should_be_excluded() || this->entirely_excluded) ? "1" : "0") << ",";
	if (censoring_and_flag_details.flag.specified())
		o << censoring_and_flag_details.flag.label_short;
	o << ","
		<< (censoring_and_flag_details.is_censored() ? "1" : "0") << ","
		<< censoring_and_flag_details.number_of_worms() << ",";

	o << this->elements.size() << ",";
	if (death_time_annotations().empty())
		o << ",";
	else o << death_time_annotations().events.begin()->loglikelihood << ",";
	o << this->movement_analysis_result.animal_died() << ",";
	o << this->movement_analysis_result.animal_contracted() << ",";

	o << this->low_density_path << ",";
	o << (elements.begin()->absolute_time - first_stationary_timepoint()) / 60.0 / 60/24.0 << "," << (elements.rbegin()->absolute_time - first_stationary_timepoint()) / 60.0 / 60 / 24.0 << ",";
	ns_basic_stats intensity_slope, intensity_val, movement_val;
	for (unsigned int i = 0; i < elements.size(); i++) {
		intensity_slope.reg(elements[i].measurements.change_in_total_stabilized_intensity_4x);
		intensity_val.reg(elements[i].measurements.total_intensity_within_stabilized_denoised);
		movement_val.reg(elements[i].measurements.death_time_posture_analysis_measure_v2_uncropped());
	}
	intensity_slope.average /= intensity_slope.N;
	intensity_val.average /= intensity_val.N;
	movement_val.average /= movement_val.N;

	for (unsigned int i = 0; i < elements.size(); i++) {
		intensity_slope.var += pow(elements[i].measurements.change_in_total_stabilized_intensity_1x - intensity_slope.average, 2);
		intensity_val.var += pow(elements[i].measurements.total_intensity_within_stabilized_denoised - intensity_val.average, 2);
		movement_val.var += pow(elements[i].measurements.death_time_posture_analysis_measure_v2_uncropped() - movement_val.average, 2);
	}
	intensity_slope.var = sqrt(intensity_slope.var / intensity_slope.N);
	intensity_val.var = sqrt(intensity_val.var / intensity_val.N);
	movement_val.var = sqrt(movement_val.var / movement_val.N);
	intensity_slope.write(o);
	o << ",";
	intensity_val.write(o);
	o << ",";
	movement_val.write(o);
	o << "\n";
}


void ns_analyzed_image_time_path::write_detailed_movement_quantification_analysis_data(const ns_region_metadata & m, const unsigned long group_id, const unsigned long path_id, std::ostream & o, const bool output_only_elements_with_hand,const bool abbreviated_time_series)const{
	if (output_only_elements_with_hand && by_hand_annotation_event_times[(int)ns_movement_cessation].period_end_was_not_observed)
		return;
	//find animal size one day before death
	//double death_relative_time_to_normalize(0);
	double time_after_event_time_to_write_in_abbreviated(4*24*60*60);
	double time_before_event_time_to_write_in_abbreviated(2 * 24*60 * 60);
	
	std::map<ns_movement_state, ns_event_time_to_use > state_times;
	state_times[ns_movement_stationary].event_type = ns_movement_cessation;
	state_times[ns_movement_death_associated_expansion].event_type = ns_death_associated_expansion_start;

	for (auto p = state_times.begin(); p != state_times.end(); ++p) {
		if (movement_analysis_result.state_intervals[(int)p->first].skipped && by_hand_annotation_event_times[(int)p->second.event_type].period_end_was_not_observed)
			continue;
		p->second.found = true;
		long least_dt_to_death(LONG_MAX);
		if (!by_hand_annotation_event_times[(int)p->second.event_type].period_end_was_not_observed)
			p->second.time = by_hand_annotation_event_times[(int)p->second.event_type].period_end;
		else{
				bool skipped;
				p->second.time = machine_event_time(p->second.event_type, skipped).best_estimate_event_time_for_possible_partially_unbounded_interval();
		}


		for (unsigned long k = 0; k < elements.size(); k++) {
			const double dt((double)p->second.time - elements[k].absolute_time);
			if (dt > time_before_event_time_to_write_in_abbreviated)
				p->second.before_index = k;
			if (-time_after_event_time_to_write_in_abbreviated < dt)
				p->second.after_index = k;
			const double adt(fabs(dt));
			if (adt < least_dt_to_death) {
				least_dt_to_death = adt;
				p->second.event_index = k;
			}
		}
	}
	
	//set up start and end indicies to output to file
	unsigned long start_index(0),end_index(elements.size());
	if (abbreviated_time_series) {
		if (state_times[ns_movement_death_associated_expansion].found)
			end_index = state_times[ns_movement_death_associated_expansion].after_index;
		if (state_times[ns_movement_stationary].found && state_times[ns_movement_stationary].after_index > end_index)
			end_index = state_times[ns_movement_stationary].after_index;
		
		if (state_times[ns_movement_stationary].found)
			start_index = state_times[ns_movement_stationary].before_index;
		if (state_times[ns_movement_death_associated_expansion].found  && state_times[ns_movement_death_associated_expansion].before_index < start_index)
			start_index = state_times[ns_movement_death_associated_expansion].before_index;

	}

	ns_time_series_event_aligner event_aligner;
	event_aligner.calculate_alignment_dts(by_hand_annotation_event_times);
	for (unsigned long k = start_index; k < end_index; k++){
		m.out_JMP_plate_identity_data_short(o);
		o << ",";
		o << group_id<<","<<path_id<<","
			<< ((censoring_and_flag_details.is_excluded() || censoring_and_flag_details.flag.event_should_be_excluded()) ?"1":"0")<< ",";                                               
		if (censoring_and_flag_details.flag.specified())
				o << censoring_and_flag_details.flag.label_short;
			o << ","
			<< (censoring_and_flag_details.is_censored()?"1":"0") << ","
			<< censoring_and_flag_details.number_of_worms() << ","
			<< elements[k].number_of_extra_worms_observed_at_position << ","
			<< elements[k].region_offset_in_source_image().x + elements[k].worm_region_size().x/2 << ","
			<< elements[k].region_offset_in_source_image().y + elements[k].worm_region_size().y/2 << ","
			// "Offset from Path Magnitude, Offset within Registered Image Magnitude,"
		// "Registration Offset X, Registration Offset Y, Registration Offset Magnitude,"
		// "Absolute Time, Age Relative Time,"
			<< elements[k].measurements.registration_displacement.x << ","
			<< elements[k].measurements.registration_displacement.y << ","
			<< elements[k].measurements.registration_displacement.mag() << ","
			<< elements[k].absolute_time << ","
			<< ns_to_string_short((elements[k].absolute_time - m.time_at_which_animals_had_zero_age)/(60.0*60*24),3) << ","
			<< ns_movement_state_to_string(explicitly_recognized_movement_state(elements[k].absolute_time)) << ","
			<< ns_movement_state_to_string(by_hand_movement_state(elements[k].absolute_time)) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_stationary],*this) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_posture],*this) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_slow],*this) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_death_associated_expansion], *this) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_death_associated_post_expansion_contraction], *this) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_movement_cessation]) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_translation_cessation]) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_fast_movement_cessation]) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_death_associated_expansion_start]) << ","
			<< ns_output_interval_difference(elements[k].absolute_time, by_hand_annotation_event_times[(int)ns_death_associated_post_expansion_contraction_start]) << ","
			<< ns_output_best_guess_interval_difference(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_stationary],	by_hand_annotation_event_times[(int)ns_movement_cessation], *this) << ","
			<< ns_output_best_guess_interval_difference(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_posture],	by_hand_annotation_event_times[(int)ns_translation_cessation], *this) << ","
			<< ns_output_best_guess_interval_difference(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_slow], by_hand_annotation_event_times[(int)ns_fast_movement_cessation], *this) << ","
				<< ns_output_best_guess_interval_difference(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_death_associated_expansion], by_hand_annotation_event_times[(int)ns_death_associated_expansion_start], *this) << ","
			<< ns_output_best_guess_interval_difference(elements[k].absolute_time, movement_analysis_result.state_intervals[(int)ns_movement_death_associated_post_expansion_contraction],by_hand_annotation_event_times[(int)ns_death_associated_post_expansion_contraction_start], *this) << ","
			<< event_aligner.rel_dt(by_hand_movement_state(elements[k].absolute_time),elements[k].absolute_time) << ",";

		
			o << elements[k].measurements.movement_sum << ",";                                                               	
			o << elements[k].measurements.movement_score << ","
				<< elements[k].measurements.denoised_movement_score << ","
				<< elements[k].measurements.spatial_averaged_movement_sum_cropped << ","
				<< elements[k].measurements.spatial_averaged_movement_sum_uncropped << ","
				<< elements[k].measurements.spatial_averaged_movement_score_cropped << ","
				<< elements[k].measurements.spatial_averaged_movement_score_uncropped << ","
				<< elements[k].measurements.death_time_posture_analysis_measure_v2_cropped() << ","
				<< elements[k].measurements.death_time_posture_analysis_measure_v2_uncropped() << ","
				//	<< elements[k].measurements.total_intensity_in_previous_frame_scaled_to_current_frames_histogram << ","		
				<< elements[k].measurements.movement_alternate_worm_sum << ","													
				<< elements[k].measurements.total_foreground_area << ","														
				<< elements[k].measurements.total_stabilized_area << ","														
				<< elements[k].measurements.total_region_area << ","															
				<< elements[k].measurements.total_alternate_worm_area << ","
				<< elements[k].measurements.total_intensity_within_foreground << ","
				<< elements[k].measurements.total_intensity_within_stabilized << ","
				<< elements[k].measurements.total_intensity_within_region << ","
				<< elements[k].measurements.total_intensity_within_alternate_worm << ","

				<< elements[k].measurements.change_in_total_foreground_intensity << ","
				<< elements[k].measurements.change_in_total_stabilized_intensity_1x << ","
				<< elements[k].measurements.change_in_total_stabilized_intensity_2x << ","
				<< elements[k].measurements.change_in_total_stabilized_intensity_4x << ","
				<< elements[k].measurements.change_in_total_region_intensity << ","
			
			
		
			<< (elements[k].saturated_offset ? "1" : "0") << ",";
		if (movement_analysis_result.state_intervals[(int)ns_movement_stationary].skipped)
			 o << ",";
		else o << ns_output_interval_difference(this->state_entrance_interval_time(movement_analysis_result.state_intervals[(int)ns_movement_stationary]).
													best_estimate_event_time_for_possible_partially_unbounded_interval(),
													by_hand_annotation_event_times[(int)ns_movement_cessation]) <<",";

		if (!state_times[ns_movement_stationary].found)
			o << ",,,";
		else o << state_times[ns_movement_stationary].time << ","
			<<elements[state_times[ns_movement_stationary].event_index].measurements.total_foreground_area << ","
			<< elements[state_times[ns_movement_stationary].event_index].measurements.total_stabilized_area << ",";
		if (!state_times[ns_movement_death_associated_expansion].found)
			o << ",,";
		else o << state_times[ns_movement_death_associated_expansion].time << ","
			<< elements[state_times[ns_movement_death_associated_expansion].event_index].measurements.total_foreground_area << ","
			<< elements[state_times[ns_movement_death_associated_expansion].event_index].measurements.total_stabilized_area;

	#ifdef NS_CALCULATE_OPTICAL_FLOW
		elements[k].measurements.scaled_flow_magnitude.write(o); o << ",";
		elements[k].measurements.raw_flow_magnitude.write(o); o << ",";
		elements[k].measurements.scaled_flow_dx.write(o); o << ",";
		elements[k].measurements.scaled_flow_dy.write(o); o << ",";
		elements[k].measurements.raw_flow_dx.write(o); o << ",";
		elements[k].measurements.raw_flow_dy.write(o);
#endif
		for (unsigned int i = 0; i < elements[k].measurements.posture_quantification_extra_debug_fields.size(); i++){
			o << "," << elements[k].measurements.posture_quantification_extra_debug_fields[i];
		}
		o << "\n";
	}
}
/*
void ns_time_path_image_movement_analyzer::write_summary_movement_quantification_analysis_data(const ns_region_metadata & m, std::ostream & o)const{

	for (unsigned long i = 0; i < groups.size(); i++){
		for (unsigned long j = 0; j < groups[i].paths.size(); j++){	
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path() || groups[i].paths[j].entirely_excluded)
					continue;
			if (groups[i].paths[j].state_intervals.size() != ns_movement_number_of_states)
				throw ns_ex("ns_time_path_image_movement_analyzer::write_summary_movement_quantification_analysis_data()::Event Indicies not loaded properly!");
			groups[i].paths[j].write_summary_movement_quantification_analysis_data(m,i,j,o);
		}
	}
}*/


template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::write_posture_analysis_optimization_data(const std::string & software_version,const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m,std::ostream & o, ns_parameter_optimization_results & results, ns_parameter_optimization_results * results_2) const{
	srand(0);
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){	
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path() || groups[i].paths[j].excluded() || !groups[i].paths[j].by_hand_data_specified() ||
				groups[i].paths[j].censoring_and_flag_details.number_of_worms_at_location_marked_by_hand > 1)
				//we do not test multi-worm clusters because there is an ambiguity in which of the multiple by hand death times should match up to the machine death time
					continue;
			if (groups[i].paths[j].censoring_and_flag_details.flag.specified() && ( groups[i].paths[j].censoring_and_flag_details.flag.event_should_be_excluded() || 
				groups[i].paths[j].censoring_and_flag_details.flag.label_short == "2ND_WORM_ERR" ||
				groups[i].paths[j].censoring_and_flag_details.flag.label_short == "STILL_ALIVE" ))
				continue;
			groups[i].paths[j].write_posture_analysis_optimization_data(software_version,generate_stationary_path_id(i,j),thresholds,hold_times,m,denoising_parameters_used,o, results,results_2);
		}
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::write_expansion_analysis_optimization_data(const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m, std::ostream & o, ns_parameter_optimization_results & results, ns_parameter_optimization_results * results_2) const {
	for (unsigned int i = 0; i < groups.size(); i++) {
		for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path() || groups[i].paths[j].excluded() || !groups[i].paths[j].by_hand_data_specified())
				continue;
			groups[i].paths[j].write_expansion_analysis_optimization_data(generate_stationary_path_id(i, j), thresholds, hold_times, m,o, results,results_2);
		}
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::write_detailed_movement_quantification_analysis_data(const ns_region_metadata & m, std::ostream & o, const bool only_output_elements_with_by_hand_data, const long specific_animal_id, const bool abbreviated_time_series)const{
	 
	for (unsigned long i = 0; i < groups.size(); i++){
		for (unsigned long j = 0; j < groups[i].paths.size(); j++){
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path() ||
				specific_animal_id != -1 && i != specific_animal_id || groups[i].paths[j].entirely_excluded)
				continue;
			if (groups[i].paths[j].movement_analysis_result.state_intervals.size() != ns_movement_number_of_states)
				throw ns_ex("ns_time_path_image_movement_analyzer::write_movement_quantification_analysis_data()::Event Indicies not loaded properly!");
			groups[i].paths[j].write_detailed_movement_quantification_analysis_data(m,i,j,o,only_output_elements_with_by_hand_data,abbreviated_time_series);
		}
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::write_path_classification_diagnostics_data(const ns_region_metadata& m, std::ostream& o, const bool only_output_elements_with_by_hand_data, const long specific_animal_id)const {

	for (unsigned long i = 0; i < groups.size(); i++) {
		for (unsigned long j = 0; j < groups[i].paths.size(); j++) {
			if (specific_animal_id != -1 && i != specific_animal_id )
				continue;
			if (groups[i].paths[j].movement_analysis_result.state_intervals.size() != ns_movement_number_of_states)
				throw ns_ex("ns_time_path_image_movement_analyzer::write_movement_quantification_analysis_data()::Event Indicies not loaded properly!");
			groups[i].paths[j].write_path_classification_diagnostics_data(m, i, j, o, only_output_elements_with_by_hand_data);
		}
	}
}

/* When making animated graphs of time series data, we want to place a marker on the data point corresponding
	to the current time in the animation.  This marker moves accross data demonstrating the passage of time.
	When set_marker(time,graph) is called, the x_axis of the graph column is read to locate the time
	closest to the time specified as a function argument.  A marker is created in the graph object
	at the time closest to that requested.*/
class ns_marker_manager{
public:
	void set_marker(const unsigned long time,ns_graph & graph){
		unsigned int marker_position = 0;
		if (time == -1){
			for (unsigned int i = 0; i < graph.contents[marker_id]->y.size(); i++){
				graph.contents[marker_id]->y[i] = -1;
				return;
			}
		}
		unsigned int i;
		for (i = 0; i < graph.contents[x_axis_id]->x.size(); i++){
			if (graph.contents[x_axis_id]->x[i] >= (unsigned long)time)
				break;
			graph.contents[marker_id]->y[i] = -1;
		}
		if (i == graph.contents[x_axis_id]->x.size())
			return;
		graph.contents[marker_id]->y[i] = graph.contents[y_axis_id]->y[i];
		for (i = i+1; i < graph.contents[marker_id]->y.size(); i++)
			graph.contents[marker_id]->y[i] = -1;
	}
private:
	enum{y_axis_id = 0,x_axis_id=1,marker_id=2};
};
/*Movement is measured by collecting statistics on pixel changes over time.
ns_make_path_movement_graph produces a graph of those statistics
*/
void ns_make_path_movement_graph(const ns_analyzed_image_time_path & path,ns_graph & graph){

	const unsigned long number_of_measurements(path.element_count());
	if (number_of_measurements == 1)
		throw ns_ex("Not enough measurements!");

	ns_graph_object	movement_ratios(ns_graph_object::ns_graph_dependant_variable);
	movement_ratios.y.resize(number_of_measurements);
	ns_graph_object graph_x_axis(ns_graph_object::ns_graph_independant_variable);
	graph_x_axis.x.resize(number_of_measurements);

	//ns_graph_object cutoff_posture(ns_graph_object::ns_graph_dependant_variable),
	//			    cutoff_stationary(ns_graph_object::ns_graph_dependant_variable);

	//cutoff_posture.y.resize(number_of_measurements);
	//cutoff_stationary.y.resize(number_of_measurements);

	ns_graph_object transition_posture(ns_graph_object::ns_graph_vertical_line),
				    transition_stationary(ns_graph_object::ns_graph_vertical_line);

	ns_graph_object marker(ns_graph_object::ns_graph_dependant_variable);
	marker.y.resize(number_of_measurements,-1);

	

	for (unsigned int i = 0; i < path.element_count(); i++){
		movement_ratios.y[i] = path.element(i).measurements.death_time_posture_analysis_measure_v2_uncropped();
		graph_x_axis.x[i] = path.element(i).absolute_time;
	//	cutoff_stationary.y[i] = ns_analyzed_image_time_path::stationary_cutoff_ratio();
	//	cutoff_posture.y[i] = ns_analyzed_image_time_path::posture_cutoff_ratio();
	}

	bool slow(true),posture(true);
//	transition_posture.y[0] = -1;
//	transition_stationary.y[0] = -1;
	for (unsigned int i = 1; i < path.element_count(); i++){
		ns_movement_state m(path.explicitly_recognized_movement_state(path.element(i).absolute_time));
		if (slow && m == ns_movement_posture){
			transition_posture.x.push_back(path.element(i).absolute_time);
			transition_posture.y.push_back(movement_ratios.y[i]);
			//transition_posture.y[i]=0; 
			slow = false;
		}
	//	else transition_posture.y[i]=-1;
		

		if (posture && m == ns_movement_stationary){
			transition_stationary.x.push_back(path.element(i).absolute_time);
			transition_stationary.y.push_back(movement_ratios.y[i]);
			posture = false;
		}
	//	else transition_stationary.y[i]=-1;
	}

	marker.properties.line.draw = 0;
	marker.properties.area_fill.draw = 0;
	marker.properties.point.draw = true;
	marker.properties.point.color = marker.properties.area_fill.color;
	marker.properties.point.width = 3;
	marker.properties.point.edge_width = marker.properties.point.width/3;
	marker.properties.point.edge_color = ns_color_8(255,255,175);
	marker.properties.draw_negatives = false;

	transition_posture.properties.point.draw = false;
	transition_posture.properties.line.draw = true;
	transition_posture.properties.line.width = 2;
	transition_posture.properties.line_hold_order = ns_graph_properties::ns_first;
	transition_posture.properties.draw_vertical_lines = ns_graph_properties::ns_no_line;
	transition_posture.properties.line.color = ns_color_8(175,0,0);

	//cutoff_stationary.properties = transition_posture.properties;
	//cutoff_stationary.properties =  ns_color_8(175,0,0);;
	//cutoff_posture.properties = transition_stationary.properties;
	//cutoff_posture.properties.line.color = ns_color_8(0,175,0);
	//transition_posture.properties = cutoff_posture.properties;
	transition_posture.properties.draw_vertical_lines = ns_graph_properties::ns_full_line;
	transition_posture.properties.line_hold_order = ns_graph_properties::ns_zeroth_centered;
	transition_posture.properties.draw_negatives = false;
	transition_posture.properties.line.color = ns_color_8(0,255,0);

	transition_stationary.properties = transition_posture.properties;
	transition_stationary.properties.draw_vertical_lines = ns_graph_properties::ns_full_line;
	transition_stationary.properties.line_hold_order = ns_graph_properties::ns_zeroth_centered;
	transition_stationary.properties.draw_negatives = false;
	transition_stationary.properties.line.color = ns_color_8(255,0,0);

	movement_ratios.properties.point.draw = false;
	movement_ratios.properties.line.draw = true;
	movement_ratios.properties.line.width = 2;
	movement_ratios.properties.line_hold_order = ns_graph_properties::ns_first;
	movement_ratios.properties.draw_vertical_lines = ns_graph_properties::ns_no_line;
	movement_ratios.properties.line.color=ns_color_8(125,125,125);

	graph.x_axis_properties.line.color=ns_color_8(175,175,175);
	graph.x_axis_properties.text.draw =false;
	graph.x_axis_properties.point.color=ns_color_8(175,175,175);
	graph.x_axis_properties.area_fill.color=ns_color_8(0,0,0);
	graph.x_axis_properties.draw_tick_marks = false;
	graph.y_axis_properties = 
		graph.area_properties = 
		graph.title_properties = 
		graph.x_axis_properties;



	graph.add_and_store(movement_ratios);
	graph.add_and_store(graph_x_axis);
	graph.add_and_store(marker);
	//graph.contents.push_back(cutoff_stationary);
//	graph.contents.push_back(cutoff_posture);
	//graph.contents.push_back(transition_stationary);
	//graph.contents.push_back(transition_posture);
	ns_graph_axes axes;
	graph.set_graph_display_options("",axes);

}


ns_death_time_annotation_time_interval ns_analyzed_image_time_path::state_entrance_interval_time(const ns_movement_state_observation_boundary_interval & e) const{
	if (e.skipped)
		throw ns_ex("state_entrance_interval_time()::Requesting state entrance time for skipped state!");
	if (e.entrance_interval.interval_occurs_after_observation_interval)
		throw ns_ex("state_entrance_interval_time()::Requesting the entrance time of an interval reported to occur after observation interval!");
	if (e.entrance_interval.interval_occurs_before_observation_interval)
		return time_path_limits.interval_before_first_observation;

	if (e.entrance_interval.period_start_index < 0 || e.entrance_interval.period_start_index >= elements.size() ||
		e.entrance_interval.period_end_index < 0 || e.entrance_interval.period_end_index >= elements.size())
		throw ns_ex("ns_analyzed_image_time_path::state_entrance_interval_time()::Invalid time: ") << e.entrance_interval.period_start_index << " a path with " << elements.size() << " elements";
		
	return ns_death_time_annotation_time_interval(elements[e.entrance_interval.period_start_index].absolute_time,
		elements[e.entrance_interval.period_end_index].absolute_time);
}

ns_death_time_annotation_time_interval ns_analyzed_image_time_path::state_exit_interval_time(const ns_movement_state_observation_boundary_interval & e) const{
	if (e.skipped) throw ns_ex("state_exit_interval_time()::Requesting state exit time for skipped state!");
	if (e.exit_interval.interval_occurs_before_observation_interval)
		return time_path_limits.interval_before_first_observation;
	if (e.exit_interval.interval_occurs_after_observation_interval)	
		throw ns_ex("state_entrance_interval_time()::Requesting the exit time of an interval reported to occur after observation interval!");

	if (e.exit_interval.period_start_index < 0 || e.exit_interval.period_start_index >= elements.size() ||
		e.exit_interval.period_end_index < 0 || e.exit_interval.period_end_index >= elements.size())
		throw ns_ex("ns_analyzed_image_time_path::state_entrance_interval_time()::Invalid time!");
		
	return ns_death_time_annotation_time_interval(elements[e.exit_interval.period_start_index].absolute_time,
													elements[e.exit_interval.period_end_index].absolute_time);
}


void ns_analyzed_image_time_path::add_death_time_events_to_set(ns_death_time_annotation_set & set) const{
	set.add(movement_analysis_result.death_time_annotation_set);
}

long ns_find_last_valid_observation_index(const long index,const ns_analyzed_image_time_path::ns_element_list & e){
	for (long i = index-1; i >= 0; i--){
		if (!e[i].excluded)
			return i;
	}
	return -1;
}



void calculate_state_transitions_in_the_presence_of_missing_states(
	const ns_movement_state_time_interval_indicies & frame_before_first,
	const ns_movement_state_observation_boundary_interval & slow_moving_interval,
	const ns_movement_state_observation_boundary_interval & posture_changing_interval,
	const ns_movement_state_observation_boundary_interval & dead_interval,
	const ns_movement_state_observation_boundary_interval & expansion_interval,
	const ns_movement_state_observation_boundary_interval & post_expansion_contraction_interval,
	ns_movement_state_observation_boundary_interval & slow_moving_interval_including_missed_states,
	ns_movement_state_observation_boundary_interval & posture_changing_interval_including_missed_states,
	ns_movement_state_observation_boundary_interval & dead_interval_including_missed_states,
	ns_movement_state_observation_boundary_interval & expansion_interval_including_missed_states,
	ns_movement_state_observation_boundary_interval & post_expansion_contraction_interval_including_missed_states) {

	slow_moving_interval_including_missed_states = slow_moving_interval;
	posture_changing_interval_including_missed_states = posture_changing_interval;
	dead_interval_including_missed_states = dead_interval;

	//to make it into a path, the animals *have* to have slowed down to slow moving.
	if (slow_moving_interval.skipped) {
		slow_moving_interval_including_missed_states.skipped = false;
		slow_moving_interval_including_missed_states.entrance_interval = frame_before_first;
		if (!posture_changing_interval.skipped)
			slow_moving_interval_including_missed_states.exit_interval = posture_changing_interval.entrance_interval;
		else if (!dead_interval.skipped)
			slow_moving_interval_including_missed_states.exit_interval = dead_interval.entrance_interval;
		else throw ns_ex("Movement estimator reported slow movent as having been skipped when all later states had also been skipped!");
	}
	//we only know that the posture changing interval *had* to have occurred if the animal was ultimately seen to have died.
	if (posture_changing_interval.skipped && !dead_interval.skipped) {
		posture_changing_interval_including_missed_states.skipped = false;
		//if both slow and posture are skipped, they both occur the frame before the first
		if (slow_moving_interval.skipped)
			posture_changing_interval_including_missed_states = slow_moving_interval_including_missed_states;
		else {
			//otherwise the posture changing interval was sandwhiched between fast movement and death.
			posture_changing_interval_including_missed_states.entrance_interval = dead_interval.entrance_interval;
			posture_changing_interval_including_missed_states.exit_interval = dead_interval.entrance_interval;
		}
	}

	if (expansion_interval.skipped)
		expansion_interval_including_missed_states.skipped = true;
	else
		expansion_interval_including_missed_states = expansion_interval;
	if (post_expansion_contraction_interval.skipped)
		post_expansion_contraction_interval_including_missed_states.skipped = true;
	else
		post_expansion_contraction_interval_including_missed_states = post_expansion_contraction_interval;

	//dead animals can be skipped without any editing, as we'll never be able to confirm that it happened unless we observed it.

}


ns_movement_state_time_interval_indicies calc_frame_before_first(const ns_movement_state_time_interval_indicies & first_valid_element_id) {

	ns_movement_state_time_interval_indicies frame_before_first(first_valid_element_id);
	if (first_valid_element_id.period_start_index == 0) {
		frame_before_first.interval_occurs_before_observation_interval = true;
		frame_before_first.period_start_index = -1;
	}
	else frame_before_first.period_start_index--;
	frame_before_first.interval_occurs_after_observation_interval = false;
	frame_before_first.period_end_index--;
	return frame_before_first;
}


//Welcome to the dark innards of the lifespan machine!
//
//detect_death_times_and_generate_annotations_from_movement_quantification() is the part of the machine
//that turns observations of movement into death time annotations; there's a lot of detail here
//so bear with me as I attempt to explain.
//Note that how those annotations are handled (and aggregated into mortality data) is up to downstream users
//Annotations are a description of what happened when, nothing more.
//
//The specified ns_analyzed_image_time_path_death_time_estimator 
//analyzes the movement quantification and identifies the frames at which
//the animal stops moving slowly and when it stops changing posture (i.e it dies).
//Complexity is introduced because animals do not changes states at a specific
//time, they change states during the interval between observations.
//So we need to convert the frame indicies produced by ns_analyzed_image_time_path_death_time_estimator ()
//into observation periods--the period between the last observation during which the animal was in one state
//and the first observation in which it was in the next.
//This would be easy if all worms were observed at all times, but 
//since worms can be missing for various amounts of time and observations may be explicitly excluded
//we need to be careful about finding the right observation periods.
//
//Prior to producing annotations, for each possible state we build description of the interval in which
//that state was occupiued; e.g the pair of observations in which an animal enters a state and 
//and the pair of observations in which the animal exits the state.
//These intervals are saved in the state_intervals structure for later use by other time_path_image_analyzer member functions
//and also used to generate annotations
//
//It is possible that animals may fail to be observed entering or exiting certain states.
//If an animal is in a state from the beginning of the path the state interval specificaiton  has its 
//ns_movement_state_observation_boundary_interval::entrance_interval::interval_occurs_before_observation_interval flag set
//Similarly, states that the animal is never observed to leave have their 
//ns_movement_state_observation_boundary_interval::exit_interval::interval_occurs_after_observation_interval flag set

//The state intervals are used to generate event annotations.
//In the case where the state begins before the start of the pathis assumed that it entered that state
//during the interval ending with the first observation in the path. 
//This means, for examples, animals who are identified as stationary at the first frame of the path are reported to have done so
//in the interval immediately before the path starts.
//If the case where the path starts at the beginning of the observation period (there is no 
//previous observation interval), then the ns_death_time_annotation_time_interval::period_start_was_not_observed flag
//is set on the apropriate annotation.  These flags are set during the initialization of the time path group, eg
//ns_analyzed_image_time_path_group::ns_analyzed_image_time_path_group()
	
//
//There is no problem handling events that continue past the end of the path;
//we don't use this information for anything and do not output any annotations about it.
void ns_analyzed_image_time_path::detect_death_times_and_generate_annotations_from_movement_quantification(const ns_stationary_path_id & path_id, const ns_analyzed_image_time_path_death_time_estimator * movement_death_time_estimator, ns_movement_analysis_result & result, const unsigned long last_time_point_in_analysis) const {
	std::vector<double > tmp_storage_1;
	std::vector<unsigned long > tmp_storage_2;
	detect_death_times_and_generate_annotations_from_movement_quantification(path_id, movement_death_time_estimator, result, last_time_point_in_analysis,tmp_storage_1, tmp_storage_2);
}
void ns_analyzed_image_time_path::detect_death_times_and_generate_annotations_from_movement_quantification(const ns_stationary_path_id & path_id, const ns_analyzed_image_time_path_death_time_estimator * movement_death_time_estimator, ns_movement_analysis_result & analysis_result, const unsigned long last_time_point_in_analysis, std::vector<double > & tmp_storage_1, std::vector<unsigned long > & tmp_storage_2 ) const {
		
	//if (path_id.group_id == 5)
	//	cerr << "RA";
	analysis_result.state_intervals.clear();
	analysis_result.state_intervals.resize((int)ns_movement_number_of_states);
	//by_hand_annotation_event_times.resize((int)ns_number_of_movement_event_types,ns_death_time_annotation_time_interval::unobserved_interval());

	unsigned long current_time(ns_current_time());
	analysis_result.death_time_annotation_set.clear();
	if (elements.size() == 0) return;

	analysis_result.first_valid_element_id = ns_movement_state_time_interval_indicies(elements.size(), elements.size());
	analysis_result.last_valid_element_id = ns_movement_state_time_interval_indicies(0,0);
	for (unsigned int i = 0; i < elements.size(); i++)
		if (!elements[i].excluded && !elements[i].element_before_fast_movement_cessation){
			//find first measurment
			if (analysis_result.first_valid_element_id.period_start_index >= i)
				analysis_result.first_valid_element_id.period_start_index = i;
			else
				//find the second measurement
			if (analysis_result.first_valid_element_id.period_end_index > i)
				analysis_result.first_valid_element_id.period_end_index = i;

			//find the last measurement
			if (analysis_result.last_valid_element_id.period_end_index < i){
				//set the measurement previous to the last measurement
				analysis_result.last_valid_element_id.period_start_index =
					analysis_result.last_valid_element_id.period_end_index;
				analysis_result.last_valid_element_id.period_end_index = i;
			}
		}
		if (analysis_result.first_valid_element_id.period_start_index+1 >= elements.size())
		return;

	//if the experiment is cropped before the current path ends, then we don't add a stationary worm is lost event.
	//unsigned long actual_time_of_first_measurement_after_path_end(time_path_limits.interval_after_last_observation.period_end);
	//if (last_valid_element_id.period_end+1 < elements.size())
	//	actual_time_of_first_measurement_after_path_end = 0;

	//if the path has been declared as not having enough information
	//for annotation, register it as so.
	if (this->is_low_density_path()){
		unsigned long end_index(analysis_result.first_valid_element_id.period_end_index);
		if (end_index > elements.size())
			throw ns_ex("Invalid end index");
		if (end_index == elements.size())
			end_index = elements.size()-1;
		analysis_result.death_time_annotation_set.add(ns_death_time_annotation(ns_no_movement_event,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[analysis_result.first_valid_element_id.period_start_index].absolute_time,
													elements[end_index].absolute_time),
			elements[end_index].region_offset_in_source_image(),
			elements[end_index].worm_region_size(),
			ns_death_time_annotation::ns_not_excluded,			//filling in the gaps of these things work really well! Let the user exclude them in the worm browser
			ns_death_time_annotation_event_count(1+elements[end_index].number_of_extra_worms_observed_at_position,0),
			current_time,ns_death_time_annotation::ns_lifespan_machine,
			(elements[end_index].part_of_a_multiple_worm_disambiguation_group)?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,true,elements[end_index].inferred_animal_location,elements[end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed, "low_density"));
	}
	
	analysis_result.machine_movement_state_solution = movement_death_time_estimator->operator()(this,true,tmp_storage_1,tmp_storage_2);

	//Some movement detection algorithms need a significant amount of time after an animal has died
	//to detect its death.  So, if we are going to censor an animal at the end of the experiment,
	//we need to ask the movement detection about the latest time such an event can occur.
	const unsigned long last_possible_death_time(movement_death_time_estimator->latest_possible_death_time(this,last_time_point_in_analysis));

	const double loglikelihood_of_solution (analysis_result.machine_movement_state_solution.loglikelihood_of_solution);
	const string reason_to_be_censored(analysis_result.machine_movement_state_solution.reason_for_animal_to_be_censored);

	ns_movement_state_observation_boundary_interval slow_moving_interval_including_missed_states,
		posture_changing_interval_including_missed_states,
		dead_interval_including_missed_states,
		expansion_interval_including_missed_states,
		post_expansion_contraction_interval_including_missed_states;

	slow_moving_interval_including_missed_states.longest_observation_gap_within_interval = analysis_result.machine_movement_state_solution.slowing.longest_observation_gap_within_interval;
	posture_changing_interval_including_missed_states.longest_observation_gap_within_interval = analysis_result.machine_movement_state_solution.moving.longest_observation_gap_within_interval;
	dead_interval_including_missed_states.longest_observation_gap_within_interval = analysis_result.machine_movement_state_solution.dead.longest_observation_gap_within_interval;
	expansion_interval_including_missed_states.longest_observation_gap_within_interval = 0;
	
	unsigned long longest_skipped_interval_before_death = slow_moving_interval_including_missed_states.longest_observation_gap_within_interval;
	if (longest_skipped_interval_before_death < posture_changing_interval_including_missed_states.longest_observation_gap_within_interval)
		longest_skipped_interval_before_death = posture_changing_interval_including_missed_states.longest_observation_gap_within_interval;	
	
	{

		ns_movement_state_time_interval_indicies frame_before_first = calc_frame_before_first(analysis_result.first_valid_element_id);
		convert_movement_solution_to_state_intervals(frame_before_first, analysis_result.machine_movement_state_solution, analysis_result.state_intervals);

		//ok, now we have the correct state intervals.  BUT we need to change them around because we know that animals have to pass through 
		//fast to slow movement to posture, and posture to death.  If the movement detection algorithms didn't find any states
		//that's because the animals went through too quickly.  So we make *new* intervals for this purpose
		//which involves transitioning through the missed state within a single frame
		calculate_state_transitions_in_the_presence_of_missing_states(
			frame_before_first,
			analysis_result.state_intervals[ns_movement_slow],
			analysis_result.state_intervals[ns_movement_posture],
			analysis_result.state_intervals[ns_movement_stationary],
			analysis_result.state_intervals[ns_movement_death_associated_expansion],
			analysis_result.state_intervals[ns_movement_death_associated_post_expansion_contraction],
			slow_moving_interval_including_missed_states,
			posture_changing_interval_including_missed_states,
			dead_interval_including_missed_states,
			expansion_interval_including_missed_states,
			post_expansion_contraction_interval_including_missed_states);
	}
	if (!expansion_interval_including_missed_states.skipped &&
		expansion_interval_including_missed_states.entrance_interval.period_start_index == expansion_interval_including_missed_states.exit_interval.period_start_index)
		cout << "empty unskipped expansion interval";

	//if the path has extra worms at least 25% of the points leading up to it's death
		//mark the path as containing that extra worm
	unsigned long number_of_extra_worms_in_path(0),
		total_observations(0);
	bool part_of_a_multiple_worm_disambiguation_group(false);
	{
		unsigned long stop_i(analysis_result.last_valid_element_id.period_end_index);
		if (!dead_interval_including_missed_states.skipped)
			stop_i=dead_interval_including_missed_states.entrance_interval.period_end_index;
	
		unsigned long total_number_of_extra_worms(0),total_mult_worm_d(0);
		for (unsigned int i = 0; i < stop_i; i++){
			total_number_of_extra_worms+=elements[i].number_of_extra_worms_observed_at_position;
			total_mult_worm_d+=elements[i].part_of_a_multiple_worm_disambiguation_group?1:0;
			total_observations++;
		}
		if (total_observations == 0){
			number_of_extra_worms_in_path = 0;
			part_of_a_multiple_worm_disambiguation_group = 0;
		}
		else{
			number_of_extra_worms_in_path = (unsigned long)(floor(total_number_of_extra_worms/(float)(total_observations)+.75));
			part_of_a_multiple_worm_disambiguation_group = floor(total_mult_worm_d/(float)(total_observations)+.75) != 0;
		}
	}
	const bool part_of_a_full_trace(!dead_interval_including_missed_states.skipped);

	//Since the animal has slowed down enough to count as a path, we mark
	//the interval during which it slowed down as one before the first frame of the path.
	//ns_death_time_annotation_time_interval slow_movement_entrance_interval;
	//if (slow_moving_interval.skipped)
	//	slow_movement_entrance_interval = time_path_limits.interval_before_first_observation;
	//else slow_movement_entrance_interval = state_entrance_interval_time(slow_moving_interval);
	
	
	ns_death_time_annotation::ns_exclusion_type exclusion_type(ns_death_time_annotation::ns_not_excluded);
	if (!reason_to_be_censored.empty()){
		exclusion_type = ns_death_time_annotation::ns_censored;
	}
	analysis_result.death_time_annotation_set.add(
		ns_death_time_annotation(ns_fast_movement_cessation,
		0,region_info_id,
		state_entrance_interval_time(slow_moving_interval_including_missed_states),
		elements[analysis_result.first_valid_element_id.period_start_index].region_offset_in_source_image(),  //register the position of the object at that time point
		elements[analysis_result.first_valid_element_id.period_start_index].worm_region_size(),
		exclusion_type,
		ns_death_time_annotation_event_count(1+number_of_extra_worms_in_path,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
		(part_of_a_multiple_worm_disambiguation_group)?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
		path_id,part_of_a_full_trace,elements[analysis_result.first_valid_element_id.period_start_index].inferred_animal_location, 
			elements[analysis_result.first_valid_element_id.period_start_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed, reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	
	//observations are made at specific times (i.e. we see a fast moving worm at this time)
	for (unsigned int i = slow_moving_interval_including_missed_states.entrance_interval.period_end_index; i < slow_moving_interval_including_missed_states.exit_interval.period_end_index; i++){

		if (elements[i].excluded) continue;

		analysis_result.death_time_annotation_set.add(
			ns_death_time_annotation(ns_slow_moving_worm_observed,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[i].absolute_time,elements[i].absolute_time),
			elements[i].region_offset_in_source_image(),
			elements[i].worm_region_size(),exclusion_type,
			ns_death_time_annotation_event_count(1+elements[i].number_of_extra_worms_observed_at_position,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			elements[i].part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[i].inferred_animal_location,elements[i].subregion_info, ns_death_time_annotation::ns_explicitly_observed,reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death)
			);
	}

	if (!expansion_interval_including_missed_states.skipped) {

		ns_death_time_annotation e1(ns_death_associated_expansion_start,
			0, region_info_id,
			ns_death_time_annotation_time_interval(state_entrance_interval_time(expansion_interval_including_missed_states)),
			elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed,
			reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e1);
		
		ns_death_time_annotation e2(ns_death_associated_expansion_stop,
			0, region_info_id,
			ns_death_time_annotation_time_interval(state_exit_interval_time(expansion_interval_including_missed_states)),
			elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location, 
			elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed, reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e2);
	}
	else {
		ns_death_time_annotation e1(ns_death_associated_expansion_start,
			0, region_info_id,
			ns_death_time_annotation_time_interval(0, 0),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_not_observed,
			reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e1);

		ns_death_time_annotation e2(ns_death_associated_expansion_stop,
			0, region_info_id,
			ns_death_time_annotation_time_interval(0,0),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_not_observed, reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e2);
	}
	if (!post_expansion_contraction_interval_including_missed_states.skipped) {

		ns_death_time_annotation e1(ns_death_associated_post_expansion_contraction_start,
			0, region_info_id,
			ns_death_time_annotation_time_interval(state_entrance_interval_time(post_expansion_contraction_interval_including_missed_states)),
			elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed,
			reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e1);

		ns_death_time_annotation e2(ns_death_associated_post_expansion_contraction_stop,
			0, region_info_id,
			ns_death_time_annotation_time_interval(state_exit_interval_time(post_expansion_contraction_interval_including_missed_states)),
			elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[post_expansion_contraction_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed, reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e2);
	}
	else {
		ns_death_time_annotation e1(ns_death_associated_post_expansion_contraction_start,
			0, region_info_id,
			ns_death_time_annotation_time_interval(0, 0),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_not_observed,
			reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e1);

		ns_death_time_annotation e2(ns_death_associated_post_expansion_contraction_stop,
			0, region_info_id,
			ns_death_time_annotation_time_interval(0, 0),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
			path_id, part_of_a_full_trace, elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[slow_moving_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_not_observed, reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death);
		analysis_result.death_time_annotation_set.add(e2);
	}
	
	if (posture_changing_interval_including_missed_states.skipped && dead_interval_including_missed_states.skipped)
		return;
	if (posture_changing_interval_including_missed_states.entrance_interval.period_start_index >
		posture_changing_interval_including_missed_states.entrance_interval.period_end_index)
		throw ns_ex("Death start interval boundaries appear to be reversed:") << posture_changing_interval_including_missed_states.entrance_interval.period_end_index << " vs "
			<< posture_changing_interval_including_missed_states.entrance_interval.period_end_index;
	if (posture_changing_interval_including_missed_states.entrance_interval.interval_occurs_after_observation_interval)
		throw ns_ex("Encountered an unskipped start interval for posture changing that occurs on the last element of a timeseries");
	if (posture_changing_interval_including_missed_states.entrance_interval.period_end_index >= elements.size())
		throw ns_ex("Encountered an unskipped start interval for posture changing with an invalid end point");
		
	analysis_result.death_time_annotation_set.add(
		ns_death_time_annotation(ns_translation_cessation,
		0,region_info_id,
		state_entrance_interval_time(posture_changing_interval_including_missed_states),
		elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),  //register the position of the object at that time point
		elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
		exclusion_type,
		ns_death_time_annotation_event_count(number_of_extra_worms_in_path+1,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
		part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
		path_id,part_of_a_full_trace,elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location, elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed, reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	
	
	for (unsigned int i = posture_changing_interval_including_missed_states.entrance_interval.period_end_index; i < posture_changing_interval_including_missed_states.exit_interval.period_end_index; i++){
		if (elements[i].excluded) continue;
		analysis_result.death_time_annotation_set.add(
			ns_death_time_annotation(ns_posture_changing_worm_observed,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[i].absolute_time,elements[i].absolute_time),
			elements[i].region_offset_in_source_image(),  //register the position of the object at that time point
			elements[i].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1+elements[i].number_of_extra_worms_observed_at_position,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			elements[i].part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[i].inferred_animal_location, elements[i].subregion_info, ns_death_time_annotation::ns_explicitly_observed, reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	}
		
	if (dead_interval_including_missed_states.skipped){
		//we allow animals who are alive at the experiment, and confirmed as real animals (using the storyboard) to be exported
		//as censored.
		bool remains_alive(false);
		unsigned long last_observation_index(posture_changing_interval_including_missed_states.exit_interval.period_end_index);
		if (posture_changing_interval_including_missed_states.exit_interval.interval_occurs_after_observation_interval)
			last_observation_index = posture_changing_interval_including_missed_states.exit_interval.period_start_index;
	
		if (elements[last_observation_index].absolute_time > last_possible_death_time) {
			unsigned long i1;
			for (unsigned int k = 0; k < elements.size(); k++) {
				i1 = k;
				if (elements[k].absolute_time > last_possible_death_time) {
					break;
				}
			}
			ns_death_time_annotation_time_interval interval(0, elements[i1].absolute_time);
			if (i1 > 0)
				interval.period_start = elements[i1 - 1].absolute_time;
			else interval.period_start_was_not_observed = true;

			analysis_result.death_time_annotation_set.add(
				ns_death_time_annotation(ns_movement_cessation,
					0, region_info_id,
					interval,
					elements[i1].region_offset_in_source_image(),
					elements[i1].worm_region_size(),
					ns_death_time_annotation::ns_censored_at_end_of_experiment,
					ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
					part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
					path_id, part_of_a_full_trace,
					elements[i1].inferred_animal_location, elements[i1].subregion_info, ns_death_time_annotation::ns_explicitly_observed,
					"Alive at experiment end", loglikelihood_of_solution, longest_skipped_interval_before_death));

		}

		return;
	}

	
	if (dead_interval_including_missed_states.entrance_interval.period_start_index >=
		dead_interval_including_missed_states.entrance_interval.period_end_index)
		throw ns_ex("Death start interval boundaries appear to be equal or reversed:") << dead_interval_including_missed_states.entrance_interval.period_end_index << " vs "
			<< dead_interval_including_missed_states.exit_interval.period_end_index;

	if (dead_interval_including_missed_states.entrance_interval.interval_occurs_after_observation_interval)
		throw ns_ex("Encountered an unskipped start interval for death that occurs on the last element of a timeseries");
	if (dead_interval_including_missed_states.entrance_interval.period_end_index >= elements.size())
		throw ns_ex("Encountered an unskipped start interval for death with an invalid end point");
		
	analysis_result.death_time_annotation_set.add(
		ns_death_time_annotation(ns_movement_cessation,
		0,region_info_id,
		ns_death_time_annotation_time_interval(state_entrance_interval_time(dead_interval_including_missed_states)),
		elements[dead_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
		elements[dead_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
		exclusion_type,
		ns_death_time_annotation_event_count(1+number_of_extra_worms_in_path,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
		part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
		path_id,part_of_a_full_trace,elements[dead_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[dead_interval_including_missed_states.entrance_interval.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed, reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	
	for (unsigned int i = dead_interval_including_missed_states.entrance_interval.period_end_index; i < dead_interval_including_missed_states.exit_interval.period_end_index; i++){
		if (elements[i].excluded) continue;
		analysis_result.death_time_annotation_set.add(
			ns_death_time_annotation(ns_stationary_worm_observed,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[i].absolute_time,elements[i].absolute_time),
			elements[i].region_offset_in_source_image(),
			elements[i].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(elements[i].number_of_extra_worms_observed_at_position+1,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			elements[i].part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[i].inferred_animal_location, elements[i].subregion_info, ns_death_time_annotation::ns_explicitly_observed,
				reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	}

	

	//if the path ends before the end of the plate's observations
	//output an annotation there.
	if (!time_path_limits.interval_after_last_observation.period_end_was_not_observed){

		analysis_result.death_time_annotation_set.add(
			ns_death_time_annotation(ns_stationary_worm_disappearance,
			0,region_info_id,
			time_path_limits.interval_after_last_observation,
			elements[analysis_result.last_valid_element_id.period_end_index].region_offset_in_source_image(),
			elements[analysis_result.last_valid_element_id.period_end_index].worm_region_size(),
			ns_death_time_annotation::ns_not_excluded,
			ns_death_time_annotation_event_count(1+number_of_extra_worms_in_path,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[analysis_result.last_valid_element_id.period_end_index].inferred_animal_location,
				elements[analysis_result.last_valid_element_id.period_end_index].subregion_info, ns_death_time_annotation::ns_explicitly_observed,
				"Alive at end of observation",loglikelihood_of_solution));
	}
}



void ns_analyzed_image_time_path::convert_movement_solution_to_state_intervals(const ns_movement_state_time_interval_indicies & frame_before_first, const ns_time_path_posture_movement_solution &solution, ns_movement_analysis_result::ns_state_interval_list & list) const {
	ns_movement_state_observation_boundary_interval slow_moving_interval,
		posture_changing_interval,
		dead_interval,
		expansion_interval,
		post_expansion_contraction_interval;

	//the movement detection algorithms give us the first frame in which the animal was observed in each state.
	//We need to conver these into *intervals* between frames during which the transition occurred.
	//This is because events may have occurred at any time during the interval and we don't want to assume
	//the event occurred at any specific time (yet!)
	//We'll resolve this observational ambiguity later.


	ns_movement_state_time_interval_indicies frame_after_last(elements.size() - 1, elements.size());
	frame_after_last.interval_occurs_after_observation_interval = true;


	if (solution.moving.skipped) {
		if (solution.slowing.skipped &&
			solution.dead.skipped)
			throw ns_ex("ns_analyzed_image_time_path::detect_death_times_and_generate_annotations_from_movement_quantification()::Movement death time estimator skipped all states!");
		//if we skip slow moving, that means we start changing posture or death, which means
		//we transition out of fast movement before the first frame of the path
		slow_moving_interval.skipped = true;
	}
	else {
		slow_moving_interval.skipped = false;
		slow_moving_interval.entrance_interval = frame_before_first;
		//if all following states are skipped, this state continues past the end of the observation interval
		if (solution.slowing.skipped && solution.dead.skipped)
			slow_moving_interval.exit_interval = frame_after_last;
		else {
			slow_moving_interval.exit_interval.period_end_index = solution.moving.end_index;
			slow_moving_interval.exit_interval.period_start_index = ns_find_last_valid_observation_index(solution.moving.end_index, elements);
			if (slow_moving_interval.exit_interval.period_start_index == -1) slow_moving_interval.exit_interval = frame_before_first;
		}
	}
	if (solution.slowing.skipped) {
		posture_changing_interval.skipped = true;
	}
	else {
		posture_changing_interval.skipped = false;
		//if the animal is never observed to be slow moving, it has been changing posture since before the observation interval
		if (slow_moving_interval.skipped)
			posture_changing_interval.entrance_interval = frame_before_first;
		else
			posture_changing_interval.entrance_interval = slow_moving_interval.exit_interval;
		//if the animal never dies, it continues to change posture until the end of observation interval
		if (solution.dead.skipped)
			posture_changing_interval.exit_interval = frame_after_last;
		else {
			posture_changing_interval.exit_interval.period_end_index = solution.slowing.end_index;
			posture_changing_interval.exit_interval.period_start_index = ns_find_last_valid_observation_index(solution.slowing.end_index, elements);
			if (posture_changing_interval.exit_interval.period_start_index == -1) posture_changing_interval.exit_interval = frame_before_first;
		}
	}
	if (solution.dead.skipped) {
		dead_interval.skipped = true;
	}
	else {
		dead_interval.skipped = false;
		if (posture_changing_interval.skipped && slow_moving_interval.skipped)
			dead_interval.entrance_interval = frame_before_first;
		else if (posture_changing_interval.skipped)
			dead_interval.entrance_interval = slow_moving_interval.exit_interval;
		else
			dead_interval.entrance_interval = posture_changing_interval.exit_interval;

		dead_interval.exit_interval = frame_after_last;
	}
	if (solution.expanding.skipped)
		expansion_interval.skipped = true;
	else {
		expansion_interval.skipped = false;
		if (solution.expanding.start_index == solution.expanding.end_index)
			cout << "Blank expansion interval!\n";
		expansion_interval.entrance_interval.period_end_index = solution.expanding.start_index;
		expansion_interval.entrance_interval.period_start_index = ns_find_last_valid_observation_index(solution.expanding.start_index, elements);
		if (expansion_interval.entrance_interval.period_start_index == -1)expansion_interval.entrance_interval = frame_before_first;


		expansion_interval.exit_interval.period_end_index = solution.expanding.end_index;
		expansion_interval.exit_interval.period_start_index = ns_find_last_valid_observation_index(solution.expanding.end_index, elements);
		if (expansion_interval.exit_interval.period_start_index == -1)expansion_interval.exit_interval = frame_before_first;
	}

	if (solution.post_expansion_contracting.skipped)
		post_expansion_contraction_interval.skipped = true;
	else {
		post_expansion_contraction_interval.skipped = false;
		if (solution.post_expansion_contracting.start_index == solution.post_expansion_contracting.end_index)
			cout << "Blank post-expansion interval!\n";
		post_expansion_contraction_interval.entrance_interval.period_end_index = solution.post_expansion_contracting.start_index;
		post_expansion_contraction_interval.entrance_interval.period_start_index = ns_find_last_valid_observation_index(solution.post_expansion_contracting.start_index, elements);
		if (post_expansion_contraction_interval.entrance_interval.period_start_index == -1) post_expansion_contraction_interval.entrance_interval = frame_before_first;

		post_expansion_contraction_interval.exit_interval.period_end_index = solution.post_expansion_contracting.end_index;
		post_expansion_contraction_interval.exit_interval.period_start_index = ns_find_last_valid_observation_index(solution.post_expansion_contracting.end_index, elements);
		if (post_expansion_contraction_interval.exit_interval.period_start_index == -1) post_expansion_contraction_interval.exit_interval = frame_before_first;
	}

	list[ns_movement_slow] = slow_moving_interval;
	list[ns_movement_posture] = posture_changing_interval;
	list[ns_movement_stationary] = dead_interval;
	list[ns_movement_death_associated_expansion] = expansion_interval;
	list[ns_movement_death_associated_post_expansion_contraction] = post_expansion_contraction_interval;
}

void ns_analyzed_image_time_path::analyze_movement(const ns_analyzed_image_time_path_death_time_estimator * movement_death_time_estimator,const ns_stationary_path_id & path_id, const unsigned long last_timepoint_in_analysis){
	movement_analysis_result.clear();
	detect_death_times_and_generate_annotations_from_movement_quantification(path_id,movement_death_time_estimator, movement_analysis_result,last_timepoint_in_analysis);
}
bool inline ns_state_match(const unsigned long t,const ns_movement_state_observation_boundary_interval & i, const ns_analyzed_image_time_path & p){
	if (i.skipped)
		return false;
	const ns_death_time_annotation_time_interval interval(p.state_entrance_interval_time(i));
	if (!interval.period_start_was_not_observed && t < interval.period_start)
		return false;
	else if (interval.period_end_was_not_observed && t < interval.period_end)
		return false;

	if (!interval.period_end_was_not_observed && t >= interval.period_end)
		return false;
	else if (interval.period_end_was_not_observed && t>= interval.period_start)
		return false;
	return true;
}

ns_movement_state ns_analyzed_image_time_path::best_guess_movement_state(const unsigned long & t) const {
	ns_movement_state_observation_boundary_interval
		slow_moving_interval_including_missed_states,
		posture_changing_interval_including_missed_states,
		dead_interval_including_missed_states,
		expansion_interval_including_missed_states,
		post_expansion_contraction_interval_including_missed_states;

	//get the relevant transition times
	ns_movement_state_time_interval_indicies frame_before_first = calc_frame_before_first(movement_analysis_result.first_valid_element_id);
	calculate_state_transitions_in_the_presence_of_missing_states(
		frame_before_first,
		movement_analysis_result.state_intervals[ns_movement_slow],
		movement_analysis_result.state_intervals[ns_movement_posture],
		movement_analysis_result.state_intervals[ns_movement_stationary],
		movement_analysis_result.state_intervals[ns_movement_death_associated_expansion],
		movement_analysis_result.state_intervals[ns_movement_death_associated_post_expansion_contraction],
		slow_moving_interval_including_missed_states,
		posture_changing_interval_including_missed_states,
		dead_interval_including_missed_states, expansion_interval_including_missed_states,post_expansion_contraction_interval_including_missed_states);
	
	//slow moving is never skipped
	if (t < state_entrance_interval_time(slow_moving_interval_including_missed_states).best_estimate_event_time_for_possible_partially_unbounded_interval())
		return ns_movement_fast;

	if (posture_changing_interval_including_missed_states.skipped) 
		 return ns_movement_slow;

	if (t < state_entrance_interval_time(posture_changing_interval_including_missed_states).best_estimate_event_time_for_possible_partially_unbounded_interval())
		return ns_movement_slow;

	if (dead_interval_including_missed_states.skipped)
		return ns_movement_posture;

	if (t < state_entrance_interval_time(dead_interval_including_missed_states).best_estimate_event_time_for_possible_partially_unbounded_interval())
		return ns_movement_posture;	

	if (!expansion_interval_including_missed_states.skipped &&
		t < state_exit_interval_time(expansion_interval_including_missed_states).best_estimate_event_time_for_possible_partially_unbounded_interval())
		return ns_movement_death_associated_expansion;
	if (!post_expansion_contraction_interval_including_missed_states.skipped &&
		t < state_exit_interval_time(post_expansion_contraction_interval_including_missed_states).best_estimate_event_time_for_possible_partially_unbounded_interval())
		return ns_movement_death_associated_post_expansion_contraction;
	return ns_movement_stationary;

}
ns_movement_state ns_analyzed_image_time_path::explicitly_recognized_movement_state(const unsigned long & t) const{
	//if (this->is_not_stationary())
//		return ns_movement_fast;
	if (this->is_low_density_path())
		return ns_movement_machine_excluded;

	//ns_event_index_list::const_iterator p;
	if (ns_state_match(t, movement_analysis_result.state_intervals[ns_movement_fast],*this))
		return ns_movement_fast;
	
	if (ns_state_match(t, movement_analysis_result.state_intervals[ns_movement_slow],*this))
		return ns_movement_slow;

	if (ns_state_match(t, movement_analysis_result.state_intervals[ns_movement_posture],*this))
		return ns_movement_posture;
	
	if (ns_state_match(t, movement_analysis_result.state_intervals[ns_movement_stationary],*this))
		return ns_movement_stationary;

	if (ns_state_match(t, movement_analysis_result.state_intervals[ns_movement_death_associated_expansion],*this))
		return ns_movement_death_associated_expansion;	

	if (ns_state_match(t, movement_analysis_result.state_intervals[ns_movement_death_associated_post_expansion_contraction], *this))
		return ns_movement_death_associated_post_expansion_contraction;
	return ns_movement_not_calculated;
}
int ns_greater_than_time(const long a, const long t){
	if (t == -1)
		return -1;
	return a < t;
}
bool ns_analyzed_image_time_path::by_hand_data_specified() const{
	return !by_hand_annotation_event_times.empty() &&
		(!by_hand_annotation_event_times[ns_movement_cessation].period_end_was_not_observed 
			|| !by_hand_annotation_event_times[(int)ns_translation_cessation].period_end_was_not_observed  
			|| !by_hand_annotation_event_times[(int)ns_fast_movement_cessation].period_end_was_not_observed 
			);
}
ns_death_time_annotation_time_interval ns_analyzed_image_time_path::by_hand_movement_cessation_time() const{
	if (!by_hand_annotation_event_times[ns_movement_cessation].period_end_was_not_observed)
		return by_hand_annotation_event_times[ns_movement_cessation];
	else return ns_death_time_annotation_time_interval::unobserved_interval();
}
ns_death_time_annotation_time_interval ns_analyzed_image_time_path::by_hand_death_associated_expansion_time() const {
	if (!by_hand_annotation_event_times[ns_death_associated_expansion_start].period_end_was_not_observed)
		return by_hand_annotation_event_times[ns_death_associated_expansion_start];
	else return ns_death_time_annotation_time_interval::unobserved_interval();
}

ns_hmm_movement_state ns_analyzed_image_time_path::by_hand_hmm_movement_state(const unsigned long & t) const {
	//For translation and vigorous movement, users often just accept the machine annotations by default.
	//so, if the user hasn't specified them, we use the machine annotations
	ns_death_time_annotation_time_interval translation_end(cessation_of_fast_movement_interval());
	bool translation_skipped(true);
	if (!by_hand_annotation_event_times[(int)ns_translation_cessation].period_end_was_not_observed) {
		translation_end = by_hand_annotation_event_times[(int)ns_translation_cessation];
		translation_skipped = false;
	}
	else {
		translation_end = machine_event_time(ns_translation_cessation, translation_skipped);
	}

	ns_death_time_annotation_time_interval vigorous_end;
	bool vigorous_skipped(true);
	if (!by_hand_annotation_event_times[(int)ns_fast_movement_cessation].period_end_was_not_observed) {
		vigorous_end = by_hand_annotation_event_times[(int)ns_fast_movement_cessation];
		vigorous_skipped = false;
	}
	else {
		vigorous_end = machine_event_time(ns_translation_cessation, vigorous_skipped);
	}

	//if the worm hasn't arrived yet, it's missing.
	if (!translation_skipped && t <= translation_end.period_end)
		return ns_hmm_missing;

	//Ignore by-hand vigorous labels.
	/*
	if (!vigorous_skipped && t <= vigorous_end.period_end) {
		if (estimator.state_defined(ns_hmm_moving_vigorously))
			return ns_hmm_moving_vigorously;
		else return ns_hmm_moving_weakly;
	}*/

	//if the worm never stops moving, or hasn't stopped moving yet
	//it is either moving weakly or expanding.
	if (by_hand_annotation_event_times[(int)ns_movement_cessation].period_end_was_not_observed ||
		t <= by_hand_annotation_event_times[(int)ns_movement_cessation].period_end) {

		//if expansion end was marked
		if (!by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end_was_not_observed) {
			//and we are after it
			if (t > by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end)
				return ns_hmm_moving_weakly_post_expansion;
			//if no start time was annotated
			if (by_hand_annotation_event_times[ns_death_associated_expansion_start].period_end_was_not_observed)
				return ns_hmm_moving_weakly_expanding;
			//if we are after the start time
			if (t >= by_hand_annotation_event_times[ns_death_associated_expansion_start].period_end)
				return ns_hmm_moving_weakly_expanding;
			//if we are before the start time
			return ns_hmm_moving_weakly;
		}
		//no expansion was marked
		return ns_hmm_moving_weakly;
	}
	//the death time was annotated and we are after it.

	//if the animal never expanded nor contracted or has stopped expanding and contracting
	if ((by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end_was_not_observed ||
		t >= by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end) && 
		(by_hand_annotation_event_times[ns_death_associated_post_expansion_contraction_stop].period_end_was_not_observed ||
		t >= by_hand_annotation_event_times[ns_death_associated_post_expansion_contraction_stop].period_end)) 
		return ns_hmm_not_moving_dead;

	//the animal hasn't stopped expanding yet

	//if the animal ever expanded 
	if ((!by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end_was_not_observed &&
		t < by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end) &&
		//and if if the animal started expanding right after death(eg the start was not annotated)
		(by_hand_annotation_event_times[(int)ns_death_associated_expansion_start].period_end_was_not_observed ||
		//or it has already started expanding
		t >= by_hand_annotation_event_times[ns_death_associated_expansion_start].period_end))
		return ns_hmm_not_moving_expanding;

	//if the animal ever contracted
	if ((!by_hand_annotation_event_times[ns_death_associated_post_expansion_contraction_stop].period_end_was_not_observed &&
		t < by_hand_annotation_event_times[ns_death_associated_post_expansion_contraction_stop].period_end) &&

		//and if if the animal started contracting right after expansion (eg the start was not annotated)
		(by_hand_annotation_event_times[(int)ns_death_associated_post_expansion_contraction_start].period_end_was_not_observed ||
		//or it has already started expanding
		t >= by_hand_annotation_event_times[ns_death_associated_post_expansion_contraction_start].period_end))
		return ns_hmm_contracting_post_expansion;

	//if the animal hasn't started expanding yet, it's alive

	return ns_hmm_not_moving_alive;
}
ns_movement_state ns_analyzed_image_time_path::by_hand_movement_state( const unsigned long & t) const{
	
	if (!by_hand_annotation_event_times[ns_movement_cessation].period_end_was_not_observed &&
		t >= by_hand_annotation_event_times[ns_movement_cessation].period_end){
		if (!by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end_was_not_observed &&
			t < by_hand_annotation_event_times[ns_death_associated_expansion_stop].period_end)
			return ns_movement_death_associated_expansion;
		else if (!by_hand_annotation_event_times[ns_death_associated_post_expansion_contraction_stop].period_end_was_not_observed &&
			t < by_hand_annotation_event_times[ns_death_associated_post_expansion_contraction_stop].period_end)
			return ns_movement_death_associated_post_expansion_contraction;
		else
			return ns_movement_stationary;
	}
	if (by_hand_annotation_event_times[(int)ns_translation_cessation].period_end_was_not_observed &&
		t >= by_hand_annotation_event_times[(int)ns_translation_cessation].period_end)
		return ns_movement_posture;

	if (by_hand_annotation_event_times[(int)ns_fast_movement_cessation].period_end_was_not_observed){
		if(t >= by_hand_annotation_event_times[(int)ns_fast_movement_cessation].period_end)
			return ns_movement_slow;
		else return ns_movement_fast;
	}

	return ns_movement_not_calculated;
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::produce_death_time_annotations(ns_death_time_annotation_set & set) const{
	
	set.clear();
	for (unsigned long j = 0; j < groups.size(); j++){
		for (unsigned long k = 0; k < groups[j].paths.size(); k++){
			groups[j].paths[k].add_death_time_events_to_set(set);
		}
	}

	//add animals that are fast moving at the last time point
	set.add(extra_non_path_events);
	
}
template<class allocator_T>
void ns_analyzed_image_time_path_group<allocator_T>::clear_images(ns_time_path_image_movement_analysis_memory_pool<allocator_T> & pool){
	for (unsigned int i = 0; i < paths.size(); i++)
		for (unsigned int j = 0; j < paths[i].elements.size(); j++){
			paths[i].elements[j].clear_movement_images(pool.registered_image_pool);
			paths[i].elements[j].clear_path_aligned_images(pool.aligned_image_pool);

		}

}

//transfers a line from a rbg image to a grayscale one.
class ns_output_subline{
public:
	template<class ns_component, class ns_component_2>
		inline void operator()(const ns_image_whole<ns_component> & source, 
			const unsigned long & source_x_offset, 
			const unsigned long & dest_x_offset,
			const unsigned long & width, 
			const unsigned long &source_y, 
			const unsigned long &dest_y,
			ns_image_whole<ns_component_2> & dest,const unsigned long & channel){
			if (width+dest_x_offset >= dest.properties().width)
				throw ns_ex("ns_output_subline()::X overflow!!");
			if (dest_y >= dest.properties().height)
				throw ns_ex("ns_output_subline()::X overflow!!");
			for (unsigned int x = 0; x < dest_x_offset; x++)
				dest[dest_y][x] = 0;
			for (unsigned int x = 0; x < width; x++)
				dest[dest_y][x+dest_x_offset] = (ns_component_2)source[source_y][3*(x+source_x_offset)+channel];
			for (unsigned int x = width+dest_x_offset; x < dest.properties().width; x++)
				dest[dest_y][x] = 0;
		}	
	template<class ns_component>
		inline void output_specific_value(const ns_image_whole<ns_component> & source, 
			const unsigned long & source_x_offset, 
			const unsigned long & dest_x_offset,
			const unsigned long & width, 
			const unsigned long &source_y, 
			const unsigned long &dest_y,
			ns_path_aligned_image_set * dest,const unsigned long & channel, const ns_component region_val,const ns_component worm_val){

			for (unsigned int x = 0; x < dest_x_offset; x++)
				dest->set_thresholds(dest_y,x,0,0);
			for (unsigned int x = 0; x < width; x++)
				dest->set_thresholds(dest_y,x+dest_x_offset,
				source[source_y][3*(x+source_x_offset)+channel] == region_val,
				source[source_y][3*(x+source_x_offset)+channel] == worm_val
			);
			for (unsigned int x = width+dest_x_offset; x < dest->image.properties().width; x++)
				dest->set_thresholds(dest_y,x,0,0);
		}
};



bool ns_analyzed_image_time_path::region_image_is_required(const unsigned long time, const bool interpolated, const bool moving_backward){
	for (unsigned int k = 0; k < elements.size(); k++){
		if (elements[k].inferred_animal_location != interpolated)
			continue;
		if (time == elements[k].absolute_time){
			return true;
		}
	}
	return false;
}

//generates path_aligned_images->image from region visualiation
//note that the region images contain context images (ie. the extra boundary around the region_image)
template<class allocator_T>
bool ns_analyzed_image_time_path::populate_images_from_region_visualization(const unsigned long time,const ns_image_standard &region_visualization,const ns_image_standard & interpolated_region_visualization,bool just_do_a_consistancy_check, ns_analyzed_image_time_path::ns_load_type load_type, ns_time_path_image_movement_analysis_memory_pool<allocator_T> & memory_pool_){
	//region visualization is all worms detected at that time point.
	ns_image_properties path_aligned_image_image_properties(region_visualization.properties());
	if (region_visualization.properties().width == 0){
		path_aligned_image_image_properties = interpolated_region_visualization.properties();
	}
	
	
	//this sets the correct image size based on the solution's information.
	//the image resolution is taken from the region visualization
	const bool just_flag_elements_as_loaded = (load_type == ns_lrv_just_flag);
	if (!just_flag_elements_as_loaded)
		set_path_alignment_image_dimensions(path_aligned_image_image_properties);

	ns_output_subline output_subline;
	for (unsigned int k = 0; k < elements.size(); k++){
		if (time != elements[k].absolute_time) continue;
		ns_analyzed_image_time_path_element & e(elements[k]);
	//	if (group_id.group_id == 0 || group_id.group_id == 12)
	//		cerr << "#" << group_id.group_id << "(" << k << ")";
		const bool was_previously_loaded(e.path_aligned_image_is_loaded());
		try{
			if (!just_flag_elements_as_loaded && !just_do_a_consistancy_check && !was_previously_loaded) //don't do the check for double initilaizing if we're running a consistancy check 
				e.initialize_path_aligned_images(path_aligned_image_image_properties,memory_pool_.aligned_image_pool); // These allocations take a lot of time, so we pool them.  This speeds things up on machines with enough RAM to keep it all in memory.
	
			//offset_from_path is the distance the region image is from the bounding box around the path
			const ns_vector_2i tl_worm_context_position_in_pa(ns_analyzed_image_time_path::maximum_alignment_offset()+e.offset_from_path);
			if (e.path_aligned_images == 0)
				throw ns_ex("Encountered unloaded image!");
			
			if (tl_worm_context_position_in_pa.y > e.path_aligned_images->worm_region_threshold.properties().height ||
				path_aligned_image_image_properties.width > e.path_aligned_images->worm_region_threshold.properties().width || 
				tl_worm_context_position_in_pa.y > e.path_aligned_images->image.properties().height ||
				path_aligned_image_image_properties.width > e.path_aligned_images->image.properties().width)
				throw ns_ex("Out of bounds worm context position ") 
					<< path_aligned_image_image_properties.width << "," << tl_worm_context_position_in_pa.y 
					<< " for a worm with an image size " << e.path_aligned_images->worm_region_threshold.properties().width << "," << e.path_aligned_images->worm_region_threshold.properties().height;
		
			if (!just_do_a_consistancy_check && !just_flag_elements_as_loaded ){
				for (long y = 0; y < tl_worm_context_position_in_pa.y; y++){
					for (unsigned long x = 0; x < path_aligned_image_image_properties.width; x++){
						e.path_aligned_images->image[y][x] = 0;
						e.path_aligned_images->set_thresholds(y,x,0,0);
					}
				}
			}

			//fill in center
			ns_vector_2i br_worm_context_position_in_pa(tl_worm_context_position_in_pa + e.worm_context_size());
			if (br_worm_context_position_in_pa.x >= path_aligned_image_image_properties.width)
				throw ns_ex("Overflow in worm position!");
			if (br_worm_context_position_in_pa.y >= path_aligned_image_image_properties.height)
				throw ns_ex("Overflow in worm position!");

			const ns_image_standard * im(&region_visualization);
			if (e.inferred_animal_location){
				if (interpolated_region_visualization.properties().width == 0)
					throw ns_ex("No interpolated region image has been specified from which to extract inferred worm image");
				im = & interpolated_region_visualization;
				if (im->properties().width == 0)
					throw ns_ex("Required interpolated region image was not provided");
			}
			else
				if (im->properties().width == 0)
					throw ns_ex("Required region image was not provided");

			if (e.worm_context_size().x + e.context_offset_in_region_visualization_image().x > im->properties().width ||
				e.worm_context_size().y + e.context_offset_in_region_visualization_image().y > im->properties().height)
				throw ns_ex("Invalid region specification at time ") << time 
				<< "; worm position in region visualization image is larger than region visualization: The worm position: " 
				<< e.context_offset_in_region_visualization_image().x << "," << e.context_offset_in_region_visualization_image().x  
				<< "; the region image: " << im->properties().width << "," << im->properties().height << "; "
				<< " the worm context size: " << e.worm_context_size().x << "," << e.worm_context_size().y;
			if (load_type != ns_analyzed_image_time_path::ns_lrv_just_images)
				e.path_aligned_images_are_loaded_and_released = true;

			if (!just_do_a_consistancy_check && !just_flag_elements_as_loaded ){
				for (long y = 0; y < e.worm_context_size().y; y++){		
					output_subline(*im,							//source
					e.context_offset_in_region_visualization_image().x,		//source x offset
					tl_worm_context_position_in_pa.x,									//dest x offset
					e.worm_context_size().x,										//width
					y+e.context_offset_in_region_visualization_image().y,	//source y offset
					y+tl_worm_context_position_in_pa.y,												//dest y offset
					e.path_aligned_images->image,1);	

					output_subline.output_specific_value(*im,							//source
					e.context_offset_in_region_visualization_image().x,		//source x offset
					tl_worm_context_position_in_pa.x,							//dest x offset
					e.worm_context_size().x,										//width
					y+e.context_offset_in_region_visualization_image().y,	//source y offset
					y+tl_worm_context_position_in_pa.y,	
					//dest y offset
					e.path_aligned_images,2,(ns_8_bit)NS_REGION_VIS_ALL_THRESHOLDED_OBJECTS_VALUE,(ns_8_bit)NS_REGION_VIS_WORM_THRESHOLD_VALUE);										//dest

					/*
					output_subline.output_specific_value(region_visualization,							//source
					e.context_offset_in_region_visualization_image().x,		//source x offset
					tl_worm_context_position_in_pa.x,											//dest x offset
					e.worm_context_size().x,										//width
					y+e.context_offset_in_region_visualization_image().y,	//source y offset
					y+tl_worm_context_position_in_pa.y,	
					//dest y offset
					e.path_aligned_images->region_threshold,2);										//dest		*/
				}
		


			//fill in gap at bottom
				for (long y = br_worm_context_position_in_pa.y; y < path_aligned_image_image_properties.height; y++){
					for (unsigned long x = 0; x < path_aligned_image_image_properties.width*path_aligned_image_image_properties.components; x++){
						e.path_aligned_images->image[y][x] = 0;
						e.path_aligned_images->set_thresholds(y,x,0,0);
			
					}

				}
			}
		
			if (!just_flag_elements_as_loaded  && just_do_a_consistancy_check && !was_previously_loaded){
				e.release_path_aligned_images(memory_pool_.aligned_image_pool);
			}
		}
		catch (ns_ex & ex) {
			if (!just_do_a_consistancy_check && !was_previously_loaded) {
				e.release_path_aligned_images(memory_pool_.aligned_image_pool);
			}
			throw ex;
		}
		catch(...){
			if (!just_do_a_consistancy_check && !was_previously_loaded){
				e.release_path_aligned_images(memory_pool_.aligned_image_pool);
			}
			throw;
		}
		return true;
	}
	return false;
}
void ns_output_subimage(const ns_image_standard & im,const long offset,ns_image_standard & out){
	const ns_image_properties & prop(out.properties());
	const ns_image_properties & prop_i(im.properties());
	
	for (long y = 0; y < prop.height; y++)
		for (unsigned int x = 0; x < prop.width*prop.components; x++)
			out[y][x] = im[y+offset][x/prop_i.components];
}

void ns_hmm_movement_analysis_optimizatiom_stats::write_error_header(std::ostream & o,const std::vector<std::string> & extra_columns) {
	o << "Experiment,Device,Plate Name,Animal Details,Group ID,Path ID,Excluded,Censored,Number of Worms, Cross Validation Genotype,Cross Validation Info,Cross Validation Replicate ID";
	for (unsigned int j = 0; j < ns_hmm_movement_analysis_optimizatiom_stats_record::number_of_states; j++) {
		const std::string state = ns_movement_event_to_string(ns_hmm_movement_analysis_optimizatiom_stats_record::states[j]);
		o << "," << state << " identified by hand?, " << state << " identified by machine?," <<
			state << " time by hand (days)," << state << " time by machine (days), " << state << " Difference Between Machine and By Hand Event Times (Days), " << state << " Difference Squared (Days)";
	}
	o << ", Solution log-likelihood, Path length";
	for (unsigned int i = 0; i < extra_columns.size(); i++)
		o << "," << extra_columns[i];
	o << "\n";
}

void ns_hmm_movement_analysis_optimizatiom_stats::write_error_data(std::ostream & o, const std::string & genotype_set, const std::string & cross_validation_info, const unsigned long & replicate_id,const std::map<std::string,std::map<ns_64_bit,ns_region_metadata> > & metadata_cache) const{

	for (unsigned int k = 0; k < animals.size(); k++) {
		auto db_m = metadata_cache.find(*animals[k].database_name);
		if (db_m == metadata_cache.end())
			throw ns_ex("Could not find metadata for database  ") << *animals[k].database_name;
		auto m = db_m->second.find( animals[k].properties.region_info_id);
		if (m == db_m->second.end())	
			throw ns_ex("Could not find metadata for region  ") << animals[k].properties.region_info_id;
			   
		//ns_acquire_for_scope<ostream> all_observations(image_server.results_storage.time_path_image_analysis_quantification(sub, "hmm_obs", false, sql()).output());
		o << m->second.experiment_name << "," << m->second.device << "," << m->second.plate_name() << "," << m->second.plate_type_summary("-")
			<< "," << animals[k].path_id.group_id << "," << animals[k].path_id.path_id << ","
			<< (animals[k].properties.is_excluded() ? "1" : "0") << ","
			<< (animals[k].properties.is_censored() ? "1" : "0") << ","
			<< animals[k].properties.number_of_worms() << ","
			<< genotype_set << "," << cross_validation_info << "," << replicate_id;
		for (unsigned int s = 0; s < ns_hmm_movement_analysis_optimizatiom_stats_record::number_of_states; s++) {
			auto state_p = animals[k].measurements.find(ns_hmm_movement_analysis_optimizatiom_stats_record::states[s]);
			if (state_p == animals[k].measurements.end())
				throw ns_ex("Could not find requested state ") << ns_movement_event_to_string(ns_hmm_movement_analysis_optimizatiom_stats_record::states[s]);
			o << ","
				<< (state_p->second.by_hand_identified ? "y" : "n") << ","
				<< (state_p->second.machine_identified ? "y" : "n") << ","
				<< (state_p->second.by_hand_identified ? ns_to_string((state_p->second.by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval() - m->second.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24.0) : "") << ","
				<< (state_p->second.machine_identified ? ns_to_string((state_p->second.machine.best_estimate_event_time_for_possible_partially_unbounded_interval() - m->second.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24.0) : "") << ",";
			if (state_p->second.machine_identified && state_p->second.by_hand_identified) {
				const double r = ((double)state_p->second.machine.best_estimate_event_time_for_possible_partially_unbounded_interval() - (double)state_p->second.by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval()) / 60.0 / 60.0 / 24.0;
				o << r << "," << r * r;
			}
			else o << ",";
		}
		o << "," << animals[k].solution_loglikelihood;
		o << "," << animals[k].machine_state_info.path.size();
		if (animals[k].machine_state_info.path.size() > 0) {
			std::vector<double> measurement_averages(animals[k].state_info_variable_names.size(),0);
			for (int i = 0; i < animals[k].machine_state_info.path.size(); i++) {
				for (unsigned int j = 0; j < animals[k].machine_state_info.path[i].sub_measurements.size(); j++) {
					measurement_averages[j] += animals[k].machine_state_info.path[i].sub_measurements[j];
					if (!std::isfinite(animals[k].machine_state_info.path[i].sub_measurements[j]) || !std::isfinite(measurement_averages[j]))
						std::cerr << "Yikes";
				}
			}
			for (unsigned int i = 0; i < animals[k].state_info_variable_names.size(); i++) {
				o << "," << measurement_averages[i] / animals[k].machine_state_info.path.size();
			}
		}
		o << "\n";
	}
}

void ns_hmm_movement_analysis_optimizatiom_stats::write_hmm_path_header(std::ostream & o) const {
	o << "Experiment,Plate Name,Animal Details,Group ID,Path ID,Excluded,Censored,Number of Worms, Time (Days),Machine HMM state, By Hand HMM state, Machine likelihood, By hand likelihood, log(p(Machine) / p(by hand)) , Cumulative log(p(Machine) / p(by hand)) ";

	if (animals.size() > 0) {
		for (unsigned int i = 0; i < animals[0].state_info_variable_names.size(); i++) {
			o << ", Measurement " << animals[0].state_info_variable_names[i];
			o << ", log(Machine p(" << animals[0].state_info_variable_names[i] << ") / By Hand p(" << animals[0].state_info_variable_names[i] << "))";
			o << ", Cumulative log(Machine p(" << animals[0].state_info_variable_names[i] << ") / By Hand p(" << animals[0].state_info_variable_names[i] << "))";
		}
	}
	o << ", Machine Movement Cessation Time Error (days), Machine Expansion time Error (days), Machine post-expansion Contraction time Error (days)\n";
}

void ns_hmm_movement_analysis_optimizatiom_stats::write_hmm_path_data(std::ostream & o, const std::map<std::string,std::map<ns_64_bit, ns_region_metadata> > & metadata_cache) const {
	for (unsigned int k = 0; k < animals.size(); k++) {
		auto db_m = metadata_cache.find(*animals[k].database_name);
		if (db_m == metadata_cache.end())
			throw ns_ex("Could not find metadata for database") << *animals[k].database_name;

		auto m = db_m->second.find(animals[k].properties.region_info_id);
		if (m == db_m->second.end())
			throw ns_ex("Could not find metadata for region ") << animals[k].properties.region_info_id;
		double cumulative_differential_probability(0);
		std::vector<double> cumulative_sub_probabilities(animals[0].state_info_variable_names.size(), 0);
		if (animals[k].machine_state_info.path.size() != animals[k].state_info_times.size())
			throw ns_ex("Invalid path size");
		for (unsigned int i = 0; i < animals[k].machine_state_info.path.size(); i++) {
			o << m->second.experiment_name << "," << m->second.plate_name() << "," << m->second.plate_type_summary("-")
				<< "," << animals[k].path_id.group_id << "," << animals[k].path_id.path_id << ","
				<< (animals[k].properties.is_excluded() ? "1" : "0") << ","
				<< (animals[k].properties.is_censored() ? "1" : "0") << ","
				<< (animals[k].properties.number_of_worms()) << ",";
			o << ((animals[k].state_info_times[i] - m->second.time_at_which_animals_had_zero_age) / 60.0 / 60.0 / 24.0) << ",";
			o << ns_hmm_movement_state_to_string(animals[k].machine_state_info.path[i].state) << "," << ns_hmm_movement_state_to_string(animals[k].by_hand_state_info.path[i].state) << ",";
			o << animals[k].machine_state_info.path[i].total_log_probability << "," << animals[k].by_hand_state_info.path[i].total_log_probability << ",";
			const double diff_p = (animals[k].machine_state_info.path[i].total_log_probability - animals[k].by_hand_state_info.path[i].total_log_probability);
			if (animals[k].machine_state_info.path[i].state != ns_hmm_missing)	//missing states don't count in verterbi calculations, so we shouldn't calculate it
				cumulative_differential_probability += diff_p;
			o << diff_p << ",";
			o << cumulative_differential_probability;
			for (unsigned int pp = 0; pp < animals[0].state_info_variable_names.size(); pp++) {
				const double diff_p_sub(animals[k].machine_state_info.path[i].log_sub_probabilities[pp] - animals[k].by_hand_state_info.path[i].log_sub_probabilities[pp]);
				if (animals[k].machine_state_info.path[i].state != ns_hmm_missing)	//missing states don't count in verterbi calculations, so we shouldn't calculate it
					cumulative_sub_probabilities[pp] += diff_p_sub;
				o << "," << animals[k].machine_state_info.path[i].sub_measurements[pp]
				  << "," << diff_p_sub
				  << "," << cumulative_sub_probabilities[pp];
			}
			o << ",";
			auto movement_cessation_p = animals[k].measurements.find(ns_movement_cessation);
			

			if (movement_cessation_p != animals[k].measurements.end())
			if (movement_cessation_p->second.by_hand_identified && movement_cessation_p->second.machine_identified)
				o<< (movement_cessation_p->second.machine.best_estimate_event_time_for_possible_partially_unbounded_interval() - movement_cessation_p->second.by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval()) / 60.0 / 60.0 / 24.0;
			
			o << ",";
			auto relaxation_p = animals[k].measurements.find(ns_death_associated_expansion_start);
			if (relaxation_p != animals[k].measurements.end())
			if (relaxation_p->second.by_hand_identified && relaxation_p->second.machine_identified)
				o << (relaxation_p->second.machine.best_estimate_event_time_for_possible_partially_unbounded_interval() - relaxation_p->second.by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval())/ 60.0 / 60.0 / 24.0;
			o << ",";
			auto contraction_p = animals[k].measurements.find(ns_death_associated_post_expansion_contraction_start);
			if (contraction_p != animals[k].measurements.end())
				if (contraction_p->second.by_hand_identified && contraction_p->second.machine_identified)
					o << (contraction_p->second.machine.best_estimate_event_time_for_possible_partially_unbounded_interval() - contraction_p->second.by_hand.best_estimate_event_time_for_possible_partially_unbounded_interval()) / 60.0 / 60.0 / 24.0;

			o << "\n";
		}
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::add_by_hand_annotations(const ns_death_time_annotation_compiler & annotations){
	ns_death_time_annotation_compiler compiler;
	//load all paths into a compiler to do the merge
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++)
			compiler.add_path(region_info_id, generate_stationary_path_id(i, j),groups[i].paths[j].path_region_position,groups[i].paths[j].path_region_size, ns_region_metadata());
	}
	//do the merge
	compiler.add(annotations);
	//now fish all the annotations back out of the compiler, and add them to the right path
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			ns_stationary_path_id id(generate_stationary_path_id(i,j));
			ns_death_time_annotation_compiler::ns_region_list::iterator p(compiler.regions.find(region_info_id));
			if (p == compiler.regions.end())
				throw ns_ex("ns_time_path_image_movement_analyzer::add_by_hand_annotation_event_times::Could not find region after merge!");
			bool found(false);
			for (ns_death_time_annotation_compiler_region::ns_location_list::iterator q = p->second.locations.begin(); q != p->second.locations.end(); q++){
				if (q->properties.stationary_path_id == generate_stationary_path_id(i,j)){
					groups[i].paths[j].add_by_hand_annotations(*q);

					found = true;
					break;
				}
			}
			if (!found)
				throw ns_ex("ns_time_path_image_movement_analyzer::add_by_hand_annotation_event_times::Could not find path after merge!");
		}
	}
}

std::pair<long, long> ns_find_index_for_time(const ns_death_time_annotation_time_interval & interval, const ns_analyzed_image_time_path & path) {
	std::pair<long, long> p(-1, -1);
	if (interval.fully_unbounded())
		return p;
	std::pair<bool, bool> found(false, false);
	for (unsigned int i = 0; i < path.element_count(); i++) {
		if (path.element(i).excluded || path.element(i).censored)
			continue;
		if (!found.first && !interval.period_start_was_not_observed && path.element(i).absolute_time >= interval.period_start) {
			p.first = i;
			found.first = true;
		}
		if (!found.second && !interval.period_end_was_not_observed && path.element(i).absolute_time >= interval.period_end) {
			p.second = i;
			found.second = true;
		}
		if (found.first && found.second)
			break;
	}
	return p;
}
ns_movement_state_observation_boundaries ns_set_boundary(const ns_death_time_annotation_time_interval & i, const ns_analyzed_image_time_path & path) {
	ns_movement_state_observation_boundaries b;
	if (i.fully_unbounded()) {
		b.skipped = true;
		return b;
	}
	b.skipped = false;
	const std::pair<long,long> p(ns_find_index_for_time(i,path));
	b.start_index = p.first;
	b.end_index = p.second;
	return b;
}

ns_time_path_posture_movement_solution ns_analyzed_image_time_path::reconstruct_movement_state_solution_from_annotations(const unsigned long first_index, const unsigned long last_index, const ns_emperical_posture_quantification_value_estimator * e, const std::vector<ns_death_time_annotation_time_interval> & time_intervals) const {
	ns_time_path_posture_movement_solution s;
	if (time_intervals[(ns_translation_cessation)].fully_unbounded()) {
		s.moving.skipped = true;
		//std::cout << "Encountered an animal that never slowed\n";
		return s;
	}

	const std::pair< long, long> undefined(-1, -1);
	//these variables tell the start and stop indicies of the time period between observations during which the specified event happened.
	//we need to translate this into the intervals during which animals were in a given state.
	std::pair<long, long>
		translation_cessation(ns_find_index_for_time(time_intervals[(int)ns_translation_cessation], *this)),
		fast_movement_cessation(ns_find_index_for_time(time_intervals[(int)ns_fast_movement_cessation], *this)),
		movement_cessation(ns_find_index_for_time(time_intervals[(int)ns_movement_cessation], *this)),
		expansion_start(ns_find_index_for_time(time_intervals[(int)ns_death_associated_expansion_start], *this)),
		expansion_end(ns_find_index_for_time(time_intervals[(int)ns_death_associated_expansion_stop], *this)),
		post_expansion_contraction_start(ns_find_index_for_time(time_intervals[(int)ns_death_associated_post_expansion_contraction_start], *this)),
		post_expansion_contraction_end(ns_find_index_for_time(time_intervals[(int)ns_death_associated_post_expansion_contraction_stop], *this));

	
	
	//use the machine annotations for translation and fast movement cessation if not specified explicitly
	if (translation_cessation == undefined) {
		translation_cessation.first = movement_analysis_result.state_intervals[(int)ns_translation_cessation].exit_interval.period_start_index;
		translation_cessation.second = movement_analysis_result.state_intervals[(int)ns_translation_cessation].exit_interval.period_end_index;
	}
	if (translation_cessation.second == -1)
		throw ns_ex("Worm never stopped!");


	//we can only use by hand annotations that have corresponding states defined by the current hmm estimator.  
	//Estimators will lack states if the user has no by-hand annotations in the data set used to fit the hmm estimator, so we need to be flexible.
	//If extra by-hand states exist in the by-hand annotations not found in the estimator, we need to ignore them.
	{
		if (!e->state_defined(ns_hmm_moving_weakly) && movement_cessation != undefined) {
			fast_movement_cessation = undefined;
			translation_cessation.second = movement_cessation.first;
		}

		//if animals can't expand before death, truncate expansion at movement cessation time
		if (!e->state_defined(ns_hmm_moving_weakly_expanding) || !e->state_defined(ns_hmm_moving_weakly_post_expansion) || !e->state_defined(ns_hmm_not_moving_alive)) {
			if (expansion_end.second != -1 && movement_cessation.second != -1) {
				if (expansion_end.second < movement_cessation.second)  //if all expansion happens before movement cessation, eliminate the expansion entirely
					expansion_start = expansion_end = undefined;
				if (expansion_start.second != -1 && expansion_start.second < movement_cessation.second)
					expansion_start = movement_cessation;	//truncate expansion at movement cessation time
			}
		}
		if (!e->state_defined(ns_hmm_not_moving_expanding)) 
			expansion_start = expansion_end = undefined;

		if (!e->state_defined(ns_hmm_contracting_post_expansion))
			post_expansion_contraction_start = post_expansion_contraction_end = undefined;
	}

	if (expansion_end.first != -1 && expansion_end.second == -1)
		throw ns_ex("Unbounded expansion end interval!");

	s.expanding.skipped = (expansion_end.first == -1);
	if (!s.expanding.skipped) {
			s.expanding.end_index = expansion_end.first;
			if (expansion_start.second != -1)
				s.expanding.start_index = expansion_start.second;
			else if (movement_cessation.second == -1)
				throw ns_ex("Partially annotated expanding state in non-dying animal!");
			else 
				s.expanding.start_index = movement_cessation.second;
	}
	s.post_expansion_contracting.skipped = (post_expansion_contraction_end.first == -1);
	if (!s.post_expansion_contracting.skipped) {
		s.post_expansion_contracting.end_index = post_expansion_contraction_end.first;
		if (post_expansion_contraction_start.second != -1)
			s.post_expansion_contracting.start_index = post_expansion_contraction_start.second;
		else if (expansion_end.second != -1)
			s.post_expansion_contracting.start_index = expansion_end.second;
		else if (movement_cessation.second == -1)
			throw ns_ex("Partially annotated expanding state in non-dying animal!");
		else s.post_expansion_contracting.start_index = movement_cessation.second;
	}


	s.dead.skipped = true;
	if (movement_cessation.second != -1) {
		s.dead.start_index = movement_cessation.second;
		s.dead.end_index = last_index;
		s.dead.skipped = false;
	}

	s.slowing.skipped = fast_movement_cessation.second == -1 || fast_movement_cessation.second == movement_cessation.second;
	if (!s.slowing.skipped) {
		if (movement_cessation.second != -1)
			s.slowing.end_index = movement_cessation.second;
		else s.slowing.end_index = last_index;
		if (fast_movement_cessation.second != -1)
			s.slowing.start_index = fast_movement_cessation.second;
		else s.slowing.start_index = translation_cessation.second;
	}
	s.moving.skipped = translation_cessation.second == fast_movement_cessation.second || translation_cessation.second == movement_cessation.second;
	if (!s.moving.skipped){
		s.moving.start_index = translation_cessation.second;
		if (s.slowing.skipped)
			if (s.dead.skipped)
				s.moving.end_index = last_index;
			else s.moving.end_index = s.dead.start_index;
		else s.moving.end_index = s.slowing.start_index;
	}
	return s;
}

void ns_analyzed_image_time_path::add_by_hand_annotations(const ns_death_time_annotation_compiler_location & l){
	l.properties.transfer_sticky_properties(censoring_and_flag_details);
	add_by_hand_annotations(l.annotations);

}
void ns_analyzed_image_time_path::add_by_hand_annotations(const ns_death_time_annotation_set & set){
	for (unsigned int i = 0; i < set.events.size(); i++){
		const ns_death_time_annotation & e(set.events[i]);
		e.transfer_sticky_properties(censoring_and_flag_details);
		if (e.type != ns_translation_cessation &&
			e.type != ns_movement_cessation &&		
			e.type != ns_fast_movement_cessation &&
			e.type != ns_death_associated_expansion_stop &&
			e.type != ns_death_associated_expansion_start &&
			e.type != ns_death_associated_post_expansion_contraction_stop &&
			e.type != ns_death_associated_post_expansion_contraction_start)
			continue;
		//if (e.time.best_estimate_event_time_for_possible_partially_unbounded_interval() == 0)	
			//worm browser occassionaly generates spurious translation cessation events.
			//XXX We suppress them here
			//continue;

			by_hand_annotation_event_times[(int)e.type] = e.time;
			//if (e.time.best_estimate_event_time_for_possible_partially_unbounded_interval() == 0)
			//	throw ns_ex("ns_analyzed_image_time_path::add_by_hand_annotations()::Adding zeroed event time!");
			by_hand_annotation_event_explicitness[(int)e.type] = e.event_explicitness;
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::output_visualization(const string & base_directory) const{

	ns_dir::create_directory_recursive(base_directory);
	unsigned long p(0);

	string all_quant_filename(base_directory + DIR_CHAR_STR + "all_quant.csv");
	ofstream all_quant(all_quant_filename.c_str());
	all_quant << "path_id,time,stationary,movement,alt_movement\n";

	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
		//	cerr << "Writing out visualization for path " << p << "\n";
			if (groups[i].paths[j].element_count() == 0) continue;
			string base_dir2(base_directory + DIR_CHAR_STR + "path_" + ns_to_string(p));
			ns_dir::create_directory(base_dir2);

		//	string hist_filename(base_dir2 + DIR_CHAR_STR + "hist.csv");
		//	ofstream hist(hist_filename.c_str());
//			groups[i].paths[j].out_histograms(hist);
	//		hist.close();

			string quant_filename(base_dir2 + DIR_CHAR_STR + "quant.csv");
			ofstream quant(quant_filename.c_str());
			
			quant << "path_id,time,stationary,movement,alt_movement\n";


			string dir(base_dir2 + DIR_CHAR_STR + "raw");
			string dir_aligned(base_dir2 + DIR_CHAR_STR "registered");
			string dir_movement(base_dir2 + DIR_CHAR_STR "movement");
			
			ns_dir::create_directory_recursive(dir);
			
			ns_dir::create_directory_recursive(dir_aligned);
			ns_dir::create_directory_recursive(dir_movement);

			ns_image_standard out;	
			//ns_image_properties prop(groups[i].paths[j].element(0).path_aligned_images->image.properties());
			//prop.width = groups[i].paths[j].size.x;
			//prop.height = groups[i].paths[j].size.y;
			//out.init(prop);

			ns_image_standard out_c;

			for (unsigned int k = 0; k < groups[i].paths[j].element_count(); k++){
				quant << p << "," << groups[i].paths[j].element(k).relative_time/(60*60*24.0) << ","
					<< groups[i].paths[j].element(k).measurements.total_intensity_within_stabilized << "," 
					<< groups[i].paths[j].element(k).measurements.interframe_time_scaled_movement_sum << "," 
					<< groups[i].paths[j].elements[k].measurements.movement_alternate_worm_sum <<"\n";
				
				all_quant << p << "," << groups[i].paths[j].element(k).relative_time/(60*60*24.0) << ","
					<< groups[i].paths[j].element(k).measurements.total_intensity_within_stabilized << ","
					<< groups[i].paths[j].element(k).measurements.interframe_time_scaled_movement_sum  << ","
					<< groups[i].paths[j].elements[k].measurements.movement_alternate_worm_sum <<"\n";

				string num;
				if (k < 10)
					num+="00";
				else if ( k < 100)
					num+="0";
				num+=ns_to_string(k);
				string filename = string(DIR_CHAR_STR) + "im_" + num + ".tif";

			//	ns_image_standard im(groups[i].paths[j].element(k).path_aligned_images->image);

			//	ns_image_properties prop(im.properties());
			//	prop.width = groups[i].paths[j].size.x;
			//	prop.height = groups[i].paths[j].size.y;
			//	out.init(prop);

			//	ns_output_subimage(im,ns_analyzed_image_time_path::maximum_alignment_offset,out);
			//	ns_save_image<ns_8_bit>(dir + filename,groups[i].paths[j].element(k).path_aligned_images->image);

			//	ns_output_subimage(im,ns_analyzed_image_time_path::maximum_alignment_offset + groups[i].paths[j].element(k).vertical_registration,out);
			//	ns_save_image(dir_aligned + filename,out);

			//	groups[i].paths[j].element(k).generate_movement_visualization(out_c);
		//		ns_save_image<ns_8_bit>(dir_movement + filename,out_c);

			}

			p++;
		}
	}
}

void ns_analyzed_image_time_path_element::generate_movement_visualization(ns_image_standard & out) const{
	ns_image_properties p(registered_images->movement_image_.properties());
	p.components = 3;
	out.init(p);
	for (unsigned int y = 0; y < p.height; y++)
		for (unsigned int x = 0; x < p.width; x++){
			const unsigned short r((registered_images->movement_image_[y][x]>0)?(registered_images->movement_image_[y][x]):0);
			const unsigned short g((registered_images->movement_image_[y][x]<0)?(-registered_images->movement_image_[y][x]):0);
			//r=0;
			//if (r>255)r=255;
			//r = (r+registered_images->image[y][x]);
			//if (r >= 255)r=255;
			out[y][3*x] = r;
			out[y][3*x+1] = g;
			out[y][3*x+2] = registered_images->image[y][x];
		}
}
class ns_debug_image_out{
public:
	void operator ()(const string &dir, const string & filename, const unsigned long kernal_size,const ns_image_whole<double> & im, const ns_image_whole<ns_16_bit> & consensus_count, const ns_image_whole<ns_8_bit> & unaligned_image){
		string filename_im(dir + DIR_CHAR_STR + "consensus" + DIR_CHAR_STR + filename),
			   filename_count(dir + DIR_CHAR_STR + "count" + DIR_CHAR_STR + filename),
			   filename_unaligned(dir + DIR_CHAR_STR + "unaligned" + DIR_CHAR_STR + filename);
		ns_dir::create_directory_recursive(dir + DIR_CHAR_STR + "consensus");
		ns_dir::create_directory_recursive(dir + DIR_CHAR_STR + "count");
		ns_dir::create_directory_recursive(dir + DIR_CHAR_STR + "unaligned");
		ns_image_properties prop(im.properties());
		count_swap.init(prop);
		im_swap.init(prop);

		for (unsigned int y = 0; y < prop.height; y++){
			for (unsigned int x = 0; x < prop.width; x++){
				im_swap[y][x] = (ns_8_bit)(im[y][x]/consensus_count[y][x]);
				count_swap[y][x] = (ns_8_bit)((((unsigned long)255)*consensus_count[y][x])/(kernal_size));
			}
		}
		ns_save_image<ns_8_bit>(filename_im,im_swap);
		ns_save_image(filename_count,count_swap);
		ns_save_image(filename_unaligned,unaligned_image);
	}
private:
	ns_image_whole<ns_8_bit> count_swap;
	ns_image_whole<ns_8_bit> im_swap;
};
//possible range of a sobel operator: (0,255)
inline ns_8_bit ns_sobel(const ns_image_standard & im,const unsigned long &x, const unsigned long & y){
	const unsigned long c(ns_analyzed_image_time_path::sobel_operator_width);
	const long sobel_y((long)im[y-c][x-c]+2*(long)im[y-c][x]+(long)im[y-c][x+c]
					  -(long)im[y+c][x-c]-2*(long)im[y+c][x]-(long)im[y+c][x+c]);
	const long sobel_x((long)im[y-c][x-c]+2*(long)im[y][x-c]+(long)im[y+c][x-c]
					  -(long)im[y-c][x+c]-2*(long)im[y][x+c]-(long)im[y+c][x+c]);
	return (ns_8_bit)(sqrt((double)(sobel_x*sobel_x+sobel_y*sobel_y))/5.65685);
}

class ns_movement_data_accessor{
public:
	ns_movement_data_accessor(ns_analyzed_image_time_path::ns_element_list & l):elements(&l){}
	unsigned long size()const {return elements->size();}
	const unsigned long time(const unsigned long i){return (*elements)[i].absolute_time;}
	double raw(const unsigned long i){
		if (i == 0)
			return raw(1);
		if ((*elements)[i-1].measurements.total_intensity_within_stabilized == 0)
			return 0;
		//XXX This is the definition of a "movement score" used by the v1 version of the software
		return  (
					(*elements)[i].measurements.movement_sum-
					//this term normalizes for changes in absolute intensity between frames (ie. light levels changing or worm size changes)
					fabs((double)(*elements)[i].measurements.total_intensity_within_foreground
					- (double)(*elements)[i-1].measurements.total_intensity_within_foreground)
				)
				/
				(*elements)[i-1].measurements.total_intensity_within_foreground;
		
	}
	double & processed(const unsigned long i){
		return (*elements)[i].measurements.denoised_movement_score;
	}
private:
	ns_analyzed_image_time_path::ns_element_list * elements;
};

class ns_stabilized_intensity_data_accessor {
public:
	ns_stabilized_intensity_data_accessor(ns_analyzed_image_time_path::ns_element_list& l) :elements(&l) {}
  std::size_t size()const { return elements->size(); }
	const unsigned long & time(const unsigned long i) { return (*elements)[i].absolute_time; }
	const ns_64_bit & raw(const unsigned long i) {
		return (*elements)[i].measurements.total_intensity_within_stabilized;
	}
	ns_64_bit & processed(const unsigned long i) {
		return (*elements)[i].measurements.total_intensity_within_stabilized_denoised;
	}
private:
	ns_analyzed_image_time_path::ns_element_list* elements;
};

class ns_stabilized_outside_intensity_data_accessor {
public:
	ns_stabilized_outside_intensity_data_accessor(ns_analyzed_image_time_path::ns_element_list& l) :elements(&l) {}
  std::size_t size()const { return elements->size(); }
	const unsigned long & time(const unsigned long i) { return (*elements)[i].absolute_time; }
	ns_64_bit raw(const unsigned long i) {
		return (*elements)[i].measurements.total_intensity_within_region - (*elements)[i].measurements.total_intensity_within_stabilized;
	}
	ns_64_bit& processed(const unsigned long i) {
		return (*elements)[i].measurements.total_intensity_outside_stabilized_denoised;
	}
private:
	ns_analyzed_image_time_path::ns_element_list* elements;
};

typedef enum { ns_mean, ns_median } ns_kernel_smoothing_type;
template <class T, ns_kernel_smoothing_type smoothing_type>
class ns_kernel_smoother {
public:
	void ns_kernel_smooth(const unsigned int kernel_width, T& data) {
		(*this)(kernel_width, data);
	}
	void operator()(const int kernel_width, T& data) {
		if (data.size() == 0)
			return;
		if (kernel_width % 2 == 0)
			throw ns_ex("Kernel width must be odd");
		for (long i = 0; i < data.size(); i++) {
			long start = i - kernel_width,
				stop = i + kernel_width + 1;
			if (start < 0) start = 0;
			if (stop > data.size()) stop = data.size();

			if (smoothing_type == ns_mean) {
				double s(0);
				for (unsigned int j = start; j < stop; j++)
					s += data.raw(j);
				data.processed(i) = s / (stop - start);
			}
			else {
				median_buffer.resize(stop - start);
				for (unsigned int j = start; j < stop; j++)
					median_buffer[j-start] = data.raw(j);
				data.processed(i) = median(median_buffer);
			}
		}
	}
private:
	std::vector<ns_64_bit> median_buffer;
	template<class T2>
	T2 median(std::vector<T2>& v) {
		if (v.empty())
			throw ns_ex("Requesting median of empty set");

		auto n = v.size() / 2;
		nth_element(v.begin(), v.begin() + n, v.end());
		if (v.size() & 1)	//odd
			return v[n];
		auto med = v[n];
		auto max_it = max_element(v.begin(), v.begin() + n);
		return (*max_it + med) / 2.0;
	}
};
struct ns_slope_accessor_total_in_region {
	static inline const ns_64_bit & total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.total_intensity_within_region;
	}
	static inline ns_s64_bit & slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.change_in_total_region_intensity;
	}
}; 
struct ns_slope_accessor_total_in_foreground {
	static inline const ns_64_bit & total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.total_intensity_within_foreground;
	}
	static inline ns_s64_bit & slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.change_in_total_foreground_intensity;
	}
};
struct ns_slope_accessor_total_in_stabilized_1x {
	static inline const ns_64_bit & total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.total_intensity_within_stabilized_denoised;
	}
	static inline ns_s64_bit & slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.change_in_total_stabilized_intensity_1x;
	}
}; 
struct ns_slope_accessor_total_in_stabilized_2x {
	static inline const ns_64_bit & total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.total_intensity_within_stabilized_denoised;
	}
	static inline ns_s64_bit & slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.change_in_total_stabilized_intensity_2x;
	}
}; 
struct ns_slope_accessor_total_in_stabilized_4x {
	static inline const ns_64_bit & total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.total_intensity_within_stabilized_denoised;
	}
	static inline ns_s64_bit & slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list & list) {
		return list[i].measurements.change_in_total_stabilized_intensity_4x;
	}
};
struct ns_slope_accessor_total_outside_stabilized_1x {
	static inline const ns_64_bit& total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list& list) {
		return list[i].measurements.total_intensity_outside_stabilized_denoised;
	}
	static inline ns_s64_bit& slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list& list) {
		return list[i].measurements.change_in_total_outside_stabilized_intensity_1x;
	}
};
struct ns_slope_accessor_total_outside_stabilized_2x {
	static inline const ns_64_bit& total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list& list) {
		return list[i].measurements.total_intensity_outside_stabilized_denoised;
	}
	static inline ns_s64_bit& slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list& list) {
		return list[i].measurements.change_in_total_outside_stabilized_intensity_2x;
	}
};
struct ns_slope_accessor_total_outside_stabilized_4x {
	static inline const ns_64_bit& total_intensity(const unsigned long i, const ns_analyzed_image_time_path::ns_element_list& list) {
		return list[i].measurements.total_intensity_outside_stabilized_denoised;
	}
	static inline ns_s64_bit& slope(const unsigned long i, ns_analyzed_image_time_path::ns_element_list& list) {
		return list[i].measurements.change_in_total_outside_stabilized_intensity_4x;
	}
};

//this calculates a time series of slope values, with the specified kernel width
//the particular measurements used are specified by data_accessor_t
typedef enum { ns_set_to_zero, ns_set_to_local_mean} ns_slope_calculator_edge_handling_behavior;
template<int slope_kernel_half_width, int min_edge_kernel_half_width,class data_accessor_t, ns_slope_calculator_edge_handling_behavior left_edge_behavior, ns_slope_calculator_edge_handling_behavior right_edge_behavior>
class ns_slope_calculator{
public:
	ns_slope_calculator< slope_kernel_half_width, min_edge_kernel_half_width, data_accessor_t, left_edge_behavior, right_edge_behavior>(ns_analyzed_image_time_path::ns_element_list & list, const int start_i, std::vector<ns_64_bit > & vals, std::vector<ns_64_bit > times){
		
		const int slope_kernel_width = slope_kernel_half_width * 2 + 1;

		if (list.size() < start_i)
			return;

		for (unsigned int i = 0; i < start_i; i++)
			data_accessor_t::slope(i,list) = 0;// units: per hour

		vals.reserve(slope_kernel_width);
		times.resize(slope_kernel_width);


		if (list.size() == start_i)
			return;
		//handle cases where the time series is smaller than the kernel
		const unsigned long number_of_valid_measurements = list.size() - start_i;
		unsigned long kernel_half_width_to_use = (number_of_valid_measurements - 1) / 2;
		if (kernel_half_width_to_use > slope_kernel_half_width)
			kernel_half_width_to_use = slope_kernel_half_width;

		ns_linear_regression_model model;
		//first, we "warm up" by filling in the left-most times with increasingly large kernel widths
		data_accessor_t::slope(start_i, list) = 0;
		for (unsigned int h_w = 1; h_w <= kernel_half_width_to_use; h_w++) {
			const int w(2 * h_w + 1);
			vals.resize(w);
			times.resize(w);
			for (unsigned int i = 0; i < w; i++) {
				vals[i] = data_accessor_t::total_intensity(i + start_i, list);
				times[i] = list[i + start_i].absolute_time;
			}
			if (h_w >= min_edge_kernel_half_width ) {
				ns_linear_regression_model_parameters params(model.fit(vals, times));
				data_accessor_t::slope(start_i + h_w, list) = params.slope * 60 * 60;
			}
			else 
				data_accessor_t::slope(start_i + h_w, list) = 0;
		}
		//if there is enough data to run the full kernel, do so
		if (kernel_half_width_to_use == slope_kernel_half_width) {

			int pos = 0;
			for (unsigned int i = slope_kernel_half_width + start_i; ; i++) {
				//calculate slope of current kernal
				ns_linear_regression_model_parameters params = model.fit(vals, times);

				data_accessor_t::slope(i, list) = params.slope * 60 * 60; // units/

				if (i + slope_kernel_half_width + 1 >= list.size())
					break;
				//update kernal for next step, by replacing earliest value i
				vals[pos] = data_accessor_t::total_intensity(i + slope_kernel_half_width + 1, list);
				times[pos] = list[i + slope_kernel_half_width + 1].absolute_time;
				pos++;
				if (pos == slope_kernel_width)
					pos = 0;
			}
			//fill in left edge if requested
			if (left_edge_behavior == ns_set_to_local_mean) {
				for (unsigned int i = 0; i < min_edge_kernel_half_width + start_i; i++)
					data_accessor_t::slope(i, list) = data_accessor_t::slope(slope_kernel_half_width + start_i, list);
			}
		}

		
		//finally, we "warm down" by filling in the left-most times with increasingly small kernel widths
		for (unsigned int h_w = kernel_half_width_to_use; h_w >= 1; h_w--) {
			if (h_w >= min_edge_kernel_half_width) {
				const int w(2 * h_w + 1);
				vals.resize(w);
				times.resize(w);
				for (unsigned int i = 0; i < w; i++) {
					vals[i] = data_accessor_t::total_intensity(list.size() - i - 1, list);
					times[i] = list[list.size() - i - 1].absolute_time;
				}
				ns_linear_regression_model_parameters params(model.fit(vals, times));
				data_accessor_t::slope(list.size() - h_w - 1, list) = params.slope * 60 * 60;
			}
			else if (right_edge_behavior == ns_set_to_local_mean && list.size() > kernel_half_width_to_use - 2)
				data_accessor_t::slope(list.size() - h_w - 1, list) = data_accessor_t::slope(list.size() - kernel_half_width_to_use - 2, list);
			else
				data_accessor_t::slope(list.size() - h_w - 1, list) = 0;
		}
		if (right_edge_behavior == ns_set_to_local_mean && list.size() > 1)
			data_accessor_t::slope(list.size() - 1, list) = data_accessor_t::slope(list.size() - 2, list);
		else 
			data_accessor_t::slope(list.size() - 1, list) = 0;
	}
};

void ns_analyzed_image_time_path::denoise_movement_series_and_calculate_intensity_slopes(const ns_time_series_denoising_parameters & times_series_denoising_parameters, std::vector<ns_64_bit >& tmp1, std::vector<ns_64_bit >& tmp2){
	if (elements.size() == 0)
		return;

	
	const int movement_kernel_width(1),
		intensity_kernel_width(3);
	ns_movement_data_accessor acc(elements);
	//ns_spatially_averaged_movement_data_accessor acc_spatial(elements);


	//calculate the slope at each point
	//Here, ns_movement_data_accessor calculates the movement score and stores it.
	for (unsigned int i = 0; i < elements.size(); i++) {
		elements[i].measurements.registration_displacement = elements[i].registration_offset;
		elements[i].measurements.movement_score = acc.raw(i);
		elements[i].measurements.spatial_averaged_movement_score_cropped = elements[i].measurements.spatial_averaged_movement_sum_cropped;
		elements[i].measurements.spatial_averaged_movement_score_uncropped = elements[i].measurements.spatial_averaged_movement_sum_uncropped;
		

	}
	{
		//smooth raw movement data
		ns_kernel_smoother<ns_movement_data_accessor, ns_mean> smooth_data;
		smooth_data(movement_kernel_width, acc);
		//we want to denoise intensities so that our estimates of change in intensity work better.
		ns_stabilized_intensity_data_accessor acc_2(elements);
		ns_kernel_smoother < ns_stabilized_intensity_data_accessor, ns_median> smooth_data_2;
		smooth_data_2(intensity_kernel_width, acc_2);
		//do the same for the change in intensity outside the stabelized region (eg. non-worms)
		ns_stabilized_outside_intensity_data_accessor acc_3(elements);
		ns_kernel_smoother < ns_stabilized_outside_intensity_data_accessor, ns_median> smooth_data_3;
		smooth_data_3(intensity_kernel_width, acc_3);
	}

		
	const int start_i = this->first_stationary_timepoint();  //do not use frames before worm arrives to calculate slope, as the worm's appearence will produce a very large spurious slope.
	
	ns_slope_calculator<2, 2, ns_slope_accessor_total_in_region, ns_set_to_zero,ns_set_to_local_mean> total_change_calculator(elements,start_i,tmp1,tmp2);
	ns_slope_calculator<2, 2, ns_slope_accessor_total_in_foreground, ns_set_to_zero, ns_set_to_local_mean> foreground_change_calculator(elements, start_i, tmp1, tmp2);
	ns_slope_calculator<2, 2, ns_slope_accessor_total_in_stabilized_1x, ns_set_to_zero, ns_set_to_local_mean> stabilized_change_calculator_1x(elements, start_i, tmp1, tmp2);
	ns_slope_calculator<4, 2, ns_slope_accessor_total_in_stabilized_2x, ns_set_to_zero, ns_set_to_local_mean> stabilized_change_calculator_2x(elements, start_i, tmp1, tmp2);
	ns_slope_calculator<8, 2, ns_slope_accessor_total_in_stabilized_4x, ns_set_to_zero, ns_set_to_local_mean> stabilized_change_calculator_4x(elements,start_i, tmp1, tmp2);
	ns_slope_calculator<2, 2, ns_slope_accessor_total_outside_stabilized_1x, ns_set_to_zero, ns_set_to_local_mean> stabilized_outside_change_calculator_1x(elements, start_i, tmp1, tmp2);
	ns_slope_calculator<4, 2, ns_slope_accessor_total_outside_stabilized_2x, ns_set_to_zero, ns_set_to_local_mean> stabilized_outside_change_calculator_2x(elements, start_i, tmp1, tmp2);
	ns_slope_calculator<8, 2, ns_slope_accessor_total_outside_stabilized_4x, ns_set_to_zero, ns_set_to_local_mean> stabilized_outside_change_calculator_4x(elements, start_i, tmp1, tmp2);
}

void ns_match_histograms(const ns_image_standard & im1, const ns_image_standard & im2, float * histogram_matching_factors) {
	ns_histogram<unsigned int, ns_8_bit> h1(im1.histogram());
	ns_histogram<unsigned int, ns_8_bit> h2(im2.histogram());
	long cdf1[256], cdf2[256];
	cdf1[0] = h1[0];
	for (unsigned int i = 1; i < 256; i++)	cdf1[i] = h1[i] + cdf1[i - 1];
	cdf2[0] = h2[0];
	for (unsigned int i = 1; i < 256; i++)	cdf2[i] = h2[i] + cdf2[i - 1];

	int cur_v(1);
	histogram_matching_factors[0] = 0; //0 is a special pixel value indicateing "not in this region" pixels.
	for (int i = 1; i < 256; i++) {
		while (cdf1[cur_v] < cdf2[i])
			cur_v++;
		//if (cur_v == 0) histogram_matching_factors[i] = 0;
		if (cdf1[cur_v] - cdf2[i] < cdf2[i] - cdf1[cur_v - 1])
			histogram_matching_factors[i] = cur_v;
		else histogram_matching_factors[i] = cur_v - 1;
	}
}

void ns_analyzed_image_time_path::spatially_average_movement(const int y, const int x, const int k, const ns_image_standard_signed & im, long &averaged_sum, long &count) {
	averaged_sum = 0; 
	count = 0;
	for (int dy = -k; dy <= k; dy++) {
		const long y_(y + dy);
		if (y_ < 0 || y_ >= im.properties().height)
			continue;
		for (int dx = -k; dx <= k; dx++) {
			const long x_(x + dx);
			if (x_ < 0 || x_ >=im.properties().width)
				continue;
			averaged_sum += im[y_][x_];
			count++;
		}
	}
}
void ns_analyzed_image_time_path::quantify_movement(const ns_analyzed_time_image_chunk & chunk){
	

	//get the image size for temporary images.  
	ns_image_properties prop;
	bool found_one_not_excluded(false);
	for (unsigned int i = chunk.start_i; i < chunk.stop_i; i++){
		elements[i].measurements.zero();
		if (!elements[i].excluded){
			if (elements[chunk.start_i].registered_images == 0)
				throw ns_ex("Unallocated registered image encountered: ") << i << " with 0 position as " << this->first_stationary_timepoint();
			prop = elements[chunk.start_i].registered_images->image.properties();
			found_one_not_excluded = true;
		}
	}
	if (!found_one_not_excluded)
		return;

	const int image_width(prop.width),
			  image_height(prop.height);

	for (unsigned int i = chunk.start_i; i < chunk.stop_i; i++) {
		
		const bool first_frames(i < movement_time_kernel_width);
		//calulate histogram if not already done.
		if (!elements[i].registered_images->histograms_matched) {
			if (first_frames) {
				for (unsigned int j = 0; j < 256; j++)
					elements[i].registered_images->histogram_matching_factors[j] = j;
			}
			else
				ns_match_histograms(elements[i].registered_images->image, elements[i - movement_time_kernel_width].registered_images->image, elements[i].registered_images->histogram_matching_factors);
			elements[i].registered_images->histograms_matched = true;
		}

		elements[i].measurements.zero();

		if (elements[i].excluded) continue;
		//	elements[i].measurements.registration_offset = elements[i].registration_offset.mag();

		if (elements[i].registered_images == 0)
			throw ns_ex("ns_analyzed_image_time_path::quantify_movement()::Encountered an unloaded registered image!");

		elements[i].measurements.total_region_area = elements[i].worm_region_size_.x*elements[i].worm_region_size_.y;
		for (unsigned long y = 0; y < elements[i].registered_images->movement_image_.properties().height; y++) {
			for (unsigned long x = 0; x < elements[i].registered_images->movement_image_.properties().width; x++) {
				const bool stabilized = elements[i].registered_images->get_stabilized_worm_neighborhood_threshold(y, x);
				const bool worm_threshold(stabilized && elements[i].registered_images->image[y][x] > 0);

				elements[i].measurements.total_intensity_within_region += elements[i].registered_images->image[y][x];
				elements[i].measurements.total_intensity_within_foreground += worm_threshold ? elements[i].registered_images->image[y][x] : 0;
				elements[i].measurements.total_intensity_within_stabilized += stabilized ? elements[i].registered_images->image[y][x] : 0;
				elements[i].measurements.total_foreground_area += (worm_threshold ? 1 : 0);
				elements[i].measurements.total_stabilized_area += (stabilized ? 1 : 0);
			}
		}
		
		elements[i].measurements.total_intensity_in_previous_frame_scaled_to_current_frames_histogram = 0;
		if (!first_frames) {
			for (unsigned long y = 0; y < elements[i- movement_time_kernel_width].registered_images->movement_image_.properties().height; y++) {
				for (unsigned long x = 0; x < elements[i-movement_time_kernel_width].registered_images->movement_image_.properties().width; x++) {
					//const bool worm_threshold(elements[i].registered_images->get_stabilized_worm_neighborhood_threshold(y, x));
					const bool stabilized = elements[i].registered_images->get_stabilized_worm_neighborhood_threshold(y, x);
					elements[i].measurements.total_intensity_in_previous_frame_scaled_to_current_frames_histogram +=
						stabilized ? elements[i].registered_images->histogram_matching_factors[elements[i - movement_time_kernel_width].registered_images->image[y][x]] : 0;
				}
			}
		}
		//these are calculated later
		if (first_frames) {
			elements[i].measurements.change_in_total_foreground_intensity =
				elements[i].measurements.change_in_total_region_intensity = 
				elements[i].measurements.change_in_total_stabilized_intensity_1x =
				elements[i].measurements.change_in_total_stabilized_intensity_2x =
				elements[i].measurements.change_in_total_stabilized_intensity_4x = 0;
		}
		else {
			if (elements[i].measurements.total_foreground_area > 0 && elements[i - movement_time_kernel_width].measurements.total_foreground_area > 0) {
				elements[i].measurements.change_in_total_foreground_intensity = elements[i].measurements.total_intensity_within_region / (double)elements[i].measurements.total_foreground_area
					- elements[i - movement_time_kernel_width].measurements.total_intensity_within_region / (double)elements[i - movement_time_kernel_width].measurements.total_foreground_area;

			}

		}
		const unsigned long border(0);
#ifdef NS_CALCULATE_OPTICAL_FLOW
		//note we provide image1 twice as we don't have access to image2.
		elements[i].measurements.scaled_flow_magnitude.calculate(prop,
			ns_optical_flow_accessor_scaled_movement(elements[i].registered_images->flow_image_dx,
				elements[i].registered_images->flow_image_dy,
				elements[i].registered_images->image,
				elements[i].registered_images->image),
			border,
			*elements[i].registered_images);
		elements[i].measurements.raw_flow_magnitude.calculate(prop,
			ns_optical_flow_accessor_mag<float>(elements[i].registered_images->flow_image_dx,
				elements[i].registered_images->flow_image_dy),
			border,
			*elements[i].registered_images);
		elements[i].measurements.scaled_flow_dx.calculate(prop,
			ns_optical_flow_accessor_scaled_val<float>(elements[i].registered_images->flow_image_dx,
				elements[i].registered_images->image,
				elements[i].registered_images->image),
			border,
			*elements[i].registered_images);
		elements[i].measurements.scaled_flow_dy.calculate(prop,
			ns_optical_flow_accessor_scaled_val<float>(elements[i].registered_images->flow_image_dy,
				elements[i].registered_images->image,
				elements[i].registered_images->image),
			border,
			*elements[i].registered_images);

		elements[i].measurements.raw_flow_dx.calculate(prop,
			ns_optical_flow_accessor_val<float>(elements[i].registered_images->flow_image_dx),
			border,
			*elements[i].registered_images);
		elements[i].measurements.raw_flow_dy.calculate(prop,
			ns_optical_flow_accessor_val<float>(elements[i].registered_images->flow_image_dy),
			border,
			*elements[i].registered_images);
#endif

		double movement_sum(0),
			alternate_movement_sum(0);
		elements[i].measurements.movement_sum = 0;
		elements[i].measurements.spatial_averaged_movement_sum_cropped = 0;
		elements[i].measurements.spatial_averaged_movement_sum_uncropped = 0;
		elements[i].measurements.total_intensity_within_alternate_worm = 0;
		elements[i].measurements.total_alternate_worm_area = 0;

		for (long y = 0; y < elements[i].registered_images->movement_image_.properties().height; y++) {
			for (long x = 0; x < elements[i].registered_images->movement_image_.properties().width; x++) {

				//only calculate movement sum for areas where both images are defined.
				bool worm_threshold(elements[i].registered_images->get_stabilized_worm_neighborhood_threshold(y, x) &&
					elements[i].registered_images->image[y][x] > 0);
				if (i > movement_time_kernel_width)
					worm_threshold = worm_threshold && (elements[i - movement_time_kernel_width].registered_images->image[y][x] > 0);

				const bool alternate_worm_threshold(elements[i].registered_images->get_region_threshold(y, x) == 1 &&
					!worm_threshold);
				elements[i].measurements.movement_sum += (worm_threshold ? abs(elements[i].registered_images->movement_image_[y][x]) : 0);
				alternate_movement_sum += (alternate_worm_threshold) ? abs(elements[i].registered_images->movement_image_[y][x]) : 0;
				//there is a lot of low-level pixel noise that can average out even with a small 5x5 kernal.
				if (worm_threshold) {
					long averaged_sum, count;
					spatially_average_movement(y, x, ns_time_path_image_movement_analyzer<ns_overallocation_resizer>::ns_spatially_averaged_movement_kernal_half_size, elements[i].registered_images->movement_image_, averaged_sum, count);
					long averaged_sum_cropped = averaged_sum;
					if (abs(averaged_sum) < count * ns_time_path_image_movement_analyzer<ns_overallocation_resizer>::ns_spatially_averaged_movement_threshold)
						averaged_sum_cropped = 0;
					//if (abs(averaged_sum) < count*(ns_time_path_image_movement_analyzer<ns_overallocation_resizer>::ns_spatially_averaged_movement_threshold/2))
					//	averaged_sum = 0;
					if (count > 0) {
						elements[i].measurements.spatial_averaged_movement_sum_uncropped += abs(averaged_sum / (float)count);
						elements[i].measurements.spatial_averaged_movement_sum_cropped += abs(averaged_sum_cropped / (float)count);
					}
				}
				if (alternate_worm_threshold) {
					elements[i].measurements.total_intensity_within_alternate_worm += elements[i].registered_images->image[y][x];
					elements[i].measurements.total_alternate_worm_area++;
				}


				//	elements[i].measurements.total_alternate_worm_area += (alternate_worm_threshold ? 1 : 0);
					//elements[i].measurements.total_intensity_within_alternate_worm += alternate_worm_threshold ? elements[i].registered_images->image[y][x] : 0;

			}
		}
		/*double interframe_time_scaling_factor(1);
		if (0 && !first_frames) {	//turn this off!
			const long dt_s(elements[i].absolute_time - elements[i - movement_time_kernel_width].absolute_time);
			const double standard_interval(60 * 60);//one hour
			interframe_time_scaling_factor = standard_interval / dt_s;
		}
		
		
		elements[i].measurements.interframe_time_scaled_movement_sum = elements[i].measurements.movement_sum*interframe_time_scaling_factor;
		elements[i].measurements.interframe_scaled_spatial_averaged_movement_sum = elements[i].measurements.spatial_averaged_movement_sum*interframe_time_scaling_factor;*/
		
		elements[i].measurements.movement_alternate_worm_sum = (unsigned long)alternate_movement_sum;
	}
}
template<class allocator_T>
void ns_analyzed_image_time_path::copy_aligned_path_to_registered_image(const ns_analyzed_time_image_chunk & chunk, std::vector < ns_image_standard> & temporary_images, ns_time_path_image_movement_analysis_memory_pool<allocator_T> & memory_pool_){

	const unsigned long C(256);
	const unsigned long C_sqrt(16);

	ns_image_properties prop(elements[chunk.start_i].path_aligned_images->image.properties());
	set_path_alignment_image_dimensions(prop);
	if (!(prop == elements[chunk.start_i].path_aligned_images->image.properties()))
		throw ns_ex("The path aligned image has the wrong size!");

	registered_image_properties = prop;
	prop.components = 1;
	float m_min(FLT_MAX), m_max(0);


	if (temporary_images.size() < 3)
		temporary_images.resize(3);
	ns_image_standard & worm_threshold(temporary_images[0]),
		&region_threshold(temporary_images[1]),
		&worm_neighborhood_threshold(temporary_images[2]);
	worm_threshold.prepare_to_recieve_image(registered_image_properties);
	region_threshold.prepare_to_recieve_image(registered_image_properties);
	worm_neighborhood_threshold.prepare_to_recieve_image(registered_image_properties);
	
	long istep(chunk.forward() ? 1 : -1);

	for (long i = chunk.start_i; ; i += istep) {
		if (chunk.forward() && i >= chunk.stop_i)
			break;
		if (!chunk.forward() && i <= chunk.stop_i)
			break;

		if (elements[i].excluded) {
			//		cerr << "Skipping Excluded\n";
			continue;
		}

		//		cerr << "Initializing registered images for " << i << "\n";

		if (!chunk.forward() || !elements[i].registered_image_is_loaded() || (i > ns_analyzed_image_time_path::alignment_time_kernel_width && i > this->first_stationary_timepoint()))
			elements[i].initialize_registered_images(prop, memory_pool_.registered_image_pool);
		//else cerr << "Skipping reg im init";

		if (elements[i].path_aligned_images == 0)
			throw ns_ex("Encountered an unloaded path aligned image!");

		//fill in top border

		//mapping from pixels in im to pixels in im_1


		//ok the tricky thing here is that each individual image is valid between [0, elements[i].worm_context_size()]
		//but the *intersection* between two consecutive frames, which we want to quantify
		//is only valid within the overlapping range [0 + offset, elements[i].worm_context_size+offset].
		//For each image we output the entire [0,element[i].worm_context_size()] offset by v0
		//to registered_images->image, registered_images->region_threshold, and registered_images->worm_threshold
		//but for the registered_movement_image we only output the minimal overlap which is
		// [maximum_alignment_offset()

		const ns_vector_2<float> v0(elements[i].registration_offset);

		ns_vector_2i tl(maximum_alignment_offset() + elements[i].offset_from_path);
		ns_vector_2i br(tl + elements[i].worm_context_size());
		if (br.x > prop.width - maximum_alignment_offset().x)
			br.x = prop.width - maximum_alignment_offset().x;
		if (br.y > prop.width - maximum_alignment_offset().y)
			br.y = prop.width - maximum_alignment_offset().y;

		//here we crop 2i
		//so the registered image rage may be slightly smaller than it could be
		//as we miss the left or right fraction of a pixel
		ns_vector_2i tl_registered(maximum_alignment_offset() + ns_vector_2i(v0.x, v0.y) + elements[i].offset_from_path);
		ns_vector_2i br_registered(tl_registered + elements[i].worm_context_size());
		//ns_vector_2i br(ns_vector_2i(elements[0].path_aligned_images->image.properties().width,elements[0].path_aligned_images->image.properties().height)-maximum_alignment_offset());

		//fill bottom
		for (long y = 0; y < tl_registered.y; y++) {
			for (long x = 0; x < prop.width; x++) {
				elements[i].registered_images->image[y][x] = NS_MARGIN_BACKGROUND;

				region_threshold[y][x] = NS_MARGIN_BACKGROUND;
				worm_threshold[y][x] = NS_MARGIN_BACKGROUND;
			}
		}

		for (long y = tl_registered.y; y < br_registered.y; y++) {
//fill in left gap
for (long x = 0; x < (tl_registered.x); x++) {
	elements[i].registered_images->image[y][x] = NS_MARGIN_BACKGROUND;
	region_threshold[y][x] = NS_MARGIN_BACKGROUND;
	worm_threshold[y][x] = NS_MARGIN_BACKGROUND;
}
//fill in center
for (long x = tl_registered.x; x < br_registered.x; x++) {
	elements[i].registered_images->image[y][x] = elements[i].path_aligned_images->image.sample_f(y - v0.y, x - v0.x);
	elements[i].path_aligned_images->sample_thresholds<ns_8_bit>(y - v0.y, x - v0.x, region_threshold[y][x], worm_threshold[y][x]);
}
//fill in right gap
for (long x = (br_registered.x); x < prop.width; x++) {
	elements[i].registered_images->image[y][x] = NS_MARGIN_BACKGROUND;
	region_threshold[y][x] = NS_MARGIN_BACKGROUND;
	worm_threshold[y][x] = NS_MARGIN_BACKGROUND;
}
		}
		//fill in top
		for (long y = br_registered.y; y < prop.height; y++) {
			for (long x = 0; x < prop.width; x++) {
				elements[i].registered_images->image[y][x] = NS_MARGIN_BACKGROUND;
				worm_threshold[y][x] = NS_MARGIN_BACKGROUND;
				region_threshold[y][x] = NS_MARGIN_BACKGROUND;
			}
		}
		ns_dilate<16, ns_image_standard, ns_image_standard>(worm_threshold, worm_neighborhood_threshold);
		for (unsigned int y = 0; y < worm_threshold.properties().height; y++)
			for (unsigned int x = 0; x < worm_threshold.properties().width; x++)
				elements[i].registered_images->set_thresholds(y, x, region_threshold[y][x], worm_threshold[y][x], worm_neighborhood_threshold[y][x], 0);

		//xxx
		if (i - istep > 0 && elements[i - istep].registered_image_is_loaded()) {
			ns_64_bit sum(0);
			for (unsigned int y = 0; y < worm_threshold.properties().height; y++)
				for (unsigned int x = 0; x < worm_threshold.properties().width; x++)
					sum += abs(elements[i].registered_images->image[y][x] - (long)elements[i - istep].registered_images->image[y][x]);


		}

#ifdef NS_CALCULATE_SLOW_IMAGE_REGISTRATION
		if (tmp_cmp == 0) {
			tmp_cmp = new ofstream("c:\\server\\alignment_debug.csv");
			(*tmp_cmp) << "direction,Synx,Syny,Sx,Sy,Stime,Sregistration_error,Fx,Fy,Ftime,Fregistration_error\n";
		}
		(*tmp_cmp) << (chunk.forward() ? "forward" : "backward") << ",";
		(*tmp_cmp) << elements[i].synthetic_offset.x << "," << elements[i].synthetic_offset.y << ",";

		(*tmp_cmp) << elements[i].registration_offset.x << "," << elements[i].registration_offset.y << ","
			<< elements[i].alignment_times[0] << "," << (elements[i].synthetic_offset - elements[i].registration_offset).mag() << ",";
		(*tmp_cmp) << elements[i].registration_offset_slow.x << "," << elements[i].registration_offset_slow.y << ","
			<< elements[i].alignment_times[1] << "," << (elements[i].synthetic_offset - elements[i].registration_offset_slow).mag() << "\n";
		tmp_cmp->flush();
#endif
	}

}
void ns_analyzed_image_time_path::calculate_movement_images(const ns_analyzed_time_image_chunk & chunk) {


	const long n(movement_detection_kernal_half_width);
	const long kernel_area((2 * n + 1)*(2 * n + 1));

	long istep(chunk.forward() ? 1 : -1);
	const long dt(movement_time_kernel_width);
	const unsigned long movement_start_i(chunk.forward() ? (first_stationary_timepoint() + dt) : dt);

#ifdef NS_CALCULATE_OPTICAL_FLOW
	if (flow == 0)
		flow = new ns_optical_flow_processor();
#endif
	for (long i = chunk.start_i; ; i += istep) {
		if (chunk.forward() && i >= chunk.stop_i)
			break;
		if (!chunk.forward() && i <= chunk.stop_i)
			break;

		if (elements[i].excluded) {
			continue;
		}
		elements[i].element_was_processed = true;

		bool first_frames(chunk.forward() ? (i < movement_start_i) : (i < movement_start_i));

#ifdef NS_CALCULATE_OPTICAL_FLOW
		if (first_frames) {
			flow->Dim1.from(elements[i].registered_images->image);
			flow->Dim2.from(elements[i].registered_images->image);
		}
		else {
			//when running backwards, we register in reverse but we calculate movement as if we are running /forward/!
			if (elements[i - dt].registered_images == 0)
				throw ns_ex("Improperly deallocated registered image encountered on element i=") << i << "(" << i - dt << ")";
			flow->Dim1.from(elements[i - dt].registered_images->image);
			flow->Dim2.from(elements[i].registered_images->image);
		}
		calculate_flow(i);
#endif

		if (first_frames) {
			for (unsigned int j = 0; j < 256; j++)
				elements[i].registered_images->histogram_matching_factors[j] = j;
		}
		else
			ns_match_histograms(elements[i].registered_images->image, elements[i - dt].registered_images->image, elements[i].registered_images->histogram_matching_factors);
		elements[i].registered_images->histograms_matched = true;

		//now we generate the movement image
		const ns_vector_2d v1(first_frames ? ns_vector_2d(0, 0) : elements[i - dt].registration_offset);

		//fill in bottom
		for (long y = 0; y < n; y++) {
			for (long x = 0; x < registered_image_properties.width; x++) {
				elements[i].registered_images->movement_image_[y][x] = NS_MARGIN_BACKGROUND;
			}
		}
		if (first_frames) {
			for (long y = n; y < registered_image_properties.height - n; y++)
				for (long x = 0; x < registered_image_properties.width; x++)
					elements[i].registered_images->movement_image_[y][x] = elements[i].registered_images->image[y][x];
		}
		else {
			for (long y = n; y < registered_image_properties.height - n; y++) {
				//fill in left gap
				for (long x = 0; x < n; x++)
					elements[i].registered_images->movement_image_[y][x] = NS_MARGIN_BACKGROUND;

				//fill in center
				for (long x = 0 + n; x < registered_image_properties.width - n; x++) {

#ifdef NS_DEBUG_IMAGE_ACCESS
					if (y >= elements[i].registered_images->image.properties().height ||
						x >= elements[i].registered_images->image.properties().width)
						throw ns_ex("Yikes");
#endif

					float d_numerator(0),
						d_denominator(0);

					for (long dy = -n; dy <= n; dy++) {
						for (long dx = -n; dx <= n; dx++) {
							float d(elements[i].registered_images->image[y][x]
								- elements[i].registered_images->histogram_matching_factors[elements[i - dt].registered_images->image[y][x]]);
							d_numerator += d;
						}
					}



					float val = d_numerator /= kernel_area;
					//note that the absolute value of val can never be greater than 255  
					//so val contains 9 bits of information
					elements[i].registered_images->movement_image_[y][x] = (short)val;

				}
				//fill in right gap
				for (long x = (registered_image_properties.width - n); x < registered_image_properties.width; x++) {
					elements[i].registered_images->movement_image_[y][x] = NS_MARGIN_BACKGROUND;

				}
			}
		}
		//fill in top
		for (long y = registered_image_properties.height - n; y < registered_image_properties.height; y++) {
			for (long x = 0; x < registered_image_properties.width; x++) {
				elements[i].registered_images->movement_image_[y][x] = NS_MARGIN_BACKGROUND;
			}
		}
		//start building up the data structure that will be used at the end to calculate the stabilized worm neighborhood threshold.
		if (stabilized_worm_region_total == 0) {
			ns_image_properties prop;
			prop.width = registered_image_properties.width;
			prop.height = registered_image_properties.height;
			prop.components = 1;
			stabilized_worm_region_temp.init(prop);
			for (unsigned int y = 0; y < registered_image_properties.height; y++)
				for (unsigned int x = 0; x < registered_image_properties.width; x++)
					stabilized_worm_region_temp[y][x] = 0;
		}
		for (unsigned int y = 0; y < registered_image_properties.height; y++)
			for (unsigned int x = 0; x < registered_image_properties.width; x++)
				stabilized_worm_region_temp[y][x] +=
				(elements[i].registered_images->get_worm_neighborhood_threshold(y, x)) ? 1 : 0;
		stabilized_worm_region_total++;
	}
}
void ns_analyzed_image_time_path::calculate_flow(const unsigned long element_index) {
	#ifdef NS_CALCULATE_OPTICAL_FLOW
	ns_high_precision_timer p;
	
	p.start();
	flow->calculate(30,0,2/255.0f);
	const ns_64_bit cur_t(p.stop());

	total_flow_time += cur_t;
	total_flow_calc++;
	cerr << cur_t / 1000.0f << " (" << total_flow_time / total_flow_calc / 1000.0f << ")\n";


	flow->get_movement(elements[element_index].registered_images->flow_image_dx, elements[element_index].registered_images->flow_image_dy);
#else
	throw ns_ex("Atempting to calculate flow with NS_CALCULATE_OPTICAL_FLOW set to false");
#endif
	//cerr << "}";
	/*
	float mmin(100), mmax(-100);
	for (unsigned int y = 0; y < elements[element_index].registered_images->flow_image_dx.properties().height; y++)
		for (unsigned int x = 0; x < elements[element_index].registered_images->flow_image_dx.properties().width; x++) {
			if (elements[element_index].registered_images->flow_image_dx[y][x] < mmin)
				mmin = elements[element_index].registered_images->flow_image_dx[y][x];
			if (elements[element_index].registered_images->flow_image_dx[y][x] > mmax)
				mmax = elements[element_index].registered_images->flow_image_dx[y][x];	
			if (elements[element_index].registered_images->flow_image_dy[y][x] < mmin)
				mmin = elements[element_index].registered_images->flow_image_dy[y][x];
			if (elements[element_index].registered_images->flow_image_dy[y][x] > mmax)
				mmax = elements[element_index].registered_images->flow_image_dy[y][x];
		}
	
	cout << "f(" << mmin << "," << mmax << ") ";*/
}


ns_analyzed_time_image_chunk ns_analyzed_image_time_path::initiate_image_registration(const ns_analyzed_time_image_chunk & chunk,ns_alignment_state & state, ns_calc_best_alignment_fast & align){
	const unsigned long time_kernel(alignment_time_kernel_width);
	//if (abs(chunk.stop_i - chunk.start_i) < time_kernel)
		//throw ns_ex("ns_analyzed_image_time_path::initiate_image_registration()::First chunk must be >= time kernel width!");
	ns_analyzed_time_image_chunk remaining(chunk);

	const unsigned long first_index(chunk.start_i);
	ns_image_properties prop(elements[first_index].path_aligned_images->image.properties());
	const bool first_frame(state.registration_offset_count == 0);
	if (first_frame) {
		set_path_alignment_image_dimensions(prop);

		state.consensus.init(prop);
		state.consensus_count.init(prop);
	}

	//image is offset by maximum_alignment offset in each direction
	//but we the largest vertical alignment in either direction is actually half that,
	//because two consecutive frames can be off by -max and max, producing a overal differential of 2*max offset between the two images


	//if (max_alignment_offset.x < 0) max_alignment_offset.x = 0;
	//if  (max_alignment_offset.y < 0) max_alignment_offset.y = 0;

	const ns_vector_2i bottom_offset(maximum_alignment_offset());
	const ns_vector_2i top_offset(maximum_alignment_offset());


	//load first consensus kernal
	
	const ns_vector_2i d(maximum_alignment_offset());	
	//an extra minus one because we have floating point offsets
	//and the furthest right  pixel is (width-1), i.e we can't interpolate pixels left of width-1.
	const ns_vector_2i h_sub(ns_vector_2i(state.consensus.properties().width-1,
										  state.consensus.properties().height-1)
										  -d);

	int step(chunk.forward() ? 1 : -1);

	if (first_frame) {
		for (unsigned long y = 0; y < prop.height; y++) {
			for (unsigned long x = 0; x < prop.width; x++) {
				state.consensus[y][x] = 0;
				state.consensus_count[y][x] = 0;
			}
		}
		for (long y = d.y; y < h_sub.y; y++) {
			for (long x = d.x; x < h_sub.x; x++) {
				state.consensus[y][x] = elements[first_index].path_aligned_images->image[y][x];
				state.consensus_count[y][x] = 1;
			}
		}
		state.registration_offset_sum = ns_vector_2i(0, 0);
		state.registration_offset_count = 1;

		//xxx
		//state.consensus_internal_offset = ns_vector_2i(0, 0);

		#ifdef NS_OUTPUT_ALGINMENT_DEBUG
		ns_debug_image_out out;
		out("c:\\movement", debug_path_name + "0.tif", alignment_time_kernel_width, state.consensus, state.consensus_count, elements[0].path_aligned_images->image);
		#endif
		elements[first_index].registration_offset = ns_vector_2i(0, 0);
		remaining.start_i+= step;
	}

	//ns_vector_2i test_alignment = align(state,elements[0].path_aligned_images->image);
	//cerr << "TAlignment: " << test_alignment << "\n";
	for (long i = remaining.start_i; i != remaining.stop_i; i+=step){
		if ( elements[i].path_aligned_images->image.properties().height == 0)
			throw ns_ex("ns_analyzed_image_time_path::calculate_vertical_registration()::Element ")  << i << " was not assigned!";
		
		//if we have filled up the state variable and can stop the inialization.
		//we break and return remaining, which is should contain the remainder 
		//of the current chunk.
		if (state.registration_offset_count >= time_kernel) {
			remaining.start_i = i;
			break;
		}
#ifdef NS_DEBUG_FAST_IMAGE_REGISTRATION
		global_path_debug_info.group_id = group_id;
		global_path_debug_info.path_id = path_id;
		global_path_debug_info.time = elements[i].absolute_time;
#endif
		#ifdef NS_USE_FAST_IMAGE_REGISTRATION

			const ns_vector_2i tl_worm_context_position_in_pa(ns_analyzed_image_time_path::maximum_alignment_offset() + elements[i].offset_from_path);
		
			elements[i].registration_offset = align(state.registration_offset_average(),
													maximum_alignment_offset(), 
													state, 
													elements[i].path_aligned_images->image, 
													elements[i].saturated_offset,
													tl_worm_context_position_in_pa,
													elements[i].worm_context_size());
			//cerr << elements[i].registration_offset;
	#endif
		#ifdef NS_CALCULATE_SLOW_IMAGE_REGISTRATION
			ns_calc_best_alignment align(NS_SUBPIXEL_REGISTRATION_CORSE, NS_SUBPIXEL_REGISTRATION_FINE, maximum_alignment_offset(), maximum_local_alignment_offset(), bottom_offset, top_offset);
			elements[i].registration_offset_slow = align(state,elements[i].path_aligned_images->image,elements[i].saturated_offset);
		#endif

		state.registration_offset_sum += elements[i].registration_offset;
		state.registration_offset_count++;

	//	cerr << "O:" << elements[i].vertical_registration << "\n";
		//cerr << i << ":" << (unsigned long)consensus[10][21] << "<-" << (unsigned long)elements[i].path_aligned_images->image[10-maximum_alignment_offset()-elements[i].vertical_registration][21] << "\n";
		if (d.y < elements[i].registration_offset.y ||
			d.x < elements[i].registration_offset.x ||
			h_sub.y - elements[i].registration_offset.y >= state.consensus.properties().height ||
			h_sub.x - elements[i].registration_offset.x >= state.consensus.properties().width) {
		  cerr << "invalid registration: " << elements[i].registration_offset.x << "," << elements[i].registration_offset.y << ";" << d << ";" << h_sub << "\n";
		  throw ns_ex("invalid registration: ") << elements[i].registration_offset.x << "," << elements[i].registration_offset.y << ";" << d << ";" << h_sub;
		}
		for (long y = d.y; y < h_sub.y; y++){
			for (long x = d.x; x < h_sub.x; x++){
				state.consensus[y][x] += elements[i].path_aligned_images->image.sample_f(y-elements[i].registration_offset.y,
																						 x-elements[i].registration_offset.x);
				state.consensus_count[y][x]++;
			}
		}

		#ifdef NS_OUTPUT_ALGINMENT_DEBUG
		out("c:\\movement",debug_path_name + ns_to_string(i) + ".tif",alignment_time_kernel_width,state.consensus,state.consensus_count,elements[i].path_aligned_images->image);
		#endif
		} 
	//we've consumed the entire chunk, so there's nothing remaining.
	remaining.start_i = chunk.stop_i;
	return remaining;
}

ns_vector_2i ns_analyzed_image_time_path::max_step_alignment_offset() {
	ns_vector_2i max_alignment_offset(maximum_alignment_offset().x / 2 - movement_detection_kernal_half_width,
		maximum_alignment_offset().y / 2 - movement_detection_kernal_half_width);
	if (max_alignment_offset.x < 0) max_alignment_offset.x = 0;
	if (max_alignment_offset.y < 0) max_alignment_offset.y = 0;
	return max_alignment_offset;
}

void ns_analyzed_image_time_path::calculate_image_registration(const ns_analyzed_time_image_chunk & chunk_, ns_alignment_state & state, const ns_analyzed_time_image_chunk & first_chunk, ns_calc_best_alignment_fast & align) {
	ns_analyzed_time_image_chunk chunk(chunk_);
	//if the registration needs to be initialized, do so.
	if (state.registration_offset_count < alignment_time_kernel_width) {
		chunk = initiate_image_registration(chunk_, state,align);
		//we've consumed the whole chunk initializing image registration.  We're done!
		if (chunk.start_i == chunk.stop_i)
			return;
	}

	//make alignment object
		//image is offset by maximum_alignment offset in each direction
	//but we the largest vertical alignment in either direction is actually half that,
	//because two consecutive frames can be off by -max and max, producing a overal differential of 2*max offset between the two images

	const ns_vector_2i bottom_offset(maximum_alignment_offset());
	const ns_vector_2i top_offset(maximum_alignment_offset());
	#ifdef NS_CALCULATE_SLOW_IMAGE_REGISTRATION
	ns_calc_best_alignment align(NS_SUBPIXEL_REGISTRATION_CORSE, NS_SUBPIXEL_REGISTRATION_FINE, maximum_alignment_offset(), maximum_local_alignment_offset(), bottom_offset, top_offset);
	#endif
	const long time_kernal(alignment_time_kernel_width);

	//define some constants
	ns_image_properties prop;
	set_path_alignment_image_dimensions(prop);
	ns_vector_2i d(maximum_alignment_offset());

	const ns_vector_2i h_sub(ns_vector_2i(state.consensus.properties().width - 1,
		state.consensus.properties().height - 1)
		- d);
	long step(chunk.forward() ? 1 : -1);
	for (long i = chunk.start_i; ; i += step) {
		if (chunk.forward() && i >= chunk.stop_i)
			break;
		if (!chunk.forward() && i <= chunk.stop_i)
			break;

		if (elements[i].excluded) continue;

		if (!elements[i - step*time_kernal].path_aligned_image_is_loaded()) {
			throw ns_ex("Image for ") << i - step*time_kernal << " isn't loaded. (step - " << step << "\n";
		}

		//xxx add synthetic shift to a random amount up and left.
		elements[i].synthetic_offset = ns_vector_2d(0, 0);// ns_vector_2d((rand() % 50) / 10.0, (rand() % 50) / 10.0);
	/*	for (unsigned int y = 0; y < elements[i].path_aligned_images->image.properties().height - ceil(elements[i].synthetic_offset.y)-1; y++){
			for (unsigned int x = 0; x < elements[i].path_aligned_images->image.properties().width - ceil(elements[i].synthetic_offset.x)-1; x++) {
				elements[i].path_aligned_images->image[y][x] = elements[i].path_aligned_images->image.sample_d(y + elements[i].synthetic_offset.y, x + elements[i].synthetic_offset.x);
			}
			for (unsigned int x = elements[i].path_aligned_images->image.properties().width - ceil(elements[i].synthetic_offset.x)-1; x < elements[i].path_aligned_images->image.properties().width; x++)
				elements[i].path_aligned_images->image[y][x] = 0;
		}
		for (unsigned int y = elements[i].path_aligned_images->image.properties().height - ceil(elements[i].synthetic_offset.y)-1; y < elements[i].path_aligned_images->image.properties().height; y++) {
			for (unsigned int x = 0; x < elements[i].path_aligned_images->image.properties().width; x++)
				elements[i].path_aligned_images->image[y][x] = 0;
		}
		cout << "Synthetic:" << elements[i].synthetic_offset << "\n";
		*/
		ns_high_precision_timer t;
	//	for (unsigned int kk = 0; kk < 3; kk++)
		#ifdef NS_CALCULATE_SLOW_IMAGE_REGISTRATION
		t.start();
		elements[i].registration_offset_slow = align(state,elements[i].path_aligned_images->image,elements[i].saturated_offset);
		elements[i].alignment_times[0] = t.stop() ;

		//copy over to help debug fast image alignment
		fast_alignment.debug_gold_standard_shift = elements[i].registration_offset_slow;
		#ifdef NS_OUTPUT_ALGINMENT_DEBUG
		cerr << "Alignment: " << elements[i].registration_offset << "\n";
		#endif
		//xxx
	#endif
	#ifdef	NS_DEBUG_FAST_IMAGE_REGISTRATION
	global_path_debug_info.group_id = group_id;
	global_path_debug_info.path_id = path_id;
	global_path_debug_info.time = elements[i].absolute_time;
	#endif
	#ifdef NS_USE_FAST_IMAGE_REGISTRATION
		//align.debug_gold_standard_shift = elements[i].registration_offset;
		t.start();
		const ns_vector_2i tl_worm_context_position_in_pa(ns_analyzed_image_time_path::maximum_alignment_offset() + elements[i].offset_from_path);
		elements[i].registration_offset =
			align(state.registration_offset_average() - state.consensus_internal_offset,maximum_alignment_offset(), state, elements[i].path_aligned_images->image, elements[i].saturated_offset, tl_worm_context_position_in_pa,elements[i].worm_context_size())
			+ //every once in a while we recenter the state if the offsets are getting close to saturating. 
			state.consensus_internal_offset;
		
		elements[i].alignment_times[1] = t.stop();
	#endif
	
		state.registration_offset_sum += (elements[i].registration_offset - elements[i-step*time_kernal].registration_offset);
		//elements[i].alignment_times[1] = t.stop();
		for (long y = d.y; y < h_sub.y; y++){
			for (long x = d.x; x < h_sub.x; x++){
				state.consensus		 [y][x]+= elements[i            ].path_aligned_images->image.sample_f(y-elements[i].registration_offset.y + state.consensus_internal_offset.y,x-elements[i].registration_offset.x + state.consensus_internal_offset.x);
			//	state.consensus_count[y][x]++;

				state.consensus		 [y][x]-= elements[i-step*time_kernal].path_aligned_images->image.sample_f(y-elements[i-step*time_kernal].registration_offset.y + state.consensus_internal_offset.y,x-elements[i-step*time_kernal].registration_offset.x+ +state.consensus_internal_offset.x);
			//	state.consensus_count[y][x]--;
			
			}
		}
		
		//xxx todo: this code should alow images to be registered at arbitrary distances over time
		//by recentering the state when it drifts too far from its initial state.
		//however, the way path_aligned_images are written to disk would have to be rejigged,
		//as each image would need to be cropped at the boundaries of maximum_alignment_offset
		//doable, but not now.
		/*
		if (abs(state.registration_offset_average().x) > maximum_alignment_offset().x / 2 ||
			abs(state.registration_offset_average().y) > maximum_alignment_offset().y / 2) {
			ns_vector_2i new_consensus_offset(state.consensus_internal_offset);
			if (new_consensus_offset.x > maximum_alignment_offset().x / 2)
				new_consensus_offset.x += maximum_alignment_offset().x/2;
			if (new_consensus_offset.y > maximum_alignment_offset().y / 2)
				new_consensus_offset.y += maximum_alignment_offset().y/2;

			//since we are offset the entire buffer by new_consensus_offset
			//we need to shift its contents in the opposite direction
			ns_vector_2i d = state.consensus_internal_offset - new_consensus_offset;

			int start_x(0), stop_x(state.consensus_count.properties().width), start_y(0), stop_y(state.consensus_count.properties().height), dx(0), dy(0);
			if (d.x < 0) {
				start_x = state.consensus_count.properties().width-1;
				stop_x = -1;
				dx = -1;
			}if (d.y < 0) {
				start_y = state.consensus_count.properties().height-1;
				stop_y = -1;
				dy = -1;
			}
			for (unsigned int y = start_y; y != start_y + d.y; y += dy) {
				for (unsigned int x = start_x; x < stop_x; x += dx) {
					state.consensus_count[y][x] = 0;
					state.consensus[y][x] = 0;
				}
			}
			for (unsigned int y = start_y + d.y; y != stop_y; y += dy) {
				for (unsigned int x = start_x; x < stop_x + d.x; x += dx)
					state.consensus_count[y][x] = 0;
				for (unsigned int x = start_x + d.x; x < stop_x; x += dx) {
					state.consensus_count[y][x] = state.consensus_count[y - d.y][x - d.x];
					state.consensus[y][x] = state.consensus[y - d.y][x - d.x];
				}
			}
			state.consensus_internal_offset = new_consensus_offset;
		}
		*/
		#ifdef NS_OUTPUT_ALGINMENT_DEBUG
		ns_debug_image_out out;
		out("c:\\movement\\",debug_path_name + ns_to_string(i) + ".tif",alignment_time_kernel_width,state.consensus,state.consensus_count,elements[i].path_aligned_images->image);
		#endif
	}
}

template<class T> 
struct ns_index_orderer{
	ns_index_orderer<T>(const unsigned long i,const T * d):index(i),data(d){}
	bool operator<(const ns_index_orderer<T> & r) const {return *data < *r.data;}
	unsigned long index;
	const T * data;
};
template<class allocator_T>
ns_analyzed_image_time_path_group<allocator_T>::ns_analyzed_image_time_path_group(const ns_stationary_path_id group_id, const ns_64_bit region_info_id,const ns_time_path_solution & solution_, const ns_death_time_annotation_time_interval & observation_time_interval,ns_death_time_annotation_set & rejected_annotations,ns_time_path_image_movement_analysis_memory_pool<allocator_T> & memory_pool){
	
	paths.reserve(solution_.path_groups[group_id.group_id].path_ids.size());
	unsigned long current_path_id(0);

	//we find when the first and last observations of the plate were made
	//these are needed for generating proper event times when those occur at the edges of observation
	ns_time_path_limits limits;
	limits.first_obsevation_of_plate.period_start = ULONG_MAX;
	limits.first_obsevation_of_plate.period_end = ULONG_MAX;
	limits.last_obsevation_of_plate.period_start = 0;
	limits.last_obsevation_of_plate.period_end = 0;
	for (unsigned int i = 0; i < solution_.timepoints.size(); i++){
		if (solution_.timepoints[i].time < observation_time_interval.period_start || 
			solution_.timepoints[i].time > observation_time_interval.period_end)
			continue;
		if (limits.first_obsevation_of_plate.period_start == ULONG_MAX)
			limits.first_obsevation_of_plate.period_start = solution_.timepoints[i].time;
		else if (limits.first_obsevation_of_plate.period_end == ULONG_MAX){
			limits.first_obsevation_of_plate.period_end = solution_.timepoints[i].time;
		}
		if (limits.last_obsevation_of_plate.period_end < solution_.timepoints[i].time){
			limits.last_obsevation_of_plate.period_start = limits.last_obsevation_of_plate.period_end;
			limits.last_obsevation_of_plate.period_end = solution_.timepoints[i].time;
		}
	}

	//if (group_id_ == 28)
	//			cerr << "MA";
	//if (limits.last_obsevation_of_plate.periode
	paths.reserve(solution_.path_groups[group_id.group_id].path_ids.size());
	for (unsigned int i = 0; i < solution_.path_groups[group_id.group_id].path_ids.size(); i++){
		const unsigned long & path_id(solution_.path_groups[group_id.group_id].path_ids[i]);
		const ns_time_path & source_path(solution_.paths[path_id]);
		paths.emplace_back(0);
		ns_analyzed_image_time_path &path(paths[current_path_id]);

		path.path = &source_path;
		path.group_id = group_id;
		path.group_id.path_id = path_id;
		path.solution = &solution_;
		path.region_info_id = region_info_id;
		
		if (source_path.stationary_elements.size() == 0)
			throw ns_ex("ns_analyzed_image_time_path_group::ns_analyzed_image_time_path_group()::Empty timepath found!");
		
		vector<ns_index_orderer<unsigned long> > ordered_time;
		ordered_time.reserve(source_path.stationary_elements.size());
		for (unsigned int j = 0; j < source_path.stationary_elements.size(); j++)
			ordered_time.push_back(ns_index_orderer<unsigned long>(j,&solution_.time(source_path.stationary_elements[j])));
		std::sort(ordered_time.begin(),ordered_time.end());
		
	//	ns_time_path_limits limits;
		//set the time of the first measurement after the path ends.
		//this is used to annotate the time at which the path disappears
		const unsigned long last_time_index(source_path.stationary_elements[ordered_time.rbegin()->index].t_id);
		const unsigned long first_time_index(source_path.stationary_elements[ordered_time.begin()->index].t_id);

		if (last_time_index+1 >= solution_.timepoints.size() ||
			solution_.timepoints[last_time_index].time > limits.last_obsevation_of_plate.period_start
			){
			limits.interval_after_last_observation.period_start = solution_.timepoints.rbegin()->time;
			limits.interval_after_last_observation.period_end_was_not_observed = true;
		}
		else limits.interval_after_last_observation = ns_death_time_annotation_time_interval(solution_.timepoints[last_time_index].time,
																							solution_.timepoints[last_time_index+1].time);
		if (first_time_index < 1 ||
			solution_.timepoints[first_time_index].time < limits.first_obsevation_of_plate.period_start){
			limits.interval_before_first_observation.period_start_was_not_observed = true;
			limits.interval_before_first_observation.period_end = solution_.timepoints[0].time;
		}
		else limits.interval_before_first_observation = ns_death_time_annotation_time_interval(solution_.timepoints[first_time_index-1].time,
																							solution_.timepoints[first_time_index].time);
		
		path.set_time_path_limits(limits);			
			
		path.elements.reserve(source_path.stationary_elements.size());
		for (unsigned int j = 0; j < source_path.stationary_elements.size(); j++){
			
			const unsigned long s = path.elements.size();
			const unsigned long absolute_time(solution_.time(source_path.stationary_elements[ordered_time[j].index]));
			if (s > 0 && path.elements[s-1].absolute_time == absolute_time){
				if (path.elements[s-1].excluded) continue;
				//add an annotation that there is an extra worm at the specified position
				/*std::string expl("PD");
				rejected_annotations.events.push_back(
					 ns_death_time_annotation(ns_no_movement_event,
					 0,region_info_id,
					 ns_death_time_annotation_time(solution_.time(source_path.stationary_elements[ordered_time[j].index]),solution_.time(source_path.stationary_elements[ordered_time[j].index])),
					 solution_.element(source_path.stationary_elements[ordered_time[j].index]).region_position,
					 solution_.element(source_path.stationary_elements[ordered_time[j].index]).region_size,
					 ns_death_time_annotation::ns_machine_excluded,
					 ns_death_time_annotation_event_count(2,0),ns_current_time(),
					 ns_death_time_annotation::ns_lifespan_machine,
					 (solution_.element(source_path.stationary_elements[ordered_time[j].index]).part_of_a_multiple_worm_disambiguation_cluster)?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
					 ns_stationary_path_id(),false,
					 expl)
				);*/
				path.elements[s-1].number_of_extra_worms_observed_at_position++;
				continue; //skip frames taken at duplicate times
			}

			path.elements.resize(s+1);
			path.elements[s].absolute_time = absolute_time;
			path.elements[s].relative_time = path.elements[s].absolute_time - path.elements[0].absolute_time;
			path.elements[s].context_position_in_region_vis_image = solution_.element(source_path.stationary_elements[ordered_time[j].index]).context_image_position_in_region_vis_image;
			path.elements[s].inferred_animal_location = solution_.element(source_path.stationary_elements[ordered_time[j].index]).inferred_animal_location;
			path.elements[s].element_before_fast_movement_cessation = solution_.element(source_path.stationary_elements[ordered_time[j].index]).element_before_fast_movement_cessation;

			path.elements[s].worm_region_size_ = solution_.element(source_path.stationary_elements[ordered_time[j].index]).region_size;
			path.elements[s].worm_context_size_ = solution_.element(source_path.stationary_elements[ordered_time[j].index]).context_image_size;

			path.elements[s].part_of_a_multiple_worm_disambiguation_group = solution_.element(source_path.stationary_elements[ordered_time[j].index]).part_of_a_multiple_worm_disambiguation_cluster;
			//we inherited a path_position_offset offset due to the extra margin
			//of context pixels included in the region_visualization image.
			//thus, the absolute position of all our pixels are actually offset
			path.elements[s].region_position_in_source_image = 
				solution_.element(source_path.stationary_elements[ordered_time[j].index]).region_position;
			path.elements[s].context_position_in_source_image = 
				solution_.element(source_path.stationary_elements[ordered_time[j].index]).context_image_position;
			path.elements[s].subregion_info = solution_.element(source_path.stationary_elements[ordered_time[j].index]).subregion_info;
		  
			//we'll subtract out the path location later 
		//	path.elements[s].offset_from_path =  path.elements[s].context_position_in_source_image;
		}
		path.find_first_labeled_stationary_timepoint();
		//if this path was shrunk down below the minimum size for analysis, remove it.
		const unsigned long current_time(ns_current_time());
		if (path.elements.size() < ns_analyzed_image_time_path::alignment_time_kernel_width){
		//	cerr << "Path size : " << path.elements.size() << "\n";
			for (unsigned int j = 0; j < path.elements.size(); j++){
				
				if (path.elements[j].excluded || path.elements[j].inferred_animal_location) continue;
				 rejected_annotations.events.push_back(
					 ns_death_time_annotation(ns_fast_moving_worm_observed,
					 0,region_info_id,
					 ns_death_time_annotation_time_interval(path.elements[j].absolute_time,path.elements[j].absolute_time),
					 path.elements[j].region_offset_in_source_image(),
					 path.elements[j].worm_region_size(),
					 ns_death_time_annotation::ns_not_excluded,
					 ns_death_time_annotation_event_count(1+path.elements[j].number_of_extra_worms_observed_at_position,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
					 path.elements[j].part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
					 ns_stationary_path_id(0,0,group_id.detection_set_id),false,false, path.elements[j].subregion_info, ns_death_time_annotation::ns_explicitly_observed,
					 "PTS")
					 );
			}
			path.elements.resize(0);
			paths.pop_back();
			continue;
		}
		
		//first we calculate the path bounding box in absolute coordinates
		ns_vector_2i tl(path.elements[0].context_position_in_source_image),
					 br(path.elements[0].context_position_in_source_image+path.elements[0].worm_context_size_);
		
		ofstream * debug(0);
/*		if (0 && group_id_==42){
			string fname("c:\\path_foo_");
			srand(ns_current_time());
			fname += ns_to_string(rand());
			fname += ".tif";
			debug = new ofstream(fname.c_str());
			*debug << "TLx,TLy,BRx,BRy,|,POSx,POSY,POS+SIZEX,POS+SIZEY\n";
		}*/
		for (unsigned int j = 1; j < path.elements.size(); j++){
			const ns_analyzed_image_time_path_element & e(path.elements[j]);
			const ns_vector_2i & pos(e.context_position_in_source_image);
			if (pos.x == -1 || pos.y == -1)
				throw ns_ex("An unspecified context position was found in the time path solution");
			const ns_vector_2i & size(e.worm_context_size_);
			if (debug != 0){
				*debug << tl << "," << br << ",|," << pos << "," << pos+size << "\n";
			}
			if (pos.x < tl.x)
				tl.x = pos.x;
			if (pos.y < tl.y)
				tl.y = pos.y;
			if (pos.x + size.x > br.x)
				br.x = pos.x + size.x;
			if (pos.y + size.y > br.y)
				br.y = pos.y + size.y; 
		}
		if (debug != 0)
			debug->close();
		//the absolute coordinates of the path
		path.path_context_position = tl;
		path.path_context_size = br - tl;

		//we want to map from region positions to context positions
		path.path_region_position = tl + solution_.element(source_path.stationary_elements[ordered_time[0].index]).region_position  
													 - solution_.element(source_path.stationary_elements[ordered_time[0].index]).context_image_position;
		path.path_region_size = path.path_context_size + solution_.element(source_path.stationary_elements[ordered_time[0].index]).region_size
												 - solution_.element(source_path.stationary_elements[ordered_time[0].index]).context_image_size;
	
	//	path.worm_size = path.region_size;

		for(unsigned int j = 0; j < path.elements.size(); j++)
			path.elements[j].offset_from_path = path.elements[j].context_position_in_source_image - path.path_context_position;
		
		current_path_id++;
	}
}

void ns_movement_image_collage_info::from_path(const ns_analyzed_image_time_path * p, const unsigned long only_output_first_n_frames){
	const unsigned long max_width(3000);

	if (p->element_count() == 0){
		frame_dimensions = ns_vector_2i(0,0);
		grid_dimensions = ns_vector_2i(0,0);
		prop.width = 0;
		prop.height = 0;
		prop.components = 0;
		prop.resolution = 0;
		return;
	}
	ns_image_properties pp;
	p->set_path_alignment_image_dimensions(pp);
	frame_dimensions.x = pp.width;
	frame_dimensions.y = pp.height;
	//frame_dimensions = p->path_context_size;
	if (frame_dimensions.x > max_width)
		throw ns_ex("ns_movement_image_collage_info()::Frame too large!");

	//int frames_per_row(max_width/frame_dimensions.x);
	int frames_per_row = 1;
	//we could get a lot nicer images by having multiple frames per row.
	//however, this would mean we'd not be able to output frames one at a time, which would complicate things.

	//if (frame_dimensions.x*p.element_count() < max_width)
	//	frames_per_row = p.element_count();
	unsigned long frame_count(only_output_first_n_frames);
	if (frame_count == 0)
		frame_count= p->element_count();
	grid_dimensions = ns_vector_2i(frames_per_row,frame_count/frames_per_row  + (unsigned long)((frame_count % frames_per_row)>0));

	prop = p->registered_image_properties;

	prop.width = grid_dimensions.x*frame_dimensions.x;
	prop.height = grid_dimensions.y*frame_dimensions.y;
	//if (prop.height > 1500000)
	//	throw ns_ex("ns_movement_image_collage_info::Movement collage would require an enormous image:") << prop.width << "x" << prop.height;
	prop.components = 3;
//	prop.description = xml.result();
//		cerr << p.element_count() << ": " << frames_per_row << " " << grid_dimensions << " " << frame_dimensions << " " <<
//			prop.width << "x" << prop.height << "\n";
}

std::string ns_analyzed_image_time_path::volatile_storage_name(const unsigned long &rep_id,const bool flow) const {
	return std::string("path_") + ns_to_string(this->region_info_id) + "="
		+ ns_to_string(this->group_id.group_id) + "="
		+ ns_to_string(this->group_id.path_id) + "=" + (flow?"flow":"im") + "=" + ns_to_string(rep_id) + "=" + ns_to_string(unique_process_id) + ".tif";
}
void ns_analyzed_image_time_path::reset_movement_image_saving() {
	//if (this->group_id.group_id == 0 || group_id.group_id == 12)
	//cout << "Deleting group" << this->group_id.group_id << "\n";
	ns_safe_delete(output_reciever);
	ns_safe_delete(flow_output_reciever);
	save_flow_image_buffer.resize(ns_image_stream_buffer_properties(0, 0));
	save_image_buffer.resize(ns_image_stream_buffer_properties(0, 0));
}
void ns_analyzed_image_time_path::save_movement_images(const ns_analyzed_time_image_chunk & chunk, ns_sql & sql, const ns_images_to_save & images_to_save, const ns_backwards_image_handling & backwards_image_handling, const ns_output_location & output_location){
	const bool save_image(images_to_save == ns_save_simple || images_to_save == ns_save_both),
			   save_flow_image(images_to_save == ns_save_flow || images_to_save == ns_save_both);
	#ifndef NS_CALCULATE_OPTICAL_FLOW
	if (save_flow_image)
		throw ns_ex("Attempting to save flow movement images with NS_CALCULATE_OPTICAL_FLOW set to false");
	#endif
	//handle small or non-existant images
	unsigned long number_of_frames_to_write(0);
	if (backwards_image_handling == ns_only_output_backwards_images)
		number_of_frames_to_write = this->first_stationary_timepoint();  //this returns the index of the first stationary timepoint in the path element array, which is also the number of non-stationary
																		 //timepoints, which is the total number of time-points we want to write now.

	ns_movement_image_collage_info d(this, number_of_frames_to_write);
	bool first_write(output_reciever == 0 && flow_output_reciever == 0);
	bool had_to_use_volatile_storage;
	if (first_write) {
		if (output_location == ns_local_0 || output_location == ns_local_1) {
			int round_id = (output_location == ns_local_0) ? 0 : 1;
			//	cerr << "Allocating path" << this->group_id.group_id << " in volatile storage.\n";

			debug_number_images_written = 0;

			if (save_image)
				output_reciever = new ns_image_storage_reciever_handle<ns_8_bit>(image_server_const.image_storage.request_local_cache_storage(volatile_storage_name(round_id, false), ns_tiff_lzw, save_output_buffer_height, false));
			if (save_flow_image)
				flow_output_reciever = new ns_image_storage_reciever_handle<float>(image_server_const.image_storage.request_local_cache_storage_float(volatile_storage_name(round_id, true), ns_tiff_zip, save_output_buffer_height, false));
		}
		else {
			if (save_image) {
				if (output_image.filename.empty())
					throw ns_ex("Empty filename!");
				if (ns_fix_filename_suffix(output_image.filename, ns_tiff))
					output_image.save_to_db(output_image.id, &sql);
				if (output_image.filename == ".tif")
					throw ns_ex("Invalid filename");
				output_reciever = new ns_image_storage_reciever_handle<ns_8_bit>(image_server_const.image_storage.request_storage(output_image, ns_tiff_lzw, 1.0, save_output_buffer_height, &sql, had_to_use_volatile_storage, false, ns_image_storage_handler::ns_forbid_volatile));
			}
			if (save_flow_image) {
				if (ns_fix_filename_suffix(output_image.filename, ns_tiff_zip))
					output_image.save_to_db(output_image.id, &sql);
				flow_output_reciever = new ns_image_storage_reciever_handle<float>(image_server_const.image_storage.request_storage_float(flow_output_image, ns_tiff_zip, 1.0, save_output_buffer_height, &sql, had_to_use_volatile_storage, false, ns_image_storage_handler::ns_forbid_volatile));
			}
		}
	}
	//write out a dummy image if there are no frames in the path.
	if (d.prop.width == 0 || d.prop.height == 0) {
		if (!first_write) return;
		ns_image_properties p(d.prop);
		p.height = 1;
		p.width = 1;
		if (p.components == 0)
			p.components = 3;
		ns_image_standard im;
		im.init(p);
		for (unsigned int c = 0; c < p.components; c++)
			im[0][c] = 0;
		if (save_image)	im.pump(output_reciever->output_stream(), 1024);
		if (save_flow_image) im.pump(flow_output_reciever->output_stream(), 1024);
		
		return;
	}
	
	if (first_write) {
		//if (this->group_id.group_id == 0 || group_id.group_id == 12)
		//	cout << "Opening group " << this->group_id.group_id << " between " << chunk.start_i << " and " << chunk.stop_i << ", to write out " << number_of_frames_to_write << " images @ (" << this->path_context_size.x << "," << this->path_context_size.y << ")(" << d.prop.width << "," << d.prop.height << ")\n";

		if (save_image)	output_reciever->output_stream().init(d.prop);
		if (save_flow_image)	flow_output_reciever->output_stream().init(d.prop);
	}
	else {
		//if (this->group_id.group_id == 0 || group_id.group_id == 12)
		//	cout << "writing  " << this->group_id.group_id << " between " << chunk.start_i << " and " << chunk.stop_i << "\n";
	}
	//if (chunk.stop_i == -1 && group_id.group_id == 12)
	//	cerr << "HERE!";
	if (save_image) {
		debug_number_images_written++;
		save_movement_image(chunk, *output_reciever, backwards_image_handling == ns_only_output_backwards_images);
	}
	if (save_flow_image) save_movement_flow_image(chunk, *flow_output_reciever, backwards_image_handling == ns_only_output_backwards_images);

	if (!backwards_image_handling == ns_output_all_images && chunk.stop_i == elements.size() ||
		backwards_image_handling == ns_only_output_backwards_images && chunk.stop_i ==-1) {
	//	if (this->group_id.group_id == 0 || group_id.group_id == 12)
			//cout << "Closing group " << this->group_id.group_id << "\n";
		if (save_image) {
			output_reciever->output_stream().finish_recieving_image();
			ns_safe_delete(output_reciever);
		}

		if (save_flow_image) {
			flow_output_reciever->output_stream().finish_recieving_image();
			ns_safe_delete(flow_output_reciever);
		}
	}
}

void ns_analyzed_image_time_path::save_movement_image(const ns_analyzed_time_image_chunk & chunk, ns_image_storage_reciever_handle<ns_8_bit> & out, const bool only_write_backwards_frames) {

	if (elements.size() == 0) return;

	unsigned long number_of_frames_to_write(0);
	if (only_write_backwards_frames)
		number_of_frames_to_write = this->first_stationary_timepoint();
	ns_movement_image_collage_info d(this, number_of_frames_to_write);

	//this is the vertical range in the path image corresponding the requested chunk
	long start_y = d.frame_dimensions.y*(chunk.start_i / d.grid_dimensions.x);
	long stop_y = d.frame_dimensions.y*(chunk.stop_i / d.grid_dimensions.x);

	//the chunk is moving backward but we write it out forwards.
	if (only_write_backwards_frames) {
		start_y = d.frame_dimensions.y*((chunk.stop_i+1) / d.grid_dimensions.x);
		stop_y = d.frame_dimensions.y*((chunk.start_i+1) / d.grid_dimensions.x );
	}

	//this is the size of the buffer we need to allocate to store the current chunk of the entire path image
	ns_image_stream_buffer_properties p;
	const long chunk_height = abs(stop_y - start_y);
	p.height = chunk_height;
	//don't allocate more than we've set for the reciever buffer height
	if (p.height > save_output_buffer_height)
		p.height = save_output_buffer_height;

	const unsigned long h(p.height);
	p.width = d.prop.width*d.prop.components;
	save_image_buffer.resize(p);

	//cerr << start_y << "-" << stop_y << " by " << h << "\n";
	//we output the range of the entire path image in h sized chunks"
	for (unsigned long y = start_y; y < stop_y; y += h) {
		if (d.prop.height < y)
			throw ns_ex("Err!");
		unsigned long dh(h);
		if (dh + y > d.prop.height) dh = d.prop.height - y;
		if (dh + y > stop_y) dh = stop_y - y;
		//	cerr << y << "-> " << y + dh << "\n ";

		for (unsigned long dy = 0; dy < dh; dy++) {
			//cy is the current line of the output buffer we are writing
			const unsigned long cy(y + dy);
			//i_offset is the index of the first path element we are going to write on this line of the output_buffer
			const unsigned long i_offset = d.grid_dimensions.x*(cy / d.frame_dimensions.y);
			//y_im_offset is the y within the current path element that we want to write.
			const unsigned long y_im_offset = cy%d.frame_dimensions.y;

			for (unsigned long i = 0; i < d.grid_dimensions.x; i++) {
				//l_x is the position on the current line that we want to write the path element i+i_offset
				const unsigned long l_x(i*d.frame_dimensions.x);
				//r_x is one beyond the position on the current line that we want to write the path element i+i_offset
				const unsigned long r_x((i + 1)*d.frame_dimensions.x);
				//if we've outputted all the elements; fill in the rest black.
				if (i_offset + i >= elements.size()) {
					for (unsigned int x = 3 * l_x; x < 3 * r_x; x++) {
						save_image_buffer[dy][x] = 0;
					}

				}
				else {

					const ns_analyzed_image_time_path_element &e(elements[i + i_offset]);
					if (e.registered_images == 0)
						throw ns_ex("Registered images are not allocated!");
					if (e.excluded)
						for (unsigned int x = l_x; x < r_x; x++) {
							save_image_buffer[dy][3 * x] = 0;
							save_image_buffer[dy][3 * x + 1] = 0;
							save_image_buffer[dy][3 * x + 2] = 0;
						}
					else {
						for (unsigned int x = l_x; x < r_x; x++) {
							save_image_buffer[dy][3 * x] = abs(e.registered_images->movement_image_[y_im_offset][x - l_x]);  //unsigned value of movement difference.
							save_image_buffer[dy][3 * x + 1] = e.registered_images->image[y_im_offset][x - l_x];
							save_image_buffer[dy][3 * x + 2] = (((ns_8_bit)((e.registered_images->movement_image_[y_im_offset][x - l_x]<0) ? 1 : 0))) | //sign of movement difference
								(((ns_8_bit)(e.registered_images->get_worm_neighborhood_threshold(y_im_offset, x - l_x) ? 1 : 0)) << 3) |
								(((ns_8_bit)(e.registered_images->get_worm_threshold(y_im_offset, x - l_x) ? 1 : 0)) << 4) |
								(((ns_8_bit)(e.registered_images->get_region_threshold(y_im_offset, x - l_x) ? 1 : 0)) << 5) |
								(((ns_8_bit)(e.registered_images->get_stabilized_worm_neighborhood_threshold(y_im_offset, x - l_x) ? 1 : 0)) << 6);
						}
					}
				}
			}
		}
		//	cerr << y << "->" << dh << "\n";
		out.output_stream().recieve_lines(save_image_buffer, dh);
	}

}
void ns_analyzed_image_time_path::save_movement_flow_image(const ns_analyzed_time_image_chunk & chunk, ns_image_storage_reciever_handle<float> & flow_out,const bool only_write_backwards_frames){
	#ifdef NS_CALCULATE_OPTICAL_FLOW

	if (elements.size() == 0) return;

	unsigned long number_of_frames_to_write(0);
	if (only_write_backwards_frames)
		number_of_frames_to_write = this->first_stationary_timepoint();
	ns_movement_image_collage_info d(this, number_of_frames_to_write);

	//this is the range within the entire path image that we are currently writing out
	long start_y = d.frame_dimensions.y*(chunk.start_i / d.grid_dimensions.x);
	long stop_y = d.frame_dimensions.y*((chunk.stop_i - 1) / d.grid_dimensions.x + 1);

	//the chunk is moving backward but we write it out forwards.
	if (only_write_backwards_frames) {
		start_y = d.frame_dimensions.y*(chunk.stop_i + 1 / d.grid_dimensions.x);
		stop_y = d.frame_dimensions.y*((chunk.start_i) / d.grid_dimensions.x + 1);
	}

	//this is the size of the buffer we need to allocate to store the current chunk of the entire path image
	ns_image_stream_buffer_properties p;
	const long chunk_height = abs(stop_y - start_y);

	p.height = chunk_height;
	//don't allocate too much otherwise we might hit the memmory ceiling and/or stress out libtiff
	if (p.height > 512)
		p.height = 512;

	const unsigned long h(p.height);
	p.width = d.prop.width*d.prop.components;
	save_flow_image_buffer.resize(p);

	//cerr << start_y << "-" << stop_y << " by " << h << "\n";
	//we output the range of the entire path image in h sized chunks
	for (unsigned long y = start_y; y < stop_y; y+=h){
	//	cerr <<y << "-> " << y + h << " modified to ";
		
		unsigned long dh(h);	
		if (dh + y > d.prop.height) dh = d.prop.height-y;
		if (dh + y > stop_y) dh = stop_y - y;
	//	cerr << y << "-> " << y + dh << "\n ";

		for (unsigned long dy = 0; dy < dh; dy++){
			//cy is the current line of the output buffer we are writing
			const unsigned long cy(y+dy);
			//i_offset is the index of the first path element we are going to write on this line of the output_buffer
			const unsigned long i_offset = d.grid_dimensions.x*(cy/d.frame_dimensions.y);
			//y_im_offset is the y within the current path element that we want to write.
			const unsigned long y_im_offset = cy%d.frame_dimensions.y;

			for (unsigned long i = 0; i < d.grid_dimensions.x; i++){
				//l_x is the position on the current line that we want to write the path element i+i_offset
				const unsigned long l_x(i*d.frame_dimensions.x);
				//r_x is one beyond the position on the current line that we want to write the path element i+i_offset
				const unsigned long r_x((i+1)*d.frame_dimensions.x);
				//if we've outputted all the elements; fill in the rest black.
				if (i_offset+i >= elements.size()){
					for (unsigned int x = 3*l_x; x < 3*r_x; x++){
						save_flow_image_buffer[dy][x] = 0;
					}
	
				}
				else{

					const ns_analyzed_image_time_path_element &e(elements[i+i_offset]);	

					if (e.registered_images == 0)
						throw ns_ex("Registered images are not allocated!");
					if (e.excluded)
						for (unsigned int x = l_x; x < r_x; x++){
							save_flow_image_buffer[dy][3*x] = 0;
							save_flow_image_buffer[dy][3*x+1] = 0;
							save_flow_image_buffer[dy][3*x+2] = 0;
						}
					else{
						for (unsigned int x = l_x; x < r_x; x++){
							save_flow_image_buffer[dy][3 * x] = e.registered_images->image[y_im_offset][x - l_x]/(float)255;
							save_flow_image_buffer[dy][3*x+1] =  e.registered_images->flow_image_dx[y_im_offset][x-l_x];
							save_flow_image_buffer[dy][3*x+2] =  e.registered_images->flow_image_dy[y_im_offset][x-l_x];
						}
					}
				}
			}
		}
		flow_out.output_stream().recieve_lines(save_flow_image_buffer,dh);

	}
#else
	throw ns_ex("Coiuld not save flow image with NS_CALCULATE_OPTICAL_FLOW undefined");
#endif
}


void ns_analyzed_image_time_path::initialize_movement_image_loading_no_flow(ns_image_storage_source_handle<ns_8_bit> & in, const bool read_only_backwards_frames){
	if (elements.size() == 0) return;
	unsigned long number_of_frames_to_write(0);
	if (read_only_backwards_frames)
		number_of_frames_to_write = this->first_stationary_timepoint();
	movement_loading_collage_info.from_path(this, number_of_frames_to_write);


	if (in.input_stream().properties().width != movement_loading_collage_info.prop.width ||
		in.input_stream().properties().height != movement_loading_collage_info.prop.height ||
		in.input_stream().properties().components != movement_loading_collage_info.prop.components){
			throw ns_ex("ns_analyzed_image_time_path::load_movement_images_from_file()::Unexpected collage size: ")
				<< in.input_stream().properties().width << "x" << in.input_stream().properties().height << ":" << (long)in.input_stream().properties().components << "," << (long)in.input_stream().properties().resolution << " vs "
				<< movement_loading_collage_info.prop.width << "x" << movement_loading_collage_info.prop.height << ":" << movement_loading_collage_info.prop.components << "," << movement_loading_collage_info.prop.resolution << "\n";
	}
	
	ns_image_stream_buffer_properties p;
	p.height = 2048;
	if (p.height > movement_loading_collage_info.prop.height)
		p.height = movement_loading_collage_info.prop.height;
	p.width = movement_loading_collage_info.prop.width*movement_loading_collage_info.prop.components;
	movement_loading_buffer.resize(p);

}
void ns_analyzed_image_time_path::initialize_movement_image_loading(ns_image_storage_source_handle<ns_8_bit> & in,ns_image_storage_source_handle<float> & flow_in,const bool read_only_backwards_frames){
	if (elements.size() == 0) return;

	unsigned long number_of_frames_to_write(0);
	if (read_only_backwards_frames)
		number_of_frames_to_write = this->first_stationary_timepoint();
	movement_loading_collage_info.from_path(this,number_of_frames_to_write);


	if (in.input_stream().properties().width != movement_loading_collage_info.prop.width ||
		in.input_stream().properties().height != movement_loading_collage_info.prop.height ||
		in.input_stream().properties().components != movement_loading_collage_info.prop.components){
			throw ns_ex("ns_analyzed_image_time_path::load_movement_images_from_file()::Unexpected collage size: ")
				<< in.input_stream().properties().width << "x" << in.input_stream().properties().height << ":" << (long)in.input_stream().properties().components << "," << (long)in.input_stream().properties().resolution << " vs "
				<< movement_loading_collage_info.prop.width << "x" << movement_loading_collage_info.prop.height << ":" << movement_loading_collage_info.prop.components << "," << movement_loading_collage_info.prop.resolution << "\n";
	}
	
	if (flow_in.input_stream().properties().width != movement_loading_collage_info.prop.width ||
		flow_in.input_stream().properties().height != movement_loading_collage_info.prop.height ||
		flow_in.input_stream().properties().components != movement_loading_collage_info.prop.components){
			throw ns_ex("ns_analyzed_image_time_path::load_movement_images_from_file()::Unexpected flow collage size: ")
				<< flow_in.input_stream().properties().width << "x" << flow_in.input_stream().properties().height << ":" << (long)flow_in.input_stream().properties().components << "," << (long)flow_in.input_stream().properties().resolution << " vs "
				<< movement_loading_collage_info.prop.width << "x" << movement_loading_collage_info.prop.height << ":" << movement_loading_collage_info.prop.components << "," << movement_loading_collage_info.prop.resolution << "\n";
	}

	ns_image_stream_buffer_properties p;
	p.height = 2048;
	if (p.height > movement_loading_collage_info.prop.height)
		p.height = movement_loading_collage_info.prop.height;
	p.width = movement_loading_collage_info.prop.width*movement_loading_collage_info.prop.components;
	movement_loading_buffer.resize(p);
	flow_movement_loading_buffer.resize(p);

}
void ns_analyzed_image_time_path::end_movement_image_loading(){
		movement_loading_buffer.resize(ns_image_stream_buffer_properties(0,0));
		flow_movement_loading_buffer.resize(ns_image_stream_buffer_properties(0,0));
		ns_safe_delete(output_reciever);
		ns_safe_delete(flow_output_reciever);
}
template<class allocator_T>
void ns_analyzed_image_time_path::load_movement_images(const ns_analyzed_time_image_chunk & chunk,ns_image_storage_source_handle<ns_8_bit> & in,ns_image_storage_source_handle<float> & flow_in, ns_time_path_image_movement_analysis_memory_pool<allocator_T> & memory_pool_){
	
	ns_image_stream_buffer_properties p(movement_loading_buffer.properties());
	const unsigned long h(p.height);

	for (unsigned int i = chunk.start_i; i < chunk.stop_i; i++){
		ns_image_properties pr(in.input_stream().properties());
		set_path_alignment_image_dimensions(pr);
		elements[i].initialize_registered_images(pr,memory_pool_.registered_image_pool);
	}

	unsigned long start_y(movement_loading_collage_info.frame_dimensions.y*(chunk.start_i/movement_loading_collage_info.grid_dimensions.x));
	
	unsigned long stop_y(movement_loading_collage_info.frame_dimensions.y*(chunk.stop_i/movement_loading_collage_info.grid_dimensions.x));

	for (unsigned long y = start_y; y < stop_y; y+=h){
		
		unsigned long dh(h);	
		if(h + y > stop_y) dh = stop_y-y;
		in.input_stream().send_lines(movement_loading_buffer,dh, movement_image_storage_internal_state);
		#ifdef NS_CALCULATE_OPTICAL_FLOW
		flow_in.input_stream().send_lines(flow_movement_loading_buffer,dh);
		#endif
		for (unsigned long dy = 0; dy < dh; dy++){
			const unsigned long cy(y+dy);
			const unsigned long i_offset = movement_loading_collage_info.grid_dimensions.x*(cy/movement_loading_collage_info.frame_dimensions.y);
			const unsigned long y_im_offset = cy%movement_loading_collage_info.frame_dimensions.y;
			for (unsigned long i = 0; i < movement_loading_collage_info.grid_dimensions.x; i++){
				const unsigned long l_x(i*movement_loading_collage_info.frame_dimensions.x);
				const unsigned long r_x((i+1)*movement_loading_collage_info.frame_dimensions.x);
				//if we've outputted all the elements; fill in the rest black.
				if (i_offset+i >= chunk.stop_i)
					throw ns_ex("YIKES!");

				ns_analyzed_image_time_path_element &e(elements[i+i_offset]);	
	//			cerr << "" << i + i_offset << ",";
			
				for (unsigned int x = l_x; x < r_x; x++){
					//unsigned value is stored in channel 0.  the sign of the value is stored in the first bit of channel 3
					 e.registered_images->movement_image_[y_im_offset][x-l_x] = movement_loading_buffer[dy][3*x] * (((movement_loading_buffer[dy][3*x+2])&1)?-1:1);
					 e.registered_images->image[y_im_offset][x-l_x] = movement_loading_buffer[dy][3*x+1];
					 e.registered_images->set_thresholds(y_im_offset,x-l_x,
														((movement_loading_buffer[dy][3*x+2])&(((ns_8_bit)1)<<5))>0,
				
														((movement_loading_buffer[dy][3*x+2])&(((ns_8_bit)1)<<4))>0,
						
														((movement_loading_buffer[dy][3*x+2])&(((ns_8_bit)1)<<3))>0,

														((movement_loading_buffer[dy][3 * x + 2])&(((ns_8_bit)1) << 6))>0
						 );
					#ifdef NS_CALCULATE_OPTICAL_FLOW
					  e.registered_images->flow_image_dx[y_im_offset][x-l_x] = flow_movement_loading_buffer[dy][3*x+1];
					 e.registered_images->flow_image_dy[y_im_offset][x-l_x] =  flow_movement_loading_buffer[dy][3*x+2];
					 #endif
					}
			}
		}
	//	cerr << "(" << y << "->" << dh << ")\n";
	}
}
void ns_analyzed_image_time_path_element_measurements::zero(){
	interframe_time_scaled_movement_sum = 0;
	movement_sum = 0;
	movement_alternate_worm_sum = 0;
	total_foreground_area = 0;
	total_stabilized_area = 0;
	total_region_area = 0;
	total_intensity_within_region = 0;
	total_intensity_within_stabilized = 0;
	total_intensity_within_stabilized_denoised = 0;
	total_intensity_within_foreground = 0;
	total_intensity_in_previous_frame_scaled_to_current_frames_histogram = 0;
	total_alternate_worm_area = 0;
	total_intensity_within_alternate_worm = 0;

	change_in_total_foreground_intensity = 0;
	change_in_total_region_intensity = 0;
	change_in_total_stabilized_intensity_1x = 0;
	change_in_total_stabilized_intensity_2x = 0;
	change_in_total_stabilized_intensity_4x = 0;

	change_in_total_outside_stabilized_intensity_1x = 0;
	change_in_total_outside_stabilized_intensity_2x = 0;
	change_in_total_outside_stabilized_intensity_4x = 0;

	movement_score = 0;
	denoised_movement_score = 0; 
	spatial_averaged_movement_sum_cropped = 0;
	spatial_averaged_movement_sum_uncropped = 0;
	spatial_averaged_movement_score_cropped = 0;
	spatial_averaged_movement_score_uncropped = 0;
	registration_displacement = ns_vector_2d(0,0);
}
void ns_analyzed_image_time_path_element_measurements::square() {
	interframe_time_scaled_movement_sum = interframe_time_scaled_movement_sum* interframe_time_scaled_movement_sum;
	movement_sum = movement_sum*movement_sum;
	movement_alternate_worm_sum = movement_alternate_worm_sum*movement_alternate_worm_sum;
	total_foreground_area = total_foreground_area*total_foreground_area;
	total_stabilized_area = total_stabilized_area*total_stabilized_area;
	total_region_area = total_region_area*total_region_area;
	total_intensity_within_region = total_intensity_within_region*total_intensity_within_region;
	total_intensity_within_stabilized = total_intensity_within_stabilized*total_intensity_within_stabilized;
	total_intensity_within_stabilized_denoised = total_intensity_within_stabilized_denoised*total_intensity_within_stabilized_denoised;
	total_intensity_within_foreground = total_intensity_within_foreground*total_intensity_within_foreground;
	total_intensity_in_previous_frame_scaled_to_current_frames_histogram = total_intensity_in_previous_frame_scaled_to_current_frames_histogram*total_intensity_in_previous_frame_scaled_to_current_frames_histogram;
	total_alternate_worm_area = total_alternate_worm_area*total_alternate_worm_area;
	total_intensity_within_alternate_worm = total_intensity_within_alternate_worm*total_intensity_within_alternate_worm;

	change_in_total_foreground_intensity = change_in_total_foreground_intensity*change_in_total_foreground_intensity;
	change_in_total_region_intensity = change_in_total_region_intensity*change_in_total_region_intensity;
	change_in_total_stabilized_intensity_1x = change_in_total_stabilized_intensity_1x*change_in_total_stabilized_intensity_1x;
	change_in_total_stabilized_intensity_2x = change_in_total_stabilized_intensity_2x * change_in_total_stabilized_intensity_2x;
	change_in_total_stabilized_intensity_4x = change_in_total_stabilized_intensity_4x * change_in_total_stabilized_intensity_4x;

	change_in_total_outside_stabilized_intensity_1x = change_in_total_outside_stabilized_intensity_1x * change_in_total_outside_stabilized_intensity_1x;
	change_in_total_outside_stabilized_intensity_2x = change_in_total_outside_stabilized_intensity_2x * change_in_total_outside_stabilized_intensity_2x;
	change_in_total_outside_stabilized_intensity_4x = change_in_total_outside_stabilized_intensity_4x * change_in_total_outside_stabilized_intensity_4x;

	movement_score = movement_score*movement_score;
	denoised_movement_score = denoised_movement_score*denoised_movement_score;
	spatial_averaged_movement_sum_cropped = spatial_averaged_movement_sum_cropped*spatial_averaged_movement_sum_cropped;
	spatial_averaged_movement_sum_uncropped = spatial_averaged_movement_sum_uncropped * spatial_averaged_movement_sum_uncropped;
	spatial_averaged_movement_score_cropped = spatial_averaged_movement_score_cropped * spatial_averaged_movement_score_cropped;
	spatial_averaged_movement_score_uncropped = spatial_averaged_movement_score_uncropped * spatial_averaged_movement_score_uncropped;

	registration_displacement.x = registration_displacement.x*registration_displacement.x;
	registration_displacement.y = registration_displacement.y*registration_displacement.y;
}
void ns_analyzed_image_time_path_element_measurements::square_root() {
	interframe_time_scaled_movement_sum = sqrt(interframe_time_scaled_movement_sum);
	movement_sum = sqrt(movement_sum);
	movement_alternate_worm_sum = sqrt(movement_alternate_worm_sum);
	total_foreground_area =	sqrt(total_foreground_area);
	total_stabilized_area = sqrt(total_stabilized_area);
	total_region_area = sqrt(total_region_area);
	total_intensity_within_region = sqrt(total_intensity_within_region);
	total_intensity_within_stabilized = sqrt(total_intensity_within_stabilized);
	total_intensity_within_stabilized_denoised = sqrt(total_intensity_within_stabilized_denoised);
	total_intensity_within_foreground = sqrt(total_intensity_within_foreground);
	total_intensity_in_previous_frame_scaled_to_current_frames_histogram = sqrt(total_intensity_in_previous_frame_scaled_to_current_frames_histogram);
	total_alternate_worm_area = sqrt(total_alternate_worm_area);
	total_intensity_within_alternate_worm = sqrt(total_intensity_within_alternate_worm);

	change_in_total_foreground_intensity  = sqrt(change_in_total_foreground_intensity);
	change_in_total_region_intensity = sqrt(change_in_total_region_intensity);
	change_in_total_stabilized_intensity_1x = sqrt(change_in_total_stabilized_intensity_1x);
	change_in_total_stabilized_intensity_2x = sqrt(change_in_total_stabilized_intensity_2x);
	change_in_total_stabilized_intensity_4x = sqrt(change_in_total_stabilized_intensity_4x);
	
	change_in_total_outside_stabilized_intensity_1x = sqrt(change_in_total_outside_stabilized_intensity_1x);
	change_in_total_outside_stabilized_intensity_2x = sqrt(change_in_total_outside_stabilized_intensity_2x);
	change_in_total_outside_stabilized_intensity_4x = sqrt(change_in_total_outside_stabilized_intensity_4x);

	movement_score = sqrt(movement_score);
	denoised_movement_score = sqrt(denoised_movement_score);
	spatial_averaged_movement_sum_cropped = sqrt(spatial_averaged_movement_sum_cropped);
	spatial_averaged_movement_sum_uncropped = sqrt(spatial_averaged_movement_sum_uncropped);
	spatial_averaged_movement_score_cropped = sqrt(spatial_averaged_movement_score_cropped);
	spatial_averaged_movement_score_uncropped = sqrt(spatial_averaged_movement_score_uncropped);

	registration_displacement.x = sqrt(registration_displacement.x);
	registration_displacement.y = sqrt(registration_displacement.y);
}


ns_analyzed_image_time_path_element_measurements operator+(const ns_analyzed_image_time_path_element_measurements & a, const ns_analyzed_image_time_path_element_measurements & b){
	ns_analyzed_image_time_path_element_measurements ret;
	ret.interframe_time_scaled_movement_sum = a.interframe_time_scaled_movement_sum+b.interframe_time_scaled_movement_sum;
	ret.movement_sum = a.movement_sum+b.movement_sum;
	ret.movement_alternate_worm_sum = a.movement_alternate_worm_sum+b.movement_alternate_worm_sum;
	ret.total_foreground_area = a.total_foreground_area+b.total_foreground_area;
	ret.total_stabilized_area = a.total_stabilized_area+b.total_stabilized_area;
	ret.total_region_area = a.total_region_area+b.total_region_area;
	ret.total_intensity_within_region = a.total_intensity_within_region+b.total_intensity_within_region;
	ret.total_intensity_within_stabilized = a.total_intensity_within_stabilized+b.total_intensity_within_stabilized;
	ret.total_intensity_within_stabilized_denoised = a.total_intensity_within_stabilized_denoised+b.total_intensity_within_stabilized_denoised;
	ret.total_intensity_within_foreground = a.total_intensity_within_foreground+b.total_intensity_within_foreground;
	ret.total_intensity_in_previous_frame_scaled_to_current_frames_histogram = a.total_intensity_in_previous_frame_scaled_to_current_frames_histogram+b.total_intensity_in_previous_frame_scaled_to_current_frames_histogram;
	ret.total_alternate_worm_area = a.total_alternate_worm_area+b.total_alternate_worm_area;
	ret.total_intensity_within_alternate_worm = a.total_intensity_within_alternate_worm+b.total_intensity_within_alternate_worm;

	ret.change_in_total_foreground_intensity = a.change_in_total_foreground_intensity+b.change_in_total_foreground_intensity;
	ret.change_in_total_region_intensity = a.change_in_total_region_intensity+b.change_in_total_region_intensity;
	ret.change_in_total_stabilized_intensity_1x = a.change_in_total_stabilized_intensity_1x +b.change_in_total_stabilized_intensity_1x;
	ret.change_in_total_stabilized_intensity_2x = a.change_in_total_stabilized_intensity_2x + b.change_in_total_stabilized_intensity_2x;
	ret.change_in_total_stabilized_intensity_4x = a.change_in_total_stabilized_intensity_4x + b.change_in_total_stabilized_intensity_4x;

	ret.change_in_total_outside_stabilized_intensity_1x = a.change_in_total_outside_stabilized_intensity_1x + b.change_in_total_outside_stabilized_intensity_1x;
	ret.change_in_total_outside_stabilized_intensity_2x = a.change_in_total_outside_stabilized_intensity_2x + b.change_in_total_outside_stabilized_intensity_2x;
	ret.change_in_total_outside_stabilized_intensity_4x = a.change_in_total_outside_stabilized_intensity_4x + b.change_in_total_outside_stabilized_intensity_4x;

	ret.movement_score = a.movement_score+b.movement_score;
	ret.denoised_movement_score = a.denoised_movement_score+b.denoised_movement_score;
	ret.spatial_averaged_movement_sum_cropped = a.spatial_averaged_movement_sum_cropped+b.spatial_averaged_movement_sum_cropped;
	ret.spatial_averaged_movement_sum_uncropped = a.spatial_averaged_movement_sum_uncropped + b.spatial_averaged_movement_sum_uncropped;
	ret.spatial_averaged_movement_score_cropped = a.spatial_averaged_movement_score_cropped +b.spatial_averaged_movement_score_cropped;
	ret.spatial_averaged_movement_score_uncropped = a.spatial_averaged_movement_score_uncropped + b.spatial_averaged_movement_score_uncropped;

	ret.registration_displacement.x = a.registration_displacement.x+b.registration_displacement.x;
	ret.registration_displacement.y = a.registration_displacement.y+b.registration_displacement.y;
	return ret;
}
ns_analyzed_image_time_path_element_measurements operator-(const ns_analyzed_image_time_path_element_measurements & a, const ns_analyzed_image_time_path_element_measurements & b) {
	ns_analyzed_image_time_path_element_measurements ret;
	ret.interframe_time_scaled_movement_sum = a.interframe_time_scaled_movement_sum - b.interframe_time_scaled_movement_sum;
	ret.movement_sum = a.movement_sum - b.movement_sum;
	ret.movement_alternate_worm_sum = a.movement_alternate_worm_sum - b.movement_alternate_worm_sum;
	ret.total_foreground_area = a.total_foreground_area - b.total_foreground_area;
	ret.total_stabilized_area = a.total_stabilized_area - b.total_stabilized_area;
	ret.total_region_area = a.total_region_area - b.total_region_area;
	ret.total_intensity_within_region = a.total_intensity_within_region - b.total_intensity_within_region;
	ret.total_intensity_within_stabilized = a.total_intensity_within_stabilized - b.total_intensity_within_stabilized;
	ret.total_intensity_within_stabilized_denoised = a.total_intensity_within_stabilized_denoised-b.total_intensity_within_stabilized_denoised;
	ret.total_intensity_within_foreground = a.total_intensity_within_foreground - b.total_intensity_within_foreground;
	ret.total_intensity_in_previous_frame_scaled_to_current_frames_histogram = a.total_intensity_in_previous_frame_scaled_to_current_frames_histogram - b.total_intensity_in_previous_frame_scaled_to_current_frames_histogram;
	ret.total_alternate_worm_area = a.total_alternate_worm_area - b.total_alternate_worm_area;
	ret.total_intensity_within_alternate_worm = a.total_intensity_within_alternate_worm - b.total_intensity_within_alternate_worm;

	ret.change_in_total_foreground_intensity = a.change_in_total_foreground_intensity - b.change_in_total_foreground_intensity;
	ret.change_in_total_region_intensity = a.change_in_total_region_intensity - b.change_in_total_region_intensity;
	ret.change_in_total_stabilized_intensity_1x = a.change_in_total_stabilized_intensity_1x - b.change_in_total_stabilized_intensity_1x;
	ret.change_in_total_stabilized_intensity_2x = a.change_in_total_stabilized_intensity_2x - b.change_in_total_stabilized_intensity_2x;
	ret.change_in_total_stabilized_intensity_4x = a.change_in_total_stabilized_intensity_4x - b.change_in_total_stabilized_intensity_4x;

	ret.change_in_total_outside_stabilized_intensity_1x = a.change_in_total_outside_stabilized_intensity_1x - b.change_in_total_outside_stabilized_intensity_1x;
	ret.change_in_total_outside_stabilized_intensity_2x = a.change_in_total_outside_stabilized_intensity_2x - b.change_in_total_outside_stabilized_intensity_2x;
	ret.change_in_total_outside_stabilized_intensity_4x = a.change_in_total_outside_stabilized_intensity_4x - b.change_in_total_outside_stabilized_intensity_4x;

	ret.movement_score = a.movement_score - b.movement_score;
	ret.denoised_movement_score = a.denoised_movement_score - b.denoised_movement_score;
	ret.spatial_averaged_movement_sum_cropped = a.spatial_averaged_movement_sum_cropped - b.spatial_averaged_movement_sum_cropped;
	ret.spatial_averaged_movement_sum_uncropped = a.spatial_averaged_movement_sum_uncropped - b.spatial_averaged_movement_sum_uncropped;
	ret.spatial_averaged_movement_score_cropped = a.spatial_averaged_movement_score_cropped - b.spatial_averaged_movement_score_cropped;
	ret.spatial_averaged_movement_score_uncropped = a.spatial_averaged_movement_score_uncropped - b.spatial_averaged_movement_score_uncropped;

	ret.registration_displacement.x = a.registration_displacement.x - b.registration_displacement.x;
	ret.registration_displacement.y = a.registration_displacement.y - b.registration_displacement.y;
	return ret;
}

ns_analyzed_image_time_path_element_measurements operator/(const ns_analyzed_image_time_path_element_measurements & a, const int & d) {
	ns_analyzed_image_time_path_element_measurements ret;
	ret.interframe_time_scaled_movement_sum = a.interframe_time_scaled_movement_sum / d;
	ret.movement_sum = a.movement_sum / d;
	ret.movement_alternate_worm_sum = a.movement_alternate_worm_sum / d;
	ret.total_foreground_area = a.total_foreground_area / d;
	ret.total_stabilized_area = a.total_stabilized_area / d;
	ret.total_region_area = a.total_region_area / d;
	ret.total_intensity_within_region = a.total_intensity_within_region / d;
	ret.total_intensity_within_stabilized = a.total_intensity_within_stabilized / d;
	ret.total_intensity_within_stabilized_denoised = a.total_intensity_within_stabilized /d;
	ret.total_intensity_within_foreground = a.total_intensity_within_foreground / d;
	ret.total_intensity_in_previous_frame_scaled_to_current_frames_histogram = a.total_intensity_in_previous_frame_scaled_to_current_frames_histogram / d;
	ret.total_alternate_worm_area = a.total_alternate_worm_area / d;
	ret.total_intensity_within_alternate_worm = a.total_intensity_within_alternate_worm / d;
	ret.change_in_total_foreground_intensity = a.change_in_total_foreground_intensity / d;
	ret.change_in_total_region_intensity = a.change_in_total_region_intensity / d;
	ret.change_in_total_stabilized_intensity_1x = a.change_in_total_stabilized_intensity_1x / d;
	ret.change_in_total_stabilized_intensity_2x = a.change_in_total_stabilized_intensity_2x / d;
	ret.change_in_total_stabilized_intensity_4x = a.change_in_total_stabilized_intensity_4x / d;

	ret.change_in_total_outside_stabilized_intensity_1x = a.change_in_total_outside_stabilized_intensity_1x / d;
	ret.change_in_total_outside_stabilized_intensity_2x = a.change_in_total_outside_stabilized_intensity_2x / d;
	ret.change_in_total_outside_stabilized_intensity_4x = a.change_in_total_outside_stabilized_intensity_4x / d;

	ret.movement_score = a.movement_score / d;
	ret.denoised_movement_score = a.denoised_movement_score / d;
	ret.spatial_averaged_movement_sum_cropped = a.spatial_averaged_movement_sum_cropped / d;
	ret.spatial_averaged_movement_sum_uncropped = a.spatial_averaged_movement_sum_uncropped / d;
	ret.spatial_averaged_movement_score_cropped = a.spatial_averaged_movement_score_cropped / d;
	ret.spatial_averaged_movement_score_uncropped = a.spatial_averaged_movement_score_uncropped / d;
	ret.registration_displacement.x = a.registration_displacement.x / d;
	ret.registration_displacement.y = a.registration_displacement.y / d;
	return ret;
}




void ns_analyzed_image_time_path::calc_flow_images_from_registered_images(const ns_analyzed_time_image_chunk & chunk) {

	#ifdef NS_CALCULATE_OPTICAL_FLOW
	//flow image storage locations have already been initilaized by load_movement_images_no_flow()
	long step(chunk.forward() ? 1 : -1);
	const long dt(movement_time_kernel_width);
	const unsigned long movement_start_i(chunk.forward() ? (first_stationary_timepoint() + dt) :
		((first_stationary_timepoint() - dt)));
	if (flow == 0)
		flow = new ns_optical_flow_processor();
	for (unsigned int i = chunk.start_i; i < chunk.stop_i; i++) {
		bool first_frames(chunk.forward() ? (i < movement_start_i) : (i >= movement_start_i));
		if (first_frames) {
			flow->Dim1.from(elements[i].registered_images->image);
			flow->Dim2.from(elements[i].registered_images->image);
		}
		else {
		//	cerr << i - step*dt << "vs" << i << ":";
			flow->Dim1.from(elements[i - step*dt].registered_images->image);
			flow->Dim2.from(elements[i].registered_images->image);
			/*ns_image_standard im;
			ns_image_properties p(elements[i - step*dt].registered_images->image.properties());
			p.components = 3;
			im.init(p);
			for (unsigned int y = 0; y < p.height; y++)
				for (unsigned int x = 0; x < p.width; x++) {
					im[y][3 * x + 0] = elements[i - step*dt].registered_images->image[y][x];
					im[y][3 * x + 1] = elements[i].registered_images->image[y][x];
					im[y][3 * x + 2] = abs(elements[i - step*dt].registered_images->image[y][x] - elements[i].registered_images->image[y][x]);
				}
			ns_save_image("c:\\server\\debug2\\path" + ns_to_string(this->group_id) + "_" + ns_to_string(i) + ".tif",im);
			*/
			/*long diff(0), mmax(0), mmin(255);
			for (unsigned int y = 0; y < elements[i - step*dt].registered_images->image.properties().height; y++) {
				for (unsigned int x = 0; x < elements[i - step*dt].registered_images->image.properties().width; x++) {
					diff += abs(elements[i].registered_images->image[y][x] - elements[i - step*dt].registered_images->image[y][x]);
					if (elements[i].registered_images->image[y][x] < mmin)
						mmin = elements[i].registered_images->image[y][x];
					if (elements[i].registered_images->image[y][x] > mmax)
						mmax = elements[i].registered_images->image[y][x];
					if (elements[i - step*dt].registered_images->image[y][x] < mmin)
						mmin = elements[i - step*dt].registered_images->image[y][x];
					if (elements[i - step*dt].registered_images->image[y][x] > mmax)
						mmax = elements[i - step*dt].registered_images->image[y][x];
				}
			}
			cout << "d(" << diff << "," << mmin << "," << mmax << ")";*/

		}
		//XXX
		this->calculate_flow(i);
	}
	#else 
		throw ns_ex("Attempting to calculate flow with NS_CALCULATE_OPTICAL_FLOW set to false. (2)");
	#endif
}

template<class allocator_T>
void ns_analyzed_image_time_path::load_movement_images_no_flow(const ns_analyzed_time_image_chunk & chunk, ns_image_storage_source_handle<ns_8_bit> & in, ns_time_path_image_movement_analysis_memory_pool<allocator_T> & pool){
	
	ns_image_stream_buffer_properties p(movement_loading_buffer.properties());
	const unsigned long h(p.height);

	for (unsigned int i = chunk.start_i; i < chunk.stop_i; i++){
		ns_image_properties pr(in.input_stream().properties());
		set_path_alignment_image_dimensions(pr);
		elements[i].initialize_registered_images(pr,pool.registered_image_pool);
	}

	unsigned long start_y(movement_loading_collage_info.frame_dimensions.y*(chunk.start_i/movement_loading_collage_info.grid_dimensions.x));
	
	unsigned long stop_y(movement_loading_collage_info.frame_dimensions.y*(chunk.stop_i/movement_loading_collage_info.grid_dimensions.x));

	for (unsigned long y = start_y; y < stop_y; y+=h){
		
		unsigned long dh(h);	
		if(h + y > stop_y) dh = stop_y-y;
		in.input_stream().send_lines(movement_loading_buffer,dh, movement_image_storage_internal_state);
		///XXX test slower file servers
		//ns_thread::sleep_milliseconds(1000);

		for (unsigned long dy = 0; dy < dh; dy++){
			const unsigned long cy(y+dy);
			const unsigned long i_offset = movement_loading_collage_info.grid_dimensions.x*(cy/movement_loading_collage_info.frame_dimensions.y);
			const unsigned long y_im_offset = cy%movement_loading_collage_info.frame_dimensions.y;
			for (unsigned long i = 0; i < movement_loading_collage_info.grid_dimensions.x; i++){
				const unsigned long l_x(i*movement_loading_collage_info.frame_dimensions.x);
				const unsigned long r_x((i+1)*movement_loading_collage_info.frame_dimensions.x);
				//if we've outputted all the elements; fill in the rest black.
				if (i_offset+i >= chunk.stop_i)
					throw ns_ex("YIKES!");

				ns_analyzed_image_time_path_element &e(elements[i+i_offset]);	
	//			cerr << "" << i + i_offset << ",";
			
				for (unsigned int x = l_x; x < r_x; x++){
					//unsigned value is stored in channel 0.  the sign of the value is stored in the first bit of channel 3
					 e.registered_images->movement_image_[y_im_offset][x-l_x] = movement_loading_buffer[dy][3*x] * (((movement_loading_buffer[dy][3*x+2])&1)?-1:1);
					 e.registered_images->image[y_im_offset][x-l_x] = movement_loading_buffer[dy][3*x+1];
					 e.registered_images->set_thresholds(y_im_offset,x-l_x,
						((movement_loading_buffer[dy][3*x+2])&(((ns_8_bit)1)<<5))>0,
				
						 ((movement_loading_buffer[dy][3*x+2])&(((ns_8_bit)1)<<4))>0,
						
						((movement_loading_buffer[dy][3*x+2])&(((ns_8_bit)1)<<3))>0,
						((movement_loading_buffer[dy][3 * x + 2])&(((ns_8_bit)1) << 6))>0
						 );
					}
			}
		}
	//	cerr << "(" << y << "->" << dh << ")\n";
	}
}

template<class allocator_T>
bool ns_time_path_image_movement_analyzer<allocator_T>::load_movement_image_db_info(const ns_64_bit region_id,ns_sql & sql){
	if (image_db_info_loaded)
		return true;
	sql << "SELECT group_id,path_id,image_id, id,flow_image_id FROM path_data WHERE region_id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0){
			image_db_info_loaded = false;
			return false;
	}
	for (unsigned int i = 0; i < res.size(); i++){
		ns_64_bit group_id(ns_atoi64(res[i][0].c_str())),
					  path_id(ns_atoi64(res[i][1].c_str())),
					  image_id(ns_atoi64(res[i][2].c_str())),
					  db_id(ns_atoi64(res[i][3].c_str())),
						flow_image_id(ns_atoi64(res[i][4].c_str()));

		if (group_id >= groups.size() || (!groups[group_id].paths.empty() && path_id >= groups[group_id].paths.size())){
			sql << "DELETE from path_data WHERE id = " << db_id;
			sql.send_query();
			image_server_const.add_subtext_to_current_event(ns_image_server_event("ns_time_path_image_movement_analyzer::load_movement_image_db_info()::Deleting path data as a too-large group or path_id was found: (") << group_id << "," << path_id << ") in a region with " << groups.size() << " groups\n",&sql);
		}
		else{
			groups[group_id].paths[path_id].output_image.id = image_id;
			groups[group_id].paths[path_id].flow_output_image.id = flow_image_id;
			//if (image_id == 0)
			//	cerr << "Zero Image Id";
			//if (flow_image_id == 0) cerr << "zero flow image ID";
			groups[group_id].paths[path_id].path_db_id = db_id;
		}
		
	}
	image_db_info_loaded = true;
	return true;
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::stop_asynch_group_load() {
	if (asynch_group_loading_is_running) {
		cancel_asynch_group_load = true;
		while (asynch_group_loading_is_running)
			ns_thread::sleep_milliseconds(100);
	}
}

template<class allocator_T>
bool ns_time_path_image_movement_analyzer<allocator_T>::wait_until_element_is_loaded(const unsigned long group_id, const unsigned long element_id) {
	ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Waiting for element " << element_id << " in group " << group_id);
	if (groups.size() <= group_id)
		throw ns_ex("Invalid group");
	if (groups[group_id].paths[0].elements.size() <= element_id)
		throw ns_ex("Invalid element");
	if (groups[group_id].paths[0].elements[element_id].excluded)
		cout << "Warning: waiting on excluded sample";

	while (true) {
		//wait until next chunck is loaded
		ns_acquire_lock_for_scope lock(groups[group_id].paths[0].movement_image_storage_lock, __FILE__, __LINE__);
		if (groups[group_id].paths[0].number_of_images_loaded > element_id) {
			ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Successfully waited, as " << groups[group_id].paths[0].number_of_images_loaded << " images are now loaded.");
			lock.release();

			return true;
		}
		else {
			ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Checked in but only  " << groups[group_id].paths[0].number_of_images_loaded << " images are loaded.");
			
		}
		if (!asynch_group_loading_is_running) {
			if (asynch_group_loading_failed) {
				lock.release();
				return false;
			}
			else {
				throw ns_ex("Waiting for an unloaded element even after asynch loading has terminated.");
			}
		}
		lock.release(); 
		ns_thread::sleep_milliseconds(10);
	}

}

template<class allocator_T>
class ns_asynch_image_load_parameters {
public:
	unsigned long group_id;
	unsigned long number_of_images_to_load;
	bool load_images_after_last_valid_sample;
	bool load_flow_images;
	ns_simple_local_image_cache *image_cache;
	ns_sql * sql;
	ns_time_path_image_movement_analyzer< allocator_T> * analyzer;
};

template<class allocator_T>
ns_thread_return_type ns_time_path_image_movement_analyzer<allocator_T>::load_images_for_group_asynch_internal(void * pr) {
	ns_asynch_image_load_parameters<allocator_T>* p = static_cast<ns_asynch_image_load_parameters<allocator_T>*>(pr);
	try {
		p->analyzer->asynch_group_loading_failed = false;
		ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Asynch thread.");
		p->analyzer->load_images_for_group(p->group_id, p->number_of_images_to_load, *p->sql, p->load_images_after_last_valid_sample, p->load_flow_images, *p->image_cache);
		ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Asynch Loading finished.");
		p->analyzer->asynch_group_loading_is_running = false;
		delete p;
	}
	catch (ns_ex & ex) {
		cout << "Problem during asynchronous image loading: " << ex.text() << "\n";
		p->analyzer->asynch_group_loading_is_running = false;
		p->analyzer->asynch_group_loading_failed = true;
		delete p;
	}
	catch (...) {
		cout << "Unknown problem during asnych image load.\n";
		p->analyzer->asynch_group_loading_is_running = false;
		p->analyzer->asynch_group_loading_failed = true;
		delete p;
	}
	return 0;
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::load_images_for_group_asynch(const unsigned long group_id, unsigned long requested_number_of_images_to_load, ns_sql & sql, const bool load_images_after_last_valid_sample, const bool load_flow_images, ns_simple_local_image_cache & image_cache) {
	asynch_group_loading_is_running = true;
	ns_asynch_image_load_parameters<allocator_T> *p = new ns_asynch_image_load_parameters<allocator_T>;
	p->group_id = group_id;
	p->number_of_images_to_load = requested_number_of_images_to_load;
	p->sql = &sql;
	p->load_images_after_last_valid_sample = load_images_after_last_valid_sample;
	p->load_flow_images = load_flow_images;
	p->image_cache = &image_cache;
	p->analyzer = this;
	ns_thread thread;
	thread.run(load_images_for_group_asynch_internal, p);
	thread.detach();
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::load_images_for_group(const unsigned long group_id, unsigned long requested_number_of_images_to_load,ns_sql & sql, const bool load_images_after_last_valid_sample, const bool load_flow_images, ns_simple_local_image_cache & image_cache){

	ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Loading images for group.");
	if (cancel_asynch_group_load) {
		cancel_asynch_group_load = false;
		ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Asynch cancelled");
		return;
	}
	#ifdef NS_CALCULATE_OPTICAL_FLOW
	if (load_flow_images)
		throw ns_ex("Attempting to load flow images with NS_CALCULATE_OPTICAL_FLOW set to false");
	#endif
	if (groups[group_id].paths.size() == 0) {

		ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Empty paths.");
		return;
	}
	unsigned long number_of_images_loaded(groups[group_id].paths[0].number_of_images_loaded);
	if (number_of_images_loaded == 0){
		if (group_id >= size())
			throw ns_ex("ns_time_path_image_movement_analyzer::load_images_for_group()::Invalid group: ") << group_id;
	}
	if (number_of_images_loaded >= requested_number_of_images_to_load) {
		ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Leaving satisfied, having done nothing.");
		return;
	}
	ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Waiting to start on loading lock.");
	ns_try_to_acquire_lock_for_scope loading_lock(groups[group_id].paths[0].asynchronous_loading_lock);
	ns_acquire_lock_for_scope image_access_lock(groups[group_id].paths[0].movement_image_storage_lock, __FILE__, __LINE__,false);
	while (true) {
		const bool obtained_loading_lock(loading_lock.try_to_get(__FILE__, __LINE__));

		//check to see if another thread has loaded the images we requested. If so, we're done!
		image_access_lock.get(__FILE__, __LINE__);
		number_of_images_loaded = groups[group_id].paths[0].number_of_images_loaded;
		if (number_of_images_loaded >= requested_number_of_images_to_load) {
			ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Requested image, " << requested_number_of_images_to_load << ", already loaded.");
			image_access_lock.release();
			if (obtained_loading_lock)
				loading_lock.release();
			cancel_asynch_group_load = false;

			ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Leaving satisfied, having waited.");
			return;
		}
		//if there is another thread loading images, defer to it
		if (!obtained_loading_lock) {

			ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Waiting for requested image, " << requested_number_of_images_to_load << ".");
			image_access_lock.release();
			ns_thread::sleep_milliseconds(100); //avoid rapid polling
		}
		else break;
	}

	ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Starting to load requested image , " << requested_number_of_images_to_load);
	//now we have the asynchronous loading lock, guarenteeing we are the only thread loading images for this group
	//we also have the movement access image storage lock, guarenteeing we are the only thread accessing image data for this thread

	const unsigned long chunk_size(10);

	for (unsigned int j = 0; j < groups[group_id].paths.size(); j++){
		unsigned long number_of_valid_elements(0);
		if (load_images_after_last_valid_sample)
			number_of_valid_elements = groups[group_id].paths[j].elements.size();
		else{
			for (int k = (int)groups[group_id].paths[j].elements.size()-1; k >= number_of_images_loaded; k--){
				if (!groups[group_id].paths[j].elements[k].excluded){
					number_of_valid_elements = k+1;
					break;
				}
			}
		}

		//get this number again after lock is obtained, in case another thread has altered it.
		if (number_of_images_loaded == 0){
			if (groups[group_id].paths[j].output_image.id==0){
				throw ns_ex("ns_time_path_image_movement_analyzer::load_images_for_group()::Group has no stored image id specified");
			}
			//if (groups[groups_id].paths[j].output_image.filename.empty()){
			//groups[group_id].paths[j].output_image.load_from_db(groups[group_id].paths[j].output_image.id,&sql);
			//groups[group_id].paths[j].movement_image_storage = image_server_const.image_storage.request_from_storage(groups[group_id].paths[j].output_image,&sql);
			precache_group_images_locally<ns_simple_local_image_cache::handle_t>(group_id,j,0, sql,true);

			groups[group_id].paths[j].movement_image_storage.input_stream().reset();

			groups[group_id].paths[j].movement_image_storage_internal_state = groups[group_id].paths[j].movement_image_storage.input_stream().init_send();
			if (load_flow_images) {
				groups[group_id].paths[j].flow_movement_image_storage = image_server_const.image_storage.request_from_storage_n_bits<float>(groups[group_id].paths[j].flow_output_image, &sql, ns_image_storage_handler::ns_long_term_storage);
				groups[group_id].paths[j].flow_movement_image_storage_internal_state = groups[group_id].paths[j].flow_movement_image_storage.input_stream().init_send();

				groups[group_id].paths[j].initialize_movement_image_loading(groups[group_id].paths[j].movement_image_storage, groups[group_id].paths[j].flow_movement_image_storage,false);
			}
			else {
				groups[group_id].paths[j].initialize_movement_image_loading_no_flow(groups[group_id].paths[j].movement_image_storage,false);
			}

		}

		unsigned long number_of_images_to_load = requested_number_of_images_to_load;
		if (number_of_images_to_load > number_of_valid_elements)
			number_of_images_to_load = number_of_valid_elements;
			//throw ns_ex("ns_time_path_image_movement_analyzer::load_images_for_group()::Requesting to many images!");
		if (number_of_images_to_load == 0 || cancel_asynch_group_load) {
			image_access_lock.release();
			loading_lock.release();
			cancel_asynch_group_load = false;
			ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Asynch cancelled or no images needed. (number of valid elements = " << number_of_valid_elements << ")");
			return;
		}

		unsigned long number_of_new_images_to_load(number_of_images_to_load-number_of_images_loaded);
			
		//load in chunk by chunk
		for (unsigned int k = 0; k < number_of_new_images_to_load; k+=chunk_size){
			if (cancel_asynch_group_load) {
				cancel_asynch_group_load = false;
				ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Asynch cancelled");
				return;
			}
			if (k != 0)
				image_access_lock.get(__FILE__, __LINE__);
			ns_analyzed_time_image_chunk chunk(k,k+chunk_size);
			if (chunk.stop_i >= number_of_new_images_to_load)
				chunk.stop_i = number_of_new_images_to_load;
			chunk.start_i+=number_of_images_loaded;
			chunk.stop_i+=number_of_images_loaded;

			if (!load_flow_images)
				groups[group_id].paths[j].load_movement_images_no_flow(chunk, groups[group_id].paths[j].movement_image_storage,memory_pool);
			else
				groups[group_id].paths[j].load_movement_images(chunk,groups[group_id].paths[j].movement_image_storage,groups[group_id].paths[j].flow_movement_image_storage,memory_pool);
//			
			groups[group_id].paths[j].number_of_images_loaded += chunk.stop_i-chunk.start_i;
			ns_output_asynchronous_image_loading_debug(ns_text_stream_t() << "Loaded images " << chunk.start_i << "-" << chunk.stop_i);

			//don't release on last iteration
			if (k + chunk_size <= number_of_new_images_to_load) {
				image_access_lock.release();
				ns_thread::sleep_milliseconds(10);
			}

		}
		if (number_of_images_to_load == number_of_valid_elements){
			groups[group_id].paths[j].end_movement_image_loading();
			//groups[group_id].paths[j].movement_image_storage.input_stream().close();
			groups[group_id].paths[j].movement_image_storage.clear();
		}

		image_access_lock.release();
		loading_lock.release();
	
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::clear_images_for_group(const unsigned long group_id, ns_simple_local_image_cache & image_cache){
	if (groups.size() <= group_id)
		return;
	for (unsigned int j = 0; j < groups[group_id].paths.size(); j++){
		for (unsigned int k = 0; k < groups[group_id].paths[j].elements.size(); k++){
			groups[group_id].paths[j].elements[k].clear_movement_images(memory_pool.registered_image_pool);
		}
		groups[group_id].paths[j].number_of_images_loaded = 0;
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::back_port_by_hand_annotations_to_solution_elements(ns_time_path_solution & sol) {
	for (unsigned int i = 0; i < groups.size(); i++) {
		for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
			if (i >= sol.path_groups.size() || j >= sol.path_groups[i].path_ids.size())
				throw ns_ex("Image time path and time path solution mismatch!");
			ns_time_path & path(sol.paths[sol.path_groups[i].path_ids[j]]);
			for (unsigned int k = 0; k < path.stationary_elements.size(); k++)
				sol.timepoints[path.stationary_elements[k].t_id].elements[path.stationary_elements[k].index].volatile_by_hand_annotated_properties = groups[i].paths[j].censoring_and_flag_details;
		}
	}

}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::reanalyze_stored_aligned_images(const ns_64_bit region_id,const ns_time_path_solution & solution_,const ns_time_series_denoising_parameters & times_series_denoising_parameters,const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql,const bool load_images_after_last_valid_sample,const bool recalculate_flow_images){

	if (e->software_version_number() != NS_CURRENT_POSTURE_MODEL_VERSION)
		throw ns_ex("This software, which is running threshold posture analysis version ") << NS_CURRENT_POSTURE_MODEL_VERSION << ", cannot use the incompatible posture analysis parameter set " << e->name << ", which is version " << e->software_version_number();

	#ifndef NS_CALCULATE_OPTICAL_FLOW
	if (recalculate_flow_images)
		throw ns_ex("Attempting to reanalyze flow images with NS_CALCULATE_OPTICAL_FLOW set to false");
	#endif
	const unsigned long chunk_size(10);
	try {
		region_info_id = region_id;

		externally_specified_plate_observation_interval = get_externally_specified_last_measurement(region_id, sql);
		load_from_solution(solution_);
		crop_path_observation_times(externally_specified_plate_observation_interval);

		if (!load_movement_image_db_info(region_id, sql))
			throw ns_ex("Could not find pre-calculated movement analysis for this region");
#ifdef NS_CALCULATE_OPTICAL_FLOW
		unsigned long number_flow_uncalculated(0);
		for (unsigned int i = 0; i < groups.size(); i++)
			for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
					continue;
				if (groups[i].paths[j].output_image.id == 0)
					throw ns_ex("Could not find movement image in db for region ") << region_id << " group " << i << " path " << j;
				if (groups[i].paths[j].flow_output_image.id == 0)
					number_flow_uncalculated++;
			}
		const bool calculate_flow_images(number_flow_uncalculated > 0);
		image_server_const.add_subtext_to_current_eventns_image_server_event("Some new movement quantification analyses is missing. It will be performed.", true), &sql);

		if (calculate_flow_images)
			get_output_image_storage_locations(region_id, sql, true);
#else
		typedef enum { ns_ignore, ns_use_existing, ns_create } ns_flow_image_handling;
		const ns_flow_image_handling flow_handling_approach(ns_ignore);
		#endif

		calculate_memory_pool_maximum_image_size(0,groups.size());

		//we need to load in the movement quantification file because it holds the registration offsets used to generate
		//the registered movement images, and these values need to be written to the new movement quantification file.

		//this will set the analysis id based on the file contents
		load_stored_movement_analysis_results(sql, ns_only_quantification);
		posture_model_version_used = NS_CURRENT_POSTURE_MODEL_VERSION;
		ns_64_bit file_specified_analysis_id = this->analysis_id;
		obtain_analysis_id_and_save_movement_data(region_id, sql, ns_require_existing_record, ns_do_not_write_data);
		if (file_specified_analysis_id != this->analysis_id)
			throw ns_ex("Movement analysis ID specified on disk (") << file_specified_analysis_id << ") does not agree with the ID  specified in database (" << this->analysis_id << ")";
		if (analysis_id == 0)
			throw ns_ex("Could not obtain analysis id!");
		std::vector<ns_64_bit> tmp1, tmp2;
		 long last_r(-5);
		for (unsigned int i = 0; i < groups.size(); i++){
			 long cur_r = (100 * i) / groups.size();
			if (cur_r - last_r >= 5) {
				image_server_const.add_subtext_to_current_event(ns_to_string(cur_r) + "%...", &sql);
				last_r = cur_r;
			}
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
					continue;
				groups[i].paths[j].output_image.load_from_db(groups[i].paths[j].output_image.id,&sql);
				ns_image_storage_source_handle<ns_8_bit> storage(image_server_const.image_storage.request_from_storage(groups[i].paths[j].output_image,&sql));
				
				ns_image_storage_source_handle<float> flow_storage(0);
				if (flow_handling_approach  == ns_ignore || flow_handling_approach == ns_create) {
							groups[i].paths[j].initialize_movement_image_loading_no_flow(storage,false);
				}
				else {
					flow_storage = image_server_const.image_storage.request_from_storage_n_bits<float>(groups[i].paths[j].flow_output_image, &sql, ns_image_storage_handler::ns_long_term_storage);
					groups[i].paths[j].initialize_movement_image_loading(storage, flow_storage,false);
				}

				ns_image_whole<unsigned long> temp_storage;
				temp_storage.use_more_memory_to_avoid_reallocations();
				unsigned long number_of_valid_elements(0);
				if (load_images_after_last_valid_sample)
					number_of_valid_elements = groups[i].paths[j].elements.size();
				else{
					for (int k = (int)groups[i].paths[j].elements.size()-1; k >= 0; k--){
						if (!groups[i].paths[j].elements[k].excluded){
							number_of_valid_elements = k+1;
							break;
						}
					}
				}


				//load in chunk by chunk
				//for (unsigned int k = 0; k < number_of_valid_elements; k+=chunk_size){

					ns_analyzed_time_image_chunk chunk(0,number_of_valid_elements);
					
					if (flow_handling_approach  == ns_ignore || flow_handling_approach == ns_create) {
						groups[i].paths[j].load_movement_images_no_flow(chunk, storage,memory_pool);
						if (flow_handling_approach == ns_create) {
							groups[i].paths[j].calc_flow_images_from_registered_images(chunk);
							groups[i].paths[j].save_movement_images(chunk, sql, ns_analyzed_image_time_path::ns_save_flow, ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_long_term);
						}
					}
					else {
						groups[i].paths[j].load_movement_images(chunk, storage,flow_storage,memory_pool);
					}
					//groups[i].paths[j].calculate_stabilized_worm_neighborhood(temp_storage);
					//groups[i].paths[j].save_movement_images(chunk, sql, ns_analyzed_image_time_path::ns_save_simple, ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_long_term);

					groups[i].paths[j].quantify_movement(chunk);
					/*unsigned long start(chunk.start_i);
					
					long stop(chunk.stop_i - ns_analyzed_image_time_path::movement_time_kernel_width);
					if (stop < 0)
						stop = 0;
					if (chunk.stop_i == number_of_valid_elements)
						stop = chunk.stop_i;
					for (long l = 0; l < stop; l++)
						groups[i].paths[j].elements[l].clear_movement_images();*/
				//}
				clear_images_for_group(i, image_cache);
				groups[i].paths[j].end_movement_image_loading();
				groups[i].paths[j].denoise_movement_series_and_calculate_intensity_slopes(times_series_denoising_parameters, tmp1, tmp2);
			
				//groups[i].paths[j].analyze_movement(e,generate_stationary_path_id(i,j));
				
				//groups[i].paths[j].calculate_movement_quantification_summary();
			//	debug_name += ".csv";
			//	ofstream tmp(debug_name.c_str());
			//	groups[i].paths[j].output_image_movement_summary(tmp);
			}
		}
		normalize_movement_scores_over_all_paths(e->software_version_number(),times_series_denoising_parameters,sql);
		//ofstream o("c:\\server\\debug_" + ns_format_time_string(ns_current_time()));
		//groups[0].paths[0].write_detailed_movement_quantification_analysis_header(o);
		//o << "\n";
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
					continue;
				groups[i].paths[j].analyze_movement(e,generate_stationary_path_id(i,j),last_timepoint_in_analysis_);
			//	ns_region_metadata m;
			//	if (i == 17)
			//		groups[i].paths[j].write_detailed_movement_quantification_analysis_data(m, i, j, o, false, false);

				groups[i].paths[j].calculate_movement_quantification_summary(groups[i].paths[j].movement_analysis_result);
			}
		}
		//o.close();
		memory_pool.clear();
		generate_movement_description_series();

		image_server_const.add_subtext_to_current_event("\nDone.\n", &sql);
	}
	catch(...){
		//delete_from_db(region_id,sql);
		throw;
	}
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::mark_path_images_as_cached_in_db(const ns_64_bit region_id, ns_sql & sql){
	sql << "UPDATE sample_region_image_info SET path_movement_images_are_cached = 1 WHERE id = " << region_id;
	sql.send_query();
}
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::acquire_region_image_specifications(const ns_64_bit region_id,ns_sql & sql){
	const string col(ns_processing_step_db_column_name(ns_process_region_vis));
	const string col2(ns_processing_step_db_column_name(ns_process_region_interpolation_vis));
	sql << "SELECT " << col << "," << col2 << ",capture_time, worm_detection_results_id, worm_interpolation_results_id, id "
		   "FROM sample_region_images WHERE region_info_id = " << region_id << " AND (" << col << "!= 0 || " << col2 << "!=0) ORDER BY capture_time ASC";
	ns_sql_result res;
	sql.get_rows(res);
	region_image_specifications.resize(res.size());
	for (unsigned long i = 0; i < res.size(); i++){
		//load image
		region_image_specifications[i].time = atol(res[i][2].c_str());
		region_image_specifications[i].sample_region_image_id = ns_atoi64(res[i][5].c_str());
		region_image_specifications[i].region_vis_image.load_from_db(ns_atoi64(res[i][0].c_str()),&sql);
		ns_64_bit id(ns_atoi64(res[i][1].c_str()));
		if (id != 0)
			region_image_specifications[i].interpolated_region_vis_image.load_from_db(id,&sql);
		else region_image_specifications[i].interpolated_region_vis_image.id = 0;
	}
	//make sure we only use time point data specified in the solution
	std::vector<ns_analyzed_image_specification>::iterator p = region_image_specifications.begin();
	for (unsigned int i = 0; i < solution->timepoints.size();i++){
		if (p == region_image_specifications.end() || solution->timepoints[i].time < p->time)
			throw ns_ex("ns_time_path_image_movement_analyzer::acquire_region_image_specifications()::Could not identify all region images specified in the time path solution");
		while(p->time < solution->timepoints[i].time)
			p = region_image_specifications.erase(p);
		if (p->time == solution->timepoints[i].time)
			p++;
	}
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::normalize_movement_scores_over_all_paths(const std::string & software_version, const ns_time_series_denoising_parameters & param, ns_sql & sql){
	if (param.movement_score_normalization == ns_time_series_denoising_parameters::ns_none)
		return;
	image_server.register_server_event(ns_image_server_event("Ignoring depreciated denosing parameter specification!!"),&sql);
	return;
	/*
	if (param.movement_score_normalization == ns_time_series_denoising_parameters::ns_subtract_out_device_median)
		return;

	denoising_parameters_used = param;

	std::vector<double> medians;
	std::vector<char> excluded_from_plate_measurements;
	unsigned long total_measurements(0);
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
						continue;

			ns_analyzed_image_time_path::ns_element_list & elements(groups[i].paths[j].elements);
			
						
			//find median of last 15% of curve
			unsigned long start;
			if (param.movement_score_normalization == ns_time_series_denoising_parameters::ns_subtract_out_median_of_end ||
				param.movement_score_normalization == ns_time_series_denoising_parameters::ns_subtract_out_plate_median)
				start = (17*elements.size())/20;
			else if (param.movement_score_normalization == ns_time_series_denoising_parameters::ns_subtract_out_median)
				start = 0;
			else throw ns_ex("Unknown movement score normalization scheme:") << (long)(param.movement_score_normalization);
			if (elements.size()-start < 2)
				start = 0;

			std::vector<double> values(elements.size()-start);
			for (unsigned int k = start; k < elements.size(); k++){
				if (software_version=="1")
				values[k-start] = elements[k].measurements.death_time_posture_analysis_measure_v1();
				else
					values[k - start] = elements[k].measurements.death_time_posture_analysis_measure_v2_cropped();
			}

			if (elements.size() < 10 || groups[i].paths[j].excluded())
				excluded_from_plate_measurements.push_back(1);
			else excluded_from_plate_measurements.push_back(0);

			std::sort(values.begin(),values.end());
			if (values.size()%2 == 1)
				medians.push_back(values[values.size()/2]);
			else
				medians.push_back((values[values.size()/2]+values[values.size()/2-1])/2.0);
		}
	}
	if (param.movement_score_normalization == ns_time_series_denoising_parameters::ns_subtract_out_plate_median){
		//weighted median of all measurements made of all animals detected on the plate.
		std::vector<double> medians_to_use;
		for (unsigned int i = 0; i < medians.size(); i++){
			if (!excluded_from_plate_measurements[i])
				medians_to_use.push_back(medians[i]);
		}
		std::sort(medians_to_use.begin(),medians_to_use.end());
		double median;
		if (medians_to_use.empty())
			median = 0;
		else{
			if (medians_to_use.size()%2 == 1)
				median = medians_to_use[medians_to_use.size()/2];
			else
				median = (medians_to_use[medians_to_use.size()/2]+medians_to_use[medians_to_use.size()/2-1])/2.0;
		}
		const bool v1(software_version == "1");
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
						continue; 	

				for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++)
					(v1 ?groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v1(): groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v2_cropped()) -=median;
			}
		}
	}
	else{
		//weighted median of all samples of that type
		unsigned long m_pos(0);
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
						continue;
				for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++)
					(software_version == "1" ? groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v1() : groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v2_cropped()) -=medians[m_pos];
				m_pos++;
			}
		}
	}*/
}


template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::match_plat_areas_to_paths(std::vector<ns_region_area> & areas) {
	for (unsigned int i = 0; i < areas.size(); i++) {
		areas[i].worm_id = 0;
		areas[i].movement_state = ns_movement_fast;
		areas[i].clear_stats();
		areas[i].plate_subregion_info = ns_plate_subregion_info();
		areas[i].overlap_area_with_match = 0;
	}
	ns_64_bit average_path_duration(0), path_count(0);


	for (unsigned int g = 0; g < groups.size(); g++) {
		for (unsigned int p = 0; p < groups[g].paths.size(); p++) {
			const ns_64_bit path_duration = groups[g].paths[p].elements.rbegin()->absolute_time - groups[g].paths[p].elements[0].absolute_time;
			average_path_duration += path_duration;
			path_count++;

			for (unsigned int i = 0; i < areas.size(); i++) {
				const ns_vector_2i overlap_area = ns_rectangle_overlap_area(areas[i].pos, areas[i].pos + areas[i].size,
					groups[g].paths[p].path_region_position, groups[g].paths[p].path_region_position + groups[g].paths[p].path_region_size);
				unsigned long oa = overlap_area.x*overlap_area.y;
				if (oa == 0)
					continue;
				if (oa > areas[i].overlap_area_with_match) {
					areas[i].overlap_area_with_match = oa;
					areas[i].worm_id = g + 1;

					if (groups[g].paths[p].excluded() || groups[g].paths[p].is_low_density_path())
						areas[i].total_exclusion_time_in_seconds = path_duration;
					else areas[i].total_inclusion_time_in_seconds = path_duration;

					if (areas[i].time < groups[g].paths[p].elements[0].absolute_time)
						areas[i].movement_state = ns_movement_fast;
					else if (areas[i].time > groups[g].paths[p].elements.rbegin()->absolute_time)
						areas[i].movement_state = ns_movement_stationary;
					else {
						areas[i].movement_state = groups[g].paths[p].best_guess_movement_state(areas[i].time);
					}
					//			cout << "Matched " << areas[i].worm_id << ": " << ns_movement_state_to_string_short(areas[i].movement_state) << "\n";
				}
			}
		}
	}
	average_path_duration /= path_count;
	//ns_time_path_image_movement_analyzer a;
	for (unsigned int i = 0; i < areas.size(); i++)
		areas[i].average_annotation_time_for_region = average_path_duration;
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::generate_movement_description_series(){
	//group sizes and position
	description_series.items.resize(0);
	description_series.items.resize(groups.size());
	for (unsigned int i = 0; i < groups.size(); i++){
		description_series.items[i].path_id.group_id = i;
		description_series.items[i].path_id.path_id = 0;
		if (groups[i].paths.size()==0)
			continue;
		ns_image_properties prop; 
		groups[i].paths[0].set_path_alignment_image_dimensions(prop);
		description_series.items[i].final_image_size.x = prop.width;
		description_series.items[i].final_image_size.y = prop.height;
		description_series.items[i].sticky_properties = groups[i].paths[0].censoring_and_flag_details;
		description_series.items[i].animal_death_was_observed = groups[i].paths[0].movement_analysis_result.animal_died();
		description_series.items[i].animal_alive_at_end_of_observation = !description_series.items[i].animal_death_was_observed &&
			groups[i].paths[0].observation_limits().last_obsevation_of_plate.period_start == groups[i].paths[0].observation_limits().interval_after_last_observation.period_start;
	}


	//populate timepoints from solution
	description_series.timepoints.resize(0);
	description_series.timepoints.resize(solution->timepoints.size());
	for (unsigned int i = 0; i < solution->timepoints.size(); i++){
		description_series.timepoints[i].time = solution->timepoints[i].time;
		description_series.timepoints[i].worms.reserve(10);
	}
	//add fast moving worms
	for (unsigned int i = 0; i < solution->unassigned_points.stationary_elements.size(); i++){
		description_series.timepoints[solution->unassigned_points.stationary_elements[i].t_id].worms.push_back(
			ns_worm_movement_measurement_description(
				0,
				ns_movement_fast,
				solution->element(solution->unassigned_points.stationary_elements[i]).region_position,
				solution->element(solution->unassigned_points.stationary_elements[i]).region_size,
				solution->element(solution->unassigned_points.stationary_elements[i]).context_image_position,
				solution->element(solution->unassigned_points.stationary_elements[i]).context_image_size,
				ns_stationary_path_id(-1, -1, 0)
			));
	}
	//we go through all paths, assigning path elements to the currect time point in one pass.
	//We keep track of where we are in each path by including the current
	//element id in group_path_current_element_id[].

	for (unsigned int t = 0; t < description_series.timepoints.size(); t++){
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				//see if this path is defined at the provided time point
				const ns_movement_state s(groups[i].paths[j].explicitly_recognized_movement_state(description_series.timepoints[t].time));
				if (s == ns_movement_not_calculated) continue;

				//find the position of the worm at time tt.
				long tt;
				for (tt = 0; tt < (long)groups[i].paths[j].elements.size()-1; tt++){
					if (groups[i].paths[j].elements[tt+1].absolute_time > description_series.timepoints[t].time)
						break;
				}
				
				description_series.timepoints[t].worms.push_back(
					ns_worm_movement_measurement_description(
					0,
					s,
					groups[i].paths[j].elements[tt].region_offset_in_source_image(),
					groups[i].paths[j].elements[tt].worm_region_size(),
					groups[i].paths[j].elements[tt].context_offset_in_source_image(),
					groups[i].paths[j].elements[tt].worm_context_size(),

					ns_stationary_path_id(i, j, 0)
					));
			}
		}
	}
}


ns_analyzed_image_time_path_event_index ns_analyzed_image_time_path::find_event_index(const ns_movement_event & event_to_align){
	ns_movement_state_observation_boundary_interval & state_interval(movement_analysis_result.state_intervals[(int)ns_movement_event_state(event_to_align)]);
	if (state_interval.skipped){
		return ns_analyzed_image_time_path_event_index(ns_no_movement_event,ns_movement_state_time_interval_indicies());
	}
	return ns_analyzed_image_time_path_event_index(event_to_align, state_interval.entrance_interval);
}

ns_analyzed_image_time_path_event_index ns_analyzed_image_time_path::find_event_index_with_fallback(const ns_movement_event & event_to_align){
	ns_analyzed_image_time_path_event_index ret(find_event_index(event_to_align));
	//if we've found a result, or can't fallback, return the result
	if (event_to_align ==  ns_fast_movement_cessation || ret.event_type != ns_no_movement_event) return ret;

	//if we don't have the result, fall back one step
	if (event_to_align == ns_translation_cessation)
		return find_event_index(ns_fast_movement_cessation);
	return find_event_index_with_fallback(ns_translation_cessation);
};


template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::generate_death_aligned_movement_posture_visualizations(const bool include_motion_graphs,const ns_64_bit region_id,const ns_movement_event & event_to_align,const ns_time_path_solution & solution,ns_sql & sql){
	/*
	ns_64_bit sample_id(0),experiment_id(0);
	string region_name,sample_name,experiment_name;

	ns_region_info_lookup::get_region_info(region_id,&sql,region_name,sample_name,sample_id,experiment_name,experiment_id);
	
	const long thickness(2);

	ns_marker_manager marker_manager;
	
	const ns_vector_2i graph_dimensions(include_motion_graphs?ns_vector_2i(300,200):ns_vector_2i(0,0));
	ns_worm_movement_description_series series = description_series;	

	series.calculate_visualization_grid(graph_dimensions);
	if (series.items.size() != groups.size())
		throw ns_ex("calculate_visualization_grid() returned an inconsistant result");

	//find time for which each animal needs to be aligned
	vector<ns_analyzed_image_time_path_event_index> path_aligned_event_index(groups.size());
	
	unsigned long alignment_position(0);
	unsigned long aligned_size(0);

	for (unsigned int i = 0; i < groups.size(); i++){
		if (!series.items[i].should_be_displayed)
			path_aligned_event_index[i] = ns_analyzed_image_time_path_event_index(ns_no_movement_event,ns_movement_state_time_interval_indicies(0,0));
		else{
			if (groups[i].paths[0].elements.size() == 0)
				throw ns_ex("ns_time_path_image_movement_analyzer::generate_death_aligned_movement_posture_visualizations()::"
							"Empty path found!");

			path_aligned_event_index[i] = groups[i].paths[0].find_event_index_with_fallback(event_to_align);
			
			//if worms never slow down, align the path to it's final point.
			if (path_aligned_event_index[i].event_type == ns_no_movement_event){
				path_aligned_event_index[i].index.period_end_index = groups[i].paths[0].elements.size()-1;
				path_aligned_event_index[i].event_type = ns_fast_moving_worm_observed;
			}

			if (path_aligned_event_index[i].index.period_end_index > alignment_position)
				alignment_position = path_aligned_event_index[i].index.period_end_index;
		}
	
	}
	//find the last time point in the aligned image series
	for (unsigned int i = 0; i < groups.size(); i++){
		long lp(groups[i].paths[0].elements.size()+alignment_position - path_aligned_event_index[i].index.period_end_index);
		if (lp > aligned_size)
			aligned_size = lp;
	}
	//we now have aligned the different groups.
	//all animals die at index alignment_position
	//and there are latest_aligned_index frames in the video.
	
	vector<ns_image_storage_source_handle<ns_8_bit> > path_image_source;
	vector<ns_graph> path_movement_graphs(groups.size());
	path_image_source.reserve(groups.size());

	ns_image_standard output_image;
	ns_image_properties prop;
	prop.components = 3;
	prop.width = series.visualization_grid_dimensions.x;
	prop.height = series.visualization_grid_dimensions.y;
	output_image.init(prop);

	
	ns_image_properties graph_prop(prop);
	ns_image_standard graph_temp;
	if (include_motion_graphs){
		graph_prop.width = series.metadata_dimensions.x;
		graph_prop.height = series.metadata_dimensions.y;
		graph_temp.init(graph_prop);
	}
	else
		graph_prop.width = graph_prop.height = 0;

	//initialize path movement image source for first frame
	for (unsigned int i = 0; i < groups.size(); i++){
		if (!series.items[i].should_be_displayed){
			path_image_source.push_back(ns_image_storage_source_handle<ns_8_bit>(0)); //we want the index of groups[i] and path_image_source[i] to match so we fill unwanted groups with a dummy entry.
			continue;
		}
		groups[i].paths[0].output_image.load_from_db(groups[i].paths[0].output_image.id,&sql);
		path_image_source.push_back(image_server_const.image_storage.request_from_storage(groups[i].paths[0].output_image,&sql));
		groups[i].paths[0].initialize_movement_image_loading_no_flow(path_image_source[i],false);
	}

	//make movement graphs
	if (include_motion_graphs){
		for (unsigned int i = 0; i < groups.size(); i++){
			if (!series.items[i].should_be_displayed) continue;
			ns_make_path_movement_graph(groups[i].paths[0],path_movement_graphs[i]);
			path_movement_graphs[i].draw(graph_temp);
		}
	}

	//load first frame of all paths


	//now go through each measurement time for the solution
	 long last_o = -5;
	for (unsigned long t = 0; t < aligned_size; t++){
		 long o2((100 * t) / aligned_size);
		if (o2 - last_o >= 5) {
			image_server_const.add_subtext_to_current_event(ns_to_string(o2)+"%...",&sql);
			last_o = o2;
		}

		ns_movement_posture_visualization_summary vis_summary;
		vis_summary.region_id = region_id;
		vis_summary.frame_number = t;
		vis_summary.alignment_frame_number = alignment_position;

		//init output image
		for (unsigned long y = 0; y < prop.height; y++)
			for (unsigned long x = 0; x < 3*prop.width; x++)
				output_image[y][x] = 0;

		bool image_to_output(false);

		//go through each path
		for (unsigned int g = 0; g < groups.size(); g++){
			const unsigned long path_id(0);
			ns_analyzed_image_time_path & path(groups[g].paths[path_id]);

			long i((long)t-((long)alignment_position - (long)path_aligned_event_index[g].index.period_end_index));
			
			if (!series.items[i].should_be_displayed || i >= (long)path.elements.size())
				continue;

	
			
			ns_color_8 c;
			if (i > path.elements.size()){
				c = ns_movement_colors::color(ns_movement_not_calculated);
				if (include_motion_graphs)marker_manager.set_marker(-1,path_movement_graphs[g]);
			}
			else if (i >= 0){
				c = ns_movement_colors::color(path.explicitly_recognized_movement_state(path.elements[i].absolute_time));
				if (include_motion_graphs)marker_manager.set_marker(path.elements[i].absolute_time,path_movement_graphs[g]);
			}
			else{
				c = ns_movement_colors::color(ns_movement_fast)/2;
				if (include_motion_graphs)marker_manager.set_marker(-1,path_movement_graphs[g]);
			}

			if (include_motion_graphs)path_movement_graphs[g].draw(graph_temp);
			

			
			int scale=1;
			if (i < 0)
				scale = 2;

			if (include_motion_graphs){
				for (unsigned int y = 0; y < graph_prop.height; y++){
					for (unsigned int x = 0; x < 3*graph_prop.width; x++){
						output_image[y+series.items[g].metadata_position_on_visualization_grid.y]
						[x+ 3* series.items[g].metadata_position_on_visualization_grid.x] = graph_temp[y][x]/scale;
					}
				}
			}

		
			//draw colored line around worm
			const ns_vector_2i & p (series.items[g].position_on_visualization_grid);
			const ns_vector_2i & s (series.items[g].final_image_size);
						

		//	const ns_vector_2i s (series.metadata_positions_on_visualization_grid[g]+ns_vector_2i(graph_prop.width+thickness,graph_prop.height));
			output_image.draw_line_color_thick(p+ns_vector_2i(-thickness,-thickness),p+ns_vector_2i(s.x,-thickness),c,thickness);
			output_image.draw_line_color_thick(p+ns_vector_2i(-thickness,-thickness),p+ns_vector_2i(-thickness,s.y),c,thickness);
			output_image.draw_line_color_thick(p+s,p+ns_vector_2i(s.x,-thickness),c,thickness);
			output_image.draw_line_color_thick(p+s,p+ns_vector_2i(-thickness,s.y),c,thickness);
			
			if (i >= 0){
				const unsigned long time(path.elements[i].absolute_time);
				//find most up to date frame for the path;
				ns_analyzed_time_image_chunk chunk(i,i+1);
				path.load_movement_images_no_flow(chunk,path_image_source[g]);
				if (i > 0)
					path.elements[i-1].clear_movement_images(); 

				image_to_output = true;
				//transfer over movement visualzation to the correct place on the grid.
				ns_image_standard im;
				path.elements[i].generate_movement_visualization(im);
		
				if (im.properties().height == 0)
					throw ns_ex("Registered images not loaded! Path ") << g << " i " << i;

				string::size_type ps(vis_summary.worms.size());
				vis_summary.worms.resize(ps+1);

				vis_summary.worms[ps].path_in_source_image.position = path.path_region_position;
				vis_summary.worms[ps].path_in_source_image.size= path.path_region_size;

				vis_summary.worms[ps].worm_in_source_image.position = path.elements[i].region_offset_in_source_image();
				vis_summary.worms[ps].worm_in_source_image.size = path.elements[i].worm_region_size();

				vis_summary.worms[ps].path_in_visualization.position = p;
				vis_summary.worms[ps].path_in_visualization.size = s;

				if (include_motion_graphs){
					vis_summary.worms[ps].metadata_in_visualizationA.position = series.items[g].metadata_position_on_visualization_grid;
					vis_summary.worms[ps].metadata_in_visualizationA.size = ns_vector_2i(graph_prop.width,graph_prop.height);
				}
				else vis_summary.worms[ps].metadata_in_visualizationA.position = vis_summary.worms[ps].metadata_in_visualizationA.size = ns_vector_2i(0,0);

				vis_summary.worms[ps].stationary_path_id.path_id = path_id;
				vis_summary.worms[ps].stationary_path_id.group_id = g;
				vis_summary.worms[ps].stationary_path_id.detection_set_id = analysis_id;
				vis_summary.worms[ps].path_time.period_end = path.elements.rbegin()->absolute_time;
				vis_summary.worms[ps].path_time.period_start = path.elements[0].absolute_time;
				vis_summary.worms[ps].image_time = path.elements[i].absolute_time;

			
				//ns_vector_2i region_offset(path.elements[i].region_offset_in_source_image()-path.elements[i].context_offset_in_source_image());
				for (unsigned int y = 0; y < vis_summary.worms[ps].path_in_visualization.size.y; y++){
					for (unsigned int x = 0; x < 3*vis_summary.worms[ps].path_in_visualization.size.x; x++){
						output_image[y+p.y]
						[x+ 3*p.x] = im[y][x];
					}
				}
			}


		}
		string metadata;
		vis_summary.to_xml(metadata);
		output_image.set_description(metadata);
		ns_image_server_captured_image_region reg;
		reg.region_images_id = 0;
		reg.region_info_id = region_id;
		reg.region_name = region_name;
		reg.sample_id = sample_id;
		reg.experiment_id = experiment_id;
		reg.sample_name = sample_name;
		reg.experiment_name = experiment_name;
		ns_image_server_image im(reg.create_storage_for_aligned_path_image(t,(unsigned long)event_to_align,ns_tiff,sql,ns_movement_event_to_label(event_to_align)));
		if (ns_fix_filename_suffix(im.filename, ns_tiff))
			im.save_to_db(im.id, &sql);
		try{
			bool had_to_use_volatile_storage;
			ns_image_storage_reciever_handle<ns_8_bit> r(image_server_const.image_storage.request_storage(im,ns_tiff,1.0,1024,&sql,had_to_use_volatile_storage,false,false));
			output_image.pump(r.output_stream(),1024);
			im.mark_as_finished_processing(&sql);
		}
		catch(ns_ex & ex){
			im.mark_as_finished_processing(&sql);
			throw ex;
		}
	}
	*/
}

template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::generate_movement_posture_visualizations(const bool include_graphs,const ns_64_bit region_id, const ns_time_path_solution & solution, bool generate_plate_worm_composite_images, ns_sql & sql ){
	const unsigned long thickness(3);

	ns_marker_manager marker_manager;

	ns_image_standard image_loading_temp,image_loading_temp_2, composit_temp;

	string compression_rate = image_server_const.get_cluster_constant_value("jp2k_compression_rate", ns_to_string(NS_DEFAULT_JP2K_COMPRESSION), &sql);
	float compression_rate_f = atof(compression_rate.c_str());
	if (compression_rate_f <= 0)
		throw ns_ex("Invalid compression rate: ") << compression_rate_f;

	const ns_vector_2i graph_dimensions(include_graphs ? ns_vector_2i(300, 200) : ns_vector_2i(0, 0));
	generate_movement_description_series();
	ns_worm_movement_description_series series = description_series;
	series.calculate_visualization_grid(graph_dimensions);
	if (series.items.size() != groups.size())
		throw ns_ex("calculate_visualization_grid() returned an inconsistant result");


	vector<ns_image_storage_source_handle<ns_8_bit> > path_image_source;
	vector<ns_graph> path_movement_graphs(groups.size());
	path_image_source.reserve(groups.size());

	ns_image_standard output_image;
	ns_image_properties prop;
	prop.components = 3;
	prop.width = series.visualization_grid_dimensions.x;
	prop.height = series.visualization_grid_dimensions.y;
	output_image.init(prop);

	ns_image_properties graph_prop(prop);
	ns_image_standard graph_temp;
	if (include_graphs){
		graph_prop.width = series.metadata_dimensions.x;
		graph_prop.height = series.metadata_dimensions.y;
		graph_temp.init(graph_prop);
	}
	else graph_prop.width = graph_prop.height = 0;

	ns_death_time_annotation_set machine_annotations;
	std::vector<ns_death_time_annotation> orphaned_events;
	produce_death_time_annotations(machine_annotations);
	std::string error_message;

	std::vector<ns_animal_list_at_position> machine_animals, by_hand_animals;

	ns_timing_data_configurator configurator;
	configurator(region_id,*this, machine_animals, by_hand_animals);
	ns_timing_data_and_death_time_annotation_matcher<std::vector<ns_animal_list_at_position> > matcher;
	matcher.load_timing_data_from_set(machine_annotations,true, by_hand_animals,orphaned_events,error_message);
	std::vector<ns_animal_list_at_position::ns_animal_list *> group_to_timing_data_map(groups.size(),0);

	image_server.add_subtext_to_current_event("\nCalculating intensity normalization stats...", &sql);
	//initialize path movement image source for first frame
	std::vector<ns_histogram<ns_64_bit,ns_8_bit> > pixel_ranges(groups.size());
	std::vector<std::pair<float,float> > pixel_scale_factors(groups.size());
	for (unsigned int g = 0; g < groups.size(); g++) {

		if (!series.items[g].should_be_displayed) {
			path_image_source.push_back(ns_image_storage_source_handle<ns_8_bit>(0)); //we want the index of groups[i] and path_image_source[i] to match so we fill unwanted groups with a dummy entry.
			continue;
		}
		groups[g].paths[0].output_image.load_from_db(groups[g].paths[0].output_image.id, &sql);
		path_image_source.push_back(image_server_const.image_storage.request_from_storage(groups[g].paths[0].output_image, &sql));
		groups[g].paths[0].initialize_movement_image_loading_no_flow(path_image_source[g], false);
		bool scale_images(true);
		if (scale_images) {
			//find lightest and darkest picture in all images, to get normalization factors for the visualization
			for (unsigned int j = 0; j < groups[g].paths[0].elements.size(); j++) {
				groups[g].paths[0].load_movement_images_no_flow(ns_analyzed_time_image_chunk(j, j + 1), path_image_source[g],memory_pool);
				const ns_image_standard &image(groups[g].paths[0].element(j).registered_images->image);
				for (unsigned int y = 0; y < image.properties().height; y++) {
					for (unsigned int x = 0; x < image.properties().width; x++) {
						pixel_ranges[g].increment(image[y][x]);
					}
				}
				groups[g].paths[0].elements[j].clear_movement_images(memory_pool.registered_image_pool);
			}
			pixel_scale_factors[g].first = pixel_ranges[g].average_of_ntile(1,100,true);
			ns_8_bit top = pixel_ranges[g].average_of_ntile(99, 100,true);
			if (pixel_scale_factors[g].first == top)
				pixel_scale_factors[g].second = 1;
			else
				pixel_scale_factors[g].second = 252 / (top - pixel_scale_factors[g].first);

			//reset images to prepare for the visualization generation
			path_image_source[g].clear();
			path_image_source[g] = image_server_const.image_storage.request_from_storage(groups[g].paths[0].output_image, &sql);
			groups[g].paths[0].initialize_movement_image_loading_no_flow(path_image_source[g], false);
		}
		else{
			pixel_scale_factors[g].first = 0;
			pixel_scale_factors[g].second = 1;
		}
		//link up to timing data
		for (unsigned int i = 0; i < by_hand_animals.size(); i++) {
			if (by_hand_animals[i].stationary_path_id.group_id == g)
				group_to_timing_data_map[g] = &by_hand_animals[i].animals;
		}
	}

	map<unsigned long, ns_64_bit> movement_paths_visualization_overlay;
	if (generate_plate_worm_composite_images) {
		sql << "SELECT capture_time," << ns_processing_step_db_column_name(ns_process_movement_paths_visualition_with_mortality_overlay) << " FROM sample_region_images WHERE region_info_id = " << region_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0) {
			image_server.add_subtext_to_current_event("No Movement Paths with Mortality Overlay Visualization Images have been created; the composite will not be generated.", &sql);
			generate_plate_worm_composite_images = false;
		}
		else {
			for (unsigned int i = 0; i < res.size(); i++) 
				movement_paths_visualization_overlay[atol(res[i][0].c_str())] = ns_atoi64(res[i][1].c_str());
		}
	}





	//make movement 
	if (include_graphs){
		for (unsigned int i = 0; i < groups.size(); i++){
			if (!series.items[i].should_be_displayed) continue;
			ns_make_path_movement_graph(groups[i].paths[0],path_movement_graphs[i]);
			path_movement_graphs[i].draw(graph_temp);
		}
	}

	//load first frame of all paths
	for (unsigned int g = 0; g < groups.size(); g++){
		if (!series.items[g].should_be_displayed) continue;
		groups[g].paths[0].load_movement_images_no_flow(ns_analyzed_time_image_chunk(0,1),path_image_source[g],memory_pool);
	}
	vector<unsigned long> current_path_timepoint_i(groups.size(), 0);

	image_server.add_subtext_to_current_event("\nGenerating visualization...", &sql);
	//now go through each measurement time for the solution
	 long last_r(-5);
	 for (unsigned long t = 0; t < solution.timepoints.size(); t++) {
		 long cur_r = (100 * t) / solution.timepoints.size();
		 if (cur_r - last_r >= 5) {
			 image_server_const.add_subtext_to_current_event(ns_to_string(cur_r) + "%...", &sql);
			 last_r = cur_r;
		 }

		 ns_movement_posture_visualization_summary vis_summary;
		 vis_summary.region_id = region_id;


		 //init output image
		 for (unsigned long y = 0; y < prop.height; y++)
			 for (unsigned long x = 0; x < 3 * prop.width; x++)
				 output_image[y][x] = 0;
		 bool image_to_output(false);

		 const unsigned long time(solution.timepoints[t].time);
		 //go through each path
		 for (unsigned int g = 0; g < groups.size(); g++) {
			 if (!series.items[g].should_be_displayed) continue;
			 const unsigned long path_id(0);
			 ns_analyzed_image_time_path & path(groups[g].paths[path_id]);
			 unsigned long & i(current_path_timepoint_i[g]);


			 //draw colored line around worm
			 const ns_vector_2i & p(series.items[g].position_on_visualization_grid);
			 const ns_vector_2i & s(series.items[g].final_image_size);
			 const ns_vector_2i & bc(series.items[g].visulazation_border_to_crop);
			 ns_color_8 c(63, 63, 63);
			 //if (path.element(path.first_stationary_timepoint()).absolute_time == time)
			 //	c = colors.color(ns_movement_slow);
			 output_image.draw_line_color_thick(p, p + ns_vector_2i(s.x - bc.x, 0), c, thickness);
			 output_image.draw_line_color_thick(p, p + ns_vector_2i(0, s.y - bc.y), c, thickness);
			 output_image.draw_line_color_thick(p + s - bc, p + ns_vector_2i(s.x - bc.x, 0), c, thickness);
			 output_image.draw_line_color_thick(p + s - bc, p + ns_vector_2i(0, s.y - bc.y), c, thickness);

			 unsigned best_frame;
			 if (time < path.elements[0].absolute_time)
				 continue;
			 if (i >= path.elements.size())
				 best_frame = path.elements.size() - 1;
			 else {
				 //find most up to date frame for the path;
				 for (best_frame = i; best_frame < path.elements.size() && path.elements[best_frame].absolute_time < time; best_frame++);
				 if (best_frame == path.elements.size())
					 best_frame = path.elements.size() - 1;
				 if (best_frame == 0 && path.elements[best_frame].absolute_time > time)
					 continue;
				 if (path.elements[best_frame].absolute_time > time)
					 best_frame--;

				 //now load frames until done
				 if (best_frame > i + 1)
					 image_server_const.add_subtext_to_current_event(std::string("Skipping") + ns_to_string(best_frame - i) + " frames from path " + ns_to_string(g) + "\n", &sql);


				 for (; i < best_frame;) {
					 //	cerr << "Clearing p"<<g<< " " << i << "\n";
					 if (i != 0)
						 path.elements[i - 1].clear_movement_images(memory_pool.registered_image_pool);
					 ++i;
					 ns_analyzed_time_image_chunk chunk(i, i + 1);
					 //	cerr << "loading p"<<g<< " " << i << "\n";
					 path.load_movement_images_no_flow(chunk, path_image_source[g],memory_pool);
				 }
			 }
			 //transfer over movement visualzation to the correct place on the grid.
			 const ns_image_standard &image(path.element(i).registered_images->image);
			 //	path.element(i).registered_images->image.pump(image, 1024);
				 //c = ns_movement_colors::color(path.explicitly_recognized_movement_state(time));

			 if (image.properties().height == 0)
				 throw ns_ex("Registered images not loaded! Path ") << g << " i " << i;


			 string::size_type ps(vis_summary.worms.size());
			 vis_summary.worms.resize(ps + 1);
			 vis_summary.worms[ps].path_in_source_image.position = path.path_region_position - bc / 2;
			 vis_summary.worms[ps].path_in_source_image.size = path.path_region_size;

			 vis_summary.worms[ps].worm_in_source_image.position = path.elements[i].region_offset_in_source_image() - bc / 2;
			 vis_summary.worms[ps].worm_in_source_image.size = path.elements[i].worm_region_size();

			 vis_summary.worms[ps].path_in_visualization.position = p;
			 vis_summary.worms[ps].path_in_visualization.size = s - bc;

			 if (include_graphs) {
				 vis_summary.worms[ps].metadata_in_visualizationA.position = series.items[g].metadata_position_on_visualization_grid;
				 vis_summary.worms[ps].metadata_in_visualizationA.size = ns_vector_2i(graph_prop.width, graph_prop.height);
			 }
			 else vis_summary.worms[ps].metadata_in_visualizationA.position = vis_summary.worms[ps].metadata_in_visualizationA.size = ns_vector_2i(0, 0);

			 vis_summary.worms[ps].stationary_path_id.path_id = path_id;
			 vis_summary.worms[ps].stationary_path_id.group_id = g;
			 vis_summary.worms[ps].stationary_path_id.detection_set_id = analysis_id;
			 vis_summary.worms[ps].path_time.period_start = path.elements[i].absolute_time;
			 vis_summary.worms[ps].path_time.period_end = path.elements.rbegin()->absolute_time;
			 vis_summary.worms[ps].image_time = path.elements[i].absolute_time;

			
			 //ns_vector_2i region_offset(path.elements[i].region_offset_in_source_image()-path.elements[i].context_offset_in_source_image());
			 for (unsigned int y = 0; y < vis_summary.worms[ps].path_in_visualization.size.y; y++) {
				 for (unsigned int x = 0; x < 3 * vis_summary.worms[ps].path_in_visualization.size.x; x++) {
					 //scale pixel and crop between 0 and 255
					 float p(image[y + bc.y / 2][x / 3 + bc.x / 2]);
					 p = (p > pixel_scale_factors[g].first) ? (p - pixel_scale_factors[g].first) : 0;
					 p = (p*pixel_scale_factors[g].second > 255) ?  255 : p*pixel_scale_factors[g].second;
					 
					 output_image[y + vis_summary.worms[ps].path_in_visualization.position.y]
						 [x + 3 * vis_summary.worms[ps].path_in_visualization.position.x] = (ns_8_bit)(round(p));
				 }
			 }
			 int height(10);

			 ns_death_time_annotation_time_interval current_time_interval;
			 if (t == 0)
				 current_time_interval = groups[g].paths[0].observation_limits().interval_before_first_observation;
			 else current_time_interval = ns_death_time_annotation_time_interval(
				 solution.timepoints[t - 1].time,
				 solution.timepoints[t].time);


			 if (group_to_timing_data_map[g] != 0) {
				 const unsigned long number_of_animals_annotated_at_position(group_to_timing_data_map[g]->size());

				 for (unsigned int k = 0; k < number_of_animals_annotated_at_position; k++) {
					 ns_death_time_annotation_time_interval first_path_obs, last_path_obs;
					 last_path_obs.period_end = groups[g].paths[0].element(groups[g].paths[0].element_count() - 1).absolute_time;
					 last_path_obs.period_start = groups[g].paths[0].element(groups[g].paths[0].element_count() - 2).absolute_time;
					 first_path_obs.period_start = groups[g].paths[0].element(0).absolute_time;
					 first_path_obs.period_end = groups[g].paths[0].element(1).absolute_time;

					 ns_crop_time(groups[g].paths[0].observation_limits(), first_path_obs, last_path_obs, (*group_to_timing_data_map[g])[k].fast_movement_cessation.time);
					 ns_crop_time(groups[g].paths[0].observation_limits(), first_path_obs, last_path_obs, (*group_to_timing_data_map[g])[k].death_associated_expansion_stop.time);
					 ns_crop_time(groups[g].paths[0].observation_limits(), first_path_obs, last_path_obs, (*group_to_timing_data_map[g])[k].death_associated_expansion_start.time);
					 ns_crop_time(groups[g].paths[0].observation_limits(), first_path_obs, last_path_obs, (*group_to_timing_data_map[g])[k].movement_cessation.time);
					 ns_crop_time(groups[g].paths[0].observation_limits(), first_path_obs, last_path_obs, (*group_to_timing_data_map[g])[k].translation_cessation.time);
					 (*group_to_timing_data_map[g])[k].draw_movement_diagram(
						 vis_summary.worms[ps].path_in_visualization.position + ns_vector_2i(0, vis_summary.worms[ps].path_in_visualization.size.y - (number_of_animals_annotated_at_position - k)*height),
						 ns_vector_2i(vis_summary.worms[ps].path_in_visualization.size.x, height),
						 groups[g].paths[0].observation_limits(), current_time_interval, output_image, 1, 3, ns_death_timing_data::ns_highlight_up_until_current_time,ns_death_timing_data::ns_draw_relative_to_plate);
				 }
			 }

			 if (include_graphs) {
				 marker_manager.set_marker(time, path_movement_graphs[g]);
				 path_movement_graphs[g].draw(graph_temp);

				 for (unsigned int y = 0; y < vis_summary.worms[ps].metadata_in_visualizationA.size.y; y++) {
					 for (unsigned int x = 0; x < 3 * vis_summary.worms[ps].metadata_in_visualizationA.size.x; x++) {
						 output_image[y + vis_summary.worms[ps].metadata_in_visualizationA.position.y]
							 [x + 3 * vis_summary.worms[ps].metadata_in_visualizationA.position.x] = graph_temp[y][x];
					 }
				 }
			 }



		 }
		 string metadata;
		 vis_summary.to_xml(metadata);
		 output_image.set_description(metadata);
		 ns_image_server_captured_image_region reg;
		 reg.region_images_id = solution.timepoints[t].sample_region_image_id;
		 ns_image_server_image im(reg.create_storage_for_processed_image(ns_process_movement_posture_visualization, ns_tiff, &sql));
		 try {
			 bool had_to_use_volatile_storage;
			 ns_image_storage_reciever_handle<ns_8_bit> r(image_server_const.image_storage.request_storage(im, ns_tiff, compression_rate_f, 1024, &sql, had_to_use_volatile_storage, false, ns_image_storage_handler::ns_forbid_volatile));
			 output_image.pump(r.output_stream(), 1024);
			 r.clear();
			 im.mark_as_finished_processing(&sql);
		 }
		 catch (ns_ex & ex) {
			 im.mark_as_finished_processing(&sql);
			 throw ex;
		 }

		 if (generate_plate_worm_composite_images) {
			 map<unsigned long, ns_64_bit>::iterator movement_paths_vis = movement_paths_visualization_overlay.find(time);
			 if (movement_paths_vis != movement_paths_visualization_overlay.end() &&
				 movement_paths_vis->second != 0) {
				 try {
					 ns_image_server_image im;
					 im.load_from_db(movement_paths_vis->second, &sql);
					 ns_image_storage_source_handle<ns_8_bit> in(image_server_const.image_storage.request_from_storage(im, &sql));
					 image_loading_temp.use_more_memory_to_avoid_reallocations();
					 in.input_stream().pump(image_loading_temp, 4096);
					 ns_image_properties plate_new_properties(image_loading_temp.properties());
					 plate_new_properties.width = (plate_new_properties.width*output_image.properties().height) / plate_new_properties.height;
					 plate_new_properties.height = output_image.properties().height;


					 image_loading_temp.resample(plate_new_properties, image_loading_temp_2);

					 ns_image_properties composit_prop(output_image.properties());
					 composit_prop.width += image_loading_temp_2.properties().width;
					 composit_temp.resize(composit_prop);

					 for (unsigned int y = 0; y < composit_prop.height; y++) {
						 for (unsigned x = 0; x < 3 * plate_new_properties.width; x++)
							 composit_temp[y][x] = image_loading_temp_2[y][x];
						 for (unsigned x = 0; x < 3 * output_image.properties().width; x++)
							 composit_temp[y][3 * (plate_new_properties.width) + x] = output_image[y][x];
					 }
					 ns_image_server_image im2(reg.create_storage_for_processed_image(ns_process_movement_plate_and_individual_visualization, ns_tiff, &sql));
					 bool had_to_use_volatile_storage;
					 ns_image_storage_reciever_handle<ns_8_bit> r2(image_server_const.image_storage.request_storage(im2, ns_tiff, compression_rate_f, 1024, &sql, had_to_use_volatile_storage, false, ns_image_storage_handler::ns_forbid_volatile));
					 composit_temp.pump(r2.output_stream(), 1024);
					 r2.clear();
					 im2.mark_as_finished_processing(&sql);
				 }
				 catch (ns_ex & ex) {
					 image_server_const.add_subtext_to_current_event("\nCould not generate composite image: " + ex.text(), &sql);
				 }
			 }
		 }

	 }
	 image_server_const.add_subtext_to_current_event("\nDone", &sql);
};


//we step through all region visualizations between start_i and stop_i, ordered by time in region_image_specifications
//and populate any worm images that are needed from each region image.
template<class allocator_T>
void ns_time_path_image_movement_analyzer<allocator_T>::load_region_visualization_images(const unsigned long start_i, const unsigned long stop_i,const unsigned int start_group, const unsigned int stop_group,ns_sql & sql, bool just_do_a_consistancy_check, bool running_backwards, ns_analyzed_image_time_path::ns_load_type load_type){
	if (stop_group > groups.size())
		throw ns_ex("ns_time_path_image_movement_analyzer::load_region_visualization_images()::An invalid group was specified: ") << stop_group << " (there are only " << groups.size() << " groups)";
//	cerr << "Looking for region images from time point " << start_i << " until " << stop_i;
	bool problem_occurred(false);
	ns_ex problem;

	
	for (unsigned long i = start_i; i < stop_i; i++){
		//check to see if the region images are needed at this time point
		region_image_specifications[i].region_vis_required = false;
		region_image_specifications[i].interpolated_region_vis_required = false;

		for (unsigned int g = start_group; g < stop_group; g++) {

#ifdef NS_OUTPUT_ALGINMENT_DEBUG
			//xxx
			if (g != 1)
				continue;
#endif
			for (unsigned int p = 0; p < groups[g].paths.size(); p++) {
				//if we're running backwards, we don't want to load any images
				//corresponding to time points that will only be processed when running forwards.
				if (running_backwards) {
					unsigned long latest_path_element_to_load = groups[g].paths[p].first_stationary_timepoint() + ns_analyzed_image_time_path::alignment_time_kernel_width;
					if (latest_path_element_to_load >= groups[g].paths[p].elements.size()) {
						if (groups[g].paths[p].elements.size() == 0)
							throw ns_ex("Empty path encountered!");
						else latest_path_element_to_load = groups[g].paths[p].elements.size() - 1;
					}

					if (groups[g].paths[p].first_stationary_timepoint() == 0 ||
						groups[g].paths[p].elements[latest_path_element_to_load].absolute_time < region_image_specifications[i].time) {
						/*if (g == 37) {
							if (groups[g].paths[p].first_stationary_timepoint() == 0) cout << "no stationary.";
							if (latest_path_element_to_load >= groups[g].paths[p].elements.size()) cout << "beyond latest (" << latest_path_element_to_load << ")";
							else if (groups[g].paths[p].elements[latest_path_element_to_load].absolute_time < region_image_specifications[i].time) cout << " too early";

						}*/
							continue;
					}
				}
				else {
					if (groups[g].paths[p].elements[groups[g].paths[p].first_stationary_timepoint()].absolute_time > region_image_specifications[i].time)
						continue;
				}
				//if ((g == 0 || g == 12))
				//	cout << "l(" << g << ")";

				//check to see if the group has a worm detected in the current timepoint
				bool region_vis_required = groups[g].paths[p].region_image_is_required(region_image_specifications[i].time, false, false);
				bool interpolated_region_vis_required = groups[g].paths[p].region_image_is_required(region_image_specifications[i].time, true, false);
				//if (g == 37)
				//	cout << "(" << (region_vis_required ? "1" : "0") << "," << (interpolated_region_vis_required ? "1" : "0") << ")";

				region_image_specifications[i].region_vis_required =
					region_image_specifications[i].region_vis_required || region_vis_required;
				region_image_specifications[i].interpolated_region_vis_required =
					region_image_specifications[i].interpolated_region_vis_required || interpolated_region_vis_required;
			}
		}
		if (!region_image_specifications[i].region_vis_required && !region_image_specifications[i].interpolated_region_vis_required)
			continue;

		try {
			if (region_image_specifications[i].interpolated_region_vis_required &&
				region_image_specifications[i].interpolated_region_vis_image.id==0)
				throw ns_ex("Required worm detection information could not be loaded for this image (Interpolated region visualization requested with no id specified)");
			if (region_image_specifications[i].region_vis_required &&
				region_image_specifications[i].region_vis_image.id==0)
				throw ns_ex("Required worm detection information could not be loaded for this image (Region visualization requested with no id specified)");
		

			if (load_type != ns_analyzed_image_time_path::ns_lrv_just_flag) {
				//region images contain the context images
				if (region_image_specifications[i].region_vis_required) {

					ns_image_storage_source_handle<ns_8_bit> in(image_server_const.image_storage.request_from_storage(region_image_specifications[i].region_vis_image, &sql));
					image_loading_temp.use_more_memory_to_avoid_reallocations();
					in.input_stream().pump(image_loading_temp, 4096);
				}
				else image_loading_temp.prepare_to_recieve_image(ns_image_properties(0, 0, 0, 0));

				if (region_image_specifications[i].interpolated_region_vis_required) {
					image_loading_temp2.use_more_memory_to_avoid_reallocations();
					ns_image_storage_source_handle<ns_8_bit> in2(image_server_const.image_storage.request_from_storage(region_image_specifications[i].interpolated_region_vis_image, &sql));
					in2.input_stream().pump(image_loading_temp2, 4096);
				}
				else image_loading_temp2.prepare_to_recieve_image(ns_image_properties(0, 0, 0, 0));

				//		cerr << "Done.\n";
						//extract green channel, as this has the grayscale values.
				ns_image_properties prop(image_loading_temp.properties());
				if (!region_image_specifications[i].region_vis_required)
					prop = image_loading_temp2.properties();
				if (prop.components != 3)
					throw ns_ex("ns_time_path_image_movement_analyzer::load_region_visualization_images()::Region images must be RBG");
			}
			bool new_data_allocated = false;
			for (unsigned int g = start_group; g < stop_group; g++) {
				//	if (g == 12)
					//cout << "LD";
				for (unsigned int p = 0; p < groups[g].paths.size(); p++) {
					if (g >= groups.size())
						throw ns_ex("ns_time_path_image_movement_analyzer::load_region_visualization_images()::An invalid group was specified: ") << g << " (there are only " << groups.size() << " groups)";

					if (p >= groups[g].paths.size())
						throw ns_ex("ns_time_path_image_movement_analyzer::load_region_visualization_images()::An invalid path was specified: ") << p << " (there are only " << groups[g].paths.size() << " paths)";

					if (running_backwards) {
						unsigned long latest_path_element_to_load = groups[g].paths[p].first_stationary_timepoint() + ns_analyzed_image_time_path::alignment_time_kernel_width;
						if (latest_path_element_to_load >= groups[g].paths[p].elements.size()) {
							if (groups[g].paths[p].elements.size() == 0)
								throw ns_ex("Empty path encountered!");
							else latest_path_element_to_load = groups[g].paths[p].elements.size() - 1;
						}

						if (groups[g].paths[p].first_stationary_timepoint() == 0 ||
							groups[g].paths[p].elements[latest_path_element_to_load].absolute_time < region_image_specifications[i].time) {
							//	if (g == 12) cout << "{!12}";
							continue;
						}
					}
					else {
						if (groups[g].paths[p].elements[groups[g].paths[p].first_stationary_timepoint()].absolute_time > region_image_specifications[i].time) {
							//	if (g == 12) "cout << {!12}";
							continue;
						}
					}
					//	if (g == 0 || g == 12)
					//	cout << "P[" << g << "]";
						//ns_movement_image_collage_info m(&groups[g].paths[p]);
					if (!just_do_a_consistancy_check) {
						if (groups[g].paths[p].populate_images_from_region_visualization(region_image_specifications[i].time, image_loading_temp, image_loading_temp2, just_do_a_consistancy_check, load_type, memory_pool))
							new_data_allocated = true;
					}
				}
			}
			if (!just_do_a_consistancy_check && new_data_allocated) {
				memory_pool.aligned_image_pool.mark_stack_size_waypoint_and_trim();
				memory_pool.registered_image_pool.mark_stack_size_waypoint_and_trim();
			}
		}
		catch(ns_ex & ex){
			problem_occurred = true;
			_number_of_invalid_images_encountered++;
			problem = ex;
			ns_64_bit problem_id = image_server_const.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event(ex.text(), true));
			sql << " UPDATE sample_region_images SET problem = " << problem_id << " where id = " << region_image_specifications[i].sample_region_image_id;
			sql.send_query();
			image_server_const.add_subtext_to_current_event(ns_image_server_event(ex.text()) << "\n", &sql);
		}
	}
	if (problem_occurred)
		throw problem;
}

#include "ns_threshold_movement_posture_analyzer.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_threshold_and_hmm_posture_analyzer.h"
ns_time_path_posture_movement_solution ns_threshold_movement_posture_analyzer::operator() (ns_analyzed_image_time_path * path, const bool fill_in_loglikelihood_timeseries,std::ostream * debug_output)const{
	return run(path,(std::ostream * )debug_output);
}
ns_time_path_posture_movement_solution ns_threshold_movement_posture_analyzer::operator()(const ns_analyzed_image_time_path * path, std::ostream * debug_output) const{
	return run(path,debug_output);
}
unsigned long ns_threshold_movement_posture_analyzer::latest_possible_death_time(const ns_analyzed_image_time_path * path,const unsigned long last_observation_time) const{
	return  last_observation_time - parameters.permanance_time_required_in_seconds;
}

void ns_analyzed_image_time_path::identify_expansion_time(const unsigned long death_index,
	std::vector<double> thresholds,
	std::vector<unsigned long> hold_times,
	std::vector<ns_death_time_expansion_info> & results,
	std::vector<ns_s64_bit> & intensity_changes) const {


	results.resize(thresholds.size()*hold_times.size());
	for (unsigned int i = 0; i < results.size(); i++)
		results[i].found_death_time_expansion = false;
	return;
	/*
	for (unsigned int ht = 0; ht < hold_times.size(); ht++) {
		double max_intensity_change(-DBL_MAX);
		unsigned long time_of_max_intensity_change(death_index);
		bool found_valid_time(false);
		unsigned long half_hold_time = hold_times[ht] / 2;

		
		//here we are calculate a running average rate of increase in intensity within the stablized region
		//we then identify the largest increase after death, and call that the death time increase
		for (long t = death_index; t < element_count(); t++) {
			if (element(t).excluded)
				continue;
			unsigned long cur_t = element(t).absolute_time;

			long left_i(death_index), right_i(death_index);

			//this could be done faster with a sliding window.
			//find the apropriate sized time window 
			for (long i = t-1; i >= 0; i--) {
				unsigned long dt = cur_t - elements[i].absolute_time;
				if (dt < half_hold_time)
					left_i = i;
				else break;
			}
			for (long i = t+1; i < element_count(); i++) {
				unsigned long dt = elements[i].absolute_time - cur_t;
				if (dt < half_hold_time)
					right_i = i;
				else break;
			}

			//find largest intensity change within in kernel to use as the "peak time".
			ns_s64_bit total_intensity_change(0);
			unsigned long point_count(0);

			for (long tt = left_i; tt <= right_i; tt++) {
				if (element(tt).excluded) continue;
				total_intensity_change += element(tt).measurements.change_in_total_stabilized_intensity_1x;
				point_count++;
			}

			if (point_count == 0)
				continue;
			found_valid_time = true;
			const double av_change = total_intensity_change / point_count;
			const bool is_max_so_far(av_change > max_intensity_change);
			if (is_max_so_far) {
				max_intensity_change = av_change; 
				time_of_max_intensity_change = t;
			}
		}

		for (unsigned int thresh = 0; thresh < thresholds.size(); thresh++) {
			ns_death_time_expansion_info & result(results[ht*thresholds.size() + thresh]);
			//if there's a large enough peak, declare the death time expansion identified.
			if (!found_valid_time) {
				result.found_death_time_expansion = false;
				continue;
			}

			if (max_intensity_change >= thresholds[thresh]) {
				//		if (thresholds.size() > 1)
				//			cerr << "Found " << thresholds[thresh] << " " << hold_times[ht] << "\n";
				result.found_death_time_expansion = true;
				result.time_point_at_which_death_time_expansion_peaked = time_of_max_intensity_change;
				result.time_point_at_which_death_time_expansion_started = time_of_max_intensity_change;
				result.time_point_at_which_death_time_expansion_stopped = time_of_max_intensity_change;
				//push backwards in time until the animal stops growing
				//	cerr << "*\n";
				for (long tt = time_of_max_intensity_change; tt >= 0; tt--) {
					if (element(tt).excluded)
						continue;
					if (element(tt).measurements.change_in_total_stabilized_intensity_1x > 0) {
						result.time_point_at_which_death_time_expansion_started = tt;
					}
					else
						break;
				}
				for (long tt = time_of_max_intensity_change; tt < element_count(); tt++) {
					if (element(tt).excluded)
						continue;
					if (element(tt).measurements.change_in_total_stabilized_intensity_1x > 0) {
						result.time_point_at_which_death_time_expansion_stopped = tt;
					}
					else
						break;
				}

			}
			else {
				result.found_death_time_expansion = false;
			}

			if (result.time_point_at_which_death_time_expansion_started == result.time_point_at_which_death_time_expansion_stopped)
				result.found_death_time_expansion = false;
		}
	}*/
}

ns_time_path_posture_movement_solution ns_threshold_movement_posture_analyzer::run(const ns_analyzed_image_time_path * path, std::ostream * debug_output) const{

	
	bool found_slow_movement_cessation(false),
		found_posture_change_cessation(false),
		found_death_time_expansion(false);


	//since the movement images are calculated as image[t]-image[t-1], that means the movement occurred between time points t and t-1.
	//thus if the movement ratio drops below a threshold at time t, the animal died between time t and t-1.
	//thus when an animal's movement ratio drops below some threshold at time t, we annotate it's death at time t.

	//an exception is made for animals whose movement ratio drops below a threshold at the second frame.
	//this is because we can't calculate reasonable movement ratios in the first frame, and thus assume the animals 
	//died the frame before it.
	long last_valid_event_index(-1),
		first_valid_event_index(path->element_count());

	
	for (long t = 0; t < path->element_count(); t++){
		if (path->element(t).excluded || path->element(t).censored || path->element(t).element_before_fast_movement_cessation) continue;
		if (first_valid_event_index == path->element_count()){
			first_valid_event_index = t;
		}
		last_valid_event_index = t;
	}

		
	if (last_valid_event_index == -1){
		/*ns_time_path_posture_movement_solution sol;
		sol.dead.skipped = true;
		sol.moving.skipped = true;
		sol.slowing.skipped = true;
		return sol;*/
		throw ns_ex("Encountered a path without valid elements!");
	}
	long last_time_point_at_which_slow_movement_was_observed(first_valid_event_index),
		last_time_point_at_which_posture_changes_were_observed(first_valid_event_index),
		time_point_at_which_death_time_expansion_peaked(first_valid_event_index),
		time_point_at_which_death_time_expansion_started(first_valid_event_index),
		time_point_at_which_death_time_expansion_stopped(first_valid_event_index);

	unsigned long longest_gap_in_slow(0),longest_gap_in_posture(0),longest_gap_in_dead(0);

	
	for (long t = first_valid_event_index ; t < path->element_count(); t++){
		if (path->element(t).excluded || path->element(t).censored || path->element(t).element_before_fast_movement_cessation) continue;
		
		const double r(
			parameters.use_v1_movement_score?
			path->element(t).measurements.death_time_posture_analysis_measure_v1():
			path->element(t).measurements.death_time_posture_analysis_measure_v2_cropped());

		unsigned long observation_gap(0);
		if (t > first_valid_event_index){
			observation_gap = path->element(t).absolute_time - path->element(t-1).absolute_time;
		}
		


		const unsigned long &cur_time (path->element(t).absolute_time);
		//keep on pushing the last posture time and last sationary
		//times forward until we hit a low enough movement ratio
		//to meet the criteria.  At that point, the last posture 
		//and last stationary cutoffs stick
		
		
		if (r >= parameters.posture_cutoff && !found_slow_movement_cessation)
			last_time_point_at_which_slow_movement_was_observed = t;
		if (r >=  parameters.stationary_cutoff&& !found_posture_change_cessation)
			last_time_point_at_which_posture_changes_were_observed = t;

		if (!found_slow_movement_cessation && longest_gap_in_slow < observation_gap)
			longest_gap_in_slow = observation_gap;
		
		if (!found_posture_change_cessation && longest_gap_in_posture < observation_gap)  //gaps in slow movement count towards gaps in posture change.
			longest_gap_in_posture = observation_gap;
		
		if (found_posture_change_cessation && longest_gap_in_dead < observation_gap)  //gaps in slow movement count towards gaps in posture change.
			longest_gap_in_dead = observation_gap;
		

		if (!found_slow_movement_cessation &&

			//if the last time the worm was moving slowly enough to count 
			//as changing posture or remaining stationary is a long time ago
			//mark that time as the change.
			(cur_time - path->element(last_time_point_at_which_slow_movement_was_observed).absolute_time) >= parameters.permanance_time_required_in_seconds){
				//posture_start_time = elements[last_posture_index+1].absolute_time;
				found_slow_movement_cessation = true;
				//since we can't make accurate measurements of the first time point (we can't measure movement
				//as we don't have a prior measurement of it if an event falls one after it,
				//shift it all the way to the first time point.
				if (last_time_point_at_which_slow_movement_was_observed == 1 && first_valid_event_index == 0)
					last_time_point_at_which_slow_movement_was_observed = 0;
			
		}

		if (!found_posture_change_cessation &&

			(cur_time - path->element(last_time_point_at_which_posture_changes_were_observed).absolute_time) > parameters.permanance_time_required_in_seconds){
				//stationary_start_time = elements[last_stationary_index+1].absolute_time;
				found_posture_change_cessation = true;
				if (last_time_point_at_which_posture_changes_were_observed == 1 && first_valid_event_index == 0)
					last_time_point_at_which_posture_changes_were_observed = 0;
			}
	}
	//identify time of death expansion
	if (found_posture_change_cessation) {
		vector<double> thresholds(1, parameters.death_time_expansion_cutoff);
		vector<unsigned long> hold_times(1,parameters.death_time_expansion_time_kernel_in_seconds);
		vector<ns_death_time_expansion_info> results(1);	
		vector<ns_s64_bit> tmp;
			path->identify_expansion_time(last_time_point_at_which_posture_changes_were_observed,
				thresholds,
				hold_times,
				results,
				tmp);

			found_death_time_expansion = results[0].found_death_time_expansion;
			time_point_at_which_death_time_expansion_peaked = results[0].time_point_at_which_death_time_expansion_peaked;
			time_point_at_which_death_time_expansion_started = results[0].time_point_at_which_death_time_expansion_started;
			time_point_at_which_death_time_expansion_stopped = results[0].time_point_at_which_death_time_expansion_stopped;

	}

	ns_time_path_posture_movement_solution solution;
	//assumed to start as slow, so it's only skipped if it is skipped over

	solution.dead.longest_observation_gap_within_interval = longest_gap_in_dead;
	solution.slowing.longest_observation_gap_within_interval = longest_gap_in_posture;
	solution.moving.longest_observation_gap_within_interval = longest_gap_in_slow;

	solution.moving.skipped = last_time_point_at_which_slow_movement_was_observed == first_valid_event_index  && (found_slow_movement_cessation || found_posture_change_cessation);
	if (!solution.moving.skipped){
		solution.moving.start_index = first_valid_event_index ;
		solution.moving.end_index = last_time_point_at_which_slow_movement_was_observed;
		if (!found_slow_movement_cessation && !found_posture_change_cessation)
			solution.moving.end_index = last_valid_event_index;
	//	solution.moving.end_index = 0;
	}

	solution.slowing.skipped = !found_slow_movement_cessation	//if the animal never slowed down to changing posture
		
								|| found_posture_change_cessation &&   //or it was skipped over as the animal went straight to death
								last_time_point_at_which_posture_changes_were_observed == last_time_point_at_which_slow_movement_was_observed;
	if (!solution.slowing.skipped){
		solution.slowing.start_index = (!solution.moving.skipped)?(last_time_point_at_which_slow_movement_was_observed):first_valid_event_index;
		solution.slowing.end_index = last_time_point_at_which_posture_changes_were_observed;
		if (!found_posture_change_cessation)
			solution.slowing.end_index = last_valid_event_index;
	}

	//death can't be skipped over, so it's only skipped if it is never observed to occur
	solution.dead.skipped = !found_posture_change_cessation;
	if (!solution.dead.skipped){
		solution.dead.start_index = last_time_point_at_which_posture_changes_were_observed;
		solution.dead.end_index = last_valid_event_index;
	}
	solution.expanding.skipped = !found_death_time_expansion;
	if (!solution.expanding.skipped) {
		solution.expanding.start_index = time_point_at_which_death_time_expansion_started;
		solution.expanding.end_index = time_point_at_which_death_time_expansion_stopped;
	}
	solution.post_expansion_contracting.skipped = true; //this is not detected using this algorithm.

	if (solution.dead.skipped &&
		solution.slowing.skipped &&
		solution.moving.skipped)
		throw ns_ex("Producing an all-skipped estimate!");
	if (!solution.moving.skipped && solution.moving.start_index == solution.moving.end_index)
		throw ns_ex("Unskipped 0-measurement movement period!");
	if (!solution.slowing.skipped && solution.slowing.start_index == solution.slowing.end_index)
		throw ns_ex("Unskipped 0-measurement slowing period");
	if (!solution.dead.skipped && solution.dead.start_index == solution.dead.end_index)
		throw ns_ex("Unskipped 0-measurement dead period");
	if (!solution.slowing.skipped && !solution.dead.skipped && solution.dead.start_index > solution.slowing.end_index)
		throw ns_ex("Reversed death and slow movement cessation times!");
	solution.loglikelihood_of_solution = 0;
	return solution;
}



ns_analyzed_image_time_path_death_time_estimator * ns_get_death_time_estimator_from_posture_analysis_model(const ns_posture_analysis_model & m){
  if (m.name.empty())
    throw ns_ex("ns_analyzed_image_time_path_death_time_estimator::ns_get_death_time_estimator_from_posture_analysis_model::received model with no name");
	ns_analyzed_image_time_path_death_time_estimator * p;
	if (m.posture_analysis_method == ns_posture_analysis_model::ns_hidden_markov){
	  //cout << "MARKOV";
		p = new ns_time_path_movement_markov_solver(m.hmm_posture_estimator);
	}
	else if (m.posture_analysis_method == ns_posture_analysis_model::ns_threshold){
	  // cout <<"THRESH";
		p = new ns_threshold_movement_posture_analyzer(m.threshold_parameters);
	}
	else if (m.posture_analysis_method == ns_posture_analysis_model::ns_threshold_and_hmm) {
		p = new ns_threshold_and_hmm_posture_analyzer(m);
	}
	else if (m.posture_analysis_method == ns_posture_analysis_model::ns_not_specified)
		throw ns_ex("ns_get_death_time_estimator_from_posture_analysis_model()::No posture analysis method specified.");
	else
		throw ns_ex("ns_get_death_time_estimator_from_posture_analysis_model()::Unknown posture analysis method!");

	p->name = m.name;
	return p;
}

template class ns_time_path_image_movement_analyzer< ns_wasteful_overallocation_resizer>;
template class ns_time_path_image_movement_analyzer< ns_overallocation_resizer>;
template class ns_analyzed_image_time_path_group< ns_wasteful_overallocation_resizer>;

template class ns_analyzed_image_time_path_group< ns_overallocation_resizer>;
