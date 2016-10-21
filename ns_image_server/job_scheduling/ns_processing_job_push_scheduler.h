#ifndef NS_IMAGE_SERVER_PUSH_JOB_SCHEDULER
#define NS_IMAGE_SERVER_PUSH_JOB_SCHEDULER
#include "ns_image_server_images.h"
#include "ns_processing_job.h"


class ns_processing_job_queue_item{
public:
	ns_processing_job_queue_item():id(0),
		priority(0),
		experiment_id(0),
		capture_sample_id(0),
		captured_images_id(0),
		sample_region_info_id(0),
		sample_region_image_id(0),
		image_id(0),
		processor_id(0),
		problem(0),
		progress(0),
		job_id(0),
		movement_record_id(0),
		job_class(0){}

	ns_64_bit id,
				  job_id,
				  priority,
				  experiment_id,
				  capture_sample_id,
				  captured_images_id,
				  sample_region_info_id,
				  sample_region_image_id,
				  image_id,
				  processor_id,
				  problem,
				  progress,
				  movement_record_id,
				  job_class; //certain jobs cannot be performed by certain types of hosts (ie movie compilation on linux hosts).  Job classes warn incapable clients off of inappropriate jobs

	void save_to_db(ns_sql & sql,const bool lock);
	static std::string provide_stub();
	void from_result(std::vector<std::string> & result);

};

class ns_image_server_push_job_scheduler{

public:
	void report_sample_region_image(std::vector<ns_image_server_captured_image_region> region_images, ns_sql & sql,const ns_64_bit job_to_exclude=0,const ns_processing_job::ns_job_type &job_type_to_exclude=ns_processing_job::ns_no_job_type);
	void report_capture_sample_image(std::vector<ns_image_server_captured_image> captured_images, ns_sql & sql);
	void report_new_job(const ns_processing_job & job,ns_sql & sql);
	void report_new_job_and_mark_it_so(const ns_processing_job & job,ns_sql & sql);

	void discover_new_jobs(ns_sql & sql);
	void request_jobs(unsigned long number_of_jobs,std::vector<ns_processing_job > & jobs,ns_sql & sql,bool first_in_first_out=false);
	void report_job_as_finished(const ns_processing_job & job,ns_sql & sql);
	void report_job_as_unfinished(const ns_processing_job & job,ns_sql & sql);

	//we have found an error, handle it by registering it in the
	//host_event log, and annotate the current job (and any associated images)
	//with a reference to the error that occurred.
	ns_64_bit report_job_as_problem(const ns_processing_job & job,const ns_ex & ex,ns_sql & sql);

	static void request_job_queue_discovery(ns_sql & sql);
	bool try_to_process_a_job_pending_anothers_completion(const ns_processing_job & job,ns_sql & sql);

	void delete_job(const ns_processing_job & job, ns_sql & sql);

	//larger numbers get priority over smaller numbers
	enum {
		ns_job_queue_background_priority	    = 5,
		ns_job_queue_region_priority			=10,
		ns_job_queue_capture_sample_priority	=15,
		ns_job_queue_movement_priority			=20,
		ns_job_queue_maintenance_priority		=20,
		ns_job_queue_image_priority				=20,
		ns_job_queue_whole_region_priority		=25,
		ns_job_queue_urgent_priority			=50
	};

private:
	std::vector<ns_processing_job> job_cache;
	void get_processing_jobs_from_db(ns_sql & sql);
	std::map<unsigned long,ns_machine_analysis_data_loader> movement_record_cache;
	
};

#endif
