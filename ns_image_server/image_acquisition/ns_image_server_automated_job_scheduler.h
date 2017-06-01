#ifndef NS_IMAGE_SERVER_AUTOMATED_JOB_SCHEDULER_H
#define NS_IMAGE_SERVER_AUTOMATED_JOB_SCHEDULER_H
#include "ns_ex.h"
#include "ns_sql.h"
class ns_image_server_automated_job_scheduler{
public:
	typedef enum {ns_waiting_to_begin_static_mask,ns_waiting_for_static_mask_completion,ns_worm_detection} ns_automated_job_state;
	static void scan_for_tasks(ns_sql & sql);
	static void register_static_mask_completion(const ns_64_bit region_id, ns_sql & sql);
	static void handle_when_completed_priority_jobs(ns_sql & sql);
	static void identify_experiments_needing_captured_image_protection(ns_sql & sql,const ns_64_bit specific_sample_id=0);
private:	
	static void calculate_capture_schedule_boundaries(ns_sql & sql);
	static void identify_regions_needing_static_mask(ns_sql & sql);
//	static unsigned long interval_before_database_scans();//in seconds
	static void schedule_detection_jobs_for_region(const unsigned long region_id,ns_sql & sql);
};

#endif
