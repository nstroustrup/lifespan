#ifndef NS_TIME_PATH_IMAGE_ANALYZER_H
#define NS_TIME_PATH_IMAGE_ANALYZER_H
#include "ns_time_path_solver.h"
#include "ns_movement_measurement.h"
#include "ns_survival_curve.h"
#include "ns_time_path_posture_movement_solution.h"
#include "ns_optical_flow_quantification.h"
#include "ns_subpixel_image_alignment.h"


#undef NS_CALCULATE_OPTICAL_FLOW
#define NS_USE_FAST_IMAGE_REGISTRATION
#undef NS_CALCULATE_SLOW_IMAGE_REGISTRATION


ns_analyzed_image_time_path_death_time_estimator * ns_get_death_time_estimator_from_posture_analysis_model(const ns_posture_analysis_model & m);

struct ns_analyzed_image_time_path_element_measurements{

	
	//float old_movement_ratio() const{return ((float)movement_sum - (float)movement_alternate_worm_sum)/(total_intensity_within_worm_area+1);}
	//the total sum of movement seen between this and the previous frame, minus the total change in intensity between this and the previous frame, minus the change in movement that can be accounted for by a second worm
	//normalized by the average intensity of this and the previous frames.
	float new_movement_ratio() const{
		return (float)(((float)interframe_time_scaled_movement_sum - fabs(change_in_average_normalized_worm_intensity*total_worm_area))/(total_intensity_within_worm_area-(change_in_average_normalized_worm_intensity*total_worm_area)/2+1));
	}
	inline const double & death_time_posture_analysis_measure() const{return denoised_movement_score;}//return local_maximum_movement_sum;}
	inline double & death_time_posture_analysis_measure()      {return denoised_movement_score;}//return local_maximum_movement_sum;}
	

	unsigned long interframe_time_scaled_movement_sum,
				  unnormalized_movement_sum,
				  movement_alternate_worm_sum,
				  total_worm_area,
				  total_intensity_within_worm_area,
				  total_region_area,
				  total_intensity_within_region,
				  total_alternate_worm_area,
				  total_intensity_within_alternate_worm;

	unsigned long local_maximum_movement_sum,
				  local_maximum_stationary_sum;

	ns_vector_2i local_maximum_position,
				 local_maximum_area;
	
	double change_in_total_worm_intensity,
		   change_in_average_normalized_worm_intensity,
		   change_in_average_region_intensity,
		   normalized_total_intensity,
		   normalized_worm_area,
		   denoised_change_in_average_normalized_worm_intensity;

	ns_optical_flow_quantification scaled_flow_magnitude,
								raw_flow_magnitude,
								scaled_flow_dx, scaled_flow_dy,
								raw_flow_dx, raw_flow_dy;

	double movement_score,
		   denoised_movement_score;

	ns_vector_2d registration_displacement;

	double mean_intensity_within_worm_area,
		   mean_intensity_within_region,
		   mean_intensity_within_alternate_worm;

	mutable std::vector<double> posture_quantification_extra_debug_fields;

	void calculate_means();
	void zero();

	void square();
	void square_root();
};

ns_analyzed_image_time_path_element_measurements operator+(const ns_analyzed_image_time_path_element_measurements & a,const ns_analyzed_image_time_path_element_measurements & b);
ns_analyzed_image_time_path_element_measurements operator/(const ns_analyzed_image_time_path_element_measurements & a,const int & d);


class ns_default_resizer{
public:
	ns_image_properties parse_initialization(const ns_image_properties & t){ return t;}
	template<class T>
	void resize_after_initialization(const ns_image_properties & t){;}
};

class ns_overallocation_resizer{
public:
	ns_overallocation_resizer(const ns_image_properties & overallocation_size_):overallocation_size(overallocation_size_){}
	ns_overallocation_resizer	():overallocation_size(0,0,0,0){}
	void set_size(const ns_image_properties & overallocation_size_){overallocation_size = overallocation_size_;}
	ns_image_properties parse_initialization(const ns_image_properties & t){ return overallocation_size;}
	template<class T>
	void resize_after_initialization(const ns_image_properties & t, T & v){v.resize(t);}
private:
	ns_image_properties overallocation_size;
};

//#define NS_DEBUG_POOL
template<class T, class dT = ns_default_resizer>
class ns_image_pool{
public:
	ns_image_pool():number_checked_out(0),pre_allocated(0),min_stack_size_in_history(0){
		clear_history();
	}
	ns_image_pool(const unsigned long i):number_checked_out(0),pre_allocated(0),min_stack_size_in_history(0){
		clear_history();pre_allocate(i);
	}
	~ns_image_pool(){
		clear();
	}
	void pre_allocate(const unsigned long s){
		for (unsigned int i = 0; i < s; i++)
			pool.push(new T);
	}
	template<class M>
	void pre_allocate(const unsigned long s, const M & m){
		pre_allocated+=s;
		for (unsigned int i = 0; i < s; i++){
			pool.push(new T(resizer.parse_initialization(m)));
			resizer.resize_after_initialization(m,pool.top());
		}
	}
	T * get(){
		number_checked_out++;
		if (pool.empty()){	
			T * a(new T);
			check_out(a);
			return a;
		}
		T * a(pool.top());
		pool.pop();
		return a;
	}
	unsigned long number_of_items_checked_out() const { return number_checked_out; }
	void clear(){
		empty_pool();
		#ifdef NS_DEBUG_POOL
			if (checked_out.size() > 0){
				std::cerr << "ns_image_pool::~ns_image_pool()::" << checked_out.size() << " objects remain checked out.  This is likely a memory leak.\n";
			}
		#endif
	}
	template<class M>
	T * get(const M & m){
		number_checked_out++;
		#ifdef NS_DEBUG_POOL
		std::cerr << "Number Checked out: " << number_checked_out << "\n";
		#endif
		if (pool.empty()){
			try{
				T * a(new T(resizer.parse_initialization(m)));
				resizer.resize_after_initialization(m,*a);
				check_out(a);
				return a;
			}
			catch(std::bad_alloc){
				empty_pool();
				throw ns_ex("ns_pool_allocator(): Ran out of memory with ") << number_checked_out << " checked out objects\n";
			}
		}
		T * a(pool.top());
		pool.pop();
		resizer.resize_after_initialization(m,*a);
		check_out(a);
		return a;
	}
	void release(T *p){
		if (p==0)
			throw ns_ex("Checking in null pointer!");
		number_checked_out--;
		//delete p;
		check_in(p);
		pool.push(p);
	}
	void mark_stack_size_waypoint_and_trim(){
		update_stack_size_history();
	//	if (min_stack_size_in_history != 0){
	//		std::cout << "ns_image_pool()::Freeing " << min_stack_size_in_history << " out of " << pool.size() << " pooled objects, with " << number_checked_out << " checked out\n";
	//	}
		trim_stack();
	}
	void empty_pool(){
		while(!pool.empty()){	
			T * t(pool.top());
			delete t;
			pool.pop();
		}
	}

	void set_resizer(const dT & resizer_){
		resizer = resizer_;
	}
	//const unsigned long & number_checked_out(){return checked_out;}
private:
	enum{ns_stack_history_size=10};
	unsigned long number_checked_out;
	unsigned long pre_allocated;
	std::stack<T *> pool;
	#ifdef NS_DEBUG_POOL
	std::set<T *> checked_out;
	void check_out(T * t){
		checked_out.insert(checked_out.end(),t);
	}
	void check_in(T * t){
		typename std::set<T *>::iterator p = checked_out.find(t);
		if (p == checked_out.end())
			throw ns_ex("Checking in an invalid pointer!");
		else checked_out.erase(p);
	}
	#else
	void check_out(T * t){}
	void check_in(T * t){}
	#endif
	int stack_size_history[ns_stack_history_size];
	int min_stack_size_in_history;
	inline void trim_stack(){
		for (int i = 0; i < min_stack_size_in_history && !pool.empty(); i++){
			T * t(pool.top());
			delete t;
			pool.pop();
		}
	}
	void clear_history(){
		for (unsigned int i = 0; i < ns_stack_history_size; i++)
			stack_size_history[i] = 0;
	}
	inline void update_stack_size_history(){
		min_stack_size_in_history = pool.size();
		for (int i = ns_stack_history_size-2; i >= 0; i--){
			stack_size_history[i+1] = stack_size_history[i];
			if (stack_size_history[i] < min_stack_size_in_history)
				min_stack_size_in_history = stack_size_history[i];
		}
		stack_size_history[0] = pool.size();
	}
	dT resizer;
};


struct ns_path_aligned_image_set{

	ns_path_aligned_image_set(){}
	ns_path_aligned_image_set(const ns_image_properties & p){
		worm_region_threshold.use_more_memory_to_avoid_reallocations();	
		image.use_more_memory_to_avoid_reallocations();	
		worm_region_threshold.init(p);
		image.init(p);
	}
	void resize(const ns_image_properties & p){
		worm_region_threshold.resize(p);
		image.resize(p);
	}
	enum{region_mask=1,worm_mask=2};
	//ns_image_bitmap region_threshold;
	//ns_image_bitmap worm_threshold;
	ns_image_standard image;
	ns_image_standard worm_region_threshold;

	bool get_region_threshold(const int y, const int x){ return (worm_region_threshold[y][x]&region_mask) != 0;}
	bool get_worm_threshold(const int y, const int x){ return (worm_region_threshold[y][x]&worm_mask) != 0;}

	template <class ns_component>
	void sample_thresholds(const float y, const float x, ns_component & region_threshold, ns_component & worm_threshold){
		const int p0x((int)x),
					p0y((int)y);
		const int p1x(p0x+1),
					p1y(p0y+1);
		const float dx(x-(float)p0x),
					dy(y-(float)p0y);
		const float d1x(1.0-dx),
					d1y(1.0-dy);
		region_threshold = (worm_region_threshold[p0y][p0x]&region_mask)*(d1y)*(d1x) +
							(worm_region_threshold[p0y][p1x]&region_mask)*(d1y)*(dx) + 
							(worm_region_threshold[p1y][p0x]&region_mask)*(dy)*(d1x) + 
							(worm_region_threshold[p1y][p1x]&region_mask)*(dy)*(dx)
							> 0.5;
		
		worm_threshold = (worm_region_threshold[p0y][p0x]&worm_mask)*(d1y)*(d1x) +
							(worm_region_threshold[p0y][p1x]&worm_mask)*(d1y)*(dx) + 
							(worm_region_threshold[p1y][p0x]&worm_mask)*(dy)*(d1x) + 
							(worm_region_threshold[p1y][p1x]&worm_mask)*(dy)*(dx)
							> 0.5;
	}
	void set_thresholds(const int y, const int x, const bool region,const bool worm){ 
		worm_region_threshold[y][x] = ((ns_8_bit)region) | ((ns_8_bit)worm<<1);
	}
};/*
inline void ns_set_bit(const bool f, //conditional
	const ns_8_bit m,//mask
	ns_8_bit w//value{
	){

	w = (w & ~m) | (-f & m);
}*/

struct ns_registered_image_set{
	ns_registered_image_set(){}
	ns_registered_image_set(const ns_image_properties & p){
		image.use_more_memory_to_avoid_reallocations();	
		//worm_threshold_.use_more_memory_to_avoid_reallocations();	
		worm_region_threshold.use_more_memory_to_avoid_reallocations();	
		//region_threshold.use_more_memory_to_avoid_reallocations();	
		movement_image_.use_more_memory_to_avoid_reallocations();
		#ifdef NS_CALCULATE_OPTICAL_FLOW
		flow_image_dx.use_more_memory_to_avoid_reallocations();
		flow_image_dy.use_more_memory_to_avoid_reallocations();
		#endif
		image.init(p);
		//worm_threshold_.init(p);
		worm_region_threshold.init(p);
		//region_threshold.init(p);
		movement_image_.init(p);
		#ifdef NS_CALCULATE_OPTICAL_FLOW
		flow_image_dx.init(p);
		flow_image_dy.init(p);
		#endif
	}
	enum{region_mask=1,worm_mask=2,worm_neighborhood_mask=4};
	bool get_region_threshold(const int y, const int x) const { return (worm_region_threshold[y][x]&region_mask)!=0;}
	bool get_worm_neighborhood_threshold(const int y, const int x) const { return (worm_region_threshold[y][x]&worm_neighborhood_mask)!=0;}
	bool mask(const int y, const int x) const { return get_worm_neighborhood_threshold(y, x); }
	bool get_worm_threshold(const int y, const int x)const { return (worm_region_threshold[y][x]&worm_mask)!=0;}
	void set_thresholds(const int y, const int x, const bool region,const bool worm, const bool worm_neighborhood) {
		worm_region_threshold[y][x] = ((ns_8_bit)region) | ((ns_8_bit)worm<<1) | ((ns_8_bit)worm_neighborhood<<2);
	}
	void resize(const ns_image_properties & p){
		image.resize(p);
		//worm_threshold_.resize(p);
		//worm_neighborhood_threshold.resize(p);
		//region_threshold.resize(p);
		worm_region_threshold.resize(p);
		movement_image_.resize(p);
		#ifdef NS_CALCULATE_OPTICAL_FLOW
		flow_image_dx.resize(p);
		flow_image_dy.resize(p);
		#endif
	}
	ns_image_standard image;
	//ns_image_bitmap   worm_threshold_;
	//ns_image_bitmap   worm_neighborhood_threshold;
	//ns_image_bitmap   region_threshold;
	ns_image_standard worm_region_threshold;
	ns_image_standard_signed movement_image_;
	#ifdef NS_CALCULATE_OPTICAL_FLOW
	ns_image_whole<float> flow_image_dx;
	ns_image_whole<float> flow_image_dy;
	#endif
};
typedef ns_image_pool<ns_path_aligned_image_set,ns_overallocation_resizer> ns_path_aligned_image_pool;
typedef ns_image_pool<ns_registered_image_set,ns_overallocation_resizer> ns_registered_image_pool;

class ns_analyzed_image_time_path_element{
public:
	ns_analyzed_image_time_path_element():registered_images(0),registered_image_pool(0),path_aligned_images(0),
	inferred_animal_location(false), element_before_fast_movement_cessation(false),element_was_processed(false),path_aligned_image_pool(0),movement(ns_movement_not_calculated),saturated_offset(false),registration_offset(0,0),number_of_extra_worms_observed_at_position(0),part_of_a_multiple_worm_disambiguation_group(0),excluded(false),censored(false){}
	~ns_analyzed_image_time_path_element(){
		clear_path_aligned_images();
		clear_movement_images();
	}
	inline const ns_vector_2i & context_offset_in_region_visualization_image() const {return context_position_in_region_vis_image;}
	
	bool element_was_processed;

	inline ns_vector_2i region_offset_in_context_image() const {return region_position_in_source_image-context_position_in_source_image;}

	inline const ns_vector_2i & region_offset_in_source_image() const {return region_position_in_source_image;}
	inline const ns_vector_2i & context_offset_in_source_image() const {return context_position_in_source_image;}
	inline const ns_vector_2i & worm_region_size() const {return worm_region_size_;}
	inline const ns_vector_2i & worm_context_size() const {return worm_context_size_;}
	//ns_vector_2i size;

	bool excluded;

	ns_analyzed_image_time_path_element_measurements measurements;


	unsigned long absolute_time,
				  relative_time;

	ns_movement_state movement;
	bool censored;
	//offset_from_path is the distance the region image is from the bounding box around the path
	ns_vector_2i offset_from_path;

	
	bool inferred_animal_location,
		 element_before_fast_movement_cessation;
	
	bool registered_image_is_loaded() const{return registered_images != 0 && registered_images->image.properties().height != 0;}
	bool path_aligned_image_is_loaded() const{return path_aligned_images != 0 && path_aligned_images->image.properties().height != 0;}
	const ns_image_standard & image() const { return registered_images->image;}
	const ns_image_standard * image_p() const { if (registered_images == 0 ) return 0;return &registered_images->image;}
	bool worm_threshold(unsigned long y, unsigned long x) const { return registered_images->get_worm_neighborhood_threshold(y,x);}
	const ns_image_standard_signed & movement_image_() const {return registered_images->movement_image_;}
	
	bool saturated_offset;

	ns_vector_2d  offset_in_registered_image()const {return ns_vector_2d(offset_from_path.x,offset_from_path.y) + registration_offset;}
	void initialize_path_aligned_images(const ns_image_properties & p,ns_path_aligned_image_pool & pool){
		path_aligned_image_pool = & pool;
		if (path_aligned_images != 0)
			throw ns_ex("initialize_path_aligned_images()::Initializing twice!");
		path_aligned_images = pool.get(p);
	}
	void release_path_aligned_images(ns_path_aligned_image_pool & pool){
		pool.release(path_aligned_images);
		path_aligned_images = 0;
	}
	void initialize_registered_images(const ns_image_properties & p,ns_registered_image_pool & pool){
		registered_image_pool = & pool;	
		if (registered_images != 0)
			throw ns_ex("initialize_registered_images()::Initializing twice!");
		registered_images = pool.get(p);
	}
	void generate_movement_visualization(ns_image_standard & out) const;
private:
	unsigned long number_of_extra_worms_observed_at_position;
	bool part_of_a_multiple_worm_disambiguation_group;

	ns_vector_2i worm_region_size_,worm_context_size_;
	

	//the path aligned images are *context* images, in that they 
	//contain extra boundary data around the close region image crop
	ns_path_aligned_image_set * path_aligned_images;
	ns_registered_image_set * registered_images;


	
	//this is the offset of the worm in the movement registered image
	//i.e how far the worm is moved by the registration algorithm from it's position in the path_aligned images.
	ns_vector_2d registration_offset,
				 registration_offset_slow;

	//xxx
	ns_64_bit alignment_times[2];
	ns_vector_2d synthetic_offset;

//	const ns_detected_worm_info * worm_;
	ns_vector_2i context_position_in_region_vis_image;
	ns_vector_2i region_position_in_source_image;
	ns_vector_2i context_position_in_source_image;

	
	bool clear_path_aligned_images(){
		if (path_aligned_images != 0){
			path_aligned_image_pool->release(path_aligned_images);
			path_aligned_images = 0;
			return true;
		}
		return false;
	}
	bool clear_movement_images(){
		if(registered_images != 0){
			registered_image_pool->release(registered_images);
			registered_images = 0;
			return true;
		}
		return false;
	}

	friend struct ns_analyzed_image_time_path_group;
	friend class ns_analyzed_image_time_path;
	friend class  ns_time_path_image_movement_analyzer;
	ns_path_aligned_image_pool * path_aligned_image_pool;
	ns_registered_image_pool * registered_image_pool;
};

struct ns_analyzed_time_image_chunk{
	typedef enum {ns_forward,ns_backward} ns_processing_direction;
	ns_analyzed_time_image_chunk(){}
	ns_analyzed_time_image_chunk(const long start,const long stop,const ns_processing_direction direction = ns_forward):start_i(start),stop_i(stop),processing_direction(direction){}
	long start_i,stop_i;
	bool direction;
	ns_processing_direction processing_direction;
	inline bool forward() const{return processing_direction==ns_forward;}
};
struct ns_analyzed_image_specification{
	ns_analyzed_image_specification():region_vis_required(false),interpolated_region_vis_required(false){}
	ns_image_server_image region_vis_image,
						  interpolated_region_vis_image;
	ns_64_bit sample_region_image_id;
	unsigned long time;
	bool region_vis_required,
		  interpolated_region_vis_required;
};

class ns_analyzed_image_time_path;

struct ns_movement_image_collage_info{
	ns_vector_2i frame_dimensions, //the size (in pixels) of each image in the collage
				 grid_dimensions;  //the height and width (in images) of the collage
	ns_image_properties prop;	//the dimensions of the collage
	
	ns_movement_image_collage_info(){}
	ns_movement_image_collage_info(const ns_analyzed_image_time_path * p, const unsigned long only_output_first_n_lines=0){from_path(p,only_output_first_n_lines);}
	void from_path(const ns_analyzed_image_time_path * p,const unsigned long only_output_first_n_frames=0);
};

struct ns_time_series_denoising_parameters{
	ns_time_series_denoising_parameters():movement_score_normalization(ns_none){}
	typedef enum{ns_none=0,ns_subtract_out_plate_median=1,ns_subtract_out_median=2,ns_subtract_out_median_of_end=3,ns_subtract_out_device_median=4} ns_movement_score_normalization_type;
	ns_movement_score_normalization_type movement_score_normalization;
	static ns_time_series_denoising_parameters load_from_db(const ns_64_bit region_id, ns_sql & sql);
	std::string to_string() const{ 
		switch(movement_score_normalization){
			case ns_none: return "None";
			case ns_subtract_out_median: return "Sub Med";
			case ns_subtract_out_median_of_end: return "Sub End";
			case ns_subtract_out_plate_median: return "Plate Med";
			case ns_subtract_out_device_median: return "Device Med";
			default: throw ns_ex("Unknown normalization technique:") << (unsigned long)movement_score_normalization;
		}
	}
};

	
struct ns_analyzed_image_time_path_event_index{
	ns_analyzed_image_time_path_event_index(){}
	ns_analyzed_image_time_path_event_index(const ns_movement_event & e, const ns_movement_state_time_interval_indicies & i):event_type(e),index(i){}
	ns_movement_event event_type;
	ns_movement_state_time_interval_indicies index;
};
struct ns_analyzed_time_path_quantification_summary{
	ns_analyzed_image_time_path_element_measurements mean_before_death,
													 mean_after_death,
													 mean_all;
	
	ns_analyzed_image_time_path_element_measurements variability_before_death,
													 variability_after_death,
													 variability_all;
	unsigned long count_before_death,
				  count_after_death,
				  count_all;

	unsigned long number_of_registration_saturated_frames_before_death,
				  number_of_registration_saturated_frames_after_death;
};


struct ns_time_path_image_movement_analysis_memory_pool{
	void set_overallocation_size(const ns_image_properties & p){
		aligned_image_pool.set_resizer(ns_overallocation_resizer(p));
		registered_image_pool.set_resizer(ns_overallocation_resizer(p));
	}
	ns_path_aligned_image_pool aligned_image_pool;
	ns_registered_image_pool registered_image_pool;
	
	void clear(){
		aligned_image_pool.clear();
		registered_image_pool.clear();
		temporary_images.clear();
	}
	void set_temporary_image_size(unsigned long i){
		if (i <= temporary_images.size())
			return;
		temporary_images.resize(i);
		for (unsigned int i = 0; i < temporary_images.size(); i++)
			temporary_images[i].use_more_memory_to_avoid_reallocations();
	}
	std::vector<ns_image_standard> temporary_images;
};

class ns_optical_flow_processor;
class ns_analyzed_image_time_path {
public:
	ns_analyzed_image_time_path(ns_time_path_image_movement_analysis_memory_pool & memory_pool_, ns_64_bit unique_process_id_) :
		memory_pool(&memory_pool_), volatile_backwards_path_data_written(false), first_stationary_timepoint_(0),
		entirely_excluded(false), image_analysis_temp1(0), image_analysis_temp2(0), images_preallocated(false),
		low_density_path(false), output_reciever(0), flow_output_reciever(0), path_db_id(0), region_info_id(0), movement_image_storage(0), flow_movement_image_storage(0),
		number_of_images_loaded(0), flow(0), unique_process_id(unique_process_id_){
		by_hand_annotation_event_times.resize((int)ns_number_of_movement_event_types, ns_death_time_annotation_time_interval::unobserved_interval()); state_intervals.resize((int)ns_movement_number_of_states);
	}
	ns_64_bit unique_process_id;
	~ns_analyzed_image_time_path();
	unsigned long number_of_elements_not_processed_correctly() const;
	void denoise_movement_series(const ns_time_series_denoising_parameters &);

	void find_first_labeled_stationary_timepoint() {
		first_stationary_timepoint_ = 0;
		for (unsigned int i = 0; i < elements.size(); i++) {
			if (!elements[i].element_before_fast_movement_cessation) {
				first_stationary_timepoint_ = i;
				break;
			}
		}
	}

	mutable std::vector<std::string> posture_quantification_extra_debug_field_names;
	//used by movement analysis algorithm
	bool volatile_backwards_path_data_written;
	ns_movement_state movement_state(const unsigned long & t) const;
	ns_death_time_annotation_time_interval by_hand_death_time() const;
	ns_movement_state by_hand_movement_state(const unsigned long & t) const;
	void add_death_time_events_to_set(ns_death_time_annotation_set & set) const;
	const ns_death_time_annotation_set & death_time_annotations() const { return death_time_annotation_set; }

	bool is_low_density_path() const { return path->is_low_density_path || low_density_path; }

	unsigned long start_time() const { return elements[0].absolute_time; }
	unsigned long stop_time() const { return elements[elements.size() - 1].absolute_time; }

	unsigned long element_count() const { return (unsigned long)elements.size(); }
	const ns_analyzed_image_time_path_element & element(const unsigned long i) const { return elements[i]; }

	void write_detailed_movement_quantification_analysis_header(std::ostream & o);
	static void write_summary_movement_quantification_analysis_header(std::ostream & o);
	static void write_analysis_optimization_data_header(std::ostream & o);
	void write_analysis_optimization_data(const ns_stationary_path_id & id, const std::vector<double> & thresholds, const std::vector<double> & hold_times, const ns_region_metadata & m, const ns_time_series_denoising_parameters & denoising_parameters, std::ostream & o) const;
	void calculate_analysis_optimization_data(const std::vector<double> & thresholds, const std::vector<double> & hold_times, std::vector< std::vector < unsigned long > > & death_times) const;

	ns_vector_2i path_region_position,
		path_region_size,
		path_context_position,
		path_context_size;

	//void out_histograms(std::ostream & o) const;

	void clear_images();
	void confirm_all_images_released() {
		//	if (this->registered_image_pool.number_checked_out() > 0 || this->aligned_image_pool.number_checked_out() > 0)
		//		std::cerr << "Leaking Images!";
	}
	void load_movement_images(const ns_analyzed_time_image_chunk & chunk, ns_image_storage_source_handle<ns_8_bit> & in, ns_image_storage_source_handle<float> & flow_in);
	void load_movement_images_no_flow(const ns_analyzed_time_image_chunk & chunk, ns_image_storage_source_handle<ns_8_bit> & in);
	void save_movement_image(const ns_analyzed_time_image_chunk & chunk, ns_image_storage_reciever_handle<ns_8_bit> & out, const bool only_write_backwards_frames);
	void save_movement_flow_image(const ns_analyzed_time_image_chunk & chunk, ns_image_storage_reciever_handle<float> & flow_out, const bool only_write_backwards_frames);
	typedef enum { ns_save_simple, ns_save_flow, ns_save_both } ns_images_to_save;
	void save_movement_images(const ns_analyzed_time_image_chunk & chunk,ns_sql & sql, const ns_images_to_save & images_to_save,const bool only_write_backwards_frames);
	void reset_movement_image_saving();
	std::string volatile_storage_name(const bool flow) const;
	void calc_flow_images_from_registered_images(const ns_analyzed_time_image_chunk & chunk);

	static ns_vector_2d maximum_alignment_offset(){return ns_vector_2d(60,60);}
	static ns_vector_2d maximum_local_alignment_offset(){return ns_vector_2d(4,4);}
	enum {movement_detection_kernal_half_width=2,sobel_operator_width=1,alignment_time_kernel_width=5,movement_time_kernel_width=1,save_output_buffer_height=16};

	bool entirely_excluded;
	ns_image_properties registered_image_properties;
	//void output_image_movement_summary(std::ostream & o);
	void write_detailed_movement_quantification_analysis_data(const ns_region_metadata & m, const unsigned long group_id, const unsigned long path_id, std::ostream & o,const bool output_only_elements_with_hand,const bool abbreviated_time_series=false) const;
	void write_summary_movement_quantification_analysis_data(const ns_region_metadata & m, const unsigned long group_id, const unsigned long path_id, std::ostream & o) const;
	void calculate_movement_quantification_summary();
	static ns_vector_2i max_step_alignment_offset();

	void set_path_alignment_image_dimensions(ns_image_properties & prop) const{
		prop.width = path_context_size.x + (long)(2*ns_analyzed_image_time_path::maximum_alignment_offset().x);
		prop.height = path_context_size.y + (long)(2*ns_analyzed_image_time_path::maximum_alignment_offset().y);
		prop.components = 1;
	}

	typedef std::vector<ns_analyzed_image_time_path_element> ns_element_list;
	bool by_hand_data_specified() const;
	/*void set_image_pools(	ns_path_aligned_image_pool &aligned_image_pool_,
							ns_registered_image_pool &registered_image_pool_){
		aligned_image_pool = &aligned_image_pool_;
		registered_image_pool = & registered_image_pool_;
	}*/
	ns_death_time_annotation::ns_exclusion_type excluded() const{
		return censoring_and_flag_details.excluded;
	}
	void set_time_path_limits(const ns_time_path_limits & limits){time_path_limits = limits;}

	const ns_time_path_limits & observation_limits() const{return time_path_limits;}
	ns_death_time_annotation_time_interval state_entrance_interval_time(const ns_movement_state_observation_boundary_interval & e) const;
	//ns_death_time_annotation_time_interval state_exit_interval_time(const ns_movement_state_observation_boundary_interval & e) const;

	unsigned long first_stationary_timepoint() const{return first_stationary_timepoint_;}
	ns_death_time_annotation_time_interval cessation_of_fast_movement_interval(){
		if (first_stationary_timepoint_== 0)
			return time_path_limits.interval_before_first_observation;
		return ns_death_time_annotation_time_interval(elements[first_stationary_timepoint_-1].absolute_time,elements[first_stationary_timepoint_].absolute_time);
	}


private:

		
	ns_time_path_limits time_path_limits;

	unsigned char * image_analysis_temp1,
				  * image_analysis_temp2;
	
	unsigned long first_stationary_timepoint_;

	bool images_preallocated;
	ns_death_time_annotation censoring_and_flag_details;

	unsigned long number_of_images_loaded;
	ns_image_storage_source_handle<ns_8_bit> movement_image_storage;
	ns_image_storage_source_handle<float> flow_movement_image_storage;
		
	ns_analyzed_time_path_quantification_summary quantification_summary;

	ns_analyzed_image_time_path_event_index find_event_index(const ns_movement_event & event_to_align);
	
	ns_analyzed_image_time_path_event_index find_event_index_with_fallback(const ns_movement_event & event_to_align);

	typedef std::vector<ns_movement_state_observation_boundary_interval> ns_state_interval_list;
	ns_state_interval_list state_intervals; //1 after the last index belonging to that state
	
	std::vector<ns_death_time_annotation_time_interval> by_hand_annotation_event_times;

	void quantify_movement(const ns_analyzed_time_image_chunk & chunk);
	//generates path_aligned_image from region visualiation
	bool populate_images_from_region_visualization(const unsigned long time,const ns_image_standard & region_image,const ns_image_standard & interpolated_region_image,bool just_do_a_consistancy_check);
	bool region_image_is_required(const unsigned long time,const bool interpolated, const bool direction_is_backwards);

	
	void add_by_hand_annotations(const ns_death_time_annotation_set & e);
	void add_by_hand_annotations(const ns_death_time_annotation_compiler_location & l);

	void detect_death_times_and_generate_annotations_from_movement_quantification(const ns_stationary_path_id & path_id,const ns_analyzed_image_time_path_death_time_estimator * e,  ns_death_time_annotation_set & set, const unsigned long last_timepoint_in_analysis);
	//ns_64_bit stationary_histogram[256];
	//ns_64_bit movement_histogram[256];

	ns_analyzed_time_image_chunk initiate_image_registration(const ns_analyzed_time_image_chunk & chunk,ns_alignment_state & state, ns_calc_best_alignment_fast & align);
	void calculate_image_registration(const ns_analyzed_time_image_chunk & chunk,ns_alignment_state & state, const ns_analyzed_time_image_chunk & first_chunk_to_register, ns_calc_best_alignment_fast & align);
	void calculate_movement_images(const ns_analyzed_time_image_chunk & chunk);
	void copy_aligned_path_to_registered_image(const ns_analyzed_time_image_chunk & chunk);


	void analyze_movement(const ns_analyzed_image_time_path_death_time_estimator * movement_death_time_estimator,const ns_stationary_path_id & path_id,const unsigned long last_timepoint_in_analysis);

	ns_image_storage_reciever_handle<ns_8_bit> * output_reciever;
	ns_image_storage_reciever_handle<float> * flow_output_reciever;
	ns_death_time_annotation_set death_time_annotation_set;

	bool low_density_path;



	ns_image_server_image output_image;
	ns_image_server_image flow_output_image;
	ns_64_bit path_db_id;

	unsigned long find_minimum_first_chunk_time() const{
		if (elements.size() < alignment_time_kernel_width)
			throw ns_ex("Path is too short!");
		return elements[alignment_time_kernel_width].absolute_time;
	}
	ns_element_list elements;
	//std::vector<unsigned long> movement_transition_times;
	const ns_time_path * path;
	unsigned long path_id,group_id;
	const ns_time_path_solution * solution;

	void initialize_movement_image_loading(ns_image_storage_source_handle<ns_8_bit> & in,ns_image_storage_source_handle<float> & flow_in,const bool read_only_backwards_frames);
	void initialize_movement_image_loading_no_flow(ns_image_storage_source_handle<ns_8_bit> & in, const bool read_only_backwards_frames);
	void end_movement_image_loading();
	
	ns_movement_image_collage_info movement_loading_collage_info;
	ns_image_stream_static_buffer<ns_8_bit> movement_loading_buffer;
	ns_image_stream_static_buffer<float> flow_movement_loading_buffer;
	ns_64_bit region_info_id;

	friend struct ns_analyzed_image_time_path_group;
	friend class  ns_time_path_image_movement_analyzer;
	friend class ns_size_data_accessor;
	friend class ns_intensity_data_accessor;
	friend class ns_movement_data_accessor;
	//we are allocating a ton of path_aligned images for each element.
	//we keep a pool to cut down on the time spent calling malloc()
	ns_time_path_image_movement_analysis_memory_pool * memory_pool;
	ns_image_stream_static_buffer<ns_8_bit> save_image_buffer;
	ns_image_stream_static_buffer<float> save_flow_image_buffer;

	//after the flow object has been set up, this function is called to calculate
	//the flow and save it to the data specified by element_index
	void calculate_flow(const unsigned long element_index);
	ns_optical_flow_processor * flow;
};

struct ns_analyzed_image_time_path_group{
	ns_analyzed_image_time_path_group(const unsigned long group_id_, const ns_64_bit region_info_id,const ns_time_path_solution & solution_, const ns_death_time_annotation_time_interval & observation_interval, ns_death_time_annotation_set & rejected_annotations,ns_time_path_image_movement_analysis_memory_pool & memory_pool_);
	std::vector<ns_analyzed_image_time_path> paths;
	void clear_images();
};
struct ns_movement_analysis_shared_state;
class ns_time_path_image_movement_analyzer {
public:
	ns_time_path_image_movement_analyzer():paths_loaded_from_solution(false),
		movement_analyzed(false),region_info_id(0),last_timepoint_in_analysis_(0),
		number_of_timepoints_in_analysis_(0),image_db_info_loaded(false),externally_specified_plate_observation_interval(0,ULONG_MAX){}

	~ns_time_path_image_movement_analyzer(){
		groups.clear();//do this first, ensuring all memory is returned to the pools
		memory_pool.aligned_image_pool.clear(); //then empty the pools.
		memory_pool.registered_image_pool.clear();
	}
	std::string analysis_description(const ns_movement_data_source_type::type & type)const{
		if (type != ns_movement_data_source_type::ns_time_path_image_analysis_data) 
			throw ns_ex("ns_time_path_image_movement_analyzer::Cannot generate movement description series for specified data source: ") << ns_movement_data_source_type::type_string(type);
		return "Time Path Image Analysis";
	}
	
	void add_by_hand_annotations(const ns_death_time_annotation_compiler & annotations);
	void output_allocation_state(const std::string & stage, long timepoint, std::ostream & out) const;
	void output_allocation_state_header(std::ostream & out) const;

	//three ways to populate the movement quantification data
	void process_raw_images(const ns_64_bit region_id,const ns_time_path_solution & solution_,const ns_time_series_denoising_parameters &,const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql, const long group_number=-1,const bool write_status_to_db=false);
	void reanalyze_stored_aligned_images(const ns_64_bit region_id,const ns_time_path_solution & solution_,const ns_time_series_denoising_parameters &,const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql,const bool load_images_after_last_valid_sample, const bool recalculate_flow_images);
	bool load_completed_analysis(const ns_64_bit region_id,const ns_time_path_solution & solution_, const ns_time_series_denoising_parameters &,const ns_analyzed_image_time_path_death_time_estimator * e,ns_sql & sql, bool exclude_movement_quantification=false);
	
	void reanalyze_with_different_movement_estimator(const ns_time_series_denoising_parameters &,const ns_analyzed_image_time_path_death_time_estimator * e);

	//provide access to group images
	void load_images_for_group(const unsigned long group_id, const unsigned long number_of_images_to_load,ns_sql & sql,const bool load_images_after_last_valid_sample,const bool load_flow_images);
	void clear_images_for_group(const unsigned long group_id);

	void clear(){
		groups.clear();
	}
	void clear_annotations(){
		for (unsigned int i = 0; i < groups.size(); i++){
			for (unsigned int j = 0; j < groups[i].paths.size(); j++)
				groups[i].paths[j].death_time_annotation_set.clear();
		}
			
	}
	void save_movement_data_to_db(const ns_64_bit region_id, ns_sql & sql);
	void delete_from_db(const ns_64_bit region_id,ns_sql & sql);
	
	
	void write_summary_movement_quantification_analysis_data(const ns_region_metadata & m, std::ostream & o)const;

	void write_detailed_movement_quantification_analysis_data(const ns_region_metadata & m, std::ostream & o,const bool only_output_elements_with_by_hand_data,const long specific_animal_id=-1, const bool abbreviated_time_series=false)const;
	void write_analysis_optimization_data(const std::vector<double> & thresholds, const std::vector<double> & hold_times, const ns_region_metadata & m,std::ostream & o) const;
	
	void output_visualization(const std::string & base_directory) const;

	void produce_death_time_annotations(ns_death_time_annotation_set & set) const;
	
	const ns_worm_movement_description_series & movement_description_series(const ns_movement_data_source_type::type & type) const{
		if (type != ns_movement_data_source_type::ns_time_path_image_analysis_data) 
			throw ns_ex("ns_time_path_image_movement_analyzer::Cannot generate movement description series for specified data source: ") << ns_movement_data_source_type::type_string(type);
		return description_series;
	}
	
	void generate_movement_posture_visualizations(const bool include_motion_graphs,const ns_64_bit region_id,const ns_time_path_solution & solution,ns_sql & sql);

	void generate_death_aligned_movement_posture_visualizations(const bool include_motion_graphs,const ns_64_bit region_id,const ns_movement_event & event_to_align, const ns_time_path_solution & solution,ns_sql & sql);

	inline const ns_analyzed_image_time_path_group & operator[] (const unsigned long & l)const{return groups[l];}
	inline const ns_analyzed_image_time_path_group & group(const unsigned long & l) const {return groups[l];}

	inline ns_analyzed_image_time_path_group & operator[] (const unsigned long & l){return groups[l];}
	inline ns_analyzed_image_time_path_group & group(const unsigned long & l) {return groups[l];}

	inline const std::vector<ns_analyzed_image_time_path_group>::size_type size()const{return groups.size();}
	
	void divide_job_into_chunks(std::vector<ns_analyzed_time_image_chunk> & chunks);

	void mark_path_images_as_cached_in_db(const ns_64_bit region_id, ns_sql & sql);
	unsigned long last_timepoint_in_analysis()const {return last_timepoint_in_analysis_;}
	unsigned long number_of_timepoints_in_analysis()const { return number_of_timepoints_in_analysis_;}
	ns_64_bit db_analysis_id() const{return analysis_id;}
private:

	void run_group_for_current_backwards_round(unsigned int group_id, const unsigned int path_id, ns_calc_best_alignment_fast *,ns_movement_analysis_shared_state * shared_state);
	void run_group_for_current_forwards_round(unsigned int group_id, unsigned int path_id, ns_calc_best_alignment_fast *,ns_movement_analysis_shared_state * shared_state);
	
	ns_stationary_path_id generate_stationary_path_id(const unsigned long group_id, const unsigned long path_id) const{
		return ns_stationary_path_id(group_id,path_id,analysis_id);
	}
	void populate_movement_quantification_from_file(ns_sql & sql, const bool skip_movement_data);
	
	static ns_death_time_annotation_time_interval get_externally_specified_last_measurement(const ns_64_bit region_id, ns_sql & sql);
	void crop_path_observation_times(const ns_death_time_annotation_time_interval & observation_interval);
	bool image_db_info_loaded;
	unsigned long last_timepoint_in_analysis_,
				 number_of_timepoints_in_analysis_;
	void load_movement_data_from_disk(std::istream & i,bool skip_movement_data=false);
	void save_movement_data_to_disk(std::ostream & o) const;

	ns_64_bit analysis_id;
	void get_processing_stats_from_solution(const ns_time_path_solution & solution_);
	ns_64_bit calculate_division_size_that_fits_in_specified_memory_size(const ns_64_bit & mem, const int multiplicity_of_images) const;

	void load_from_solution(const ns_time_path_solution & solution, const long group_number=-1);
	void load_region_visualization_images(const unsigned long start_i, const unsigned long stop_i,const unsigned int start_group, const unsigned int stop_group,ns_sql & sql, bool just_do_a_consistancy_check,bool running_backwards);
	void acquire_region_image_specifications(const ns_64_bit region_id,ns_sql & sql);
	void get_output_image_storage_locations(const ns_64_bit region_id,ns_sql & sql,const bool create_only_flow);
	bool load_movement_image_db_info(const ns_64_bit region_id,ns_sql & sql);
	void generate_images_from_region_visualization();
	bool paths_loaded_from_solution;
	std::vector<ns_analyzed_image_time_path_group> groups;
	std::vector<ns_analyzed_image_specification> region_image_specifications;

	const ns_time_path_solution * solution;
	void generate_movement_description_series();
	void normalize_movement_scores_over_all_paths(const ns_time_series_denoising_parameters &);
	ns_worm_movement_description_series description_series;

	ns_time_series_denoising_parameters denoising_parameters_used;

	ns_death_time_annotation_set extra_non_path_events;
	
	ns_image_standard image_loading_temp,image_loading_temp2;
	bool movement_analyzed;
	ns_64_bit region_info_id;

	ns_time_path_image_movement_analysis_memory_pool memory_pool;

	ns_death_time_annotation_time_interval externally_specified_plate_observation_interval;

	void calculate_memory_pool_maximum_image_size(const unsigned int start_group,const unsigned int stop_group);
	
};

struct ns_position_info{
	ns_vector_2i position,
				 size;
};
struct ns_movement_visualization_summary_entry{
	ns_vector_2i event_location()const{return worm_in_source_image.position+worm_in_source_image.size/2;}
	ns_position_info worm_in_source_image,
					 path_in_source_image,
					 path_in_visualization,
					 metadata_in_visualizationA;
	ns_stationary_path_id stationary_path_id;
	ns_death_time_annotation_time_interval path_time;
	unsigned long image_time;
};

class ns_movement_posture_visualization_summary{
public:
	ns_64_bit region_id;
	unsigned long frame_number;
	unsigned long alignment_frame_number;
	std::vector<ns_movement_visualization_summary_entry> worms;
	void to_xml(std::string & text);
	void from_xml(const std::string & text);
};


#endif
