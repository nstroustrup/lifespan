#include "ns_processing_job_push_scheduler.h"
#include "ns_image_server.h"
#include "ns_image_server_images.h"
#include "ns_processing_job_processor.h"
using namespace std;

void ns_processing_job_queue_item::save_to_db(ns_sql & sql,const bool lock=true){
	

	if (id == 0)
		sql << "INSERT INTO processing_job_queue SET ";
	else{
		sql << "UPDATE processing_job_queue SET ";
	}
	sql << "priority=" << priority<< ", experiment_id="<< experiment_id << ", job_name='', "
		<< "capture_sample_id=" << capture_sample_id<< ", captured_images_id=" << captured_images_id << ", sample_region_info_id=" << sample_region_info_id << ", "
		<< "sample_region_id=" << sample_region_image_id <<", image_id=" << image_id << ", processor_id="<<processor_id << ", "
		<< "problem=" << problem << ", job_id=" << job_id << ", progress=" << progress << ", movement_record_id=" << movement_record_id << ", job_class = " << job_class << ", paused=" << (paused?"1":"0");
	if (id != 0)
		sql << " WHERE id = " << id;
	if (id != 0 && lock) {
		ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("processing_job_queue", &sql, true, __FILE__, __LINE__));
		sql.send_query();
		lock.release(__FILE__, __LINE__);
	}
	else
		sql.send_query();
}
std::string ns_processing_job_queue_item::provide_stub(){
	return "SELECT id, job_id, priority, experiment_id, capture_sample_id, captured_images_id, sample_region_info_id, sample_region_id, image_id, processor_id, problem, progress, movement_record_id, job_class, paused FROM processing_job_queue ";
}
void ns_processing_job_queue_item::from_result(std::vector<std::string> & result){
		id = ns_atoi64(result[0].c_str());
		job_id = ns_atoi64(result[1].c_str());
		priority= ns_atoi64(result[2].c_str());  //higher is more important
		experiment_id= ns_atoi64(result[3].c_str());
		capture_sample_id= ns_atoi64(result[4].c_str());
		captured_images_id= ns_atoi64(result[5].c_str());
		sample_region_info_id= ns_atoi64(result[6].c_str());
		sample_region_image_id= ns_atoi64(result[7].c_str());
		image_id= ns_atoi64(result[8].c_str());
		processor_id= ns_atoi64(result[9].c_str());
		problem= ns_atoi64(result[10].c_str());
		progress= ns_atoi64(result[11].c_str());
		movement_record_id= ns_atoi64(result[12].c_str());
		job_class= ns_atoi64(result[13].c_str());
		paused = result[14] != "0";
}


void ns_image_server_push_job_scheduler::report_sample_region_image(const bool job_is_paused, std::vector<ns_image_server_captured_image_region> region_images, ns_sql & sql, const ns_64_bit job_to_exclude,const ns_processing_job::ns_job_type &job_type_to_exclude){
	
	//make sure we have all the jobs
	get_processing_jobs_from_db(sql);
	//make sure we have all the info for the submitted regions
	for (unsigned int i = 0; i < region_images.size(); i++){
	//	if (region_images[i].region_info_id == 0)
			region_images[i].load_from_db(region_images[i].region_images_id,&sql);
	}
	//look at each region job
	for (unsigned int j = 0; j < job_cache.size(); j++){
		bool exculde_region_jobs(job_cache[j].id == job_to_exclude && (
								  job_type_to_exclude == ns_processing_job::ns_no_job_type ||
								  job_type_to_exclude == ns_processing_job::ns_region_job));
		bool exculde_movement_jobs(job_cache[j].id == job_to_exclude && (
								  job_type_to_exclude == ns_processing_job::ns_no_job_type ||
								  job_type_to_exclude == ns_processing_job::ns_movement_job));
		
		//if we cannot deduce the job type, mark it as a problem.
		if(!job_cache[j].has_a_valid_job_type()){
			sql << "UPDATE processing_jobs SET problem = 1 WHERE id = " << job_cache[j].id;
			sql.send_query();
			continue;
		}
		
		if (!job_cache[j].is_job_type(ns_processing_job::ns_region_job) &&
			!job_cache[j].is_job_type(ns_processing_job::ns_movement_job)){
	//		if (jobs[j].region_id != 0)
		//		cerr << "(" << jobs[j].region_id << "!)";
			continue;
		}
		for (unsigned int i = 0; i < region_images.size(); i++){
		//	cerr << "(" << jobs[j].region_id<< "," <<  region_images[i].region_info_id  << ")";
			if (region_images[i].region_info_id == job_cache[j].region_id) {
				//the current region is the direct subject of a region job
				if (job_cache[j].is_job_type(ns_processing_job::ns_region_job) && !exculde_region_jobs) {
					//see if the requested job has operations not yet performed on the region.
					bool processing_requested = false;
					for (unsigned int k = 0; k < job_cache[j].operations.size(); k++) {
						if (region_images[i].op_images_[k] < job_cache[j].operations[k]) {
							processing_requested = true;
							break;
						}
					}
					if (!processing_requested)
						continue;

					//No need for a lock here!  We are simply inserting records for new jobs
					ns_processing_job_queue_item queue_item;
					queue_item.job_id = job_cache[j].id;
					queue_item.priority = ns_job_queue_region_priority;
					queue_item.sample_region_image_id = region_images[i].region_images_id;
					queue_item.sample_region_info_id = region_images[i].region_info_id;
					queue_item.paused = job_is_paused;
					queue_item.save_to_db(sql, false);
				}
				//the current region is the indirect subject of a movement job that may now be complete.
			/*	if (job_cache[j].is_job_type(ns_processing_job::ns_movement_job) && !exculde_movement_jobs){
					sql << "SELECT region_id_short_1, region_id_short_2, region_id_long, id FROM worm_movement WHERE "
						<< "calculated=0 AND ("
						<< "region_id_short_1=" << region_images[i].region_images_id << " OR "
						<< "region_id_short_2=" << region_images[i].region_images_id << " OR "
						<< "region_id_long=" << region_images[i].region_images_id
						<< ")";
					ns_sql_result res;
					sql.get_rows(res);
					//cerr << "Found " << res.size() << " movement records for job.\n";
					for (unsigned int k = 0; k < res.size(); k++){
						//confirm that each of the three required time points have been calculated.
						bool no_good = false;
					//	cerr << "IDs: ";
						for (unsigned int l = 0; l < 3 && !no_good; l++){
							if (res[k][l]=="0") no_good = true;
					//		cerr <<res[k][l] << " ";
						}
					//	cerr << "\n";

						for (unsigned int l = 0; l < 3 && !no_good; l++){
							sql << "SELECT " << ns_processing_step_db_column_name(ns_process_region_vis) << ", worm_detection_results_id "
								<< "FROM sample_region_images WHERE problem = 0 AND "
								<< "id = " << res[k][l];
							ns_sql_result res2;
							sql.get_rows(res2);
						//	if (res2.size() == 0)
						//		cerr << "Could not find record with id" << res[k][l] << "\n";
						//	if (res2[0][0] == "0")
						//		cerr << "process_region_vis = " << res2[0][0] << "\n";
						//	if (res2[0][1] == "0")
						//		cerr << "worm_detection_results_id = " << res[0][1] << "\n";

							if (res2.size() == 0 || res2[0][0] == "0" || res2[0][1] == "0"){
						//		cerr << "No good on " << l << "\n";
								no_good = true;
								break;
							}
						}
						if (no_good){
						//	cerr << "No good at all.\n";
							continue;
						}

						ns_processing_job_queue_item queue_item;
						//No need to lock here!  We are only inserting records
						queue_item.job_id = job_cache[j].id;
						queue_item.movement_record_id = ns_atoi64(res[k][3].c_str());
						queue_item.priority = ns_job_queue_movement_priority;
						queue_item.save_to_db(sql,false);
					
			}}*/
			}
		}
		
	}
	sql.send_query("COMMIT");
	
}
void ns_image_server_push_job_scheduler::report_capture_sample_image(std::vector<ns_image_server_captured_image> captured_images, ns_sql & sql){
//make sure we have all the jobs
	get_processing_jobs_from_db(sql);
	//make sure we have all the info for the submitted samples
	for (unsigned int i = 0; i < captured_images.size(); i++)
		if (captured_images[i].sample_id == 0){
			
		//	cerr << "Loading info for captured image " << captured_images[i].captured_images_id << "\n";
			captured_images[i].load_from_db(captured_images[i].captured_images_id,&sql);
		//	cerr << "Found it's sample_id to be " << captured_images[i].sample_id << "\n";
		}
		//else cerr << "Didn't need to load info for captured image " << captured_images[i].captured_images_id << " (Sample_id = " <<  captured_images[i].sample_id << ")\n";

	bool found_sample_job = false;
	//look at each region job
	for (unsigned int j = 0; j < job_cache.size(); j++){
		try{
			if (!job_cache[j].is_job_type(ns_processing_job::ns_sample_job))
				continue;
		}
		catch(ns_ex & ex){
			sql << "UPDATE processing_jobs SET problem = 1 WHERE id = " << job_cache[j].id;
			sql.send_query();
			ex.text(); //suppress warning
			continue;
		}
		found_sample_job = true;
	//	ns_sql_full_table_lock queue_lock(sql,"processing_job_queue");
		//no need to lock here, as we're just inserting records!
		//and see if it concerns any of the submitted region images.
		for (unsigned int i = 0; i < captured_images.size(); i++){
			if (captured_images[i].sample_id == job_cache[j].sample_id){
				if (job_cache[j].operations[ns_process_thumbnail])  //resized images are calculated automatically, don't recalculate them
					continue;
				ns_processing_job_queue_item queue_item;
				queue_item.paused = job_cache[j].paused;
				queue_item.job_id = job_cache[j].id;
				queue_item.priority=ns_job_queue_capture_sample_priority;
				queue_item.capture_sample_id = captured_images[i].sample_id;
				queue_item.captured_images_id = captured_images[i].captured_images_id;
				//cerr << "Adding sample job...\n";
				queue_item.save_to_db(sql,false);
			}
		}
		//queue_lock.unlock();
	}
	if (!found_sample_job) cerr << "Did not find any sample jobs.\n";
}

void ns_image_server_push_job_scheduler::request_job_queue_discovery(ns_sql & sql){
	
	sql.send_query("BEGIN");
	sql << "INSERT INTO processing_jobs SET time_submitted=" << ns_current_time() << ",maintenance_task=" << 
		ns_maintenance_update_processing_job_queue << ", processed_by_push_scheduler=1";
	ns_64_bit id = sql.send_query_get_id();
	{
		//ns_sql_full_table_lock table_lock(sql,"processing_job_queue");

		//no need to lock here; we are just inserting a record.
		ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("processing_job_queue", &sql, true, __FILE__, __LINE__));

		sql << "INSERT INTO processing_job_queue SET job_id=" << id << ", job_name='', priority=50";
		sql.send_query();
		lock.release(__FILE__, __LINE__);
	}
}
bool ns_image_server_push_job_scheduler::try_to_process_a_job_pending_anothers_completion(const ns_processing_job & job,ns_sql & sql){
	bool all_necessary_jobs_completed = false;
	if (!job.is_job_type(ns_processing_job::ns_maintenance_job))
		all_necessary_jobs_completed = true;
	else{
		if (job.region_id != 0){
			//while region images don't have their worm detection results calculated, don't run the job.
			sql << "SELECT worm_detection_results_id FROM sample_region_images WHERE region_info_id = " << job.region_id << " AND problem = 0 AND censored = 0";
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() > 0){
				all_necessary_jobs_completed = true;
				for (unsigned int i = 0; i < res.size(); i++){
					if (res[i][0] == "0"){
						all_necessary_jobs_completed = false;
						break;
					}
				}
			}
		}
		//while experiments have regions with unanalyzed movements, don't run the job
		else if (job.experiment_id != 0 && job.sample_id == 0){
			if (job.maintenance_task != ns_maintenance_generate_animal_storyboard)
				throw ns_ex("Only storyboards and movement analysis can be scheduled pending the completion of other jobs.");
			sql << "SELECT movement_image_analysis_quantification_id FROM sample_region_image_info WHERE id = " << job.experiment_id << " AND excluded_from_analysis = 0 AND censored = 0";
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() > 0){
				all_necessary_jobs_completed = true;
				for (unsigned int i = 0; i < res.size(); i++){
					if (res[i][0] == "0"){
						all_necessary_jobs_completed = false;
						break;
					}
				}
			}
		}
		else all_necessary_jobs_completed = true;
	}

	if (all_necessary_jobs_completed){
		sql << "UPDATE processing_jobs SET pending_another_jobs_completion = 2 WHERE id = " << job.id;
		sql.send_query();
		report_new_job_and_mark_it_so(job,sql);
		return true;
	}
	return false;
}

void ns_image_server_push_job_scheduler::report_new_job_and_mark_it_so(const ns_processing_job & job,ns_sql & sql){
			try{
				sql.send_query("BEGIN");
				report_new_job(job,sql);
				sql << "UPDATE processing_jobs SET currently_under_processing = 0, processed_by_push_scheduler = " << image_server.host_id() << " WHERE id=" << job.id;
				sql.send_query();
				sql.send_query("COMMIT");
			}
			catch(ns_ex & ex){
				ns_64_bit event_id = image_server.register_server_event(ns_image_server_event("An error occurred processing a new job:") << ex.text(),&sql);
				sql << "UPDATE processing_jobs SET currently_under_processing = 0, processed_by_push_scheduler = " << image_server.host_id() << ", problem=" << event_id << " WHERE id=" << job.id;
				sql.send_query();
				sql.send_query("COMMIT");
			}
			
		}

void ns_image_server_push_job_scheduler::discover_new_jobs(ns_sql & sql){
	ns_sql_result pending_jobs;
	try{
	//	sql.set_autocommit(false);
	//	sql.send_query("BEGIN");
		sql << ns_processing_job::provide_query_stub() << " FROM processing_jobs WHERE processed_by_push_scheduler = 0 AND currently_under_processing = 0 AND problem =0 LIMIT 1";// FOR UPDATE";
		sql.get_rows(pending_jobs);
		if (pending_jobs.size() == 0){
	//		sql.set_autocommit(true);
			return;
		}
	}
	catch(...){
	//	sql.set_autocommit(true);
		throw;
	}

	//get_processing_jobs_from_db(sql);
	ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("constants", &sql, true, __FILE__, __LINE__));
	std::string lock_grab_time( image_server.get_cluster_constant_value("processing_job_discovery_lock",ns_to_string(0),&sql));
	

	std::string current_time_string;
	sql << "SELECT UNIX_TIMESTAMP(NOW())";
	current_time_string = sql.get_value();
	const unsigned long current_time(atol(current_time_string.c_str()));

	const unsigned long time_of_last_job_discovery_lock(atol(lock_grab_time.c_str()));
	//if nobody has the lock, or the lock has expired
	if (time_of_last_job_discovery_lock == 0 || (current_time > time_of_last_job_discovery_lock &&
		current_time - time_of_last_job_discovery_lock > 10*60)){
		image_server.set_cluster_constant_value("processing_job_discovery_lock",ns_to_string(current_time),&sql);
		lock.release(__FILE__, __LINE__);
	}
	else{
		lock.release(__FILE__, __LINE__);
		image_server.add_subtext_to_current_event("/nDeferring to another node that is currently updating jobs.", &sql);
		return;
	}
	
	try{
		ns_thread::sleep(5); //wait for 5 seconds before re-loading to allow job submission to finish.
		sql << ns_processing_job::provide_query_stub() << " FROM processing_jobs WHERE processed_by_push_scheduler = 0 AND currently_under_processing = 0 AND problem =0";// FOR UPDATE";
		sql.get_rows(pending_jobs);
		std::vector<ns_processing_job> p_jobs(pending_jobs.size());
		for (unsigned int i = 0; i < pending_jobs.size(); i++){
			p_jobs[i].load_from_result(pending_jobs[i]);
			sql << "UPDATE processing_jobs SET currently_under_processing = " << image_server.host_id() << " WHERE id=" << p_jobs[i].id;
			sql.send_query();
		}
		sql.send_query("COMMIT");
		//sql.set_autocommit(true);
		for (unsigned int i = 0; i < p_jobs.size(); i++){
			try{
				//there's a chance a job pending anothers completion can be run immediately (as all necissary analysis has 
				//already been performed).  Try it now.
				if (p_jobs[i].pending_another_jobs_completion){
					if (!try_to_process_a_job_pending_anothers_completion(p_jobs[i],sql)){
						sql << "UPDATE processing_jobs SET currently_under_processing = 0, processed_by_push_scheduler = " << image_server.host_id() << " WHERE id=" << p_jobs[i].id;
						sql.send_query();
						sql.send_query("COMMIT");
					}
				}
				else
					report_new_job_and_mark_it_so(p_jobs[i],sql);
			}
			catch(...){
				for (unsigned int i = 0; i < p_jobs.size(); i++){
					sql << "UPDATE processing_jobs SET currently_under_processing = 0 WHERE id=" << p_jobs[i].id;
					sql.send_query();
					sql.send_query("COMMIT");
				}
				throw;
			}
		}
		image_server.set_cluster_constant_value("processing_job_discovery_lock",ns_to_string(0),&sql);
	}
	catch(...){
		image_server.set_cluster_constant_value("processing_job_discovery_lock",ns_to_string(0),&sql);
	}
}

void ns_image_server_push_job_scheduler::report_new_job(const ns_processing_job & job,ns_sql & sql){

	if (!job.has_a_valid_job_type()){
		sql << "UPDATE processing_jobs SET problem = 1 WHERE id=" << job.id;
		sql.send_query();
		throw ns_ex("ns_image_server_push_job_schedule::Encountered a job with an invalid job type.");
	}
	ns_acquire_for_scope<ns_processing_job_processor> processor(
		ns_processing_job_processor_factory::generate(job,image_server));
	
	std::vector<ns_processing_job_queue_item> subjects;
	if (!processor().identify_subjects_of_job_specification(subjects,sql))
		throw ns_ex("ns_image_server_push_job::report_new_job:: Did not know how to handle submitted job.");
	
	//no need to lock, only inserting elements
	for (unsigned int i = 0; i < subjects.size(); ++i)
		subjects[i].save_to_db(sql,false);
}


void ns_image_server_push_job_scheduler::request_jobs(unsigned long number_of_jobs, std::vector<ns_processing_job > & jobs, ns_sql & sql, bool first_in_first_out){
	
	std::vector<ns_processing_job_queue_item> queue_items;
	ns_sql_result queue_item_res;
	sql.set_autocommit(false);
	jobs.resize(0);
	try {
		ns_sql_table_lock lock(image_server.sql_table_lock_manager.obtain_table_lock("processing_job_queue", &sql, true, __FILE__, __LINE__));

		sql << ns_processing_job_queue_item::provide_stub() << " WHERE processor_id=0 AND problem=0 AND paused=0 ";
		if (!image_server.compile_videos())
			sql << " AND job_class != 1 ";
		if (image_server.do_not_run_multithreaded_jobs) 
			sql << " AND job_class = 0 ";
		
		if (first_in_first_out)
			sql << "ORDER BY priority DESC, id ASC LIMIT " << number_of_jobs;
		else
			sql << "ORDER BY priority DESC, id DESC LIMIT " << number_of_jobs;

		sql.get_rows(queue_item_res);
		//no jobs
		if (queue_item_res.size() == 0) {
			lock.release(__FILE__, __LINE__);
			return;
		}
		queue_items.resize(queue_item_res.size());
		jobs.resize(queue_item_res.size());
		for (unsigned int i = 0; i < queue_item_res.size(); i++) {
			queue_items[i].from_result(queue_item_res[i]);
			sql << "UPDATE processing_job_queue SET processor_id=" << image_server.host_id() << " WHERE id=" << queue_items[i].id;
			sql.send_query();
		}
		lock.release(__FILE__, __LINE__);
	
		for (unsigned int q = 0; q < queue_items.size(); q++) {
			try {
				//look for job in cache
				for (unsigned int i = 0; i < job_cache.size(); i++)
					if (job_cache[i].id == queue_items[q].job_id) {
						jobs[q] = job_cache[i];
						break;
					}

				if (jobs[q].id == 0) {
					sql << ns_processing_job::provide_query_stub() << " FROM processing_jobs WHERE id = " << queue_items[q].job_id;
					ns_sql_result job_res;
					sql.get_rows(job_res);
					if (job_res.size() == 0) {
						//no need to lock for this deletion
						sql << "DELETE FROM processing_job_queue WHERE id = " << queue_items[q].id;
						sql.send_query();
						sql.send_query("COMMIT");
						throw ns_ex("ns_image_server_push_job_scheduler::request_job()::Could not find job ") << queue_items[q].job_id << " specified by queue in db";
					}
					jobs[q].load_from_result(job_res[0]);
					job_cache.push_back(jobs[q]);
				}

				if (queue_items[q].experiment_id != 0)
					jobs[q].experiment_id = queue_items[q].experiment_id;
				if (queue_items[q].capture_sample_id != 0)
					jobs[q].sample_id = queue_items[q].capture_sample_id;
				if (queue_items[q].sample_region_info_id != 0)
					jobs[q].region_id = queue_items[q].sample_region_info_id;
				if (queue_items[q].captured_images_id != 0)
					jobs[q].captured_image_id = queue_items[q].captured_images_id;
				if (queue_items[q].sample_region_image_id != 0)
					jobs[q].sample_region_image_id = queue_items[q].sample_region_image_id;
				if (queue_items[q].movement_record_id != 0)
					jobs[q].movement_record_id = queue_items[q].movement_record_id;
				if (queue_items[q].image_id != 0)
					jobs[q].image_id = queue_items[q].image_id;
				if (queue_items[q].id != 0)
					jobs[q].queue_entry_id = queue_items[q].id;

				//load names of all the jobs for output
				ns_64_bit d;
				if (jobs[q].region_id != 0)
					ns_region_info_lookup::get_region_info(jobs[q].region_id, &sql, jobs[q].region_name, jobs[q].sample_name, d, jobs[q].experiment_name, d);
				else if (jobs[q].sample_id != 0)
					ns_region_info_lookup::get_sample_info(jobs[q].sample_id, &sql, jobs[q].sample_name, jobs[q].experiment_name, d);
				else if (jobs[q].experiment_id != 0)
					ns_region_info_lookup::get_experiment_info(jobs[q].experiment_id, &sql, jobs[q].experiment_name);
				
			}
			catch (...) {
				//no need to lock for this update
				sql << "UPDATE processing_job_queue SET processor_id=0, problem=1 WHERE id=" << queue_items[q].id;
				sql.send_query();
				throw;
			}
		}
		if (image_server.do_not_run_multithreaded_jobs) {
			//check to see if someone forgot to mark the job as being in a particular class.  If so, update it correctly and give it back.
			for (std::vector<ns_processing_job>::iterator p = jobs.begin(); p != jobs.end();) {

				if (p->is_a_multithreaded_job()) {
					sql << "UPDATE processing_job_queue SET job_class = 2, processor_id=" << 0 << " WHERE id=" << p->id;
					sql.send_query();
					p = jobs.erase(p);
				}
				else p++;
			}
		}
		return;		
	}
	catch(...){
		sql.set_autocommit(true);
		throw;
	}
}

void ns_image_server_push_job_scheduler::report_job_as_finished(const ns_processing_job & job,ns_sql & sql){
	//ns_sql_full_table_lock lock(sql,"processing_job_queue");
	sql << "DELETE FROM processing_job_queue WHERE id=" << job.queue_entry_id;
	sql.send_query();
}
void ns_image_server_push_job_scheduler::report_job_as_unfinished(const ns_processing_job & job,ns_sql & sql){
	//ns_sql_full_table_lock lock(sql,"processing_job_queue");
	sql << "UPDATE processing_job_queue SET processor_id = 0 WHERE id=" << job.queue_entry_id;
	sql.send_query();
}
ns_64_bit ns_image_server_push_job_scheduler::report_job_as_problem(const ns_processing_job & job, const ns_ex & ex,ns_sql & sql){
	ns_64_bit event_id = image_server.register_server_event(ex,&sql);
	if (event_id == 0)
		throw ns_ex("Could not register error in db::") << ex.text();
	//first register the problem in the queue
//	ns_sql_full_table_lock lock(sql,"processing_job_queue");
	sql << "UPDATE processing_job_queue SET processor_id = 0, problem = " << event_id << " WHERE id=" << job.queue_entry_id;
	sql.send_query();
//	lock.unlock();
	sql.send_query("COMMIT");
	return event_id;
}

void ns_image_server_push_job_scheduler::get_processing_jobs_from_db(ns_sql & sql){
	sql << ns_processing_job::provide_query_stub();
	sql << " FROM processing_jobs";
	ns_sql_result job_data;
	sql.get_rows(job_data);
	job_cache.resize(0);
	job_cache.resize(job_data.size());
	for (unsigned int i = 0; i < job_data.size(); i++)
		job_cache[i].load_from_result(job_data[i]);
}
