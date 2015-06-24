#include "ns_time_path_solver.h"
#include "ns_image_server.h"
#include "hungarian.h"
#include "ns_linear_regression_model.h"
#include "ns_ini.h"
using namespace std;


const int ns_hungarian_impossible_value(std::numeric_limits<int>::max()/2);

ns_time_path_solver_parameters ns_time_path_solver_parameters::default_parameters(const unsigned long experiment_length_in_seconds,
														const unsigned long short_capture_interval_in_seconds_,
														const unsigned long number_of_consecutive_sample_captures_,
														const unsigned long number_of_samples_per_device_){
					
	ns_time_path_solver_parameters param;
	param.short_capture_interval_in_seconds = short_capture_interval_in_seconds_;
	param.number_of_consecutive_sample_captures = number_of_consecutive_sample_captures_;
	param.number_of_samples_per_device = number_of_samples_per_device_;

	const bool short_experiment(experiment_length_in_seconds <  4.5*24*60*60);
	param.min_stationary_object_path_fragment_duration_in_seconds = short_experiment?(3*60*60):(6*60*60);  
	param.maximum_time_gap_between_joined_path_fragments = short_experiment?3*24*60*60:12*60*60; 
	param.maximum_time_overlap_between_joined_path_fragments = short_experiment?1*60*60:2*60*60; 
	param.min_final_stationary_path_duration_in_minutes = short_experiment?1.5*60.0:9*60;
	
	param.maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment = .5;  //allow 25% of points to be missing
	param.stationary_object_path_fragment_window_length_in_seconds = 8.0*60.0*60.0/param.maximum_object_detection_density_in_events_per_hour();  // enough time to capture 8 events
	param.stationary_object_path_fragment_max_movement_distance = 20; //pixels
	param.maximum_fraction_duplicated_points_between_joined_path_fragments = .5;
	param.maximum_distance_betweeen_joined_path_fragments = 50;
	param.maximum_path_fragment_displacement_per_hour = .9;
	param.max_average_final_path_average_timepoint_displacement = 5;
	param.maximum_fraction_of_median_gap_allowed_in_low_density_paths = 10; //600% lager than median gap.
	return param;
}


ns_time_path_solver_parameters ns_time_path_solver_parameters::default_parameters(const ns_64_bit sample_region_image_info_id, ns_sql & sql,bool create_default_parameter_file_if_needed,bool load_from_file_if_possible){
	ns_sql_result res;
	if (load_from_file_if_possible){
		//Look to see if specific parameter set has been specified
		sql << "SELECT position_analysis_model FROM sample_region_image_info WHERE id="<<sample_region_image_info_id;
		
		sql.get_rows(res);	
		if (res.empty() || res[0].empty())
			throw ns_ex("ns_time_path_solver_parameters::default_paramers()::Could not find region id") << sample_region_image_info_id << " in the database!";
		if (res[0][0].size() > 0)
			return image_server.get_position_analysis_model(res[0][0],create_default_parameter_file_if_needed,sample_region_image_info_id,&sql);
	}
	sql << "SELECT s.id, s.device_name, s.experiment_id, s.device_capture_period_in_seconds,s.number_of_consecutive_captures_per_sample FROM capture_samples as s, sample_region_image_info as r WHERE r.id =" << sample_region_image_info_id << " AND r.sample_id = s.id";
	sql.get_rows(res);
	if (res.empty())
		throw ns_ex("ns_time_path_solver_parameters::default_paramers()::Could not find region id") << sample_region_image_info_id << " in the database!";
	
	const unsigned long device_capture_period_in_seconds(atol(res[0][3].c_str()));
	const unsigned long number_of_consecutive_captures_per_sample(atol(res[0][4].c_str()));

	if (device_capture_period_in_seconds == 0 || number_of_consecutive_captures_per_sample == 0)
		throw ns_ex("Found invalid capture specification information in db: Capture period = ") << device_capture_period_in_seconds << "s; Number of consecutive samples: " << number_of_consecutive_captures_per_sample;
	const unsigned long experiment_id(atol(res[0][2].c_str()));
	sql << "SELECT count(*) FROM capture_samples WHERE device_name = '" << res[0][1] << "' AND experiment_id = " << res[0][2];
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_solver_parameters::default_paramers()::Could not count the number of capture samples on the region's device");
	const unsigned long number_of_samples(atol(res[0][0].c_str()));
	sql << "SELECT first_time_point,last_time_point FROM experiments WHERE id = " << experiment_id;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_solver_parameters::default_paramers()::Could not find experiment boundaries!");
	const unsigned long experiment_duration(atol(res[0][1].c_str())-atol(res[0][0].c_str()));
	return default_parameters(experiment_duration,device_capture_period_in_seconds,number_of_consecutive_captures_per_sample,number_of_samples);
}
std::vector<unsigned long> x_persist,
							y_persist,
							t_persist;
ns_time_path_solution_stationary_drift_estimator ns_time_path_solver::get_drift_estimator(const unsigned long path_id) const{
	ns_time_path_solution_stationary_drift_estimator estimator(get_drift_estimator(paths[path_id]));
	estimator.path_id = paths[path_id].path_id;
	return estimator;
}

ns_time_path_solution_stationary_drift_estimator ns_time_path_solver::get_drift_estimator(const ns_time_path_solver_path & path) const{
	ns_time_path_solution_stationary_drift_estimator estimator;
	//estimator.path_id = path_id;
	int head_size(path.elements.size());
	if (head_size > 25)
		head_size = 25;
	if (head_size == 1){
		estimator.early_parameters.estimation_position = element(*path.elements.begin()).e.center;
		estimator.early_parameters.estimation_time = time(*path.elements.begin());
		estimator.early_parameters.estimation_drift = ns_vector_2d(0,0);
		estimator.late_parameters = estimator.early_parameters;
		return estimator;
	}
	std::vector<unsigned long> &x(x_persist),
							   &y(y_persist),
							   &t(t_persist);
	x.resize(head_size);
	y.resize(head_size);
	t.resize(head_size);

	estimator.early_parameters.estimation_time = time(path.elements[path.elements.size()-1]);
	for (unsigned long i = 0; i < head_size; i++){
		x[i] = element(path.elements[path.elements.size()-1-i]).e.center.x;
		y[i] = element(path.elements[path.elements.size()-i-1]).e.center.y;
		t[i] = time(path.elements[path.elements.size()-i-1]) - estimator.early_parameters.estimation_time;
	}
	ns_linear_regression_model_parameters p_x(ns_linear_regression_model::fit(x,t));
	ns_linear_regression_model_parameters p_y(ns_linear_regression_model::fit(y,t));
	estimator.early_parameters.estimation_position = ns_vector_2d(p_x.y_intercept,p_y.y_intercept);
	estimator.early_parameters.estimation_drift = ns_vector_2d(p_x.slope,p_y.slope);

	
	estimator.late_parameters.estimation_time = time(path.elements[0]);
	for (unsigned long i = 0; i < head_size; i++){
		x[i] = element(path.elements[i]).e.center.x;
		y[i] = element(path.elements[i]).e.center.y;
		t[i] = time(path.elements[i]) - estimator.late_parameters.estimation_time;
	}
	p_x = ns_linear_regression_model::fit(x,t);
	p_y = ns_linear_regression_model::fit(y,t);
	estimator.late_parameters.estimation_position = ns_vector_2d(p_x.y_intercept,p_y.y_intercept);
	estimator.late_parameters.estimation_drift = ns_vector_2d(p_x.slope,p_y.slope);


	//when trying to match other paths to this one
	//you'll want to find paths that connect with the earliest point in this path.
//	estimator.match_position = find_max_time_position(path);
	//estimator.match_time = time(*path.elements.begin());
	estimator.path_bound_max = path.max_time;
	estimator.path_bound_min = path.min_time;
	return estimator;
}

//oveflows aren't good.  out of a paranoia we don't assume standard two's complement
long ns_careful_subtract(unsigned long a, unsigned long b){
	return (a>b)?(long)(a-b):-(long)(b-a);
}

/*
bool ns_is_close2(const ns_vector_2d & p){
	//3176	5866
	//992	1211
	//2054	4692
	return (abs(p.x - 2054)<50 && abs(p.y-1211) < 4692);
		
}*/

ns_vector_2d ns_time_path_solution_stationary_drift_estimator::estimate(const unsigned long time) const{
	//if the point is close to the beginning of a path, we use the estimation parameters there.

	//this means we won't use the behavior around the latest point to predict the position at the begining
	//which would obviously be noiser than using the known position at beginning
	if (abs(ns_careful_subtract(time,early_parameters.estimation_time)) < abs(ns_careful_subtract(time,late_parameters.estimation_time)))
		return early_parameters.estimation_position+early_parameters.estimation_drift*ns_careful_subtract(time,early_parameters.estimation_time);
	
	//if the point is close to the end of a path (later times), we use the estimation paramaters there.
	return late_parameters.estimation_position+late_parameters.estimation_drift*ns_careful_subtract(time,late_parameters.estimation_time);
}

ns_vector_2d ns_time_path_solver::estimate(const ns_time_path_solution_stationary_drift_estimator_group & g,const unsigned long time) const{
	//if the group is only contains one estimator, 
	//estimation is easy; we just ask the estimator to do the calculation
	if (g.estimators.size() == 1)
		return g.estimators.begin()->estimate(time);
	if (g.group_esitmator_specified())
		return g.group_estimator().estimate(time);

	//however, if the group has multiple estimators
	//any individual one won't be able to capture
	//the expected behavior of the set
	//so we merge all estimators into a temporary path
	//and generate a new estimator from the merged.
	//we then use that to calculate the estimate.
	ns_time_path_solver_path path(g.generate_path(paths));
	//if ( (ns_vector_2i(2054,4692) - g.estimators.rbegin()->early_parameters.estimation_position).squared() < 50)
	//	cerr << "WHA";
	g.specify_group_estimator(get_drift_estimator(path));
	return g.group_estimator().estimate(time);
}
void ns_time_path_solver::remove_short_and_moving_paths(const ns_time_path_solver_parameters & param){
	//remove short paths
	unsigned long debug_max_length(0);
	for (std::vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();){
		if (((p->max_time-p->min_time)/60) < param.min_final_stationary_path_duration_in_minutes){
			if (debug_max_length < p->max_time-p->min_time)
				debug_max_length = p->max_time-p->min_time;
			p = paths.erase(p);
			continue;
		}
		else p++;
	}
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::::remove_short_and_moving_paths()::Number of paths after short paths removed: ") << paths.size());
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::::remove_short_and_moving_paths()::Longest short path removed: ") << debug_max_length);
	
	std::vector<double> deltas;
	//remove paths that move too much.
	for (std::vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();){
		if(p->elements.size() == 1){
			p++;
			continue;
		}
		deltas.resize(p->elements.size()-1);

		for (unsigned int i = 1; i < p->elements.size(); i++)
			deltas[i-1] = (element(p->elements[i]).e.center-element(p->elements[i-1]).e.center).squared();
		std::sort(deltas.begin(),deltas.end());
		double median_delta;
		if (deltas.size()%2 == 0)
			median_delta = sqrt((deltas[deltas.size()/2]+deltas[deltas.size()/2-1])/2.0);
		else median_delta = sqrt(deltas[deltas.size()/2]);
		//avg_delta/=p->elements.size();
		//if (ns_is_close2(element(p->elements[0]).e.center))
		//		cerr << "WOO";
		if (median_delta > param.max_average_final_path_average_timepoint_displacement)
			p = paths.erase(p);
		else p++;
	}
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::::remove_short_and_moving_paths()::Number of paths after moving paths removed: ") << paths.size());
}

void ns_splinter_path_fragment(const ns_time_path_solver_path & source, const unsigned long begin_i,const unsigned long end_i, ns_time_path_solver_path & destination){
			
	destination.is_low_density_path = source.is_low_density_path;
	destination.is_not_stationary = source.is_not_stationary;
	destination.elements.reserve(end_i - begin_i);
	for (unsigned int j = begin_i; j < end_i; j++){
//			if (source.elements[j].index == 113 &&
//					source.elements[j].t_id == 22)
//					cerr << "WHA";
		destination.elements.push_back(source.elements[j]);
	}
}

void ns_time_path_solver::break_paths_at_large_gaps(double max_gap_factor){
	const unsigned long number_of_paths(paths.size());
	vector<double> gap_sizes;
	unsigned long break_counts(0);
	unsigned long fragment_counts(0);
	for (unsigned int i = 0; i < number_of_paths; i++){
		if (paths[i].elements.size() == 1)
			continue;
		gap_sizes.resize(paths[i].elements.size()-1);
		for (unsigned int t = 1; t < paths[i].elements.size(); t++)
			gap_sizes[t-1] = time(paths[i].elements[t-1])- time(paths[i].elements[t]); 
		std::sort(gap_sizes.begin(),gap_sizes.end());
		double median_gap_size;
		if (gap_sizes.size()%2 == 0)
			median_gap_size = gap_sizes[gap_sizes.size()/2]/2.0+gap_sizes[gap_sizes.size()/2-1]/2.0;
		else median_gap_size = gap_sizes[gap_sizes.size()/2];
		long fragment_start(-1),
			 first_fragment_start(-1);

		float early_fraction_gaps_to_remove(.05);
		//cerr << "Path median gap: " << median_gap_size/60.0/60.0 << " h; Break at " << median_gap_size*max_gap_factor/60.0/60.0/24.0 << "d\n";
		for (unsigned int t = 1; t < paths[i].elements.size(); t++){
			const double gap_size((time(paths[i].elements[t-1])- time(paths[i].elements[t])));
			const bool gap_is_too_big(gap_size/median_gap_size > max_gap_factor);
			const bool near_the_end(t > (paths[i].elements.size())*(1.0-early_fraction_gaps_to_remove));
			if (near_the_end && gap_is_too_big){
		//		cerr << "Breaking gap width " << gap_size/60.0/60.0/24.0 
		//			<< "d at " << paths[i].elements[t].t_id << "(" << t << "," << paths[i].elements.size() << ") : " << element(paths[i].elements[t]).e.center << "\n";
				if (first_fragment_start == -1){
					first_fragment_start = t;
					fragment_start = t;
					continue;
				}
				if (t - fragment_start < 3){
					fragment_start = t;
					continue;
				}
				unsigned long s(paths.size());
				paths.resize(s+1);
				paths[s].path_id = s;
//				if (paths[i].elements[fragment_start].t_id == 113 &&
//					paths[i].elements[fragment_start].index == 22)
//					cerr << "WHA";
					
//				if (paths[i].elements[t].t_id == 113 &&
//					paths[i].elements[t].index == 22)
//					cerr << "WHA";
				ns_splinter_path_fragment(paths[i],fragment_start,t,paths[s]);
				paths[s].max_time = time(*paths[s].elements.begin());
				paths[s].min_time = time(*paths[s].elements.rbegin());
				fragment_start = t;
				fragment_counts++;
			}
		}
		if (fragment_start != -1){
			break_counts++;
			const double gap_size((time(paths[i].elements[paths[i].elements.size()-2])
									- time(paths[i].elements[paths[i].elements.size()-1])));
	//		cerr << "Breaking gap width " << gap_size/60.0/60.0/24.0
	//			<< " at " << paths[i].elements[fragment_start].t_id << ":(" << fragment_start << "," << paths[i].elements.size() << "):  " << element(paths[i].elements[paths[i].elements.size()-1]).e.center << "\n";
			//const bool near_the_end(fragment_start > (95*paths[i].elements.size())/100);
			//if (near_the_end){
			if (paths[i].elements.size() - fragment_start >= 3){
				unsigned long s(paths.size());
				paths.resize(s+1);
				paths[s].path_id = s;
//				if (paths[i].elements[fragment_start].t_id == 113 &&
//					paths[i].elements[fragment_start].t_id == 22)
//					cerr << "WHA";
				ns_splinter_path_fragment(paths[i],fragment_start,paths[i].elements.size(),paths[s]);
				paths[s].max_time = time(*paths[s].elements.begin());
				paths[s].min_time = time(*paths[s].elements.rbegin());
				fragment_counts++;
			}
	//		cerr << "Resizing path from " << paths[i].elements.size() << " to " << first_fragment_start << "\n";
			paths[i].elements.resize(first_fragment_start);
			paths[i].max_time = time(*paths[i].elements.begin());
			paths[i].min_time = time(*paths[i].elements.rbegin());
		}
	}
	if (break_counts > 0)
		cout << "Split " << break_counts << "/" << number_of_paths << " paths into " << fragment_counts << " fragments\n";
}
void ns_time_path_solver::solve(const ns_time_path_solver_parameters &param, ns_time_path_solution & solve){

	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::min_stationary_object_path_fragment_duration_in_seconds: ") <<             param.min_stationary_object_path_fragment_duration_in_seconds);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::stationary_object_path_fragment_window_length_in_seconds: ") <<			  param.stationary_object_path_fragment_window_length_in_seconds);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::stationary_object_path_fragment_max_movement_distance: ") <<				  param.stationary_object_path_fragment_max_movement_distance);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::maximum_time_gap_between_joined_path_fragments: ") <<					  param.maximum_time_gap_between_joined_path_fragments);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::maximum_time_overlap_between_joined_path_fragments: ") <<				  param.maximum_time_overlap_between_joined_path_fragments);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::maximum_distance_betweeen_joined_path_fragments: ") <<					  param.maximum_distance_betweeen_joined_path_fragments);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::min_final_stationary_path_duration_in_minutes: ") <<						  param.min_final_stationary_path_duration_in_minutes);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::maximum_fraction_duplicated_points_between_joined_path_fragments: ") <<	  param.maximum_fraction_duplicated_points_between_joined_path_fragments);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::maximum_path_fragment_displacement_per_hour: ") <<						  param.maximum_path_fragment_displacement_per_hour);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::max_average_final_path_average_timepoint_displacement: ") <<				  param.max_average_final_path_average_timepoint_displacement);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment: ") <<	  param.maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::maximum_fraction_of_median_gap_allowed_in_low_density_paths: ") <<		  param.maximum_fraction_of_median_gap_allowed_in_low_density_paths);
	
	//first we find all paths that are long enough and consistant enough to be real
	//note we discard lots of stray points here
	find_stationary_path_fragments(param. maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment,
				   param.min_stationary_object_path_fragment_duration_in_seconds,
				   param.stationary_object_path_fragment_window_length_in_seconds,
				   param.stationary_object_path_fragment_max_movement_distance);
/*	ofstream dbg("c:\\server\\dbg.csv");
	dbg << "round,id,t_id,t\n";
	for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++)
			dbg << "1," << i << "," << paths[i].elements[j].t_id << "," << timepoints[paths[i].elements[j].t_id].time << "\n";
	}
	*/
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::Numer of paths found:") << paths.size());

	unsigned long debug_paths_moving_fragments_removed(0);
	
	//remove fragments that move to much
	if (1){
		for (std::vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();){
		if (p->max_time == p->min_time){
			p++;
			continue;
		}
		const double displacement_per_hour((p->max_time_position-p->min_time_position).mag()/(double)((p->max_time-p->min_time)/60.0/60.0));
		if (displacement_per_hour > param.maximum_path_fragment_displacement_per_hour){
			p = paths.erase(p);
			debug_paths_moving_fragments_removed++;
		}
		else p++;
	}
	}
/*	for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++)
			dbg << "2," << i << "," << paths[i].elements[j].t_id <<  "," << timepoints[paths[i].elements[j].t_id].time << "\n";
	}*/
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::Moving paths removed:") << debug_paths_moving_fragments_removed);

	for (vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();p++){
		for (unsigned int i = 1; i < p->elements.size(); i++){
			if (p->elements[i-1].t_id < p->elements[i].t_id)
				throw ns_ex("Out of order paths were produced!");
		}
	/*	for (unsigned int i = 0; i < p->elements.size(); i++){
			if (this->timepoints[p->elements[i].t_id].elements[p->elements[i].index].e.region_position ==
					ns_vector_2i(2579,301) )//||
					//this->timepoints[p->elements[i].t_id].time == 1408897604)
				cerr << p->path_id << " ";
		}*/
		
	}
	//then we merge all appropriate path centers together
	//(which, for example, means that if we don't measure a plate for a day
	//but get good worm data before and after it, the paths will be correctly
	//linked together accross the gap.
	
	merge_overlapping_path_fragments(param.maximum_distance_betweeen_joined_path_fragments,
									 param.maximum_time_gap_between_joined_path_fragments,
									 param.maximum_time_overlap_between_joined_path_fragments,
									 param.maximum_fraction_duplicated_points_between_joined_path_fragments);

	/*for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++)
			dbg << "3," << i << "," << paths[i].elements[j].t_id <<  "," <<timepoints[paths[i].elements[j].t_id].time << "\n";
	}*/

	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::Number of paths after merge:") << paths.size());

	for (vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();p++){
		for (unsigned int i = 1; i < p->elements.size(); i++){
			if (p->elements[i-1].t_id < p->elements[i].t_id)
				throw ns_ex("Out of order paths were produced!");
		}
		/*for (unsigned int i = 0; i < p->elements.size(); i++){
			if (this->timepoints[p->elements[i].t_id].elements[p->elements[i].index].e.region_position ==
					ns_vector_2i(2579,301) )//||
					//this->timepoints[p->elements[i].t_id].time == 1408897604)
				cerr << p->path_id << " ";
		}*/
		
	}
		remove_short_and_moving_paths(param);
	/*for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++)
			dbg << "4," << i << "," << paths[i].elements[j].t_id <<  "," <<timepoints[paths[i].elements[j].t_id].time << "\n";
	}*/
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::solve()::Number of paths after short and moving paths were removed:") << paths.size());

	find_low_density_stationary_paths(param.min_final_stationary_path_duration_in_minutes*60,
				   param.stationary_object_path_fragment_max_movement_distance/2);

	handle_low_density_stationary_paths_and_stray_points(param.maximum_distance_betweeen_joined_path_fragments,param.min_final_stationary_path_duration_in_minutes);
	
	//remove late large gaps, which probably result from linking spurious fas moving animals
	break_paths_at_large_gaps(param.maximum_fraction_of_median_gap_allowed_in_low_density_paths);
	/*for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++)
			dbg << "5," << i << "," << paths[i].elements[j].t_id <<  "," <<timepoints[paths[i].elements[j].t_id].time << "\n";
	}*/
	assign_path_ids_to_elements();

	handle_paths_with_ambiguous_points();

	remove_short_and_moving_paths(param);

	assign_path_ids_to_elements();
	/*
	for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++)
			dbg << "6," << i << "," << paths[i].elements[j].t_id <<  "," <<timepoints[paths[i].elements[j].t_id].time << "\n";
	}
	dbg.close();*/
/*	for (unsigned long i = 0; i < timepoints.size(); i++){
		for (std::vector<ns_time_path_solver_element>::iterator p  = timepoints[i].elements.begin(); p != timepoints[i].elements.end();)
			if ((p->e.center - position).squared() > d_sq)
				p = timepoints[i].elements.erase(p);
			else p++;
	}*/
	transfer_data_to_solution(solve);
	timepoints.resize(0);
	path_groups.resize(0);
	
}

void ns_time_path_solver::handle_paths_with_ambiguous_points(){
	for (unsigned int i = 0; i < paths.size(); i++){
		for (std::vector<ns_time_element_link>::iterator n = paths[i].elements.begin(); n != paths[i].elements.end();){
			std::vector<ns_time_element_link>::iterator n1(n);
			n1++;
			if (n1 == paths[i].elements.end())
				break;

			if (n->t_id==n1->t_id){
				unsigned long index_id(n-paths[i].elements.begin());
				ns_vector_2i pos(find_local_median_position(paths[i],index_id));
				const double d(((element(*n).e.center)-pos).squared()),
							 d1(((element(*n1).e.center)-pos).squared());
				if (d < d1){
					element(*n1).e.low_temporal_resolution = true;
					*n1 = *n; //push forward and delete previous.  easier iterator handling.
					n = paths[i].elements.erase(n);
				}
				else{
					element(*n).e.low_temporal_resolution = true;
					n = paths[i].elements.erase(n);
					if (n == paths[i].elements.end())
						break;
				}
			}
			else n++;
		}
	}
}

void ns_time_path_solution::save_to_db(const unsigned long region_id, ns_sql & sql) const{
	sql << "SELECT time_path_solution_id FROM sample_region_image_info WHERE id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_solution::save_to_db():Could not load info from db");
	ns_image_server_image im;
	im.id = atol(res[0][0].c_str());
	bool update_db(false);
	if (im.id == 0){
		im = image_server.image_storage.get_region_movement_metadata_info(region_id,"time_path_solution_data",sql);
		update_db = true;
	}
	ofstream * o(image_server.image_storage.request_metadata_output(im,"csv",false,&sql));
	im.save_to_db(im.id,&sql);

	if (ns_dir::extract_extension(im.filename) != "csv")
		update_db = true;
	try{
		save_to_disk(*o);
		delete o;
	}
	catch(...){
		delete o;
		throw;
	}
	if (update_db){
		sql << "UPDATE sample_region_image_info SET time_path_solution_id = " << im.id << " WHERE id = " << region_id;
		sql.send_query();
	}

}
void ns_time_path_solution::load_from_db(const ns_64_bit region_id, ns_sql & sql){
	sql << "SELECT time_path_solution_id FROM sample_region_image_info WHERE id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_solution::load_from_db():Could not load info from db");
	ns_image_server_image im;
	im.id = atol(res[0][0].c_str());
	if (im.id == 0)
		throw ns_ex("Solution data has not been stored in db");
	ifstream * i(image_server.image_storage.request_metadata_from_disk(im,false,&sql));

	try{
		load_from_disk(*i);
		delete i;
	}
	catch(...){
		delete i;
		throw;
	}
}
//it would be nice to use XML here but it is so verbose!
void ns_time_path_solution::save_to_disk(ostream & o) const{
	//output time points
	for (unsigned int i = 0; i < timepoints.size(); i++){
		o << "t," << timepoints[i].time << "," << timepoints[i].sample_region_image_id << ",,,,\n";
	}

	//output worm data
	for (unsigned int i = 0; i < timepoints.size(); i++){
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
			o << "e," << i << ","
					  << timepoints[i].elements[j].region_position.x << ","
					  << timepoints[i].elements[j].region_position.y << ","
					  << timepoints[i].elements[j].context_image_position_in_region_vis_image.x << ","
					  << timepoints[i].elements[j].context_image_position_in_region_vis_image.y << ","
					  << timepoints[i].elements[j].region_size.x << ","
					  << timepoints[i].elements[j].region_size.y << ","
					  << timepoints[i].elements[j].context_image_position.x << ","
					  << timepoints[i].elements[j].context_image_position.y << ","
					  << timepoints[i].elements[j].context_image_size.x << ","
					  << timepoints[i].elements[j].context_image_size.y << ","
					  << timepoints[i].elements[j].low_temporal_resolution << ","
					  << (timepoints[i].elements[j].slowly_moving?"1":"0") << ","
					  << timepoints[i].elements[j].number_of_extra_worms_identified_at_location << ",";
			//bitfieldi
//			if (timepoints[i].elements[j].element_before_fast_movement_cessation)
//				cerr << "DKA";
			int m = 2*(timepoints[i].elements[j].element_before_fast_movement_cessation?1:0) + (timepoints[i].elements[j].inferred_animal_location?1:0);
			
	//		if (m != 0)
	//			cerr << "WA";
	//		if (m > 1)
	//			cerr << "MA";
			o << m << ",,,\n";
		}
	}
	
	//output paths
	for (unsigned int i = 0; i < paths.size(); i++){
		o << "p," << paths[i].center.x << "," 
				  << paths[i].center.y << ","
				  <<  "0,"  //reserved for future use
				  << paths[i].is_low_density_path << ",,,,\n";
	}
	//output path membership
	for (unsigned int i = 0; i < paths.size();i++){
		for (unsigned int j = 0; j < paths[i].stationary_elements.size(); j++){
			o << "l," << i << ","
					  << paths[i].stationary_elements[j].index << ","
					  << paths[i].stationary_elements[j].t_id << ",0\n";
		}
		for (unsigned int j = 0; j < paths[i].moving_elements.size(); j++){
			o << "l," << i << ","
					  << paths[i].moving_elements[j].index << ","
					  << paths[i].moving_elements[j].t_id << ",1\n";
		}
	}
	//output unassigned points
	for (unsigned int i = 0; i < unassigned_points.stationary_elements.size(); i++){
		o << "l," << "-1," 
				  << unassigned_points.stationary_elements[i].index << ","
				  << unassigned_points.stationary_elements[i].t_id << "\n";
	}
	//output path groups
	for (unsigned int i = 0; i < path_groups.size(); i++){
		o << "g," << path_groups[i].path_ids.size();
		for (unsigned int j = 0; j < path_groups[i].path_ids.size(); j++)
			o << "," << path_groups[i].path_ids[j];
		o << "\n";
	}
}/*
class ns_get_int{
	public:
	template<class T>
	inline void operator()(istream & in, T & d){
		in >> tmp;
		d = atol(tmp.c_str());
	}
	private:
	string tmp;
};
*/
void ns_time_path_solution::check_for_duplicate_events(){
	for (unsigned int i = 0; i < timepoints.size(); i++){
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
			for (unsigned int k = j+1; k < timepoints[i].elements.size(); k++){
				if (timepoints[i].elements[j].center == timepoints[i].elements[k].center 
					&& !timepoints[i].elements[j].inferred_animal_location 
					&& !timepoints[i].elements[k].inferred_animal_location//&&
					//timepoints[i].elements[j].number_of_extra_worms_identified_at_location > 0 &&
					//timepoints[i].elements[k].part_of_a_multiple_worm_disambiguation_cluster
					)
					throw ns_ex("ns_time_path_solution::check_for_duplicate_events()::A worm seems to have been duplicated.");
			}
		}
	}
};


long ns_time_path_solution::default_length_of_fast_moving_prefix(){
	return 16;
}
class ns_link_time_reverse_sorter{
public:
	bool operator()(const ns_time_element_link & a,const ns_time_element_link & b){
		return a.t_id > b.t_id;
	}
};

void ns_time_path_solution::remove_inferred_animal_locations(const unsigned long timepoint_index, bool delete_uninferred_animals_also){
	if (timepoint_index >= timepoints.size())
		throw ns_ex("Invalid timepoint index!");
	ns_time_path_timepoint & point(timepoints[timepoint_index]);
	if (point.elements.empty())
		return;
	//build up a list of the elements being removed (marked with a -1) and the new position of the remaining elements after removal.
	vector<ns_64_bit> new_index_mapping(point.elements.size(),0);
	unsigned int cur_i = 0;
	unsigned int new_i = 0;
	for (std::vector<ns_time_path_element>::iterator p = point.elements.begin(); p != point.elements.end();){
		if (p->inferred_animal_location || delete_uninferred_animals_also){
			p = point.elements.erase(p);
			new_index_mapping[cur_i] = -1;
		}else{
			new_index_mapping[cur_i] = new_i;
			p++;
			new_i++;
		}
		cur_i++;
	}

	//go through and fix all the path assignments
	for (unsigned int g = 0; g < this->path_groups.size(); g++){
		for (unsigned int p = 0; p < path_groups[g].path_ids.size(); p++){
			const unsigned long path_id = path_groups[g].path_ids[p];
			if (paths[path_id].stationary_elements.size() == 0)
				continue;
			for (std::vector<ns_time_element_link>::iterator q = paths[path_id].stationary_elements.begin(); q != paths[path_id].stationary_elements.end();){
				if (q->t_id == timepoint_index){
					if (new_index_mapping[q->index] == -1)
						q = paths[path_id].stationary_elements.erase(q);
					else{
						if (q->index > new_index_mapping.size())
							throw ns_ex("Invalid index");
						q->index = new_index_mapping[q->index];
						q++;
					}
				}
				else q++;
			}
		}
	}
}


void ns_time_path_solution::fill_gaps_and_add_path_prefixes(const unsigned long prefix_length){
	std::vector<unsigned long> skipped_time_indices,
							   prefixes;
	std::vector<ns_time_path_element> skipped_elements;
	for (unsigned int g = 0; g < this->path_groups.size(); g++){
		for (unsigned int p = 0; p < path_groups[g].path_ids.size(); p++){
			//std::sort(paths[path_id].stationary_elements.begin(),paths[path_id].stationary_elements.end(),ns_link_time_sorter());
			//if (g==32)
			//		cerr << "WHA";
			//find gaps in path
			skipped_time_indices.resize(0);
			skipped_elements.resize(0);
			const unsigned long path_id = path_groups[g].path_ids[p];
			if (paths[path_id].stationary_elements.size() == 0)
				continue;
			unsigned long last_timepoint_index = paths[path_id].stationary_elements[0].t_id;
			for (unsigned int i = 1; i < paths[path_id].stationary_elements.size(); i++){
				if (last_timepoint_index-1 != paths[path_id].stationary_elements[i].t_id){
					ns_time_path_element e(element(paths[path_id].stationary_elements[i]));
					e.inferred_animal_location = true;
					e.element_before_fast_movement_cessation = false;
					e.context_image_position_in_region_vis_image = ns_vector_2i(-1,-1);
					for (unsigned int j = paths[path_id].stationary_elements[i].t_id+1 ; j < last_timepoint_index;j++){
						skipped_time_indices.push_back(j);					
						skipped_elements.push_back(e);
					}
				}
				last_timepoint_index = paths[path_id].stationary_elements[i].t_id;
			}
			

			//identify prefix start time
			prefixes.resize(0);
			unsigned long earliest_stationary_element_index(paths[path_id].stationary_elements.size());
			for (long k = (long)paths[path_id].stationary_elements.size()-1; k >= 0; k--){
				if ( !element(paths[path_id].stationary_elements[k]).inferred_animal_location){
					earliest_stationary_element_index = k;
					break;
				}
			}
			if (earliest_stationary_element_index == paths[path_id].stationary_elements.size())
				throw ns_ex("Could not find any non-inferred elements in path!");
			long earliest_timepoint_index = (long)paths[path_id].stationary_elements[earliest_stationary_element_index].t_id;
			long prefix_start = earliest_timepoint_index - (long)prefix_length-1;
			if (prefix_start < 0)
				prefix_start = 0;

			//check that something crazy isn't going on with a previously added prefix
			for (unsigned int k = earliest_stationary_element_index+1; k < paths[path_id].stationary_elements.size(); k++){
				if (!element(paths[path_id].stationary_elements[k]).element_before_fast_movement_cessation || 
					paths[path_id].stationary_elements[k].t_id != earliest_timepoint_index + k)
					throw ns_ex("Unusual prefix state identified.");
			}
			
			int start_of_prefix_to_add(earliest_timepoint_index + paths[path_id].stationary_elements.size() - earliest_stationary_element_index-1);
			//add in prefix points to path
			ns_time_path_element e(element(paths[path_id].stationary_elements[earliest_stationary_element_index]));
			e.inferred_animal_location = true;
			e.element_before_fast_movement_cessation = true;
			e.context_image_position_in_region_vis_image = ns_vector_2i(-1,-1);
			for (unsigned int i = prefix_start; i < start_of_prefix_to_add; i++){
				timepoints[i].elements.push_back(e);
				paths[path_id].stationary_elements.push_back(ns_time_element_link(i,timepoints[i].elements.size()-1));
			}

			//add gaps in path into path as inferred points
			for (unsigned int i = 0; i < skipped_elements.size(); i++){
				timepoints[skipped_time_indices[i]].elements.push_back(skipped_elements[i]);
				paths[path_id].stationary_elements.push_back(ns_time_element_link(skipped_time_indices[i],timepoints[skipped_time_indices[i]].elements.size()-1));
			}
			std::sort(paths[path_id].stationary_elements.begin(),paths[path_id].stationary_elements.end(),ns_link_time_reverse_sorter());
		}
	}
}

void ns_time_path_solution::load_from_disk(istream & in){
	clear();
	timepoints.reserve(100);
	paths.reserve(20);
	path_groups.reserve(10);
	worms_loaded = false;

	ns_get_int get_int;
	string tmp;
	while(true){
		string op_str;
		get_int(in,op_str);
		if (in.fail())
			break;
		if(op_str.size() != 1)
			throw ns_ex("ns_time_path_solution::load_from_disk()::Invalid op specification:") << op_str;
		char op(op_str[0]);
	//	cerr << op << " ";
		switch(op){
			case 't':{
				unsigned long s = timepoints.size();
				timepoints.resize(s+1);
				get_int(in,timepoints[s].time);
				get_int(in,timepoints[s].sample_region_image_id);
				for (unsigned int i = 0; i < 4; i++) get_int(in,tmp); //room for expansion
				timepoints[s].elements.reserve(10);
				break;
			}
			case 'e':{
				unsigned long timepoint_id;
				get_int(in,timepoint_id);
				if (timepoint_id >= timepoints.size())
					throw ns_ex("ns_time_path_solution::load_from_disk()::Invalid timepoint id: ")<< timepoint_id;
				unsigned long s = timepoints[timepoint_id].elements.size();
				timepoints[timepoint_id].elements.resize(s+1);
				get_int(in,timepoints[timepoint_id].elements[s].region_position.x);
				get_int(in,timepoints[timepoint_id].elements[s].region_position.y);
				get_int(in,timepoints[timepoint_id].elements[s].context_image_position_in_region_vis_image.x);
				get_int(in,timepoints[timepoint_id].elements[s].context_image_position_in_region_vis_image.y);
				get_int(in,timepoints[timepoint_id].elements[s].region_size.x);
				get_int(in,timepoints[timepoint_id].elements[s].region_size.y);
				get_int(in,timepoints[timepoint_id].elements[s].context_image_position.x);
				get_int(in,timepoints[timepoint_id].elements[s].context_image_position.y);
				get_int(in,timepoints[timepoint_id].elements[s].context_image_size.x);
				get_int(in,timepoints[timepoint_id].elements[s].context_image_size.y);
				timepoints[timepoint_id].elements[s].center = timepoints[timepoint_id].elements[s].region_position + timepoints[timepoint_id].elements[s].region_size/2;
				get_int(in,timepoints[timepoint_id].elements[s].low_temporal_resolution);
				get_int(in,timepoints[timepoint_id].elements[s].slowly_moving);
				get_int(in,timepoints[timepoint_id].elements[s].number_of_extra_worms_identified_at_location);
				//decode bitfield
				string m;
				get_int(in,m);
				if (m.size() == 0){
					timepoints[timepoint_id].elements[s].inferred_animal_location = false;
					timepoints[timepoint_id].elements[s].element_before_fast_movement_cessation = false;
				}
				else {
					int mm = atol(m.c_str());
			//		if (mm > 1)
			//			cerr << "WA";
			//		if (mm > 0)
			//			cerr << "WA";
					timepoints[timepoint_id].elements[s].inferred_animal_location = mm%2;
					timepoints[timepoint_id].elements[s].element_before_fast_movement_cessation = (mm/2)%2;
				}

				for (unsigned int i = 0; i < 3; i++) get_int(in,tmp); //room for expansion
				break;
			}
			case 'p':{
				unsigned long s = paths.size();
				paths.resize(s+1);
				get_int(in,paths[s].center.x);
				get_int(in,paths[s].center.y);
				int dummy;
				get_int(in,dummy); //reserved for future use
				get_int(in,paths[s].is_low_density_path);
				for (unsigned int i = 0; i < 4; i++) get_int(in,tmp); //room for expansion
				break;
			}
			case 'l':{
					long path_id,stationary_type;
					ns_time_element_link temp_link;
					get_int(in,path_id);
					get_int(in,temp_link.index);
					get_int(in,temp_link.t_id);
					in.unget();
					char a(in.get());
					if (a!=',')  //no stationary type specified; assume stationary.
						stationary_type = 0;
					else get_int(in,stationary_type);			
					ns_time_path * path;
					if (path_id == -1)
						path = &(this->unassigned_points);	
					else if (path_id >= paths.size())
						throw ns_ex("ns_time_path_solution::load_from_disk()::Invalid path id!");
					else path = &paths[path_id];
					vector<ns_time_element_link> & v(stationary_type==0?path->stationary_elements:path->moving_elements);
					unsigned long s = v.size();
					v.resize(s+1);
					v[s] = temp_link;
				break;
			}
			case 'g':{
				unsigned long s = path_groups.size();
				path_groups.resize(s+1);
				unsigned long number_of_paths;
				get_int(in,number_of_paths);
				path_groups[s].path_ids.resize(number_of_paths);
				for (unsigned int i = 0; i < number_of_paths; i++){
					get_int(in,path_groups[s].path_ids[i]);
				}
				break;
			}
			default:{
				string a;
				a+=op;
				throw ns_ex("ns_time_path_solution::load_from_disk()::Unknown object in path solution: ") << a;
				}
		}
	}
	
	ns_global_debug(ns_text_stream_t("ns_time_path_solution()::load_from_disk()::Loaded ") << unassigned_points.moving_elements.size() << " unassigned moving elements");
	ns_global_debug(ns_text_stream_t("ns_time_path_solution()::load_from_disk()::Loaded ") << unassigned_points.stationary_elements.size() << " unassigned stationary elements");
	ns_global_debug(ns_text_stream_t("ns_time_path_solution()::load_from_disk()::Loaded ") << path_groups.size() << " paths groups.");
	ns_global_debug(ns_text_stream_t("ns_time_path_solution()::load_from_disk()::Loaded ") << paths.size() << " paths.");
	ns_global_debug(ns_text_stream_t("ns_time_path_solution()::load_from_disk()::Loaded ") << timepoints.size() << " timepoints.");

	check_for_duplicate_events();
}


inline void ns_is_interesting_point(const ns_vector_2i &p){
//	if (	abs(p.x-3036) < 4	 && abs(p.y-2230) < 4)
//			cerr << "Whee";
};
//does not include in the minimization problem any elements with "element_assigned" tag set to true.


void ns_time_path_solver::assign_timepoint_elements_to_paths(std::vector<ns_time_path_solver_element> & elements, const unsigned long max_dist_sq, std::vector<ns_time_path_solver_path_builder> & opaths){
	hungarian_problem_t matching_problem;
	int unassigned_count(0);
	for (unsigned int i = 0; i < elements.size(); i++)
		unassigned_count+=elements[i].element_assigned?0:1;
	if (unassigned_count == 0)
		return;
	int ** cost_matrix = new int *[opaths.size()];
	for (unsigned int i = 0; i < opaths.size(); i++)
		cost_matrix[i] = new int[unassigned_count];
	try{
		for (unsigned int i = 0; i < opaths.size(); i++){
			unsigned long k(0);
			for (unsigned int j = 0; j < elements.size(); j++){
				if (elements[j].element_assigned) 
					continue;
				const unsigned long d((elements[j].e.center-opaths[i].center).squared());
				if (d > max_dist_sq)
					cost_matrix[i][k] = ns_hungarian_impossible_value;
				else 
					cost_matrix[i][k] = d;
				k++;
			}
		}

		hungarian_init(&matching_problem,cost_matrix,
			opaths.size(),//rows
			unassigned_count,	//columns
			HUNGARIAN_MODE_MINIMIZE_COST);

		hungarian_solve(&matching_problem);
		
		int k = 0;
		for (unsigned int j = 0; j < elements.size(); j++){
			if (elements[j].element_assigned){
				elements[j].element_assigned_in_this_round = false;
				continue;
			}
			bool found(false);
			for (unsigned int i = 0; i < opaths.size(); i++){
				if (matching_problem.assignment[i][k] != HUNGARIAN_ASSIGNED ||
					cost_matrix[i][k] == ns_hungarian_impossible_value)
					continue;
				elements[j].path_id = i;
				elements[j].element_assigned_in_this_round = true;
				found = true;
				break;
			}
			if (!found){
			//	ns_is_interesting_point(timepoints[t_id].elements[j].e.center);
				elements[j].element_assigned_in_this_round = false;
			}
			k++;
		}
		hungarian_free(&matching_problem);
		for (unsigned int i = 0; i < opaths.size(); i++)
			delete[] cost_matrix[i];
		delete[] cost_matrix;
	}
	catch(...){
		for (unsigned int i = 0; i < opaths.size(); i++)
			delete[] cost_matrix[i];
		delete[] cost_matrix;
		throw;
	}
}
double ns_time_path_solver_path_builder::calculate_current_density(const unsigned long time_density_window, const std::vector<ns_time_path_solver_timepoint> & timepoints,const unsigned long current_timepoint_i) const{
	
	if (elements.size() == 0) return 0;
	
	const unsigned long current_time(timepoints[current_timepoint_i].time);
	//find the total number of observations made on the plate during the time where the path is defined and within the time density window
	unsigned long number_of_observations_in_experiment(0);
	for (unsigned int i = current_timepoint_i; i < timepoints.size(); i++){
//		if (timepoints[i].time < elements.rbegin()->time)
//			continue;
		if (timepoints[i].time > elements[0].time || timepoints[i].time > current_time+time_density_window)
			break;
		number_of_observations_in_experiment++;
	}
	//find the total number of observations made in the path during the time window
	unsigned long point_count(0);
	int i;
	for (i = elements.size()-1; i >=0; i--){
		if (elements[i].time > current_time+time_density_window)
			break;
		point_count++;
	}
	if (number_of_observations_in_experiment == 0)
		throw ns_ex("ns_time_path_solver_path_builder::calculate_current_density()::Could not find any experimental observations in path region!");
	return point_count/(double)number_of_observations_in_experiment;
	/*
	if (i < 0){
		if (current_time-elements[0].time == 0)
			return 1.0;
		double t(elements[0].time-current_time);
		return point_count/(double)(t);
	}
	double t(elements[i].time-current_time);
	return point_count/(double)(elements[i].time/t);*/
}

void ns_time_path_solver_path_builder::calculate_center(const unsigned long time_density_window) const{
	if (elements.empty()){
		center =  ns_vector_2d(0,0);
		return;
	}
	if (time_density_window == 0){
		center = ns_vector_2d(0,0);
		for (unsigned int i = 0; i < elements.size(); i++)
			center+=elements[i].pos;
		center/=elements.size();
		return;
	}
	const unsigned long stop_time(elements.rbegin()->time+time_density_window);
	ns_vector_2d pos(0,0);
	unsigned long count(0);
	for (int i = elements.size()-1; i >=0; i--){
		if (elements[i].time > stop_time)
			break;
		pos+=elements[i].pos;
		count++;
	}
	center = pos/count;
}

ns_time_path_solution_stationary_drift_estimator ns_estimator_for_point(const ns_vector_2i & center,const unsigned long time,const ns_time_element_link & link){
	ns_time_path_solution_stationary_drift_estimator estimator;
	estimator.early_parameters.estimation_position = center;
	//	estimator.match_position = center;
	estimator.early_parameters.estimation_drift = ns_vector_2i(0,0);
	estimator.stray_point_id = link;
	//estimator.match_time = time; 
	estimator.early_parameters.estimation_time = 
		estimator.path_bound_min = 
		estimator.path_bound_max = time; 
	estimator.late_parameters = estimator.early_parameters;
	return estimator;
}

int ns_find_linked(const int id, const int size, int ** assignments){
	for (unsigned int i = 0; i < size; i++){
		if (assignments[id][i] == HUNGARIAN_ASSIGNED)
			return i;
	}
	return -1;
}
struct ns_estimator_group_group{
	ns_estimator_group_group():to_be_deleted(false){}
	ns_estimator_group_group(const ns_time_path_solution_stationary_drift_estimator_group * est, const ns_time_path_solution_stationary_drift_estimator_group * tgt):to_be_deleted(false),estimators(2){
		estimators[0] = est;
		estimators[1] = tgt;
	}
	typedef std::vector<const ns_time_path_solution_stationary_drift_estimator_group * > ns_estimator_list;
	ns_estimator_list estimators;
	bool to_be_deleted;
};

unsigned long  ns_time_path_solver::number_of_stationary_paths_at_time(const unsigned long t) const{
	unsigned long c(0);
	for (unsigned int k = 0; k < paths.size(); k++){
		if (t <= paths[k].max_time &&
			t >= paths[k].min_time)
			c++;
	}
	return c;
}

bool ns_time_path_solver::can_search_for_stray_points_at_time(const ns_time_element_link & l) const{
	return number_of_stationary_paths_at_time(timepoints[l.t_id].time) > number_of_unassigned_points_at_time(l);
}

bool operator < (const ns_time_path_solution_stationary_drift_estimator & l, const ns_time_path_solution_stationary_drift_estimator & r){
	return l.path_bound_max > r.path_bound_max;
}

bool ns_time_path_solver::time_index_is_in_path(const ns_time_path_solver_path & path,const ns_time_element_link & e) const{
	for(std::vector<ns_time_element_link>::const_iterator p = path.elements.begin(); p != path.elements.end(); p++){
		if (p->t_id == e.t_id)
			return true;
		if (p->t_id < e.t_id)
			return false;
	}
	return false;
}
bool ns_time_path_solver::time_index_is_in_path(const unsigned long path_id, const ns_time_element_link & e) const{
	return time_index_is_in_path(paths[path_id],e);
}


const ns_time_path_solver_path & ns_time_path_solution_stationary_drift_estimator_group::generate_path(const std::vector<ns_time_path_solver_path> & paths) const{
	if (path_cache_.elements.size() == 0){

		for ( ns_estimator_list::const_iterator e = estimators.begin(); e != estimators.end(); e++){
			if (e->is_a_path()){
				path_cache_.elements.insert(path_cache_.elements.end(),paths[e->path_id].elements.begin(),paths[e->path_id].elements.end());
			}
			else{
				path_cache_.elements.push_back(e->stray_point_id);
			}
		}
		path_cache_.max_time = estimators.begin()->late_parameters.estimation_time;
		path_cache_.min_time = estimators.rbegin()->early_parameters.estimation_time;
		path_cache_.path_id = -1;
	}
	return path_cache_;
}
void ns_time_path_solution_stationary_drift_estimator_group::clear_cached_path() const{
	path_cache_.elements.clear();
	group_estimator_specified_ = false;
}

bool  ns_time_path_solver::is_ok_to_merge_overlap(const ns_time_path_solution_stationary_drift_estimator_group & later, const ns_time_path_solution_stationary_drift_estimator_group & earlier,const unsigned long max_time_gap, const unsigned long max_time_overlap,const double max_fraction_points_overlap) const{
	
//	bool is_interesting(ns_is_close2(later.latest().late_parameters.estimation_position) && 
//		ns_is_close2(earlier.latest().late_parameters.estimation_position));
	//easy case
	if (earlier.latest().late_parameters.estimation_time < later.earliest().early_parameters.estimation_time)
		return true;
	const ns_time_path_solver_path & later_path(later.generate_path(paths));
	const ns_time_path_solver_path & earlier_path(earlier.generate_path(paths));
	if (earlier_path.elements.size() == 1){
		return !time_index_is_in_path(later_path,earlier_path.elements[0]);
	}
	if (later_path.elements.size() == 1){
		throw ns_ex("This should be impossible, as if the later path had only one element it would have to be non-overlapping "
					" with the earlier, and thus be accepted at the first return a few lines above");
	}

	return is_ok_to_merge_overlap(later_path,earlier_path,max_time_gap,max_time_overlap,max_fraction_points_overlap);
}

bool ns_is_close3(const ns_vector_2d & p){
	//3176	5866
	//992	1211
	//2054	4692
	//4887	3926
	//2054	4692
	//2053	4693
	return (abs(p.x - 2053)<50 && abs(p.y-4693) < 20);
		
}

bool ns_is_close4(const ns_time_element_link & e){
	return (e.t_id == 269 && e.index == 19);
}
typedef map<unsigned long,vector<ns_time_path_solution_stationary_drift_estimator_group *> > ns_sorted_estimator_list;

//lots info about this algorithm
//in handle_low_density_stationary_paths_and_stray_points() docuentation

bool ns_time_path_solver::merge_estimator_groups(const unsigned long max_dist_sq, std::vector<ns_time_path_solution_stationary_drift_estimator_group> & e) const{
	bool changes_made(false);
	const double max_fraction_points_overlap = 0.2;

	//we want to make a list of estimators sorted by time
	//these are then merged toward later points, starting at the last point
	ns_sorted_estimator_list targets, estimators;
	for (int i = 0; i < e.size(); i++){
		e[i].to_be_deleted = false;
	//	targets[e[i].latest().path_bound_max].push_back(&e[i]);
		estimators[e[i].latest().path_bound_max].push_back(&e[i]);
	}
	
	//est contains the estimators that will be used as the "later" of merged pairs that absorb "earlier" targets
	//tgt contains the targets that will be absorbed by the "later" estimators.
	vector<ns_time_path_solution_stationary_drift_estimator_group *> est,tgt;
	//go through all the targets sorted from earliest to latest
	for (ns_sorted_estimator_list::reverse_iterator t = estimators.rbegin(); t != estimators.rend(); t++){
		est.resize(0);
		tgt.resize(0);
		//choose all estimators at current time points as potentital targets
		for (unsigned int i = 0; i < t->second.size(); i++){
			if (!t->second[i]->to_be_deleted)
				tgt.push_back(t->second[i]);
		}
		//choose all estimators at any previous timepoint as potential targets
		for (ns_sorted_estimator_list::reverse_iterator e = estimators.rbegin(); e != estimators.rend(); e++){
			for (unsigned int i = 0; i < e->second.size(); i++){
				//if the estimator is deleted or occurrs earlier than the target, don't include it
				if (e->second[i]->to_be_deleted || e->second[i]->latest().path_bound_max <= t->first){
					continue;
				}
				est.push_back(e->second[i]);
			}
		}
		if (est.size() == 0)
			continue;
		int ** cost_matrix = new int *[est.size()];
		for (unsigned int i = 0; i <est.size(); i++)
			cost_matrix[i] = new int[tgt.size()];

		try{
			//i is the estimator
			for (unsigned int i = 0; i < est.size(); i++){
				//j is the target
				for (unsigned int j = 0; j < tgt.size(); j++){
					if (est[i] == tgt[j]){
						//dont attempt to match estimators to themselves!
						throw ns_ex("Attempting to match target to itself!");
					}
					//don't allow estimators to chose very far away objects,
					//or to overlap unpleasantly with it.
					//bool is_interesting(ns_is_close2(est[i]->earliest().early_parameters.estimation_position) && 
					//	ns_is_close2(tgt[j]->latest().late_parameters.estimation_position));
				
					double d((estimate(*est[i],tgt[j]->latest().late_parameters.estimation_time) - 
								tgt[j]->latest().late_parameters.estimation_position).squared());
					
					if (d >= 0 && d < max_dist_sq &&
						is_ok_to_merge_overlap(*est[i],*tgt[j],INT_MAX,INT_MAX,max_fraction_points_overlap))
						{
						/*if (is_interesting){
							cerr << "Accepted";
							cerr << "\n";
						}*/
						cost_matrix[i][j] = d;
					}
					else{
						/*if (is_interesting){
							if (!(d > 0 && d < max_dist_sq))
								cerr << "Too large a distance. ";
							cerr << "Rejected";
							cerr << "\n";
						}*/
						cost_matrix[i][j] =  ns_hungarian_impossible_value;
					}
				}
			}
			
			hungarian_problem_t matching_problem;
			hungarian_init(&matching_problem,cost_matrix,
				est.size(),//rows
				tgt.size(),	//columns
				HUNGARIAN_MODE_MINIMIZE_COST);

			hungarian_solve(&matching_problem);
		
			//merge all targets up into their matched estimators
			for (unsigned int i = 0; i < est.size(); i++){
				if (est[i]->to_be_deleted)
					throw ns_ex("Deleted Estimators were matched to targets!");
				const long linked_id = ns_find_linked(i,tgt.size(),matching_problem.assignment);
				if (linked_id != -1 && cost_matrix[i][linked_id] != ns_hungarian_impossible_value){
					
					/*if(ns_is_close3(tgt[linked_id]->latest().late_parameters.estimation_position)){
						cerr << "Found: ";
						if (!tgt[linked_id]->latest().is_a_path())
							cerr << tgt[linked_id]->latest().stray_point_id.t_id << ": ";
						else cerr << paths[tgt[linked_id]->latest().path_id].elements.begin()->t_id << ": ";
						cerr << tgt[linked_id]->latest().late_parameters.estimation_position;
						cerr << "\n";
					}*/
					//note that the group automatically sorts the new estimators such that the estimator() function will return the corrent one next time
					for (ns_time_path_solution_stationary_drift_estimator_group::ns_estimator_list::iterator p = tgt[linked_id]->estimators.begin();
							p!= tgt[linked_id]->estimators.end(); p++){
						/*
						if (p->is_a_path()){
									
							for (unsigned int k = 0; k < paths[p->path_id].elements.size(); k++){
								if (ns_is_close4(paths[p->path_id].elements[k]))
									cerr << "Found it in an estimator; " << p->path_id << "\n";
								if (element(paths[p->path_id].elements[k]).element_assigned_in_this_round)
									cerr << "WHA!";
								else
									element(paths[p->path_id].elements[k]).element_assigned_in_this_round = true;
							}
						}
						else{
							if (ns_is_close4(p->stray_point_id))
								cerr << "Found it in an estimator; stray point\n";
							if (element(p->stray_point_id).element_assigned_in_this_round)
								cerr << "WHA!";
							else
								element(p->stray_point_id).element_assigned_in_this_round = true;
						}*/
						bool element_already_existed(!est[i]->estimators.insert(*p).second);
						est[i]->clear_cached_path();	//since we've changed the estimator, we'll need to rebuild a path from it next time it's needed
														//the path is used to calculate overlaps only.
						//if (element_already_existed)
						//	std::cerr << "WAHA";
						changes_made = true;
					}

					//we need to delete the absorbed target, but we can't do it yet because
					//we have pointers into its container that need to stay valid.
					tgt[linked_id]->to_be_deleted = true;
				}
			}
			hungarian_free(&matching_problem);	
			for (unsigned int i = 0; i < est.size(); i++)
				delete[] cost_matrix[i];
			delete[] cost_matrix;
		}
		catch(...){
			for (unsigned int i = 0; i < est.size(); i++)
				delete[] cost_matrix[i];
			delete[] cost_matrix;
			throw;
		}
	}

	//now we can go ahead and delete targets that were merged.
	for (std::vector<ns_time_path_solution_stationary_drift_estimator_group>::iterator p = e.begin(); p != e.end();){
		if (p->to_be_deleted)
			p = e.erase(p);
		else p++;
	}
	return changes_made;
}

ns_vector_2i ns_time_path_solver::find_local_median_position(const ns_time_path_solver_path & p, const unsigned long i) const{
		if (p.elements.size() < 5)
			return element(p.elements[i]).e.center;
		int x[5],y[5];
		long start = (long)i-2;
		if (start < 0)
			start = 0;
		if (start + 5 >= p.elements.size())
			start = p.elements.size()-5;
		for (unsigned int i = start; i < start+5; i++){
			x[i-start] = element(p.elements[i]).e.center.x;
			y[i-start] = element(p.elements[i]).e.center.y;
		}
		std::sort(x,x+5);
		std::sort(y,y+5);
		return ns_vector_2i(x[2],y[2]);
	}

ns_vector_2i ns_time_path_solver::find_max_time_position(const ns_time_path_solver_path & p) const{
		if (p.elements.size() < 5)
			return element(*p.elements.begin()).e.center;
		int x[5],y[5];
		for (unsigned int i = 0; i < 5; i++){
			x[i] = element(p.elements[i]).e.center.x;
			y[i] = element(p.elements[i]).e.center.y;
		}
		std::sort(x,x+5);
		std::sort(y,y+5);
		return ns_vector_2i(x[2],y[2]);
	}
	ns_vector_2i ns_time_path_solver::find_min_time_position(const ns_time_path_solver_path & p) const{
		if (p.elements.size() < 5)
			return element(*p.elements.rbegin()).e.center;
		int x[5],y[5];
		for (unsigned int i = 0; i < 5; i++){
			x[i] = element(p.elements[p.elements.size()-i-1]).e.center.x;
			y[i] = element(p.elements[p.elements.size()-i-1]).e.center.y;
		}
		std::sort(x,x+5);
		std::sort(y,y+5);
		return ns_vector_2i(x[2],y[2]);
}
	
struct ns_time_element_link_orderer{
	bool operator()(const ns_time_element_link & a, const ns_time_element_link & b){
		return a.t_id < b.t_id;
	}
};

void ns_time_path_solver::handle_low_density_stationary_paths_and_stray_points(const unsigned long max_movement_distance, const double min_final_stationary_path_duration_in_minutes){
	//this is important, finicky code. The details matter.
	//It takes all the stationary points left in the data set but at low density
	//and handles them correctly.  THis is important to get censoring right (as the censoring algorithm reasons over the moving worms,
	//and low density paths show up as moving worms if not handled correctly.
	//It's also important as the low density paths may actually belong to high density paths but not have been linked in
	//in the path detection.  They are linked in here.

	//here what we do is make a big list of all possible things that may need to be joined
	//ie low density path points, random stray points, and high density path fragments.
	//These are all combined as "estimators" which are wrappers that abstract the idea that
	//to join two objects all that needs to be done is to predict the distance away from
	//one in space the other is.
	//We start at the end of the experiment.  At each time, we split all estimators into two groups
	//"Estimators" and "targets".  Targets are all estimators that have other estimators "later" than them
	//in time, and thus can potentially be merged with those later estimators.
	//obviously, the latest esitmators will never act as targets; similarly the earliest estimators
	//will never act as estimators for other targets.
	//we allow a little overlap between estimator and target, ordering is done by the latest point on each estimator

	//Given a set of possible estimators and possible targets,
	//we a big cost matrix and run a hungarian algorithm
	//to find the optimal assignments.
	//Note that the hungarian algorithm is done *not* considering time; so everything
	//is joined without regard to how far things are appart in time

	//after everything is joined together, everything might be out of order (i.e paths linked
	//to paths before them) so we sort the joined elements in time and then merge them.

	//The act of merging two estimators might make a new target for other estimators, so 
	//we repeat the process until no changes are made.

	//Note that all lists of points within paths, and estimators with estimator groups
	//are sorted in *reverse* temporal order; so the first element (begin()) is going to be
	//the latest.

	std::vector<ns_time_path_solution_stationary_drift_estimator_group> estimators;
	estimators.resize(paths.size());

	//add an estimator for each high density path
	for (unsigned int i = 0; i < paths.size(); i++){
		paths[i].path_id = i;
		estimators[i].estimators.insert(get_drift_estimator(i));
	/*	for (unsigned int j = 0; j < paths[i].elements.size(); j++)
			if (ns_is_close4(paths[i].elements[j]))
				cerr << "Found it in path\n";*/
	}
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::handle_low_density_stationary_paths_and_stray_points::Considering ") << estimators.size() << " drift estimators.");

	//we don't want to search through a whole bunch of fast-moving animals
	//so we use a heuristic to prevent these from being searched
	//(i.e from being added to the search set as estimators)
	long maximum_number_of_simultaneous_paths(0);
	
	for (unsigned long i = 0; i < timepoints.size(); i++){
		if (maximum_number_of_simultaneous_paths < number_of_stationary_paths_at_time(timepoints[i].time))
			maximum_number_of_simultaneous_paths = number_of_stationary_paths_at_time(timepoints[i].time);
	}
	
	long index_where_paths_dominate(-1);
	for (unsigned long i = 0; i < timepoints.size(); i++){
		const unsigned long stat(number_of_stationary_paths_at_time(timepoints[i].time));
		if (stat > maximum_number_of_simultaneous_paths/4 &&
			stat > number_of_unassigned_points_at_time(ns_time_element_link(i,0))){
				index_where_paths_dominate = i;
				break;
		}

	}
	const unsigned long number_of_path_estimators(estimators.size());
	const unsigned long max_dist_sq(max_movement_distance*max_movement_distance);

	unsigned long va(0),vb(0);

	//We want to include low density paths as sets of points
	//because part of low density paths may be joined in with different estimators. 
	//We'll add them explicitly a bit later, but for now we mark them as 
	//assigned so they don't get added in the next stray-point detection step
	mark_unassigned_points();
	for (unsigned int i = 0; i < low_density_paths.size(); i++){
		for (unsigned int j = 0; j < low_density_paths[i].elements.size(); j++){
			element(low_density_paths[i].elements[j]).element_assigned = true;
		}
	}

	//detect stray points and add them as estimators (ie. elements capable of being matched up)
	for (unsigned int i = 0; i < timepoints.size(); i++){
		unsigned long number_of_stationary_paths_at_current_time(0);
		if (i < index_where_paths_dominate)
			continue;
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
		//	if (ns_is_close2(timepoints[i].elements[j].e.center))
		//		cerr << "B";
			if (timepoints[i].elements[j].element_assigned)
					continue;

			for (unsigned int k = 0; k < number_of_path_estimators; k++){

				if (estimators[k].within_time_span(timepoints[i].time))
					continue;
			
				const double d((timepoints[i].elements[j].e.center - estimators[k].earliest().estimate(timepoints[i].time)).squared()); 
			
			
				if (d <= max_dist_sq){
		//			if (ns_is_close4(ns_time_element_link(i,j)))
		//				cerr << "Found it in stray\n";

		//			for (unsigned int m = 0; m < estimators.size(); m++){
		//				if (estimators[m].estimators.begin()->stray_point_id.index == j &&
		//					estimators[m].estimators.begin()->stray_point_id.t_id == i)
		//					cerr << "WHA!";
		//			}
					estimators.push_back(ns_time_path_solution_stationary_drift_estimator_group(
						ns_estimator_for_point(
						timepoints[i].elements[j].e.center,timepoints[i].time,ns_time_element_link(i,j)
						)
						)
						);
					timepoints[i].elements[j].element_assigned = true; //prevent repeat additions
					va++;
					break;	//prevent repeat additions
				}
			}
		}
	}
	cout << "Considering " << va << " unassigned matching positions\n";
	//add the low density paths as free points
	for (unsigned int i = 0 ; i < low_density_paths.size(); i++){
		for (unsigned int j = 0; j < low_density_paths[i].elements.size(); j++){
		//	if (ns_is_close4(low_density_paths[i].elements[j]))
		//		cerr << "Found in low density paths!\n";
			estimators.push_back(ns_estimator_for_point(element(low_density_paths[i].elements[j]).e.center,time(low_density_paths[i].elements[j]),low_density_paths[i].elements[j]));
			vb++;
		}
	}
	cout << "Considering " << paths.size() << " high density paths\n";
	cout << "Considering " << vb << " points from low density paths\n";
	
	for (unsigned int i = 0; i < timepoints.size(); i++){
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++)
			timepoints[i].elements[j].element_assigned_in_this_round = false;
	}
	{
		int i(0);
		while(true){
			cerr << "Round " << i+1 << ": Attempting to merge " << estimators.size() << " estimators...\n";
			i++;
			bool changes_made=merge_estimator_groups(max_dist_sq,estimators);
			if (changes_made)
				cerr << "Merge reduced to " << estimators.size() << " estimators\n";
			if (!changes_made)
				break;
		}
	}
	cerr << "Finished Merging.\n";

	
	//sort elements of the path by time, and merge them.
	//delete merged paths
	const unsigned long deletion_marker(666); //straight to hell
	for (unsigned int i = 0; i < estimators.size(); i++){
		if (estimators[i].to_be_deleted) 
			throw ns_ex("A deleted estimator survived!");
		

		//find the path that will absorb all the other points/paths
		
		ns_time_path_solver_path * path(0);
		if (estimators[i].estimators.begin()->is_a_path()){
			path = &paths[estimators[i].estimators.begin()->path_id];
		}
		else{
			unsigned long s(paths.size());
			paths.resize(s+1);
			paths[s].path_id = s;
			/*if (ns_is_close4(estimators[i].estimators.begin()->stray_point_id))
				cerr << "Merging as first member of new path\n";*/
			paths[s].elements.push_back(estimators[i].estimators.begin()->stray_point_id);
			path = &paths[s];
		}

		//merge all paths and points that need to be grouped
		ns_time_path_solution_stationary_drift_estimator_group::ns_estimator_list::iterator p(estimators[i].estimators.begin());
	
		p++;
		for (; p!= estimators[i].estimators.end(); p++){
			bool overlap(false);
			if (p->is_a_path()){
				
				if (path->elements.begin()->t_id >= paths[p->path_id].elements.rbegin()->t_id)
					overlap = true;
			//		cerr << "Out of order paths were produced!\n";
				
			/*for (unsigned int j = 0;j < paths[p->path_id].elements.size(); j++)
				if (ns_is_close4(paths[p->path_id].elements[j])){
					cerr << "Merging into " << path->path_id << " from " <<  p->path_id;
					cerr << "\n";
				}*/


				path->elements.insert(path->elements.end(),
					paths[p->path_id].elements.begin(),
					paths[p->path_id].elements.end());
				paths[p->path_id].path_id = deletion_marker;
			}
			else{
				if (path->elements.begin()->t_id >= p->stray_point_id.t_id)
					overlap = true;//cerr << "Out of order paths were produced!\n";
			/*	if (ns_is_close4(p->stray_point_id)){
					cerr << "Merging stray point into " << path->path_id << "\n";
				}*/
				path->elements.insert(path->elements.end(),p->stray_point_id);
			}
			if (overlap)
				std::sort(path->elements.rbegin(),path->elements.rend(),ns_time_element_link_orderer());
		}
	/*	for (unsigned int i = 0; i < path->elements.size(); i++){
			if (ns_is_close4(path->elements[i])){
				cerr << " Found " << path->elements[i].t_id;
				cerr << "\n";
			}
		}*/
	}
	for (vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();){
		if (p->path_id == deletion_marker)
			p = paths.erase(p);
		else p++;
	}

	//some points previously in low-temporal-resolution paths may have been added to paths.  we remove them from 
	//the low-temporal-resolution paths.
	mark_unassigned_points();
	for (std::vector<ns_time_path_solver_path>::iterator p = low_density_paths.begin(); p != low_density_paths.end();){
		for (std::vector<ns_time_element_link>::iterator e = p->elements.begin(); e != p->elements.end();){
		/*	if (ns_is_close4(*e)){
				cerr << " Found in original stray path,.";
				cerr << "\n";
			}*/
			if (element(*e).element_assigned)
				e = p->elements.erase(e);
			else e++;
		}
		if (p->elements.size() == 0)	//delete completely empty paths.
			p = low_density_paths.erase(p);
		else p++;
	}
	//set path data
	unsigned long i(0);
	for (vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();p++){
		for (unsigned int i = 1; i < p->elements.size(); i++){
			if (p->elements[i-1].t_id < p->elements[i].t_id)
				throw ns_ex("Out of order paths were produced!");
		}
		/*
		if (p->elements.size() < 3 || ((time(*p->elements.begin())-time(*p->elements.rbegin()))/60) < min_final_stationary_path_duration_in_minutes){
			if (can_search_for_stray_points_at_time(*p->elements.begin())
				|| can_search_for_stray_points_at_time(*p->elements.rbegin())){
				for (unsigned int i = 0; i < p->elements.size(); i++){
					element(p->elements[i]).e.low_temporal_resolution = true;
				}
			}
			p = paths.erase(p);
			continue;
		}
		*/
	//	ns_vector_2i ns_time_path_solver::find_max_time_position(const ns_time_path_solver_path & p) const{
		p->max_time = time(*p->elements.begin());
		p->min_time = time(*p->elements.rbegin());
		p->max_time_position = find_max_time_position(*p);//element(*p->elements.begin()).e.center;
		p->min_time_position = find_min_time_position(*p);//element(*p->elements.rbegin()).e.center;
	}

	mark_unassigned_points();
	//look for points that are inside any paths.
	//these can occur when there are a few points that aren't dense enough
	//to become high density path fragments, but are within a gap between two high-density path fragments
	//smaller than the maximum fragment joining temporal distance.
	//In this case the merge_fragment() function generates one path from thet two fragments that skip
	//over the inbetween low-density points.
	//This is actually hard to deal with elsewhere correctly, so we do it after the fact here.
	cerr << "Identifying any remaining unassigned positions.\n";
	
	for (unsigned int t = 0; t < timepoints.size(); t++){
		for (unsigned int e = 0; e < timepoints[t].elements.size(); e++){
			if (timepoints[t].elements[e].element_assigned)
				continue;

			vector<ns_time_path_solver_path>::iterator closest_path(paths.end());
			double closest_distance(DBL_MAX);
			unsigned long location_to_insert_in_closest(0);
	

			for (vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();p++){
				const long stop_i(p->elements[p->elements.size()-1].t_id),
							  start_i(p->elements[0].t_id);
				if (t <= stop_i || t >= start_i)	//point does not fall within the current path
					continue;
				bool path_already_has_point_at_curent_time(false);
				unsigned long location_to_insert(0);
				for (unsigned int i = 0; i < p->elements.size(); i++){
					//skip if path already has a point at the specified time.
					if (p->elements[i].t_id == t){
						path_already_has_point_at_curent_time = true;
						break;
					}
					if (p->elements[i].t_id <= t){	// path elements are sorted in reverse temporal order so we don't need to search further.
						location_to_insert = i;
						break;
					}
				}
				if (path_already_has_point_at_curent_time)
					continue;
				ns_vector_2d path_pos = element(p->elements[location_to_insert-1]).e.center + element(p->elements[location_to_insert]).e.center;
				path_pos/=2.0;
				const double d((path_pos-timepoints[t].elements[e].e.center).squared());
				if  (d <= max_dist_sq && d < closest_distance){
						closest_path = p;
						location_to_insert_in_closest = location_to_insert;
						closest_distance = d;
				}
			}
			if (closest_path != paths.end()){
				timepoints[t].elements[e].e.low_temporal_resolution = true;
				//insert immediately before the next path_element_id;
	//			if (ns_is_close4(ns_time_element_link(t,e)))
	//				cerr << "ADDING IT!\n";
				closest_path->elements.insert(closest_path->elements.begin()+location_to_insert_in_closest,ns_time_element_link(t,e));
				break;
			}
		}
	}
}


void ns_time_path_solver::find_stationary_path_fragments(const double fraction_of_points_required_to_be_detected, const unsigned long min_path_duration_in_seconds, const unsigned long time_window_length_in_seconds,const unsigned long max_movement_distance){
	const unsigned long mdsq(max_movement_distance*max_movement_distance);
	
	std::vector<ns_time_path_solver_path_builder> open_paths;
	open_paths.reserve(100);
	paths.reserve(100);
	unsigned long debug_max_paths(0),debug_paths_discarded_for_being_short(0),debug_paths_discarded_for_low_density(0);
	unsigned long max_discarded_path_length(0), min_discarded_path_length(INT_MAX);
	unsigned long debug_paths_discarded_for_being_short_at_end(0);
	unsigned long longest_path_length(0);
	for (long i = (long)timepoints.size()-1; i >= 0; i--){
		//attempt to assign an element to an existing path

		//xxx
		//if (timepoints[i].time > 1359806811 && timepoints[i].time < 1361495211)
	//		cerr << "WHA";
		assign_timepoint_elements_to_paths(timepoints[i].elements,mdsq,open_paths);
		if (open_paths.size() > debug_max_paths)
			debug_max_paths = open_paths.size();
		for (unsigned int j= 0; j < timepoints[i].elements.size(); j++){
			//if we haven't been able to assign an element, use it to create a new seed
			if (timepoints[i].elements[j].element_assigned_in_this_round) {
				open_paths[timepoints[i].elements[j].path_id].elements.push_back(ns_time_path_solver_path_builder_point(ns_time_element_link(i,j),timepoints[i].elements[j].e.center,timepoints[i].time));
				open_paths[timepoints[i].elements[j].path_id].calculate_center(time_window_length_in_seconds);
			}
			else{
				//ns_is_interesting_point(timepoints[i].elements[j].e.center);
				open_paths.push_back(ns_time_path_solver_path_builder(ns_time_path_solver_path_builder_point(ns_time_element_link(i,j),timepoints[i].elements[j].e.center,timepoints[i].time)));
			}
		}
		
		//close paths that have too large a time gap between their end and the current frame
		for(std::vector<ns_time_path_solver_path_builder>::iterator p = open_paths.begin(); p!=open_paths.end();){
			if (longest_path_length < p->elements.size())
				longest_path_length = p->elements.size();
			//the path is closed.  We either move it to the list of good paths, or delete it.
			if (p->elements.begin()->time - timepoints[i].time < time_window_length_in_seconds){
				//don't start deleting potential paths until we have a good estimate of their density
				p++;
				continue;
			}
			const double current_path_point_density(
				p->calculate_current_density(time_window_length_in_seconds,timepoints,i)
				);
			if (current_path_point_density < fraction_of_points_required_to_be_detected){
				
				if (p->elements.size() > 1 && p->elements.begin()->time-p->elements.rbegin()->time >= min_path_duration_in_seconds){
					paths.push_back(*p);
					paths.rbegin()->path_id = paths.size();
				}

				if (p->elements.size() > 1 && p->elements.begin()->time-p->elements.rbegin()->time < min_path_duration_in_seconds){
					unsigned long dt(p->elements.begin()->time-p->elements.rbegin()->time);
					debug_paths_discarded_for_being_short++;
					if (max_discarded_path_length < dt)
						max_discarded_path_length = dt;
					if (min_discarded_path_length > dt)
						min_discarded_path_length = dt;
				}


				p = open_paths.erase(p);
			}
			else p++;

		}
	}
	
	//close any remaining open paths
	for (unsigned int i = 0; i < open_paths.size(); i++){
		//make sure that the current open path is high enough density at it's end
		//const double current_path_point_density(open_paths[i].calculate_current_density(time_window_length_in_seconds,timepoints,open_paths[i].elements[0].link.t_id));
	
		if (open_paths[i].elements.size() > 1
			&& open_paths[i].elements.begin()->time-open_paths[i].elements.rbegin()->time >= min_path_duration_in_seconds){
			paths.push_back(open_paths[i]);
			paths.rbegin()->path_id = paths.size();
		}

		if (open_paths[i].elements.size() > 1 && open_paths[i].elements.begin()->time-open_paths[i].elements.rbegin()->time < min_path_duration_in_seconds){
				debug_paths_discarded_for_being_short_at_end++;
				unsigned long dt(open_paths[i].elements.begin()->time-open_paths[i].elements.rbegin()->time);
				if (max_discarded_path_length < dt)
					max_discarded_path_length = dt;
				if (min_discarded_path_length > dt)
					min_discarded_path_length = dt;
		}
	}
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::find_stationary_tyj path_fragments::Maximum simultaneous open paths: ") << debug_max_paths);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::find_stationary_path_fragments::Paths discarded for being too short: ") << debug_paths_discarded_for_being_short);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::find_stationary_path_fragments::Paths discarded for being too short at end: ") << debug_paths_discarded_for_being_short_at_end);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::find_stationary_path_fragments::Minimum discarded path length: ") << min_discarded_path_length);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::find_stationary_path_fragments::Maximum discarded path length: ") << max_discarded_path_length);
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::find_stationary_path_fragments::Longest path length: ") << longest_path_length);
}

unsigned long ns_time_path_solver::number_of_unassigned_points_at_time(const ns_time_element_link & l) const {
	unsigned long number_of_unassigned_points(0);
	for (unsigned int j = 0; j < timepoints[l.t_id].elements.size(); j++)
		number_of_unassigned_points+=timepoints[l.t_id].elements[j].element_assigned?0:1;
	return number_of_unassigned_points;
}
void ns_time_path_solver::find_low_density_stationary_paths(const unsigned long min_path_duration_in_seconds, const unsigned long max_movement_distance){
	const unsigned long mdsq(max_movement_distance*max_movement_distance);
	mark_unassigned_points();
	std::vector<ns_time_path_solver_path_builder> open_paths;
	open_paths.reserve(10);
	low_density_paths.resize(0);

	for (long i = (long)timepoints.size()-1; i >= 0; i--){
		const bool start_new_paths(can_search_for_stray_points_at_time(ns_time_element_link(i,0)));
		
		if (!start_new_paths)
			continue;
		//attempt to assign an element to an existing path.  Elements already in paths are ignored
		assign_timepoint_elements_to_paths(timepoints[i].elements,mdsq,open_paths);
		for (unsigned int j= 0; j < timepoints[i].elements.size(); j++){
			if (timepoints[i].elements[j].element_assigned)
				continue;
			//if we haven't been able to assign an element, use it to create a new seed
			if (timepoints[i].elements[j].element_assigned_in_this_round) {
				open_paths[timepoints[i].elements[j].path_id].elements.push_back(ns_time_path_solver_path_builder_point(ns_time_element_link(i,j),timepoints[i].elements[j].e.center,timepoints[i].time));
				open_paths[timepoints[i].elements[j].path_id].calculate_center(0);
			}
			else{
				open_paths.push_back(ns_time_path_solver_path_builder(ns_time_path_solver_path_builder_point(ns_time_element_link(i,j),timepoints[i].elements[j].e.center,timepoints[i].time)));
			}
		}
	}
	//close any remaining open paths that meet the required criterea
	for (unsigned int i = 0; i < open_paths.size(); i++){
		if (open_paths[i].elements.size() > 3 && 
			open_paths[i].elements.begin()->time-open_paths[i].elements.rbegin()->time >= min_path_duration_in_seconds)
			low_density_paths.push_back(open_paths[i]);
	}
	for (unsigned int i = 0; i < low_density_paths.size(); i++){
		low_density_paths[i].is_low_density_path = true;
		low_density_paths[i].max_time = time(*low_density_paths[i].elements.begin());
		low_density_paths[i].min_time = time(*low_density_paths[i].elements.rbegin());
		low_density_paths[i].max_time_position = find_max_time_position(low_density_paths[i]);//element(*low_density_paths[i].elements.begin()).e.center;
		low_density_paths[i].min_time_position = find_min_time_position(low_density_paths[i]);;///element(*low_density_paths[i].elements.rbegin()).e.center;
		for (unsigned int j = 0; j < low_density_paths[i].elements.size(); j++){
			element(low_density_paths[i].elements[j]).e.low_temporal_resolution = true;
		}
	}
}



bool operator ==(const ns_time_element_link & a, const ns_time_element_link & b){
	return a.t_id == b.t_id && a.index == b.index;
}

void ns_time_path_solver_element::load(const ns_detected_worm_info * w){
	e = ns_time_path_element(w);
}
void ns_time_path_solver::mark_unassigned_points(){
	for (unsigned int i = 0; i < timepoints.size(); i++){
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
			timepoints[i].elements[j].element_assigned = false;
		}
	}
	for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++){
			element(paths[i].elements[j]).element_assigned = true;
		}
	}
}

void ns_time_path_solver_timepoint::load(const unsigned long worm_detection_results_id,ns_image_worm_detection_results & results,ns_sql & sql){
	elements.resize(0);
	worm_detection_results = &results;
	results.id = worm_detection_results_id;
	results.load_from_db(false,false,sql);
	const std::vector<const ns_detected_worm_info *> & worms(results.actual_worm_list());
//	if (results.capture_time == 1321821542)		

//		cerr << "WHA";
	elements.resize(worms.size());
	for(unsigned int i = 0; i < worms.size(); i++){
		elements[i].load(worms[i]);
//		if (ns_vector_2i(600,209) == elements[i].e.context_image_position_in_region_vis_image)
//			cerr << "WAH";
	}

	//later on we might need to load worm images from the worm region visualization image.
	//thus, we load the location of each worm's image in that bitmap.
	vector<ns_vector_2i> positions;
	results.worm_collage.info().image_locations_in_collage((unsigned long)worms.size(),positions);
	
	for(unsigned int i = 0; i < worms.size(); i++){
		elements[i].e.context_image_position_in_region_vis_image = positions[i];
		//cerr << "Position : " << positions[i] << "\n";
	}
}

void ns_register_path_solver_load_error(unsigned long region_info_id,const std::string & expl, ns_sql & sql){
	ns_ex ex("For ");
	sql << "SELECT r.name,s.id,s.name,e.name FROM sample_region_image_info as r,capture_samples as s,"
			"experiments as e WHERE r.sample_id = s.id AND s.experiment_id = e.id AND r.id = " << region_info_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0){
		ex << "region info id ";
		ex << region_info_id;
	}
	else ex <<res[0][3]<<"::" <<res[0][2] << "::" <<res[0][0];
	ex << ":" << expl;
	image_server.register_server_event(ex,&sql);

}

void ns_time_path_solver::load_detection_results(unsigned long region_id,ns_sql & sql){
	sql << "SELECT time_of_last_valid_sample FROM sample_region_image_info WHERE id = " << region_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_time_path_solver::load()::Could not load region ") << region_id << " from database.";
	unsigned long time_of_last_valid_sample = atol(res[0][0].c_str());

	if (time_of_last_valid_sample == 0)
		ns_global_debug(ns_text_stream_t("ns_time_path_solver::load_detection_results()::Time of last valid sample is not set."));
	else
		ns_global_debug(ns_text_stream_t("ns_time_path_solver::load_detection_results()::Time of last valid sample = ") << ns_format_time_string_for_human(time_of_last_valid_sample));
	

	sql << "SELECT worm_detection_results_id,capture_time, id FROM sample_region_images WHERE region_info_id = " 
		<< region_id << " AND worm_detection_results_id != 0 AND problem = 0 AND currently_under_processing = 0 "
		<< "AND censored = 0";
	if (time_of_last_valid_sample != 0)
		sql << " AND capture_time <= " << time_of_last_valid_sample;
	sql << " ORDER BY capture_time ASC";
	
	ns_sql_result time_point_result;
	sql.get_rows(time_point_result);
	
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::load_detection_results()::Found ") << time_point_result.size() << " time points (measurement times) in the db");
	
	//output a whole bunch of debug information if no results are found
	if (time_point_result.empty()){
		ns_sql_result res;
		sql << "SELECT worm_detection_results_id,capture_time, id FROM sample_region_images WHERE region_info_id = " 
			<< region_id;
		sql.get_rows(res);
		ns_global_debug(ns_text_stream_t("ns_time_path_solver::load():Images in region: ")<<res.size());

		sql << "SELECT worm_detection_results_id,capture_time, id FROM sample_region_images WHERE region_info_id = " 
			<< region_id << " AND worm_detection_results_id != 0";
		sql.get_rows(res);
		ns_global_debug(ns_text_stream_t("ns_time_path_solver::load():Images with worm detection complete: ")<<res.size());

		sql << "SELECT worm_detection_results_id,capture_time, id FROM sample_region_images WHERE region_info_id = " 
			<< region_id << " AND worm_detection_results_id != 0 AND problem = 0";
		sql.get_rows(res);
		ns_global_debug(ns_text_stream_t("ns_time_path_solver::load():Images without problem flag: ")<<res.size());

		sql << "SELECT worm_detection_results_id,capture_time, id FROM sample_region_images WHERE region_info_id = " 
			<< region_id << " AND worm_detection_results_id != 0 AND problem = 0 AND currently_under_processing = 0";
		sql.get_rows(res);
		ns_global_debug(ns_text_stream_t("ns_time_path_solver::load():Images not being processed ")<<res.size());

		sql << "SELECT worm_detection_results_id,capture_time, id FROM sample_region_images WHERE region_info_id = " 
		<< region_id << " AND worm_detection_results_id != 0 AND problem = 0 AND currently_under_processing = 0 "
		<< "AND censored = 0";
		sql.get_rows(res);
		ns_global_debug(ns_text_stream_t("ns_time_path_solver::load():Images not censored ")<<res.size());
	}
	timepoints.reserve(time_point_result.size());
	set<long> times;
	set<long> detection_ids;
	detection_results->results.resize(time_point_result.size());
	unsigned long current_timepoint_i(0);
	cout << "Loading Worm Detection Results Metadata...\n";
	for (unsigned int i = 0; i < time_point_result.size();i++){
		if (current_timepoint_i == timepoints.size())
			timepoints.resize(timepoints.size()+1);
		if (current_timepoint_i > timepoints.size())
			throw ns_ex("ns_time_path_solver::load_detection_results()::Logic Error!");
		timepoints[current_timepoint_i].time = atol(time_point_result[i][1].c_str());
		timepoints[current_timepoint_i].sample_region_image_id = atol(time_point_result[i][2].c_str());
		const unsigned long worm_detection_results_id(atol(time_point_result[i][0].c_str()));
		//if (timepoints[current_timepoint_i].time == 1321821542)
		//	cerr << "MA";
		if (!times.insert(timepoints[current_timepoint_i].time).second){
			ns_register_path_solver_load_error(region_id,
			std::string("A duplicate capture sample entry was discovered at time ")
			+ ns_format_time_string_for_human(timepoints[current_timepoint_i].time) + "(" + ns_to_string(timepoints[current_timepoint_i].time) + ")",sql);
			//timepoints.pop_back();
		}
		else if (!detection_ids.insert(worm_detection_results_id).second){
			ns_register_path_solver_load_error(region_id,
			std::string("A duplicate worm detection entry was discovered at time ")
			+ ns_format_time_string_for_human(timepoints[current_timepoint_i].time) + "(" + ns_to_string(timepoints[current_timepoint_i].time) + ")",sql);
			//timepoints.pop_back();
		}
		else{
			timepoints[current_timepoint_i].worm_results_id = worm_detection_results_id;
			current_timepoint_i++;
		}
		//else timepoints.rbegin()->load(worm_detection_results_id,detection_results->results[i],sql);
		
	}
	if (timepoints.size() != current_timepoint_i)
		timepoints.resize(current_timepoint_i);
	
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::load_detection_results()::Found ") << timepoints.size() << " valid timepoints.");
}
void ns_time_path_solver::load(unsigned long region_id, ns_sql & sql){
	
	load_detection_results(region_id,sql);

	long last_c(-2);
	cerr << "Compiling Detection Point Cloud...";
	unsigned long debug_max_points_per_timepoint(0);
	bool problem = false;
	for (unsigned int i = 0; i < timepoints.size(); i++){
		
		if ((long)((i*100)/timepoints.size()) >= last_c+5){
			cerr << (i*100)/timepoints.size() << "%...";
			last_c = i;
		}
	//	if (timepoints[i].time == 1321821542)
	//		cerr << "WHA";
		try{
			timepoints[i].load(timepoints[i].worm_results_id,detection_results->results[i],sql);
		}
		catch(ns_ex & ex){
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
			sql << "UPDATE sample_region_images SET " 
				<< ns_processing_step_db_column_name(ns_process_worm_detection) << " = 0,"
				<< "worm_detection_results_id = 0"
				<< " WHERE id = " << timepoints[i].sample_region_image_id;
			sql.send_query();
			problem = true;
			continue;
		}
		if (timepoints[i].elements.size() > debug_max_points_per_timepoint)
			debug_max_points_per_timepoint = timepoints[i].elements.size();
	}
	cerr << "\n";
	if (problem)
		throw ns_ex("One or more timepoints did not load correctly.  The problematic images have had their worm detection results deleted; recompute these and try again.");
	ns_global_debug(ns_text_stream_t("ns_time_path_solver::load()::Max number of elements per timepoint:") << debug_max_points_per_timepoint);
	
	unsigned long debug_combined_elements(0);
	for (unsigned int i = 0; i < timepoints.size(); i++){
		if (timepoints[i].combine_very_close_elements(i))
			debug_combined_elements++;
	}

	ns_global_debug(ns_text_stream_t("ns_time_path_solver::load()::Combined elements:") << debug_combined_elements);
	
}

bool ns_time_path_solver_timepoint::combine_very_close_elements(const unsigned long max_d_squared){
	for (std::vector<ns_time_path_solver_element>::iterator p = elements.begin(); p != elements.end(); p++)
		p->element_assigned = false;

	//add duplicates as children
	for (std::vector<ns_time_path_solver_element>::iterator p = elements.begin(); p != elements.end(); p++){
		if (p->element_assigned) continue;
		std::vector<ns_time_path_solver_element>::iterator q = p;
		q++;
		for (; q != elements.end(); q++){
			if (q->element_assigned) continue;
			if ((p->e.center - q->e.center).squared() <= max_d_squared){
				p->extra_elements_at_current_position.push_back(q->e);
				q->element_assigned = true;
			}
		}
	}
	bool removed(false);
	//remove duplicates from base list
	for (std::vector<ns_time_path_solver_element>::iterator p = elements.begin(); p != elements.end();){
		if (p->element_assigned){
			p = elements.erase(p);
			removed = true;
		}
		else p++;
	}
	return removed;
}

///void ns_time_path_solver::check_for_duplicate_events(){
//	for (unsigned int i = 0; i < timepoints.size(); i++)		
//		timepoints[i].combine_very_close_elements(4);
//}
void ns_time_path_solver::generate_raw_solution(ns_time_path_solution & solution){
	this->paths.resize(0);
	this->path_groups.resize(0);
	for (unsigned int i = 0; i < timepoints.size(); i++){
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
			timepoints[i].elements[j].path_id = 0;
			timepoints[i].elements[j].element_assigned = false;
		}
	}
	transfer_data_to_solution(solution);
}
void ns_time_path_solver::load_from_raw_solution(const unsigned long region_id,const ns_time_path_solution & solution,ns_sql & sql){
	timepoints.resize(solution.timepoints.size());
	for (unsigned int i = 0; i < solution.timepoints.size(); i++){
		timepoints[i].elements.resize(solution.timepoints[i].elements.size());
		for (unsigned int j = 0; j < solution.timepoints[i].elements.size(); j++){
			timepoints[i].elements[j].e = solution.timepoints[i].elements[j];
			timepoints[i].elements[j].element_assigned = false;
			timepoints[i].elements[j].element_assigned_in_this_round = false;
			timepoints[i].elements[j].path_id = 0;
		}
	}
	load_detection_results(region_id,sql);
}
void ns_time_path_solver::transfer_data_to_solution(ns_time_path_solution & solution){
	solution.clear();
	//transfer all worm results data
	solution.set_results(detection_results);
	solution.worms_loaded = true;
	detection_results = 0;

	//transfer all points and populate unassigned points path.
	solution.timepoints.resize(timepoints.size());
	for (unsigned int i = 0; i < timepoints.size(); i++){
		solution.timepoints[i].time = timepoints[i].time;
		solution.timepoints[i].sample_region_image_id = timepoints[i].sample_region_image_id;
		solution.timepoints[i].elements.reserve(timepoints[i].elements.size());
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
			solution.timepoints[i].elements.push_back(timepoints[i].elements[j].e);
			solution.timepoints[i].elements.rbegin()->number_of_extra_worms_identified_at_location = 
					timepoints[i].elements[j].extra_elements_at_current_position.size();
	//		cerr << timepoints[i].elements[j].e.position_in_region_vis_image << "vs" << solution.timepoints[i].elements.rbegin()->position_in_region_vis_image << "\n";
			if (timepoints[i].elements[j].path_id == 0)
				solution.unassigned_points.stationary_elements.push_back(ns_time_element_link(i,j));
		}
	}

	solution.paths.reserve(path_groups.size());
	solution.path_groups.reserve(path_groups.size());
	for (unsigned int i = 0; i < path_groups.size(); i++){	
		unsigned long path_length(0);
		unsigned long s = (unsigned long)solution.path_groups.size();
		unsigned long number_of_uncensored_paths_in_group(0);
		for (unsigned int j = 0; j < path_groups[i].path_ids.size(); j++){
			ns_time_path_solver_path & path(paths[path_groups[i].path_ids[j]]); //path id 0 is ungrouped elements
			if (path.is_not_stationary) {
				cerr << "SKipping noise";
				continue;
			}
			if (number_of_uncensored_paths_in_group==0){
				solution.path_groups.resize(s+1);
				solution.path_groups[s].path_ids.push_back(s);
				solution.paths.resize(s+1);
				solution.paths[s].center = ns_vector_2i(0,0);
				solution.paths[s].stationary_elements.reserve(path.elements.size()*path_groups[i].path_ids.size());
				solution.paths[s].is_low_density_path = false;
			}
			unsigned long number_of_low_density_points(0),
						  number_of_high_density_points(0);
			for (unsigned int k = 0; k < path.elements.size(); k++){
				solution.paths[s].stationary_elements.push_back(path.elements[k]);
				solution.paths[s].center += element(path.elements[k]).e.center;
				number_of_low_density_points += element(path.elements[k]).e.low_temporal_resolution?1:0;
				number_of_high_density_points += element(path.elements[k]).e.low_temporal_resolution?0:1;
			}
			if (2*number_of_low_density_points >= path.elements.size()){
				solution.paths[s].is_low_density_path = true;
				if (number_of_high_density_points != 0){
					for (unsigned int k = 0; k < solution.paths[s].stationary_elements.size(); k++)
						solution.element(solution.paths[s].stationary_elements[k]).low_temporal_resolution = true;
				}
			}
			else{
				if (number_of_high_density_points != 0){
					for (unsigned int k = 0; k < solution.paths[s].stationary_elements.size(); k++)
						solution.element(solution.paths[s].stationary_elements[k]).low_temporal_resolution = false;
				}
			}
			number_of_uncensored_paths_in_group++;
		}
		if (path_length != 0)
		solution.paths[s].center = solution.paths[s].center/=path_length;
	}
	
	solution.check_for_duplicate_events();
}

void ns_time_path_solution::output_mathematica_file(std::ostream & o){
	o << "cc := ColorData[\"Rainbow\"]\n";
	o << "Graphics3D[{";
	o << "Point[{";
	//output unassigned points
	for (unsigned int i = 0; i< unassigned_points.stationary_elements.size(); i++){
		o << "{" << (time(unassigned_points.stationary_elements[i])- timepoints[0].time)/60.0/60.0/24.0 << ","
				 << element(unassigned_points.stationary_elements[i]).center.x << ","
				 << element(unassigned_points.stationary_elements[i]).center.y << "}";
		if (i+1 != unassigned_points.stationary_elements.size())
			o << ",";
	}
	o << "}]";
	//output paths
	for (unsigned int g = 0; g < path_groups.size(); g++){
		for (unsigned int i = 0; i < path_groups[g].path_ids.size(); i++){
			//output color
			o << ",cc[" << g/(double)path_groups.size() << "],";
			o << "Line[{";
			for (unsigned int j = 0; j < paths[path_groups[g].path_ids[i]].stationary_elements.size(); j++){
				ns_time_element_link & e(paths[path_groups[g].path_ids[i]].stationary_elements[j]);
				o << "{";
				o << (time(e)- timepoints[0].time)/60.0/60.0/24.0 << ","
				  << element(e).center.x << ","
				  << element(e).center.y << "}";
				if (j+1 != paths[path_groups[g].path_ids[i]].stationary_elements.size())
					o << ",";
			}
			o << "}],";
			o<< "cc[" << g/(double)path_groups.size() << "],";
			o << "Point[{";
			for (unsigned int j = 0; j < paths[path_groups[g].path_ids[i]].stationary_elements.size(); j++){
				ns_time_element_link & e(paths[path_groups[g].path_ids[i]].stationary_elements[j]);
				o << "{";
				o << (time(e)- timepoints[0].time)/60.0/60.0/24.0 << ","
				  << element(e).center.x << ","
				  << element(e).center.y << "}";
				if (j+1 != paths[path_groups[g].path_ids[i]].stationary_elements.size())
					o << ",";
			}
			o << "}]";
		}
	}
	o << ",BoxRatios->{1,1,1}}];";
}

void ns_time_path_solution::output_visualization_csv(ostream & o){
	ns_time_path_solution::output_visualization_csv_header(o);
	//output unassigned points
	for (unsigned int i = 0; i< unassigned_points.stationary_elements.size(); i++){
		ns_time_path_solution::output_visualization_csv_data(o,
			((float)(time(unassigned_points.stationary_elements[i])- timepoints[0].time))/60.0/60.0/24.0,
			time(unassigned_points.stationary_elements[i]),
			element(unassigned_points.stationary_elements[i]).center,
			element(unassigned_points.stationary_elements[i]).region_size,
			0,
			0,
			element(unassigned_points.stationary_elements[i]).slowly_moving,
			element(unassigned_points.stationary_elements[i]).low_temporal_resolution,
			element(unassigned_points.stationary_elements[i]).inferred_animal_location,
			element(unassigned_points.stationary_elements[i]).element_before_fast_movement_cessation,
				0,0,
				ns_movement_not_calculated);
	}
	//output paths
	for (unsigned int g = 0; g < path_groups.size(); g++){
		for (unsigned int i = 0; i < path_groups[g].path_ids.size(); i++){
			for (unsigned int j = 0; j < paths[path_groups[g].path_ids[i]].moving_elements.size(); j++){
				ns_time_element_link & e(paths[path_groups[g].path_ids[i]].moving_elements[j]);
				ns_time_path_solution::output_visualization_csv_data(o,
					 ((float)(time(e)- timepoints[0].time))/60.0/60.0/24.0,
					 time(e),
					 element(e).center,
					 element(e).region_size,
					 path_groups[g].path_ids[i]+1,
					 g+1,
					 1,
					 (element(e).low_temporal_resolution ||
						paths[path_groups[g].path_ids[i]].is_low_density_path),0,false,false,
					 0,ns_movement_not_calculated
				);
			}
			for (unsigned int j = 0; j < paths[path_groups[g].path_ids[i]].stationary_elements.size(); j++){
				ns_time_element_link & e(paths[path_groups[g].path_ids[i]].stationary_elements[j]);
				ns_time_path_solution::output_visualization_csv_data(o,
					 ((float)(time(e)- timepoints[0].time))/60.0/60.0/24.0,
					 time(e),
					 element(e).center,
					 element(e).region_size,
					 path_groups[g].path_ids[i]+1,
					 g+1,
					 0,
					 (element(e).low_temporal_resolution ||
						paths[path_groups[g].path_ids[i]].is_low_density_path),
					0,element(e).inferred_animal_location,
					element(e).element_before_fast_movement_cessation,
					 0,ns_movement_not_calculated
				);
			}
		}
	}
}
void ns_time_path_solver::output_visualization_csv(ostream & o){
	unsigned int d(1);
	ns_time_path_solution::output_visualization_csv_header(o);
	for (unsigned int i = 0; i < timepoints.size(); i+=d){
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
			ns_time_path_solution::output_visualization_csv_data(
				o,
				(float)(timepoints[i].time - timepoints[0].time)/60.0/60.0/24.0,
				timepoints[i].time,
				timepoints[i].elements[j].e.center,
				timepoints[i].elements[j].e.region_size,
				timepoints[i].elements[j].path_id,
				0,
				timepoints[i].elements[j].e.slowly_moving,
				timepoints[i].elements[j].e.low_temporal_resolution,
				false,
				0,false,false,
				ns_movement_not_calculated
				);
		}
	}
}

void ns_time_path_solver::assign_path_ids_to_elements(){
		//for now, just put one path per group.
	path_groups.resize(0);
	path_groups.resize(paths.size());
	for (unsigned int i = 0; i < paths.size(); i++){
		paths[i].path_id = 0;
		paths[i].group_id = i+1;
		path_groups[i].path_ids.push_back(i);
	}

	for (unsigned int i = 0; i < timepoints.size(); i++){
		for (unsigned int j = 0; j < timepoints[i].elements.size(); j++){
			timepoints[i].elements[j].path_id = 0;
		}
	}

	for (unsigned int i = 0; i < paths.size(); i++){
		for (unsigned int j = 0; j < paths[i].elements.size(); j++){
			if (element(paths[i].elements[j]).path_id != 0 )
				cerr << paths[i].elements[j].t_id << ": " << paths[i].elements[j].index << " "<< element(paths[i].elements[j]).e.center << 
				" assigned to both " << element(paths[i].elements[j]).path_id << " (position " << j << "/" << paths[i].elements.size() << " and " << paths[i].group_id << "\n";
			element(paths[i].elements[j]).path_id = paths[i].group_id;
		}
	}
}

typedef std::vector<ns_time_path_solver_path> ns_time_path_solver_path_list;
typedef std::vector<ns_time_path_solver_path_list::iterator> ns_time_path_solver_path_orderer_list;
typedef std::map<unsigned long,ns_time_path_solver_path_orderer_list > ns_time_path_solver_path_orderer;

bool ns_is_close(ns_vector_2i & p){
	
	return (abs(p.x - 4580)<50 && abs(p.y-3500) < 50);
		
}
struct ns_default_zero{
	ns_default_zero():count(0){}
	unsigned long count;
};
double ns_time(const unsigned long t){
	return (t-1310000000)/60.0/60.0/24;
}
std::ostream & operator << (std::ostream & o,const ns_time_path_solver_path & p){
	o << p.min_time_position.x << "," << p.min_time_position.y << "(" << ns_time(p.min_time) << "-" << ns_time(p.max_time) << ")";
	return o;
}
bool ns_time_path_solver::is_ok_to_merge_overlap(const ns_time_path_solver_path & later, const ns_time_path_solver_path & earlier,const unsigned long max_time_gap, const unsigned long max_time_overlap,const double max_fraction_points_overlap){
	if (later.max_time < earlier.max_time)
		throw ns_ex("YIKES");
//	cerr << "Comparing " << later << " and " << earlier << " ";
	
	bool one_within_another = later.min_time <= earlier.min_time &&
							  later.max_time >= later.max_time ||
							  earlier.min_time <= later.min_time &&
							  earlier.max_time >= later.max_time;
	if (!one_within_another){
		if (earlier.min_time >= later.min_time || earlier.max_time == later.max_time ){
		//	cerr << "Within.\n";
			return false;
		}
	
		 //too far apart
		if (earlier.max_time + max_time_gap < later.min_time){
	//		cerr << "Too large gap\n";
				return false;
		}
		//too large an overlap
		if (earlier.max_time >= later.min_time &&
			earlier.max_time - later.min_time > max_time_overlap){
		//		cerr << "Too large overlap\n";
			return false;
		}
	}
	const unsigned long late_min_t_index(later.elements.rbegin()->t_id),
		early_max_t_index(earlier.elements.begin()->t_id);

	//there is a small overlap
	if (late_min_t_index < early_max_t_index){
		//<tid,number_of_occurances>
		std::map<unsigned long,ns_default_zero> indicies_in_overlap;
		for (unsigned int i = 0; i < earlier.elements.size(); i++){
			if (earlier.elements[i].t_id >= late_min_t_index){
				indicies_in_overlap[earlier.elements[i].t_id].count++;
			}
		}
		for (unsigned int i = 0; i < later.elements.size(); i++){
			if (later.elements[i].t_id <= early_max_t_index){
				indicies_in_overlap[later.elements[i].t_id].count++;
			}
		}
		unsigned long singles(0),multiples(0);
		for (std::map<unsigned long,ns_default_zero>::const_iterator p = indicies_in_overlap.begin(); p != indicies_in_overlap.end(); p++){
			if (p->second.count > 1)
				multiples++;
			else singles++;
		}
		//too many redundant points in curves.
		if (multiples/(double)(singles+multiples) > max_fraction_points_overlap){
	//		cerr << "Too many shared overlap points\n";
			return false;
		}
	}
//	cerr << "Good!\n";
	return true;

}
void ns_time_path_solver::merge_overlapping_path_fragments(const unsigned long max_center_distance,const unsigned long max_time_gap, const unsigned long max_time_overlap,const double max_fraction_points_overlap){
	//note here that the merged paths have
	//1) All their elements sorted in order of decreasing time
	//2) Their center matched as the center of their earliest fragment
	const long max_dist_sq(max_center_distance*max_center_distance);
	ns_time_path_solver_path_orderer paths_ordered_by_max_time;
	ns_time_path_solver_path_orderer paths_ordered_by_min_time;

	
	const unsigned long erase_constant(6666666666666);
	for (std::vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();p++){
		p->group_id = 0;
	}
	bool merge_performed = false;
	//merge paths forward in time
	for (std::vector<ns_time_path_solver_path>::iterator later = paths.begin(); later != paths.end();){
		if (later->path_id == erase_constant){
			later++;
			continue;
		}
		if (!merge_performed){
			//paths_ordered_by_max_time.clear();
			paths_ordered_by_min_time.clear();
			for (std::vector<ns_time_path_solver_path>::iterator q = paths.begin(); q != paths.end();q++){
				if (q->group_id == erase_constant)  //ignore paths marked for deletion
					continue;
				//if (p == q)	//don't merge with self
				//	continue;
				//if (q->max_time > p->max_time)
				//	continue;
				//paths_ordered_by_max_time[p->max_time].push_back(q);
				paths_ordered_by_min_time[q->max_time].push_back(q);
				
			}
		}
		merge_performed = false;
		for (ns_time_path_solver_path_orderer::iterator q =paths_ordered_by_min_time.begin();	
				q != paths_ordered_by_min_time.end() && !merge_performed; q++){
			for (ns_time_path_solver_path_orderer_list::iterator earlier = q->second.begin(); earlier != q->second.end();earlier++){
			//	if ((later->path_id == 24 || later->path_id == 2 ) &&
				//	((*earlier)->path_id == 24 || (*earlier)->path_id == 2))
					//cerr << "WHA";
				if (later == *earlier)
					continue;
				if (later->max_time <= (*earlier)->max_time)
					continue;
				if ((*earlier)->group_id == erase_constant)
					continue;
	//ns_is_close((*r)->min_time_position);
	//				ns_is_close(p->max_time_position);
			//	if (ns_is_close(later->max_time_position) && ns_is_close((*earlier)->min_time_position))
			//		cerr << "WHA";
				if (is_ok_to_merge_overlap(*later,*(*earlier),max_time_gap,max_time_overlap, max_fraction_points_overlap)){
				//if (p->max_time < (*r)->min_time &&
				//	p->max_time + max_time_gap >= (*r)->min_time){
					//the two paths overlap spacially
					//if (element(p->elements[0]).x
					if (((*earlier)->min_time_position - later->max_time_position).squared() > max_dist_sq){
				//		if (ns_is_close(later->max_time_position) && ns_is_close((*earlier)->min_time_position))
				//			cerr << "WHA";
						//not close enough
						continue;
					}
			//		if (((*earlier)->path_id == 24 || (*earlier)->path_id == 32 || (*earlier)->path_id ==  74) ||
				//		(later->path_id == 24 || later->path_id == 32 || later->path_id ==  74))
					//	cerr << "Merging " << (*earlier)->path_id << " with " << later->path_id << "\n";
					bool overlap(false);
					if (later->elements.rbegin()->t_id < (*earlier)->elements.begin()->t_id)
						overlap = true;
					later->elements.insert(later->elements.end(),(*earlier)->elements.begin(),(*earlier)->elements.end());
					if (overlap){
						std::sort(later->elements.rbegin(),later->elements.rend(),ns_time_element_link_orderer());

				
					}
					later->min_time = timepoints[later->elements.rbegin()->t_id].time;
					later->min_time_position = find_min_time_position(*later);//element(*p->elements.begin()).e.center;
					(*earlier)->group_id = erase_constant; //delete it
					merge_performed = true;
					break;
				}
			}
		}
		if (merge_performed) //if we've made a change to the current timepoint, do another search
			continue;
		else later++;
	}

	for (std::vector<ns_time_path_solver_path>::iterator p = paths.begin(); p != paths.end();){
		if (p->group_id == erase_constant){
	//		cerr << "ERASING" << *p << "\n";
			p = paths.erase(p);
		}
		else p++;
	}
}
