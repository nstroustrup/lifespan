#ifndef NS_PROCESSING_JOB_SCHEDULER
#define NS_PROCESSING_JOB_SCHEDULER
#include "ns_image_server.h"
#include "ns_processing_job.h"
#include <string>
#include <ctime>
#include "ns_processing_job.h"
#include "ns_processing_job_push_scheduler.h"
//#include "ns_processing_job_processor.h"


void ns_delete_movement_analysis(const ns_64_bit region_id, bool delete_files, ns_sql & sql);
void ns_handle_image_metadata_delete_action(ns_processing_job & job,ns_sql & sql);
void ns_handle_file_delete_request(ns_processing_job & job,ns_sql & sql);
ns_processing_job ns_handle_file_delete_action(ns_processing_job & job,ns_sql & sql);

#define ns_pipeline_chunk_size 1024  //note that, if openjpeg2000 is being used, the chunk size sets the tile height of the openjpeg2000 images.

struct ns_image_processing_pipline_holder;

class ns_processing_error_specification{

	unsigned long image_id;
	unsigned long region_image_id;
	unsigned long processesing_job_queue_id;
	unsigned long processing_job_id;
	ns_ex error;
};

//This class is in charge of deciding which, if any pending job should be fed into the current pipeline,
//and then starting the job.
class ns_processing_job_scheduler{
public:
	ns_processing_job_scheduler(const ns_image_server & cur_image_server):idle_timer_running(false),
	  current_server(&cur_image_server),db_movement_build_probility(25),pipeline(0){init_pipeline();}
	~ns_processing_job_scheduler(){destruct_pipeline();}

	bool run_a_job(ns_processing_job & job,ns_sql & sql);

	void clear_heap();

private:
	ns_high_precision_timer idle_timer;
	bool idle_timer_running;
	void init_pipeline();
	void destruct_pipeline();
	const ns_image_server * current_server;
	ns_image_processing_pipline_holder * pipeline;
	unsigned int db_movement_build_probility;
};

#endif
