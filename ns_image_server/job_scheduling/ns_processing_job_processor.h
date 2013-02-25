#ifndef NS_PROCESSING_JOB_PROCESSOR
#define NS_PROCESSING_JOB_PROCESSOR
#include "ns_processing_job_push_scheduler.h"
#include "ns_image_processing_pipeline.h"

class ns_processing_job_processor{
public:
	ns_processing_job_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):job(job_),image_server(&image_server_),pipeline(&pipeline_){}
	ns_processing_job_processor(const ns_processing_job & job_):job(job_),image_server(0),pipeline(0){}
	virtual bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant)=0;
	virtual void mark_subject_as_busy(const bool busy,ns_sql & sql)=0;
	virtual void mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql)=0;
	virtual bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql)=0;
	virtual bool run_job(ns_sql & sql)=0;
	virtual void handle_concequences_of_job_completion(ns_sql & sql){};
	virtual bool flag_job_as_being_processed_before_processing()=0;
	virtual void flag_job_as_being_processed(ns_sql & sql){
		sql << "UPDATE processing_jobs SET currently_under_processing=" << image_server->host_id() << " WHERE id=" << job.id;
		sql.send_query();
	};
	virtual bool delete_job_after_processing()=0;
	virtual void delete_job(ns_sql & sql){
		sql << "DELETE from processing_jobs WHERE id=" << job.id;
		sql.send_query();
	}
protected:
	ns_processing_job job;
	ns_image_server  * image_server;
	ns_image_processing_pipeline * pipeline;
	std::string busy_str(const bool busy){return (busy?ns_to_string(image_server->host_id()):"0");}
};
/*
class  ns_processing_job_movement_processor: public ns_processing_job_processor{
public:
	ns_processing_job_movement_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):ns_processing_job_processor(job_,image_server_,pipeline_){}
	ns_processing_job_movement_processor(const ns_processing_job & job_):ns_processing_job_processor(job_){}
	bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant);
	void mark_subject_as_busy(const bool busy,ns_sql & sql);
	void mark_subject_as_problem(const unsigned long problem_id,ns_sql & sql);
	bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql);
	bool run_job(ns_sql & sql);
	bool delete_job_after_processing(){return false;}
	bool flag_job_as_being_processed_before_processing(){return false;}
};
*/
class ns_processing_job_sample_processor : public ns_processing_job_processor{
public:
	ns_processing_job_sample_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):ns_processing_job_processor(job_,image_server_,pipeline_){}
	ns_processing_job_sample_processor(const ns_processing_job & job_):ns_processing_job_processor(job_){}
	bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant);
	void mark_subject_as_busy(const bool busy,ns_sql & sql);
	void mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql);
	bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql);
	bool run_job(ns_sql & sql);
	void handle_concequences_of_job_completion(ns_sql & sql);
	bool delete_job_after_processing(){return false;}
	bool flag_job_as_being_processed_before_processing(){return false;}
private:
	std::vector<ns_image_server_captured_image_region> output_regions;
};

class ns_processing_job_region_processor : public ns_processing_job_processor{
public:
	ns_processing_job_region_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):ns_processing_job_processor(job_,image_server_,pipeline_){}
	ns_processing_job_region_processor(const ns_processing_job & job_):ns_processing_job_processor(job_){}
	bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant);
	void mark_subject_as_busy(const bool busy,ns_sql & sql);
	void mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql);
	bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql);
	bool run_job(ns_sql & sql);
	void handle_concequences_of_job_completion(ns_sql & sql);
	bool delete_job_after_processing(){return false;}
	bool flag_job_as_being_processed_before_processing(){return false;}
};
class ns_processing_job_whole_region_processor : public ns_processing_job_processor{
public:
	ns_processing_job_whole_region_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):ns_processing_job_processor(job_,image_server_,pipeline_){}
	ns_processing_job_whole_region_processor(const ns_processing_job & job_):ns_processing_job_processor(job_){}
	bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant);
	void mark_subject_as_busy(const bool busy,ns_sql & sql);
	void mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql);
	bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql);
	bool run_job(ns_sql & sql);
	bool delete_job_after_processing(){return true;}
	bool flag_job_as_being_processed_before_processing(){return true;}
};
class ns_processing_job_whole_sample_processor : public ns_processing_job_processor{
public:
	ns_processing_job_whole_sample_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):ns_processing_job_processor(job_,image_server_,pipeline_){}
	ns_processing_job_whole_sample_processor(const ns_processing_job & job_):ns_processing_job_processor(job_){}
	bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant);
	void mark_subject_as_busy(const bool busy,ns_sql & sql);
	void mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql);
	bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql);
	bool run_job(ns_sql & sql);
	bool delete_job_after_processing(){return true;}
	bool flag_job_as_being_processed_before_processing(){return true;}
};
class ns_processing_job_maintenance_processor : public ns_processing_job_processor{
public:
	ns_processing_job_maintenance_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):ns_processing_job_processor(job_,image_server_,pipeline_){}
	ns_processing_job_maintenance_processor(const ns_processing_job & job_):ns_processing_job_processor(job_){}
	bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant);
	void mark_subject_as_busy(const bool busy,ns_sql & sql);
	void mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql);
	bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql);
	bool run_job(ns_sql & sql);
	bool delete_job_after_processing(){return true;}
	void delete_job(ns_sql & sql);
	bool flag_job_as_being_processed_before_processing(){return true;}
	void flag_job_as_being_processed(ns_sql & sql);
private:
	bool delete_job_;
	ns_processing_job parent_job;
};
class ns_processing_job_image_processor : public ns_processing_job_processor{
public:
	ns_processing_job_image_processor(const ns_processing_job & job_, ns_image_server & image_server_, ns_image_processing_pipeline & pipeline_):ns_processing_job_processor(job_,image_server_,pipeline_){}
	ns_processing_job_image_processor(const ns_processing_job & job_):ns_processing_job_processor(job_){}
	bool job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant);
	void mark_subject_as_busy(const bool busy,ns_sql & sql);
	void mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql);
	bool identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql);
	bool run_job(ns_sql & sql);
	bool delete_job_after_processing(){return true;}
	bool flag_job_as_being_processed_before_processing(){return true;}
};
class ns_processing_job_processor_factory{
	public:
	static ns_processing_job_processor * generate(const ns_processing_job & job, 
		ns_image_server & current_server_, 
		ns_image_processing_pipeline & pipeline_){return generate(job,&current_server_,&pipeline_);}
	static ns_processing_job_processor * generate(const ns_processing_job & job){return generate(job,0,0);}
	private:
	static ns_processing_job_processor * generate(const ns_processing_job & job, 
		ns_image_server * current_server_, 
		ns_image_processing_pipeline * pipeline_);
};

#endif
