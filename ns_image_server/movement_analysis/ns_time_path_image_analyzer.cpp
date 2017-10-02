#include "ns_time_path_image_analyzer.h"
#include "ns_graph.h"
#include "ns_xml.h"
#include "ns_image_tools.h"
#include "ctmf.h"
#include "ns_thread_pool.h"
#include "ns_linear_regression_model.h"

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



ns_analyzed_image_time_path::~ns_analyzed_image_time_path(){
		ns_safe_delete(output_reciever);
		ns_safe_delete(flow_output_reciever);
		for (unsigned int i = 0; i < elements.size(); i++){
			elements[i].clear_movement_images();
			elements[i].clear_path_aligned_images();
		}
		
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
void ns_time_path_image_movement_analyzer::delete_from_db(const ns_64_bit region_id,ns_sql & sql){
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
ns_64_bit ns_time_path_image_movement_analyzer::calculate_division_size_that_fits_in_specified_memory_size(const ns_64_bit & mem_size, const int multiplicity_of_images)const{
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
void ns_time_path_image_movement_analyzer::calculate_memory_pool_maximum_image_size(const unsigned int start_group,const unsigned int stop_group){
	
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
void ns_time_path_image_movement_analyzer::output_allocation_state(const std::string & stage, long timepoint,std::ostream & out) const {
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
void ns_time_path_image_movement_analyzer::output_allocation_state_header(std::ostream & out) const {
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

void ns_time_path_image_movement_analyzer::run_group_for_current_backwards_round(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data * persistant_data,ns_movement_analysis_shared_state * shared_state) {
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
		groups[i].paths[j].copy_aligned_path_to_registered_image(chunk, persistant_data->temporary_image);
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
		if (groups[i].paths[j].elements[k].clear_path_aligned_images())
			shared_state->deallocated_aligned_count++;
	}
	//cerr << "Cr" << i << "(" << chunk.start_i + registered_image_clear_lag << "-" << groups[i].paths[j].elements.size() << ")";
	for (long k = chunk.start_i + shared_state->registered_image_clear_lag; k < groups[i].paths[j].elements.size(); k++)
		groups[i].paths[j].elements[k].clear_movement_images();

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
			if (groups[i].paths[j].elements[k].clear_path_aligned_images())
				shared_state->deallocated_aligned_count++;
		}
		//cerr << "FCr" << i << "(0-" << groups[i].paths[j].elements.size() << ")";
		for (long k = 0; k < groups[i].paths[j].elements.size(); k++)
			groups[i].paths[j].elements[k].clear_movement_images();

		groups[i].paths[j].reset_movement_image_saving();
		//shared_state->path_reset[i]++;
		//if (shared_state->path_reset[i] > 1)
			//cerr << "YIKES!";
	}
}


typedef void (ns_time_path_image_movement_analyzer::*ns_time_path_image_analysis_thread_job_pointer)(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data *,ns_movement_analysis_shared_state *);

struct ns_time_path_image_movement_analyzer_thread_pool_job {
	ns_time_path_image_movement_analyzer_thread_pool_job() {}
	ns_time_path_image_movement_analyzer_thread_pool_job(unsigned long g_id,
		unsigned long p_id,
		ns_time_path_image_analysis_thread_job_pointer f,
		ns_time_path_image_movement_analyzer * m,
		ns_movement_analysis_shared_state * ss) :group_id(g_id), path_id(p_id), function_to_call(f), ma(m), shared_state(ss) {}
	unsigned long group_id, path_id;
	ns_time_path_image_analysis_thread_job_pointer function_to_call;
	ns_time_path_image_movement_analyzer * ma;
	ns_movement_analysis_shared_state * shared_state;

	void operator()(ns_time_path_image_movement_analyzer_thread_pool_persistant_data & persistant_data){
		(ma->*function_to_call)(group_id, path_id, &persistant_data,shared_state);
	}
};

typedef std::pair<unsigned int, unsigned int> ns_tpiatp_job;

void ns_time_path_image_movement_analyzer::finish_up_and_write_to_long_term_storage(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data * persistant_data, ns_movement_analysis_shared_state * shared_state) {

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
		groups[group_id].paths[path_id].load_movement_images_no_flow(chunk, in);

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
			groups[group_id].paths[path_id].elements[j].clear_movement_images();
	}
	groups[group_id].paths[path_id].denoise_movement_series_and_calculate_intensity_slopes(0, *shared_state->time_series_denoising_parameters);
}
void ns_time_path_image_movement_analyzer::run_group_for_current_forwards_round(unsigned int group_id, unsigned int path_id, ns_time_path_image_movement_analyzer_thread_pool_persistant_data * persistant_data,ns_movement_analysis_shared_state * shared_state) {
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
	groups[i].paths[j].copy_aligned_path_to_registered_image(chunk, persistant_data->temporary_image);
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
		groups[i].paths[j].elements[k].clear_path_aligned_images();
	for (long k = 0; k < (long)chunk.start_i - (long)shared_state->registered_image_clear_lag + 1; k++)
		groups[i].paths[j].elements[k].clear_movement_images();

	if (shared_state->chunk_generators[i][j].no_more_chunks_forward()) {

		for (long k = 0; k < groups[i].paths[j].elements.size(); k++) {
			groups[i].paths[j].elements[k].clear_path_aligned_images();
			groups[i].paths[j].elements[k].clear_movement_images();
		}
		groups[i].paths[j].reset_movement_image_saving();
	//	shared_state->path_reset[i]++;
	//	if (shared_state->path_reset[i] > 1)
	//		cerr << "YIKES!";
	}
}
void throw_pool_errors(
	ns_thread_pool<ns_time_path_image_movement_analyzer_thread_pool_job,
	ns_time_path_image_movement_analyzer_thread_pool_persistant_data> & thread_pool,ns_sql & sql) {

	ns_time_path_image_movement_analyzer_thread_pool_job job;
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



bool ns_time_path_image_movement_analyzer::try_to_rebuild_after_failure() const {return  _number_of_invalid_images_encountered*10 <= number_of_timepoints_in_analysis_; }


void ns_time_path_image_movement_analyzer::process_raw_images(const ns_64_bit region_id,const ns_time_path_solution & solution_, const ns_time_series_denoising_parameters & times_series_denoising_parameters,const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql, const long group_number,const bool write_status_to_db){
	region_info_id = region_id; 
	obtain_analysis_id_and_save_movement_data(region_id, sql, ns_force_creation_of_new_db_record,ns_do_not_write_data);
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
		ns_thread_pool<ns_time_path_image_movement_analyzer_thread_pool_job, 
					   ns_time_path_image_movement_analyzer_thread_pool_persistant_data> thread_pool;
		thread_pool.set_number_of_threads(image_server_const.maximum_number_of_processing_threads()*3);
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
						image_server_const.add_subtext_to_current_event(ns_image_server_event("Found an error; doing a consistancy check on all images in the region: ") << ex.text(),&sql);
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
								ns_time_path_image_movement_analyzer_thread_pool_job(i, j,
									&ns_time_path_image_movement_analyzer::run_group_for_current_backwards_round,
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

					throw_pool_errors(thread_pool, sql);
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
						groups[i].paths[j].load_movement_images_no_flow(chunk, storage);
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
						//it shouldn't matter where this is done; we might want to paralellize it if it ever gets expensive.
						//groups[i].paths[j].quantify_movement(chunk);
#ifdef NS_CALCULATE_OPTICAL_FLOW
						groups[i].paths[j].save_movement_images(chunk, sql, ns_analyzed_image_time_path::ns_save_both, false);
#else
						groups[i].paths[j].save_movement_images(chunk, sql, ns_analyzed_image_time_path::ns_save_simple,ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_local_1);
#endif
						//we leave the image saving buffers open as we'll continue writing to them while moving forward.
	//					groups[i].paths[j].reset_movement_image_saving();

						//deallocate all before first time point.
						for (long k = 0; k <= groups[i].paths[j].first_stationary_timepoint(); k++) {
							groups[i].paths[j].elements[k].clear_movement_images();
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
								ns_time_path_image_movement_analyzer_thread_pool_job(i, j,
									&ns_time_path_image_movement_analyzer::run_group_for_current_forwards_round,
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
					throw_pool_errors(thread_pool, sql);
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

		image_server_const.add_subtext_to_current_event("\n", (write_status_to_db ? (&sql) : 0));
		image_server_const.add_subtext_to_current_event(ns_image_server_event("Stabilizing regions of focus..."), (write_status_to_db ? (&sql) : 0));

		thread_pool.set_number_of_threads(image_server_const.maximum_number_of_processing_threads());
		thread_pool.prepare_pool_to_run();

		//we just need to do a few final calculations and then copy everything to long term storage
		for (unsigned int i = 0; i < groups.size(); i++) {
			for (unsigned int j = 0; j < groups[i].paths.size(); j++) {

				thread_pool.add_job_while_pool_is_not_running(
					ns_time_path_image_movement_analyzer_thread_pool_job(i, j,
						&ns_time_path_image_movement_analyzer::finish_up_and_write_to_long_term_storage,
						this, &shared_state));
			}
		}

		thread_pool.run_pool();

		//output progres information
		if (write_status_to_db) {
			int last_n(0);
			while (true) {
				int n = thread_pool.number_of_jobs_pending();
				if (n != last_n) {
					ns_acquire_lock_for_scope lock(shared_state.sql_lock, __FILE__, __LINE__);
					image_server_const.add_subtext_to_current_event(ns_to_string((100 * (groups.size()-n)) / groups.size()) + "%...", (write_status_to_db ? (&sql) : 0));
					lock.release();
					last_n = n;
				}
				if (n == 0)
					break;
			}
			ns_thread::sleep(3);
		}

		thread_pool.wait_for_all_threads_to_become_idle();
		throw_pool_errors(thread_pool, sql);
		thread_pool.shutdown();

		//OK! Now we have /everything/ finished with the images.
		//calculate some final stats and then we're done.
		normalize_movement_scores_over_all_paths(e->software_version_number(),times_series_denoising_parameters);
		//xxx this could be paraellelized if it was worth it.
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
						if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
								continue;
				groups[i].paths[j].analyze_movement(e,ns_stationary_path_id(i,j,analysis_id),last_timepoint_in_analysis_);
				groups[i].paths[j].calculate_movement_quantification_summary();
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
		generate_movement_description_series();


		mark_path_images_as_cached_in_db(region_id,sql);
		movement_analyzed = true;
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

void ns_time_path_image_movement_analyzer::load_from_solution(const ns_time_path_solution & solution_, const long group_number){
	solution = &solution_;
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
		groups.push_back(ns_analyzed_image_time_path_group(ns_stationary_path_id(group_number,0,analysis_id),region_info_id,solution_,externally_specified_plate_observation_interval,extra_non_path_events,memory_pool));
		for (unsigned int i = 0; i < groups.rbegin()->paths.size(); i++){
			if (groups.rbegin()->paths[i].elements.size() < ns_analyzed_image_time_path::alignment_time_kernel_width)
				throw ns_ex("ns_time_path_image_movement_analyzer::load_from_solution::Path loaded that is too short.");
		}
		if (groups.rbegin()->paths.size() == 0)
			groups.pop_back();
	}
	else{
		for (unsigned int i = 0; i < solution_.path_groups.size(); i++){
			
			groups.push_back(ns_analyzed_image_time_path_group(ns_stationary_path_id(i, 0, analysis_id),region_info_id,solution_,externally_specified_plate_observation_interval,extra_non_path_events,memory_pool));
			for (unsigned int j = 0; j < groups.rbegin()->paths.size(); j++){
				if (groups.rbegin()->paths[j].elements.size() < ns_analyzed_image_time_path::alignment_time_kernel_width)
					throw ns_ex("ns_time_path_image_movement_analyzer::load_from_solution::Path loaded that is too short.");
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
				ns_stationary_path_id(0,0,analysis_id),false,e.inferred_animal_location, e.subregion_info,
				expl)
		);
	}

	
	get_processing_stats_from_solution(solution_);
}


void ns_time_path_image_movement_analyzer::get_output_image_storage_locations(const ns_64_bit region_id,ns_sql & sql,const bool create_only_flow){

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
ns_death_time_annotation_time_interval ns_time_path_image_movement_analyzer::get_externally_specified_last_measurement(const ns_64_bit region_id, ns_sql & sql){
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
void ns_time_path_image_movement_analyzer::crop_path_observation_times(const ns_death_time_annotation_time_interval & val){
	
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
ns_image_server_image ns_time_path_image_movement_analyzer::get_movement_quantification_id(const ns_64_bit reg_info_id, ns_sql & sql) {
	sql << "SELECT movement_image_analysis_quantification_id FROM sample_region_image_info WHERE id = " << reg_info_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_image_movement_analyzer::load_movement_data_from_db():Could not load info from db");
	ns_image_server_image im;
	im.id = ns_atoi64(res[0][0].c_str());
	if (im.id == 0)
		throw ns_ex("Movement quantification data has not been stored in db");
	return im;
}

void ns_time_path_image_movement_analyzer::populate_movement_quantification_from_file(ns_sql & sql,const bool skip_movement_data){
	
	
	ns_image_server_image im = get_movement_quantification_id(this->region_info_id,sql);
	ifstream * i(image_server_const.image_storage.request_metadata_from_disk(im,false,&sql));

	try{
		load_movement_data_from_disk(*i, skip_movement_data);
		delete i;
	}
	catch(...){
		delete i;
		throw;
	}
};

void ns_time_path_image_movement_analyzer::reanalyze_with_different_movement_estimator(const ns_time_series_denoising_parameters &,const ns_analyzed_image_time_path_death_time_estimator * e){
	if (region_info_id == 0)
		throw ns_ex("Attempting to reanalyze an unloaded image!");
	
	for (unsigned long g = 0; g < groups.size(); g++)
		for (unsigned long p = 0; p < groups[g].paths.size(); p++){
			if (ns_skip_low_density_paths && groups[g].paths[p].is_low_density_path())
				continue;
			groups[g].paths[p].analyze_movement(e,ns_stationary_path_id(g,p,analysis_id),last_timepoint_in_analysis_);
			groups[g].paths[p].calculate_movement_quantification_summary();
		}
}
bool ns_time_path_image_movement_analyzer::load_completed_analysis(const ns_64_bit region_id,const ns_time_path_solution & solution_,  const ns_time_series_denoising_parameters & times_series_denoising_parameters, const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql, bool exclude_movement_quantification){
	region_info_id = region_id;
	externally_specified_plate_observation_interval = get_externally_specified_last_measurement(region_id,sql);
	load_from_solution(solution_);
	crop_path_observation_times(externally_specified_plate_observation_interval);
	bool found_path_info_in_db = load_movement_image_db_info(region_info_id,sql);


	
	for (unsigned long g = 0; g < groups.size(); g++){
		for (unsigned long p = 0; p < groups[g].paths.size(); p++)
			for (unsigned int i = 0; i < groups[g].paths[p].death_time_annotations().events.size(); i++){
			//	groups[g].paths[p].by_hand_annotation_event_times.resize((int)ns_number_of_movement_event_types,-1);
				groups[g].paths[p].death_time_annotation_set.events[i].region_info_id = region_info_id;
			}
	}
	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Populating movement quantification from file"));
	populate_movement_quantification_from_file(sql, exclude_movement_quantification);
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
		//	if (g == 5)
		//		cerr << "WHA";
			groups[g].paths[p].denoise_movement_series_and_calculate_intensity_slopes(0,times_series_denoising_parameters);
		}

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Normalizing scores"));
	normalize_movement_scores_over_all_paths(e->software_version_number(),times_series_denoising_parameters);
	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Calculating quantifications"));

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
		//	if (g == 5)
		//		cerr << "WHA";
			groups[g].paths[p].analyze_movement(e,ns_stationary_path_id(g,p,analysis_id),last_timepoint_in_analysis_);
			groups[g].paths[p].calculate_movement_quantification_summary();
		}	

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("Done"));

	//generate_movement_description_series();
	movement_analyzed = true;
	return found_path_info_in_db;
}

void ns_time_path_image_movement_analyzer::obtain_analysis_id_and_save_movement_data(const ns_64_bit region_id, ns_sql & sql, ns_analysis_db_options id_options, ns_data_write_options write_options){

	
	sql << "SELECT movement_image_analysis_quantification_id FROM sample_region_image_info WHERE id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_image_movement_analyzer::save_movement_data_to_db():Could not load info from db");
	ns_image_server_image im;
	im.id = ns_atoi64(res[0][0].c_str());

	if (id_options == ns_require_existing_record && im.id == 0)
		throw ns_ex(" ns_time_path_image_movement_analyzer::obtain_analysis_id_and_save_movement_data()::Could not find existing record.");

	if (im.id != 0) { //load and check for bad filenames
		im.load_from_db(im.id, &sql);
		if (im.filename.empty()) {
			if (id_options == ns_require_existing_record)
				throw ns_ex("obtain_analysis_id_and_save_movement_data()::Encountered an empty record");
			id_options = ns_force_creation_of_new_db_record;
		}
	}
	if (id_options == ns_force_creation_of_new_db_record && im.id != 0) {
		sql << "DELETE FROM images WHERE id = " << im.id;
		sql.send_query();
		im.id = 0; 
		sql << "UPDATE sample_region_image_info SET movement_image_analysis_quantification_id = 0 WHERE id = " << region_id;
		sql.send_query();
	}

	bool update_db(false);
	if (im.id == 0){
		im = image_server_const.image_storage.get_region_movement_metadata_info(region_id,"time_path_movement_image_analysis_quantification",sql);
		update_db = true;
	}
	ofstream * o(0);
	try {
		if (im.filename.empty()) throw ns_ex("Encountered a blank filename!");
		if (write_options == ns_write_data)
			o = image_server_const.image_storage.request_metadata_output(im, ns_csv, false, &sql);
	}
	catch (ns_ex & ex) {
		//if there's a problem create a new entry in the db to halt propigation of db errors
		if (id_options == ns_require_existing_record)
			throw ns_ex("ns_time_path_image_movement_analyzer::obtain_analysis_id_and_save_movement_data()::Cannot write to existing movement analysis record");
		im = ns_image_server_image();
		im = image_server_const.image_storage.get_region_movement_metadata_info(region_id, "time_path_movement_image_analysis_quantification", sql);
		if (im.filename.empty()) throw ns_ex("Encountered a blank filename!");
		im.save_to_db(im.id, &sql);
		update_db = true;

		if (write_options == ns_write_data)
			o = image_server_const.image_storage.request_metadata_output(im, ns_csv, false, &sql);
	}
	//set analysis id that will uniquely identify all annotations generated by this analysis
	if (update_db)
		im.save_to_db(im.id, &sql);
	analysis_id = im.id;
	try{
		if (write_options == ns_write_data) {
			save_movement_data_to_disk(*o);
			o->close();
		}
		ns_safe_delete(o);
	}
	catch(...){
		ns_safe_delete(o);
		throw;
	}
	if (update_db){
		sql << "UPDATE sample_region_image_info SET movement_image_analysis_quantification_id = " << im.id << " WHERE id = " << region_id;
		sql.send_query();
	}

}


void ns_time_path_image_movement_analyzer::load_movement_data_from_disk(istream & in, bool skip_movement_data){

	ns_get_int get_int;
	ns_get_double get_double;
	get_int(in,this->analysis_id);
	if (in.fail())
		throw ns_ex("Empty Specification!");
	if (skip_movement_data)
		return;
		
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++){
				groups[i].paths[j].elements[k].absolute_time = 0;
			}
		}
	}

	while(true){
		unsigned long group_id,path_id,element_id;
		get_int(in,group_id); //1
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
		if(in.fail()) throw ns_ex("Invalid Specification 4");																										
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.interframe_time_scaled_movement_sum);//5										
		if(in.fail()) throw ns_ex("Invalid Specification 5");																										
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.movement_alternate_worm_sum);//6												
		if(in.fail()) throw ns_ex("Invalid Specification 6");																										
		get_double(in,groups[group_id].paths[path_id].elements[element_id].measurements.change_in_total_region_intensity);//7										
		if(in.fail()) throw ns_ex("Invalid Specification 7");																										
		get_double(in,groups[group_id].paths[path_id].elements[element_id].measurements.change_in_total_foreground_intensity);//8								  
		if(in.fail()) throw ns_ex("Invalid Specification 8");																										
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.total_foreground_area);//9														
		if(in.fail()) throw ns_ex("Invalid Specification 9");																										
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.total_intensity_within_foreground);//10										
		if(in.fail()) throw ns_ex("Invalid Specification 10");																										
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.total_region_area);//11														
		if(in.fail()) throw ns_ex("Invalid Specification 11");																										
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.total_intensity_within_region);//12
		if(in.fail()) throw ns_ex("Invalid Specification 12");
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.total_alternate_worm_area);//13
		if(in.fail()) throw ns_ex("Invalid Specification 13");
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.total_intensity_within_alternate_worm);//14
		if(in.fail()) throw ns_ex("Invalid Specification 14");
		int t;
		get_int(in,t);
		if(in.fail()) throw ns_ex("Invalid Specification 15");
		groups[group_id].paths[path_id].elements[element_id].saturated_offset = (t!=0);//15

		get_double(in,groups[group_id].paths[path_id].elements[element_id].registration_offset.x);//16
		if(in.fail()) 
			throw ns_ex("Invalid Specification 15");
		get_double(in,groups[group_id].paths[path_id].elements[element_id].registration_offset.y);//17
		if(in.fail()) 
			throw ns_ex("Invalid Specification 17");
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.movement_sum);//18
		if(in.fail()) 
			throw ns_ex("Invalid Specification 18");
		get_double(in,groups[group_id].paths[path_id].elements[element_id].measurements.denoised_movement_score);//19
		if(in.fail()) 
			throw ns_ex("Invalid Specification 19");
		get_double(in,groups[group_id].paths[path_id].elements[element_id].measurements.movement_score);//20
		if(in.fail()) 
			throw ns_ex("Invalid Specification 20");
		get_int(in,groups[group_id].paths[path_id].elements[element_id].measurements.total_intensity_within_stabilized);//21
		if(in.fail()) 
			throw ns_ex("Invalid Specification 21");
		get_double(in, groups[group_id].paths[path_id].elements[element_id].measurements.spatial_averaged_movement_sum);//22
		if (in.fail()) 
			throw ns_ex("Invalid Specification 22");
		get_double(in, groups[group_id].paths[path_id].elements[element_id].measurements.interframe_scaled_spatial_averaged_movement_sum);//23
		if (in.fail()) 
			throw ns_ex("Invalid Specification 23");
		get_double(in, groups[group_id].paths[path_id].elements[element_id].measurements.spatial_averaged_movement_score);//24
		if (in.fail()) 
			throw ns_ex("Invalid Specification 24");
		get_double(in, groups[group_id].paths[path_id].elements[element_id].measurements.denoised_spatial_averaged_movement_score);//25
		if (in.fail()) 
			throw ns_ex("Invalid Specification 25");
		get_int(in, groups[group_id].paths[path_id].elements[element_id].measurements.total_intensity_in_previous_frame_scaled_to_current_frames_histogram);//26
		if (in.fail()) 
			throw ns_ex("Invalid Specification 26");
		get_int(in, groups[group_id].paths[path_id].elements[element_id].measurements.total_stabilized_area);//27
		if (in.fail())
			throw ns_ex("Invalid Specification 27");
		get_int(in, groups[group_id].paths[path_id].elements[element_id].measurements.change_in_total_stabilized_intensity);//28
		if (in.fail())
			throw ns_ex("Invalid Specification 28");
		string tmp;
		char a = get_int(in, tmp);
		if (in.fail()) 
			throw ns_ex("Invalid Specification 29");
		if (a == '\n') {
			//old style records
	#ifdef NS_CALCULATE_OPTICAL_FLOW
			groups[group_id].paths[path_id].elements[element_id].measurements.raw_flow_magnitude.zero();
			groups[group_id].paths[path_id].elements[element_id].measurements.scaled_flow_dx.zero();
			groups[group_id].paths[path_id].elements[element_id].measurements.scaled_flow_dy.zero();
			groups[group_id].paths[path_id].elements[element_id].measurements.raw_flow_dx.zero();
			groups[group_id].paths[path_id].elements[element_id].measurements.raw_flow_dy.zero();
#endif
			continue;
		}
		if (!tmp.empty()) {
			//throw ns_ex("Badly formed input file!");
			cerr << "Badly formed input file: " << tmp << "\n";
		}
		#ifdef NS_CALCULATE_OPTICAL_FLOW
		groups[group_id].paths[path_id].elements[element_id].measurements.scaled_flow_magnitude.read(in);
		groups[group_id].paths[path_id].elements[element_id].measurements.raw_flow_magnitude.read(in);
		groups[group_id].paths[path_id].elements[element_id].measurements.scaled_flow_dx.read(in);
		groups[group_id].paths[path_id].elements[element_id].measurements.scaled_flow_dy.read(in);
		groups[group_id].paths[path_id].elements[element_id].measurements.raw_flow_dx.read(in);
		groups[group_id].paths[path_id].elements[element_id].measurements.raw_flow_dy.read(in);
	/*
		for (unsigned int i = 0; i < 6; i++)
		  for (unsigned int j = 0; j < 6; j++){
		    get_int(in,tmp);
		    if (in.fail()) throw ns_ex("Invalid Specification") << 27 +6*i+j;
		  }*/
#endif

		//open for future use
		for (unsigned int i = 0; i < 9; i++){
			char a = get_int(in,tmp);
			if (a == '\n')
				break;
	//		std::cerr << "E" << i << "'" << tmp << "' ";
				if(in.fail() || !tmp.empty())
					throw ns_ex("Invalid Specification");
		}
	//	std::cerr << "\n";

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
void ns_time_path_image_movement_analyzer::get_processing_stats_from_solution(const ns_time_path_solution & solution_){
	number_of_timepoints_in_analysis_ = solution_.timepoints.size();
	
	last_timepoint_in_analysis_ = 0;
	for (unsigned int i = 0; i < solution_.timepoints.size(); i++){
		if (solution_.timepoints[i].time > last_timepoint_in_analysis_)
			last_timepoint_in_analysis_ = solution_.timepoints[i].time;
	}

}







void ns_time_path_image_movement_analyzer::save_movement_data_to_disk(ostream & o) const{
	o << this->analysis_id << "\n";
	for (unsigned long i = 0; i < groups.size(); i++){
		for (unsigned long j = 0; j < groups[i].paths.size(); j++){
			for (unsigned long k = 0; k < groups[i].paths[j].elements.size(); k++){
				o << i << ","//1
					<< j << ","//2
					<< k << ","//3
					<< groups[i].paths[j].elements[k].absolute_time << ","//4 
					<< groups[i].paths[j].elements[k].measurements.interframe_time_scaled_movement_sum << ","//5
					<< groups[i].paths[j].elements[k].measurements.movement_alternate_worm_sum << ","//6
					<< groups[i].paths[j].elements[k].measurements.change_in_total_region_intensity << ","//7
					<< groups[i].paths[j].elements[k].measurements.change_in_total_foreground_intensity << ","//8
					<< groups[i].paths[j].elements[k].measurements.total_foreground_area << ","//9
					<< groups[i].paths[j].elements[k].measurements.total_intensity_within_foreground << ","//10
					<< groups[i].paths[j].elements[k].measurements.total_region_area << ","//11
					<< groups[i].paths[j].elements[k].measurements.total_intensity_within_region << ","//12
					<< groups[i].paths[j].elements[k].measurements.total_alternate_worm_area << ","//13
					<< groups[i].paths[j].elements[k].measurements.total_intensity_within_alternate_worm << ","//14
					<< (groups[i].paths[j].elements[k].saturated_offset ? "1" : "0") << ","//15
					<< groups[i].paths[j].elements[k].registration_offset.x << ","//16
					<< groups[i].paths[j].elements[k].registration_offset.y << ","//17
					<< groups[i].paths[j].elements[k].measurements.movement_sum << ","//18
					<< groups[i].paths[j].elements[k].measurements.denoised_movement_score << ","//19
					<< groups[i].paths[j].elements[k].measurements.movement_score << ","//20
					<< groups[i].paths[j].elements[k].measurements.total_intensity_within_stabilized << ","//21
					<< groups[i].paths[j].elements[k].measurements.spatial_averaged_movement_sum << ","//22
					<< groups[i].paths[j].elements[k].measurements.interframe_scaled_spatial_averaged_movement_sum << ","//23
					<< groups[i].paths[j].elements[k].measurements.spatial_averaged_movement_score << ","//24
					<< groups[i].paths[j].elements[k].measurements.denoised_spatial_averaged_movement_score << ","//25
					<< groups[i].paths[j].elements[k].measurements.total_intensity_in_previous_frame_scaled_to_current_frames_histogram << ","//26
					<< groups[i].paths[j].elements[k].measurements.total_stabilized_area << ","//27
					<< groups[i].paths[j].elements[k].measurements.change_in_total_stabilized_intensity;//28

			    #ifdef NS_CALCULATE_OPTICAL_FLOW
				o << ","; //deliberately left blank to mark old record format
       				groups[i].paths[j].elements[k].measurements.scaled_flow_magnitude.write(o); o << ",";
				groups[i].paths[j].elements[k].measurements.raw_flow_magnitude.write(o); o << ",";
				groups[i].paths[j].elements[k].measurements.scaled_flow_dx.write(o); o << ",";
				groups[i].paths[j].elements[k].measurements.scaled_flow_dy.write(o); o << ",";
				groups[i].paths[j].elements[k].measurements.raw_flow_dx.write(o); o << ",";
				groups[i].paths[j].elements[k].measurements.raw_flow_dy.write(o);
				#endif
				for (unsigned int i = 0; i < 9; i++)
					o << ",";


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


void ns_analyzed_image_time_path::write_posture_analysis_optimization_data_header(std::ostream & o){
	o << "Experiment,Device,Plate Name,Animal Details,Group ID,Path ID,Excluded,Censored,Number of Worms in Clump,"
		"Movement Threshold, Min Hold Time (Hours), Denoising Technique Used, "
		"Visual Inspection Death Age (Days),Machine Death Age (Days), Visual Inspection Death Time (Date), Difference Between Machine and By Hand Death Times (Days), Sqrt(Difference Squared) (Days), Random Group";
}

std::vector< std::vector < unsigned long > > static_messy_death_time_matrix;
 void ns_analyzed_image_time_path::write_posture_analysis_optimization_data(int software_version_number,const ns_stationary_path_id & id, const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m,const ns_time_series_denoising_parameters & denoising_parameters,std::ostream & o, ns_parameter_optimization_results & results) const{

	unsigned long death_time(by_hand_annotation_event_times[(int)ns_movement_cessation].period_end);
	if (by_hand_annotation_event_times[(int)ns_movement_cessation].fully_unbounded())
		return;
	results.number_valid_worms++;
	calculate_posture_analysis_optimization_data(thresholds,hold_times,static_messy_death_time_matrix, software_version_number);
	const unsigned long random_group(rand()%2);
	for (unsigned int i = 0; i < thresholds.size(); i++)
		for (unsigned int j = 0; j < hold_times.size(); j++){

			const double err(((double)static_messy_death_time_matrix[i][j] - death_time)/(60.0*60.0*24.0));
			const double err_sq = err*err;
			results.death_total_mean_square_error_in_hours[i][j] += err_sq;
			results.counts[i][j] ++;

			o << m.experiment_name << "," << m.device << "," << m.plate_name() << "," << m.plate_type_summary() 
				<< "," << id.group_id << "," << id.path_id << ","
				<< (censoring_and_flag_details.is_excluded()?"1":"0") << ","
				<< (censoring_and_flag_details.is_censored()?"1":"0") << ","
				<< censoring_and_flag_details.number_of_worms() << ","
				<< thresholds[i] << "," << (hold_times[j])/60.0/60.0 << "," 
				<< denoising_parameters.to_string() << ","
				<< (death_time - m.time_at_which_animals_had_zero_age)/(60.0*60.0*24)  << ","
				<< (static_messy_death_time_matrix[i][j] - m.time_at_which_animals_had_zero_age)/(60.0*60.0*24)  << ","
				<< death_time << ","
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
 void ns_analyzed_image_time_path::write_expansion_analysis_optimization_data(const ns_stationary_path_id & id, const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m,  std::ostream & o, ns_parameter_optimization_results & results) const {
	
	 if (by_hand_annotation_event_times[(int)ns_movement_cessation].fully_unbounded() ||
		 by_hand_annotation_event_times[(int)ns_death_posture_relaxation_start].fully_unbounded() ||
		 by_hand_annotation_event_times[(int)ns_death_posture_relaxation_termination].fully_unbounded())
		 return;

	const unsigned long death_time(by_hand_annotation_event_times[(int)ns_movement_cessation].period_end);
	
	const unsigned long time_at_which_death_time_expansion_started = by_hand_annotation_event_times[(int)ns_death_posture_relaxation_start].period_end,
				   time_at_which_death_time_expansion_stopped = by_hand_annotation_event_times[(int)ns_death_posture_relaxation_termination].period_end;
	 results.number_valid_worms++;
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
void ns_analyzed_image_time_path::calculate_posture_analysis_optimization_data(const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, std::vector< std::vector < unsigned long > > & death_times, int software_version) const{
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

	death_times.resize(0);
	death_times.resize(thresholds.size(),std::vector<unsigned long>(hold_times.size(),0));

	for (long t = start_i; t < elements.size(); t++){
		
		if (elements[t].excluded) continue;

		double r((software_version==1)?elements[t].measurements.death_time_posture_analysis_measure_v1():
									   elements[t].measurements.death_time_posture_analysis_measure_v2());
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

void ns_analyzed_image_time_path::calculate_movement_quantification_summary(){

	quantification_summary.mean_before_death.zero();
	quantification_summary.mean_after_death.zero();
	quantification_summary.variability_all.zero();
	quantification_summary.variability_before_death.zero();
	quantification_summary.variability_after_death.zero();
	quantification_summary.mean_all.zero();
	quantification_summary.count_after_death = 0;
	quantification_summary.count_before_death = 0;
	quantification_summary.count_all = 0;
	quantification_summary.number_of_registration_saturated_frames_before_death = 0;
	quantification_summary.number_of_registration_saturated_frames_after_death = 0;
	for (unsigned int i  = 0; i < elements.size(); i++)
		elements[i].measurements.registration_displacement = elements[i].registration_offset;

	const long death_index((!state_intervals[(int)ns_movement_stationary].skipped)?
		state_intervals[(int)ns_movement_stationary].entrance_interval.period_end_index:(-1));
	if (death_index != -1){
		for (unsigned int i = 0; i <= death_index; i++){
			quantification_summary.count_before_death++;
		//	elements[i].measurements.calculate_means();
			quantification_summary.mean_before_death = quantification_summary.mean_before_death + elements[i].measurements;
			if (elements[i].saturated_offset)
				quantification_summary.number_of_registration_saturated_frames_before_death++;
		}
	}	
	for (unsigned int i = death_index+1; i < elements.size();i++){
	quantification_summary.count_after_death++;
	//	elements[i].measurements.calculate_means();
		quantification_summary.mean_after_death = quantification_summary.mean_after_death + elements[i].measurements;
		if (elements[i].saturated_offset)
			quantification_summary.number_of_registration_saturated_frames_after_death++;
	}
	
	quantification_summary.count_all = quantification_summary.count_before_death + quantification_summary.count_after_death;
	quantification_summary.mean_all = quantification_summary.mean_before_death + quantification_summary.mean_after_death;
	
	if (quantification_summary.count_all != 0) 
		quantification_summary.mean_all = quantification_summary.mean_all / quantification_summary.count_all;
	if (quantification_summary.count_before_death != 0) 
		quantification_summary.mean_before_death = quantification_summary.mean_before_death / quantification_summary.count_before_death;
	if (quantification_summary.count_after_death != 0) 
		quantification_summary.mean_after_death = quantification_summary.mean_after_death / quantification_summary.count_after_death;

	if (death_index != -1){
		for (unsigned int i = 0; i <= death_index; i++){
			
			ns_analyzed_image_time_path_element_measurements dif((elements[i].measurements + quantification_summary.mean_before_death/-1));
			dif.square();
			quantification_summary.variability_before_death = quantification_summary.variability_before_death + dif;

			dif = (elements[i].measurements + quantification_summary.mean_all/-1);
			dif.square();
			quantification_summary.variability_all = quantification_summary.variability_all + dif;
		}
	}	
	for (unsigned int i = death_index+1; i < elements.size();i++){
		ns_analyzed_image_time_path_element_measurements dif((elements[i].measurements + quantification_summary.mean_after_death/-1));
		dif.square();
		quantification_summary.variability_after_death = quantification_summary.variability_after_death + dif;

		dif = (elements[i].measurements + quantification_summary.mean_all/-1);
		dif.square();
		quantification_summary.variability_all = quantification_summary.variability_all + dif;
	}

	quantification_summary.variability_after_death.square_root();
	quantification_summary.variability_before_death.square_root();
	quantification_summary.variability_all.square_root();

	if (quantification_summary.count_all != 0) 
		quantification_summary.variability_all = quantification_summary.variability_all / quantification_summary.count_all;
	if (quantification_summary.count_before_death != 0) 
		quantification_summary.variability_before_death = quantification_summary.variability_before_death / quantification_summary.count_before_death;
	if (quantification_summary.count_after_death != 0) 
		quantification_summary.variability_after_death = quantification_summary.variability_after_death / quantification_summary.count_after_death;
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
		"Machine Death Relative Time, Machine Slow Movement Cessation Relative Time, Machine Fast Movement Cessation Relative Time,Machine Death-Associated Expansion Time,"
		"By Hand Death Relative Time, By Hand Slow Movement Cessation Relative Time, By Hand Fast Movement Cessation Relative Time,By Hand Death-Associated Expansion Time,"
		"Movement Sum, Movement Score, Denoised Movement Score,"
		"Spatially Averaged Movement Sum,"
		"Spatially Averaged Movement Score,"
		"Denoised Spatially Averaged Movement Score,"
		"Movement quantification used to identify death,"
		"Movement Alternate Worm Sum,"
		"Total Foreground Area, Total Stabilized Area, Total Region Area,Total Alternate Worm Area,"
		"Total Foreground Intensity, Total Stabilized Intensity,Total Region Intensity,Total Alternate Worm Intensity,"
		"Change in Foreground Intensity (pix/hour),Change in Stabilized Intensity (pix/hour),Change in Region Intensity (pix/hour),"
		"Saturated Registration, Machine Error (Days ),"
		"Death Time,"
		"Total Foreground Area at Death,"
		"Total Stablilized Intensity at Death,";

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
void ns_analyzed_image_time_path::write_detailed_movement_quantification_analysis_data(const ns_region_metadata & m, const unsigned long group_id, const unsigned long path_id, std::ostream & o, const bool output_only_elements_with_hand,const bool abbreviated_time_series)const{
	if (output_only_elements_with_hand && by_hand_annotation_event_times[(int)ns_movement_cessation].period_end_was_not_observed)
		return;
	//find animal size one day before death
	//double death_relative_time_to_normalize(0);
	double time_after_death_to_write_in_abbreviated(24*60*60);
	unsigned long end_index(0);
	unsigned long death_index(0);
	
	unsigned long death_time(0);
	if(!state_intervals[(int)ns_movement_stationary].skipped){
		long least_dt_to_death(LONG_MAX);
		//double size_at_closest_distance(1);
		if (by_hand_annotation_event_times[(int)ns_movement_stationary].period_end_was_not_observed)
			death_time = state_entrance_interval_time(state_intervals[(int)ns_movement_stationary]).best_estimate_event_time_for_possible_partially_unbounded_interval();
		else 
			death_time = by_hand_annotation_event_times[(int)ns_movement_stationary].period_end;
		
		const double time_to_match_2(death_time+time_after_death_to_write_in_abbreviated);

		for (unsigned long k = 0; k < elements.size(); k++){
	
			const double dt(fabs(elements[k].absolute_time- death_time));
			if (dt < least_dt_to_death){
				least_dt_to_death = dt;
				death_index = k;
			}
		}
		
		if (abbreviated_time_series){
			for (end_index = 0; end_index < elements.size(); end_index++){
				if (elements[end_index].absolute_time >= time_to_match_2)
					break;
			}
		}
	}
	
	if (!abbreviated_time_series)
		end_index = elements.size();

	for (unsigned long k = 1; k < end_index; k++){
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
			<< ns_calc_rel_time_by_index(elements[k].absolute_time,state_intervals[(int)ns_movement_stationary],*this) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time,state_intervals[(int)ns_movement_posture],*this) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time,state_intervals[(int)ns_movement_slow],*this) << ","
			<< ns_calc_rel_time_by_index(elements[k].absolute_time, state_intervals[(int)ns_movement_death_posture_relaxation], *this) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_movement_cessation]) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_translation_cessation]) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_fast_movement_cessation]) << ","
			<< ns_output_interval_difference(elements[k].absolute_time,by_hand_annotation_event_times[(int)ns_death_posture_relaxation_termination]) << ",";

		
			o << elements[k].measurements.interframe_time_scaled_movement_sum << ",";                                                               	
			o << elements[k].measurements.movement_score << ","
				<< elements[k].measurements.denoised_movement_score << ","
				<< elements[k].measurements.interframe_scaled_spatial_averaged_movement_sum << ","
				<< elements[k].measurements.spatial_averaged_movement_score << ","
				<< elements[k].measurements.denoised_spatial_averaged_movement_score << ","
				<< elements[k].measurements.death_time_posture_analysis_measure_v2() << ","
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
				<< elements[k].measurements.change_in_total_stabilized_intensity << ","
				<< elements[k].measurements.change_in_total_region_intensity << ","
			
			
		
			<< (elements[k].saturated_offset ? "1" : "0") << ",";
		if ( state_intervals[(int)ns_movement_stationary].skipped)
			 o << ",";
		else o << ns_output_interval_difference(this->state_entrance_interval_time(state_intervals[(int)ns_movement_stationary]).
													best_estimate_event_time_for_possible_partially_unbounded_interval(),
													by_hand_annotation_event_times[(int)ns_movement_cessation]) <<",";

		o << death_time << ",";
		if (state_intervals[(int)ns_movement_stationary].skipped)
			o << ",,";
		else o << elements[death_index].measurements.total_foreground_area << ","
			<< elements[death_index].measurements.total_stabilized_area << ",";

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


void ns_time_path_image_movement_analyzer::write_posture_analysis_optimization_data(int software_version,const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m,std::ostream & o, ns_parameter_optimization_results & results) const{
	srand(0);
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){	
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path() || groups[i].paths[j].excluded() || !groups[i].paths[j].by_hand_data_specified())
					continue;
			groups[i].paths[j].write_posture_analysis_optimization_data(software_version,generate_stationary_path_id(i,j),thresholds,hold_times,m,denoising_parameters_used,o, results);
		}
	}
}

void ns_time_path_image_movement_analyzer::write_expansion_analysis_optimization_data(const std::vector<double> & thresholds, const std::vector<unsigned long> & hold_times, const ns_region_metadata & m, std::ostream & o, ns_parameter_optimization_results & results) const {
	for (unsigned int i = 0; i < groups.size(); i++) {
		for (unsigned int j = 0; j < groups[i].paths.size(); j++) {
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path() || groups[i].paths[j].excluded() || !groups[i].paths[j].by_hand_data_specified())
				continue;
			groups[i].paths[j].write_expansion_analysis_optimization_data(generate_stationary_path_id(i, j), thresholds, hold_times, m,o, results);
		}
	}
}

void ns_time_path_image_movement_analyzer::write_detailed_movement_quantification_analysis_data(const ns_region_metadata & m, std::ostream & o, const bool only_output_elements_with_by_hand_data, const long specific_animal_id, const bool abbreviated_time_series)const{
	 
	for (unsigned long i = 0; i < groups.size(); i++){
		for (unsigned long j = 0; j < groups[i].paths.size(); j++){
			if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path() ||
				specific_animal_id != -1 && i != specific_animal_id || groups[i].paths[j].entirely_excluded)
				continue;
			if (groups[i].paths[j].state_intervals.size() != ns_movement_number_of_states)
				throw ns_ex("ns_time_path_image_movement_analyzer::write_movement_quantification_analysis_data()::Event Indicies not loaded properly!");
			groups[i].paths[j].write_detailed_movement_quantification_analysis_data(m,i,j,o,only_output_elements_with_by_hand_data,abbreviated_time_series);
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
		movement_ratios.y[i] = path.element(i).measurements.denoised_spatial_averaged_movement_score;
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
	set.add(death_time_annotation_set);
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
	ns_movement_state_observation_boundary_interval & slow_moving_interval_including_missed_states,
	ns_movement_state_observation_boundary_interval & posture_changing_interval_including_missed_states,
	ns_movement_state_observation_boundary_interval & dead_interval_including_missed_states,
	ns_movement_state_observation_boundary_interval & expansion_interval_including_missed_states) {

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
void ns_analyzed_image_time_path::detect_death_times_and_generate_annotations_from_movement_quantification(const ns_stationary_path_id & path_id, const ns_analyzed_image_time_path_death_time_estimator * movement_death_time_estimator,ns_death_time_annotation_set & set, const unsigned long last_time_point_in_analysis){
	
	//if (path_id.group_id == 5)
	//	cerr << "RA";
	state_intervals.clear();
	state_intervals.resize((int)ns_movement_number_of_states);
	by_hand_annotation_event_times.resize((int)ns_number_of_movement_event_types,ns_death_time_annotation_time_interval::unobserved_interval());

	unsigned long current_time(ns_current_time());
	set.clear();
	if (elements.size() == 0) return;

	first_valid_element_id = ns_movement_state_time_interval_indicies(elements.size(), elements.size());
	last_valid_element_id = ns_movement_state_time_interval_indicies(0,0);
	for (unsigned int i = 0; i < elements.size(); i++)
		if (!elements[i].excluded && !elements[i].element_before_fast_movement_cessation){
			//find first measurment
			if (first_valid_element_id.period_start_index >= i)
				first_valid_element_id.period_start_index = i;
			else
				//find the second measurement
			if (first_valid_element_id.period_end_index > i)
				first_valid_element_id.period_end_index = i;

			//find the last measurement
			if (last_valid_element_id.period_end_index < i){
				//set the measurement previous to the last measurement
				last_valid_element_id.period_start_index =
					last_valid_element_id.period_end_index;
				last_valid_element_id.period_end_index = i;
			}
		}
		if (first_valid_element_id.period_start_index+1 >= elements.size())
		return;

	//if the experiment is cropped before the current path ends, then we don't add a stationary worm is lost event.
	//unsigned long actual_time_of_first_measurement_after_path_end(time_path_limits.interval_after_last_observation.period_end);
	//if (last_valid_element_id.period_end+1 < elements.size())
	//	actual_time_of_first_measurement_after_path_end = 0;

	//if the path has been declared as not having enough information
	//for annotation, register it as so.
	if (this->is_low_density_path()){
		unsigned long end_index(first_valid_element_id.period_end_index);
		if (end_index > elements.size())
			throw ns_ex("Invalid end index");
		if (end_index == elements.size())
			end_index = elements.size()-1;
		set.add(ns_death_time_annotation(ns_no_movement_event,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[first_valid_element_id.period_start_index].absolute_time,
													elements[end_index].absolute_time),
			elements[end_index].region_offset_in_source_image(),
			elements[end_index].worm_region_size(),
			ns_death_time_annotation::ns_not_excluded,			//filling in the gaps of these things work really well! Let the user exclude them in the worm browser
			ns_death_time_annotation_event_count(1+elements[end_index].number_of_extra_worms_observed_at_position,0),
			current_time,ns_death_time_annotation::ns_lifespan_machine,
			(elements[end_index].part_of_a_multiple_worm_disambiguation_group)?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,true,elements[end_index].inferred_animal_location,elements[end_index].subregion_info,"low_density"));
	}
	ns_time_path_posture_movement_solution movement_state_solution(movement_death_time_estimator->operator()(this,true));

	//Some movement detection algorithms need a significant amount of time after an animal has died
	//to detect its death.  So, if we are going to censor an animal at the end of the experiment,
	//we need to ask the movement detection about the latest time such an event can occur.
	const unsigned long last_possible_death_time(movement_death_time_estimator->latest_possible_death_time(this,last_time_point_in_analysis));


	const double loglikelihood_of_solution(movement_state_solution.loglikelihood_of_solution);
	const string reason_to_be_censored(movement_state_solution.reason_for_animal_to_be_censored);

	ns_movement_state_observation_boundary_interval slow_moving_interval_including_missed_states,
		posture_changing_interval_including_missed_states,
		dead_interval_including_missed_states,
		expansion_interval_including_missed_states;

	slow_moving_interval_including_missed_states.longest_observation_gap_within_interval = movement_state_solution.slowing.longest_observation_gap_within_interval;
	posture_changing_interval_including_missed_states.longest_observation_gap_within_interval = movement_state_solution.moving.longest_observation_gap_within_interval;
	dead_interval_including_missed_states.longest_observation_gap_within_interval = movement_state_solution.dead.longest_observation_gap_within_interval;
	expansion_interval_including_missed_states.longest_observation_gap_within_interval = 0;
	
	unsigned long longest_skipped_interval_before_death = slow_moving_interval_including_missed_states.longest_observation_gap_within_interval;
	if (longest_skipped_interval_before_death < posture_changing_interval_including_missed_states.longest_observation_gap_within_interval)
		longest_skipped_interval_before_death = posture_changing_interval_including_missed_states.longest_observation_gap_within_interval;	
	
	{
		ns_movement_state_observation_boundary_interval slow_moving_interval,
														posture_changing_interval,
														dead_interval,
														expansion_interval;

		//the movement detection algorithms give us the first frame in which the animal was observed in each state.
		//We need to conver these into *intervals* between frames during which the transition occurred.
		//This is because events may have occurred at any time during the interval and we don't want to assume
		//the event occurred at any specific time (yet!)
		//We'll resolve this observational ambiguity later.
	
		ns_movement_state_time_interval_indicies frame_before_first = calc_frame_before_first(first_valid_element_id);

		ns_movement_state_time_interval_indicies frame_after_last(elements.size()-1,elements.size());
		frame_after_last.interval_occurs_after_observation_interval = true;


		if (movement_state_solution.moving.skipped){
			if (movement_state_solution.slowing.skipped &&
				movement_state_solution.dead.skipped)
				throw ns_ex("ns_analyzed_image_time_path::detect_death_times_and_generate_annotations_from_movement_quantification()::Movement death time estimator skipped all states!");
			//if we skip slow moving, that means we start changing posture or death, which means
			//we transition out of fast movement before the first frame of the path
			slow_moving_interval.skipped = true;
		}
		else{
			slow_moving_interval.skipped = false;
			slow_moving_interval.entrance_interval = frame_before_first;
			//if all following states are skipped, this state continues past the end of the observation interval
			if (movement_state_solution.slowing.skipped && movement_state_solution.dead.skipped)
				slow_moving_interval.exit_interval = frame_after_last;
			else{
				slow_moving_interval.exit_interval.period_end_index = movement_state_solution.moving.end_index;
				slow_moving_interval.exit_interval.period_start_index = ns_find_last_valid_observation_index(movement_state_solution.moving.end_index,elements);
				if (slow_moving_interval.exit_interval.period_start_index == -1) slow_moving_interval.exit_interval = frame_before_first;
			}
		}
		if (movement_state_solution.slowing.skipped){
			posture_changing_interval.skipped = true;
		}
		else{
			posture_changing_interval.skipped = false;
			//if the animal is never observed to be slow moving, it has been changing posture since before the observation interval
			if (slow_moving_interval.skipped)
				posture_changing_interval.entrance_interval = frame_before_first;
			else 
				posture_changing_interval.entrance_interval = slow_moving_interval.exit_interval;
			//if the animal never dies, it continues to change posture until the end of observation interval
			if (movement_state_solution.dead.skipped)
				posture_changing_interval.exit_interval	= frame_after_last;
			else{
				posture_changing_interval.exit_interval.period_end_index = movement_state_solution.slowing.end_index;
				posture_changing_interval.exit_interval.period_start_index = ns_find_last_valid_observation_index(movement_state_solution.slowing.end_index,elements); 
				if (posture_changing_interval.exit_interval.period_start_index == -1) posture_changing_interval.exit_interval = frame_before_first;
			}
		}
		if (movement_state_solution.dead.skipped){
			dead_interval.skipped = true;
		}
		else{
			dead_interval.skipped = false;
			if (posture_changing_interval.skipped && slow_moving_interval.skipped)
				dead_interval.entrance_interval = frame_before_first;
			else if (posture_changing_interval.skipped)
				dead_interval.entrance_interval = slow_moving_interval.exit_interval;
			else
				dead_interval.entrance_interval = posture_changing_interval.exit_interval;

			dead_interval.exit_interval = frame_after_last;
		}
		if (movement_state_solution.expanding.skipped)
			expansion_interval.skipped = true;
		else {
			expansion_interval.skipped = false;

			expansion_interval.entrance_interval.period_end_index = movement_state_solution.expanding.start_index;
			expansion_interval.entrance_interval.period_start_index = ns_find_last_valid_observation_index(movement_state_solution.expanding.start_index, elements);
			if (expansion_interval.entrance_interval.period_start_index == -1)expansion_interval.entrance_interval = frame_before_first;


			expansion_interval.exit_interval.period_end_index = movement_state_solution.expanding.end_index;
			expansion_interval.exit_interval.period_start_index = ns_find_last_valid_observation_index(movement_state_solution.expanding.end_index, elements);
			if (expansion_interval.exit_interval.period_start_index == -1)expansion_interval.exit_interval = frame_before_first;
		}
	
		state_intervals[ns_movement_slow] = slow_moving_interval;
		state_intervals[ns_movement_posture] = posture_changing_interval;
		state_intervals[ns_movement_stationary] = dead_interval;
		state_intervals[ns_movement_death_posture_relaxation] = expansion_interval;

	
		//ok we have the correct state intervals.  BUT we need to change them around because we know that animals have to pass through 
		//fast to slow movement to posture, and posture to death.  If the movement detection algorithms didn't find any states
		//that's because the animals went through too quickly.  So we make *new* intervals for this purpose with those assumptions in mind
		calculate_state_transitions_in_the_presence_of_missing_states(
			frame_before_first,
			slow_moving_interval,
			posture_changing_interval,
			dead_interval,
			expansion_interval,
			slow_moving_interval_including_missed_states,
			posture_changing_interval_including_missed_states,
			dead_interval_including_missed_states,
			expansion_interval_including_missed_states);
	}

	//if the path has extra worms at least 25% of the points leading up to it's death
		//mark the path as containing that extra worm
	unsigned long number_of_extra_worms_in_path(0),
		total_observations(0);
	bool part_of_a_multiple_worm_disambiguation_group(false);
	{
		unsigned long stop_i(last_valid_element_id.period_end_index);
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
	
	//if (path_id.group_id == 28)
	//	cerr << "MA";
	ns_death_time_annotation::ns_exclusion_type exclusion_type(ns_death_time_annotation::ns_not_excluded);
	if (!reason_to_be_censored.empty()){
		exclusion_type = ns_death_time_annotation::ns_censored;
	}
	set.add(
		ns_death_time_annotation(ns_fast_movement_cessation,
		0,region_info_id,
		state_entrance_interval_time(slow_moving_interval_including_missed_states),
		elements[first_valid_element_id.period_start_index].region_offset_in_source_image(),  //register the position of the object at that time point
		elements[first_valid_element_id.period_start_index].worm_region_size(),
		exclusion_type,
		ns_death_time_annotation_event_count(1+number_of_extra_worms_in_path,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
		(part_of_a_multiple_worm_disambiguation_group)?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
		path_id,part_of_a_full_trace,elements[first_valid_element_id.period_start_index].inferred_animal_location, elements[first_valid_element_id.period_start_index].subregion_info,reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	
	//observations are made at specific times (i.e. we see a fast moving worm at this time)
	for (unsigned int i = slow_moving_interval_including_missed_states.entrance_interval.period_end_index; i < slow_moving_interval_including_missed_states.entrance_interval.period_end_index; i++){
		if (elements[i].excluded) continue;

		set.add(
			ns_death_time_annotation(ns_slow_moving_worm_observed,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[i].absolute_time,elements[i].absolute_time),
			elements[i].region_offset_in_source_image(),
			elements[i].worm_region_size(),exclusion_type,
			ns_death_time_annotation_event_count(1+elements[i].number_of_extra_worms_observed_at_position,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			elements[i].part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[i].inferred_animal_location,elements[i].subregion_info,reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death)
			);
	}

	if (!expansion_interval_including_missed_states.skipped) {
		set.add(
			ns_death_time_annotation(ns_death_posture_relaxation_start,
				0, region_info_id,
				ns_death_time_annotation_time_interval(state_entrance_interval_time(expansion_interval_including_missed_states)),
				elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
				elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
				exclusion_type,
				ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
				part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
				path_id, part_of_a_full_trace, elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location, elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].subregion_info,
				reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death));
		set.add(
			ns_death_time_annotation(ns_death_posture_relaxation_termination,
				0, region_info_id,
				ns_death_time_annotation_time_interval(state_exit_interval_time(expansion_interval_including_missed_states)),
				elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
				elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
				exclusion_type,
				ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
				part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
				path_id, part_of_a_full_trace, elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location, elements[expansion_interval_including_missed_states.entrance_interval.period_end_index].subregion_info,reason_to_be_censored, loglikelihood_of_solution, longest_skipped_interval_before_death));
	}
	
	if (posture_changing_interval_including_missed_states.skipped && dead_interval_including_missed_states.skipped)
		return;
	//ns_death_time_annotation_time_interval posture_changing_entrance_interval;
	//if animals skip slow moving, we assume they transition from fast moving to dead within one interval
	//if (posture_changing_interval.skipped){
	//	posture_changing_entrance_interval = state_entrance_interval_time(dead_interval);
	//else if (slow_moving_interval.skipped)
	//	posture_changing_entrance_interval = state_entrance_interval_time(posture_changing_interval);
	if (posture_changing_interval_including_missed_states.entrance_interval.period_start_index >
		posture_changing_interval_including_missed_states.entrance_interval.period_end_index)
		throw ns_ex("Death start interval boundaries appear to be reversed:") << posture_changing_interval_including_missed_states.entrance_interval.period_end_index << " vs "
			<< posture_changing_interval_including_missed_states.entrance_interval.period_end_index;
	if (posture_changing_interval_including_missed_states.entrance_interval.interval_occurs_after_observation_interval)
		throw ns_ex("Encountered an unskipped start interval for posture changing that occurs on the last element of a timeseries");
	if (posture_changing_interval_including_missed_states.entrance_interval.period_end_index >= elements.size())
		throw ns_ex("Encountered an unskipped start interval for posture changing with an invalid end point");
		
	set.add(
		ns_death_time_annotation(ns_translation_cessation,
		0,region_info_id,
		state_entrance_interval_time(posture_changing_interval_including_missed_states),
		elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),  //register the position of the object at that time point
		elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
		exclusion_type,
		ns_death_time_annotation_event_count(number_of_extra_worms_in_path+1,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
		part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
		path_id,part_of_a_full_trace,elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location, elements[posture_changing_interval_including_missed_states.entrance_interval.period_end_index].subregion_info,reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	
	
	for (unsigned int i = posture_changing_interval_including_missed_states.entrance_interval.period_end_index; i < posture_changing_interval_including_missed_states.exit_interval.period_end_index; i++){
		if (elements[i].excluded) continue;
		set.add(
			ns_death_time_annotation(ns_posture_changing_worm_observed,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[i].absolute_time,elements[i].absolute_time),
			elements[i].region_offset_in_source_image(),  //register the position of the object at that time point
			elements[i].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(1+elements[i].number_of_extra_worms_observed_at_position,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			elements[i].part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[i].inferred_animal_location, elements[i].subregion_info,reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
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

			set.add(
				ns_death_time_annotation(ns_movement_cessation,
					0, region_info_id,
					interval,
					elements[i1].region_offset_in_source_image(),
					elements[i1].worm_region_size(),
					ns_death_time_annotation::ns_censored_at_end_of_experiment,
					ns_death_time_annotation_event_count(1 + number_of_extra_worms_in_path, 0), current_time, ns_death_time_annotation::ns_lifespan_machine,
					part_of_a_multiple_worm_disambiguation_group ? ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster : ns_death_time_annotation::ns_single_worm,
					path_id, part_of_a_full_trace,
					elements[i1].inferred_animal_location, elements[i1].subregion_info,
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
		
	set.add(
		ns_death_time_annotation(ns_movement_cessation,
		0,region_info_id,
		ns_death_time_annotation_time_interval(state_entrance_interval_time(dead_interval_including_missed_states)),
		elements[dead_interval_including_missed_states.entrance_interval.period_end_index].region_offset_in_source_image(),
		elements[dead_interval_including_missed_states.entrance_interval.period_end_index].worm_region_size(),
		exclusion_type,
		ns_death_time_annotation_event_count(1+number_of_extra_worms_in_path,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
		part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
		path_id,part_of_a_full_trace,elements[dead_interval_including_missed_states.entrance_interval.period_end_index].inferred_animal_location,
			elements[dead_interval_including_missed_states.entrance_interval.period_end_index].subregion_info,reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	
	for (unsigned int i = dead_interval_including_missed_states.entrance_interval.period_end_index; i < dead_interval_including_missed_states.exit_interval.period_end_index; i++){
		if (elements[i].excluded) continue;
		set.add(
			ns_death_time_annotation(ns_stationary_worm_observed,
			0,region_info_id,
			ns_death_time_annotation_time_interval(elements[i].absolute_time,elements[i].absolute_time),
			elements[i].region_offset_in_source_image(),
			elements[i].worm_region_size(),
			exclusion_type,
			ns_death_time_annotation_event_count(elements[i].number_of_extra_worms_observed_at_position+1,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			elements[i].part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[i].inferred_animal_location, elements[i].subregion_info,
				reason_to_be_censored,loglikelihood_of_solution,longest_skipped_interval_before_death));
	}

	

	//if the path ends before the end of the plate's observations
	//output an annotation there.
	if (!time_path_limits.interval_after_last_observation.period_end_was_not_observed){

		set.add(
			ns_death_time_annotation(ns_stationary_worm_disappearance,
			0,region_info_id,
			time_path_limits.interval_after_last_observation,
			elements[last_valid_element_id.period_end_index].region_offset_in_source_image(),
			elements[last_valid_element_id.period_end_index].worm_region_size(),
			ns_death_time_annotation::ns_not_excluded,
			ns_death_time_annotation_event_count(1+number_of_extra_worms_in_path,0),current_time,ns_death_time_annotation::ns_lifespan_machine,
			part_of_a_multiple_worm_disambiguation_group?ns_death_time_annotation::ns_part_of_a_mutliple_worm_disambiguation_cluster:ns_death_time_annotation::ns_single_worm,
			path_id,part_of_a_full_trace,elements[last_valid_element_id.period_end_index].inferred_animal_location,
				elements[last_valid_element_id.period_end_index].subregion_info,
				"",loglikelihood_of_solution));
	}
}

void ns_analyzed_image_time_path::analyze_movement(const ns_analyzed_image_time_path_death_time_estimator * movement_death_time_estimator,const ns_stationary_path_id & path_id, const unsigned long last_timepoint_in_analysis){
	death_time_annotation_set.clear();
	
	detect_death_times_and_generate_annotations_from_movement_quantification(path_id,movement_death_time_estimator,death_time_annotation_set,last_timepoint_in_analysis);
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
		expansion_interval_including_missed_states;

	//get the relevant transition times
	ns_movement_state_time_interval_indicies frame_before_first = calc_frame_before_first(first_valid_element_id);
	calculate_state_transitions_in_the_presence_of_missing_states(
		frame_before_first,
		state_intervals[ns_movement_slow],
		state_intervals[ns_movement_posture],
		state_intervals[ns_movement_stationary],
		state_intervals[ns_movement_death_posture_relaxation],
		slow_moving_interval_including_missed_states,
		posture_changing_interval_including_missed_states,
		dead_interval_including_missed_states, expansion_interval_including_missed_states);
	
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
		return ns_movement_death_posture_relaxation;
	return ns_movement_stationary;

}
ns_movement_state ns_analyzed_image_time_path::explicitly_recognized_movement_state(const unsigned long & t) const{
	//if (this->is_not_stationary())
//		return ns_movement_fast;
	if (this->is_low_density_path())
		return ns_movement_machine_excluded;

	//ns_event_index_list::const_iterator p;
	if (ns_state_match(t,state_intervals[ns_movement_fast],*this))
		return ns_movement_fast;
	
	if (ns_state_match(t,state_intervals[ns_movement_slow],*this))
		return ns_movement_slow;

	if (ns_state_match(t,state_intervals[ns_movement_posture],*this))
		return ns_movement_posture;
	
	if (ns_state_match(t,state_intervals[ns_movement_stationary],*this))
		return ns_movement_stationary;

	if (ns_state_match(t,state_intervals[ns_movement_death_posture_relaxation],*this))
		return ns_movement_death_posture_relaxation;
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
ns_death_time_annotation_time_interval ns_analyzed_image_time_path::by_hand_death_time() const{
	if (!by_hand_annotation_event_times[ns_movement_cessation].period_end_was_not_observed)
		return by_hand_annotation_event_times[ns_movement_cessation];
	else return ns_death_time_annotation_time_interval::unobserved_interval();
}
ns_movement_state ns_analyzed_image_time_path::by_hand_movement_state( const unsigned long & t) const{
	
	if (!by_hand_annotation_event_times[ns_movement_cessation].period_end_was_not_observed &&
		t >= by_hand_annotation_event_times[ns_movement_cessation].period_end){
		if (!by_hand_annotation_event_times[ns_death_posture_relaxation_termination].period_end_was_not_observed &&
			t < by_hand_annotation_event_times[ns_death_posture_relaxation_termination].period_end)
			return ns_movement_death_posture_relaxation;
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
void ns_time_path_image_movement_analyzer::produce_death_time_annotations(ns_death_time_annotation_set & set) const{
	
	set.clear();
	for (unsigned long j = 0; j < groups.size(); j++){
		for (unsigned long k = 0; k < groups[j].paths.size(); k++){
			groups[j].paths[k].add_death_time_events_to_set(set);
		}
	}

	//add animals that are fast moving at the last time point
	set.add(extra_non_path_events);
	
}

void ns_analyzed_image_time_path_group::clear_images(){
	for (unsigned int i = 0; i < paths.size(); i++)
		for (unsigned int j = 0; j < paths[i].elements.size(); j++){
			paths[i].elements[j].clear_movement_images();
			paths[i].elements[j].clear_path_aligned_images();

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
bool ns_analyzed_image_time_path::populate_images_from_region_visualization(const unsigned long time,const ns_image_standard &region_visualization,const ns_image_standard & interpolated_region_visualization,bool just_do_a_consistancy_check, ns_analyzed_image_time_path::ns_load_type load_type){
	//region visualization is all worms detected at that time point.
	ns_image_properties path_aligned_image_image_properties(region_visualization.properties());
	if (region_visualization.properties().width == 0){
		path_aligned_image_image_properties = interpolated_region_visualization.properties();
	}
	
	//	if (group_id == 42)
	//		cerr << "WHA";
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
				e.initialize_path_aligned_images(path_aligned_image_image_properties,memory_pool->aligned_image_pool); // These allocations take a lot of time, so we pool them.  This speeds things up on machines with enough RAM to keep it all in memory.
	
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
				e.release_path_aligned_images(memory_pool->aligned_image_pool);
			}
		}
		catch (ns_ex & ex) {
			if (!just_do_a_consistancy_check && !was_previously_loaded) {
				e.release_path_aligned_images(memory_pool->aligned_image_pool);
			}
			throw ex;
		}
		catch(...){
			if (!just_do_a_consistancy_check && !was_previously_loaded){
				e.release_path_aligned_images(memory_pool->aligned_image_pool);
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

void ns_time_path_image_movement_analyzer::add_by_hand_annotations(const ns_death_time_annotation_compiler & annotations){
	ns_death_time_annotation_compiler compiler;
	//load all paths into a compiler to do the merge
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			ns_death_time_annotation a;
			a.region_info_id = region_info_id;
			a.stationary_path_id = generate_stationary_path_id(i,j);
			compiler.add(a,ns_region_metadata());
		}
	}
	//do the merge
	compiler.add(annotations);
	//now fish all the annotations back out of the compiler, and add them to the right path
	for (unsigned int i = 0; i < groups.size(); i++){
		for (unsigned int j = 0; j < groups[i].paths.size(); j++){
			ns_stationary_path_id id(generate_stationary_path_id(i,j));
			ns_death_time_annotation_compiler::ns_region_list::iterator p(compiler.regions.find(region_info_id));
			if (p == compiler.regions.end())
				throw ns_ex("ns_time_path_image_movement_analyzer::add_by_hand_annotation_event_times::Could not find region aftyer merge!");
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


void ns_analyzed_image_time_path::add_by_hand_annotations(const ns_death_time_annotation_compiler_location & l){
//	if (l.properties.number_of_worms() > 1)
//		cerr << "WHEE";
	l.properties.transfer_sticky_properties(censoring_and_flag_details);
	add_by_hand_annotations(l.annotations);
}
void ns_analyzed_image_time_path::add_by_hand_annotations(const ns_death_time_annotation_set & set){
	for (unsigned int i = 0; i < set.events.size(); i++){
		const ns_death_time_annotation & e(set.events[i]);
		if (e.type != ns_translation_cessation &&
			e.type != ns_movement_cessation &&		
			e.type != ns_fast_movement_cessation &&
			e.type != ns_death_posture_relaxation_termination &&
			e.type != ns_death_posture_relaxation_start)
			continue;

			e.transfer_sticky_properties(censoring_and_flag_details);
			if (e.type != ns_no_movement_event)
				by_hand_annotation_event_times[(int)e.type] = e.time;
	}
}

void ns_time_path_image_movement_analyzer::output_visualization(const string & base_directory) const{

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
		//XXX This is the definition of a "movement score" used by the old version of the software
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

class ns_spatially_averaged_movement_data_accessor {
public:
	ns_spatially_averaged_movement_data_accessor(ns_analyzed_image_time_path::ns_element_list & l) :elements(&l) {}
	unsigned long size()const { return elements->size(); }
	const unsigned long time(const unsigned long i) { return (*elements)[i].absolute_time; }
	double raw(const unsigned long i) {
		if (i == 0)
			return raw(1);
		if ((*elements)[i - 1].measurements.total_intensity_within_stabilized == 0)
			return 0;
		//XXX This is the definition of a "spatially averaged movement score".
		//and in the current version used  used to identify death time
		return  (*elements)[i].measurements.spatial_averaged_movement_sum;
		

	}
	double & processed(const unsigned long i) {
		return (*elements)[i].measurements.denoised_spatial_averaged_movement_score;
	}
private:
	ns_analyzed_image_time_path::ns_element_list * elements;
};
/*
class ns_intensity_data_accessor{
public:
	ns_intensity_data_accessor(ns_analyzed_image_time_path::ns_element_list & l):elements(&l){}
	unsigned long size()const {return elements->size();}
	const unsigned long time(const unsigned long i){return (*elements)[i].absolute_time;}
	double raw(const unsigned long i){
		return (*elements)[i].measurements.change_in_average_normalized_worm_intensity;
	}
	double & processed(const unsigned long i){
		return (*elements)[i].measurements.denoised_change_in_average_normalized_worm_intensity;
	}
private:
	ns_analyzed_image_time_path::ns_element_list * elements;
};

class ns_size_data_accessor{
public:
	ns_size_data_accessor(ns_analyzed_image_time_path::ns_element_list & l):elements(&l){}
	unsigned long size()const {return elements->size();}
	const unsigned long time(const unsigned long i){return (*elements)[i].absolute_time;}
	double raw(const unsigned long i){
		return (*elements)[i].measurements.total_foreground_area;
	}
	double & processed(const unsigned long i){
		return (*elements)[i].measurements.normalized_foreground_area;
	}
private:
	ns_analyzed_image_time_path::ns_element_list * elements;
};*/

template <class T>
class ns_kernel_smoother{
public:
	void ns_kernel_smooth(const unsigned int kernel_width,T & data){
		(*this)(kernel_width,data);
	}
	void operator()(const int kernel_width,T & data){
		if (data.size() == 0)
			return;
		if (kernel_width%2 == 0)
			throw ns_ex("Kernel width must be odd");
		
		if (1){
			for (unsigned int i = 0; i < kernel_width; i++)
				data.processed(i) = data.raw(i);
			for (unsigned int i = kernel_width; i < data.size()-kernel_width; i++){
				double s(0);
				for (int di = -kernel_width; di <= kernel_width; di++)
					s+= data.raw(i+di);
				s+=data.raw(i);
				data.processed(i)=s/(2*kernel_width+2);
			}
			for (unsigned int i = data.size()-kernel_width; i < data.size(); i++)
				data.processed(i) = data.raw(i);
		}
	}
};
void ns_analyzed_image_time_path::denoise_movement_series_and_calculate_intensity_slopes(const unsigned long change_time_in_seconds, const ns_time_series_denoising_parameters & times_series_denoising_parameters){
	if (elements.size() == 0)
		return;


	const bool use_kernal_smoother(true);
	if (use_kernal_smoother){
		const int kernel_width(1);
		ns_movement_data_accessor acc(elements);
		ns_spatially_averaged_movement_data_accessor acc_spatial(elements);
		

		//calculate the slope at each point
		//Here, ns_movement_data_accessor calculates the movement score and stores it.
		for (unsigned int i = 0; i < elements.size(); i++){
			elements[i].measurements.movement_score = acc.raw(i);
			elements[i].measurements.spatial_averaged_movement_score = acc_spatial.raw(i);
		//	if (i > 0)
			//	elements[i].measurements.change_in_total_worm_intensity =  (double)elements[i].measurements.total_intensity_within_worm_area - (double)elements[i-1].measurements.total_intensity_within_worm_area;
			//else elements[i].measurements.change_in_total_worm_intensity = 0;
		}

		//Here, ns_movement_data_accessor calculates and provides the raw movement score to the kernel smoother,
		//which then uses it to calculate the denoised movement scores
		//These denoised movmenet scores are used for automated movement analaysis.
		//If the "normalize_movement_timeseries_to_median" flag is set, the set smoothed movement score values
		//for each object is subtracted out to zero.

		ns_kernel_smoother<ns_movement_data_accessor>m;
		ns_kernel_smoother<ns_spatially_averaged_movement_data_accessor>m_s;
		m(kernel_width,acc);
		m_s(kernel_width, acc_spatial);

		/*for (unsigned int i= 0; i < elements.size(); i++){
			elements[i].measurements.normalized_worm_area = elements[i].measurements.total_worm_area/(double)elements[first_stationary_timepoint()].measurements.total_worm_area;
			if (elements[i].measurements.total_worm_area == 0)
				elements[i].measurements.normalized_total_intensity = 0;
			else elements[i].measurements.normalized_total_intensity = elements[i].measurements.total_intensity_within_worm_area / elements[i].measurements.total_worm_area;
			elements[i].measurements.change_in_average_normalized_worm_intensity = 0;
		}*/

	/*	ns_kernel_smoother<ns_intensity_data_accessor> i;
		ns_intensity_data_accessor acc2(elements);
		i(kernel_width,acc2);*/

		//use a kernal to calculate slope
		const int slope_kernel_half_width(8);
		const int slope_kernel_width = slope_kernel_half_width * 2 + 1;
		if (elements.size() >= slope_kernel_width) {
			std::vector<ns_64_bit > stabilized_vals(slope_kernel_width);
			std::vector<ns_64_bit > foreground_vals(slope_kernel_width);
			std::vector<ns_64_bit > region_vals(slope_kernel_width);
			std::vector<ns_64_bit > times(slope_kernel_width);
			for (unsigned int i = 0; i < slope_kernel_width; i++) {
				foreground_vals[i] = elements[i].measurements.total_intensity_within_foreground;
				stabilized_vals[i] = elements[i].measurements.total_intensity_within_stabilized;
				region_vals[i] = elements[i].measurements.total_intensity_within_region;
				times[i] = elements[i].absolute_time;
			}
			ns_linear_regression_model model;
			ns_linear_regression_model_parameters foreground_params(model.fit(foreground_vals, times));
			ns_linear_regression_model_parameters region_params(model.fit(region_vals, times));
			ns_linear_regression_model_parameters stabilized_params(model.fit(stabilized_vals, times));
			for (unsigned int i = 0; i < slope_kernel_half_width; i++) {
				elements[i].measurements.change_in_total_foreground_intensity = foreground_params.slope * 60 * 60; // units: per hour
				elements[i].measurements.change_in_total_region_intensity = region_params.slope * 60 * 60; // units/hour
				elements[i].measurements.change_in_total_stabilized_intensity = stabilized_params.slope * 60 * 60; // units/hour

			}
			int pos = 0;
			for (unsigned int i = slope_kernel_half_width; ; i++) {
				//calculate slope of current kernal
				foreground_params = model.fit(foreground_vals, times);
				region_params = model.fit(region_vals, times);
				stabilized_params = model.fit(stabilized_vals, times);

				elements[i].measurements.change_in_total_foreground_intensity = foreground_params.slope * 60 * 60; // units/
				elements[i].measurements.change_in_total_region_intensity = region_params.slope * 60 * 60; // units/hour
				elements[i].measurements.change_in_total_stabilized_intensity = stabilized_params.slope * 60 * 60; // units/hour

				if (i + slope_kernel_half_width + 2 >= elements.size())
					break;
				//update kernal for next step, by replacing earliest value i
				foreground_vals[pos] = elements[i + slope_kernel_half_width + 2].measurements.total_intensity_within_foreground;
				stabilized_vals[pos] = elements[i + slope_kernel_half_width + 2].measurements.total_intensity_within_stabilized;
				region_vals[pos] = elements[i + slope_kernel_half_width + 2].measurements.total_intensity_within_region;
				times[pos] = elements[i + slope_kernel_half_width + 2].absolute_time;
				pos++;
				if (pos == slope_kernel_width)
					pos = 0;
			}
			for (unsigned int i = elements.size() - slope_kernel_half_width - 1; i < elements.size(); i++) {
				elements[i].measurements.change_in_total_foreground_intensity = foreground_params.slope * 60 * 60; // units/
				elements[i].measurements.change_in_total_region_intensity = region_params.slope * 60 * 60; // units/hour
				elements[i].measurements.change_in_total_stabilized_intensity = stabilized_params.slope * 60 * 60; // units/hour
			}

		}
		
		return;
	}
	else {
		throw ns_ex("THIS CODE SHOULD NOT BE RUNNING");
		const int offset_size(4);
		const bool use_median(true);
		std::vector<double> normalization_factors(offset_size);
		if (use_median) {
			std::vector<std::vector<ns_64_bit> > values(offset_size);
			for (unsigned int i = 0; i < offset_size; i++) {
				values[i].reserve(elements.size() / offset_size);
			}
			int k(0);
			for (unsigned int i = 0; i < elements.size(); i++) {
				values[k].push_back(elements[i].measurements.movement_sum);
				k++;
				if (k == offset_size) k = 0;
			}
			for (unsigned int i = 0; i < offset_size; i++) {
				std::sort(values[i].begin(), values[i].end());
				normalization_factors[i] = values[i][values[i].size() / 2];
			}
		}
		else {
			long long sums[offset_size];
			long count[offset_size];
			//initialize
			for (unsigned int i = 0; i < offset_size; i++) {
				sums[i] = 0;
				count[i] = 0;
			}
			//calculate mean of each offset, first by calculating sum and count
			int k(0);
			for (unsigned int i = elements.size() / 3; i < elements.size(); i++) {
				sums[k] += elements[i].measurements.movement_sum;
				count[k]++;
				k++;
				if (k == offset_size) k = 0;
			}
			//then dividing to get the means.
			for (unsigned int i = 0; i < offset_size; i++)
				normalization_factors[i] = sums[i] / (double)count[i];
		}
		//now normalize each movement score by its offset's mean
		int k = 0;
		for (unsigned int i = 0; i < elements.size(); i++) {
			elements[i].measurements.denoised_movement_score = elements[i].measurements.movement_sum / (double)normalization_factors[k];
			k++;
			if (k == offset_size) k = 0;
		}
	}
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

		if (first_frames) {
			elements[i].measurements.change_in_total_foreground_intensity =
				elements[i].measurements.change_in_total_region_intensity = elements[i].measurements.change_in_total_stabilized_intensity = 0;
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
		elements[i].measurements.spatial_averaged_movement_sum = 0;


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
				//there is a lot of low-level pixel noise that can average out even with a small 5x5 kernal.
				if (worm_threshold) {
					long averaged_sum, count;
					spatially_average_movement(y, x, ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_kernal_half_size,elements[i].registered_images->movement_image_, averaged_sum, count);
					if (abs(averaged_sum) < count*ns_time_path_image_movement_analyzer::ns_spatially_averaged_movement_threshold)
						averaged_sum = 0;
					if (count > 0) {
						elements[i].measurements.spatial_averaged_movement_sum += abs(averaged_sum / (float)count);
						elements[i].measurements.total_intensity_within_alternate_worm += (averaged_sum > 0 ? elements[i].registered_images->image[y][x] : 0);
						elements[i].measurements.total_alternate_worm_area += (averaged_sum > 0 ? 1 : 0);
					}
				}

				alternate_movement_sum += (alternate_worm_threshold) ? elements[i].registered_images->movement_image_[y][x] : 0;

			//	elements[i].measurements.total_alternate_worm_area += (alternate_worm_threshold ? 1 : 0);
				//elements[i].measurements.total_intensity_within_alternate_worm += alternate_worm_threshold ? elements[i].registered_images->image[y][x] : 0;

			}
		}
		double interframe_time_scaling_factor(1);
		if (!first_frames) {
			const long dt_s(elements[i].absolute_time - elements[i - movement_time_kernel_width].absolute_time);
			const double standard_interval(60 * 60);//one hour
			interframe_time_scaling_factor = standard_interval / dt_s;
		}
		

		elements[i].measurements.interframe_time_scaled_movement_sum = elements[i].measurements.movement_sum*interframe_time_scaling_factor;
		elements[i].measurements.interframe_scaled_spatial_averaged_movement_sum = elements[i].measurements.spatial_averaged_movement_sum*interframe_time_scaling_factor;
		elements[i].measurements.movement_alternate_worm_sum = (unsigned long)alternate_movement_sum;
	}
}
//xxx 
//ofstream * tmp_cmp = 0;
void ns_analyzed_image_time_path::copy_aligned_path_to_registered_image(const ns_analyzed_time_image_chunk & chunk, std::vector < ns_image_standard> & temporary_images) {
	
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
			elements[i].initialize_registered_images(prop, memory_pool->registered_image_pool);
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
#ifdef NS_OUTPUT_ALGINMENT_DEBUG
		if (i == 34)
			std::cerr << "WHA";
#endif
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
ns_analyzed_image_time_path_group::ns_analyzed_image_time_path_group(const ns_stationary_path_id group_id, const ns_64_bit region_info_id,const ns_time_path_solution & solution_, const ns_death_time_annotation_time_interval & observation_time_interval,ns_death_time_annotation_set & rejected_annotations,ns_time_path_image_movement_analysis_memory_pool & memory_pool){
	
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

	for (unsigned int i = 0; i < solution_.path_groups[group_id.group_id].path_ids.size(); i++){
		const unsigned long & path_id(solution_.path_groups[group_id.group_id].path_ids[i]);
		const ns_time_path & source_path(solution_.paths[path_id]);
		paths.resize(current_path_id+1,ns_analyzed_image_time_path(memory_pool,0));
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
	//		if (path.elements[s].element_before_fast_movement_cessation)
	//			cerr << "WHA";
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
					 ns_stationary_path_id(0,0,group_id.detection_set_id),false,false, path.elements[j].subregion_info,
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

		for(unsigned int j = 0; j < path.elements.size(); j++){
			path.elements[j].offset_from_path = path.elements[j].context_position_in_source_image - path.path_context_position;
			/*	if (path.elements[j].region_offset_in_source_image()==ns_vector_2i(2579,300))
				cerr << "WHA";
				if (path.elements[j].region_offset_in_source_image()==ns_vector_2i(2579,301))
				cerr << "WHA";
				if (path.elements[j].region_offset_in_source_image()==ns_vector_2i(2579,301) && path.elements[j].absolute_time == 1408897604)
				cerr << "WHA";*/
		}
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
				output_reciever = new ns_image_storage_reciever_handle<ns_8_bit>(image_server_const.image_storage.request_storage(output_image, ns_tiff_lzw, 1.0, save_output_buffer_height, &sql, had_to_use_volatile_storage, false, false));
			}
			if (save_flow_image) {
				if (ns_fix_filename_suffix(output_image.filename, ns_tiff_zip))
					output_image.save_to_db(output_image.id, &sql);
				flow_output_reciever = new ns_image_storage_reciever_handle<float>(image_server_const.image_storage.request_storage_float(flow_output_image, ns_tiff_zip, 1.0, save_output_buffer_height, &sql, had_to_use_volatile_storage, false, false));
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

void ns_analyzed_image_time_path::load_movement_images(const ns_analyzed_time_image_chunk & chunk,ns_image_storage_source_handle<ns_8_bit> & in,ns_image_storage_source_handle<float> & flow_in){
	
	ns_image_stream_buffer_properties p(movement_loading_buffer.properties());
	const unsigned long h(p.height);

	for (unsigned int i = chunk.start_i; i < chunk.stop_i; i++){
		ns_image_properties pr(in.input_stream().properties());
		set_path_alignment_image_dimensions(pr);
		elements[i].initialize_registered_images(pr,memory_pool->registered_image_pool);
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
	total_intensity_within_foreground = 0;
	total_intensity_in_previous_frame_scaled_to_current_frames_histogram = 0;
	total_alternate_worm_area = 0;
	total_intensity_within_alternate_worm = 0;

	change_in_total_foreground_intensity = 0;
	change_in_total_region_intensity = 0;
	change_in_total_stabilized_intensity = 0;

	movement_score = 0;
	denoised_movement_score = 0; 
	spatial_averaged_movement_sum = 0;
	interframe_scaled_spatial_averaged_movement_sum = 0;
	spatial_averaged_movement_score = 0;
	denoised_spatial_averaged_movement_score = 0;
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
	total_intensity_within_foreground = total_intensity_within_foreground*total_intensity_within_foreground;
	total_intensity_in_previous_frame_scaled_to_current_frames_histogram = total_intensity_in_previous_frame_scaled_to_current_frames_histogram*total_intensity_in_previous_frame_scaled_to_current_frames_histogram;
	total_alternate_worm_area = total_alternate_worm_area*total_alternate_worm_area;
	total_intensity_within_alternate_worm = total_intensity_within_alternate_worm*total_intensity_within_alternate_worm;

	change_in_total_foreground_intensity = change_in_total_foreground_intensity*change_in_total_foreground_intensity;
	change_in_total_region_intensity = change_in_total_region_intensity*change_in_total_region_intensity;
	change_in_total_stabilized_intensity = change_in_total_stabilized_intensity*change_in_total_stabilized_intensity;

	movement_score = movement_score*movement_score;
	denoised_movement_score = denoised_movement_score*denoised_movement_score;
	spatial_averaged_movement_sum = spatial_averaged_movement_sum*spatial_averaged_movement_sum;
	interframe_scaled_spatial_averaged_movement_sum = interframe_scaled_spatial_averaged_movement_sum*interframe_scaled_spatial_averaged_movement_sum;
	spatial_averaged_movement_score = spatial_averaged_movement_score*spatial_averaged_movement_score;
	denoised_spatial_averaged_movement_score = denoised_spatial_averaged_movement_score*denoised_spatial_averaged_movement_score;

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
	total_intensity_within_foreground = sqrt(total_intensity_within_foreground);
	total_intensity_in_previous_frame_scaled_to_current_frames_histogram = sqrt(total_intensity_in_previous_frame_scaled_to_current_frames_histogram);
	total_alternate_worm_area = sqrt(total_alternate_worm_area);
	total_intensity_within_alternate_worm = sqrt(total_intensity_within_alternate_worm);

	change_in_total_foreground_intensity  = sqrt(change_in_total_foreground_intensity);
	change_in_total_region_intensity = sqrt(change_in_total_region_intensity);
	change_in_total_stabilized_intensity = sqrt(change_in_total_stabilized_intensity);

	movement_score = sqrt(movement_score);
	denoised_movement_score = sqrt(denoised_movement_score);
	spatial_averaged_movement_sum = sqrt(spatial_averaged_movement_sum);
	interframe_scaled_spatial_averaged_movement_sum = sqrt(interframe_scaled_spatial_averaged_movement_sum);
	spatial_averaged_movement_score = sqrt(spatial_averaged_movement_score);
	denoised_spatial_averaged_movement_score = sqrt(denoised_spatial_averaged_movement_score);

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
	ret.total_intensity_within_foreground = a.total_intensity_within_foreground+b.total_intensity_within_foreground;
	ret.total_intensity_in_previous_frame_scaled_to_current_frames_histogram = a.total_intensity_in_previous_frame_scaled_to_current_frames_histogram+b.total_intensity_in_previous_frame_scaled_to_current_frames_histogram;
	ret.total_alternate_worm_area = a.total_alternate_worm_area+b.total_alternate_worm_area;
	ret.total_intensity_within_alternate_worm = a.total_intensity_within_alternate_worm+b.total_intensity_within_alternate_worm;

	ret.change_in_total_foreground_intensity = a.change_in_total_foreground_intensity+b.change_in_total_foreground_intensity;
	ret.change_in_total_region_intensity = a.change_in_total_region_intensity+b.change_in_total_region_intensity;
	ret.change_in_total_stabilized_intensity = a.change_in_total_stabilized_intensity+b.change_in_total_stabilized_intensity;

	ret.movement_score = a.movement_score+b.movement_score;
	ret.denoised_movement_score = a.denoised_movement_score+b.denoised_movement_score;
	ret.spatial_averaged_movement_sum = a.spatial_averaged_movement_sum+b.spatial_averaged_movement_sum;
	ret.interframe_scaled_spatial_averaged_movement_sum = a.interframe_scaled_spatial_averaged_movement_sum+b.interframe_scaled_spatial_averaged_movement_sum;
	ret.spatial_averaged_movement_score = a.spatial_averaged_movement_score+b.spatial_averaged_movement_score;
	ret.denoised_spatial_averaged_movement_score = a.denoised_spatial_averaged_movement_score+b.denoised_spatial_averaged_movement_score;

	ret.registration_displacement.x = a.registration_displacement.x+b.registration_displacement.x;
	ret.registration_displacement.y = a.registration_displacement.y+b.registration_displacement.y;
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
	ret.total_intensity_within_foreground = a.total_intensity_within_foreground / d;
	ret.total_intensity_in_previous_frame_scaled_to_current_frames_histogram = a.total_intensity_in_previous_frame_scaled_to_current_frames_histogram / d;
	ret.total_alternate_worm_area = a.total_alternate_worm_area / d;
	ret.total_intensity_within_alternate_worm = a.total_intensity_within_alternate_worm / d;
	ret.change_in_total_foreground_intensity = a.change_in_total_foreground_intensity / d;
	ret.change_in_total_region_intensity = a.change_in_total_region_intensity / d;
	ret.change_in_total_stabilized_intensity = a.change_in_total_stabilized_intensity / d;
	ret.movement_score = a.movement_score / d;
	ret.denoised_movement_score = a.denoised_movement_score / d;
	ret.spatial_averaged_movement_sum = a.spatial_averaged_movement_sum / d;
	ret.interframe_scaled_spatial_averaged_movement_sum = a.interframe_scaled_spatial_averaged_movement_sum / d;
	ret.spatial_averaged_movement_score = a.spatial_averaged_movement_score / d;
	ret.denoised_spatial_averaged_movement_score = a.denoised_spatial_averaged_movement_score / d;
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
void ns_analyzed_image_time_path::load_movement_images_no_flow(const ns_analyzed_time_image_chunk & chunk,ns_image_storage_source_handle<ns_8_bit> & in){
	
	ns_image_stream_buffer_properties p(movement_loading_buffer.properties());
	const unsigned long h(p.height);

	for (unsigned int i = chunk.start_i; i < chunk.stop_i; i++){
		ns_image_properties pr(in.input_stream().properties());
		set_path_alignment_image_dimensions(pr);
		elements[i].initialize_registered_images(pr,memory_pool->registered_image_pool);
	}

	unsigned long start_y(movement_loading_collage_info.frame_dimensions.y*(chunk.start_i/movement_loading_collage_info.grid_dimensions.x));
	
	unsigned long stop_y(movement_loading_collage_info.frame_dimensions.y*(chunk.stop_i/movement_loading_collage_info.grid_dimensions.x));

	for (unsigned long y = start_y; y < stop_y; y+=h){
		
		unsigned long dh(h);	
		if(h + y > stop_y) dh = stop_y-y;
		in.input_stream().send_lines(movement_loading_buffer,dh, movement_image_storage_internal_state);

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

bool ns_time_path_image_movement_analyzer::load_movement_image_db_info(const ns_64_bit region_id,ns_sql & sql){
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

void ns_time_path_image_movement_analyzer::load_images_for_group(const unsigned long group_id, unsigned long number_of_images_to_load,ns_sql & sql, const bool load_images_after_last_valid_sample, const bool load_flow_images){
	#ifdef NS_CALCULATE_OPTICAL_FLOW
	if (load_flow_images)
		throw ns_ex("Attempting to load flow images with NS_CALCULATE_OPTICAL_FLOW set to false");
	#endif
	if (groups[group_id].paths.size() == 0)
		return;
	const unsigned long number_of_images_loaded(groups[group_id].paths[0].number_of_images_loaded);
	if (number_of_images_loaded == 0){
		if (group_id >= size())
			throw ns_ex("ns_time_path_image_movement_analyzer::load_images_for_group()::Invalid group: ") << group_id;
	}
	if (number_of_images_loaded >= number_of_images_to_load)
		return;

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
		
		if (number_of_images_loaded == 0){
			if (groups[group_id].paths[j].output_image.id==0){
				throw ns_ex("ns_time_path_image_movement_analyzer::load_images_for_group()::Group has no stored image id specified");
			}
			groups[group_id].paths[j].output_image.load_from_db(groups[group_id].paths[j].output_image.id,&sql);
			groups[group_id].paths[j].movement_image_storage = image_server_const.image_storage.request_from_storage(groups[group_id].paths[j].output_image,&sql);
		//	cerr << group_id << " " << j <<( groups[group_id].paths[j].movement_image_storage.bound() ? "YES" : "NO" )<< "\n";
			/*ns_image_storage_source<ns_8_bit> & tmp2 = 
			unsigned long tmp = tmp2.init_send();
			tmp--;
			tmp++;*/
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

		
		if (number_of_images_to_load > number_of_valid_elements)
			number_of_images_to_load = number_of_valid_elements;
			//throw ns_ex("ns_time_path_image_movement_analyzer::load_images_for_group()::Requesting to many images!");
		if (number_of_images_to_load == 0)
			return;

		unsigned long number_of_new_images_to_load(number_of_images_to_load-number_of_images_loaded);
			
		//load in chunk by chunk
		for (unsigned int k = 0; k < number_of_new_images_to_load; k+=chunk_size){
			ns_analyzed_time_image_chunk chunk(k,k+chunk_size);
			if (chunk.stop_i >= number_of_new_images_to_load)
				chunk.stop_i = number_of_new_images_to_load;
			chunk.start_i+=number_of_images_loaded;
			chunk.stop_i+=number_of_images_loaded;

			if (!load_flow_images)
				groups[group_id].paths[j].load_movement_images_no_flow(chunk, groups[group_id].paths[j].movement_image_storage);
			else
				groups[group_id].paths[j].load_movement_images(chunk,groups[group_id].paths[j].movement_image_storage,groups[group_id].paths[j].flow_movement_image_storage);
//			
		}
		if (number_of_images_to_load == number_of_valid_elements){
			groups[group_id].paths[j].end_movement_image_loading();
			groups[group_id].paths[j].movement_image_storage.clear();
		}
		groups[group_id].paths[j].number_of_images_loaded = number_of_images_to_load;
		//groups[i].paths[j].analyze_movement(persistance_time,ns_stationary_path_id(i,j,analysis_id));
			
	//	debug_name += ".csv";
	//	ofstream tmp(debug_name.c_str());
	//	groups[i].paths[j].output_image_movement_summary(tmp);
	}
}

void ns_time_path_image_movement_analyzer::clear_images_for_group(const unsigned long group_id){
	if (groups.size() <= group_id)
		return;
	for (unsigned int j = 0; j < groups[group_id].paths.size(); j++){
		for (unsigned int k = 0; k < groups[group_id].paths[j].elements.size(); k++){
			groups[group_id].paths[j].elements[k].clear_movement_images();
		}
		groups[group_id].paths[j].number_of_images_loaded = 0;
	}
}

void ns_time_path_image_movement_analyzer::back_port_by_hand_annotations_to_solution_elements(ns_time_path_solution & sol) {
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

void ns_time_path_image_movement_analyzer::reanalyze_stored_aligned_images(const ns_64_bit region_id,const ns_time_path_solution & solution_,const ns_time_series_denoising_parameters & times_series_denoising_parameters,const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql,const bool load_images_after_last_valid_sample,const bool recalculate_flow_images){


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
		populate_movement_quantification_from_file(sql,false);
		ns_64_bit file_specified_analysis_id = this->analysis_id;
		//xxxxxx disabled for debug
		obtain_analysis_id_and_save_movement_data(region_id, sql, ns_require_existing_record, ns_do_not_write_data);
		//if (file_specified_analysis_id != this->analysis_id)
		//	throw ns_ex("Movement analysis ID specified on disk does not agree with the ID  specified in database.");
		if (analysis_id == 0)
			throw ns_ex("Could not obtain analysis id!");

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
						/*	bool had_to_use_volatile_storage;
							groups[i].paths[j].flow_output_reciever = new ns_image_storage_reciever_handle<float>(image_server_const.image_storage.request_storage_float(groups[i].paths[j].flow_output_image, ns_tiff_zip, 1024, &sql, had_to_use_volatile_storage, false, false));
							ns_image_properties prop(storage.input_stream().properties());
							prop.components = 3;
							groups[i].paths[j].flow_output_reciever->output_stream().init(prop);*/
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
						groups[i].paths[j].load_movement_images_no_flow(chunk, storage);
						if (flow_handling_approach == ns_create) {
							groups[i].paths[j].calc_flow_images_from_registered_images(chunk);
							groups[i].paths[j].save_movement_images(chunk, sql, ns_analyzed_image_time_path::ns_save_flow, ns_analyzed_image_time_path::ns_output_all_images, ns_analyzed_image_time_path::ns_long_term);
						}
					}
					else {
						groups[i].paths[j].load_movement_images(chunk, storage,flow_storage);
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
				clear_images_for_group(i);
				groups[i].paths[j].end_movement_image_loading();
				groups[i].paths[j].denoise_movement_series_and_calculate_intensity_slopes(0,times_series_denoising_parameters);
			
				//groups[i].paths[j].analyze_movement(e,generate_stationary_path_id(i,j));
				
				//groups[i].paths[j].calculate_movement_quantification_summary();
			//	debug_name += ".csv";
			//	ofstream tmp(debug_name.c_str());
			//	groups[i].paths[j].output_image_movement_summary(tmp);
			}
		}
		normalize_movement_scores_over_all_paths(e->software_version_number(),times_series_denoising_parameters);

		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
					continue;
				groups[i].paths[j].analyze_movement(e,generate_stationary_path_id(i,j),last_timepoint_in_analysis_);
				groups[i].paths[j].calculate_movement_quantification_summary();
			}
		}
		memory_pool.clear();
		image_server_const.add_subtext_to_current_event("\n", &sql);
		generate_movement_description_series();
		movement_analyzed = true;
	}
	catch(...){
		//delete_from_db(region_id,sql);
		throw;
	}
}
void ns_time_path_image_movement_analyzer::mark_path_images_as_cached_in_db(const ns_64_bit region_id, ns_sql & sql){
	sql << "UPDATE sample_region_image_info SET path_movement_images_are_cached = 1 WHERE id = " << region_id;
	sql.send_query();
}
void ns_time_path_image_movement_analyzer::acquire_region_image_specifications(const ns_64_bit region_id,ns_sql & sql){
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

void ns_time_path_image_movement_analyzer::normalize_movement_scores_over_all_paths(const int software_version, const ns_time_series_denoising_parameters & param){
	if (param.movement_score_normalization == ns_time_series_denoising_parameters::ns_none ||
		param.movement_score_normalization == ns_time_series_denoising_parameters::ns_subtract_out_device_median)
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
				if (software_version==1)
				values[k-start] = elements[k].measurements.death_time_posture_analysis_measure_v1();
				else
					values[k - start] = elements[k].measurements.death_time_posture_analysis_measure_v2();
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
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++){
				if (ns_skip_low_density_paths && groups[i].paths[j].is_low_density_path())
						continue; 	

				for (unsigned int k = 0; k < groups[i].paths[j].elements.size(); k++)
					(software_version==1?groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v1(): groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v2()) -=median;
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
					(software_version == 1 ? groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v1() : groups[i].paths[j].elements[k].measurements.death_time_posture_analysis_measure_v2()) -=medians[m_pos];
				m_pos++;
			}
		}
	}
}


void ns_time_path_image_movement_analyzer::match_plat_areas_to_paths(std::vector<ns_region_area> & areas) {
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
	ns_time_path_image_movement_analyzer a;
	for (unsigned int i = 0; i < areas.size(); i++)
		areas[i].average_annotation_time_for_region = average_path_duration;
}

void ns_time_path_image_movement_analyzer::generate_movement_description_series(){
	//group sizes and position
	description_series.group_region_sizes.resize(groups.size(),ns_vector_2i(0,0));
	description_series.group_region_position_in_source_image.resize(groups.size(),ns_vector_2i(0,0));
	description_series.group_context_sizes.resize(groups.size(),ns_vector_2i(0,0));
	description_series.group_context_position_in_source_image.resize(groups.size(),ns_vector_2i(0,0));
	for (unsigned int i = 0; i < groups.size(); i++){
		if (groups[i].paths.size()==0)
			continue;
		description_series.group_region_sizes[i] = groups[i].paths[0].path_region_size;
		description_series.group_region_position_in_source_image[i] = groups[i].paths[0].path_region_position;
		description_series.group_context_sizes[i] = groups[i].paths[0].path_context_size;
		description_series.group_context_position_in_source_image[i] = groups[i].paths[0].path_context_position;
	}


	//populate timepoints from solution
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
				-1
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
					i
					));
			}
		}
	}
}


ns_analyzed_image_time_path_event_index ns_analyzed_image_time_path::find_event_index(const ns_movement_event & event_to_align){
	ns_movement_state_observation_boundary_interval & state_interval(state_intervals[(int)ns_movement_event_state(event_to_align)]);
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


void ns_time_path_image_movement_analyzer::generate_death_aligned_movement_posture_visualizations(const bool include_motion_graphs,const ns_64_bit region_id,const ns_movement_event & event_to_align,const ns_time_path_solution & solution,ns_sql & sql){
	
	ns_64_bit sample_id(0),experiment_id(0);
	string region_name,sample_name,experiment_name;

	ns_region_info_lookup::get_region_info(region_id,&sql,region_name,sample_name,sample_id,experiment_name,experiment_id);
	
	const long thickness(2);

	ns_marker_manager marker_manager;
	
	const ns_vector_2i graph_dimensions(include_motion_graphs?ns_vector_2i(300,200):ns_vector_2i(0,0));
	ns_worm_movement_description_series series = description_series;	

	series.calculate_visualization_grid(graph_dimensions);
	if (series.group_positions_on_visualization_grid.size() != groups.size() ||
		series.group_should_be_displayed.size() != groups.size() ||
		series.group_region_sizes.size() != groups.size())
		throw ns_ex("calculate_visualization_grid() returned an inconsistant result");

	//find time for which each animal needs to be aligned
	vector<ns_analyzed_image_time_path_event_index> path_aligned_event_index(groups.size());
	
	unsigned long alignment_position(0);
	unsigned long aligned_size(0);

	for (unsigned int i = 0; i < groups.size(); i++){
		if (!series.group_should_be_displayed[i])
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
		if (!series.group_should_be_displayed[i]){
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
			if (!series.group_should_be_displayed[i]) continue;
			ns_make_path_movement_graph(groups[i].paths[0],path_movement_graphs[i]);
			path_movement_graphs[i].draw(graph_temp);
		}
	}

	//load first frame of all paths
	/*for (unsigned int g = 0; g < groups.size(); g++){
		if (!series.group_should_be_displayed[g]) continue;
		groups[g].paths[0].load_movement_images(ns_analyzed_time_image_chunk(0,1),path_image_source[g]);
	}*/

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
			
			if (!series.group_should_be_displayed[g] || i >= (long)path.elements.size()) 
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
						output_image[y+series.metadata_positions_on_visualization_grid[g].y]
						[x+ 3*series.metadata_positions_on_visualization_grid[g].x] = graph_temp[y][x]/scale;
					}
				}
			}

		
			//draw colored line around worm
			const ns_vector_2i & p (series.group_positions_on_visualization_grid[g]);
			const ns_vector_2i & s (series.group_context_sizes[g]);

			ns_vector_2i pos(series.group_positions_on_visualization_grid[g]);
			if (include_motion_graphs){
				if ( series.group_context_sizes[g].x < graph_temp.properties().width)
					pos.x += (graph_temp.properties().width-series.group_context_sizes[g].x)/2;
			}


		//	const ns_vector_2i s (series.metadata_positions_on_visualization_grid[g]+ns_vector_2i(graph_prop.width+thickness,graph_prop.height));
			output_image.draw_line_color_thick(pos+ns_vector_2i(-thickness,-thickness),pos+ns_vector_2i(s.x,-thickness),c,thickness);
			output_image.draw_line_color_thick(pos+ns_vector_2i(-thickness,-thickness),pos+ns_vector_2i(-thickness,s.y),c,thickness);
			output_image.draw_line_color_thick(pos+s,pos+ns_vector_2i(s.x,-thickness),c,thickness);
			output_image.draw_line_color_thick(pos+s,pos+ns_vector_2i(-thickness,s.y),c,thickness);
			
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

				vis_summary.worms[ps].path_in_visualization.position = pos;
				vis_summary.worms[ps].path_in_visualization.size=  series.group_context_sizes[g];

				if (include_motion_graphs){
					vis_summary.worms[ps].metadata_in_visualizationA.position = series.metadata_positions_on_visualization_grid[g];
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
						output_image[y+pos.y]
						[x+ 3*pos.x] = im[y][x];
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
}

void ns_time_path_image_movement_analyzer::generate_movement_posture_visualizations(const bool include_graphs,const ns_64_bit region_id,const ns_time_path_solution & solution,ns_sql & sql){
	const unsigned long thickness(3);

	ns_marker_manager marker_manager;

	const ns_vector_2i graph_dimensions(include_graphs?ns_vector_2i(300,200):ns_vector_2i(0,0));
	ns_worm_movement_description_series series = description_series;
	series.calculate_visualization_grid(graph_dimensions);
	if (series.group_positions_on_visualization_grid.size() != groups.size() ||
		series.group_should_be_displayed.size() != groups.size() ||
		series.group_region_sizes.size() != groups.size())
		throw ns_ex("calculate_visualization_grid() returned an inconsistant result");

	vector<unsigned long> path_i(groups.size(),0);
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

	//initialize path movement image source for first frame
	for (unsigned int i = 0; i < groups.size(); i++){
		if (!series.group_should_be_displayed[i]){
			path_image_source.push_back(ns_image_storage_source_handle<ns_8_bit>(0)); //we want the index of groups[i] and path_image_source[i] to match so we fill unwanted groups with a dummy entry.
			continue;
		}
		groups[i].paths[0].output_image.load_from_db(groups[i].paths[0].output_image.id,&sql);
		path_image_source.push_back(image_server_const.image_storage.request_from_storage(groups[i].paths[0].output_image,&sql));
		groups[i].paths[0].initialize_movement_image_loading_no_flow(path_image_source[i],false);
	}

	//make movement 
	if (include_graphs){
		for (unsigned int i = 0; i < groups.size(); i++){
			if (!series.group_should_be_displayed[i]) continue;
			ns_make_path_movement_graph(groups[i].paths[0],path_movement_graphs[i]);
			path_movement_graphs[i].draw(graph_temp);
		}
	}

	//load first frame of all paths
	for (unsigned int g = 0; g < groups.size(); g++){
		if (!series.group_should_be_displayed[g]) continue;
		groups[g].paths[0].load_movement_images_no_flow(ns_analyzed_time_image_chunk(0,1),path_image_source[g]);
	}
	//now go through each measurement time for the solution
	 long last_r(-5);
	for (unsigned long t = 0; t < solution.timepoints.size(); t++){
		 long cur_r = (100 * t) / solution.timepoints.size();
		if (cur_r - last_r >= 5) {
			image_server_const.add_subtext_to_current_event(ns_to_string(cur_r) + "%...", &sql);
			last_r = cur_r;
		}

		ns_movement_posture_visualization_summary vis_summary;
		vis_summary.region_id = region_id;


		//init output image
		for (unsigned long y = 0; y < prop.height; y++)
			for (unsigned long x = 0; x < 3*prop.width; x++)
				output_image[y][x] = 0;
		bool image_to_output(false);

		const unsigned long time(solution.timepoints[t].time);
		//go through each path
		for (unsigned int g = 0; g < groups.size(); g++){
			if (!series.group_should_be_displayed[g]) continue;
			const unsigned long path_id(0);
			ns_analyzed_image_time_path & path(groups[g].paths[path_id]);
			unsigned long & i(path_i[g]);
			if (i >= path.elements.size()) continue;
			//find most up to date frame for the path;
			unsigned best_frame;

			for (best_frame = i; best_frame < path.elements.size() && path.elements[best_frame].absolute_time < time; best_frame++);
			if (best_frame == path.elements.size())
				continue;
			if (best_frame == 0 && path.elements[best_frame].absolute_time > time)
				continue;
			if (path.elements[best_frame].absolute_time > time)
				best_frame--;

			//now load frames until done
			if (best_frame > i+1)
				image_server_const.add_subtext_to_current_event(std::string("Skipping") + ns_to_string(best_frame - i) +" frames from path " + ns_to_string(g) + "\n", &sql);
				

			for (; i < best_frame;){
			//	cerr << "Clearing p"<<g<< " " << i << "\n";
				if (i!=0)
					path.elements[i-1].clear_movement_images(); 
				++i;
				ns_analyzed_time_image_chunk chunk(i,i+1);
			//	cerr << "loading p"<<g<< " " << i << "\n";
				path.load_movement_images_no_flow(chunk,path_image_source[g]);
			}

			image_to_output = true;
			//transfer over movement visualzation to the correct place on the grid.
			ns_image_standard im;
			ns_color_8 c;
			path.elements[i].generate_movement_visualization(im);
			c = ns_movement_colors::color(path.explicitly_recognized_movement_state(time));
		
			if (im.properties().height == 0)
				throw ns_ex("Registered images not loaded! Path ") << g << " i " << i;


			string::size_type ps(vis_summary.worms.size());
			vis_summary.worms.resize(ps+1);
			vis_summary.worms[ps].path_in_source_image.position = path.path_region_position;
			vis_summary.worms[ps].path_in_source_image.size= path.path_region_size;

			vis_summary.worms[ps].worm_in_source_image.position = path.elements[i].region_offset_in_source_image();
			vis_summary.worms[ps].worm_in_source_image.size = path.elements[i].worm_region_size();

			vis_summary.worms[ps].path_in_visualization.position = series.group_positions_on_visualization_grid[g];
			vis_summary.worms[ps].path_in_visualization.size= series.group_context_sizes[g];

			if (include_graphs){
				vis_summary.worms[ps].metadata_in_visualizationA.position = series.metadata_positions_on_visualization_grid[g];
				vis_summary.worms[ps].metadata_in_visualizationA.size = ns_vector_2i(graph_prop.width,graph_prop.height);
			}
			else vis_summary.worms[ps].metadata_in_visualizationA.position = vis_summary.worms[ps].metadata_in_visualizationA.size = ns_vector_2i(0,0);

			vis_summary.worms[ps].stationary_path_id.path_id = path_id;
			vis_summary.worms[ps].stationary_path_id.group_id = g;
			vis_summary.worms[ps].stationary_path_id.detection_set_id = analysis_id;
			vis_summary.worms[ps].path_time.period_start = path.elements[i].absolute_time;
			vis_summary.worms[ps].path_time.period_end = path.elements.rbegin()->absolute_time;
			vis_summary.worms[ps].image_time = path.elements[i].absolute_time;

			
			//ns_vector_2i region_offset(path.elements[i].region_offset_in_source_image()-path.elements[i].context_offset_in_source_image());
			for (unsigned int y = 0; y < vis_summary.worms[ps].path_in_visualization.size.y; y++){
				for (unsigned int x = 0; x < 3*vis_summary.worms[ps].path_in_visualization.size.x; x++){
					output_image[y+vis_summary.worms[ps].path_in_visualization.position.y]
					[x+ 3*vis_summary.worms[ps].path_in_visualization.position.x] = im[y/*+region_offset.y*/][x/*+3*region_offset.x*/];
				}
			}

			if (include_graphs){
				marker_manager.set_marker(time,path_movement_graphs[g]);
				path_movement_graphs[g].draw(graph_temp);

				for (unsigned int y = 0; y < vis_summary.worms[ps].metadata_in_visualizationA.size.y; y++){
					for (unsigned int x = 0; x < 3*vis_summary.worms[ps].metadata_in_visualizationA.size.x; x++){
						output_image[y+vis_summary.worms[ps].metadata_in_visualizationA.position.y]
						[x+ 3*vis_summary.worms[ps].metadata_in_visualizationA.position.x] = graph_temp[y][x];
					}
				}
			}


			//draw colored line around worm
			const ns_vector_2i & p (vis_summary.worms[ps].path_in_visualization.position);
			const ns_vector_2i & s (vis_summary.worms[ps].path_in_visualization.size);
			output_image.draw_line_color_thick(p,p+ns_vector_2i(s.x,0),c,thickness);
			output_image.draw_line_color_thick(p,p+ns_vector_2i(0,s.y),c,thickness);
			output_image.draw_line_color_thick(p+s,p+ns_vector_2i(s.x,0),c,thickness);
			output_image.draw_line_color_thick(p+s,p+ns_vector_2i(0,s.y),c,thickness);
		}
		string metadata;
		vis_summary.to_xml(metadata);
		output_image.set_description(metadata);
		ns_image_server_captured_image_region reg;
		reg.region_images_id = solution.timepoints[t].sample_region_image_id;
		ns_image_server_image im(reg.create_storage_for_processed_image(ns_process_movement_posture_visualization,ns_tiff,&sql));
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
};


//we step through all region visualizations between start_i and stop_i, ordered by time in region_image_specifications
//and populate any worm images that are needed from each region image.
void ns_time_path_image_movement_analyzer::load_region_visualization_images(const unsigned long start_i, const unsigned long stop_i,const unsigned int start_group, const unsigned int stop_group,ns_sql & sql, bool just_do_a_consistancy_check, bool running_backwards, ns_analyzed_image_time_path::ns_load_type load_type){
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
				throw ns_ex("Interpolated region visualization requested with no id specified");
			if (region_image_specifications[i].region_vis_required &&
				region_image_specifications[i].region_vis_image.id==0)
				throw ns_ex("Region visualization requested with no id specified");
		

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
						if (groups[g].paths[p].populate_images_from_region_visualization(region_image_specifications[i].time, image_loading_temp, image_loading_temp2, just_do_a_consistancy_check, load_type))
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
//	intensity_changes.reserve(50);
//	intensity_changes.resize(0);

	for (unsigned int ht = 0; ht < hold_times.size(); ht++) {
		double max_intensity_change(-DBL_MAX);
		unsigned long time_of_max_intensity_change(death_index);
		bool found_valid_time(false);

		//here we are calculate a running average rate of increase in intensity within the stablized region
		//we then identify the largest increase after death, and call that the death time increase
		for (long t = death_index; t < element_count(); t++) {
			if (element(t).excluded)
				continue;
			unsigned long cur_t = element(t).absolute_time;
			//find largest intensity change within in kernel to use as the "peak time".
			ns_s64_bit max_val = element(t).measurements.change_in_total_stabilized_intensity;
			ns_s64_bit max_tt = t;
			ns_s64_bit total_intensity_change(0);
			unsigned long point_count(0);
			//this could be done faster with a sliding window.
			for (long tt = t; tt < element_count() && element(tt).absolute_time <= cur_t + hold_times[ht]; tt++) {
				if (element(tt).excluded) continue;
				total_intensity_change += element(tt).measurements.change_in_total_stabilized_intensity;
				point_count++;
				//	intensity_changes.push_back(element(tt).measurements.change_in_total_stabilized_intensity);
				if (max_val < element(tt).measurements.change_in_total_stabilized_intensity) {
					max_val = element(tt).measurements.change_in_total_stabilized_intensity;
					max_tt = tt;
				}
			}

			//max_tt is now set as the largest intensity change in the window
			//total_intensity change/point_count is the average intensity change in the window.

			if (point_count == 0)
				continue;
			found_valid_time = true;
			const double av_change = total_intensity_change / point_count;
			const bool is_max_so_far(av_change > max_intensity_change);
			if (is_max_so_far) {
				max_intensity_change = av_change;
				time_of_max_intensity_change = max_tt;
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
					if (element(tt).measurements.change_in_total_stabilized_intensity > 0) {
						result.time_point_at_which_death_time_expansion_started = tt;
					}
					else
						break;
				}
				for (long tt = time_of_max_intensity_change; tt < element_count(); tt++) {
					if (element(tt).excluded)
						continue;
					if (element(tt).measurements.change_in_total_stabilized_intensity > 0) {
						result.time_point_at_which_death_time_expansion_stopped = tt;
					}
					else
						break;
				}

			}
			else {
				result.found_death_time_expansion = false;
			}
		}
	}
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
			path->element(t).measurements.death_time_posture_analysis_measure_v2());

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
	if (m.posture_analysis_method == ns_posture_analysis_model::ns_hidden_markov){
		return new ns_time_path_movement_markov_solver(m.hmm_posture_estimator);
	}
	else if (m.posture_analysis_method == ns_posture_analysis_model::ns_threshold){
		return new ns_threshold_movement_posture_analyzer(m.threshold_parameters);
	}
	else if (m.posture_analysis_method == ns_posture_analysis_model::ns_not_specified)
		throw ns_ex("ns_get_death_time_estimator_from_posture_analysis_model()::No posture analysis method specified.");
	throw ns_ex("ns_get_death_time_estimator_from_posture_analysis_model()::Unknown posture analysis method!");
}
