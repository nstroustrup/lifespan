#ifndef NS_WORM_T_PATH
#define NS_WORM_T_PATH
#include <ostream>
#include "ns_detected_worm_info.h"
#include "ns_vector_bitmap_interface.h"
#include "ns_time_path_solver_parameters.h"


struct ns_worm_detection_results_set{
	std::vector<ns_image_worm_detection_results> results;
};

struct ns_time_element_link{
	ns_time_element_link(){}
	ns_time_element_link(const unsigned long t,const unsigned long i):t_id(t),index(i){}
	unsigned long t_id,index;
};

struct ns_time_path_element{
	ns_time_path_element():slowly_moving(false),inferred_animal_location(false),low_temporal_resolution(false),
			element_before_fast_movement_cessation(false),part_of_a_multiple_worm_disambiguation_cluster(false),worm_(0),number_of_extra_worms_identified_at_location(false){}
	
	ns_time_path_element(const ns_detected_worm_info * w):
		region_position(w->region_position_in_source_image),region_size(w->region_size),
			context_image_position(w->context_position_in_source_image),inferred_animal_location(false),
			element_before_fast_movement_cessation(false),
			part_of_a_multiple_worm_disambiguation_cluster(w->part_of_a_multiple_worm_cluster),
			context_image_size(w->context_image_size),center(w->region_position_in_source_image+w->region_size/2),slowly_moving(false),
			low_temporal_resolution(false),worm_(w),context_image_position_in_region_vis_image(0,0),number_of_extra_worms_identified_at_location(0){
		if (context_image_size.x == 0)
			throw ns_ex("Empty Context Image Found");
		}
	ns_vector_2i region_position;
	ns_vector_2i region_size;
	ns_vector_2i context_image_position;
	ns_vector_2i context_image_size;
	ns_vector_2i center;
	bool slowly_moving;
	bool low_temporal_resolution;
	const ns_detected_worm_info & worm() const{return *worm_;}
	bool worm_is_loaded()const{return worm_ != 0;}
	unsigned long number_of_extra_worms_identified_at_location;
	bool part_of_a_multiple_worm_disambiguation_cluster;
	ns_plate_subregion_info subregion_info;
	ns_death_time_annotation volatile_by_hand_annotated_properties;

	ns_vector_2i context_image_position_in_region_vis_image;
	bool inferred_animal_location,
		 element_before_fast_movement_cessation;

private:
	const ns_detected_worm_info * worm_;
};

struct ns_time_path_element_time_sorter{
	bool operator()(const ns_time_path_element & e1, const ns_time_path_element & e2){
		return false;
	}
};

struct ns_time_path{
	ns_time_path():is_low_density_path(false){}
	std::vector<ns_time_element_link> stationary_elements;
	std::vector<ns_time_element_link> moving_elements;
	bool is_low_density_path;
//	bool is_not_stationary;
	ns_vector_2i center;
};
struct ns_time_path_group{
	std::vector<unsigned long> path_ids;
};

struct ns_time_path_timepoint{
	unsigned long time;
	ns_64_bit sample_region_image_id;
	std::vector<ns_time_path_element> elements;
};
class ns_time_path_solution {
public:
	static long default_length_of_fast_moving_prefix();
	ns_time_path_solution() :detection_results(0), worms_loaded(false) {}
	~ns_time_path_solution() { if (detection_results != 0)delete detection_results; detection_results = 0; }
	static void output_visualization_csv_header(std::ostream & o) {
		o << "t,t_abs,x,y,w,h,path_group_id,path_id,slowly_moving,low_temporal_resolution,hand_annotation,inferred,before_first_stationary,number_of_extra_worms,movement, plate_subregion_id, plate_nearest_neighbor_subregion_id, subregion_info,nearest_neighbor_distance_x,nearest_neighbor_distance_y,excluded_by_hand\n";
	};
	static void output_visualization_csv_data(std::ostream & o, const float relative_time, const unsigned long absolute_time, const ns_vector_2i & pos, const ns_vector_2i & size,
		const long path_id, const long path_group_id, const bool slowly_moving,
		const bool low_temporal_resolution, const bool by_hand_annotation, const bool inferred_worm, const bool before_first_stationary_measurement, const unsigned long number_of_extra_worms,
		const ns_movement_state & movement, const ns_plate_subregion_info & subregion_info, const ns_death_time_annotation & by_hand_annotations) {
		o << relative_time << "," << absolute_time << "," << pos.x << "," << pos.y << "," << size.x << "," << size.y << ","
			<< path_id << "," << (path_group_id ? "1" : "0") << "," << (slowly_moving ? "1" : "0") << "," << (low_temporal_resolution ? "1" : "0") << "," << (by_hand_annotation ? "1" : "0") << ","
			<< (inferred_worm ? "1" : "0") << "," << (before_first_stationary_measurement ? "1" : "0") << ","
			<< number_of_extra_worms << "," << (int)movement << "," << subregion_info.plate_subregion_id << "," << subregion_info.nearest_neighbor_subregion_distance << "," << subregion_info.nearest_neighbor_subregion_distance.x << "," << subregion_info.nearest_neighbor_subregion_distance.y << "," 
			<< (by_hand_annotations.is_excluded()?"1":"0") << "\n";
	}
	//output worm positions in comma separated value format
	void output_visualization_csv(std::ostream & o) const;

	//output worm positions in WCON format
	void output_visualization_wcon(std::ostream & o) const;

	void output_mathematica_file(std::ostream & o);
	bool worms_loaded;
	bool paths_loaded()const { return paths.size() != 0; }
	ns_time_path_element & element(const ns_time_element_link & e) { return timepoints[e.t_id].elements[e.index]; }

	const ns_time_path_element & element(const ns_time_element_link & e) const { return timepoints[e.t_id].elements[e.index]; }
	const unsigned long & time(const ns_time_element_link & e) const { return timepoints[e.t_id].time; }
	unsigned long & time(const ns_time_element_link & e) { return timepoints[e.t_id].time; }

	void fill_gaps_and_add_path_prefixes(const unsigned long prefix_length);
	void remove_inferred_animal_locations(const unsigned long timepoint_index, bool delete_uninferred_animals_also);
	std::vector<ns_time_path_timepoint> timepoints;
	std::vector<ns_time_path> paths;
	std::vector<ns_time_path_group> path_groups;

	void clear() {
		if (detection_results != 0)delete detection_results;
		detection_results = 0;
		timepoints.resize(0);
		paths.resize(0);
		path_groups.resize(0);
		unassigned_points.stationary_elements.resize(0);
		unassigned_points.center = ns_vector_2i(0, 0);
		worms_loaded = false;
	}
	void unlink_detection_results() {
		detection_results = 0;
	}
	ns_time_path unassigned_points;

	void set_results(ns_worm_detection_results_set * results) { if (detection_results != 0)delete detection_results; detection_results = results; }

	bool remove_invalidated_points(const ns_64_bit region_id, const ns_time_path_solver_parameters &param, ns_sql & sql);
	void save_to_db(const ns_64_bit region_id, ns_sql & sql) const;
	void load_from_db(const ns_64_bit region_id, ns_sql & sql, bool load_directly_from_disk_without_db);
	void save_to_disk(std::ostream & o) const;
	void load_from_disk(std::istream & o);
	void check_for_duplicate_events();
	bool identify_subregions_labels_from_subregion_mask(const ns_64_bit region_id, ns_sql & sql);

private:

	ns_worm_detection_results_set * detection_results;
};




class ns_time_path_solver_element{
public:
	ns_time_path_solver_element():path_id(0),recursively_named(false),element_assigned(false),element_assigned_in_this_round(false){}

	ns_time_path_element e;
	std::vector<ns_time_path_element> extra_elements_at_current_position;
	
	inline bool overlap(const ns_time_path_solver_element & el) const
		{return ns_rectangle_intersect(e.region_position,e.region_position+e.region_size,el.e.region_position,el.e.region_position+el.e.region_size);}

	void load(const ns_detected_worm_info * w);

	unsigned long path_id,
			  recursively_named;
	mutable bool element_assigned,
		 element_assigned_in_this_round;

};

struct ns_time_path_solver_timepoint{
	unsigned long time;
	ns_64_bit sample_region_image_id;

	std::vector<ns_time_path_solver_element> elements;
	bool combine_very_close_elements(const unsigned long max_d_squared);
	void load(const ns_64_bit worm_detection_results_id,ns_image_worm_detection_results & results,ns_sql & sql);

	ns_64_bit worm_results_id;
private:
	ns_image_worm_detection_results * worm_detection_results;
};

struct ns_time_path_solver_path_builder_point{
	ns_time_path_solver_path_builder_point():link(0,0),pos(0,0),time(0){}
	ns_time_path_solver_path_builder_point(const ns_time_element_link & l, const ns_vector_2i & p, const unsigned long t):link(l),pos(p),time(t){}
	ns_time_element_link link;
	ns_vector_2i pos;
	unsigned long time;
};
struct ns_time_path_solver_path_builder{

	ns_time_path_solver_path_builder():center(0,0){}
	ns_time_path_solver_path_builder(const ns_time_path_solver_path_builder_point & p):center(p.pos),elements(1,p){}
	
	double calculate_current_density(const unsigned long time_density_window, const std::vector<ns_time_path_solver_timepoint> & all_timepoints,const unsigned long current_timepoint_i) const;

	void calculate_center(const unsigned long time_density_window) const;
	mutable ns_vector_2d center;
	std::vector<ns_time_path_solver_path_builder_point> elements;
};

struct ns_time_path_solver_path{
	ns_time_path_solver_path():is_low_density_path(false),is_not_stationary(false){}
	
	unsigned long path_id,
				  group_id;
	ns_vector_2i max_time_position,
				 min_time_position;
	unsigned long max_time,
		          min_time;
	std::vector<ns_time_element_link> elements;
	bool is_low_density_path;
	bool is_not_stationary;

	ns_time_element_link & end(){return elements[elements.size()-1];}

	ns_time_path_solver_path(const ns_time_path_solver_path_builder & b){
		elements.resize(b.elements.size());
		is_low_density_path =false;
		is_not_stationary =false;
		if (b.elements.empty()){
			max_time_position = min_time_position = ns_vector_2i(0,0);
			min_time = max_time = 0;
			return;
		}
		for (unsigned int i = 0; i < elements.size(); i++)
			elements[i] = b.elements[i].link;
		max_time = b.elements.begin()->time;
		max_time_position = b.elements.begin()->pos;
		min_time = b.elements.rbegin()->time;
		min_time_position = b.elements.rbegin()->pos;
	}
};



struct ns_time_path_solver_path_group{
	std::vector<ns_64_bit> path_ids;
};


struct ns_estimator_parameters{
	ns_vector_2d estimation_position;
	ns_vector_2d estimation_drift;
	unsigned long estimation_time;
};


//in a variety of contexts, we'll want to know how near a point is near a path's location.
//at points observed at times earlier or later than the duration where the path is defined,
//we'll need to predict where that path would have been had it been defined at the point's time.
//Since paths often drift a little bit over time,
//we cant just use the last point on the path, we need to model the drift
//to make the best estimate as to where the path would have drifted too.
//We choose a linear model and assume animals move to positions (x,y) = (m_x,m_y)*(t-t_offset) + (bx,by)
//the t_offset is there just to prevent the t's from getting too large and producing really small
//m_x's and m_y's and causing numerical precision errors.

//Note for single points, we can't estimate drift, so we just set
//m_x and m_y to zero and assume no drift.
class ns_time_path_solution_stationary_drift_estimator{
public:
	
	ns_time_path_solution_stationary_drift_estimator():path_id(-1),stray_point_id(0,0),path_bound_min(0),path_bound_max(0){}

	//use the linear model to estimate where the path would be at requested time
	ns_vector_2d estimate(const unsigned long time) const;

	bool within_time_span(unsigned long time){
		return time >= path_bound_min && time <= path_bound_max;
	}
	//paths may behave differently early and late in the experiment
	//so we make two different models; one describing drift at the start
	//of the interval in which the path is defined
	//and another describing drift at the end of the interval in which
	//the path is defined
	ns_estimator_parameters early_parameters,
							late_parameters;
	//path id and stray_point_id point to elements
	//in the time_path_solver data structures
	//from which the estimator was generated.
	//Either one or the other is specified
	//as an estimator can come from only one source.
	bool is_a_path()const {return path_id !=-1;}
	long path_id;
	ns_time_element_link stray_point_id;

	//the earliest and latest points on the path
	unsigned long path_bound_min,path_bound_max;

};

bool operator < (const ns_time_path_solution_stationary_drift_estimator & l, const ns_time_path_solution_stationary_drift_estimator & r);

//conatins multiple estimators that have been merged by the merging algorithm
//handle_low_density_stationary_paths_and_stray_points()
class ns_time_path_solution_stationary_drift_estimator_group{
public:
	typedef std::set<ns_time_path_solution_stationary_drift_estimator> ns_estimator_list;
	ns_estimator_list estimators;
	ns_time_path_solution_stationary_drift_estimator_group():group_estimator_specified_(false){}
	ns_time_path_solution_stationary_drift_estimator_group(const ns_time_path_solution_stationary_drift_estimator & e):group_estimator_specified_(false){estimators.insert(e);}
	bool within_time_span(const unsigned long time){
		return (estimators.rbegin()->path_bound_min <= time && time <= estimators.begin()->path_bound_max);
	}
	const ns_time_path_solution_stationary_drift_estimator & earliest() const{return (*estimators.rbegin());}
	const ns_time_path_solution_stationary_drift_estimator & latest() const{return (*estimators.begin());}
	/*bool time_does_not_overlap(const unsigned long & time) const{ 
		for (ns_estimator_list::const_iterator p=estimators.begin(); p!= estimators.end(); p++)
				if (!p->time_does_not_overlap(time))
					return false;
		return true;
	}*/

	bool use_as_a_target,
		 use_as_a_estimator;
	bool to_be_deleted;
	
	//In some contexts we'll need to combine individual estimators into a single path
	//and use that path to do temporary math
	//generate paths makes this path and caches it to prevent unneccisary recalculation
	const ns_time_path_solver_path & generate_path(const std::vector<ns_time_path_solver_path> & paths) const;
	//changes to the group require recalculation, which can be triggered by clearing the cache
	void clear_cached_path() const;

	//sometimes estimators are created from the cached path
	//these remain useful until the gorup is modified, and can 
	//be cached here by specifify_group_estimator().
	//The actual calculation of the estimator can't be directly
	//handled by the group object because it doesn't have access to all
	//the needed private members of the time_path_solver object.
	void specify_group_estimator(const ns_time_path_solution_stationary_drift_estimator & e) const{
		group_estimator_ = e;
		group_estimator_specified_ = true;
	}
	const ns_time_path_solution_stationary_drift_estimator & group_estimator()const{return group_estimator_;}
	bool group_esitmator_specified() const{return group_estimator_specified_;}

private:
	mutable ns_time_path_solver_path path_cache_;
	mutable ns_time_path_solution_stationary_drift_estimator group_estimator_;
	mutable bool group_estimator_specified_;

};



class ns_time_path_solver{
public:
	//aggregates the set of worm positions from saved individual frames
	void load(ns_64_bit region_id, ns_sql & sql);
	//identifies staitonary objects in the 3d point cloud of worm positions
	void solve(const ns_time_path_solver_parameters &parameters, ns_time_path_solution & solution, ns_image_server_sql * sql_for_debug_output_only);
	
	void output_visualization_csv(std::ostream & o);

	//Sometimes we may want to aggregate the worm positions and save the result to a local file
	//before solving them.
	//This can be done by creating the "raw" (unsolved) solution and saving it to disk.
	//that file can then be reloaded and solved later.
	//This is great for debugging as the aggregation step can take a lot longer than just loading
	//the result from disk.
	void generate_raw_solution(ns_time_path_solution & solution);
	void load_from_raw_solution(const ns_64_bit region_id,const ns_time_path_solution & solution,ns_sql & sql);

	ns_time_path_solver():detection_results(new ns_worm_detection_results_set){}
	~ns_time_path_solver(){if (detection_results != 0) delete detection_results; detection_results = 0;}
	bool empty() const { return timepoints.empty(); }
	static void load_worm_detection_ids(const ns_64_bit region_id, ns_sql & sql, unsigned long & time_of_last_valid_sample, ns_sql_result & res);
private:
	//all measurement times, each containing the objects detected at that time
	std::vector<ns_time_path_solver_timepoint> timepoints;
	//each stationary object
	std::vector<ns_time_path_solver_path> paths;
	//staitonary objects can be grouped into "clusters"
	//Currently we don't do this so there's a one to one mapping between paths[] and path_groups[]
	std::vector<ns_time_path_solver_path_group> path_groups;
	//stationary objects may not be detected in every observation.
	//objects that are obviously stationary but very rately detected
	//are stored as paths in low_density_paths
	std::vector<ns_time_path_solver_path> low_density_paths;

	//used for loading worm images etc.  Is this ever actually used?
	ns_worm_detection_results_set * detection_results;
	
	std::vector<char> path_assign_temp;
	//mark all points as unassigned in the timepoints[] structure.
	//this is important for finding stray points,
	//preventing various algorithms for assigning single points to multiple 
	//paths, etc.
	void mark_unassigned_points();

	//generate path ids and write them to
	//the points in the timepoints[] structure
	void assign_path_ids_to_elements();

	//find high density paths and merge them together when appropriate
	void find_stationary_path_fragments(const double min_path_density_in_points_per_hour, const unsigned long min_path_duration_in_seconds, const unsigned long time_window_length_in_seconds,const unsigned long max_movement_distance);
	void merge_overlapping_path_fragments(const unsigned long max_center_distance,const unsigned long max_time_gap, const unsigned long max_time_overlap, const double max_fraction_points_overlap);


	//find low-density paths, and when possible merge them into the high-density paths
	void find_low_density_stationary_paths(const unsigned long min_path_duration_in_seconds, const unsigned long max_movement_distance);
	void handle_low_density_stationary_paths_and_stray_points(const unsigned long max_movement_distance, const double min_final_stationary_path_duration_in_minutes, ns_image_server_sql * sql_for_debug_output_only);

	//used during high-density path detection algorithm
	void assign_timepoint_elements_to_paths(std::vector<ns_time_path_solver_element> & elements, const unsigned long max_dist_sq, std::vector<ns_time_path_solver_path_builder> & paths);


	//make a solution out of the contents of timepoints[] and paths[]
	void transfer_data_to_solution(ns_time_path_solution & solution);
	
	//paths[] contains links into timepoints; to get actual position data
	//the links need to be looked up
	inline const unsigned long time(const ns_time_element_link & l) const { return timepoints[l.t_id].time;}
	inline const ns_time_path_solver_element & element(const ns_time_element_link & l) const { return timepoints[l.t_id].elements[l.index];}
	inline ns_time_path_solver_element & element(const ns_time_element_link & l){ return timepoints[l.t_id].elements[l.index];}

	//see documentation for the class ns_time_path_solution_stationary_drift_estimator
	ns_time_path_solution_stationary_drift_estimator get_drift_estimator(const  unsigned long path_id) const;
	ns_time_path_solution_stationary_drift_estimator get_drift_estimator(const ns_time_path_solver_path & path) const;

	unsigned long number_of_stationary_paths_at_time(const unsigned long t) const;
	unsigned long number_of_unassigned_points_at_time(const ns_time_element_link & l) const;
	bool can_search_for_stray_points_at_time(const ns_time_element_link & l) const;
	
	//find various statistics paths.
	//these aren't defined as members of ns_time_path_solver_path
	//because that class doesn't have access to the private member timepoints[]
	ns_vector_2i find_max_time_position(const ns_time_path_solver_path & p) const;
	ns_vector_2i find_min_time_position(const ns_time_path_solver_path & p) const;
	ns_vector_2i find_local_median_position(const ns_time_path_solver_path & p, const unsigned long i) const;
	bool time_index_is_in_path(const unsigned long path_id,const ns_time_element_link & e) const;
	bool time_index_is_in_path(const ns_time_path_solver_path & p,const ns_time_element_link & e) const;

	//used in high and low density path algorithm merging
	static bool is_ok_to_merge_overlap(const ns_time_path_solver_path & a, const ns_time_path_solver_path & b,const unsigned long max_time_gap, const unsigned long max_time_overlap,const double max_fraction_points_overlap) ;
	bool is_ok_to_merge_overlap(const ns_time_path_solution_stationary_drift_estimator_group & later, const ns_time_path_solution_stationary_drift_estimator_group & earlier,const unsigned long max_time_gap, const unsigned long max_time_overlap,const double max_fraction_points_overlap) const;
	bool merge_estimator_groups(const unsigned long max_dist_sq, std::vector<ns_time_path_solution_stationary_drift_estimator_group> & e) const;
	
	//remove duplicate observations created by spurious worm detections.
	void handle_paths_with_ambiguous_points();

	//enforce various desired restrictions on stationary worms
	void remove_short_and_moving_paths(const ns_time_path_solver_parameters & p);

	//lots of documentation available in the definition of the class ns_time_path_solution_stationary_drift_estimator_group
	//and also in function body definition.
	ns_vector_2d estimate(const ns_time_path_solution_stationary_drift_estimator_group & g,const unsigned long time) const;

	//load the detection results for aggregated point cloud from the database
	void load_detection_results(ns_64_bit region_id,ns_sql & sql);

	void break_paths_at_large_gaps(double fraction_max_gap_factor);
};

#endif
