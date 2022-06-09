#include "ns_image_server.h"
#include "ns_barcode.h"
#include "ns_socket.h"
#include "ns_thread.h"
#include "ns_image_server_message.h"
#ifndef NS_MINIMAL_SERVER_BUILD
#include "ns_image_server_dispatcher.h"
#ifndef NS_ONLY_IMAGE_ACQUISITION
#include "ns_spatial_avg.h"
#include "ns_time_path_solver_parameters.h"
#endif
#endif
#include "ns_image_storage_handler.h"
#include "ns_os_signal_handler.h"
#include "ns_ini.h"
#include "ns_dir.h"

#include <iostream>
using namespace std;

#include <time.h>
#include "ns_image_server_stock_quotes.h"
#include "ns_capture_schedule.h"
//#include "ns_processing_job_push_scheduler.h"

#include "zlib.h"
#include "ns_image_server_automated_job_scheduler.h"
//#include "ns_capture_device_manager.h"
#include "image_server_db_schema.sql.h"

void ns_image_server_global_debug_handler(const ns_text_stream_t & t);

ns_image_server::ns_image_server() : exit_has_been_requested(false), exit_happening_now(false), handling_exit_request(false), ready_to_exit(true), update_software(false),
sql_lock("ns_is::sql"), server_event_lock("ns_is::server_event"), performance_stats_lock("ns_pfl"), simulator_scan_lock("ns_is::sim_scan"), local_buffer_sql_lock("ns_is::lb"), processing_run_counter_lock("ns_pcl"),
_act_as_processing_node(true), exit_lock("ns_is::el"), cleared(false), do_not_run_multithreaded_jobs(false), remember_barcodes_across_sessions_(true),
#ifndef NS_ONLY_IMAGE_ACQUISITION
image_registration_profile_cache(1024 * 4), //allocate 4 gigabytes of disk space in which to store reference images for capture sample registration
storyboard_cache(0), worm_detection_model_cache(0), posture_analysis_model_cache(0), survival_data_cache(0),
#endif
_verbose_debug_output(false), _cache_subdirectory("cache"), sql_database_choice(possible_sql_databases.end()), next_scan_for_problems_time(0),
_terminal_window_scale_factor(1), _system_parallel_process_id(0), _allow_multiple_processes_per_system(false), sql_table_lock_manager(this),
alert_handler_lock("ahl"), max_external_thread_id(1), currently_experiencing_a_disk_storage_emergency(false), verbose_disk_storage_reporting(false), last_verbose_disk_storage_reporting_time(0) { 
	ns_socket::global_init();
	#ifndef NS_ONLY_IMAGE_ACQUISITION
	ns_worm_detection_constants::init();
	#endif
	ns_set_global_debug_output_handler(ns_image_server_global_debug_handler);
	_software_version_compile = 3;
	image_storage.cache.set_memory_allocation_limit_in_kb(maximum_image_cache_memory_size());
	system_host_name = ns_get_system_hostname();
	//by default, run indefinately
	set_image_processing_run_limits(0, 0);
	reset_empty_job_queue_check_count();
}
bool ns_image_server::scan_for_problems_now(){
	unsigned long current_time = ns_current_time();
	if (next_scan_for_problems_time > current_time){
		return false;
	}
	if (next_scan_for_problems_time + 2*mean_problem_scan_interval_in_seconds() < current_time)
		next_scan_for_problems_time = current_time;

	next_scan_for_problems_time = rand()%(2*mean_problem_scan_interval_in_seconds())+next_scan_for_problems_time;
	return true;
}
void ns_image_server::update_scan_delays_from_db(ns_image_server_sql * con){
	this->_maximum_allowed_local_scan_delay = atol(get_cluster_constant_value("maximum_local_scan_delay_in_minutes","4",con).c_str());
	this->_maximum_allowed_remote_scan_delay = atol(get_cluster_constant_value("maximum_remote_scan_delay_in_minutes","10",con).c_str());
}

void ns_image_server::toggle_central_mysql_server_connection_error_simulation()const{
	if (ns_sql_connection::unreachable_hostname().empty())
		ns_sql_connection::simulate_unreachable_mysql_server(sql_server_addresses[0],true);
	else ns_sql_connection::simulate_unreachable_mysql_server(sql_server_addresses[0],false);
}

void ns_image_server::toggle_long_term_storage_server_connection_error_simulation() const {
	image_storage.simulate_long_term_storage_errors = !image_storage.simulate_long_term_storage_errors;
}

std::string  ns_image_server::video_compilation_parameters(const std::string & input_file, const std::string & output_file, const unsigned long number_of_frames, const std::string & fps, ns_sql & sql)const{
	//for x264
	string p(get_cluster_constant_value("video_compilation_parameters","--crf=10",&sql));
	unsigned long distance_between_keyframes(250);
	if (4*number_of_frames <= distance_between_keyframes)
		distance_between_keyframes = (number_of_frames-1)/4;


	return p + " -I " + ns_to_string(distance_between_keyframes) + " --fps " + fps + " \"" + input_file + "\" -o \"" + output_file + "\"";
}

bool ns_image_server::register_and_run_simulated_devices(ns_image_server_sql * sql) {
	string handle_simulated_devices_str(get_cluster_constant_value("run_simulated_devices","",sql));
	if (handle_simulated_devices_str == ""){
		set_cluster_constant_value("run_simulated_devices","0",sql);
		return false;
	}
	if (handle_simulated_devices_str == "1" || handle_simulated_devices_str == "yes")
		return true;
	return false;
};

void ns_output_logged_info(const std::string & info, ns_sql * sql){
  image_server_const.add_subtext_to_current_event(info,sql);
}

#ifndef NS_MINIMAL_SERVER_BUILD
void ns_image_server::start_autoscans_for_device(const std::string & device_name, ns_sql & sql){

	sql<< "SELECT id, scan_interval FROM autoscan_schedule WHERE device_name = '" << device_name
						  << "' AND autoscan_start_time <= " << ns_current_time()
						  << " AND autoscan_completed_time = 0 ORDER BY autoscan_start_time ASC";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() > 1)
		image_server.register_server_event(ns_image_server_event("Multiple autoscan schedules were specified for device ") << device_name,&sql);

	for (string::size_type i = 0; i < res.size(); i++){
		const unsigned long interval(atol(res[i][1].c_str()));
		image_server.device_manager.set_autoscan_interval(device_name,interval);
		image_server.register_server_event(ns_image_server_event("Starting scheduled autoscans on device ") << device_name << " with a period of " << res[i][1] << " seconds",&sql);
		sql << "DELETE FROM autoscan_schedule WHERE device_name = '" << device_name << "'";
		sql.send_query();
		sql.send_query("COMMIT");
	}
}
#endif
struct ns_image_data_disk_size{
	ns_image_data_disk_size():unprocessed_captured_images(0),
				processed_captured_images(0),
			  unprocessed_region_images(0),
			  processed_region_images(0),
			  metadata(0),
			  video(0){}

	double unprocessed_captured_images,
				processed_captured_images,
			  unprocessed_region_images,
			  processed_region_images,
			  metadata,
			  video;
	void update_experiment_metadata(ns_64_bit experiment_id,ns_sql & sql){
		sql << "UPDATE experiments SET "
				"size_unprocessed_captured_images=" << (unsigned long)unprocessed_captured_images << ","
				"size_processed_captured_images=" << (unsigned long)processed_captured_images << ","
				"size_unprocessed_region_images=" << (unsigned long)unprocessed_region_images << ","
				"size_processed_region_images=" << (unsigned long)processed_region_images << ","
				"size_metadata=" << (unsigned long)metadata << ","
				"size_video=" << (unsigned long)video << ","
				"size_calculation_time='" << ns_current_time() <<
				"' WHERE id = " << experiment_id;
		sql.send_query();

	}
	void update_sample_metadata(ns_64_bit sample_id,ns_sql & sql){
		sql << "UPDATE capture_samples SET "
				"size_unprocessed_captured_images=" << (unsigned long)unprocessed_captured_images << ","
				"size_processed_captured_images=" << (unsigned long)processed_captured_images << ","
				"size_unprocessed_region_images=" << (unsigned long)unprocessed_region_images << ","
				"size_processed_region_images=" << (unsigned long)processed_region_images << ","
				"size_metadata=" << (unsigned long)metadata << ","
				"size_calculation_time='" << ns_current_time() <<
				"' WHERE id = " << sample_id;
		sql.send_query();
	}
};
ns_image_data_disk_size operator+(const ns_image_data_disk_size & a, const ns_image_data_disk_size & b){
	ns_image_data_disk_size ret(a);
	ret.metadata+=b.metadata;
	ret.video+=b.video;
	ret.processed_captured_images+=b.processed_captured_images;
	ret.processed_region_images+=b.processed_region_images;
	ret.unprocessed_captured_images+=b.unprocessed_captured_images;
	ret.unprocessed_region_images+=b.unprocessed_region_images;
	return ret;
}


void ns_image_server::calculate_experiment_disk_usage(const ns_64_bit experiment_id,ns_sql & sql) const{
	sql << "SELECT name FROM experiments WHERE id = " << experiment_id;
	ns_sql_result experiment;
	sql.get_rows(experiment);
	if(experiment.size() == 0)
		throw ns_ex("ns_image_server::calculate_experiment_disk_usage()::Could not find experiment ") << experiment_id << " in the database";
	image_server.register_server_event(ns_image_server_event("ns_image_server::calculate_experiment_disk_usage()::Calculating disk usage for experiment ") << experiment[0][0] << "(" << experiment_id << ")",&sql);


	sql << "SELECT id, name FROM capture_samples WHERE experiment_id = " << experiment_id << " ORDER BY name ASC";
	ns_sql_result samples;
	sql.get_rows(samples);

	ns_image_data_disk_size full_experiment;
	full_experiment.video = image_storage.get_experiment_video_size_on_disk(experiment_id,sql);

	for (unsigned int i = 0; i < samples.size(); i++){
		ns_64_bit sample_id(ns_atoi64(samples[i][0].c_str()));
		ns_image_data_disk_size s;
		s.unprocessed_captured_images= image_storage.get_sample_images_size_on_disk(sample_id,ns_unprocessed,sql);
			//grab any old-style images stored in the base directory of the region
		s.unprocessed_captured_images+=image_storage.get_sample_images_size_on_disk(sample_id,ns_process_last_task_marker,sql);

		s.processed_captured_images=image_storage.get_sample_images_size_on_disk(sample_id,ns_process_thumbnail,sql);

		//cerr << samples[i][1] << "::" << s.unprocessed_captured_images + s.processed_captured_images << " ";
		sql << "SELECT DISTINCT id, name FROM sample_region_image_info WHERE sample_id = " << sample_id << " ORDER BY name ASC";
		ns_sql_result regions;
		sql.get_rows(regions);
		string last_name;

		//heat map directory holds data for the entire sample so we just add it in once
		if (regions.size() != 0){
			ns_64_bit region_id(ns_atoi64(regions[0][0].c_str()));
			s.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_heat_map,sql);
			s.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_static_mask,sql);
		}
		image_server.add_subtext_to_current_event(ns_to_string((100*i)/samples.size())+ "%...",&sql);
		for (unsigned int j = 0; j < regions.size(); j++){
			//don't redo duplicate regions
			if (regions[j][1] == last_name)
				continue;
			last_name = regions[j][1];
		//	cerr << "["<<samples[i][1] << "::" << regions[j][1] << ":";
			ns_64_bit region_id(ns_atoi64(regions[j][0].c_str()));
			ns_image_data_disk_size r;
			r.unprocessed_region_images = image_storage.get_region_images_size_on_disk(region_id,ns_unprocessed,sql);
			//grab any old-style images stored in the base directory of the region
			r.unprocessed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_last_task_marker,sql);

			r.metadata = image_storage.get_region_metadata_size_on_disk(region_id,sql);

			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_spatial,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_lossy_stretch,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_threshold,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_worm_detection,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_worm_detection_labels,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_region_vis,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_interpolated_vis,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_movement_coloring,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_movement_mapping,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_movement_coloring_with_survival,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_movement_paths_visualization,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_movement_paths_visualition_with_mortality_overlay,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_movement_posture_visualization,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id, ns_process_movement_plate_and_individual_visualization,sql);
			r.processed_region_images += image_storage.get_region_images_size_on_disk(region_id,ns_process_unprocessed_backup,sql);
			s = s + r;
			//cerr << r.unprocessed_region_images + r.processed_region_images + r.metadata << "] ";
		}
	//	cerr << "\n";
		s.update_sample_metadata(sample_id,sql);
		full_experiment = full_experiment + s;
	}
	full_experiment.update_experiment_metadata(experiment_id,sql);
}


#ifndef NS_MINIMAL_SERVER_BUILD
#ifndef NS_ONLY_IMAGE_ACQUISITION
void ns_get_automated_job_scheduler_lock_for_scope::release() {
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
	release_lock(sql());
	sql.release();
}

void ns_get_automated_job_scheduler_lock_for_scope::release_lock(ns_sql& sql) {
	if (!lock_held) return;
	//ns_sql_full_table_lock lock(sql,"automated_job_scheduling_data");
	ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("automated_job_scheduling_data", &sql, true, __FILE__, __LINE__));

	sql << "SELECT currently_running_host_id FROM automated_job_scheduling_data";
	ns_sql_result res;
	sql.get_rows(res);

	if (res.size() == 0) {
		lock.release(__FILE__, __LINE__);
		image_server.register_server_event(ns_image_server_event("This client's lock on the automation process was deleted!"), &sql);
		return;
	}

	if (ns_atoi64(res[0][0].c_str()) != image_server.host_id()) {
		lock.release(__FILE__, __LINE__);
		image_server.register_server_event(ns_image_server_event("This client's lock on the automation process was usurped!"), &sql);
		return;
	}

	sql << "UPDATE automated_job_scheduling_data SET currently_running_host_id = 0";
	sql.send_query();
	lock.release(__FILE__, __LINE__);
	lock_held = false;
}
void ns_get_automated_job_scheduler_lock_for_scope::wait_for_lock(const unsigned long second_delay_until_next_run, ns_sql& sql, bool ignore_time_limits) {
	if (lock_held)
		throw ns_ex("Attempting to take a automation lock that is already held!");
	//unsigned long counter(0);
	//while (true){
	ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("automated_job_scheduling_data", &sql, true, __FILE__, __LINE__));

	sql << "SELECT currently_running_host_id, acquisition_time, next_run_time,UNIX_TIMESTAMP() FROM automated_job_scheduling_data";
	ns_sql_result res;
	sql.get_rows(res);

	if (res.size() == 0) {
		sql << "INSERT INTO automated_job_scheduling_data SET currently_running_host_id = 0";
		sql.send_query();
		get_ownership(second_delay_until_next_run, sql);
		lock.release(__FILE__, __LINE__);
		return;
	}
	if (res.size() > 1)
		throw ns_ex("ns_get_automated_job_scheduler_lock_for_scope::wait_for_lock()::Multiple entries for lock : ") << res.size();
	const ns_64_bit current_running_id(ns_atoi64(res[0][0].c_str())),
		acquisition_time(atol(res[0][1].c_str())),
		next_run_time(atol(res[0][2].c_str())),
		cur_time(atol(res[0][3].c_str()));
	//if no event has been registered as having been run, prepare to run it immediately
	if (next_run_time == 0) {
		get_ownership(second_delay_until_next_run, sql);
		lock.release(__FILE__, __LINE__);
		return;
	}
	if (current_running_id != 0 && cur_time - acquisition_time >= image_server.automated_job_timeout_in_seconds()) {
		get_ownership(second_delay_until_next_run, sql);
		lock.release(__FILE__, __LINE__);
		image_server.register_server_event(ns_image_server_event("ns_get_automated_job_scheduler_lock_for_scope()::Claiming abandoned lock."), &sql);
		return;
	}
	if (ignore_time_limits || cur_time > next_run_time) {
		get_ownership(second_delay_until_next_run, sql);
		lock.release(__FILE__, __LINE__);
		return;
	}
	lock.release(__FILE__, __LINE__);

	//	ns_thread::sleep(2);
	//	counter++;
	//	if (counter == 30)
	//		throw ns_ex("Giving up waiting for automation lock!");
	//}
}
ns_64_bit ns_get_automated_job_scheduler_lock_for_scope::id() {
	ns_64_bit id = image_server.host_id();
	if (id == 0) id = 666;
	return id;
}
void ns_get_automated_job_scheduler_lock_for_scope::get_ownership(const unsigned long second_delay_until_next_run, ns_sql& sql) {
	sql << "SELECT UNIX_TIMESTAMP()";
	current_time = sql.get_integer_value();
	next_run_time = current_time + second_delay_until_next_run;

	sql << "UPDATE automated_job_scheduling_data SET currently_running_host_id = " << id()
		<< ", acquisition_time = " << current_time
		<< ", next_run_time = " << next_run_time;

	sql.send_query();
	lock_held = true;
}

void ns_image_server_automated_job_scheduler::scan_for_tasks(ns_sql & sql, bool ignore_timers_and_run_now) {
	unsigned long timer_interval = image_server.automated_job_interval();
	if (ignore_timers_and_run_now)
		timer_interval = 0;
	ns_get_automated_job_scheduler_lock_for_scope lock(timer_interval,sql);
	if (!lock.run_requested()){
		lock.release(sql);
		return;
	}
	image_server.register_server_event(ns_image_server_event("Running automated job scheduler"),&sql);

	unsigned long start_time(ns_current_time());
	image_server.update_posture_analysis_model_registry(sql,false);
	image_server.update_worm_detection_model_registry(sql, false);
	calculate_capture_schedule_boundaries(sql);
	identify_experiments_needing_captured_image_protection(sql);
	handle_when_completed_priority_jobs(sql);
	//identify_regions_needing_static_mask(sql);
	lock.release(sql);
}
void ns_image_server_automated_job_scheduler::handle_when_completed_priority_jobs(ns_sql & sql){
	sql << ns_processing_job::provide_query_stub() << " FROM processing_jobs  WHERE pending_another_jobs_completion = 1 AND processed_by_push_scheduler > 0";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() > 0){
		ns_image_server_push_job_scheduler push_job_scheduler;
		for (unsigned long i = 0;  i < res.size(); i++){
			ns_processing_job job;
			job.load_from_result(res[i]);
			push_job_scheduler.try_to_process_a_job_pending_anothers_completion(job,sql);
		}
	}
}
bool ns_image_server_automated_job_scheduler::identify_experiments_needing_captured_image_protection(ns_sql & sql,const ns_64_bit specific_sample_id){
	const long number_of_images_to_protect(5);
	std::vector<ns_64_bit> sample_ids;
	ns_sql_result samples;
	if (specific_sample_id == 0){
		sql << "SELECT s.id,s.name, e.name FROM capture_samples as s, experiments as e WHERE e.id = s.experiment_id "
				"AND s.first_frames_are_protected=0 AND e.hidden = 0";
		sql.get_rows(samples);
		sample_ids.resize(samples.size());
		for (unsigned int i = 0; i < samples.size(); i++){
			sample_ids[i] = ns_atoi64(samples[i][0].c_str());
		}
	}else{
		sample_ids.push_back(specific_sample_id);
		sql << "SELECT s.id,s.name, e.name FROM capture_samples as s, experiments as e WHERE s.id = " << specific_sample_id << " AND e.id = s.experiment_id ";
		sql.get_rows(samples);
		if (samples.size() == 0)
			throw ns_ex("Could not find sample ") << specific_sample_id << " in the db";
	}
	bool made_a_change = false;
	for (unsigned int i = 0; i < sample_ids.size(); i++){
		const ns_64_bit sample_id(sample_ids[i]);
		sql << "SELECT id, never_delete_image FROM captured_images WHERE image_id != 0 AND problem = 0 "
			    "AND currently_being_processed = 0 AND sample_id = " << sample_id
			<< " ORDER BY capture_time ASC LIMIT " << number_of_images_to_protect;
		ns_sql_result images;
		sql.get_rows(images);
		if (images.size() > number_of_images_to_protect)
			throw ns_ex("ns_image_server::perform_experiment_maintenance()::mysql returned too many rows!");
		for (unsigned int j = 0; j < images.size(); j++){
			if (images[j][1] == "1") continue;
			sql << "UPDATE captured_images SET never_delete_image = 1 WHERE id = " << images[j][0];
			sql.send_query();
			made_a_change = true;
		}
		if (images.size() == number_of_images_to_protect){

			sql.send_query("COMMIT");
			sql << "UPDATE capture_samples SET first_frames_are_protected=1 WHERE id = " << sample_id;
			sql.send_query();
			image_server.register_server_event(ns_image_server_event("The first frames of sample ") << samples[i][2] << "::" << samples[i][1] << " are now protected.",&sql);
		}
	}
	return made_a_change;
}


void ns_image_server_automated_job_scheduler::calculate_capture_schedule_boundaries(ns_sql & sql){

	image_server.register_server_event(ns_image_server_event("Calculating Experiment Boundaries"),&sql);

	sql << "SELECT experiment_id,count(*),min(scheduled_time),"
		  "max(scheduled_time) FROM capture_schedule WHERE censored=0 GROUP BY experiment_id";
	ns_sql_result res;
	sql.get_rows(res);
	for (unsigned int i = 0; i < res.size(); i++){
		sql << "UPDATE experiments SET num_time_points=" << res[i][1] << ", first_time_point='" << res[i][2] << "', last_time_point='" << res[i][3]
			<< "' WHERE id = " << res[i][0];
		sql.send_query();
	}
}


void ns_image_server_automated_job_scheduler::identify_regions_needing_static_mask(ns_sql & sql){
	const long number_of_frames_needed_for_static_mask(2*12*4); //four days worth of data

	sql <<	"SELECT r.id,r.name,s.name, e.name FROM sample_region_image_info as r, capture_samples as s, experiments as e "
			" WHERE e.id = s.experiment_id AND r.sample_id = s.id "
			" AND r.analysis_scheduling_state=" << (int)ns_waiting_to_begin_static_mask << " AND e.hidden = 0 AND e.run_automated_job_scheduling != 0";
	ns_sql_result regions;
	sql.get_rows(regions);
	bool jobs_submitted(false);
	ns_sql_result region_images;
	bool announced(false);
	for (unsigned int i = 0; i < regions.size(); i++){
		//check to see if the region has enough of the first few images with median filters specified
		sql << "SELECT " << ns_processing_step_db_column_name(ns_process_spatial)  << " FROM sample_region_images WHERE region_info_id = " << regions[i][0] << " AND "
			<< " currently_under_processing=0 AND problem = 0 ORDER BY capture_time ASC LIMIT " << (3*number_of_frames_needed_for_static_mask/2);
		sql.get_rows(region_images);
		unsigned long count(0);
		for (unsigned int j = 0; j < region_images.size(); ++j){
			if (region_images[j][0] != "0")
				count++;
		}

		if (count < number_of_frames_needed_for_static_mask)
			continue;
		if (!announced){
			image_server.register_server_event(ns_image_server_event("Starting static image mask jobs."),&sql);
			announced = true;
		}
		ns_processing_job job;
		job.region_id = ns_atoi64(regions[i][0].c_str());
		job.time_submitted = ns_current_time();
		job.operations[(int)ns_process_heat_map] = true;
		job.operations[(int)ns_process_static_mask] = true;
		job.save_to_db(sql);
		jobs_submitted= true;
		sql << "UPDATE sample_region_image_info SET analysis_scheduling_state = " << (int)ns_waiting_for_static_mask_completion << " WHERE id = " << regions[i][0];
		sql.send_query();
	}
	if (jobs_submitted)
		ns_image_server_push_job_scheduler::request_job_queue_discovery(sql);
}
void ns_image_server_automated_job_scheduler::register_static_mask_completion(const ns_64_bit region_id, ns_sql & sql){
	sql << "UPDATE sample_region_image_info SET analysis_scheduling_state = " << (int)ns_worm_detection << " WHERE id = " << region_id;
	sql.send_query();
	schedule_detection_jobs_for_region(region_id,sql);
	ns_image_server_push_job_scheduler::request_job_queue_discovery(sql);
}
void ns_image_server_automated_job_scheduler::schedule_detection_jobs_for_region(const ns_64_bit region_id,ns_sql & sql){
	ns_processing_job job;
	job.region_id = region_id;
	job.time_submitted = ns_current_time();
	job.operations[(int)ns_process_worm_detection] = true;
	job.operations[(int)ns_process_worm_detection_labels] = true;
	job.save_to_db(sql);
}



void ns_image_server::perform_experiment_maintenance(ns_sql & sql) const{
	automated_job_scheduler.scan_for_tasks(sql,false);
}
#endif
#endif
void ns_image_server::register_alerts_as_handled(){
	alert_handler_thread.report_as_finished();
}
#ifndef NS_MINIMAL_SERVER_BUILD
 void ns_image_server::submit_capture_schedule_specification(ns_experiment_capture_specification & spec, std::vector < std::string > & warnings, ns_sql & sql,const ns_experiment_capture_specification::ns_handle_existing_experiment & existing_experiment ,const bool submit_to_db,const std::string & summary_output_file,const bool output_to_stdout){

	warnings.resize(0);


	string summary(spec.submit_schedule_to_db(warnings,sql,submit_to_db, existing_experiment));
	if (output_to_stdout && !submit_to_db){
		cout << summary;
	}
	else if (summary_output_file.size() > 0 && !submit_to_db){
		ofstream o(summary_output_file.c_str());
		if (o.fail())
			throw ns_ex("Could not open ") << summary_output_file << " for output.";
		o << summary;
	}
}
#endif
ofstream * ns_image_server::write_device_configuration_file() const{
	std::string filename = volatile_storage_directory + DIR_CHAR_STR + "device_information";
	ns_dir::create_directory_recursive(filename);
	filename += DIR_CHAR_STR;
	filename += "last_device_configuration.txt";
	ofstream * out(new ofstream(filename.c_str()));
	if (out->fail())
		throw ns_ex("ns_image_server::Could not save last known device config ") << filename;
	return out;
}
ifstream * ns_image_server::read_device_configuration_file() const{
	std::string filename = volatile_storage_directory + DIR_CHAR_STR + "device_information";
	ns_dir::create_directory_recursive(filename);
	filename += DIR_CHAR_STR;
	filename += "last_device_configuration.txt";
	return new ifstream(filename.c_str());
}

string ns_image_server::get_cluster_constant_value(const string & key, const string & default_value, ns_image_server_sql * sql)const{
	unsigned long value(0);
	ns_sql_result res;
	*sql << "SELECT v FROM "<< sql->table_prefix() << "constants WHERE k = '" << key << "'";
	sql->get_rows(res);
	if (res.size() != 1){
		if (res.size() > 1){
			*sql << "DELETE FROM "<< sql->table_prefix() << "constants WHERE k = '" << key << "'";
			sql->send_query();
		}
		*sql << "INSERT INTO "<< sql->table_prefix() << "constants SET k='" << key << "', v='" << sql->escape_string(default_value) << "'";
		sql->send_query();
		return default_value;
	}
	return res[0][0].c_str();

}

void ns_image_server::set_cluster_constant_value(const string & key, const string & value, ns_image_server_sql * sql, const int time_stamp){
	if (get_cluster_constant_value(key,value,sql) == value)
		return;
	*sql << "UPDATE "<< sql->table_prefix() << "constants SET v='" << value << "' ";
	if (time_stamp != -1)
		*sql << ",time_stamp=FROM_UNIXTIME(" << time_stamp << ") ";
	*sql << "WHERE k='" << sql->escape_string(key) << "'";
	sql->send_query();
}

void add_string_or_number(std::string & destination, const std::string & desired_string, const std::string & fallback_prefix, const ns_64_bit fallback_int){
	if (desired_string.size() == 0){
		destination+=fallback_prefix;
		destination+=ns_to_string(fallback_int);
	}
	else destination+=desired_string;
}
#ifndef NS_ONLY_IMAGE_ACQUISITION
ns_64_bit ns_image_server::make_record_for_new_sample_mask(const ns_64_bit sample_id, ns_sql & sql){
	sql << "SELECT name, experiment_id FROM capture_samples WHERE id = " << sample_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_server::make_record_for_new_sample_mask()::Could not load sample with id") << sample_id;
	std::string sample_name = res[0][0],
		   experiment_id = res[0][1];
	sql << "SELECT name, id FROM experiments WHERE id=" << experiment_id;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_server::make_record_for_new_sample_mask()::Could not load experiment with id") << experiment_id;
	std::string experiment_name = res[0][0];

	std::string path;
	add_string_or_number(path,experiment_name,"experiment_", ns_atoi64(experiment_id.c_str()));
	path += DIR_CHAR_STR;
	path += "region_masks";
	std::string filename = "mask_";
	filename += sample_name + ".tif";
	std::string partition = image_server.image_storage.get_partition_for_experiment(ns_atoi64(experiment_id.c_str()), &sql);
	sql << "INSERT INTO images SET host_id = " << host_id() << ", creation_time=" << ns_current_time() << ", currently_under_processing=0, "
		<< "path = '" << sql.escape_string(path) << "', filename='" << sql.escape_string(filename) << "', `partition`='" << sql.escape_string(partition) << "'";
//	cerr << sql.query() << "\n";
	ns_64_bit id = sql.send_query_get_id();
	sql.send_query("COMMIT");

	return id;

}
#endif
void ns_image_server::check_for_sql_database_access(ns_image_server_sql * sql) const {
	sql->select_db(*sql_database_choice);
}
void ns_image_server::check_for_local_sql_database_access(ns_local_buffer_connection * sql) const {
	sql->select_db(local_buffer_db);
}
void ns_image_server::reconnect_sql_connection(ns_sql * sql){
	try{
		sql->disconnect();
	}
	catch(...){}

	ns_acquire_lock_for_scope lock(sql_lock,__FILE__,__LINE__);

	//Look through all possible paths to the server until one is found to work.
	if (sql_server_addresses.empty())
		throw ns_ex("ns_image_server::reconnect_sql_connection()::No addresses are specified for the sql server");
	unsigned long start_address_id = 0;
	ns_ex er;
	for (unsigned int i = 0; i < sql_server_addresses.size(); ++i){
		const unsigned int j = (i+start_address_id)%sql_server_addresses.size();
		try{
			sql->connect(sql_server_addresses[j],sql_user,sql_pwd,0);
			er.clear_text();
			break;
		}
		catch(ns_ex & ex){
			er = ex;

		}
	}
	if (!er.text().empty()){
		lock.release();
		throw er;
	}

	lock.release();
	sql->select_db(*sql_database_choice);
}

ns_sql * ns_image_server::new_sql_connection_no_lock_or_retry(const std::string & source_file, const unsigned int source_line) const{
	ns_sql *con(0);
	con = new ns_sql();
	//con->local_locking_behavior = ns_thread_locking;
	try{

		for (std::vector<string>::size_type server_i=0;  server_i < sql_server_addresses.size(); server_i++){
				//cycle through different possible connections
				try{
					con->connect(sql_server_addresses[server_i],sql_user,sql_pwd,0);
					con->select_db(*sql_database_choice);
					return con;
				}
				catch(...){}
		}
	}
	catch(...){
		delete con;
		throw;
	}
	return 0;
}


ns_local_buffer_connection * ns_image_server::new_local_buffer_connection(const std::string & source_file, const unsigned int source_line, const bool select_default_database){
	ns_acquire_lock_for_scope lock(local_buffer_sql_lock,__FILE__,__LINE__);
	ns_local_buffer_connection * buf;
	try{
	  buf = (new_local_buffer_connection_no_lock_or_retry(source_file,source_line, select_default_database));
	}
	catch(ns_ex & ex){
	  ns_ex ex2("Problem connecting to local buffer:");
	  ex2 << ex.text() << ns_sql_fatal;
	  throw ex2;
	}
	lock.release();
	return buf;
}
void ns_add_version_compatibility_columns(const std::string& table_name, ns_sql& sql) {
	if (table_name == "sample_region_image_info") {
		sql << "ALTER TABLE `sample_region_image_info` "
			"ADD COLUMN `temporal_interpolation_performed` INT UNSIGNED NOT NULL DEFAULT '0' AFTER `op30_image_id`,"
			"ADD COLUMN `movement_file_triplet_id` INT NULL AFTER `temporal_interpolation_performed`,"
			"ADD COLUMN `movement_file_triplet_interpolated_id` INT NULL AFTER `movement_file_triplet_id`,"
			"ADD COLUMN `movement_file_time_path_id` INT NULL AFTER `movement_file_triplet_interpolated_id`,"
			"ADD COLUMN `movement_file_time_path_image_id` INT NULL AFTER `movement_file_time_path_id`";
		sql.send_query();
	}
}
void ns_remove_version_compatibility_columns(const std::string& table_name, ns_sql& sql) {
	if (table_name == "sample_region_image_info") {
		sql << "ALTER TABLE `sample_region_image_info` "
			"DROP COLUMN `temporal_interpolation_performed`,"
			"DROP COLUMN `movement_file_triplet_id`,"
			"DROP COLUMN `movement_file_triplet_interpolated_id`,"
			"DROP COLUMN `movement_file_time_path_id`,"
			"DROP COLUMN `movement_file_time_path_image_id`";
		sql.send_query();
	}
}

void ns_get_table_create(const std::string & table_name, std::string & result, ns_sql & sql){
	sql << "SHOW CREATE TABLE `" << table_name << "`";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_get_table_create::Could not find ") << table_name << " in db!";
	result = res[0][1];
}

void ns_add_buffer_suffix_to_table_create(const std::string & table_name, std::string & r){
	std::string::size_type p = r.find(table_name);
	if (p == r.npos)
		throw ns_ex("ns_add_buffer_suffix_to_table_create::Could not find table name in create spec: ") << table_name;
	r.insert(p,"buffered_");
}
void ns_image_server::set_up_local_buffer(){
	ns_acquire_for_scope<ns_local_buffer_connection> buf(new ns_local_buffer_connection);
	try{
		buf().connect(local_buffer_ip,local_buffer_user,local_buffer_pwd,0);
		buf().select_db(local_buffer_db);
	}
	catch(ns_ex & ex){
			throw ns_ex("set_up_local_buffer()::Could not connect to local buffer.  A local database must be running at ")<<
				local_buffer_ip << " with username \"" << local_buffer_user << "\" granted access to the schema \"" <<
				local_buffer_db << "\". These credentials can be set in the image server configuration file."
				<< "  The error was: " << ex.text();;
	}
	const unsigned long number_of_tables(7);
	const std::string table_names[number_of_tables] = {"capture_schedule",
								  "captured_images",
								  "images",
								  "experiments",
								  "capture_samples",
								  "constants",
								  "host_event_log"};

	std::vector<char> table_found_in_buffer(number_of_tables,0);

	buf() << "SHOW TABLES";
	ns_sql_result buffer_tables;
	buf().get_rows(buffer_tables);
	for (unsigned int i = 0; i < buffer_tables.size(); i++){
		for (unsigned int j = 0; j < number_of_tables; j++){
			if (table_found_in_buffer[j]) continue;
			if (buffer_tables[i][0] == std::string("buffered_") + table_names[j])
				table_found_in_buffer[j] = true;
		}
	}
	if (buffer_tables.size() != 0){
		for (unsigned int i = 0; i < number_of_tables; i++){
			if (!table_found_in_buffer[i]){
				throw ns_ex("ns_image_server::set_up_local_buffer()::An incomplete local buffer schema is present.  Table buffered_")
					<< table_names[i] << " was not found.  Drop all tables to allow an automatic rebuild of the buffer schema.";
			}
		}
		std::cerr << "Local db looks great.\n";
		//if everything is present, we're done!
		return;
	}

	ns_acquire_for_scope<ns_sql> sql(new_sql_connection(__FILE__,__LINE__));


	for (unsigned int i = 0; i < number_of_tables; i++){
		string create;
		ns_get_table_create(table_names[i],create,sql());
		ns_add_buffer_suffix_to_table_create(table_names[i],create);
		buf().send_query(create);
	}

	#ifndef NS_MINIMAL_SERVER_BUILD
	ns_buffered_capture_scheduler::store_last_update_time_in_db(ns_synchronized_time(ns_buffered_capture_scheduler::ns_default_update_time,ns_buffered_capture_scheduler::ns_default_update_time),buf());
	#endif
}
ns_local_buffer_connection * ns_image_server::new_local_buffer_connection_no_lock_or_retry(const std::string & source_file, const unsigned int source_line, const bool select_default_database) const{
	ns_local_buffer_connection * buf(new ns_local_buffer_connection);
	try{
		buf->connect(local_buffer_ip,local_buffer_user,local_buffer_pwd,0);
		if (select_default_database)
			buf->select_db(local_buffer_db);
	}
	catch(...){

		delete buf;
		throw;
	}
	return buf;
}


void ns_image_server::request_database_from_db_and_switch_to_it(ns_sql & sql, bool update_hosts_records_in_db){
	sql << "SELECT database_used FROM hosts WHERE id = " << host_id() << "";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		return;
	//nothing needs doing if we are already at the correct database.
	ns_acquire_lock_for_scope lock(sql_lock,__FILE__, __LINE__);
	if (*sql_database_choice == res[0][0]) {
		lock.release();
		return;
	}
	lock.release();


	try{
		set_sql_database(res[0][0], update_hosts_records_in_db,&sql);
	}
	catch(ns_ex & ex){
		register_server_event(ex,&sql);
	}
	return;
};

void ns_image_server::switch_to_default_db() {
	if (possible_sql_databases.empty())
		throw ns_ex("ns_image_server::set_sql_database()::No possible databases specified in ini file!");
	sql_database_choice = possible_sql_databases.begin();
}
void ns_image_server::set_sql_database(const std::string & database_name,const bool update_hosts_records_in_db, ns_image_server_sql * sql){
	if (possible_sql_databases.size() == 0)
		throw ns_ex("ns_image_server::set_sql_database()::No possible databases specified in ini file!");

	*sql << "SHOW DATABASES";
	ns_sql_result database;
	sql->get_rows(database);
	bool found(false);
	for (unsigned int i = 0; i < database.size(); i++){
		if (database_name == database[i][0]){
			found = true;
			break;
		}
	}
	if (!found){
		throw ns_ex("ns_image_server::set_sql_database()::Requested database name '") << database_name << "' could not be found";
	}

	if (update_hosts_records_in_db){
		register_server_event(ns_image_server_event("Switching to database ") << database_name,sql);
		*sql << "UPDATE hosts SET database_used='" << database_name << "' WHERE id = " << host_id();
		sql->send_query();
		unregister_host(sql);
		clear_processing_status(sql);
	}
	sql_lock.wait_to_acquire(__FILE__, __LINE__);
	std::vector<std::string>::const_iterator p = std::find(possible_sql_databases.begin(),possible_sql_databases.end(),database_name);
	if (p == possible_sql_databases.end()){
		p = possible_sql_databases.insert(possible_sql_databases.end(),database_name);
	}
	sql_database_choice = p;
	sql->select_db(database_name);
	ns_death_time_annotation_flag::get_flags_from_db(sql);
	sql_lock.release();
	if (update_hosts_records_in_db){
		register_host(sql,true,false);
		register_devices(false,sql);
	}
}

bool ns_sql_column_exists(const char * table, const char * column, ns_sql_connection * sql){
	*sql << "SHOW COLUMNS FROM " << table << " WHERE field = '" << column << "'";
	ns_sql_result res;
	sql->get_rows(res);
	return !res.empty();
}
bool ns_sql_column_exists(const std::string & table, const std::string & column,ns_sql_connection * sql){
	*sql << "SHOW COLUMNS FROM " << table << " WHERE field = '" << column << "'";
	ns_sql_result res;
	sql->get_rows(res);
	
	return !res.empty();
}
bool ns_image_server::upgrade_tables(ns_sql_connection * sql, const bool just_test_if_needed, const std::string & schema_name, const bool updating_local_buffer) {
	bool changes_made = false;
	const std::string t_suf = updating_local_buffer ? "buffered_" : "";

	sql->select_db(schema_name);

	*sql << "SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
		"WHERE table_name = '" << t_suf << "captured_images' AND COLUMN_NAME = 'registration_horizontal_offset' "
		"AND TABLE_SCHEMA = '" << schema_name << "'";
	ns_sql_result res;
	sql->get_rows(res);
	if (res.size() > 0 && res[0][0].find("unsigned") != res[0][0].npos) {
		if (just_test_if_needed)
			return true;
		cout << "Fixing " << schema_name << " registration horizontal record table\n";
		*sql << "ALTER TABLE " << schema_name << "." << t_suf << "captured_images CHANGE COLUMN `registration_horizontal_offset` "
			"`registration_horizontal_offset` INT(10) NOT NULL DEFAULT '0' ";
		sql->send_query();
		changes_made = true;
	}

	if (!updating_local_buffer) {

		if (!ns_sql_column_exists("sample_region_image_info", "position_analysis_model", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding position analysis model column to sample_region_image_info\n";
			*sql << "ALTER TABLE sample_region_image_info "
				"ADD COLUMN `position_analysis_model` TEXT NOT NULL AFTER `time_series_denoising_flag`";
			sql->send_query();

			changes_made = true;
		}

		//check to see if path_data has wrong bit depth
		*sql << "SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
			"WHERE table_name = 'path_data' AND COLUMN_NAME = 'region_id' "
			"AND TABLE_SCHEMA = '" << schema_name << "'";
		sql->get_rows(res);
		if (res.size() > 0 && res[0][0].find("bigint") == res[0][0].npos) {
			if (just_test_if_needed)
				return true;
			cout << "Fixing path id bit depths in path_data\n";
			*sql << "ALTER TABLE `path_data`"
				"CHANGE COLUMN `region_id` `region_id` BIGINT UNSIGNED NOT NULL FIRST,"
				"CHANGE COLUMN `image_id` `image_id` BIGINT UNSIGNED NOT NULL AFTER `path_id`,"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT AFTER `image_id`";
			sql->send_query();
			changes_made = true;
		}

		if (!ns_sql_column_exists("path_data", "flow_image_id", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding flow record in path data\n";
			*sql << " ALTER TABLE `path_data` ADD COLUMN `flow_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `id`";
			sql->send_query();

			changes_made = true;
		}
		if (!ns_sql_column_exists("sample_region_image_info", "op28_video_id", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Updating sample region image info table\n";
			*sql <<
				"ALTER TABLE `sample_region_image_info` "
				"CHANGE COLUMN `mask_region_id` `mask_region_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' FIRST,"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT AFTER `details`,"
				"CHANGE COLUMN `op21_image_id` `op21_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op20_image_id`,"
				"CHANGE COLUMN `op22_image_id` `op22_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op21_image_id`,"
				"CHANGE COLUMN `time_path_solution_id` `time_path_solution_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `path_movement_images_are_cached`,"
				"CHANGE COLUMN `op0_video_id` `op0_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `analysis_scheduling_state`,"
				"CHANGE COLUMN `op2_video_id` `op2_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op0_video_id`,"
				"CHANGE COLUMN `op3_video_id` `op3_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op2_video_id`,"
				"CHANGE COLUMN `op4_video_id` `op4_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op3_video_id`,"
				"CHANGE COLUMN `op5_video_id` `op5_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op4_video_id`,"
				"CHANGE COLUMN `op6_video_id` `op6_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op5_video_id`,"
				"CHANGE COLUMN `op7_video_id` `op7_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op6_video_id`,"
				"CHANGE COLUMN `op8_video_id` `op8_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op7_video_id`,"
				"CHANGE COLUMN `op17_video_id` `op17_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op8_video_id`,"
				"CHANGE COLUMN `op20_video_id` `op20_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op17_video_id`,"
				"CHANGE COLUMN `op24_video_id` `op24_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op20_video_id`,"
				"CHANGE COLUMN `op25_video_id` `op25_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op24_video_id`,"
				"CHANGE COLUMN `op26_video_id` `op26_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op25_video_id`,"
				"CHANGE COLUMN `op27_video_id` `op27_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op26_video_id`,"
				"ADD COLUMN `op28_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op27_video_id`,"
				"CHANGE COLUMN `op30_image_id` `op30_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `position_analysis_model`";
			sql->send_query();

			changes_made = true;
		}

		*sql << "SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
			"WHERE table_name = 'sample_region_images' AND COLUMN_NAME = 'id' "
			"AND TABLE_SCHEMA = '" << schema_name << "'";
		sql->get_rows(res);
		if (res.size() > 0 && res[0][0].find("bigint") == res[0][0].npos) {
			if (just_test_if_needed)
				return true;
			cout << "Updating multiple table columns to 64 bit integer keys.\n  This may take a while, up to several hours for large databases. 0%...";
			*sql <<
				"ALTER TABLE `sample_region_images`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST,"
				"CHANGE COLUMN `region_info_id` `region_info_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `id`,"
				"CHANGE COLUMN `capture_time` `capture_time` INT UNSIGNED NOT NULL DEFAULT '0' AFTER `region_info_id`,"
				"CHANGE COLUMN `image_id` `image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `capture_time`,"
				"CHANGE COLUMN `op1_image_id` `op1_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `image_id`,"
				"CHANGE COLUMN `op2_image_id` `op2_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op1_image_id`,"
				"CHANGE COLUMN `op3_image_id` `op3_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op2_image_id`,"
				"CHANGE COLUMN `last_modified` `last_modified` BIGINT UNSIGNED NULL DEFAULT NULL AFTER `op3_image_id`,"
				"CHANGE COLUMN `op4_image_id` `op4_image_id` BIGINT NOT NULL DEFAULT '0' AFTER `last_modified`,"
				"CHANGE COLUMN `op5_image_id` `op5_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op4_image_id`,"
				"CHANGE COLUMN `op6_image_id` `op6_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op5_image_id`,"
				"CHANGE COLUMN `op7_image_id` `op7_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op6_image_id`,"
				"CHANGE COLUMN `op8_image_id` `op8_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op7_image_id`,"
				"CHANGE COLUMN `op9_image_id` `op9_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op8_image_id`,"
				"CHANGE COLUMN `op10_image_id` `op10_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op9_image_id`,"
				"CHANGE COLUMN `op11_image_id` `op11_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op10_image_id`,"
				"CHANGE COLUMN `op12_image_id` `op12_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op11_image_id`,"
				"CHANGE COLUMN `op13_image_id` `op13_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op12_image_id`,"
				"CHANGE COLUMN `op14_image_id` `op14_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op13_image_id`,"
				"CHANGE COLUMN `op15_image_id` `op15_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op14_image_id`,"
				"CHANGE COLUMN `op16_image_id` `op16_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op15_image_id`,"
				"CHANGE COLUMN `op17_image_id` `op17_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op16_image_id`,"
				"CHANGE COLUMN `op18_image_id` `op18_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op17_image_id`,"
				"CHANGE COLUMN `op19_image_id` `op19_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op18_image_id`,"
				"CHANGE COLUMN `op20_image_id` `op20_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op19_image_id`,"
				"CHANGE COLUMN `op21_image_id` `op21_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op20_image_id`,"
				"CHANGE COLUMN `op22_image_id` `op22_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op21_image_id`,"
				"CHANGE COLUMN `op23_image_id` `op23_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op22_image_id`,"
				"CHANGE COLUMN `op24_image_id` `op24_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op23_image_id`,"
				"CHANGE COLUMN `op25_image_id` `op25_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op24_image_id`,"
				"CHANGE COLUMN `op26_image_id` `op26_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op25_image_id`,"
				"CHANGE COLUMN `op27_image_id` `op27_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op26_image_id`,"
				"CHANGE COLUMN `op28_image_id` `op28_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op27_image_id`,"
				"CHANGE COLUMN `op29_image_id` `op29_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op28_image_id`,"
				"CHANGE COLUMN `op30_image_id` `op30_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `op29_image_id`,"
				"CHANGE COLUMN `capture_sample_image_id` `capture_sample_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' COMMENT 'a link back to the capture sample image from which this region was cut' AFTER `op30_image_id`,"
				"CHANGE COLUMN `worm_detection_results_id` `worm_detection_results_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `problem`,"
				"CHANGE COLUMN `worm_interpolation_results_id` `worm_interpolation_results_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `worm_detection_results_id`,"
				"CHANGE COLUMN `currently_under_processing` `currently_under_processing` INT NOT NULL DEFAULT '0' AFTER `worm_interpolation_results_id`,"
				"CHANGE COLUMN `worm_movement_id` `worm_movement_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `currently_under_processing`,"
				"CHANGE COLUMN `make_training_set_image_from_frame` `make_training_set_image_from_frame` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `image_statistics_id`";
			sql->send_query();

			cout << "25%...";
			*sql << "ALTER TABLE `animal_storyboard`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST,"
				"CHANGE COLUMN `region_id` `region_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `id`,"
				"CHANGE COLUMN `sample_id` `sample_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `region_id`,"
				"CHANGE COLUMN `experiment_id` `experiment_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `sample_id`,"
				"CHANGE COLUMN `image_id` `image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `experiment_id`,"
				"CHANGE COLUMN `metadata_id` `metadata_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `image_id`";
			sql->send_query();

			*sql << "ALTER TABLE `automated_job_scheduling_data`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST";
			sql->send_query();

			*sql << "ALTER TABLE `alerts`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT AFTER `acknowledged`";
			sql->send_query();

			*sql << "ALTER TABLE `sample_region_image_aligned_path_images`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST,"
				"CHANGE COLUMN `region_info_id` `region_info_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `id`,"
				"CHANGE COLUMN `image_id` `image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `frame_index`";
			sql->send_query();

			*sql << "ALTER TABLE `autoscan_schedule`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST";
			sql->send_query();

			cout << "30%...";
			*sql << "ALTER TABLE `captured_images`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST,"
				"CHANGE COLUMN `image_id` `image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `id`,"
				"CHANGE COLUMN `experiment_id` `experiment_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `last_modified`,"
				"CHANGE COLUMN `sample_id` `sample_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `experiment_id`,"
				"CHANGE COLUMN `problem` `problem` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `mask_applied`";
			sql->send_query();

			*sql << "ALTER TABLE `capture_samples`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST,"
				"CHANGE COLUMN `experiment_id` `experiment_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `name`,"
				"CHANGE COLUMN `mask_id` `mask_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `description`,"
				"CHANGE COLUMN `problem` `problem` BIGINT NOT NULL DEFAULT '0' AFTER `device_id`,"
				"CHANGE COLUMN `op0_video_id` `op0_video_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `size_calculation_time`";
			sql->send_query();

			cout << "50%...";
			*sql << "ALTER TABLE `capture_schedule`"
				"CHANGE COLUMN `experiment_id` `experiment_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `id`,"
				"CHANGE COLUMN `sample_id` `sample_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `problem`";
			sql->send_query();

			cout << "75%...";
			*sql << "ALTER TABLE `delete_file_jobs`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST,"
				"CHANGE COLUMN `parent_job_id` `parent_job_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' COMMENT 'points to the file deletion specification job that produced this deletion job' AFTER `confirmed`";
			sql->send_query();

			*sql << "ALTER TABLE `delete_file_specifications`"
				"CHANGE COLUMN `delete_job_id` `delete_job_id` BIGINT UNSIGNED NULL DEFAULT NULL FIRST";
			sql->send_query();

			*sql << "ALTER TABLE `devices`"
				"CHANGE COLUMN `host_id` `host_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `comments`,"
				"CHANGE COLUMN `barcode_image_id` `barcode_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `preview_requested`,"
				"CHANGE COLUMN `id` `id` BIGINT NOT NULL DEFAULT '0' AFTER `error_text`";
			sql->send_query();

			*sql << "ALTER TABLE `hosts`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST";
			sql->send_query();

			*sql << "ALTER TABLE `experiments`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST";
			sql->send_query();

			*sql << "ALTER TABLE `image_masks`"
				"CHANGE COLUMN `image_id` `image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' FIRST,"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT AFTER `image_id`,"
				"CHANGE COLUMN `visualization_image_id` `visualization_image_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `processed`";
			sql->send_query();

			*sql << "ALTER TABLE `image_mask_regions`"
				"CHANGE COLUMN `mask_id` `mask_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' FIRST,"
				"CHANGE COLUMN `pixel_count` `pixel_count` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `y_max`,"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT AFTER `mask_value`";
			sql->send_query();
			cout << "90%...";

			*sql << "ALTER TABLE `image_statistics`"
				"CHANGE COLUMN `id` `id` BIGINT UNSIGNED NOT NULL AUTO_INCREMENT FIRST";
			sql->send_query();

			*sql << "ALTER TABLE `processing_jobs`"
				"CHANGE COLUMN `delete_file_job_id` `delete_file_job_id` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `subregion_stop_time`";
			sql->send_query();

			cout << "100%...Done.\n";

			changes_made = true;
		}

		*sql << "SHOW TABLES IN " << schema_name;
		sql->get_rows(res);
		bool found_model_registry_table = false;
		for (unsigned int i = 0; i < res.size(); i++) {
			if (res[i][0] == "analysis_model_registry") {
				found_model_registry_table = true;
				break;
			}
		}
		if (!found_model_registry_table) {
			if (just_test_if_needed)
				return true;
			cout << "Creating analysis model registry table\n";
			*sql << "CREATE TABLE `analysis_model_registry` ("
				"`name` TEXT NOT NULL,"
				"`version` TEXT NOT NULL,"
				"`analysis_method` VARCHAR(10) NOT NULL DEFAULT '',"
				"`analysis_step` VARCHAR(10) NOT NULL DEFAULT '',"
				"`file_time` BIGINT NULL DEFAULT '0'"
				") ENGINE = InnoDB";
			sql->send_query();
			changes_made = true;
		}
		if (res.size() > 0) {
			bool found(false);
			for (unsigned int i = 0; i < res.size(); i++) {
				if (res[i][0] == "sample_time_relationships") {
					if (just_test_if_needed)
						return true;
					if (!found) cout << "Dropping unused tables...\n";
					*sql << "DROP TABLE `sample_time_relationships`";
					sql->send_query();
					changes_made = true;
					found = true;
				}
				else if (res[i][0] == "worm_movement") {
					if (just_test_if_needed)
						return true;
					if (!found) cout << "Dropping unused tables....\n";
					*sql << "DROP TABLE `worm_movement`";
					sql->send_query();
					changes_made = true;
					found = true;

				}
				else if (res[i][0] == "processing_job_log") {
					if (just_test_if_needed)
						return true;
					if (!found) cout << "Dropping unused tables.....\n";
					*sql << "DROP TABLE `processing_job_log`";
					sql->send_query();
					changes_made = true;
					found = true;

				}
			}
		}


		*sql << "SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
			"WHERE table_name = 'annotation_flags' AND COLUMN_NAME = 'exclude' "
			"AND TABLE_SCHEMA = '" << schema_name << "'";
		sql->get_rows(res);
		if (res.size() > 0 && res[0][0].find("tiny") != res[0][0].npos) {
			if (just_test_if_needed)
				return true;
			cout << "Updating flag table...";
			*sql << "ALTER TABLE annotation_flags CHANGE COLUMN exclude exclude INT NOT NULL DEFAULT '0' AFTER label;";
			sql->send_query();
			cout << "\n";
		}
		*sql << "SELECT COLUMN_TYPE FROM INFORMATION_SCHEMA.COLUMNS "
			"WHERE table_name = 'processing_jobs' AND COLUMN_NAME = 'urgent' "
			"AND TABLE_SCHEMA = '" << schema_name << "'";
		sql->get_rows(res);		if (res.size() > 0 && res[0][0].find("tiny") != res[0][0].npos) {
			if (just_test_if_needed)
				return true;
			cout << "Updating flag table...";
			*sql << "ALTER TABLE `processing_jobs` CHANGE COLUMN urgent urgent INT NOT NULL DEFAULT '0' AFTER image_id;";
			sql->send_query();
			cout << "\n";
		}

	}

	if (!ns_sql_column_exists(t_suf + "host_event_log", "sub_text", sql)) {
		if (just_test_if_needed)
			return true;
		cout << "Adding column for additional subtext to host log \n";
		*sql << "ALTER TABLE `" << t_suf << "host_event_log` "
			"ADD COLUMN `sub_text` MEDIUMTEXT NOT NULL AFTER `processing_duration`";
		sql->send_query();

		changes_made = true;
	}



	if (!ns_sql_column_exists(t_suf + "host_event_log", "node_id", sql)) {
		if (just_test_if_needed)
			return true;
		cout << "Adding adding thread id column to host event log\n";
		*sql << "ALTER TABLE `" << t_suf << "host_event_log` "
			"ADD COLUMN `node_id` INT NOT NULL DEFAULT '0' AFTER `sub_text`";
		sql->send_query();

		changes_made = true;
	}

	if (!updating_local_buffer) {
		std::string subregion_mask_column_name(ns_processing_step_db_column_name(ns_process_subregion_label_mask));
		if (!ns_sql_column_exists(t_suf + "sample_region_image_info", subregion_mask_column_name, sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding adding subregion label image id column to sample_region_image_info\n";
			*sql << "ALTER TABLE `" << t_suf << "sample_region_image_info` "
				"ADD COLUMN `" << subregion_mask_column_name << "` BIGINT(20) NOT NULL DEFAULT '0' AFTER `position_analysis_model`";
			sql->send_query();

			changes_made = true;
		}
		if (!ns_sql_column_exists(t_suf + "sample_region_image_info", "subregion_mask_id", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding adding subregion label mask id column to sample_region_image_info\n";
			*sql << "ALTER TABLE `" << t_suf << "sample_region_image_info` "
				"ADD COLUMN `subregion_mask_id` BIGINT(20) NOT NULL DEFAULT '0' AFTER `mask_id`";
			sql->send_query();

			changes_made = true;
		}
		if (!ns_sql_column_exists(t_suf + "analysis_model_registry", "filename",sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding adding filename columns to analysis model registry\n";
			*sql << "ALTER TABLE analysis_model_registry ADD COLUMN filename TEXT NULL AFTER file_time";
			sql->send_query();
		}

		if (!ns_sql_column_exists(t_suf + "sample_region_image_info", "last_posture_analysis_model_used", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding adding model tracking columns to sample_region_image_info\n";
			*sql << "ALTER TABLE `sample_region_image_info`"
				"ADD COLUMN `last_posture_analysis_model_used` TEXT NOT NULL AFTER `worm_detection_model`,"
				"ADD COLUMN `last_posture_analysis_method_used` VARCHAR(10) NOT NULL AFTER `last_posture_analysis_model_used`,"
				"ADD COLUMN `last_worm_detection_model_used` TEXT NOT NULL AFTER `last_posture_analysis_method_used`,"
				"ADD COLUMN `last_position_analysis_model_used` TEXT NOT NULL AFTER `last_worm_detection_model_used`";
			sql->send_query();
		}

		if (!ns_sql_column_exists("hosts", "system_hostname", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding system hostname and additional host description columns to hosts table\n";
			*sql << " ALTER TABLE `hosts` "
				"ADD COLUMN `system_hostname` CHAR(255) NOT NULL DEFAULT '' AFTER `time_of_last_successful_long_term_storage_write`, "
				"ADD COLUMN `additional_host_description` CHAR(255) NOT NULL DEFAULT '' AFTER `system_hostname` ";
			sql->send_query();
			changes_made = true;
		}

		if (!ns_sql_column_exists(t_suf + "hosts", "dispatcher_refresh_interval", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding column for dispatcher refresh interval\n";
			*sql << "ALTER TABLE `" << t_suf << "hosts` "
				"ADD COLUMN `dispatcher_refresh_interval` INT NOT NULL DEFAULT '0' AFTER `database_used`";
			sql->send_query();
			changes_made = true;
		}
		if (!ns_sql_column_exists(t_suf + "analysis_model_registry", "details", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Updating model registry\n";
			*sql << "ALTER TABLE `" << t_suf << "analysis_model_registry` "
				"ADD COLUMN `details` TEXT NOT NULL DEFAULT '' AFTER `filename`";
			sql->send_query();
			changes_made = true;
		}


	}
	if (!ns_sql_column_exists(t_suf + "experiments", "mask_time", sql)) {
		if (just_test_if_needed)
			return true;
		cout << "Adding additional metadata columns to experiment table\n";
		*sql << "ALTER TABLE `" << t_suf << "experiments`"
			"ADD COLUMN `mask_time` BIGINT(20) UNSIGNED NOT NULL DEFAULT '0' AFTER `number_of_regions_in_latest_storyboard_build`,"
			"ADD COLUMN `compression_type` CHAR(50) NOT NULL DEFAULT 'jp2k' AFTER `mask_time`,"
			"ADD COLUMN `compression_ratio` FLOAT NOT NULL DEFAULT '0.05' AFTER `compression_type`";
		sql->send_query();
		changes_made = true;
	}
	if (!ns_sql_column_exists(t_suf + "capture_samples", "conversion_16_bit_low_bound", sql)) {
		if (just_test_if_needed)
			return true;
		cout << "Updating capture samples column\n";
		*sql << "ALTER TABLE `" << t_suf << "capture_samples` "
			"ADD COLUMN `conversion_16_bit_low_bound` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `number_of_consecutive_captures_per_sample`";
		sql->send_query();
		changes_made = true;
	}
	if (!ns_sql_column_exists(t_suf + "capture_samples", "conversion_16_bit_high_bound", sql)) {
		if (just_test_if_needed)
			return true;
		cout << "Updating capture samples column\n";
		*sql << "ALTER TABLE `" << t_suf << "capture_samples` "
			"ADD COLUMN `conversion_16_bit_high_bound` BIGINT UNSIGNED NOT NULL DEFAULT '0' AFTER `conversion_16_bit_low_bound`";
		sql->send_query();
		changes_made = true;
	}


	if (!updating_local_buffer) {
		if (!ns_sql_column_exists("hosts", "system_parallel_process_id", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Adding additional host HPC columns\n";
			*sql << "ALTER TABLE `hosts` ADD COLUMN `system_parallel_process_id` INT UNSIGNED NOT NULL DEFAULT '0' AFTER `additional_host_description`";
			sql->send_query();
			changes_made = true;
		}


		*sql << "SELECT table_name "
			"FROM information_schema.tables "
			"WHERE table_schema = '" << schema_name << "' "
			" AND table_name = 'processing_node_status'";
		res.clear();
		sql->get_rows(res);
		if (res.empty()) {
			if (just_test_if_needed)
				return true;
			cout << "Adding additional host HPC columns\n";
			*sql << "CREATE TABLE `processing_node_status` ("
				"`host_id` BIGINT NULL,"
				"`node_id` BIGINT NULL,"
				"`current_processing_job_queue_id` BIGINT NULL,"
				"`current_processing_job_id` BIGINT NULL,"
				"`state` VARCHAR(20) NOT NULL DEFAULT '',"
				"`current_output_event_id` BIGINT NULL,"
				"`ts` TIMESTAMP NOT NULL DEFAULT CURRENT_TIMESTAMP ON UPDATE CURRENT_TIMESTAMP"
				")"
				"COLLATE = 'utf8_general_ci'"
				"ENGINE = MyISAM";
			sql->send_query();
			changes_made = true;
		}
		if (!ns_sql_column_exists("animal_storyboard", "minimum_distance_to_juxtipose_neighbors", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Updating storyboard column\n";
			*sql << "ALTER TABLE animal_storyboard ADD COLUMN minimum_distance_to_juxtipose_neighbors INT NOT NULL DEFAULT 0 AFTER image_delay_time_after_event";
			sql->send_query();
			changes_made = true;
		}

		if (!ns_sql_column_exists("processing_jobs", "generate_video_at_high_res", sql)) {
			if (just_test_if_needed)
				return true;
			cout << "Updating processing jobs column\n";
			*sql << "ALTER TABLE `processing_jobs` ADD COLUMN `generate_video_at_high_res` INT(10) UNSIGNED NOT NULL DEFAULT '0' AFTER `pending_another_jobs_completion` ";
			sql->send_query();
			changes_made = true;
		}
	}
	if (!changes_made && !just_test_if_needed){
		cout << "The database appears up-to-date; no changes were made.\n";
	}


	return changes_made;
}

#ifdef _WIN32
#include <iostream>
#include <stdexcept>
#include <string>
#include <windows.h>
#else

#include <termios.h>
#include <unistd.h>
#include <stdio.h>
#include <iostream>
#include <string>

int getch() {
	int ch;
	struct termios t_old, t_new;

	tcgetattr(STDIN_FILENO, &t_old);
	t_new = t_old;
	t_new.c_lflag &= ~(ICANON | ECHO);
	tcsetattr(STDIN_FILENO, TCSANOW, &t_new);

	ch = getchar();

	tcsetattr(STDIN_FILENO, TCSANOW, &t_old);
	return ch;
}


#endif

std::string get_hidden_password() {
	#ifdef _WIN32
		string result;

		// Set the console mode to no-echo, not-line-buffered input
		DWORD mode, count;
		HANDLE ih = GetStdHandle(STD_INPUT_HANDLE);
		HANDLE oh = GetStdHandle(STD_OUTPUT_HANDLE);
		if (!GetConsoleMode(ih, &mode))
			throw runtime_error(
				"getpassword: You must be connected to a console to use this function.\n"
			);
		SetConsoleMode(ih, mode & ~(ENABLE_ECHO_INPUT | ENABLE_LINE_INPUT));

		// Get the password string
		char c;
		while (ReadConsoleA(ih, &c, 1, &count, NULL) && (c != '\r') && (c != '\n'))
		{
			if (c == '\b')
			{
				if (result.length())
				{
					WriteConsoleA(oh, "\b \b", 3, &count, NULL);
					result.erase(result.end() - 1);
				}
			}
			else
			{
				WriteConsoleA(oh, "*", 1, &count, NULL);
				result.push_back(c);
			}
		}

		// Restore the console mode
		SetConsoleMode(ih, mode);
		cout << endl;
		return result;
	#else
		const char BACKSPACE = 127;
		const char RETURN = 10;

		string password;
		unsigned char ch = 0;
		while (true){
			ch = getch();
			if (ch == RETURN)
				break;
			if (ch == BACKSPACE){
				if (password.length() != 0){
					password.resize(password.length() - 1);
				}
			}
			else{
				password += ch;
			}
		}
		cout << endl;
		return password;
	#endif
}

std::string ns_image_server::create_and_configure_sql_database(bool local, const std::string& schema_specification_filename) {
	if (local) {
		if (local_buffer_ip.empty()) throw ns_ex("Please specifiy a local buffer ip in the ns_image_server.ini file");
		if (local_buffer_user.empty()) throw ns_ex("Please specifiy a local buffer username in the ns_image_server.ini file");
		if (local_buffer_pwd.empty()) throw ns_ex("Please specifiy a local buffer password in the ns_image_server.ini file");
		if (local_buffer_db.empty()) throw ns_ex("Please specifiy a local buffer database name in the ns_image_server.ini file");
	}
	else {
		if (sql_user.empty()) throw ns_ex("Please specifiy a sql username in the ns_image_server.ini file.");
		if (sql_pwd.empty()) throw ns_ex("Please specifiy a sql password in the ns_image_server.ini file.");
		if (sql_server_addresses.size() != 1 || sql_server_addresses[0].empty()) throw ns_ex("Please specifiy only one sql server address in the ns_image_server.ini file.");
		if (possible_sql_databases.size() != 1 || possible_sql_databases[0].empty()) throw ns_ex("Please specifiy only one sql database in the ns_image_server.ini file.");
	}
	ifstream schema;
	if (!local && !schema_specification_filename.empty()) {
		schema.open(schema_specification_filename);
		if (schema.fail()) {
			throw ns_ex("Could not open the schema file specified at ") << schema_specification_filename;
		}
	}
	const std::string username(local ? local_buffer_user : sql_user),
		password(local ? local_buffer_pwd : sql_pwd),
		db(local ? local_buffer_db : possible_sql_databases[0]),
		hostname(local ? local_buffer_ip : sql_server_addresses[0]);
	std::cout << "**Configuring the ";
	if (local)
		std::cout << "local db buffer ";
	else std::cout << "central sql database ";
	std::cout << db << " on server " << hostname << "**\n";
	std::cout << "To modify schema, please provide a username with administrative privileges on your sql server: ";
	std::string root_username;
	getline(cin, root_username);
	std::cout << "Please enter the password for this account: ";
	std::string root_password = get_hidden_password();

	ns_sql_connection sql;

	sql.connect(hostname, root_username, root_password, 0);

	sql << "SHOW DATABASES";
	ns_sql_result res;
	sql.get_rows(res);
	bool found_db(false);
	for (unsigned int i = 0; i < res.size(); i++) {
		if (res[i][0] == db) {
			found_db = true;
			break;
		}
	}
	if (found_db) {
		sql << "SELECT ROUND(SUM(data_length + index_length) / 1024 / 1024, 2) FROM information_schema.TABLES";
		ns_sql_result res2;
		sql.get_rows(res2);
		if (res2.size() == 0)
			throw ns_ex("Could not obtain database size.");
		ns_64_bit database_size_in_mb(ns_atoi64(res2[0][0].c_str()));
		if (database_size_in_mb > 10)
			throw ns_ex() << "The database " << db << " already exists on " << hostname << ", and contains " << database_size_in_mb << " megabytes of data.  It is too dangerous to delete this automatically.  However, if you really want to install a new schema, log in manually and run the sql command \"DROP SCHEMA " << db << "\";";

		cout << "The database " << db << " already exists on " << hostname << ".  Do you want to delete it and create it again?\n"
			"WARNING: This will delete all metadata not backed up to disk!\n"
			"To delete, type y . To cancel and do nothing, type n : ";
		while (true) {
			string a;
			getline(cin, a);
			cout << "\n";
			if (a.size() > 0 && a[0] == 'y') {
				cout << "Are you sure?  This will drop your database schema and erase " << database_size_in_mb << " megabytes of data on " << hostname << "\n"
					"Only proceed if you really understand what you are doing!\n"
					"To proceed, type y . To cancel and do nothing, type n : ";
				while (true) {
					string b;
					getline(cin, b);
					cout << "\n";
					if (b.size() > 0 && b[0] == 'y')
						break;
					if (b.size() > 0 && (b[0] == 'n' || b[0] == 'q' || b[0] == 'c'))
						throw ns_ex("The request was cancelled by the user.");
					cout << "Unknown response: \"" << b << "\".  Please type y or n :";
				}
				break;
			}
			if (a.size() > 0 && (a[0] == 'n' || a[0] == 'q' || a[0] == 'c'))
				throw ns_ex("The request was cancelled by the user.");
			cout << "Unknown response: \"" << a << "\".  Please type y or n :";
		}
		sql << "DROP SCHEMA " << db;
		sql.send_query();
	}
	sql << "SELECT EXISTS(SELECT 1 FROM mysql.user WHERE user = '" << username << "' AND host='%')";
	ns_sql_result tmp;
	sql.get_rows(tmp);
	if (tmp.empty())
		throw ns_ex("mysql.user table not found!");
	if (tmp[0][0] == "0") {
		sql << "CREATE USER '" << username << "'@'%' identified by '" << password << "'";
		sql.send_query();
	}
	sql << "SELECT EXISTS(SELECT 1 FROM mysql.user WHERE user = '" << username << "' AND host='localhost')";

	sql.get_rows(tmp);
	if (tmp.empty())
		throw ns_ex("mysql.user table not found!");
	if (tmp[0][0] == "0") {
		sql << "CREATE USER '" << username << "'@'localhost' identified by '" << password << "'";
		sql.send_query();
	}

	sql << "CREATE DATABASE " << db;
	sql.send_query();
	sql << "GRANT ALL on *.* TO '" << username << "'@'localhost'";
	sql.send_query();
	sql << "GRANT ALL on *.* TO '" << username << "'@'localhost' identified by '" << password << "'";
	sql.send_query();
	sql << "GRANT ALL on *.* TO '" << username << "'@'%'";
	sql.send_query();
	sql << "GRANT ALL on *.* TO '" << username << "'@'%' identified by '" << password << "'";
	sql.send_query();
	sql << "USE " << db;
	sql.send_query();


	if (!local) {
		//upload schema from file
		if (!schema_specification_filename.empty()) {
			std::string line;
			while (!schema.fail()) {
				getline(schema, line);
				sql << line;
				sql.send_query();
			}
		}
		else {
			//upload schema from copy stored in header file
			std::string ch;
			bool in_quote = false;
			if (image_server_db_schema_sql_len == 0)
				throw ns_ex("No db schema provided!");
			else cout << "Schema len: " << image_server_db_schema_sql_len << "\n";
			for (unsigned long i = 0; i < image_server_db_schema_sql_len; i++) {

				const char a = image_server_db_schema_sql[i];
				if (a == '\'')
					in_quote = !in_quote;
				if (a == '\n' || a == '\r') {
					if (ch.size() >= 2 && ch[0] == '-' && ch[1] == '-') {
						ch.resize(0);
						continue;
					}
					else continue;
				}
				if (!in_quote && a == ';') {
					if (ch.empty())
						continue;
					cout << ch << "\n\n";
					sql.send_query(ch);

					ch.resize(0);
					continue;
				}
				else ch += image_server_db_schema_sql[i];
			}
		}
		sql.send_query("COMMIT");
	}

	cout << "Done!\n";
	schema.close();
	sql.disconnect();
	return db;
}

bool ns_open_db_table_file(const ns_sql_result & columns, ns_sql_result & data, const std::string & filename, const std::string & directory, std::ofstream & ff){
	std::string f(directory + DIR_CHAR_STR + filename);
	ff.open(f.c_str());
	if(ff.fail())
		throw ns_ex("ns_write_db_table()::Could not open file for writing: ") << f;
	if (columns.size() == 0)
		throw ns_ex("ns_write_db_table()::No columns were provided for ") << f;
	if(data.size() > 0 && columns.size() != data[0].size())
		throw ns_ex("ns_write_db_table()::Column and data dimensions do not match: ") << filename;
	return true;
}
std::string ns_preserve_quotes(const std::string & s){
	std::string r;
	r.reserve(s.size());
	for (unsigned int i = 0; i < s.size(); i++){
		if (s[i] == '"')
			r+="\\\"";
		else r+=s[i];
	}
	return r;
}
bool ns_write_db_table(const ns_sql_result & columns, ns_sql_result & data, const std::string & filename, const std::string & directory){

	ofstream ff;
	if (!ns_open_db_table_file(columns,data,filename,directory,ff))
		return false;
	ff << "\"" << ns_preserve_quotes(columns[0][0]) << "\"";
	for (unsigned int i = 1; i < columns.size(); i++){
		ff << ",\"" << ns_preserve_quotes(columns[i][0]) << "\"";
	}
	ff << "\n";
	if (data.size() == 0)
		return false;
	for (unsigned int i = 0; i < data.size(); i++){
		ff << "\"" << ns_preserve_quotes(data[i][0]) << "\"";
		for (unsigned int j = 1; j < data[i].size(); j++)
			ff << ",\"" << ns_preserve_quotes(data[i][j]) << "\"";
		ff << "\n";
	}
	ff.close();
	return true;
};

void ns_concatenate_results(const ns_sql_result & male, ns_sql_result & female){
	unsigned long s(female.size());
	female.resize(male.size()+female.size());
	for (unsigned int i = 0; i < male.size(); i++)
		female[s+i].insert(female[s+i].end(),male[i].begin(),male[i].end());
}
void ns_gzip_file(std::string & f1, std::string f2){
	ifstream in(f1.c_str());
	if (in.fail())
		throw ns_ex("ns_gzip_file()::Could not open ") << f1;
	gzFile gf;
	gf = gzopen(f2.c_str(),"wb9");
	if (gf == Z_NULL)
		throw ns_ex("Could not open ") << f2;
	const unsigned long chunk_size(1024*1024*8);
	char * buf = new char[chunk_size];
	try{
		//gzbuffer(gzFile,chunk_size);
		while(true){
			in.read(buf,chunk_size);
			unsigned long s(in.gcount());
			if (!gzwrite(gf,buf,s))
				throw ns_ex("ns_gzip_file()::Could not write to gzip file");
			if (in.fail())
				break;

		}
	}
	catch(...){
		delete[] buf;
	}
	in.close();
	if(gzclose(gf) != Z_OK)
		throw ns_ex("Could not close zip file");
}
/*
void ns_zip_file(FILE * source, FILE * dest){

	const unsigned long chunk_size(8*1024*1024);

	unsigned char * in = new unsigned char[chunk_size];
	unsigned char * out = new unsigned char[chunk_size];
	try{
		int ret, flush;
		unsigned have;
		z_stream strm;
			// allocate deflate state
		strm.zalloc = Z_NULL;
		strm.zfree = Z_NULL;
		strm.opaque = Z_NULL;
		ret = deflateInit(&strm, Z_BEST_COMPRESSION);
		if (ret != Z_OK)
			throw ns_ex("Could not initialize zlib");
		do {
			strm.avail_in = fread(in, 1, chunk_size, source);
			if (ferror(source)) {
				(void)deflateEnd(&strm);
				throw ns_ex("Could not read from source");
			}
			flush = feof(source) ? Z_FINISH : Z_NO_FLUSH;
			strm.next_in = in;

		// run deflate() on input until output buffer not full, finish
		//	compression if all of source has been read in
			do {
				strm.avail_out = chunk_size;
				strm.next_out = out;
				ret = deflate(&strm, flush);   // no bad return value
				if (ret == Z_STREAM_ERROR)
					throw ns_ex("Error in deflate");//state not clobbered
				have = chunk_size - strm.avail_out;
				if (fwrite(out, 1, have, dest) != have || ferror(dest)) {
					(void)deflateEnd(&strm);
					throw ns_ex("Could not write to file");
				}
			} while (strm.avail_out == 0);
			if (strm.avail_in != 0)
				throw ns_ex("Data left over");
				// done when last data in file processed
		} while (flush != Z_FINISH);
		if (ret != Z_STREAM_END)
				throw ns_ex("Stream not ended!");
			// clean up and return
		(void)deflateEnd(&strm);
		delete [] in;
		delete [] out;
	}
	catch(...){
		delete [] in;
		delete [] out;
	}
}
*/
void ns_zip_experimental_data(const std::string & output_directory,bool delete_original){
	ns_dir dir;
	std::vector<std::string> files;
	dir.load_masked(output_directory,"csv",files);


	for (unsigned int i = 0; i < files.size(); i++){
		if (ns_dir::extract_extension(files[i]) == "gz")
			continue;
		std::string source(output_directory + DIR_CHAR_STR + files[i]);
		std::string sink(source + ".gz" );
		ns_gzip_file(source,sink);
	}
	if (delete_original){
		for (unsigned int i = 0; i < files.size(); i++){
			if (ns_dir::extract_extension(files[i]) == "gz")
				continue;
			std::string source(output_directory + DIR_CHAR_STR + files[i]);
			ns_dir::delete_file(source);
		}
	}

}

class ns_zipped_file_input{
	char * buff;
	std::string line,line_ret;
	unsigned long buff_size,buff_pos,buff_max_size;
	gzFile gf;
	public:
	ns_zipped_file_input(const unsigned long buff_max_size_)
		:buff(new char[buff_max_size_]),
		 buff_size(0),buff_pos(0),buff_max_size(buff_max_size_){}

	void open(const std::string & file){
		gf = gzopen(file.c_str(),"rb");
		if (gf == Z_NULL)
			throw ns_ex("Could not open ") << file;
	}
	~ns_zipped_file_input(){close();}
	void close(){
		if (gf != 0)
			gzclose(gf);
		gf = 0;
	}
	bool eof() const{
		return gzeof(gf) && buff_pos >= buff_size;
	}

	const std::string & read_line(){
		while(true){
			if (!eof()){
				if (buff_pos == buff_size){
					buff_size = gzread(gf,buff,buff_max_size);
					buff_pos = 0;
				}
			}
			else{
				if (buff_pos >= buff_size)
					throw ns_ex("Reading past end of file!");
			}
			if (buff[buff_pos] == '\n' || buff_pos == buff_size){
				buff_pos++;
				line_ret = line;
				line.resize(0);
				return line_ret;
			}
			else{
				line+=buff[buff_pos];
				buff_pos++;
			}
		}
	}

};

void ns_ungzip_file(std::string & f1, std::string f2){
	ns_zipped_file_input input(1024*1024);
	input.open(f1);
	ofstream out(f2.c_str());
	while (!input.eof())
		out << input.read_line() << "\n";
	input.close();
	out.close();
}

void ns_parse_quoted_csv_line(const std::string & line, std::vector<std::string> & data){
	data.resize(0);
	if(line.size() == 0)
		return;
	data.resize(1);
	bool in_quotes(false);
	for(unsigned int i = 0; i < line.size(); i++){
		//handle backslashed quotation marks
		if (line[i] == '\\' && (i+1 < line.size()) && line[i+1] == '"'){
			data.rbegin()->push_back('"');
			i++;
			continue;
		}
		if (line[i] == '"'){
			in_quotes = !in_quotes;
			continue;
		}
		if (!in_quotes && line[i] == ','){
			data.resize(data.size()+1);
			continue;
		}
		data.rbegin()->push_back(line[i]);
	}
}
void ns_add_data_to_db_from_file(const std::string table_name,const std::string file_name,ns_sql & sql,const bool allow_empty=false){
	ns_zipped_file_input input(1024*1024);
	input.open(file_name);
	std::vector<std::string> column_names;
	ns_parse_quoted_csv_line(input.read_line(),column_names);
	if(input.eof()){
		if (allow_empty)
			return;
		else
		throw ns_ex("ns_add_data_to_db_from_file()::Could not read first line from file ") << table_name;
	}
	if (column_names.size() == 0)
		throw ns_ex("ns_add_data_to_db_from_file()::No columns provided for ") << table_name;
	for (unsigned int i = 0; i < column_names.size(); i++)
		if (column_names[i].size() == 0)
			throw ns_ex("ns_add_data_to_db_from_file()::Empty column name specified for table ") << table_name;
	std::vector<std::string> data;
	unsigned long line_number(1);
	sql.set_autocommit(false);
	try{
		sql.send_query("BEGIN");
		while(!input.eof()){
			ns_parse_quoted_csv_line(input.read_line(),data);
			if (data.size() != column_names.size())
					throw ns_ex("Invalid number of columns on line number ") << line_number;
			sql << "INSERT INTO `" << table_name << "` SET ";
			for (unsigned int i = 0; i < column_names.size(); i++){
				sql << "`" << column_names[i] << "`=\"" << sql.escape_string(data[i]) << "\"";
					if (i+1 != column_names.size())
						sql << ",";
			}
			//cout << sql.query() << "\n";
			sql.send_query();
		}
		sql.send_query("COMMIT");
		sql.set_autocommit(true);
	}
	catch(...){
		sql.clear_query();
		sql.send_query("ROLLBACK");
		sql.set_autocommit(false);
		throw;
	}
	input.close();

}
bool ns_update_db_using_experimental_data_from_file(const std::string new_database,bool use_existing_database, const std::string & output_directory,ns_sql & sql){

	if (new_database.find("=") != new_database.npos ||
		new_database.find("&") != new_database.npos ||
		new_database.find("%") != new_database.npos ||
		new_database.find("$") != new_database.npos ||
		new_database.find("#") != new_database.npos ||
		new_database.find("-") != new_database.npos ||
		new_database.find("+") != new_database.npos ||
		new_database.find("@") != new_database.npos ||
		new_database.find("!") != new_database.npos||
		new_database.find("'") != new_database.npos||
		new_database.find("\"") != new_database.npos||
		new_database.find(";") != new_database.npos||
		new_database.find(":") != new_database.npos)
		throw ns_ex("The database name you suggested ") << new_database <<
		" contains a character such as "<< "=&%$#-+@!'\";:";

	std::string current_database;
	current_database = sql.get_value("SELECT DATABASE()");

	sql << "SHOW DATABASES";
	ns_sql_result databases;
	sql.get_rows(databases);
	bool database_exists(false), need_to_create_schema(false);
	for (unsigned int i = 0; i < databases.size();i++){
		if (databases[i][0] == new_database){
			database_exists = true;
			break;
		}
	}
	if (!database_exists)
		need_to_create_schema = true;

	if (database_exists && !use_existing_database) {
		//don't overwrite databases that have any existing tables.
		sql << "USE " << new_database << "";
		sql.send_query();
		try {
			sql << "SHOW TABLES";
			ns_sql_result tables;
			sql.get_rows(tables);
			if (tables.size() > 0) {
				sql << "USE " << current_database;
				sql.send_query();
				return false;
			}
			sql << "USE " << current_database;
			sql.send_query();
			need_to_create_schema = true;
		}
		catch (...) {
			sql << "USE " << current_database;
			sql.send_query();
			throw;
		}

	}

	ns_dir dir;
	std::vector<std::string> files;
	dir.load_masked(output_directory,"csv.gz",files);
	if (files.size() == 0)
		throw ns_ex("No data files could be found in the directory") << output_directory;

	if (!database_exists || need_to_create_schema){
		cout << "Creating a new database, \"" << new_database << "\", to hold the data...\n";
		sql << "SHOW TABLES";
		ns_sql_result tables;
		sql.get_rows(tables);
		vector<std::string> create_commands(tables.size());
		for (unsigned int i = 0; i < tables.size(); i++)
				ns_get_table_create(tables[i][0],create_commands[i],sql);
		if (tables.size() == 0)
			throw ns_ex("Before importing, please set the default/current database to one that already has the schema set up!");
		if (!database_exists) {
			sql << "CREATE DATABASE " << new_database << "";
			sql.send_query();
		}
		sql << "USE " << new_database << "";
		sql.send_query();
		try{
			for (unsigned int i = 0; i < create_commands.size(); i++){
					sql << create_commands[i];
					sql.send_query();
			}
			sql << "USE " << current_database << "";
			sql.send_query();
		}
		catch(...){
			sql.clear_query();
			sql << "USE " << current_database;
			sql.send_query();
			throw;
		}

	}



	for (unsigned int i = 0; i < files.size(); i++){
		if (ns_dir::extract_extension(files[i]) != "csv.gz")
			continue;
		std::string source(output_directory + DIR_CHAR_STR + files[i]);
		std::string table_name = ns_dir::extract_filename(source);
		std::string::size_type t(table_name.find_first_of("."));
		if (t == table_name.npos)
			continue;
		table_name = table_name.substr(0,t);
		sql << "USE " << new_database;
		sql.send_query();
		try{
			cout << "Adding data for table " << table_name << "...\n";
			bool allow_empty_file = false;
			if (table_name == "experiment_groups" ||
				table_name == "path_data" ||
				table_name == "worm_detection_results" ||
				table_name == "sample_region_image_info" ||
				table_name == "sample_region_images" ||
				table_name == "image_masks"

				)
				allow_empty_file = true;
			try {
				ns_add_data_to_db_from_file(table_name, source, sql, allow_empty_file);
			}
			catch (ns_ex & ex) {
				//Importing from old database schema can cause problems when they contained columns
				//no longer present in the database.  We can rescue import by adding them and then deleting them afterwards.
				ns_add_version_compatibility_columns(table_name, sql);
				try {
					ns_add_data_to_db_from_file(table_name, source, sql, allow_empty_file);
					ns_remove_version_compatibility_columns(table_name, sql);
				}
				catch (...) {
					ns_remove_version_compatibility_columns(table_name, sql);
					throw;
				}
			}
			sql << "USE " << current_database;
			sql.send_query();
		}
		catch(...){
			sql.clear_query();
			sql << "USE " << current_database;
			sql.send_query();
			throw;
		}
	}
	return true;
};

void ns_write_experimental_data_in_database_to_file(const unsigned long experiment_id, const std::string & output_directory,ns_sql & sql){

	ns_dir::create_directory_recursive(output_directory);
	cout << "Selecting experiment data...";
	sql << "SHOW COLUMNS IN experiments";
	ns_sql_result experiment_columns;
	sql.get_rows(experiment_columns);
	sql << "SELECT * from experiments WHERE id = " << experiment_id;
	ns_sql_result experiment_data;
	sql.get_rows(experiment_data);
	ns_write_db_table(experiment_columns,experiment_data,"experiments.csv",output_directory);

	sql << "SHOW COLUMNS IN experiment_groups";
	ns_sql_result experiment_group_columns;
	sql.get_rows(experiment_group_columns);
	sql << "SELECT g.* from experiment_groups as g, experiments as e WHERE g.group_id = e.group_id AND e.id = " << experiment_id;
	ns_sql_result experiment_group_data;
	sql.get_rows(experiment_group_data);
	ns_write_db_table(experiment_group_columns,experiment_group_data,"experiment_groups.csv",output_directory);

	sql << "SHOW COLUMNS IN capture_samples";
	ns_sql_result sample_columns;
	sql.get_rows(sample_columns);
	sql << "SELECT * from capture_samples WHERE experiment_id = " << experiment_id;
	ns_sql_result sample_data;
	sql.get_rows(sample_data);
	ns_write_db_table(sample_columns,sample_data,"capture_samples.csv",output_directory);

	sql << "SHOW COLUMNS IN capture_schedule";
	ns_sql_result capture_schedule_columns,
			capture_schedule_data;
	sql.get_rows(capture_schedule_columns);
	sql << "SELECT * from capture_schedule WHERE experiment_id = " << experiment_id;
	sql.get_rows(capture_schedule_data);
	ns_write_db_table(capture_schedule_columns,capture_schedule_data,"capture_schedule.csv",output_directory);

	ns_sql_result sample_region_image_info_columns,
				 sample_region_image_info_data;
	sql << "SHOW COLUMNS IN sample_region_image_info";
	sql.get_rows(sample_region_image_info_columns);
	sql << "SELECT i.* FROM sample_region_image_info as i, capture_samples as s "
		"WHERE i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(sample_region_image_info_data);
	ns_write_db_table(sample_region_image_info_columns,sample_region_image_info_data,"sample_region_image_info.csv",output_directory);

	ns_sql_result sample_region_images_columns,
			  sample_region_images_data;
	sql << "SHOW COLUMNS IN sample_region_images";
	sql.get_rows(sample_region_images_columns);
	sql << "SELECT m.* FROM sample_region_images as m, sample_region_image_info as i, capture_samples as s "
		"WHERE m.region_info_id = i.id AND i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(sample_region_images_data);
	ns_write_db_table(sample_region_images_columns,sample_region_images_data,"sample_region_images.csv",output_directory);

	ns_sql_result worm_detection_results_columns,
			  worm_detection_results_data;
	sql << "SHOW COLUMNS IN worm_detection_results";
	sql.get_rows(worm_detection_results_columns);
	sql << "SELECT d.* FROM worm_detection_results as d, sample_region_images as m, sample_region_image_info as i, capture_samples as s "
		"WHERE (d.id = m.worm_detection_results_id OR d.id = m.worm_interpolation_results_id) AND m.region_info_id = i.id AND i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(worm_detection_results_data);
	ns_write_db_table(worm_detection_results_columns,worm_detection_results_data,"worm_detection_results.csv",output_directory);

	ns_sql_result captured_images_columns,
			  captured_images_data;
	sql << "SHOW COLUMNS IN captured_images";
	sql.get_rows(captured_images_columns);
	sql << "SELECT i.* from captured_images as i, capture_samples as s "
		"WHERE i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(captured_images_data);
	ns_write_db_table(captured_images_columns,captured_images_data,"captured_images.csv",output_directory);

	ns_sql_result path_data_columns,
			  path_data_data;
	sql << "SHOW COLUMNS IN path_data";
	sql.get_rows(path_data_columns);
	sql << "SELECT p.* from path_data as p, sample_region_image_info as r, capture_samples as s "
		"WHERE p.region_id = r.id AND r.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(path_data_data);
	ns_write_db_table(path_data_columns,path_data_data,"path_data.csv",output_directory);

	ns_sql_result mask_columns,
			 mask_data;
	sql << "SHOW COLUMNS IN image_masks";
	sql.get_rows(mask_columns);
	sql << "SELECT DISTINCT m.* FROM image_masks as m, sample_region_image_info as i, capture_samples as s "
		"WHERE m.id = i.mask_id AND i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(mask_data);
	ns_write_db_table(mask_columns,mask_data,"image_masks.csv",output_directory);


	vector<ns_processing_task> processing_tasks;
	processing_tasks.push_back(ns_unprocessed);
	processing_tasks.push_back(ns_process_thumbnail);
	processing_tasks.push_back(ns_process_lossy_stretch);
	processing_tasks.push_back(ns_process_spatial);
	processing_tasks.push_back(ns_process_threshold);
	processing_tasks.push_back(ns_process_worm_detection);
	processing_tasks.push_back(ns_process_worm_detection_labels);
	processing_tasks.push_back(ns_process_interpolated_vis);
	processing_tasks.push_back(ns_process_region_vis);
	processing_tasks.push_back(ns_process_region_interpolation_vis);

	ns_sql_result image_ids;
	sql << "SELECT ";
	sql << "m." << ns_processing_step_db_column_name(processing_tasks[0]);
	for (unsigned int i = 1; i < processing_tasks.size(); i++)
		sql << ",m." << ns_processing_step_db_column_name(processing_tasks[i]);
	sql << " FROM sample_region_images as m, sample_region_image_info as i, capture_samples as s "
		"WHERE m.region_info_id = i.id AND i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(image_ids);

	ns_sql_result image_ids2;
	sql << "SELECT d.data_storage_on_disk_id FROM worm_detection_results as d, sample_region_images as m, sample_region_image_info as i, capture_samples as s "
		"WHERE (d.id = m.worm_detection_results_id OR d.id = m.worm_interpolation_results_id) AND m.region_info_id = i.id AND i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(image_ids2);
	ns_concatenate_results(image_ids2,image_ids);

	sql << "SELECT i.image_id,i.small_image_id from captured_images as i, capture_samples as s "
		"WHERE i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(image_ids2);
	ns_concatenate_results(image_ids2,image_ids);

/*	sql << "SELECT m.image_id FROM sample_region_image_aligned_path_images as m, sample_region_image_info as i, capture_samples as s "
	"WHERE m.region_info_id = i.id AND i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(image_ids2);
	ns_concatenate_results(image_ids2,image_ids);
	*/

	sql << "SELECT i.time_path_solution_id, i.movement_image_analysis_quantification_id FROM sample_region_image_info as i, capture_samples as s "
		"WHERE i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(image_ids2);
	ns_concatenate_results(image_ids2,image_ids);

	sql << "SELECT p.image_id from path_data as p, sample_region_image_info as r, capture_samples as s "
		"WHERE p.region_id = r.id AND r.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(image_ids2);
	ns_concatenate_results(image_ids2,image_ids);

	sql << "SELECT DISTINCT m.image_id FROM image_masks as m, sample_region_image_info as i, capture_samples as s "
		"WHERE m.id = i.mask_id AND i.sample_id = s.id AND s.experiment_id = " << experiment_id;
	sql.get_rows(image_ids2);
	ns_concatenate_results(image_ids2,image_ids);


	ns_sql_result images_columns,
			images_data;
	sql << "SHOW COLUMNS IN images";
	sql.get_rows(images_columns);

	ofstream ff;
	ns_open_db_table_file(images_columns,images_data,"images.csv",output_directory,ff);
	ff << "\"" << ns_preserve_quotes(images_columns[0][0]) << "\"";
	for (unsigned int i = 1; i < images_columns.size(); i++)
		ff << ",\"" << ns_preserve_quotes(images_columns[i][0]) << "\"";
	ff << "\n";

	cout << "\nSelecting image table...";
	for (unsigned int i = 0; i < image_ids.size(); i++){
		if (i%1000 == 0)
			cout << (100*i)/image_ids.size() << "%...";
		sql << "SELECT * from images WHERE ";
		sql << "id = " << image_ids[i][0];
		for (unsigned int j = 1; j < image_ids[i].size(); j++)
			sql << " OR id = " << image_ids[i][j];
		sql.get_rows(images_data);
		for (unsigned int j = 0; j < images_data.size(); j++){
			ff << "\"" << ns_preserve_quotes(images_data[j][0]) << "\"";
			for (unsigned int k = 1; k< images_data[j].size(); k++)
				ff << ",\"" << ns_preserve_quotes(images_data[j][k]) << "\"";
			ff << "\n";
		}
	}
	cout << "\n";
}

ns_sql * ns_image_server::new_sql_connection(const std::string & source_file, const unsigned int source_line, const unsigned int retry_count, const bool select_default_database) const{
	//if there's a problem with the sql server, handle it.  Don't just cycle through errors!
	ns_acquire_lock_for_scope lock(sql_lock,source_file.c_str(),source_line);
	ns_sql *con(0);
	unsigned long try_count(0);
	con = new ns_sql();
	//con->local_locking_behavior = ns_sql_connection::ns_thread_locking;
	unsigned long start_address_id = 0;

	try{
		if (sql_server_addresses.empty())
			throw ns_ex("ns_image_server::new_sql_connection()::No addresses are specified for the sql server");

		while(true){
			//cycle through different possible connections
			const unsigned int server_id = (try_count+start_address_id)%sql_server_addresses.size();
			try{
				con->connect(sql_server_addresses[server_id],sql_user,sql_pwd,0);
				break;
			}
			catch(ns_ex & ex){
				std::string alert_text;
				ns_image_server_event sev;
				if (retry_count > 0){
					alert_text = "Cannot contact the SQL server on attempt ";
					alert_text += ns_to_string(try_count) + "/" + ns_to_string(retry_count) + ": ";
				}
				else{
					alert_text = "Cannot contact the SQL server:";
				}

				alert_text += sql_user + "@" + sql_server_addresses[server_id] + "):: ";
				alert_text += source_file + "::" + ns_to_string(source_line);

				if (retry_count > 0){
					ns_image_server_event sev;
					sev << alert_text << " : " << ex.text() << "..." << ns_ts_sql_error;
					register_server_event_no_db(sev);

					try{
						//after two checks of each possible route to the sql server, send an alert
						if (try_count == 2*sql_server_addresses.size()){
							alert_handler.submit_desperate_alert(alert_text);
						}
					}
					catch(ns_ex & ex2){
						ns_image_server_event sev2;
						sev2 << "Exception trying to send alert: " << ex2.text() << ns_ts_sql_error;
						register_server_event_no_db(sev2);
					}
					catch(...){
						ns_image_server_event sev2;
						sev2 << "Unknown exception trying to send alert."  << ns_ts_sql_error;
						register_server_event_no_db(sev2);
					}
				}
				if (retry_count == 0)
					throw ns_ex(alert_text);

				if (try_count == sql_server_addresses.size()*retry_count)
					throw ns_ex("Gave up looking for the sql server.") << ns_sql_fatal;


				try_count++;
				ns_thread::sleep(10);
				//cerr << "Retrying\n";
			}
		}

		lock.release();
		if (select_default_database)
			con->select_db(*sql_database_choice);
		return con;
	}
	catch(...){
		lock.release();
		ns_safe_delete(con);
		throw;
	}
}

//look for existing device name, create if not found
void ns_image_server::register_device(const ns_device_summary & device, ns_image_server_sql * con){

	*con << "SELECT host_id FROM devices WHERE name='" << device.name << "'";
	ns_sql_result device_info;
	con->get_rows(device_info);
	if (device_info.size() > 1)
		throw ns_ex() << static_cast<unsigned long>(device_info.size()) << " devices named " << device.name << " were found!";

	//if the device does not exist in database, register it.
	if (device_info.size() == 0){
		*con << "INSERT INTO devices SET host_id ='" << image_server.host_id() << "', name='" << device.name << "',"
			<< "simulated_device=" << (device.simulated_device?"1":"0") << ", unknown_identity=" << (device.unknown_identity?"1":"0")
			<< ", pause_captures=" << (device.paused?"1":"0") <<  ", currently_scanning=" << (device.currently_scanning?"1":"0")
			<< ", autoscan_interval=" << device.autoscan_interval << ",comments='',error_text=''";
		con->send_query();
	}
	else{
		//if the device is registered as being owned by another server, change its registration.
		if (atoi(device_info[0][0].c_str()) != image_server.host_id()){
			*con << "UPDATE devices SET host_id ='" << image_server.host_id() << "', in_recognized_error_state='0', "
			<< "simulated_device= " << (device.simulated_device?"1":"0") << ", unknown_identity=" << (device.unknown_identity?"1":"0")
			<< ", pause_captures=" << (device.paused?"1":"0") << ", currently_scanning=" << (device.currently_scanning?"1":"0")
			<< ", autoscan_interval=" << device.autoscan_interval
			<< " WHERE name='" << device.name << "'";
			con->send_query();
		}
	}
}

void ns_image_server::load_quotes(std::vector<std::pair<std::string,std::string> > & quotes, ns_sql & con){
	ns_sql_result res;
	con.get_rows("SELECT quote, author FROM daily_quotes",res);

	ns_stock_quotes q;
	int stk = con.get_integer_value("SELECT count(quote) FROM daily_quotes WHERE stock=1");
	if (res.size() == 0 || (stk > 0 && stk != q.count())){
		q.submit_quotes(con);
		con.get_rows("SELECT quote,author FROM daily_quotes",res);
	}
	quotes.resize(res.size());
	for (unsigned int i = 0; i < res.size(); i++){
		quotes[i].first = res[i][0];
		quotes[i].second = res[i][1];
	}
}
bool ns_image_server::new_software_release_available(){
	ns_acquire_for_scope<ns_sql> sql(new_sql_connection(__FILE__,__LINE__));
	bool res = new_software_release_available(sql());
	sql.release();
	return res;
}

void ns_image_server::wait_for_pending_threads(){
	alert_handler_thread.block_on_finish();
};

ns_thread_return_type alert_handler_start(void * a){
	try{
		ns_alert_handler * alert_handler(static_cast<ns_alert_handler*>(a));
		ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__,0));
		alert_handler->initialize(image_server.mail_from_address(),sql());
		alert_handler->submit_buffered_alerts(sql());
		if (alert_handler->can_send_alerts())
			alert_handler->handle_alerts(sql());
		sql.release();
	}
	catch(ns_ex & ex){
		try{
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_ex("alert_handler_start::") << ex.text());
		}
		catch(ns_ex & ex2){
			cerr << "alert_handler_start::Could not register the error " << ex.text() << ":" << ex2.text() << "\n";
		}
		catch(...){
			cerr << "alert_handler_start::Could not register the error " << ex.text() << ": due to an unknown error\n";
		}
	}
	catch(...){
		try{
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_ex("alert_handler_start::Unknown error occurred!"));
		}
		catch(ns_ex & ex2){
			cerr << "alert_handler_start::Could not register an Unknown Error :" << ex2.text() << "\n";
		}
		catch(...){
			cerr << "alert_handler_start::Could not register an unknown error due to an unknown error!\n";
		}
	}

	try{
		image_server.register_alerts_as_handled();
	}
	catch(ns_ex & ex){
		try{
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_ex("alert_handler_start::") << ex.text());
		}
		catch(ns_ex & ex2){
			cerr << "alert_handler_start::Could not register the error " << ex.text() << ":" << ex2.text() << "\n";
		}
		catch(...){
			cerr << "alert_handler_start::Could not register the error " << ex.text() << ": due to an unknown error\n";

		}
		return 0;
	}
	catch(...){
		try{
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_ex("alert_handler_start::Unknown error occurred in thread cleanup!"));
		}
		catch(ns_ex & ex2){
			cerr << "alert_handler_start::Could not register an Unknown Error :" << ex2.text() << "\n";
		}
		catch(...){
			cerr << "alert_handler_start::Could not register an unknown error due to an unknown error!\n";
		}
		return 0;
	}
	return 0;
};
void ns_image_server::handle_alerts(){
	if (!alert_handler_thread.is_running())
		alert_handler_thread.run(alert_handler_start,&alert_handler);
}
bool ns_image_server::new_software_release_available(ns_sql & sql){
	sql << "SELECT software_version_major, software_version_minor, software_version_compile FROM hosts WHERE "
		<< "software_version_major > " << software_version_major() << " OR "
		<< "(software_version_major = " << software_version_major() << " AND software_version_minor >" << software_version_minor() << ") OR "
		<< "(software_version_major = " << software_version_major() << " AND software_version_minor =" << software_version_minor() << " AND "
		<< " software_version_compile > " << software_version_compile() << ")";
	ns_sql_result res;
	sql.get_rows(res);
	return (res.size() > 0);
}

void ns_image_server::unregister_host(ns_image_server_sql * sql) {
	*sql << "UPDATE hosts SET last_ping = 0, dispatcher_refresh_interval=0 WHERE id = " << host_id();
	sql->send_query();
};

void ns_image_server::clear_processing_status(ns_image_server_sql * sql) const {
	*sql << "DELETE FROM  processing_node_status WHERE host_id = " << _host_id;
	sql->send_query();
}
void ns_image_server::update_processing_status(const std::string & processing_state, const ns_64_bit processing_job_id, const ns_64_bit processing_job_queue_id,ns_sql_connection * sql, const ns_64_bit impersonate_using_internal_thread_id) const {

	std::map<ns_64_bit, ns_thread_output_state>::iterator current_thread_state = get_current_thread_state_info(impersonate_using_internal_thread_id);

	*sql << "DELETE FROM  processing_node_status WHERE " << " host_id = " << _host_id << " AND node_id = " << current_thread_state->second.external_thread_id;
	sql->send_query();

	*sql << "INSERT INTO processing_node_status SET host_id = " << _host_id << ", node_id = " << current_thread_state->second.external_thread_id << ", "
		   "state='" << processing_state << "', current_processing_job_queue_id = " << processing_job_queue_id << ", current_processing_job_id = " << processing_job_id << ", current_output_event_id = " << current_thread_state->second.last_event_sql_id;
	sql->send_query();
}
void ns_image_server::register_host(ns_image_server_sql * sql, bool overwrite_current_entry, bool respect_existing_database_choice) {

	//log in to the server, register the host, get its database id, and update the filed ip address.

	std::string long_term_storage_ = "0";
	if (image_storage.long_term_storage_was_recently_writeable())
		long_term_storage_ = "1";
	sql->set_autocommit(false);
	sql->send_query("BEGIN");
	*sql << "SELECT id, ip, port, long_term_storage_enabled FROM hosts WHERE name='" << host_name << "' "
		"AND system_hostname='" << system_host_name << "' AND system_parallel_process_id=" << system_parallel_process_id();
			//"AND additional_host_description='" << additional_host_description << "'";
	if (overwrite_current_entry)
		*sql << " FOR UPDATE";

	ns_sql_result h;
	sql->get_rows(h);

	if (h.size() == 0){
		*sql<< "INSERT INTO hosts SET name='" << host_name << "', base_host_name='" << base_host_name << "', ip='" << host_ip << "', port='" << _dispatcher_port
			<< "', long_term_storage_enabled='" << long_term_storage_ << "', comments='', "
			<< "software_version_major=" << software_version_major() << ", software_version_minor=" << software_version_minor()
			<< ", software_version_compile=" << software_version_compile()<< ",database_used='" << *sql_database_choice
			<< "', time_of_last_successful_long_term_storage_write=0,"
			<< "system_hostname = '" << system_host_name << "',"
			<< "additional_host_description='"<< additional_host_description << "', system_parallel_process_id=" << system_parallel_process_id()<< ", dispatcher_refresh_interval = " << _dispatcher_refresh_interval;
		_host_id = sql->send_query_get_id();
	}
	else if (h.size() != 1)
		throw ns_ex() << (int)h.size() << " hosts found with current hostname!";
	else{
		_host_id = atoi(h[0][0].c_str());
		if (overwrite_current_entry){
			*sql << "UPDATE hosts SET ip='" << host_ip << "', port='" << _dispatcher_port
				<< "', long_term_storage_enabled='" << long_term_storage_ << "', "
				<< "software_version_major=" << software_version_major() << ", software_version_minor=" << software_version_minor()
				<< ", software_version_compile=" << software_version_compile()
				<< ", additional_host_description='"<< additional_host_description << "', system_parallel_process_id=" << system_parallel_process_id();
				//if the user has requested a database change, we update the record in the new database (to, for example, prevent infinite looping between databases).
				//on the initial startup, however, we want to respect the specification in the db, and switch databases if requested.
			if (!respect_existing_database_choice)
				*sql << ",database_used='" << *sql_database_choice << "'";
			*sql << ",system_hostname = '" << system_host_name << "',dispatcher_refresh_interval=" << _dispatcher_refresh_interval;
			*sql <<  " WHERE id='" << _host_id << "'";
			sql->send_query();
		}
	}
	sql->send_query("COMMIT");
}
void ns_image_server::update_device_status_in_db(ns_sql & sql) const{
	#ifndef NS_MINIMAL_SERVER_BUILD
	ns_image_server_device_manager::ns_device_name_list devices;
	image_server.device_manager.request_device_list(devices);
	for (unsigned int i = 0; i < devices.size(); ++i){
		sql << "UPDATE devices SET currently_scanning=" << (devices[i].currently_scanning?"1":"0")
			<< ", last_capture_start_time=" << devices[i].last_capture_start_time
			<< ", pause_captures=" << devices[i].paused << ", autoscan_interval="<<devices[i].autoscan_interval
			<< ", last_autoscan_time=" << devices[i].last_autoscan_time
			<< ", next_autoscan_time=" << devices[i].next_autoscan_time
			<< " WHERE name = '" << devices[i].name << "'";
		sql.send_query();
	}
	#endif
}
void ns_image_server::register_devices(const bool verbose, ns_image_server_sql * sql){
	#ifndef NS_MINIMAL_SERVER_BUILD


	ns_image_server_device_manager::ns_device_name_list connected_devices;
	device_manager.request_device_list(connected_devices);

	//we want to remember specified information about devices, so we download it from the db before clearing the db.
	*sql << "SELECT name, pause_captures,autoscan_interval, next_autoscan_time,preview_requested FROM devices WHERE host_id = " << image_server.host_id();
	ns_sql_result device_state;
	sql->get_rows(device_state);
	std::map<string,ns_device_summary> device_state_map;
	for (unsigned int i = 0; i < device_state.size(); i++){
		device_state_map[device_state[i][0]].paused = device_state[i][1]!="0";
		device_state_map[device_state[i][0]].autoscan_interval = atol(device_state[i][2].c_str());
		device_state_map[device_state[i][0]].next_autoscan_time = atol(device_state[i][3].c_str());
		device_state_map[device_state[i][0]].preview_capture_requested = atol(device_state[i][3].c_str());
	}
	unsigned long current_time(ns_current_time());
	//only specify new pause states for devices that are currently connected
	for(unsigned int i = 0; i < connected_devices.size(); i++){
		std::map<string,ns_device_summary>::iterator p(device_state_map.find(connected_devices[i].name));
		if (p==device_state_map.end())
			continue;
		try{
			device_manager.set_pause_state(connected_devices[i].name,p->second.paused);
			if (p->second.next_autoscan_time < current_time)
				 device_manager.set_autoscan_interval_and_balance(connected_devices[i].name,p->second.autoscan_interval,sql);
			else device_manager.set_autoscan_interval(connected_devices[i].name,p->second.autoscan_interval,p->second.next_autoscan_time);
		}
		catch(ns_ex & ex){
			image_server.register_server_event(ex,sql);
		}
	}

	*sql << "DELETE from devices WHERE host_id ='" << image_server.host_id() << "'";
	sql->send_query();

	std::string devices_registered;

	for (unsigned int i = 0; i < connected_devices.size(); i++){
		register_device(connected_devices[i],sql);
		devices_registered += connected_devices[i].name + ",";
	}

	for (unsigned int i = 0; i < connected_devices.size(); i++) {
		std::map<string, ns_device_summary>::iterator p(device_state_map.find(connected_devices[i].name));
		if (p == device_state_map.end())
			continue;
		try {
			if (p->second.preview_capture_requested) {
				*sql << "UPDATE devices SET preview_requested = " << device_state_map[device_state[i][0]].preview_capture_requested << " WHERE name='" << connected_devices[i].name << "'";
				sql->send_query();
			}
		}
		catch (ns_ex & ex) {
			image_server.register_server_event(ex, sql);
		}
	}
	ns_image_server_event ev("Registering devices ");
	ev << devices_registered;

	if (verbose)image_server.register_server_event(ev,sql);

	#endif
}
void ns_image_server::open_log_file(const ns_image_server::ns_image_server_exec_type & exec_type, ns_64_bit thread_id, const std::string & volatile_directory, const std::string & file_name, ofstream & out) {
	//open local logfile.
	std::string lname = volatile_directory;
	lname += DIR_CHAR_STR;
	lname += ns_dir::extract_filename_without_extension(file_name);
	string log_suffix;
	switch (exec_type) {
	case ns_image_server_type:
		log_suffix = "_server";
		break;
	case ns_worm_terminal_type:
		log_suffix = "_worm_browser";
		break;
	case ns_sever_updater_type:
		log_suffix = "_updater";
		break;
	default: throw ns_ex("ns_image_server::load_constants()::Unknown image server exec type!");
	}
	lname += log_suffix;
	if (thread_id > 0)
		lname += ns_to_string(thread_id);
	lname +=".txt";

	ns_dir::create_directory_recursive(volatile_directory);
	if (out.is_open())
		out.close();
	out.open(lname.c_str(), ios_base::app);
	if (out.fail())
		throw ns_ex("ns_image_server::Could not open log file (") << lname << ") for writing";
}

bool ns_to_bool(const std::string & s){
	return (s == "yes" || s == "Yes" || s == "YES" || s == "true" || s == "True" || s == "TRUE" ||
			s == "y" || s == "Y" || s == "1");
}


void ns_image_server::load_constants(const ns_image_server::ns_image_server_exec_type & exec_type, const std::string & ini_file_path, const bool reject_incorrect_fields){
	ns_ini constants;
	constants.reject_incorrect_fields(reject_incorrect_fields);
	//register ini file constants
	constants.start_specification_group(ns_ini_specification_group("*** Lifespan Machine Image Server Configuration File ***","This file allows you to specify various configuration options that will determine the behavior of all lifespan machine software running on this machine, including image acquisition servers, image analysis servers, and the worm browser."));

	constants.start_specification_group(ns_ini_specification_group("Important Configuration Parameters for this machine","These parameters need to be set correctly for the server to function."));
	constants.add_field("host_name", "ns_server","Each instance of the image acquisition and image analysis servers needs to have a unique name to identify it.  Thus, host_name should be set to a different value on every LINUX or Windows machine running the software.  Use a name that you'll recognize, such as linux_server_on_my_desk, bob, or lab_desktop_1");
	constants.add_field("long_term_storage_directory","","All image server software must be able to access a central directory used to store images.  This is often located on a NAS or institutional file server.  This directory should be mounted as a path on the machine running the server.  Set this parameter to the location of that directory");
	constants.add_field("results_storage_directory","","All image server software must be able to access a central directory used to store processed statistical data, including survival curves, descriptions of worm movement, etc.  This is often located on a NAS or institutional file server.  This directory should be mounted as a path on the machine running the server.  Set this parameter to the location of that directory");
	constants.add_field("volatile_storage_directory","","The image acquisition server and image analysis servers need to store temporary files on the local machine.  Set this parameter to the location of that directory; it can be anywhere you like.  For image acquisition servers, this is the local buffer for captured images pending transfer to the long term storage directory, so you should locate the directory on a drive with a couple hundred 100 GB of free space.");

	constants.start_specification_group(ns_ini_specification_group("Access to the central SQL database","These parameters need to be set to match the account set up on your central sql database, to allow the server to log in."));
	constants.add_field("central_sql_hostname","localhost","The IP address or DNS name of the computer running the central SQL server");
	constants.add_field("central_sql_username","image_server","The username with which the software should log into the central SQL server");
	constants.add_field("central_sql_password","","The password with which the software should log into the central SQL server");
	constants.add_field("central_sql_databases","image_server","The name of the database set up on the SQL server for the image server.  It's possible to specify multiple independent databases, each separated by a colon, but this is not needed in simple installations.");

	constants.start_specification_group(ns_ini_specification_group("Access to the local SQL database","Image acquisition servers use a local SQL database to store metadata pending its transfer to the central SQL database.  This lets acquisition servers continue to operate correctly through network disruptions, sql database crashes, etc.  These parameters need to be set to match the account set up on the machine's local sql database, to allow the server to log in"));
	constants.add_field("local_buffer_sql_hostname","localhost","The IP address or DNS name of the computer running the local SQL buffer.  This is only needed for image capture servers, and in all but exceptional cases should be set to localhost");
	constants.add_field("local_buffer_sql_username","image_server","The username with which the software should log into the local SQL buffer");
	constants.add_field("local_buffer_sql_database","image_server_buffer","The name of the local SQL buffer database");
	constants.add_field("local_buffer_sql_password","","The password with which the software should log into the local SQL buffer");

	constants.start_specification_group(ns_ini_specification_group("Image Acquisition Server Settings ","These settings control the behavior of image acquisition servers"));
	constants.add_field("act_as_image_capture_server","no","Should the server try to control attached scanners? (yes / no)");
	constants.add_field("device_capture_command","/usr/local/bin/scanimage","the path to the SANE component scanimage, with which scans can be started");
	constants.add_field("device_list_command","/usr/local/bin/sane-find-scanner","the path to the SANE component sane-find-scanners, with which scanners can be identified");
	constants.add_field("device_barcode_coordinates","-l 0in -t 10.3in -x 8in -y 2in", "The coordinates of the barcode adhered to the surface of each scanner");
	constants.add_field("simulated_device_name", ".","For software debugging, an image acquisition server can simulate an attached device");
	constants.add_field("device_names", "", "This can be used to explicitly specify scanner names on an image acquisition server.  These should be detected just fine automatically, and so in most cases this field can be left blank");
	constants.add_field("output_files_with_all_read_permissions", "yes", "Generate files with 755 access permissions",true);
	constants.add_field("output_file_group_permissions", "ns_readwrite", "The default file permissions for all files written by the lifespan machine.  Set to none, read, or readwrite.");

	constants.start_specification_group(ns_ini_specification_group("Image Analysis Server Settings ","These settings control the behavior of image processing servers"));
	constants.add_field("allow_multiple_processes_per_system", "no", "By default, only one ns_image_server process can run on each system.  If allow_multiple_process_per_system is set to yes then this constraint is removed.  In that case, be certain to specify a unique value of additional_host_description at the commandline for each ns_image_server process run on the same system, to prevent cache, image, and database corruption.");
	constants.add_field("number_of_times_to_check_empty_processing_job_queue_before_stopping", "0", "The number of times the image server should re-check an empty processing job queue before giving up and shutting down.  Useful when running on a HPC cluster.  A value of 0 indicates the server never should shut down for this reason.");
	constants.add_field("act_as_processing_node","yes","Should the server run image processing jobs requested by the user via the website? (yes / no)");
	constants.add_field("nodes_per_machine", "1","A single computer can run multiple copies of the image processing server simultaneously, which allows many jobs to be processed in parallel.  Set this value to the number of parallel servers you want to run on this machine.  This can usually be set to the number of physical cores on the machine's processor, or the number of GB of RAM on the machine; whichever is smaller.");
	constants.add_field("hide_window","no","On windows, specifies whether the server should start minimized.  (yes / no )");
	constants.add_field("compile_videos","no","Should the server process videos? (yes / no)");
	constants.add_field("video_compiler_filename","./x264.exe","Path to the x264 transcoder program required to generate videos.  Only needed on image processing servers.  If you don't have this, set compile_videos to no");
	constants.add_field("video_ppt_compiler_filename","./ffmpeg.exe","Path to the ffmpeg transcoder required to generate videos. Only needed on image processing servers.  If you don't have this, set compile_videos to no");
	constants.add_field("halt_on_new_software_release","no", "Should the server shut down if a new version of the software is detected running on the cluster? (yes / no)");
	constants.add_field("latest_release_path","image_server_software/image_server_win32.exe", "Image acquisition servers can be set to automatically update if new versions of the software is identified as running on the cluster.  This is the path name where the new software can be found.");
	constants.add_field("run_autonomously","yes","should the server automatically poll the MySQL database for new scans/jobs (yes) or should it only do this when a command is received from an external source (no).  Most configurations set this to yes.");

	constants.start_specification_group(ns_ini_specification_group("Other Settings","These settings control the behavior of image acquisition and image processing servers"));
	constants.add_field("verbose_debug_output","false","If this option is set to true, the image server and worm browser will generate detailed debug information while running various steps of image acquisition and image processing.  An file containing this output will be written to the volatile_storage directory.");
	constants.add_field("dispatcher_refresh_interval","6000","How often should image acquisition servers check for pending scans?  (in milliseconds).  Also specifies how often analysis servers will check for new jobs.");
	constants.add_field("mail_path", "/bin/mail","Each copy of the image server running on the cluster occasionally checks for errors occurring in other nodes, for example missed scans or low disk space.  If problems are discovered, the image server can send users an email notifying them of the problem.  To activate this feature, set mail_path to the POSIX mail program on the local system.");
	constants.add_field("mail_from", "Local User <user@localhost>", "The 'from' field to provide the mailer daemon when sending emails.");
	constants.add_field("ethernet_interface","","This field should be left blank if you want the server to access the network through the default network interface.  If you have multiple network interfaces and want to use a specific one, specify it here.");
	constants.add_field("dispatcher_port","1043","Image acquisition and image processing servers open a TCP/IP port on the local machine through which control commands can be sent.  dispatcher_port determines the specific port on which the dispatcher should listen for remote requests.");
	constants.add_field("server_crash_daemon_port","1042",": To provide some protection against server crashes, an image acquisition server running under linux launches a persistent second thread that checks whether the image acquisition server has crashed.  In the event of a crash, the crash_daemon launches a new instance of the image acquisition server.  Often, the crashed copy retains a lock its TCP/IP port, requiring that the crash daemon use a second port instead, specified here.");
	constants.add_field("server_timeout_interval","300","How long should a server wait before giving up on a dead network connection (in seconds)");
	constants.add_field("log_filename","image_server_log.txt","Image acquisition and image processing servers keep a log file in the central SQL database.  However, to help diagnose crashes, a text file containing the same log information is stored on the local machine.  The log file is stored in the directory specified by the volatile_storage_directory option (described above), and its filename is specified by here.");
	constants.add_field("maximum_memory_allocation_in_mb","3840","Movement analaysis benefits from access to multiple gigabytes of RAM.  This value should be set to approximately the size of system memory.  Larger values will cause sporadic crashes during movement analysis.");
	constants.add_field("verbose_local_storage_space_reporting", "false", "Regularly report the disk space available currently available to the host");
	constants.add_field("remember_devices_across_sessions", "true", "Maintain a list of scanners to reduce barcode reading");

	ns_ini terminal_constants;
	terminal_constants.reject_incorrect_fields(reject_incorrect_fields);
	terminal_constants.start_specification_group(ns_ini_specification_group("*** Worm Browser Configuration File ***","This file allows you to specify various configuration options that will determine the behavior of the worm browser software run on this machine.  It's behavior is also influenced by the settings in the ns_image_server configuration file."));
	terminal_constants.add_field("max_width","1024","The maximum width of the worm browser window");
	terminal_constants.add_field("max_height","768","The maximum height of the worm browser window");
	terminal_constants.add_field("hand_annotation_resize_factor","2","How many times should images of worms be shrunk before being displayed when looking at storyboards?  Larger values result in smaller worms during by hand annotation of worms");
	terminal_constants.add_field("mask_upload_database","image_server","The SQL database in which image masks should be stored.  This is usually set to the value specified for the central_sql_databases option in the ns_image_server.ini file");
	terminal_constants.add_field("mask_upload_hostname","myhost","The host name (e.g bob or lab_desktop_1) of the server where sample region masks should be uploaded. This is usually set to the value of the host_name option set the acquisition server's ns_image_server.ini file");
	terminal_constants.add_field("verbose_debug_output","false","If this option is set to true, the image server and worm browser will generate detailed debug information while running various steps of image acquisition and image processing.  An file containing this output will be written to the volatile_storage directory.");
	terminal_constants.add_field("window_scale_factor","1","On high resolution screens, set this to a value greater than one to rescale all imagery.");
	string ini_directory,ini_filename;
	try {
		//look for the ini file in the current directory
		if (!ini_file_path.empty()) {
			if (!ns_dir::file_exists(ini_file_path))
				throw ns_ex("Could not load ini file specified at ") << ini_file_path;
			ini_filename = ini_file_path;
			constants.load(ini_filename);
			ini_directory = ns_dir::extract_path(ini_file_path);
		}
		else if (ns_dir::file_exists("./ns_image_server.ini")) {
			ini_filename = "./ns_image_server.ini";
			constants.load("./ns_image_server.ini");
			ini_directory = "./";
		}
		//if not found, look for a forwarding file
		else if (ns_dir::file_exists("./ns_image_server_ini.forward")) {
			ifstream in("./ns_image_server_ini.forward");
			std::string ini_filename;
			in >> ini_filename;
			in.close();
			ini_directory = ns_dir::extract_path(ini_filename);
			constants.load(ini_filename);
		}
		else if (ns_dir::file_exists("c:\\ns_image_server_ini.forward")) {
			ifstream in("c:\\ns_image_server_ini.forward");
			std::string ini_filename;
			in >> ini_filename;
			in.close();
			ini_directory = ns_dir::extract_path(ini_filename);
			constants.load(ini_filename);
		}
		//if not found, look in the default directory location
		else {
			ini_filename = NS_INI_PATH;
			constants.load(NS_INI_PATH);
			ini_directory = ns_dir::extract_path(NS_INI_PATH);
		}

		if (exec_type == ns_worm_terminal_type) {
			ini_filename = ini_directory + DIR_CHAR_STR + "ns_worm_browser.ini";
			terminal_constants.load(ini_filename);
			max_terminal_window_size.x = atol(terminal_constants["max_width"].c_str());
			max_terminal_window_size.y = atol(terminal_constants["max_height"].c_str());
			terminal_hand_annotation_resize_factor = atol(terminal_constants["hand_annotation_resize_factor"].c_str());
			//mask_upload_database = terminal_constants["mask_upload_database"];
			//mask_upload_hostname = terminal_constants["mask_upload_hostname"];
			if (terminal_constants.field_specified("verbose_debug_output") && terminal_constants["verbose_debug_output"] == "true") {
				_verbose_debug_output = true;
			}
			if (terminal_constants.field_specified("window_scale_factor")) {
				_terminal_window_scale_factor = atof(terminal_constants["window_scale_factor"].c_str());
			}
		}
		else {
			if (constants.field_specified("verbose_debug_output") && ns_to_bool(constants["verbose_debug_output"]))
				_verbose_debug_output = true;
		}

		{

			string sql_server_tmp(constants["central_sql_hostname"]);
			sql_server_addresses.resize(0);
			//split the server names into an ordered list.
			//the earliest addresses will be tried first.
			if (sql_server_tmp.size() != 0) {
				sql_server_addresses.resize(1);
				for (unsigned int i = 0; i < sql_server_tmp.size(); i++) {

					if (sql_server_tmp[i] == ';' || isspace(sql_server_tmp[i]))
						sql_server_addresses.resize(sql_server_addresses.size() + 1);
					else
						(*sql_server_addresses.rbegin()) += sql_server_tmp[i];
				}
			}
		}
		{
			string sql_db_tmp(constants["central_sql_databases"]);
			//split the server database options into an ordered list.
			//the earliest addresses will be tried first.
			possible_sql_databases.resize(0);
			if (sql_db_tmp.size() != 0) {
				possible_sql_databases.resize(1);
				for (unsigned int i = 0; i < sql_db_tmp.size(); i++) {

					if (sql_db_tmp[i] == ';' || isspace(sql_db_tmp[i]))
						possible_sql_databases.resize(possible_sql_databases.size() + 1);
					else
						(*possible_sql_databases.rbegin()) += sql_db_tmp[i];
				}
			}
		}
		//set default database
		switch_to_default_db();

		sql_user = constants["central_sql_username"];
		sql_pwd = constants["central_sql_password"];

		local_buffer_db = constants["local_buffer_sql_database"];
		local_buffer_ip = constants["local_buffer_sql_hostname"];
		local_buffer_pwd = constants["local_buffer_sql_password"];
		local_buffer_user = constants["local_buffer_sql_username"];
		if (constants.field_specified("remember_devices_across_sessions")) {
			remember_barcodes_across_sessions_ = ns_to_bool(constants["remember_devices_across_sessions"]);
		}
		else remember_barcodes_across_sessions_ = true;

		if (constants.field_specified("verbose_local_storage_space_reporting")) {
			verbose_disk_storage_reporting = ns_to_bool(constants["verbose_local_storage_space_reporting"]);
		}
		if (constants.field_specified("output_files_with_all_read_permissions") && constants.field_specified("output_file_group_permissions"))
			throw ns_ex("In ns_image_server.ini, either output_files_with_all_read_permissions or output_file_group_permissions can be specified but not both.");

		if (constants.field_specified("output_files_with_all_read_permissions")) {
			output_file_group_permissions = ns_fp_read;
		}
		else output_file_group_permissions = ns_fp_none;

		if (constants.field_specified("output_file_group_permissions")) {
			const std::string& fp_spec = constants["output_file_group_permissions"];
			if (fp_spec == "none")
				output_file_group_permissions = ns_fp_none;
			else if (fp_spec == "read")
				output_file_group_permissions = ns_fp_read;
			else if (fp_spec == "readwrite")
				output_file_group_permissions = ns_fp_readwrite;
			else throw ns_ex("output_file_group_permissions must be set to one of three values: none, read, or readwrite.");
		}
		//now implement the permissions settings specificied in the ini file
		#ifndef _WIN32
		switch (output_file_group_permissions) {
			
			case ns_fp_none: umask(0700); break;
			case ns_fp_read: umask(0755); break;
			case ns_fp_readwrite: umask(0775); break;
			default: throw ns_ex("Unknown value for output_file_group_permissions: ") << (int)output_file_group_permissions;
			}
		#endif
		switch (output_file_group_permissions) {
		      
			case ns_fp_none:
				image_storage.set_file_permissions_readable_by_other(ns_dir::ns_no_special_permissions); break;
			case ns_fp_read:
				image_storage.set_file_permissions_readable_by_other(ns_dir::ns_group_read); break;
			case ns_fp_readwrite:
				image_storage.set_file_permissions_readable_by_other(ns_dir::ns_group_readwrite); break;
			default: throw ns_ex("Unknown value for output_file_group_permissions: ") << (int)output_file_group_permissions;
			
		}

		number_of_times_to_check_empty_job_queue_before_stopping = atol(constants["number_of_times_to_check_empty_processing_job_queue_before_stopping"].c_str());

		for (unsigned int i = 0; i < possible_sql_databases.size(); i++){
			if (local_buffer_db == possible_sql_databases[i])
				throw ns_ex("The local sql database cannot have the same name as any of the central databases.");
		}

		if (host_name.size() > 15)
			throw ns_ex("Host names must be 15 characters or less.  \"") << host_name << "\" is " << host_name.size() << " characters.";
		base_host_name			= constants["host_name"];
		host_name				= base_host_name;

		maximum_number_of_processing_threads_ = atol(constants["nodes_per_machine"].c_str());
		volatile_storage_directory	= ns_dir::format_path(constants["volatile_storage_directory"]);
		_dispatcher_port		= atoi(constants["dispatcher_port"].c_str());
		_act_as_an_image_capture_server = ( ns_to_bool(constants["act_as_image_capture_server"]));
		_compile_videos = ( ns_to_bool(constants["compile_videos"]));

		long_term_storage_directory	= ns_dir::format_path(constants["long_term_storage_directory"]);
		results_storage_directory	= ns_dir::format_path(constants["results_storage_directory"]);
		if (constants.field_specified("maximum_memory_allocation_in_mb")){
			_maximum_memory_allocation_in_mb = atol(constants["maximum_memory_allocation_in_mb"].c_str());
			if (_maximum_memory_allocation_in_mb < 1024)
				throw ns_ex("maximum_memory_allocation_in_mb is set to an extremely low value: 1024.  The lifespan machine will not function properly with less than one GB of RAM.");
		}else _maximum_memory_allocation_in_mb = 1024*4;

		_allow_multiple_processes_per_system = ns_to_bool(constants["allow_multiple_processes_per_system"]);
		if (_act_as_an_image_capture_server && _allow_multiple_processes_per_system)
			throw ns_ex("Multiple image capture servers cannot be allowed to run on the same system.  Please unset either act_as_an_image_capture_server or allow_multiple_processes_per_system in ns_image_server.ini");

		_capture_command		= constants["device_capture_command"];
		_server_crash_daemon_port	= atol(constants["server_crash_daemon_port"].c_str());

		_dispatcher_refresh_interval = atoi(constants["dispatcher_refresh_interval"].c_str())/1000;
		if (_dispatcher_refresh_interval == 0) _dispatcher_refresh_interval = 1;
		_server_timeout_interval = atol(constants["server_timeout_interval"].c_str());
		_log_filename = constants["log_filename"] + "_" + image_server.system_host_name;
		_hide_window = ns_to_bool(constants["hide_window"]);
		_run_autonomously = ns_to_bool(constants["run_autonomously"]);
		_scanner_list_command	= constants["device_list_command"];
		_scanner_list_coord = constants["device_barcode_coordinates"];
		_act_as_processing_node = ns_to_bool(constants["act_as_processing_node"]);
#ifdef NS_ONLY_IMAGE_ACQUISITION
if (_act_as_processing_node)
	throw ns_ex("This software was compiled with the NS_ONLY_IMAGE_ACQUISITION flag set, and so cannot process images.  In the image_server.ini file, act_as_processing_node must be set to false.");
#endif
	_halt_on_new_software_release = ns_to_bool(constants["halt_on_new_software_release"]);
		_video_compiler_filename = constants["video_compiler_filename"];
		_video_ppt_compiler_filename = constants["video_ppt_compiler_filename"];
		_latest_release_path = constants["latest_release_path"];

		_mail_from = constants["mail_from"];


	//	_local_exec_path = constants["local_exec_path"];
		std::string specified_simulated_device_name(constants["simulated_device_name"]);
		if (specified_simulated_device_name== ".")
			specified_simulated_device_name = "";
		_simulated_device_name = specified_simulated_device_name;
		for (unsigned int i = 0; i < _simulated_device_name.size(); i++){
			if (!isalpha(_simulated_device_name[i]) && !isdigit(_simulated_device_name[i]))
				_simulated_device_name[i] = '_';
		}
		if (specified_simulated_device_name.length() > 23)
			throw ns_ex("Simulated device name is too large.");
		_maximum_allowed_local_scan_delay = 0;
		_maximum_allowed_remote_scan_delay = 0;
		if (_simulated_device_name.size() < 2)
			_simulated_device_name.clear();
		_mail_path= constants["mail_path"];

		if (_compile_videos){
			if (!ns_dir::file_exists(image_server.video_compiler_filename()))
				throw ns_ex ("ns_image_server::Specified video complier filename (") << _video_compiler_filename << ") does not exist.";
			try{
				ns_external_execute_options opt;
				opt.binary = true;
				ns_external_execute exec;
				exec.run(image_server.video_compiler_filename(), "", opt);
				exec.release_io();
			}
			catch(ns_ex & ex){
				throw ns_ex ("ns_image_server::Specified video complier filename (") << _video_compiler_filename << ") cannot be executed: " << ex.text();
			}
			catch(...){
				throw ns_ex ("ns_image_server::Specified video complier filename (") << _video_compiler_filename << ") cannot be executed.";
			}
		}
		#ifndef _WIN32
			//omp_set_num_threads(_number_of_simultaneous_threads);
			//omp_set_dynamic(_number_of_simultaneous_threads);
		#endif
	}
	catch(ns_ex & ex){
		ns_ex ex2("Problem identified in ");
		ex2 << ini_filename << ":" << ex.text();
		throw ns_ex(ex2);
	}

		//	cerr << "LOOP";
	//set output directories for ther server
	image_storage.set_directories(volatile_storage_directory + DIR_CHAR_STR + system_host_name, long_term_storage_directory);

	ns_dir::create_directory_recursive(volatile_storage_directory);

	results_storage.set_results_directory(results_storage_directory);
	//cerr << "SNAP";
	//cerr << "GFRP";
	if (host_name.size() == 0)
		throw ns_ex("No host name specified in ini file!");

	open_log_file(exec_type, 0, volatile_storage_directory, _log_filename, event_log);


	switch(exec_type){
		case ns_image_server_type: _cache_subdirectory = "server_cache"; break;
		case ns_worm_terminal_type: _cache_subdirectory = "terminal_cache"; break;
		case ns_sever_updater_type: _cache_subdirectory = "updater_cache"; break;
		default: throw ns_ex("ns_image_server::load_constants()::Unknown image server exec type!");

	}

	//Obtain local network information
	_ethernet_interface = constants["ethernet_interface"];


	ns_socket socket;
	host_ip = socket.get_local_ip_address(_ethernet_interface);
}
void ns_image_server::pause_host(){
	image_server.server_pause_status = true;
	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	sql() << "UPDATE hosts SET pause_requested=1 WHERE id="<<image_server.host_id();
	sql().send_query();
	sql.release();
}
void ns_image_server::pause_host(ns_image_server_sql * sql)const {
	image_server.server_pause_status = true;
	*sql << "UPDATE hosts SET pause_requested=1 WHERE id=" << image_server.host_id();
	sql->send_query();
}

void ns_image_server::get_alert_suppression_lists(std::vector<std::string> & devices_to_suppress, std::vector<std::string> & experiments_to_suppress,ns_image_server_sql * sql){
		*sql << "SELECT v FROM constants WHERE k='supress_alerts_for_device'";
		ns_sql_result res;
		sql->get_rows(res);
		if (res.size() == 0)
			image_server.set_cluster_constant_value("supress_alerts_for_device",".",sql);
		for (unsigned int i = 0; i < res.size(); i++){
			if (res[i][0] == ".") continue;
			devices_to_suppress.push_back(res[i][0]);
		}

		*sql << "SELECT v FROM constants WHERE k='supress_alerts_for_experiment'";
		sql->get_rows(res);
		if (res.size() == 0)
			image_server.set_cluster_constant_value("supress_alerts_for_experiment",".",sql);

		for (unsigned int i = 0; i < res.size(); i++){
			if (res[i][0] == ".") continue;
			experiments_to_suppress.push_back(res[i][0]);
		}
}

#ifdef _WIN32
void ns_image_server::set_console_window_title(const string & title) const{
	string t(title);
	if (t.size() == 0)
			t="ns_image_server";
	SetConsoleTitle(t.c_str());
}
#endif


void ns_image_server::shut_down_host(){
	exit_lock.wait_to_acquire(__FILE__,__LINE__);
	exit_has_been_requested = true;
	exit_lock.release();
	//shut down the dispatcher
	if (!send_message_to_running_server(NS_QUIT))
		throw ns_ex("Could not submit shutdown command to ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";

}

#ifndef NS_ONLY_IMAGE_ACQUISITION
void ns_svm_model_specification::write_statistic_ranges(const std::string & filename, bool write_all_features) const{


	ofstream out(filename.c_str());
	if (out.fail())
		throw ns_ex("ns_detected_worm_stats::Could not open range file ") << filename;
	out << "Feature\tMin\tMax\tAvg\tSTD\tWorm Avg\n";
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		if (included_statistics.size() <= i || statistics_ranges.size() <= i) throw ns_ex("Yikes");
		if (write_all_features || (included_statistics[i]!=0 && statistics_ranges[i].specified)){
			out << i <<"\t" << statistics_ranges[i].min << "\t" << statistics_ranges[i].max << "\t"
			<< statistics_ranges[i].avg << "\t" << statistics_ranges[i].std
			<< "\t" << statistics_ranges[i].worm_avg << "\t\t\\\\" << ns_classifier_label((ns_detected_worm_classifier)i) <<"\n";
		}
	}
		out.close();
}
void ns_svm_model_specification::read_statistic_ranges(const std::string & filename){
	if (statistics_ranges.size() == 0)
		statistics_ranges.resize((unsigned int)ns_stat_number_of_stats);

	ifstream in(filename.c_str());
	if (in.fail())
		throw ns_ex("ns_detected_worm_stats::Could not open range file ") << filename;
	while(true){
		char a(in.get());
		if (in.fail())
			throw ns_ex("ns_detected_worm_stats::Error in ") << filename << " Line " << 0;
		if (a == '\n')
			break;
	}
	std::string temp;
	for (unsigned int i = 0; i < statistics_ranges.size(); i++){
		statistics_ranges[i].specified = false;
	}
	unsigned long line_id(1);
	while(true){
		line_id++;
		unsigned long feature_id;
		in >> feature_id;
		if (in.fail())
			break;
		if (feature_id >= statistics_ranges.size())
			throw ns_ex("Invalid feature specified:" ) << feature_id;
		statistics_ranges[feature_id].specified = true;
		in >> statistics_ranges[feature_id].min;
		if (in.fail())
			throw ns_ex("ns_detected_worm_stats::Error in ") << filename << " Line " << line_id;
		in >> statistics_ranges[feature_id].max;
		if (in.fail())
		if (in.fail())
			throw ns_ex("ns_detected_worm_stats::Error in ") << filename << " Line " << line_id;
		in >> statistics_ranges[feature_id].avg;
		if (in.fail())
			throw ns_ex("ns_detected_worm_stats::Error in ") << filename << " Line " << line_id;
		in >> statistics_ranges[feature_id].std;
		if (in.fail())
			throw ns_ex("ns_detected_worm_stats::Error in ") << filename << " Line " << line_id;
		in >> statistics_ranges[feature_id].worm_avg;
		if (in.fail())
			throw ns_ex("ns_detected_worm_stats::Error in ") << filename << " Line " << line_id;
		std::getline(in,temp);
		if (in.fail())
			throw ns_ex("ns_detected_worm_stats::Error in ") << filename << " Line " << line_id;
	}
	in.close();
}


void ns_svm_model_specification::read_included_stats(const std::string & filename){
	ifstream in(filename.c_str());
	if (in.fail())
		throw ns_ex("ns_model_specification::read_included_stats()Could not load file ") << filename;
	unsigned long statistics_used(0);
	string stat_str;
	int state(0);
	bool no_flags_found(false);
	while(true){
		char a;
		a = in.get();
		if (in.fail())
			break;
		if (state == 0){
			if (!isspace(a))
				stat_str+=a;
			else{
				if (stat_str.size() == 0)
					continue;  //skip leading whitespace

				if (!no_flags_found) {  //special flags can be specified in the included statistics flag.
										//if one is found, set the flag and stop looking for any range info.
					if (stat_str == "ACCEPT_ALL_OBJECTS") {
						//cerr << stat_str << "\n";
						this->flag = ns_svm_model_specification::ns_accept_all_objects;
						return;
					}
					no_flags_found = true;
				}
				int stat_specified = atol(stat_str.c_str());  //we've read in the entire number
				if (stat_specified >= included_statistics.size())
					throw ns_ex("ns_model_specification::Invalid statistic specified ") << stat_specified;
				included_statistics[stat_specified] = 1;
				statistics_used++;
				stat_str.resize(0);
				state = (a == '\n')?0:1;
			}
		}
		else if (state == 1){
			if (a == '\n') state = 0; //keep going until we find the newline
			continue;
		}
	}
}
void ns_svm_model_specification::read_excluded_stats(const std::string & filename){
	std::vector<unsigned long> excluded_statistics((unsigned int)ns_stat_number_of_stats,0);

	ifstream in(filename.c_str());
	if (in.fail())
		throw ns_ex("ns_model_specification::read_excluded_stats()Could not load file ") << filename;
	while(true){
		int a = 0;
		in >> a;
		if (in.fail())
			break;
		if ((unsigned int)a >= excluded_statistics.size())
			throw ns_ex("ns_model_specification::Invalid statistic exclusion: ") << a;
	//	cerr << "ns_model_specification::Excluding statistic " << ns_classifier_label((ns_detected_worm_classifier)a) << "\n";
		excluded_statistics[a] = 1;
	}
	included_statistics.resize((unsigned int)ns_stat_number_of_stats);
	for (unsigned int i = 0; i < included_statistics.size(); i++)
		included_statistics[i] = !excluded_statistics[i];
}
void ns_principal_component_transformation_specification::read(const std::string & filename){
	pc_vectors.resize(0);
	ifstream in(filename.c_str());
	if (in.fail())
		return;

	pc_vectors.resize((int)ns_stat_number_of_stats,std::vector<double>((int)ns_stat_number_of_stats,0));

	std::string temp;
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		for (unsigned int j = 0; j < (unsigned int)ns_stat_number_of_stats; j++){
			in >> pc_vectors[j][i];
			if (in.fail())
				throw ns_ex("ns_principal_component_transformation_specification::Error in file ") << filename;
		}
	}
	in.close();
}
#endif

std::string ns_image_server::capture_preview_parameters(const ns_capture_device::ns_device_preview_type & type,ns_sql & sql){
	string transparency_default("--mode=Gray --format=tiff --source=\"TPU8X10\" --resolution=300 --depth=8 -l 0in -t 0in -x 8.5in -y 10.5in"),
		   reflective_default("--mode=Gray --format=tiff --source=\"Flatbed\" --resolution=300 --depth=8 -l 0in -t 0in -x 8.5in -y 11.7in");

	switch(type){
		case ns_capture_device::ns_no_preview:
			throw ns_ex("ns_image_server::capture_preview_parameters():Requesting capture command for a no_preview request.");
		case ns_capture_device::ns_transparency_preview:
			return get_cluster_constant_value("transparency_preview_parameters",transparency_default,&sql);
		case ns_capture_device::ns_reflective_preview:
			return get_cluster_constant_value("reflective_preview_parameters",transparency_default,&sql);
		default: throw ns_ex("ns_image_server::capture_preview_parameters():Unknown preview request type: ) ") << (unsigned int)type;
	}
}
void ns_image_server::clear_old_server_events(ns_sql & sql){
	image_server.register_server_event(ns_image_server_event("Cleaning up server event log."),&sql);
	sql << "DELETE FROM host_event_log WHERE time < " << ns_current_time() - 14*24*60*60;
	sql.send_query();
	sql.send_query("COMMIT");
}


void ns_image_server::add_subtext_to_current_event(const char * str, ns_image_server_sql * sql,bool suppress_display,const ns_64_bit impersonate_using_internal_thread_id) const {
	if (sql != 0) {
		std::map<ns_64_bit, ns_thread_output_state>::iterator current_thread_state = get_current_thread_state_info(impersonate_using_internal_thread_id);
		if (sql->query().size() > 0) {
			cerr << "Warning! add_subtext_to_current_event() was passed an sql object with a command left in its query buffer: " << sql->query().size() << "\n";
			sql->clear_query();
		}
		*sql << "UPDATE " << sql->table_prefix() << "host_event_log SET sub_text = CONCAT(sub_text,'" << sql->escape_string(str) << "') WHERE id = " << current_thread_state->second.last_event_sql_id;
		sql->send_query();
	}
	if (!suppress_display)
		cout << str;
}

void  ns_image_server::add_subtext_to_current_event(const ns_image_server_event & s_event, ns_image_server_sql * sql,bool display_date,bool suppress_display, const ns_64_bit impersonate_using_internal_thread_id) const {
	if (!display_date)
		add_subtext_to_current_event(s_event.text(), sql,suppress_display, impersonate_using_internal_thread_id);
	else {
		std::string time_str = ns_format_time_string(ns_current_time());
		time_str+=std::string(": ");
		time_str += s_event.text();
		time_str += std::string("\n");
		add_subtext_to_current_event(time_str, sql,suppress_display, impersonate_using_internal_thread_id);
	}
}
ns_64_bit ns_image_server::register_server_event(const ns_register_type type,const ns_image_server_event & s_event)const{
	if (s_event.type()==ns_ts_error || s_event.type() == ns_ts_debug){
			register_server_event_no_db(s_event);
			return 1;
	}

	ns_acquire_for_scope<ns_image_server_sql> sql;

	if (type==ns_register_in_central_db_with_fallback || type == ns_register_in_central_db){
		try{
			sql.attach(new_sql_connection_no_lock_or_retry(__FILE__,__LINE__));
		}
		catch(ns_ex & ex){
			if (type!=ns_register_in_central_db_with_fallback)
				throw;

			sql.attach(new_local_buffer_connection_no_lock_or_retry(__FILE__,__LINE__));
			register_server_event(ex,&sql());
		}

		if (sql.is_null())
			try {
				sql.attach(new_local_buffer_connection_no_lock_or_retry(__FILE__, __LINE__));
		}
		catch (ns_ex & ex) {
			throw ns_ex("Error when falling back to local sql buffer: ") << ex.text() << ns_sql_fatal;
		}
	}
	else {
		try {
			sql.attach(new_local_buffer_connection_no_lock_or_retry(__FILE__, __LINE__));
		}
		catch (ns_ex & ex) {
			throw ns_ex("Error when connecting to local sql buffer: ") << ex.text() << ns_sql_fatal;
		}
	}
	try{
		return register_server_event(s_event,&sql());
		sql.release();
	}
	catch(...){
		sql.release();
		throw;
	}
}

ns_64_bit ns_image_server::register_server_event(const ns_register_type type,const ns_ex & ex)const {
		ns_image_server_event s_event(ex.text());
		if (ex.type() == ns_sql_fatal) s_event << ns_ts_sql_error;
		else s_event << ns_ts_error;
		return register_server_event(type,s_event);
}



void ns_image_server::register_server_event_no_db(const ns_image_server_event & s_event,bool no_double_endline)const{

	std::map<ns_64_bit, ns_thread_output_state>::iterator current_thread_state = get_current_thread_state_info();
	current_thread_state->second.last_event_sql_id = 0;
	ns_acquire_lock_for_scope lock(server_event_lock,__FILE__,__LINE__);
	if (s_event.text().size() != 0){
		if (!no_double_endline)
			cerr <<"\n";
		s_event.print(cerr);
		//cerr << "\n";
		s_event.print(event_log);
		if (!no_double_endline)
			event_log << "\n";
		event_log.flush();
	}
	lock.release();
}

ns_64_bit ns_image_server::register_server_event(const ns_ex & ex, ns_image_server_sql * sql, const bool no_display)const{
	ns_image_server_event s_event(ex.text());
	if (ex.type() == ns_sql_fatal) s_event << ns_ts_sql_error;
	else s_event << ns_ts_error;
	return register_server_event(s_event,sql,no_display);

}
void ns_image_server::set_main_thread_internal_id() {
	_main_thread_internal_id = ns_thread::current_thread_id();
}
std::map<ns_64_bit, ns_thread_output_state>::iterator ns_image_server::get_current_thread_state_info(const ns_64_bit thread_internal_id_to_impersonate) const {
	ns_acquire_lock_for_scope lock(server_event_lock, __FILE__, __LINE__);
	ns_64_bit system_thread_id;
	if (thread_internal_id_to_impersonate == 0)
		system_thread_id = ns_thread::current_thread_id();
	else
		system_thread_id = thread_internal_id_to_impersonate;

	std::map<ns_64_bit, ns_thread_output_state>::iterator current_thread_state = thread_states.find(system_thread_id);

	if (current_thread_state == thread_states.end()) {
		current_thread_state = thread_states.insert(std::map<ns_64_bit, ns_thread_output_state>::value_type(system_thread_id, ns_thread_output_state())).first;
		current_thread_state->second.internal_thread_id = system_thread_id;
		current_thread_state->second.external_thread_id = 0;
	}
	lock.release();
	return current_thread_state;
}

void ns_image_server::log_current_thread_output_in_separate_file() const {
	std::map<ns_64_bit, ns_thread_output_state>::iterator current_thread_state = get_current_thread_state_info();

	ns_acquire_lock_for_scope lock(server_event_lock, __FILE__, __LINE__);
	try {
		if (current_thread_state->second.external_thread_id == 0) {
			if (current_thread_state->second.internal_thread_id == image_server.main_thread_internal_id())
				throw ns_ex("Asking to register main thread output in separate file!");
			current_thread_state->second.external_thread_id = max_external_thread_id;
			max_external_thread_id++;
		}
		if (current_thread_state->second.thread_specific_logfile != 0) {
			lock.release();
			return;
		}
		current_thread_state->second.thread_specific_logfile = new ofstream;
		open_log_file(ns_image_server_type, current_thread_state->second.external_thread_id, volatile_storage_directory, _log_filename, *(current_thread_state->second.thread_specific_logfile));
	}
	catch (ns_ex & ex) {
		cerr << ex.text() << "\n";
		current_thread_state->second.thread_specific_logfile = 0;
	}
	lock.release();
}

ns_64_bit ns_image_server::register_server_event(const ns_image_server_event & s_event, ns_image_server_sql * sql,const bool no_display) const{
	try{

		if (!no_display){

			ns_acquire_lock_for_scope lock(server_event_lock, __FILE__, __LINE__);
			if (s_event.text().size() != 0){
				s_event.print(cerr);
				cerr << "\n";
			}
			lock.release();
		}


		std::map<ns_64_bit, ns_thread_output_state>::iterator current_thread_state = get_current_thread_state_info();

		ns_64_bit event_id(0);
		//if the event is to be logged and doesn't involve an sql connection error, connect to the db and log it.
		if (s_event.log && s_event.type() != ns_ts_sql_error){
			try{
				sql->clear_query();
				*sql << "INSERT INTO ";
				*sql << sql->table_prefix() << "host_event_log SET host_id='" << _host_id << "', event='";
				*sql << sql->escape_string(s_event.text()) << "', time=";
				if (s_event.event_time() == 0)
					*sql << ns_current_time();
				else *sql << s_event.event_time();
				*sql << ", processing_job_op =" << (unsigned int)s_event.processing_job_operation;
				*sql << ", parent_event_id=" << s_event.parent_event_id << ", subject_experiment_id=" << s_event.subject_experiment_id;
				*sql << ", subject_sample_id=" << s_event.subject_sample_id << ", subject_captured_image_id=" << s_event.subject_captured_image_id;
				*sql << ", subject_region_info_id=" << s_event.subject_region_info_id << ", subject_region_image_id=" << s_event.subject_region_image_id;
				*sql << ", subject_image_id=" << s_event.subject_image_id;
				*sql << ", subject_width = " << s_event.subject_properties.width << ", subject_height = " << s_event.subject_properties.height;
				*sql << ", processing_duration = " << s_event.processing_duration;
				*sql << ", sub_text = ''";
				if (current_thread_state->second.separate_output())
					*sql << ", node_id = " << current_thread_state->second.external_thread_id;
				if (s_event.type() == ns_ts_error)
					*sql << ", error=1";
				event_id = sql->send_query_get_id();
				current_thread_state->second.last_event_sql_id = event_id;
			}


			catch(ns_ex & ex){

				current_thread_state->second.last_event_sql_id = 0;
				event_id = 1;
				ns_image_server_event ev("ns_image_server::register_server_event()::Could not access ") ;
				ev << (sql->connected_to_central_database()?("central database"):("local buffer database")) << " to register an error:";
				ev << ex.text();
				ns_acquire_lock_for_scope lock(server_event_lock,__FILE__,__LINE__);
				cerr << "\n";
				ev.print(cerr);
				cerr << "\n";

				ev.print(event_log);
				event_log << "\n";
				event_log.flush();
				lock.release();
			}
		}
		else event_id = 1;

		if (!no_display){
			if (!current_thread_state->second.separate_output()) {
				ns_acquire_lock_for_scope lock(server_event_lock, __FILE__, __LINE__);
				s_event.print(event_log);
				event_log << "\n";
				event_log.flush();
				lock.release();
			}
			else {
				s_event.print(*current_thread_state->second.thread_specific_logfile);
				*current_thread_state->second.thread_specific_logfile << "\n";
				current_thread_state->second.thread_specific_logfile->flush();
			}
		}

		return event_id;
	}
	catch(ns_ex & ex){
		cerr << "ns_image_server::register_server_event()::Threw an exception!" << ex.text() << "\n";
	}
	catch(...){
		cerr << "ns_image_server::register_server_event()::Threw an unknown eexception!\n";
	}
	return 0;
}

void ns_image_server::set_up_model_directory(){
	std::string path = long_term_storage_directory + DIR_CHAR_STR + worm_detection_model_directory();
	ns_dir::create_directory_recursive(path);
	path = long_term_storage_directory +  DIR_CHAR_STR + posture_analysis_model_directory();
	ns_dir::create_directory_recursive(path);
	path = long_term_storage_directory +  DIR_CHAR_STR + position_analysis_model_directory();
	ns_dir::create_directory_recursive(path);
}

#ifndef NS_ONLY_IMAGE_ACQUISITION
#ifndef NS_MINIMAL_SERVER_BUILD
void ns_image_server::load_all_worm_detection_models(std::vector<ns_worm_detection_model_cache::const_handle_t> & spec){
	std::string path = long_term_storage_directory + DIR_CHAR_STR + worm_detection_model_directory();
	ns_dir dir;
	std::vector<std::string> files;
	dir.load_masked(path,"txt",files);
	unsigned long count(0);
	spec.resize(dir.files.size());
	for (unsigned int i = 0; i < files.size(); i++){
		std::string::size_type l = files[i].rfind("_model");
		if (l == std::string::npos)
			continue;
	//	l-=5;
		std::string model_name = files[i].substr(0,l);
		get_worm_detection_model(model_name,spec[count]);
		count++;
	}
	spec.resize(count);
}

void ns_image_server::get_posture_analysis_model_for_region(const ns_64_bit region_info_id, typename ns_image_server::ns_posture_analysis_model_cache::const_handle_t & it, ns_sql & sql){
	sql << "SELECT posture_analysis_model, posture_analysis_method FROM sample_region_image_info WHERE id = " << region_info_id;
	ns_sql_result res;
	sql.get_rows(res);

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("get_posture_analysis_model_for_region::sql result obtained"));
	if (res.size() == 0)
		throw ns_ex("ns_image_server::get_posture_analysis_model_for_region()::Could not find region ") << region_info_id << " in db";
	if (res[0][0].size() == 0)
		throw ns_ex("ns_image_server::get_posture_analysis_model_for_region()::No posture analysis model was specified for region ")<< region_info_id;
	//cout << "Loading region " << res[0][0] << "\n";
	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("get_posture_analysis_model_for_region::sql result checked"));
	ns_posture_analysis_model_entry_source source;
	source.analysis_method = (ns_posture_analysis_model::method_from_string(res[0][1]));

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("get_posture_analysis_model_for_region::method obtained"));
	source.set_directory(long_term_storage_directory, posture_analysis_model_directory());

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("get_posture_analysis_model_for_region::directory set"));
	ns_posture_analysis_model_cache_specification pa_spec(res[0][0], ns_posture_analysis_model::method_from_string(res[0][1]));
	posture_analysis_model_cache.get_for_read(pa_spec, it, source);
	if (it().model_specification.posture_analysis_method == ns_posture_analysis_model::ns_hidden_markov || it().model_specification.posture_analysis_method == ns_posture_analysis_model::ns_threshold_and_hmm)
		it().model_specification.hmm_posture_estimator.validate_model_settings(sql);
	
	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("get_posture_analysis_model_for_region::gotten for read"));
}


void ns_posture_analysis_model_entry_source::set_directory(const std::string long_term_storage_directory, const std::string & posture_model_dir) {
	model_directory.resize(0);
	model_directory = long_term_storage_directory;
	model_directory += DIR_CHAR;
	model_directory += posture_model_dir;
	model_directory += DIR_CHAR;
}
void ns_posture_analysis_model_entry::load_from_external_source(const ns_posture_analysis_model_cache_specification & name_, ns_posture_analysis_model_entry_source & external_source) {

	if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("ns_posture_analysis_model_entry::opening"));
	name = name_;
	model_specification.name = name_.first;
	model_specification.posture_analysis_method = name_.second;
	if (model_specification.posture_analysis_method != external_source.analysis_method)
		throw ns_ex("Found a model with the incorrect analysis method specified!  Please rebuild the model registry.");
	if (model_specification.posture_analysis_method == ns_posture_analysis_model::ns_hidden_markov || model_specification.posture_analysis_method == ns_posture_analysis_model::ns_threshold_and_hmm) {
		ifstream moving((external_source.model_directory + model_specification.name + ".csv").c_str());
		if (moving.fail())
			throw ns_ex("Could not load ") << external_source.model_directory + model_specification.name + ".csv";

		model_specification.hmm_posture_estimator.read(moving);
		moving.close();
		if (model_specification.posture_analysis_method == ns_posture_analysis_model::ns_hidden_markov)
		return;
	}
	if (model_specification.posture_analysis_method == ns_posture_analysis_model::ns_threshold || model_specification.posture_analysis_method == ns_posture_analysis_model::ns_threshold_and_hmm) {
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("ns_posture_analysis_model_entry::threshold"));
		ifstream thresh((external_source.model_directory + model_specification.name + "_threshold.csv").c_str());
		if (thresh.fail()) {

			if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("ns_posture_analysis_model_entry::error!"));
			throw ns_ex("Could not load ") << external_source.model_directory + model_specification.name + "_threshold.csv";
		}

		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("ns_posture_analysis_model_entry::reading"));
		model_specification.threshold_parameters.read(thresh);
		if (image_server.verbose_debug_output()) image_server.register_server_event_no_db(ns_image_server_event("ns_posture_analysis_model_entry::done reading"));
		thresh.close();
		//model_specification.threshold_parameters.write(cerr);
		//cerr << "\n";
		return;
	}
	if (model_specification.posture_analysis_method == ns_posture_analysis_model::ns_not_specified)
		throw ns_ex("The posture analysis model specificied, ") << model_specification.name << ", does not have a method specification.";
	if (model_specification.posture_analysis_method == ns_posture_analysis_model::ns_unknown)
		throw ns_ex("The posture analysis model specificied, ") << model_specification.name << ", has an unknown method specification.";
	throw ns_ex("The posture analysis model specificied, ") << model_specification.name << ", has an invalid method specification: " << (int)model_specification.posture_analysis_method;

}

struct ns_posture_analysis_model_registry_entry {
	ns_posture_analysis_model_registry_entry(){}
	ns_posture_analysis_model_registry_entry(const std::string& name_, const std::string & filename_, const ns_posture_analysis_model::ns_posture_analysis_method& m, const std::string& v, const unsigned long f, const std::string & details) :
		name(name_),filename(filename_), method(m), version(v),file_time(f){}
	std::string name, filename, details;
	ns_posture_analysis_model::ns_posture_analysis_method method;
	std::string version;
	unsigned long file_time;
};
struct ns_model_registry_info {
	ns_posture_analysis_model_registry_entry model_entry;
	bool is_a_threshold_file;
	unsigned long last_modified_time_on_disk, last_modified_time_in_db;
	std::string filename, model_name;
	bool needs_to_be_updated_in_registry;
};

void ns_image_server::update_posture_analysis_model_registry(ns_sql& sql, bool force) {

	ns_posture_analysis_model_entry_source source;
	source.model_directory = image_server.long_term_storage_directory + DIR_CHAR_STR + image_server.posture_analysis_model_directory() + DIR_CHAR_STR;

	std::vector<ns_model_registry_info> model_files_on_disk;
	{
		ns_dir dir;
		std::vector<std::string> files;
		dir.load_masked(source.model_directory, ".csv", files);
		model_files_on_disk.resize(files.size());
		for (unsigned int i = 0; i < files.size(); i++) {
			model_files_on_disk[i].filename = files[i];
			std::string::size_type threshold_pos = files[i].find("_threshold.csv");
			std::string name;
			if (threshold_pos != files[i].npos) {
				model_files_on_disk[i].model_name = files[i].substr(0, threshold_pos);
				model_files_on_disk[i].is_a_threshold_file = true;
			}
			else {
				model_files_on_disk[i].model_name = files[i].substr(0, files[i].size() - 4); //remove .csv
				model_files_on_disk[i].is_a_threshold_file = false;
			}
		}
	}

	bool need_to_reset_registry = force;
	for (unsigned int i = 0; i < model_files_on_disk.size(); i++)
		model_files_on_disk[i].last_modified_time_on_disk = ns_dir::get_file_timestamp(source.model_directory + model_files_on_disk[i].filename);
	

	bool models_removed_from_disk = false,
		models_updated_on_disk = false;
	{
		//find the db time of all model files.
		sql << "SELECT MIN(file_time),filename FROM analysis_model_registry WHERE analysis_step='posture' GROUP BY filename";
		ns_sql_result min_model_timesmodels_in_db;
		sql.get_rows(min_model_timesmodels_in_db);
		if (!force) {
			for (unsigned int i = 0; i < model_files_on_disk.size(); i++) {
				model_files_on_disk[i].last_modified_time_in_db = 0;
				for (unsigned int j = 0; j < min_model_timesmodels_in_db.size(); j++) {
					if (model_files_on_disk[i].filename == min_model_timesmodels_in_db[j][1]) {
						model_files_on_disk[i].last_modified_time_in_db = atol(min_model_timesmodels_in_db[j][0].c_str());
						break;
					}
				}

				if (model_files_on_disk[i].last_modified_time_in_db < model_files_on_disk[i].last_modified_time_on_disk) {
					models_updated_on_disk = true;
					model_files_on_disk[i].needs_to_be_updated_in_registry = true;
				}
				else
					model_files_on_disk[i].needs_to_be_updated_in_registry = false;
			}
		}
		//see if any model files have been removed from disk
		for (unsigned int j = 0; j < min_model_timesmodels_in_db.size(); j++) {
			bool found_on_disk = false;
			for (unsigned int i = 0; i < model_files_on_disk.size(); i++) {
				if (model_files_on_disk[i].filename == min_model_timesmodels_in_db[j][1]) {
					found_on_disk = true;
					break;
				}
			}
			if (!found_on_disk)
				models_removed_from_disk = true;
		}
	}
	
	if (!models_removed_from_disk && !models_updated_on_disk)
		return;

	//throw away all previous info in the registry and rebuild anew.
	cout << "Updating Posture Analysis Model Registry\n";
	
	std::vector< ns_posture_analysis_model::ns_posture_analysis_method> models_to_try;
	for (unsigned int i = 0; i < model_files_on_disk.size(); i++) {
		models_to_try.resize(0);

		if (model_files_on_disk[i].is_a_threshold_file)
			models_to_try.push_back(ns_posture_analysis_model::ns_threshold);
		else {
			models_to_try.push_back(ns_posture_analysis_model::ns_threshold_and_hmm);
			models_to_try.push_back(ns_posture_analysis_model::ns_hidden_markov);
		}

		for (auto m = models_to_try.begin(); m != models_to_try.end(); ++m) {
			try {
				source.analysis_method = *m;
				ns_posture_analysis_model_cache::const_handle_t handle;
				posture_analysis_model_cache.get_for_read(ns_posture_analysis_model_cache_specification(model_files_on_disk[i].model_name,*m), handle, source);
				model_files_on_disk[i].model_entry = ns_posture_analysis_model_registry_entry(model_files_on_disk[i].model_name, model_files_on_disk[i].filename, source.analysis_method, handle().model_specification.software_version_when_built(), model_files_on_disk[i].last_modified_time_on_disk, handle().model_specification.model_description_text());
				handle.release();
				break;
			}
			catch (ns_ex & ex) {
				if (*m == ns_posture_analysis_model::ns_threshold_and_hmm)	//some files might not exist so don't automatically add them as missing.
					continue;
				cout << "Encountered an un-loadable file in the model directory:" << model_files_on_disk[i].filename << ": " << ex.text() << "\n";
				model_files_on_disk[i].model_entry = ns_posture_analysis_model_registry_entry(model_files_on_disk[i].model_name, model_files_on_disk[i].filename, ns_posture_analysis_model::ns_unknown, "?", model_files_on_disk[i].last_modified_time_on_disk,"");
			}
		}
	}
	
	sql.set_autocommit(false);
	sql.send_query("START TRANSACTION");
	sql.send_query("LOCK TABLES analysis_model_registry WRITE");
	sql.send_query("DELETE FROM analysis_model_registry WHERE analysis_step='posture'");
	for (unsigned int i = 0; i < model_files_on_disk.size(); i++) {
		sql << "INSERT INTO analysis_model_registry SET name='" << model_files_on_disk[i].model_entry.name << "', analysis_method='" << ns_posture_analysis_model::method_to_string(model_files_on_disk[i].model_entry.method)
			<< "', version='" << model_files_on_disk[i].model_entry.version << "', analysis_step='posture', file_time = " << model_files_on_disk[i].model_entry.file_time
			<< ", filename='" << model_files_on_disk[i].model_entry.filename
			<< "', details='" << model_files_on_disk[i].model_entry.details << "'";
		sql.send_query();
	}
	sql.send_query("COMMIT");
	sql.send_query("UNLOCK TABLES");
	
}

struct ns_worm_detection_model_registry_info {
	ns_posture_analysis_model_registry_entry model_entry;
	std::string included_stats_filename, range_filename, pca_spec_filename;
	unsigned long last_modified_time_on_disk, last_modified_time_in_db;
	bool needs_to_be_updated_in_registry;
	bool all_necissary_files_exist;
};

void ns_image_server::update_worm_detection_model_registry(ns_sql& sql, bool force) {

	std::string model_directory = image_server.long_term_storage_directory + DIR_CHAR_STR + image_server.worm_detection_model_directory() + DIR_CHAR_STR;


	std::map<std::string, ns_worm_detection_model_registry_info> model_files_on_disk;
	unsigned long latest_change_in_directory = 0;
	{
		ns_dir dir;
		std::vector<std::string> files;
		std::set<std::string> orphan_files;
		dir.load_masked(model_directory, ".txt", files);

		for (unsigned int i = 0; i < files.size(); i++) {

			const unsigned long tm = ns_dir::get_file_timestamp(model_directory + files[i]);
			if (tm > latest_change_in_directory)
				latest_change_in_directory = tm;

			std::string::size_type l = files[i].rfind("_model.txt");
			if (l == std::string::npos) {
				orphan_files.emplace(files[i]);
				continue;
			}
			const std::string model_name = files[i].substr(0, l);
			auto p = model_files_on_disk.emplace(std::pair<std::string, ns_worm_detection_model_registry_info>(model_name, ns_worm_detection_model_registry_info())).first;
			p->second.model_entry.filename = files[i];
			p->second.model_entry.name = model_name;
			p->second.model_entry.version = "1";
		}
		for (auto p = model_files_on_disk.begin(); p != model_files_on_disk.end(); ++p) {
			for (auto o = orphan_files.begin(); o != orphan_files.end(); ) {
				if (o->find(p->second.model_entry.name) != o->npos) {
					const std::string suffix = o->substr(p->second.model_entry.name.size());
					if (suffix == "_range.txt") {
						p->second.range_filename = *o;
						o = orphan_files.erase(o);
					}
					else if (suffix == "_included_stats.txt") {
						p->second.included_stats_filename = *o;
						o = orphan_files.erase(o);
					}
					else if (suffix == "_pca_spec.txt") {
						p->second.pca_spec_filename = *o;
						o = orphan_files.erase(o);
					}
					else ++o;
				}
				else ++o;
			}
			std::string error;
			if (p->second.range_filename.empty())
				error = "Could find range file";
			if (p->second.included_stats_filename.empty())
				error = "Could not find included stats file";
			try {
				ns_worm_detection_model_cache::const_handle_t it;
				get_worm_detection_model(p->second.model_entry.name, it);
				it.release();
			}
			catch (ns_ex & ex) {
				error = ex.text();
			}
			if (error.empty())
				p->second.all_necissary_files_exist = true;
			else {
				p->second.all_necissary_files_exist = false;
				cout << "Could not use model " << p->second.model_entry.name << ": " << error << "\n";
			}


		}
		
		//get latest file date for each model
		for (auto p = model_files_on_disk.begin(); p != model_files_on_disk.end(); ++p) {
			const std::string* files_to_check[4] = { &p->second.model_entry.filename,&p->second.included_stats_filename,&p->second.pca_spec_filename,&p->second.range_filename };
			unsigned long latest_change = 0;
			for (unsigned int i = 0; i < 4; i++) {
				if (files_to_check[i]->empty())
					continue;
				const unsigned long tm = ns_dir::get_file_timestamp(model_directory + *files_to_check[i]);
				if (tm > latest_change)
					latest_change = tm;
			}
			p->second.model_entry.file_time = latest_change;
			p->second.last_modified_time_on_disk = latest_change;
		}
	}

	bool need_to_reset_registry = force;
	bool models_removed_from_disk = false,
		models_updated_on_disk = false;
	{
		//find the db time of all model files.
		sql << "SELECT MIN(file_time),filename FROM analysis_model_registry WHERE analysis_step='detection' GROUP BY filename";
		ns_sql_result min_model_timesmodels_in_db;
		sql.get_rows(min_model_timesmodels_in_db);
		if (!force) {
			for (auto p = model_files_on_disk.begin(); p != model_files_on_disk.end(); ++p) {
				p->second.last_modified_time_in_db = 0;
				for (unsigned int j = 0; j < min_model_timesmodels_in_db.size(); j++) {
					if (p->second.model_entry.filename == min_model_timesmodels_in_db[j][1]) {
						p->second.last_modified_time_in_db = atol(min_model_timesmodels_in_db[j][0].c_str());
						break;
					}
				}

				if (p->second.last_modified_time_in_db < p->second.last_modified_time_on_disk) {
					models_updated_on_disk = true;
					p->second.needs_to_be_updated_in_registry = true;
				}
				else
					p->second.needs_to_be_updated_in_registry = false;
			}
		}
		//see if any model files have been removed from disk
		for (unsigned int j = 0; j < min_model_timesmodels_in_db.size(); j++) {
			bool found_on_disk = false;	
			for (auto p = model_files_on_disk.begin(); p != model_files_on_disk.end(); ++p) {
				if (p->second.model_entry.filename == min_model_timesmodels_in_db[j][1] &&
					p->second.all_necissary_files_exist) {
					found_on_disk = true;
					break;
				}
			}
			if (!found_on_disk)
				models_removed_from_disk = true;
		}
	}

	if (!models_removed_from_disk && !models_updated_on_disk)
		return;

	//throw away all previous info in the registry and rebuild anew.
	cout << "Updating Worm Detection Model Registry\n";

	sql.set_autocommit(false);
	sql.send_query("START TRANSACTION");
	sql.send_query("LOCK TABLES analysis_model_registry WRITE");
	sql.send_query("DELETE FROM analysis_model_registry WHERE analysis_step='detection'"); 
	for (auto p = model_files_on_disk.begin(); p != model_files_on_disk.end(); ++p){
		if (!p->second.all_necissary_files_exist)
			continue;
		sql << "INSERT INTO analysis_model_registry SET name='" << p->second.model_entry.name << "', analysis_method='" << ns_posture_analysis_model::method_to_string(p->second.model_entry.method) << "', version='" << p->second.model_entry.version << "', analysis_step='detection', file_time = " << p->second.model_entry.file_time << ", filename='" << p->second.model_entry.filename << "'";
		sql.send_query();
	}
	sql.send_query("COMMIT");
	sql.send_query("UNLOCK TABLES");
	
}

ns_time_path_solver_parameters ns_image_server::get_position_analysis_model(const std::string & model_name,bool create_default_if_does_not_exist,const ns_64_bit region_info_id_for_default, ns_sql * sql_for_default) const{

	std::string filename = long_term_storage_directory + DIR_CHAR_STR + this->position_analysis_model_directory() + DIR_CHAR_STR + model_name;
	ns_ini ini;
	ini.reject_incorrect_fields(true);
	{
		ns_time_path_solver_parameters p;
		if (create_default_if_does_not_exist)
			p = ns_time_path_solver_parameters::default_parameters(region_info_id_for_default,*sql_for_default,false,false);
		else p = ns_time_path_solver_parameters::default_parameters(0,0,0,0);
		ini.add_field("short_capture_interval_in_seconds",ns_to_string(p.short_capture_interval_in_seconds));
		ini.add_field("number_of_consecutive_sample_captures",ns_to_string(p.number_of_consecutive_sample_captures));
		ini.add_field("number_of_samples_per_device",ns_to_string(p.number_of_samples_per_device));
		ini.add_field("min_stationary_object_path_fragment_duration_in_seconds",ns_to_string(p.min_stationary_object_path_fragment_duration_in_seconds));
		ini.add_field("maximum_time_gap_between_joined_path_fragments",ns_to_string(p.maximum_time_gap_between_joined_path_fragments));
		ini.add_field("maximum_time_overlap_between_joined_path_fragments",ns_to_string(p.maximum_time_overlap_between_joined_path_fragments));
		ini.add_field("min_final_stationary_path_duration_in_minutes",ns_to_string(p.min_final_stationary_path_duration_in_minutes));
		ini.add_field("maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment",ns_to_string(p.maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment));
		ini.add_field("stationary_object_path_fragment_window_length_in_seconds",ns_to_string(p.stationary_object_path_fragment_window_length_in_seconds));
		ini.add_field("stationary_object_path_fragment_max_movement_distance",ns_to_string(p.stationary_object_path_fragment_max_movement_distance));
		ini.add_field("maximum_fraction_duplicated_points_between_joined_path_fragments",ns_to_string(p.maximum_fraction_duplicated_points_between_joined_path_fragments));
		ini.add_field("maximum_distance_betweeen_joined_path_fragments",ns_to_string(p.maximum_distance_betweeen_joined_path_fragments));
		ini.add_field("maximum_path_fragment_displacement_per_hour",ns_to_string(p.maximum_path_fragment_displacement_per_hour));
		ini.add_field("max_average_final_path_average_timepoint_displacement",ns_to_string(p.max_average_final_path_average_timepoint_displacement));
		ini.add_field("maximum_fraction_of_median_gap_allowed_in_low_density_paths",ns_to_string(p.maximum_fraction_of_median_gap_allowed_in_low_density_paths));
	}
	try{
		if (!ns_dir::file_exists(filename))
			throw ns_ex("Could not find position analysis model file: ") << this->position_analysis_model_directory() + DIR_CHAR_STR + model_name;
	}
	catch(...){
		if (create_default_if_does_not_exist){
			ns_dir::create_directory_recursive(ns_dir::extract_path(filename));
			ini.save(filename);
			return get_position_analysis_model(model_name,false,0,0);
		}
		else throw;
	}
	ini.load(filename);

	ns_time_path_solver_parameters p;
	p.short_capture_interval_in_seconds = ini.get_integer_value("short_capture_interval_in_seconds");
	p.number_of_consecutive_sample_captures = ini.get_integer_value("number_of_consecutive_sample_captures");
	p.number_of_samples_per_device = ini.get_integer_value("number_of_samples_per_device");

	p.min_stationary_object_path_fragment_duration_in_seconds = ini.get_integer_value("min_stationary_object_path_fragment_duration_in_seconds");
	p.stationary_object_path_fragment_window_length_in_seconds = ini.get_integer_value("stationary_object_path_fragment_window_length_in_seconds");
	p.stationary_object_path_fragment_max_movement_distance = ini.get_integer_value("stationary_object_path_fragment_max_movement_distance");
	p.maximum_time_gap_between_joined_path_fragments = ini.get_integer_value("maximum_time_gap_between_joined_path_fragments");
	p.maximum_time_overlap_between_joined_path_fragments = ini.get_integer_value("maximum_time_overlap_between_joined_path_fragments");
	p.maximum_distance_betweeen_joined_path_fragments = ini.get_integer_value("maximum_distance_betweeen_joined_path_fragments");
	p.min_final_stationary_path_duration_in_minutes = ini.get_integer_value("min_final_stationary_path_duration_in_minutes");

	p.maximum_fraction_duplicated_points_between_joined_path_fragments = atof(ini.get_value("maximum_fraction_duplicated_points_between_joined_path_fragments").c_str());
	p.maximum_path_fragment_displacement_per_hour = atof(ini.get_value("maximum_path_fragment_displacement_per_hour").c_str());
	p.max_average_final_path_average_timepoint_displacement = atof(ini.get_value("max_average_final_path_average_timepoint_displacement").c_str());
	p.maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment = atof(ini.get_value("maximum_fraction_of_points_allowed_to_be_missing_in_path_fragment").c_str());
	p.maximum_fraction_of_median_gap_allowed_in_low_density_paths = atof(ini.get_value("maximum_fraction_of_median_gap_allowed_in_low_density_paths").c_str());
	return p;
}
#endif


void ns_svm_model_specification_entry_source::set_directory(const std::string long_term_storage_directory, const std::string & worm_detection_dir) {
	model_directory.resize(0);
	model_directory = long_term_storage_directory;
	model_directory += DIR_CHAR;
	model_directory += worm_detection_dir;
	model_directory += DIR_CHAR;
}
void ns_svm_model_specification_entry::load_from_external_source(const std::string & name, ns_svm_model_specification_entry_source & external_source) {
	ns_dir::create_directory_recursive(external_source.model_directory);
	model_specification.read_statistic_ranges(external_source.model_directory + name + "_range.txt");
	model_specification.read_included_stats(external_source.model_directory + name + "_included_stats.txt");
	model_specification.pca_spec.read(external_source.model_directory + name + "_pca_spec.txt");
	model_specification.model_name = name;

	#ifdef NS_USE_MACHINE_LEARNING
			std::string fn = external_source.model_directory + name + "_model.txt";
	#ifdef NS_USE_TINYSVM
			if (!spec.model.read(fn.c_str()))
				throw ns_ex("ns_image_server::Could not load SVM model file: ") << fn;
	#else
			model_specification.model = std::shared_ptr<svm_model>(svm_load_model(fn.c_str()),ns_svm_model_specification::ns_svm_deleter);
			if (model_specification.model == NULL)
				throw ns_ex("ns_image_server::Could not load SVM model file:") << fn;
	#endif
	#endif
}


void ns_storyboard_cache_entry::load_from_external_source(const ns_experiment_storyboard_spec & specification, ns_sql & sql){
	spec = specification;
	name = spec.to_string();
	manager.load_metadata_from_db(spec, storyboard,sql);
	storyboard.prepare_to_draw(sql);
}

void ns_storyboard_cache_entry::clean_up(ns_sql & external_source) {
	storyboard.clear();
}


void ns_image_server::get_storyboard(const ns_experiment_storyboard_spec & spec, ns_storyboard_cache::const_handle_t & handle, ns_sql & sql) {
	storyboard_cache.get_for_read(spec, handle, sql);
}

void ns_image_server::clean_up_storyboard_cache(bool remove_all, ns_sql & sql) {
	if (remove_all)
		storyboard_cache.clear_cache(sql);
	else storyboard_cache.remove_old_images(60*5,sql);

}

void  ns_image_server::get_worm_detection_model(const std::string & name, typename  ns_worm_detection_model_cache::const_handle_t & it) {
	ns_svm_model_specification_entry_source source;
	source.set_directory(long_term_storage_directory, worm_detection_model_directory());
	worm_detection_model_cache.get_for_read(name, it, source);
}

#endif

#ifdef _WIN32
//from example http://www.ddj.com/showArticle.jhtml?documentID=win0312d&pgno=4
void ns_update_software(const bool just_launch){

	char full_command_line_char[512];
	GetModuleFileName(NULL,full_command_line_char,512);
	const std::string full_command_line(full_command_line_char);
	string dll_path(ns_dir::extract_path(full_command_line));
	dll_path += "\\ns_image_server_update.dll";
	if (!ns_dir::file_exists(dll_path))
		throw ns_ex("Update dll file cannot be located at ") << dll_path;

	 //find rundll32
    char commandLine[MAX_PATH * 3+1];
    const unsigned long s(GetWindowsDirectory(commandLine, sizeof(commandLine)));
	commandLine[s] = 0;
	std::string command = commandLine;
	command += "\\rundll32.exe";
    if (GetFileAttributes(command.c_str()) == INVALID_FILE_ATTRIBUTES){
        const unsigned long t(GetSystemDirectory(commandLine, sizeof(commandLine)));
		commandLine[t] = 0;
        command = commandLine;
		command += "\\rundll32.exe";
    }

	//request rundll32 executes the update dll
    command+=" \"";  //remember to add quotes so that paths with spaces work
	command+=dll_path;
	command+= "\"";

	command+=", _get_and_install_update@16 ";

/*	//hack to force "Documents and Settings" directories through despite it having
	//whitespace
	std::string short_dir = local_exec_path;
	std::string search = "Documents and Settings";
	std::string::size_type pos = short_dir.find(search);
	if (pos != short_dir.npos){
		short_dir.replace(pos,search.size(),"Docume~1");
	}*/

    //give the update dll the commandline to update
	command += "\"";
	command += full_command_line;
	command += "\"";


    //Execute the command
    PROCESS_INFORMATION procInfo;
    STARTUPINFO startInfo;
    memset(&startInfo, 0, sizeof(startInfo));
    startInfo.dwFlags = STARTF_FORCEOFFFEEDBACK | CREATE_NEW_PROCESS_GROUP;
    CreateProcess(0, const_cast<char *>(command.c_str()), 0, 0, FALSE, NORMAL_PRIORITY_CLASS, 0, 0,
                  &startInfo, &procInfo);
}
#endif

void ns_wrap_m4v_stream(const std::string & m4v_filename, const std::string & output_basename){
	//we have now produced a raw mpeg4 stream.
	//compile it into mp4 movies at four different frame_rates
	for (unsigned int j = 0; j < 5; j++){

		std::string output,
			   error_output;
		std::string param = "-add " + m4v_filename;
		std::string fps;
		switch(j){
			case 0: fps="0.5"; break;
			case 1: fps="1"; break;
			case 2: fps="5"; break;
			case 3: fps="10"; break;
			case 4: fps="30"; break;
		}
		std::string vid_filename = output_basename + "=" + fps + "fps.mp4";
		ns_dir::delete_file(vid_filename);
		param += " -mpeg4 -fps " + fps + " " + vid_filename;


		ns_external_execute_options opt;
		opt.binary = true;
		ns_external_execute exec;
		exec.run(image_server.video_compiler_filename(), param, opt);
		exec.release_io();
	}
}



ns_image_storage_reciever_handle<ns_8_bit> ns_image_server_results_storage::movement_timeseries_collage(ns_image_server_results_subject & spec,const std::string & graph_type ,const ns_image_type & image_type, const unsigned long max_line_length, ns_sql & sql){
	spec.get_names(sql);
	string path(spec.experiment_name + DIR_CHAR_STR + movement_timeseries_folder());
	ns_image_server_results_file f(results_directory,path, spec.experiment_filename()  + graph_type + "=collage.tif");
	ns_dir::create_directory_recursive(path);
	return ns_image_storage_reciever_handle<ns_image_storage_handler::ns_component>(new ns_image_storage_reciever_to_disk<ns_image_storage_handler::ns_component>(max_line_length, f.path(),image_type,false));
}


ns_image_storage_reciever_handle<ns_8_bit> ns_image_server_results_storage::machine_learning_training_set_image(ns_image_server_results_subject & spec, const unsigned long max_line_length, ns_sql & sql){
	spec.get_names(sql);
	string path( machine_learning_training_set_folder());
	ns_image_server_results_file f(results_directory, spec.experiment_name + DIR_CHAR_STR + path,spec.region_filename() +"=training_set.tif");
	ns_dir::create_directory_recursive(f.dir());
	return ns_image_storage_reciever_handle<ns_image_storage_handler::ns_component>(new ns_image_storage_reciever_to_disk<ns_image_storage_handler::ns_component>(max_line_length, f.path(),ns_tiff,false));
}
ns_ostream * ns_image_server_results_storage::machine_learning_training_set_metadata(ns_image_server_results_subject& spec, const unsigned long max_line_length, const ns_image_type& image_type, ns_sql& sql) {
	spec.get_names(sql);
	string path(machine_learning_training_set_folder());
	std::string fname = spec.region_filename() + "=training_set";
	ns_add_image_suffix(fname, image_type);
	ns_image_server_results_file f(results_directory, spec.experiment_name + DIR_CHAR_STR + path, fname);
	ns_dir::create_directory_recursive(f.dir());
	if (image_type == ns_csv_gz || image_type == ns_wrm_gz || image_type == ns_xml_gz)
		return new ns_ostream(new ogzstream(f.path().c_str(),ios_base::binary));
	return new ns_ostream(new ofstream(f.path().c_str()));
}
ns_image_server_results_file ns_image_server_results_storage::optimized_posture_analysis_parameter_set(ns_image_server_results_subject & spec, const std::string & type, ns_sql & sql) const {
	spec.get_names(sql);
	string dir, fname;
	dir = time_path_image_analysis_quantification() + DIR_CHAR_STR + "optimal_posture_analysis_parameter_sets" ;

	fname = type;
	ns_add_image_suffix(fname, ns_csv);
	return ns_image_server_results_file(results_directory, spec.experiment_name + DIR_CHAR_STR + dir, fname);
}

ns_image_server_results_file ns_image_server_results_storage::time_path_image_analysis_quantification(ns_image_server_results_subject & spec,const std::string & type, const bool store_in_results_directory,ns_sql & sql, bool abbreviated_time_series, bool compress_file_names, const ns_image_type& file_type) const{
		spec.get_names(sql);
		string fname(""),dir("");
		string abbreviated;
		if(abbreviated_time_series)
			abbreviated = "=abr";
		if (spec.region_id != 0){
			dir = time_path_image_analysis_quantification() + DIR_CHAR_STR + "regions";
			if (compress_file_names)
				fname = ns_image_server_results_subject::create_short_name(spec.region_filename());
			else fname = spec.region_filename();
		}
		else{
			dir = time_path_image_analysis_quantification();
			if (compress_file_names)
				fname = ns_image_server_results_subject::create_short_name(spec.experiment_filename());
			else fname = spec.experiment_filename();
		}
		fname += type + abbreviated;
		ns_add_image_suffix(fname, file_type);

		if (store_in_results_directory)
 			return ns_image_server_results_file(results_directory,spec.experiment_name + DIR_CHAR_STR + dir, fname);
		else{
			std::string abs_path;
			std::string relative_path = image_server.image_storage.movement_file_directory(spec.region_id, &sql, abs_path);
			return ns_image_server_results_file(abs_path,relative_path+ DIR_CHAR_STR+dir, fname);
		}
	}



//global server object
ns_image_server image_server;
const ns_image_server & image_server_const(image_server);



void ns_image_server_global_debug_handler(const ns_text_stream_t & t){
	if (image_server.verbose_debug_output())
		image_server.register_server_event_no_db(ns_image_server_event() << t.text() << ns_ts_debug,true);
}

