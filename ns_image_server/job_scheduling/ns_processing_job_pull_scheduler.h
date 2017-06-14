#ifndef NS_PROCESSING_JOB_PULL_SCHEDULER
#define NS_PROCESSING_JOB_PULL_SCHEDULER
#include "ns_image_server_images.h"
#include "ns_processing_job.h"


struct ns_processing_job_pull_finder{
	ns_processing_job_pull_finder():last_job_taken(1024),randomize_experiment_priority(false){}
	ns_text_stream_t job_conditions,
					 data_conditions;
	string job_order;
	string data_tables;
	string data_columns;
	string data_order;
	bool randomize_experiment_priority;

	void load_jobs(ns_sql & sql, bool lock_row=false){
		selected_job = -1;

		jobs.resize(0);
		if (lock_row){	
			sql.set_autocommit(false);
			sql.send_query("BEGIN");
		}
		locked_job = lock_row;
		sql << ns_processing_job::provide_query_stub();
		sql << " FROM processing_jobs WHERE ";
		sql << job_conditions.text();
		sql << " AND processing_jobs.problem = 0"
			<< " ORDER BY experiment_id ";
		if (job_order.size() != 0)
			sql << ", " << job_order;
		if (lock_row) sql << " FOR UPDATE";
		ns_sql_result rows;	
		sql.get_rows(rows);	
		jobs.resize(rows.size());
		if (jobs.size() == 0)
			return;
		unsigned int start(0);
		//if requested, we won't always process the same experiment until
		//it has no jobs left.  Instead, we will randomly choose
		//jobs from each available experiment so they all are worked on evenly
		if (randomize_experiment_priority){
			ns_64_bit last_id = 0;
			vector<ns_64_bit> exp_id_start;
			exp_id_start.reserve(10);
			//go through all the jobs and see the result row
			//at which each experiment's jobs start.
			for (unsigned int i = 0; i < rows.size(); i++){
				ns_64_bit id = ns_atoi64(rows[i][1].c_str());
				if (id != last_id){
					exp_id_start.push_back(i);
					last_id = id;
				}
			}
			//choose a random experiment at which to start looking for jobs
			if (exp_id_start.size() != 0)
				start = exp_id_start[rand()%exp_id_start.size()];
		}
		for (unsigned int i = start; i < rows.size()+start; i++)
			jobs[i%rows.size()].load_from_result(rows[i%rows.size()]);
	}

	bool get_job(ns_sql & sql, bool lock_now=false){
		selected_job++;
		if (jobs.size() == 0)
			return false;
		if (selected_job >= (int)jobs.size())
			return false;
		locked_at_get_job=lock_now;
		if (lock_now){
			if (!locked_job)
				throw ns_ex("ns_processing_job_finder::Requesting immediate job locking when no lock was requested in load_jobs");
			sql << "UPDATE processing_jobs SET currently_under_processing=" << image_server.host_id() << " WHERE id = " << jobs[selected_job].id;
			sql.send_query();
			sql.send_query("COMMIT");
			sql.set_autocommit(true);
		}
		return true;
	}
	void finish_job(ns_sql & sql){
		if (locked_job){
			sql.clear_query();
			sql << "UPDATE processing_jobs SET currently_under_processing=0 WHERE id = " << jobs[selected_job].id;
			sql.send_query();
			sql.send_query("COMMIT");
			sql.set_autocommit(true);
		}
	}

	
	void mark_as_no_jobs_found(ns_sql & sql){
		sql.send_query("COMMIT");
	}
	inline bool get_data(unsigned int & data_id, ns_sql & sql, bool lock_row=false){
		unsigned long t;
		bool r = get_data(t,sql,lock_row);
		data_id = (unsigned int)t;
		return r;
	}
	bool get_data(unsigned long & data_id, ns_sql & sql, bool lock_row=false){
		if (data_columns.size() == 0 || data_tables.size() == 0)
			throw ns_ex("ns_processing_job_finder::No data required for specified job.") << jobs[selected_job].id;
		if (jobs[selected_job].id == 0)
			throw ns_ex("ns_processing_job_finder::Getting data on un-specified job.");
		if (lock_row){	
			sql.set_autocommit(false);
			sql.send_query("BEGIN");
		}
		sql << "SELECT " << data_columns << " FROM processing_jobs, " << data_tables;
		sql << " WHERE processing_jobs.id = " << jobs[selected_job].id << " AND ";
		sql << data_conditions.text();
		sql << " " << data_order;
		if (lock_row) sql << " FOR UPDATE";
		ns_sql_result res;
		sql.get_rows(res);
		if(res.size() == 0)
			return false;
		if (locked_job){
			sql << "UPDATE processing_jobs SET currently_under_processing=" << image_server.host_id() << " WHERE id = " << jobs[selected_job].id;
			sql.send_query();
			if (!lock_row){ //can't commit the processing_job change immediately because that would remove the data table UPDATE lock.
				sql.send_query("COMMIT");
				sql.set_autocommit(true);
			}
		}
		unsigned int i = rand()%(unsigned int)res.size();
		data_id = atoi(res[i][0].c_str());
		return true;

	}
	ns_processing_job & job(){return jobs[selected_job];}
private:
	int selected_job;
	vector<ns_processing_job> jobs;
	bool locked_job;
	unsigned int last_job_taken;
	bool locked_at_get_job;
};


struct ns_processing_job_pull_finder_manager{
	ns_processing_job_pull_finder_manager(const ns_image_server & image_server){ build_premade_sql_statments(image_server);}
	ns_processing_job_pull_finder single_images;
	ns_processing_job_pull_finder experiments;
	ns_processing_job_pull_finder samples;
	ns_processing_job_pull_finder masks;
	ns_processing_job_pull_finder regions;
	ns_processing_job_pull_finder whole_regions;
	ns_processing_job_pull_finder whole_samples;
	ns_processing_job_pull_finder whole_experiments;
	ns_processing_job_pull_finder movement;
	ns_processing_job_pull_finder maintenance;

	void build_premade_sql_statments(const ns_image_server & image_server){
		//generate premade sql statements

		single_images.job_conditions	<< "processing_jobs.image_id != 0 "
										<< "AND processing_jobs.currently_under_processing = 0 "
										<< "AND processing_jobs.problem = 0 AND maintenance_task = 0 ";
		single_images.job_order			= "";
		single_images.data_columns		= "";
		single_images.data_tables		="images";
		single_images.data_conditions	<< "images.problem=0 AND images.currently_under_processing=0 ";	
		single_images.data_order		= "";
		
		experiments.job_conditions		<< "processing_jobs.image_id = 0 AND processing_jobs.region_id = 0 AND processing_jobs.sample_id = 0 AND maintenance_task = 0 ";
		experiments.job_order			= "";
		experiments.data_tables			=  "captured_images";
		experiments.data_columns		=  "captured_images.id";
		experiments.data_conditions		<< "captured_images.experiment_id = processing_jobs.experiment_id AND "
										<< "captured_images.currently_being_processed = 0 "
										<< "AND captured_images.problem = 0 ";	
		experiments.data_order			= "";

		samples.job_conditions 			<< "processing_jobs.image_id = 0 AND maintenance_task = 0 "
							  			<< "AND processing_jobs.region_id = 0 AND processing_jobs.sample_id != 0 "
							  			<< "AND processing_jobs.op" << ns_to_string(ns_process_apply_mask) << " = 0 ";
		samples.job_order				= "";
		samples.data_tables				=  "captured_images";
		samples.data_columns			=  "captured_images.id";
		samples.data_conditions			<< "captured_images.sample_id = processing_jobs.sample_id "
							  			<< "AND captured_images.currently_being_processed = 0 ";	
		samples.data_order				= "";

		masks.job_conditions			<< "processing_jobs.image_id = 0 AND maintenance_task = 0 "
							  			<< "AND processing_jobs.region_id = 0 AND processing_jobs.sample_id != 0 "
							  			<< "AND processing_jobs.op" << ns_to_string(ns_process_apply_mask) << " = 1 ";
		masks.job_order					= "";
		masks.data_tables				=  "captured_images";
		masks.data_columns				=  "captured_images.id";
		masks.data_conditions			<< "captured_images.mask_applied = 0 "
										<< "AND captured_images.sample_id = processing_jobs.sample_id "
										<< "AND captured_images.problem = 0 "	
										<< "AND captured_images.currently_being_processed = 0 ";	
		masks.data_order				= "";

		regions.job_conditions			<< "processing_jobs.image_id = 0 AND processing_jobs.region_id != 0 AND maintenance_task = 0 " 
										<< "AND processing_jobs.op" << ns_to_string(ns_process_compile_video) << "=0 "
										<< "AND processing_jobs.op" << ns_to_string(ns_process_heat_map) << " = 0 "
										<< "AND processing_jobs.op" << ns_to_string(ns_process_static_mask) << " = 0 ";
		regions.job_order				=  "job_name";
		regions.data_tables				=  "sample_region_images, worm_movement";
		regions.data_columns			=  "sample_region_images.id";
		regions.data_conditions			<< "sample_region_images.region_info_id = processing_jobs.region_id "
										<< "AND sample_region_images.problem = 0 "
										<< "AND sample_region_images.worm_movement_id != 0 "
										<< "AND worm_movement.id = sample_region_images.worm_movement_id "
										<< "AND worm_movement.region_id_long != 0 "
										<< "AND worm_movement.id = sample_region_images.worm_movement_id "
										<< "AND worm_movement.currently_under_processing = 0 "
										<< "AND sample_region_images.currently_under_processing = 0 "
										<< "AND (";	
		regions.data_order				= "";
		regions.randomize_experiment_priority = true;

		for (unsigned int i = 1; i < (int)ns_process_last_task_marker; i++){
			if (i == ns_process_analyze_mask ||
				i == ns_process_compile_video ||
				i == ns_process_movement_coloring ||
				i == ns_process_movement_mapping || 
				i == ns_process_posture_vis ||
				i == ns_process_region_interpolation_vis ||
				i == ns_process_region_vis)
				continue;
			if (i != 1)
				regions.data_conditions << "OR ";
			string num = ns_to_string(i);
			if (i == (unsigned int)ns_process_movement_coloring_with_graph)
				regions.data_conditions << " (sample_region_images.op" << num  << "_image_id < processing_jobs.op" << num << " AND sample_region_images.op" << ns_process_movement_coloring << "_image_id != 0) ";
			else
				regions.data_conditions << " sample_region_images.op" << num  << "_image_id < processing_jobs.op" << num << " ";
		}
		regions.data_conditions << ") ";

		whole_regions.job_conditions	<< "processing_jobs.image_id = 0 AND processing_jobs.region_id != 0 AND maintenance_task = 0 AND (";
		if (image_server.compile_videos())
			whole_regions.job_conditions<< "processing_jobs.op" << ns_to_string(ns_process_compile_video) << " != 0 OR ";
		whole_regions.job_conditions	<< "processing_jobs.op" << ns_to_string(ns_process_heat_map) << " != 0 OR "
										<< "processing_jobs.op" << ns_to_string(ns_process_static_mask) << " != 0"
										<< ") AND processing_jobs.currently_under_processing = 0 AND processing_jobs.problem = 0 ";
		whole_regions.job_order			= "";
		whole_regions.data_tables		=  "sample_region_images";
		whole_regions.data_columns		=  "sample_region_images.id";
		whole_regions.data_conditions	<< "sample_region_images.region_info_id = processing_jobs.region_id "
										<< "AND sample_region_images.problem = 0 ";
		whole_regions.data_order		= "";


		whole_samples.job_conditions	<< "processing_jobs.image_id = 0 AND processing_jobs.sample_id != 0 AND processing_jobs.region_id = 0 AND maintenance_task = 0 AND (";
		if (image_server.compile_videos())
			whole_samples.job_conditions<< "processing_jobs.op" << ns_to_string(ns_process_compile_video) << " != 0 OR ";
		whole_samples.job_conditions	<< "processing_jobs.op" << ns_to_string(ns_process_heat_map) << " != 0 OR "
										<< "processing_jobs.op" << ns_to_string(ns_process_static_mask) << " != 0"
		
										<< ") AND processing_jobs.currently_under_processing = 0 AND processing_jobs.problem = 0 ";
		whole_samples.job_order			= "";
		whole_samples.data_tables		=  "captured_images";
		whole_samples.data_columns		=  "captured_images.id";
		whole_samples.data_conditions	<< "captured_images.sample_id = processing_jobs.sample_id "
										<< "AND captured_images.problem = 0 ";
		whole_samples.data_order		= "";


		whole_experiments.job_conditions	<< "processing_jobs.image_id = 0 AND processing_jobs.experiment_id != 0 AND maintenance_task = 0 AND (";
		if (image_server.compile_videos())
			whole_experiments.job_conditions<< "processing_jobs.op" << ns_to_string(ns_process_compile_video) << " != 0 OR ";
		whole_experiments.job_conditions	<< "processing_jobs.op" << ns_to_string(ns_process_heat_map) << " != 0 OR "
										<< "processing_jobs.op" << ns_to_string(ns_process_static_mask) << " != 0"
										<< ") AND processing_jobs.currently_under_processing = 0 AND processing_jobs.problem = 0 ";
		whole_experiments.job_order			= "";
		whole_experiments.data_tables		=  "";
		whole_experiments.data_columns		=  "";
		whole_experiments.data_conditions	<< "";
		whole_experiments.data_order		= "";


		movement.job_conditions			<< "processing_jobs.image_id = 0 AND processing_jobs.region_id != 0 AND maintenance_task = 0 AND ("
										<< "processing_jobs.op"<< (int)ns_process_movement_coloring << " != 0 ||"
										<< "processing_jobs.op"<< (int)ns_process_movement_mapping << " != 0 ||"
										<< "processing_jobs.op"<< (int)ns_process_posture_vis << " != 0 )"
										<< "AND processing_jobs.currently_under_processing = 0 ";
		movement.job_order				= "job_name";
		movement.data_columns			= "worm_movement.id";
		movement.data_tables			= "worm_movement, sample_region_images a, sample_region_images b, sample_region_images c ";
		movement.data_conditions		<< "worm_movement.currently_under_processing = 0 "
										<< "AND worm_movement.problem = 0 "
										<< "AND worm_movement.calculated = 0 "
										<< "AND worm_movement.region_info_id = processing_jobs.region_id "
										<< "AND a.id = worm_movement.region_id_short_1 AND a." << ns_processing_step_db_column_name(ns_process_region_vis) << "!=0 "
										<< "AND b.id = worm_movement.region_id_short_2 AND b." << ns_processing_step_db_column_name(ns_process_region_vis) << "!=0 "
										<< "AND c.id = worm_movement.region_id_long AND c." << ns_processing_step_db_column_name(ns_process_region_vis) << "!=0 "
										<< "AND a.worm_detection_results_id !=0 "
										<< "AND b.worm_detection_results_id !=0 "
										<< "AND c.worm_detection_results_id !=0 "
										<< "AND a.currently_under_processing =0 "
										<< "AND b.currently_under_processing =0 "
										<< "AND c.currently_under_processing =0 ";
		movement.data_order				= "";
		movement.randomize_experiment_priority = true;

		maintenance.job_conditions		<< "processing_jobs.maintenance_task != 0 ";
		maintenance.job_order			= "";
		maintenance.data_columns		= "";
		maintenance.data_tables			= "";
		maintenance.data_conditions		= "";
		maintenance.data_order			= "";

	}
};

#endif
