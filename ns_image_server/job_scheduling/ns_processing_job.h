#ifndef NS_PROCESSING_JOB_H
#define NS_PROCESSING_JOB_H
#ifndef NS_ONLY_IMAGE_ACQUISITION
#include "ns_machine_analysis_data_loader.h"
#include "ns_svm_model_specification.h"
#endif
#include "ns_processing_tasks.h"
typedef enum{ns_maintenance_no_task,
			 ns_maintenance_rebuild_sample_time_table,
			 ns_maintenance_rebuild_worm_movement_table,
			 ns_maintenance_update_processing_job_queue,
			 ns_maintenance_delete_files_from_disk_request,
			 ns_maintenance_delete_files_from_disk_action,
			 ns_maintenance_rebuild_movement_data,
			 ns_maintenance_rebuild_movement_from_stored_images,
			 ns_maintenance_rebuild_movement_from_stored_image_quantification,
			 ns_maintenance_recalculate_censoring,
			 ns_maintenance_generate_movement_posture_visualization,
			 ns_maintenance_generate_movement_posture_aligned_visualization,
			 ns_maintenance_generate_sample_regions_from_mask,
			 ns_maintenance_delete_images_from_database,
			 ns_maintenance_check_for_file_errors,
			 ns_maintenance_determine_disk_usage,
			 ns_maintenance_generate_animal_storyboard,
			 ns_maintenance_generate_animal_storyboard_subimage,
			 ns_maintenance_compress_stored_images,
			 ns_maintenance_generate_subregion_mask,
			 ns_maintenance_rerun_image_registration,
			 ns_maintenance_recalc_image_stats,
			 ns_maintenance_recalc_worm_morphology_statistics,
			 ns_maintenance_last_task
		} ns_maintenance_task;

std::string ns_maintenance_task_to_string(const ns_maintenance_task & task);
class ns_lifespan_curve_cache_entry;
struct ns_processing_step{
	unsigned int experiment_id,
		sample_id,
		region_id,
		image_id;
	ns_processing_task task;
};

struct ns_processing_job{
	ns_processing_job():
		id(0),
		experiment_id(0),
		sample_id(0),
		region_id(0),
		image_id(0),
		processor_id(0),
		time_submitted(0),
		mask_id(0),
		sample_region_image_id(0),
		captured_image_id(0),
		movement_record_id(0),
		maintenance_task(ns_maintenance_no_task),
		maintenance_flag(ns_none),
		subregion_position(0,0),
		subregion_size(0,0),
		subregion_start_time(0),
		subregion_stop_time(0),
		delete_file_job_id(0),
		urgent(false),
		pending_another_jobs_completion(0),
		video_timestamp_type(ns_no_timestamp),
		generate_video_at_high_res(false),
		death_time_annotations(0){operations.resize((unsigned int)ns_process_last_task_marker);}
		typedef enum{ns_no_timestamp,ns_date_timestamp,ns_age_timestamp} ns_timestamp_type;
	typedef enum {ns_experiment_job, ns_sample_job, ns_region_job, ns_image_job, ns_movement_job, ns_whole_region_job, ns_whole_sample_job, ns_whole_experiment_job, ns_maintenance_job,ns_no_job_type} ns_job_type;
	typedef enum {ns_none, ns_only_delete_processed_captured_images, ns_delete_censored_images, ns_delete_entire_sample_region, ns_delete_everything_but_raw_data} ns_maintenance_flag;
	bool has_a_valid_job_type() const{
		return !is_job_type(ns_no_job_type);
	}
	bool is_a_multithreaded_job() const{
		return maintenance_task == ns_maintenance_rebuild_movement_data ||
			 maintenance_task == ns_maintenance_rebuild_movement_from_stored_images ||
		//	 maintenance_task == ns_maintenance_rebuild_movement_from_stored_image_quantification ||
			 operations.size() > (int)ns_process_compile_video  &&
			 operations[(int)ns_process_compile_video] != 0;
	}
	//returns the type of job the specification requests.
	bool is_job_type(const ns_job_type t) const{

		if (maintenance_task != ns_maintenance_no_task)
			return (t == ns_maintenance_job);

		bool whole_job =	operations[(unsigned int)ns_process_compile_video] != 0 ||
							operations[(unsigned int)ns_process_heat_map] != 0 ||
							operations[(unsigned int)ns_process_static_mask] != 0 ||
							operations[(unsigned int)ns_process_subregion_label_mask] != 0;
		if (image_id != 0)
			return (t == ns_image_job);

		if (region_id != 0){
			//job is either a whole region job, movement job, or region job.
			if (whole_job)
				return (t == ns_whole_region_job);

			if (t!=ns_movement_job && t!=ns_region_job)
				return false;

			bool is_a_movement_job(false);

			if (operations[ns_process_movement_coloring] ||
				operations[ns_process_movement_mapping] ||
				operations[ns_process_posture_vis])
					is_a_movement_job = true;
			if (t == ns_movement_job)
				return is_a_movement_job;

			if (t == ns_region_job &&
				(!is_a_movement_job ||
				operations[ns_process_movement_coloring_with_graph] ||
				operations[ns_process_movement_coloring_with_survival]))
				return true;

			//check to see if the job is both a region and a movement job.
			for (int i = ns_process_spatial; i < ns_process_analyze_mask; i++){
				if (operations[i]){
					return true;
				}
			}
		}
		if (sample_id != 0){
			if (whole_job)
				return (t==ns_whole_sample_job);
			else return (t == ns_sample_job);
		}
		if (experiment_id != 0){
			if (whole_job)
				return (t==ns_whole_experiment_job);
			else return (t==ns_experiment_job);
		}
		else
			return (t == ns_no_job_type);
	}

	//general job information
	ns_64_bit id,
		experiment_id,
		sample_id,
		region_id,  //this is the region info id...misnamed just to be confusing.
		image_id,
		processor_id;
	unsigned long
		time_submitted;

	//job information specific to certain types of jobs.
	ns_64_bit mask_id,
		sample_region_image_id,
		captured_image_id,
		movement_record_id;
	ns_maintenance_task maintenance_task;

	ns_maintenance_flag maintenance_flag;

	ns_vector_2i subregion_position,
				 subregion_size;
	bool generate_video_at_high_res;
	unsigned long subregion_start_time,
				  subregion_stop_time,
		pending_another_jobs_completion;
	ns_64_bit delete_file_job_id;

	bool urgent;
	ns_timestamp_type video_timestamp_type;
	std::string experiment_name,
			sample_name,
			region_name,
			image_filename,
			image_path,
			processor_name;

	std::vector<char> operations; //operations[i] = 1 means that the operation is requested
#ifndef NS_ONLY_IMAGE_ACQUISITION
	const ns_lifespan_curve_cache_entry * death_time_annotations;

	typename ns_worm_detection_model_cache::const_handle_t model;
#else
void * death_time_annotations;
#endif

	static std::string provide_query_stub(){
		return std::string("SELECT processing_jobs.id, processing_jobs.experiment_id, processing_jobs.sample_id, ")
				 + "processing_jobs.region_id, processing_jobs.image_id,"
				+ "processing_jobs.urgent, processing_jobs.processor_id, processing_jobs.time_submitted, processing_jobs.mask_id, "
				+ "processing_jobs.maintenance_task,subregion_position_x,subregion_position_y,subregion_width,subregion_height, "
				+ "subregion_start_time,subregion_stop_time, processing_jobs.delete_file_job_id, processing_jobs.video_add_timestamp, "
				+ "processing_jobs.maintenance_flag, processing_jobs.pending_another_jobs_completion , processing_jobs.generate_video_at_high_res,"

				+ "processing_jobs.op0, processing_jobs.op1, processing_jobs.op2, processing_jobs.op3, processing_jobs.op4, processing_jobs.op5, processing_jobs.op6, "
 				+ "processing_jobs.op7, processing_jobs.op8, processing_jobs.op9, processing_jobs.op10, processing_jobs.op11, "
				+ "processing_jobs.op12,processing_jobs.op13,processing_jobs.op14,processing_jobs.op15,processing_jobs.op16,processing_jobs.op17"
				+ ",processing_jobs.op18,processing_jobs.op19,processing_jobs.op20,processing_jobs.op21,processing_jobs.op22,processing_jobs.op23,"
				+ "processing_jobs.op24,processing_jobs.op25,processing_jobs.op26,processing_jobs.op27,processing_jobs.op28,processing_jobs.op29,processing_jobs.op30 ";
	}

	void load_from_result(const std::vector<std::string> & res){
		id = ns_atoi64(res[0].c_str());
		experiment_id = ns_atoi64(res[1].c_str());
		sample_id = ns_atoi64(res[2].c_str());
		region_id = ns_atoi64(res[3].c_str());
		image_id = ns_atoi64(res[4].c_str());
		urgent =			(res[5] == "1");
		processor_id = ns_atoi64(res[6].c_str());
		time_submitted =	        atol(res[7].c_str());
		mask_id	= ns_atoi64(res[8].c_str());
		//x
		maintenance_task	= (ns_maintenance_task)atol(res[9].c_str());
		subregion_position.x = atol(res[10].c_str());
		subregion_position.y = atol(res[11].c_str());
		subregion_size.x = atol(res[12].c_str());
		subregion_size.y = atol(res[13].c_str());
		subregion_start_time =  atol(res[14].c_str());
		subregion_stop_time =  atol(res[15].c_str());
		delete_file_job_id = ns_atoi64(res[16].c_str());
		video_timestamp_type = (ns_processing_job::ns_timestamp_type)(atol(res[17].c_str()));
		maintenance_flag = (ns_maintenance_flag)atol(res[18].c_str());
		pending_another_jobs_completion = atol(res[19].c_str());
		generate_video_at_high_res = atol(res[20].c_str())==1;
		operations.resize((int)ns_process_last_task_marker);
		for (unsigned int i = 0; i < operations.size(); i++)
			operations[i] = (res[i+21] == "1");
	}
	std::string description() const{
		ns_text_stream_t t;
		t<< "id[" << id << "] experiment_id[" << experiment_id << "] sample_id=[" << sample_id << "] region_id[" << region_id
			  << "] image_id[" << image_id << "] urgent[" << (urgent?"1":"0")
			  << "] processor_id[" << processor_id << "] time_submitted[" << time_submitted
			  << "] mask_id[" << mask_id
			  << "] maintenance_task[" << (long)maintenance_task
			  << "] delete_file_job_id[" << delete_file_job_id << "] video_add_timestamp[" << ((int)video_timestamp_type)
			  << "] maintenance_flag[" << (long)maintenance_flag;
		return t.text();
	}
	void save_to_db(ns_sql & sql){
		if (id == 0)
			sql << "INSERT INTO ";
		else sql << "UPDATE ";
		sql << "processing_jobs SET experiment_id = '" << experiment_id << "', sample_id = '" << sample_id << "', region_id = '" << region_id
			  << "', image_id = '" << image_id << "', urgent='" << (urgent?"1":"0")
			  << "', processor_id= '" << processor_id << "', time_submitted='" << time_submitted
			  << "', mask_id = '" << mask_id
			  << "', maintenance_task = '" << (long)maintenance_task
			  << "', subregion_position_x = '" << subregion_position.x << "', subregion_position_y = '" << subregion_position.y
			  << "', subregion_width = '" << subregion_size.x << "', subregion_height='" << subregion_size.y
			  << "', delete_file_job_id = '" << delete_file_job_id << "', video_add_timestamp = '" << ((int)video_timestamp_type)
			  << "', maintenance_flag = '" << (long)maintenance_flag
			  << "', pending_another_jobs_completion = '" << pending_another_jobs_completion;

		for (unsigned int i = 0; i < operations.size(); i++){
			sql << "', op" << ns_to_string(i) << "='";
			if (operations[i])
				sql << "1";
			else sql << "0";
		}
		sql << "'";
		if (id == 0)
			id = sql.send_query_get_id();
		else{
			sql << " WHERE id= " << id;
			sql.send_query();
		}
	}

	//used by the push_scheduler to check in/out jobs.
	ns_64_bit queue_entry_id;
};


#endif
