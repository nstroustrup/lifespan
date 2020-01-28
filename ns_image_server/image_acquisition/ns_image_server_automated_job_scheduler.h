#ifndef NS_IMAGE_SERVER_AUTOMATED_JOB_SCHEDULER_H
#define NS_IMAGE_SERVER_AUTOMATED_JOB_SCHEDULER_H
#include "ns_ex.h"
#include "ns_sql.h"
class ns_image_server_automated_job_scheduler{
public:
	typedef enum {ns_waiting_to_begin_static_mask,ns_waiting_for_static_mask_completion,ns_worm_detection} ns_automated_job_state;
	static void scan_for_tasks(ns_sql & sql,bool ignore_timers_and_run_now);
	static void register_static_mask_completion(const ns_64_bit region_id, ns_sql & sql);
	static void handle_when_completed_priority_jobs(ns_sql & sql);
	static bool identify_experiments_needing_captured_image_protection(ns_sql & sql,const ns_64_bit specific_sample_id=0);
private:	
	static void calculate_capture_schedule_boundaries(ns_sql & sql);
	static void identify_regions_needing_static_mask(ns_sql & sql);
//	static unsigned long interval_before_database_scans();//in seconds
	static void schedule_detection_jobs_for_region(const ns_64_bit region_id,ns_sql & sql);
};

#ifndef NS_ONLY_IMAGE_ACQUISITION
class ns_get_automated_job_scheduler_lock_for_scope {
public:
	ns_get_automated_job_scheduler_lock_for_scope(const unsigned long second_delay_until_next_run, ns_sql& sql, bool ignore_time_limits=false) :current_time(0), next_run_time(0), lock_held(false) {
		wait_for_lock(second_delay_until_next_run, sql, ignore_time_limits);
	};
	bool lock_held;
	void release(ns_sql& sql) { release_lock(sql); }
	bool run_requested() { return lock_held; }
	ns_64_bit id();
	void wait_for_lock(const unsigned long second_delay_until_next_run, ns_sql& sql, bool ignore_time_limits = false);
	void get_ownership(const unsigned long second_delay_until_next_run, ns_sql& sql);
	~ns_get_automated_job_scheduler_lock_for_scope() { if (lock_held)release(); }
private:
	void release();
	void release_lock(ns_sql& sql);
	unsigned long next_run_time, current_time;

	
};
#endif
#endif
