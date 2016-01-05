#include "ns_processing_job_processor.h"
#include "ns_processing_job_push_scheduler.h"
#include "ns_experiment_storyboard.h"
#include "ns_time_path_image_analyzer.h"
#include "ns_hidden_markov_model_posture_analyzer.h"
#include "ns_hand_annotation_loader.h"
#include "ns_heat_map_interpolation.h"
#include "ns_captured_image_statistics_set.h"
//#include "ns_worm_tracker.h"
#include <map>
#include <vector>
//#include "ns_image_server_time_path_inferred_worm_aggregator.h"

#include "ns_image_server_time_path_inferred_worm_aggregator.h"

using namespace std;
ns_processing_job_processor * ns_processing_job_processor_factory::generate(const ns_processing_job & job, ns_image_server & image_server, ns_image_processing_pipeline * pipeline){
	
	if (!job.has_a_valid_job_type())
		throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Unknown job type found in queue.");

	if (job.is_job_type(ns_processing_job::ns_sample_job) && !job.is_job_type(ns_processing_job::ns_image_job)){
		if (pipeline == 0)
			return new ns_processing_job_sample_processor(job,image_server);
		else 	return new ns_processing_job_sample_processor(job,image_server,*pipeline);
	}
	
	if (job.is_job_type(ns_processing_job::ns_movement_job) && job.movement_record_id != 0){
		throw ns_ex("Depreciated!");
		//	if (image_server==0)
		//			return new ns_processing_job_movement_processor(job);
		//	else	return new ns_processing_job_movement_processor(job,*image_server,*pipeline);
	}
	
	if (job.is_job_type(ns_processing_job::ns_region_job)){
		if (pipeline == 0)
			return new ns_processing_job_region_processor(job,image_server);
			else 	return new ns_processing_job_region_processor(job,image_server,*pipeline);
	}
	
	if (job.is_job_type(ns_processing_job::ns_whole_region_job)){
		if (pipeline == 0)
			return new ns_processing_job_whole_region_processor(job,image_server);
			else 	return new ns_processing_job_whole_region_processor(job,image_server,*pipeline);
	}

	if (job.is_job_type(ns_processing_job::ns_whole_sample_job)){
		if (pipeline == 0)
			return new ns_processing_job_whole_sample_processor(job,image_server);
			else 	return new ns_processing_job_whole_sample_processor(job,image_server,*pipeline);
	}

	if (job.is_job_type(ns_processing_job::ns_maintenance_job)){
		if (pipeline == 0)
			return new ns_processing_job_maintenance_processor(job,image_server);
			else 	return new ns_processing_job_maintenance_processor(job,image_server,*pipeline);
	}

	if (job.is_job_type(ns_processing_job::ns_image_job)){
		if (pipeline == 0)
			return new ns_processing_job_image_processor(job,image_server);
			else 	return new ns_processing_job_image_processor(job,image_server,*pipeline);
	}

	if (job.is_job_type(ns_processing_job::ns_whole_experiment_job) ||
		job.is_job_type(ns_processing_job::ns_experiment_job))
			throw ns_ex("ns_processing_job_processor_factory::generate()::Whole experiment and experiment jobs not implemented.");
	throw ns_ex("ns_processing_job_processor_factory::generate()::Unknown job type requested");
}

void ns_get_worm_detection_model_for_job(ns_processing_job & job, ns_image_server & image_server,ns_sql & sql){
	std::string model_name;
	if (job.region_id== 0){
		throw ns_ex("No region id provided!");
	}
	else {
		sql << "SELECT worm_detection_model FROM sample_region_image_info WHERE "
			<< "sample_region_image_info.id = " << job.region_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("ns_processing_job_scheduler::The specified job refers to a region with an invalid sample reference: ") << job.region_id;
		model_name = res[0][0];
	}
	//else throw ns_ex("ns_processing_job_scheduler::Not enough information specified to get model filename for job");

	if (model_name == "")
		throw ns_ex("ns_processing_job_scheduler::The specified job refers to a sample with no model file specified");
	job.model = &image_server.get_worm_detection_model(model_name);
}

ns_image_server_captured_image_region ns_get_region_image(const ns_processing_job &job){
	ns_image_server_captured_image_region region_image;
	region_image.sample_id					= job.sample_id;
	region_image.experiment_id				= job.experiment_id;
	region_image.sample_name				= job.sample_name;
	region_image.experiment_name			= job.experiment_name;
	region_image.region_images_id			= job.sample_region_image_id;
	region_image.region_info_id				= job.region_id;
	region_image.region_name				= job.region_name;
	return region_image;
}

bool ns_processing_job_sample_processor::job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant){
	sql << "SELECT image_id,small_image_id,mask_applied,currently_being_processed, problem FROM captured_images WHERE id=" << job.captured_image_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		return false;
	//don't process jobs with no source image
	if (atol(res[0][0].c_str()) == 0){
		reason_not_relevant = "Raw captured image does not exist.";
		return false;
	}
	//don't process completed jobs
	if (job.operations[ns_process_thumbnail] && atol(res[0][1].c_str())!=0){
		reason_not_relevant = "Captured image's resized copy has already been calculated.";
		return false;
	}
	//don't process completed jobs
	if (job.operations[ns_process_apply_mask] && atol(res[0][2].c_str())!=0){
		reason_not_relevant = "Captured image has already had its mask applied.";
		return false;
	}
	//don't processes busy or problem jobs
	unsigned long host_id = atol(res[0][3].c_str());

	if (host_id != 0){
		reason_not_relevant = "Captured image is flagged as being processed by";
		reason_not_relevant += ns_to_string(host_id);
		return false;
	}
	if (atol(res[0][4].c_str()) !=0){
		reason_not_relevant = "Captured image is flagged as having a problem";
		return false;
	}
	return true;
}

void ns_processing_job_sample_processor::handle_concequences_of_job_completion(ns_sql & sql){
	if (output_regions.size()){
		ns_image_server_push_job_scheduler push_scheduler;
		push_scheduler.report_sample_region_image(output_regions,sql);
	}
}

void ns_processing_job_sample_processor::mark_subject_as_busy(const bool busy,ns_sql & sql){
	sql << "UPDATE captured_images SET currently_being_processed=" << busy_str(busy) << " WHERE id=" << job.captured_image_id;
	sql.send_query();
}
void ns_processing_job_sample_processor::mark_subject_as_problem(ns_64_bit problem_id,ns_sql & sql){
	ns_image_server_captured_image im;
	im.load_from_db(job.captured_image_id,&sql);
	im.mark_as_problem(&sql,problem_id);
	//sql.send_query();
}

//void ns_processing_job_movement_processor::mark_subject_as_busy(const bool busy,ns_sql & sql){
//	sql << "UPDATE worm_movement SET currently_under_processing = "<< busy_str(busy) << " WHERE id = " << job.movement_record_id;
//	sql.send_query();
//}
//void ns_processing_job_movement_processor::mark_subject_as_problem(const unsigned long problem_id,ns_sql & sql){
//	sql << "UPDATE worm_movement SET problem = "<< problem_id << " WHERE id = " << job.movement_record_id;
//	sql.send_query();
//}

/*bool ns_processing_job_movement_processor::job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant){
	sql << "SELECT problem, currently_under_processing, calculated FROM worm_movement WHERE id = " << job.movement_record_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0){
		reason_not_relevant = "The movement job does not exist";
		return false;
	}
	unsigned long host_id =  atol(res[0][1].c_str());
	if (atol(res[0][0].c_str()) != 0 ){
		reason_not_relevant = "The movement record is flagged as having a problem.";
		return false;
	}
	if (host_id != 0){
		reason_not_relevant = "The movement record is flagged as being processed by";
		reason_not_relevant += ns_to_string(host_id);	
		return false;
	}
	if (atol(res[0][2].c_str()) != 0){
		reason_not_relevant = "The movement record has already been calculated";
		return false;
	}
	return true;
}*/
void ns_processing_job_region_processor::handle_concequences_of_job_completion(ns_sql & sql){
	ns_image_server_push_job_scheduler push_scheduler;
	ns_image_server_captured_image_region region_image(ns_get_region_image(job));
	push_scheduler.report_sample_region_image(std::vector<ns_image_server_captured_image_region>(1,region_image),sql,job.id,ns_processing_job::ns_region_job);
}
void ns_processing_job_region_processor::mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql){
	ns_image_server_captured_image_region reg;
//	reg.load_from_db(job.region_id,&sql);
//	reg.mark_as_problem(&sql,problem_id);
}

bool ns_processing_job_region_processor::job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant){
	ns_image_server_captured_image_region reg;
	reg.load_from_db(job.sample_region_image_id,&sql);
	if (reg.problem_id != 0){
		reason_not_relevant = "The region image is flagged as having a problem.";
		return false;
	}
	if (reg.processor_id != 0){
		reason_not_relevant = "The region image is flagged as being processed by";
		reason_not_relevant += ns_to_string(reg.processor_id);	
		return false;
	}
	for (unsigned int i = 0; i < job.operations.size(); i++){
		if (job.operations[i] && !reg.op_images_[i])
			return true;
	}
	reason_not_relevant = "For this region image, all requested operations have already been calculated";
	return false;
}
void ns_processing_job_region_processor::mark_subject_as_busy(const bool busy,ns_sql & sql){
	ns_image_server_captured_image_region reg;
	reg.load_from_db(job.sample_region_image_id,&sql);
	if (busy)
		reg.mark_as_under_processing(image_server->host_id(),&sql);
	else reg.mark_as_finished_processing(&sql);
}
bool ns_processing_job_whole_region_processor::job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant){
	return true;
}
void ns_processing_job_whole_region_processor::mark_subject_as_busy(const bool busy,ns_sql & sql){
	return;
}
void ns_processing_job_whole_region_processor::mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql){
	return;
}

bool ns_processing_job_whole_sample_processor::job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant){
	return true;
}
void ns_processing_job_whole_sample_processor::mark_subject_as_busy(const bool busy,ns_sql & sql){
	return;
}
void ns_processing_job_whole_sample_processor::mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql){
	return;
}

bool 
	
ns_processing_job_maintenance_processor::job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant){
	return true;
}
void ns_processing_job_maintenance_processor::mark_subject_as_busy(const bool busy,ns_sql & sql){
	return;
}
void ns_processing_job_maintenance_processor::mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql){
	sql << "UPDATE processing_jobs SET problem = " << problem_id << " WHERE id = " << job.id;
	sql.send_query();
	return;
}

bool ns_processing_job_image_processor::job_is_still_relevant(ns_sql & sql, std::string & reason_not_relevant){
	return true;
}
void ns_processing_job_image_processor::mark_subject_as_busy(const bool busy,ns_sql & sql){
	return;
}
void ns_processing_job_image_processor::mark_subject_as_problem(const ns_64_bit problem_id,ns_sql & sql){
	return;
}

bool ns_processing_job_sample_processor::identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql){
	if (job.operations[ns_process_apply_mask] != 0){
		//check to see that the region has a mask
		sql << "SELECT mask_id FROM capture_samples WHERE id = " << job.sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		if(res.size()==0)
			throw ns_ex("ns_image_server_push_job_scheduler::report_new_job()::Cannot find sample id ") << job.sample_id << " in db.";
		if (atol(res[0][0].c_str()) != 0){
			sql << "SELECT id FROM captured_images WHERE sample_id = " << job.sample_id << " AND mask_applied = 0 AND image_id != 0 AND problem = 0 AND censored = 0 AND currently_being_processed=0";
			sql.get_rows(res);
			for (unsigned int i = 0; i < res.size(); i++){
				ns_processing_job_queue_item item;
				item.captured_images_id = atol(res[i][0].c_str());
				item.capture_sample_id = job.sample_id;
				item.job_id = job.id;
				if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
				else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_capture_sample_priority;
				subjects.push_back(item);
			}
		}
		return true;
	}	
	if (job.operations[ns_process_thumbnail] != 0){
		sql << "SELECT id FROM captured_images WHERE sample_id = " << job.sample_id << " AND image_id != 0 AND small_image_id = 0 AND problem = 0 AND currently_being_processed=0";
		ns_sql_result res;
		sql.get_rows(res);
		for (unsigned int i = 0; i < res.size(); i++){
			ns_processing_job_queue_item item;
			item.captured_images_id = atol(res[i][0].c_str());
			item.capture_sample_id = job.sample_id;
			item.job_id = job.id;
			if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
			else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_capture_sample_priority;
			subjects.push_back(item);
		}
		return true;
	}
	return false;
}

/*bool ns_processing_job_movement_processor::identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql){
	
	sql << "SELECT worm_movement.id FROM worm_movement, sample_region_images a, sample_region_images b, sample_region_images c WHERE "
		<< "worm_movement.currently_under_processing = 0 "
		<< "AND worm_movement.problem = 0 "
		<< "AND worm_movement.region_info_id = " << job.region_id << " "
		<< "AND a.id = worm_movement.region_id_short_1 AND a." << ns_processing_step_db_column_name(ns_process_region_vis) << "!=0 AND (";

	//if we're doing a first run movement analysis, require that it hasn't already been calculated
	bool need_or = false;
	if (job.operations[ns_process_movement_coloring]){
		need_or = true;
		sql << "worm_movement.calculated = 0 ";
	}

	//if we're producing visualizations for movement mapping or posture, require that those haven't been calculated
	if (job.operations[ns_process_movement_mapping]){
		if (need_or) sql << " OR ";
		sql << "a." << ns_processing_step_db_column_name(ns_process_movement_mapping) << "=0 ";
		need_or = true;
	}
	if (job.operations[ns_process_posture_vis]){
		if (need_or) sql << " OR ";
		sql << "a." << ns_processing_step_db_column_name(ns_process_posture_vis) << "=0 ";
	}

	sql << ") AND b.id = worm_movement.region_id_short_2 AND b." << ns_processing_step_db_column_name(ns_process_region_vis) << "!=0 "
		<< "AND c.id = worm_movement.region_id_long AND c." << ns_processing_step_db_column_name(ns_process_region_vis) << "!=0 "
		<< "AND a.worm_detection_results_id !=0 "
		<< "AND b.worm_detection_results_id !=0 "
		<< "AND c.worm_detection_results_id !=0 "
		<< "AND a.currently_under_processing =0 "
		<< "AND b.currently_under_processing =0 "
		<< "AND c.currently_under_processing =0 ";
	//ofstream foo("c:\\query.txt");
	//foo << sql.query();
	//foo.close();
	ns_sql_result res;
	sql.get_rows(res);
	
	for (unsigned int i = 0; i < res.size(); i++){
		ns_processing_job_queue_item item;
		item.movement_record_id = atol(res[i][0].c_str());
		item.job_id = job.id;
		if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
		else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_movement_priority;
		subjects.push_back(item);
	}
	return true;
}
*/

bool ns_processing_job_region_processor::identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql){
	sql << "SELECT id FROM sample_region_images WHERE region_info_id=" << job.region_id << " AND problem =0 AND censored = 0 AND currently_under_processing=0 AND (";
	bool requested_constraint(false);
	for (unsigned int i = 1; i < (int)ns_process_last_task_marker; i++){
		if (i == ns_process_analyze_mask ||		 i == ns_process_compile_video ||
			i == ns_process_movement_coloring || i == ns_process_movement_mapping || 
			i == ns_process_posture_vis  || i == ns_process_region_vis) continue;
		if (job.operations[i] == 1){
			if (requested_constraint) sql << "OR ";
			else requested_constraint = true;
			if (i == (unsigned int)ns_process_movement_coloring_with_graph || i == (unsigned int)ns_process_movement_coloring_with_survival)
				sql << " (sample_region_images.op" << i  << "_image_id=0 AND sample_region_images.op" << ns_process_movement_coloring << "_image_id != 0) ";
			else
				sql<< " sample_region_images.op" << i  << "_image_id=0 ";
		}
	}
	sql<< ") ";
	ns_sql_result res;
	sql.get_rows(res);
//	ns_sql_full_table_lock lock(sql,"processing_job_queue");
	for (unsigned int i = 0; i < res.size(); i++){
		ns_processing_job_queue_item item;
		item.sample_region_info_id = job.region_id;
		item.sample_region_image_id = atol(res[i][0].c_str());
		item.job_id = job.id;
		if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
		else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_region_priority;
		subjects.push_back(item);
	}
	//XXX lock.unlock();
	return true;
}

bool ns_processing_job_whole_region_processor::identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql){
	//we check to see if there is at least one frame existing in the region
	sql << "SELECT id FROM sample_region_images WHERE region_info_id=" << job.region_id << " LIMIT 1";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("No sample region images exist for the specified whole region job.");
	
	//ns_sql_full_table_lock lock(sql,"processing_job_queue");
	ns_processing_job_queue_item item;
	item.sample_region_info_id = job.region_id;
	item.sample_region_image_id = atol(res[0][0].c_str());
	item.job_id = job.id;
	if (job.operations[(int)ns_process_compile_video] != 0)
		item.job_class = 1;
	if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
	else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_whole_region_priority;
	subjects.push_back(item);
	return true;

}
bool ns_processing_job_whole_sample_processor::identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql){
	sql << "SELECT id FROM captured_images WHERE sample_id=" << job.sample_id << " LIMIT 1";
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("No captured images exist for the specified whole region job.");
	
	ns_processing_job_queue_item item;
	item.capture_sample_id = job.sample_id;
	item.captured_images_id = atol(res[0][0].c_str());
	item.job_id = job.id;
	if (job.operations[(int)ns_process_compile_video] != 0)
		item.job_class = 1; //only process one video per computer (because the code uses all processors effectively)
	if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
	else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_whole_region_priority;
	subjects.push_back(item);
	return true;
}
bool ns_processing_job_maintenance_processor::identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql){
	ns_processing_job_queue_item item;
	item.experiment_id = job.experiment_id;
	item.capture_sample_id = job.sample_id;
	item.sample_region_info_id = job.region_id;
	item.job_id = job.id;
	if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
	else if (job.maintenance_task == ns_maintenance_compress_stored_images)
		item.priority = ns_image_server_push_job_scheduler::ns_job_queue_background_priority;
	else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_maintenance_priority;
	//if (job.maintenance_task ==  ns_maintenance_rebuild_movement_data)
	//	item.job_class = 1; //only process one video per computer (because the code uses lots of memory)
//	item.save_to_db(sql);
	subjects.push_back(item);
	return true;
}


bool ns_processing_job_image_processor::identify_subjects_of_job_specification(std::vector<ns_processing_job_queue_item> & subjects,ns_sql & sql){
	if (job.operations[ns_process_analyze_mask] == 0)
			throw ns_ex("ns_image_server_push_job::report_new_job::(Whole) Experiment and image jobs are not supported.");
		
	ns_processing_job_queue_item item;
	item.job_id = job.id;
	item.image_id = job.image_id;
	if (job.urgent) item.priority = ns_image_server_push_job_scheduler::ns_job_queue_urgent_priority;
	else item.priority = ns_image_server_push_job_scheduler::ns_job_queue_image_priority;
	subjects.push_back(item);
	return true;
	
}

bool ns_processing_job_sample_processor::run_job(ns_sql & sql){
	ns_image_server_captured_image sample;
	sample.captured_images_id		= job.captured_image_id;
	sample.sample_id				= job.sample_id;
	sample.experiment_id			= job.experiment_id;
	sample.sample_name				= job.sample_name;
	sample.experiment_name			= job.experiment_name;
	//if we're doing anything other than applying the mask, we can't handle it
	if (!job.operations[ns_process_apply_mask] && !job.operations[ns_process_thumbnail])
		throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Operations other than masks and resize cannot currently be applied to samples");
	
	/*get_model(job,sql);
	if (job.model == 0)
		throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Attempting to run task on region without specifying model");
	*/
	if (job.operations[ns_process_apply_mask]){
		pipeline->apply_mask(sample,output_regions,sql);
	}
	else if (job.operations[ns_process_thumbnail]){
		pipeline->resize_sample_image(sample,sql);
	}
	return true;	
}
/*
bool ns_processing_job_movement_processor::run_job(ns_sql & sql){
	ns_worm_movement_measurement_set movement_record;
	movement_record.id = job.movement_record_id;
	pipeline->characterize_movement(movement_record, job.operations, sql);
	return true;
}*/
bool ns_processing_job_region_processor::run_job(ns_sql & sql){
	ns_image_server_captured_image_region region_image(ns_get_region_image(job));
	
	if (job.operations[ns_process_thumbnail])
		pipeline->resize_region_image(region_image,sql);
	
	//load cached graph if necessary.
	if (job.operations[ns_process_worm_detection_with_graph] || 
		job.operations[ns_process_movement_coloring_with_graph] || 
		job.operations[ns_process_movement_paths_visualization] || 
		job.operations[ns_process_movement_paths_visualition_with_mortality_overlay] ||
		job.operations[ns_process_movement_posture_visualization] ||
		job.operations[ns_process_movement_posture_aligned_visualization]){

		job.death_time_annotations = &pipeline->lifespan_curve_cache.get_experiment_data(job.experiment_id,sql);
	}
	
	bool detection_calculation_required(false);
	for (unsigned int i = 0; i < job.operations.size(); i++){
		if (job.operations[i] && ns_image_processing_pipeline::detection_calculation_required((ns_processing_task)i)){
			detection_calculation_required = true;
			break;
		}
	}

	if (detection_calculation_required){
		ns_get_worm_detection_model_for_job(job,*image_server,sql);
		if (job.model == 0)
			throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Attempting to run task on region without specifying model");
	}
	if (job.death_time_annotations != 0)
		pipeline->process_region(region_image,job.operations,sql,*job.model,*job.death_time_annotations);
	else{
		ns_lifespan_curve_cache_entry s;
		pipeline->process_region(region_image,job.operations,sql,*job.model,s);
	}
	return true;
}

bool ns_processing_job_whole_region_processor::run_job(ns_sql & sql){
	
	ns_image_server_captured_image_region region_image;
	region_image.sample_id					= job.sample_id;
	region_image.experiment_id				= job.experiment_id;
	region_image.sample_name				= job.sample_name;
	region_image.experiment_name			= job.experiment_name;
	region_image.region_info_id				= job.region_id;
	region_image.region_images_id			= job.sample_region_image_id;
	region_image.region_name				= job.region_name;
	if (!image_server->compile_videos() && job.operations[(int)ns_process_compile_video])
		image_server->register_server_event(ns_image_server_event("Video job accepted by a node that cannot compile videos."),&sql);

	if (job.operations[(int)ns_process_compile_video]){
		image_server->register_server_event(
			ns_image_server_event("Compiling Video for '")
			<< ns_processing_task_to_string(ns_process_compile_video) << "' on"
			<< job.experiment_name <<"(" << job.experiment_id << ")" << job.sample_name << "(" << job.sample_id << ")" 
			<< job.region_name << "(" << job.region_id << ")",&sql
		);
		sql << "SELECT time_at_which_animals_had_zero_age,time_of_last_valid_sample FROM sample_region_image_info WHERE id = " << job.region_id;
		ns_sql_result res;
		sql.get_rows(res);
		if(res.size() == 0)
			throw ns_ex("Could not load region");
		const unsigned long zero_time(atol(res[0][0].c_str()));
		const unsigned long default_stop_time(atol(res[0][1].c_str()));
#ifndef NS_NO_XVID
		if (job.subregion_stop_time == 0)
			job.subregion_stop_time = default_stop_time;

		pipeline->compile_video(region_image,job.operations,
												ns_video_region_specification(	job.subregion_position.x,
													 job.subregion_position.y,
													 job.subregion_size.x,
													 job.subregion_size.y,
													 job.subregion_start_time,
													 job.subregion_stop_time,
													 (ns_video_region_specification::ns_timestamp_type)((int)job.video_timestamp_type),
													 zero_time),
													 sql);
#else
		throw ns_ex("The NS_NO_XVID flag was set, so xvid support has been disabled");
#endif
	}
	else if (job.operations[(int)ns_process_heat_map] || job.operations[(int)ns_process_static_mask]){
		image_server->register_server_event(
			ns_image_server_event("Processing '")
			<< ((job.operations[(int)ns_process_heat_map])?ns_processing_task_to_string(ns_process_heat_map):"") << " "
			<< ((job.operations[(int)ns_process_static_mask])?ns_processing_task_to_string(ns_process_static_mask):"") << " on "
			<< job.experiment_name <<"(" << job.experiment_id << ")" << job.sample_name << "(" << job.sample_id << ")" 
			<< job.region_name << "(" << job.region_id << ")",&sql
		);
		pipeline->calculate_static_mask_and_heat_map(job.operations,region_image,sql);
	}
	else if (job.operations[(int)ns_process_temporal_interpolation]){
		image_server->register_server_event(
			ns_image_server_event("Running Temporal Interpolation for '")
			<< ns_processing_task_to_string(ns_process_temporal_interpolation) << "' on"
			<< job.experiment_name <<"(" << job.experiment_id << ")" << job.sample_name << "(" << job.sample_id << ")" 
			<< job.region_name << "(" << job.region_id << ")",&sql
		);
		throw ns_ex("Depreciated!");
		//pipeline->calculate_temporal_interpolation(job.operations,region_image,sql);
	}
	else throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Unknown whole region job type.");				
	return true;
}

bool ns_processing_job_whole_sample_processor::run_job(ns_sql & sql){
	ns_image_server_captured_image sample_image;
	sample_image.sample_id					= job.sample_id;
	sample_image.experiment_id				= job.experiment_id;
	sample_image.sample_name				= job.sample_name;
	sample_image.experiment_name			= job.experiment_name;
	sample_image.captured_images_id			= job.captured_image_id;
	if (!image_server->compile_videos() && job.operations[(int)ns_process_compile_video]){
		image_server->register_server_event(ns_image_server_event("Video job accepted by a node that cannot compile videos."),&sql);
	}
	
	if (job.operations[(int)ns_process_compile_video]){
		sql << "SELECT MIN(capture_time) FROM captured_images WHERE sample_id = " << job.sample_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			throw ns_ex("Sample has no images!");
		const unsigned long zero_time(atol(res[0][0].c_str()));
		#ifndef NS_NO_XVID
		pipeline->compile_video(sample_image,job.operations,
					 ns_video_region_specification(	job.subregion_position.x,
													 job.subregion_position.y,
													 job.subregion_size.x,
													 job.subregion_size.y,
													 job.subregion_start_time,
													 job.subregion_stop_time,
													 (ns_video_region_specification::ns_timestamp_type)((int)job.video_timestamp_type),
													 zero_time),
		
#else
		
		throw ns_ex("The NS_NO_XVID flag was set, so xvid support has been disabled");
#endif
													sql);
	}

	else if (job.operations[(int)ns_process_heat_map] || job.operations[(int)ns_process_static_mask])
		throw ns_ex("Cannot calculate heat map for an entire sample");
	else throw ns_ex("Unknown whole region job type");
	return true;
}


void ns_processing_job_maintenance_processor::delete_job(ns_sql & sql){
	if (!delete_job_){
		sql << "UPDATE processing_jobs SET currently_under_processing=0 WHERE id=" << job.id;
		sql.send_query();
		if (parent_job.id != 0){
			sql << "UPDATE processing_jobs SET currently_under_processing=0 WHERE id=" << parent_job.id;
			sql.send_query();
		}
		return;
	}
	//delete the job from the queue.
	sql << "DELETE from processing_jobs WHERE id=" << job.id;
	sql.send_query();
	if (parent_job.id != 0){
		sql << "DELETE from processing_jobs WHERE id=" << parent_job.id;
		sql.send_query();
	}
}
void ns_storyboard_spec_from_job(const ns_processing_job & job, std::vector<ns_experiment_storyboard_spec> & specs){
	specs.resize(0);
	ns_experiment_storyboard_spec spec;
	if (job.region_id != 0)
		spec.region_id = job.region_id;
	else if (job.sample_id != 0)
		spec.sample_id = job.sample_id;
	else if (job.experiment_id != 0)
		spec.experiment_id = job.experiment_id;
	for (unsigned int i = 0; i < (int)ns_experiment_storyboard_spec::ns_number_of_flavors; i++){
		spec.set_flavor((ns_experiment_storyboard_spec::ns_storyboard_flavor)i);
		specs.push_back(spec);
	}
}

void ns_processing_job_maintenance_processor::flag_job_as_being_processed(ns_sql & sql){
	if (parent_job.id != 0){
		sql << "UPDATE processing_jobs SET currently_under_processing=" << image_server->host_id() << " WHERE id=" << parent_job.id;
		sql.send_query();
	}
	ns_processing_job_processor::flag_job_as_being_processed(sql);
}


bool ns_processing_job_maintenance_processor:: run_job(ns_sql & sql){
	delete_job_ = true;

//	ns_movement_database_maintainer m;

	image_server->register_server_event(
		ns_image_server_event("Performing maintenance task '")
			<< ns_maintenance_task_to_string(job.maintenance_task) << "' on "
			<< job.experiment_name <<"(" << job.experiment_id << ")" << job.sample_name << "(" << job.sample_id << ")" 
			<< job.region_name << "(" << job.region_id << ")",&sql
		);

	//just so the web-based UI shows that the job is being processed.  Not needed for locking.
	sql << "UPDATE processing_jobs SET currently_under_processing=" << image_server->host_id() << " WHERE id=" << job.id;
	sql.send_query();
	switch(job.maintenance_task){
	case ns_maintenance_generate_sample_regions_from_mask:{
			if (job.sample_id == 0)
				throw ns_ex("No sample specified for region generation from mask");
			sql << "SELECT image_resolution_dpi FROM capture_samples WHERE id = " << job.sample_id;
			ns_sql_result res;
			if (res.size() == 0)
				throw ns_ex("Could not find sample ") << job.sample_id << " in db.";

			ns_image_processing_pipeline::generate_sample_regions_from_mask(job.sample_id,atof(res[0][0].c_str()),sql);
			break;
		}
	case ns_maintenance_recalc_image_stats:{

		ns_image_server_results_subject sub;
		if (job.region_id != 0){
			sub.region_id = job.region_id;
			ns_region_metadata metadata;
			metadata.load_from_db(job.region_id,"",sql);
			ns_capture_sample_region_data data;
			//this will recalculate image statistics for all timepoints and save them to db.
			data.load_from_db(job.region_id,metadata,false,false,sql,true);
		
			//ns_image_server_results_file f(image_server->results_storage.capture_region_image_statistics(sub,sql,false));
			//ns_acquire_for_scope<ostream> o(f.output());
			//ns_capture_sample_region_data::output_region_data_in_jmp_format_header("",o());
			//data.output_region_data_in_jmp_format(o());
			//o.release();
		}else{
			//whole experiment
			ns_image_server_results_subject sub;
			sub.experiment_id = job.experiment_id;
			ns_image_server_results_file f(image_server->results_storage.capture_region_image_statistics(sub,sql,false));
	
			ns_capture_sample_region_statistics_set set;
			set.load_whole_experiment(job.experiment_id,sql,true);

			ns_acquire_for_scope<ostream> o(f.output());
			ns_capture_sample_region_data::output_region_data_in_jmp_format_header("",o());

			for (unsigned int j = 0; j < set.regions.size(); j++)
				set.regions[j].output_region_data_in_jmp_format(o());

			o.release();

		}
	
	break;
										   }
	case ns_maintenance_rerun_image_registration:{
		if (job.region_id == 0)
				throw ns_ex("No region specified for re-registration");
			ns_rerun_image_registration(job.region_id,sql);
			break;
		}
		
		case ns_maintenance_rebuild_movement_data:{
		case ns_maintenance_rebuild_movement_from_stored_images:
		case ns_maintenance_rebuild_movement_from_stored_image_quantification:
			if (job.region_id == 0)
				throw ns_ex("Movement data can be rebuilt only for regions.");
		
			ns_high_precision_timer tm;
			tm.start();

			ns_image_server_results_subject results_subject;
			results_subject.region_id = job.region_id;
			
			
			//first we solve the point cloud to find non-translating animals

		//	ns_image_server_results_subject sub2;
		//	sub2.region_id = job.region_id;
		//	image_server->results_storage.time_path_image_analysis_quantification(sub2,"detailed",false,sql).output();
			//ns_analyzed_image_time_path::write_detailed_movement_quantification_analysis_header(o());
		//	o() << "\n";
			
			const bool skip_inferred_worm_analysis(image_server->get_cluster_constant_value("skip_inferred_worm_analysis","false",&sql) != "false");

			ns_time_path_solution time_path_solution;
			if (skip_inferred_worm_analysis){
				try{
					time_path_solution.load_from_db(job.region_id,sql,false); 
				}
				catch(ns_ex & ex){
					ns_time_path_solver tp_solver;
					tp_solver.load(job.region_id,sql);
					ns_time_path_solver_parameters solver_parameters(ns_time_path_solver_parameters::default_parameters(job.region_id,sql));
					tp_solver.solve(solver_parameters,time_path_solution);
					time_path_solution.save_to_db(job.region_id,sql);
				}
			}
			else if( job.maintenance_task == ns_maintenance_rebuild_movement_from_stored_images || 
				job.maintenance_task == ns_maintenance_rebuild_movement_from_stored_image_quantification){
				image_server->register_server_event(ns_image_server_event("Loading point cloud solution from disk."),&sql);
				time_path_solution.load_from_db(job.region_id,sql,false); 
			}
			else{
				unsigned long count(0);
				while(true){
					image_server->register_server_event(ns_image_server_event("Solving (x,y,t) point cloud."),&sql);
					ns_time_path_solver_parameters solver_parameters(ns_time_path_solver_parameters::default_parameters(job.region_id,sql));
			
					ns_time_path_solver tp_solver;
					tp_solver.load(job.region_id,sql);
					tp_solver.solve(solver_parameters,time_path_solution);
				
					image_server->register_server_event(ns_image_server_event("Filling gaps and adding path prefixes."),&sql);
					time_path_solution.fill_gaps_and_add_path_prefixes(ns_time_path_solution::default_length_of_fast_moving_prefix());
				
					//unnecissary save, done for debug
				//	time_path_solution.save_to_db(job.region_id,sql);
				
					image_server->register_server_event(ns_image_server_event("Caching image data for inferred worm positions."),&sql);
					ns_image_server_time_path_inferred_worm_aggregator ag;
					bool solution_needs_to_be_rebuilt(ag.create_images_for_solution(job.region_id,time_path_solution,sql));
					//corrupt images are marked as problematic in the db and have their worm detection deleted. Thus, we need to rebuild the 
					//solution in order to account for these deletions.
					if (solution_needs_to_be_rebuilt){
						if (count > 2)
							throw ns_ex("Multiple attempts at analysis turned up corrupt images.  They all should have been caught in the first round, so we're giving up.");
						image_server->register_server_event(ns_image_server_event("Corrupt images were found and excluded.  Attempting to re-run analysis.."),&sql);
						count++;
					}
					else break;
				}
				
				//ns_worm_tracking_model_parameters tracking_parameters(ns_worm_tracking_model_parameters::default_parameters());
				//ns_worm_tracker worm_tracker;
				//worm_tracker.track_moving_worms(tracking_parameters,time_path_solution);
				time_path_solution.save_to_db(job.region_id,sql);
				
				ns_acquire_for_scope<ostream> position_3d_file_output(
				image_server->results_storage.animal_position_timeseries_3d(
					results_subject,sql,ns_image_server_results_storage::ns_3d_plot
				).output()
				);
				time_path_solution.output_visualization_csv(position_3d_file_output());
				position_3d_file_output.release();

				image_server->results_storage.write_animal_position_timeseries_3d_launcher(results_subject,ns_image_server_results_storage::ns_3d_plot,sql);
				//return true;
			}


			ns_time_path_image_movement_analyzer time_path_image_analyzer;
			ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
				ns_get_death_time_estimator_from_posture_analysis_model(
				image_server->get_posture_analysis_model_for_region(job.region_id,sql)));
			const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(job.region_id,sql));

			if (job.maintenance_task==ns_maintenance_rebuild_movement_data){
				//load in worm images and analyze them for non-translating animals for posture changes
				image_server->register_server_event(ns_image_server_event("Analyzing images for animal posture changes."),&sql);

				time_path_image_analyzer.process_raw_images(job.region_id,time_path_solution,time_series_denoising_parameters,&death_time_estimator(),sql,-1,true);
				time_path_image_analyzer.save_movement_data_to_db(job.region_id,sql);
			}
			else if (job.maintenance_task==ns_maintenance_rebuild_movement_from_stored_images){
				//load in previously calculated image quantification from disk for re-analysis
				//This is good, for example, if changes have been made to the movement quantification analyzer
				//and it needs to be rerun though the actual images don't need to be requantified.
				image_server->register_server_event(ns_image_server_event("Quantifying stored animal posture images"),&sql);
				time_path_image_analyzer.reanalyze_stored_aligned_images(job.region_id,time_path_solution,time_series_denoising_parameters,&death_time_estimator(),sql,true);
				time_path_image_analyzer.save_movement_data_to_db(job.region_id,sql);
			}
			else{
				//load in previously calculated image quantification from disk for re-analysis
				//This is good, for example, if changes have been made to the movement quantification analyzer
				//and it needs to be rerun though the actual images don't need to be requantified.
				//or if by hand annotations have been performed and the alignment in the output files should be redone.
				image_server->register_server_event(ns_image_server_event("Analyzing stored animal posture quantification."),&sql);
				time_path_image_analyzer.load_completed_analysis(job.region_id,time_path_solution,time_series_denoising_parameters,&death_time_estimator(),sql);
				time_path_image_analyzer.save_movement_data_to_db(job.region_id,sql);
			}
			death_time_estimator.release();
			
			ns_hand_annotation_loader by_hand_region_annotations;
			ns_region_metadata metadata;
			try{
				metadata = by_hand_region_annotations.load_region_annotations(ns_death_time_annotation_set::ns_censoring_and_movement_transitions,job.region_id,sql);
			}
			catch(ns_ex & ex){
				image_server->register_server_event(ex,&sql);
				metadata.load_from_db(job.region_id,"",sql);
			}

			ns_death_time_annotation_set set;
			time_path_image_analyzer.produce_death_time_annotations(set);
			ns_death_time_annotation_compiler compiler;
			compiler.add(set);
			compiler.add(by_hand_region_annotations.annotations);
			vector<ns_ex> censoring_file_io_problems;
			ns_ex censoring_problem;
			try{
				ns_death_time_annotation_set censoring_set;
				//calculate censoring events according to different censoring strategies
				ns_death_time_annotation::ns_by_hand_annotation_integration_strategy by_hand_annotation_integration_strategy[2] = 
				{ns_death_time_annotation::ns_machine_annotations_if_no_by_hand,ns_death_time_annotation::ns_only_machine_annotations};

				for (unsigned int bhais = 0; bhais < 2; bhais++){
					for (unsigned int censoring_strategy = 0; censoring_strategy < (int)ns_death_time_annotation::ns_number_of_multiworm_censoring_strategies; censoring_strategy++){
						if (censoring_strategy == (int)ns_death_time_annotation::ns_by_hand_censoring)
							continue;
						
							ns_worm_movement_summary_series summary_series;
			
	
						if (censoring_strategy == ns_death_time_annotation::ns_merge_multiple_worm_clusters_and_missing_and_censor){
							summary_series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
																 (ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
																  ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_missing_times,
																	compiler,ns_include_unchanged);
							summary_series.generate_censoring_annotations(metadata,censoring_set);
							try{
								ns_image_server_results_file movement_timeseries(image_server->results_storage.movement_timeseries_data(
									by_hand_annotation_integration_strategy[bhais],
									(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
									ns_death_time_annotation::ns_censoring_assume_uniform_distribution_of_missing_times,
									ns_include_unchanged,
									results_subject,"single_region","movement_timeseries",sql));
								ns_acquire_for_scope<std::ostream> movement_out(movement_timeseries.output());
								ns_worm_movement_measurement_summary::out_header(movement_out());
								summary_series.to_file(metadata,movement_out());
								movement_out.release();
							}catch(ns_ex & ex){
								censoring_file_io_problems.push_back(ex);
							}

						}
						else{
							summary_series.from_death_time_annotations(by_hand_annotation_integration_strategy[bhais],
																		(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
																		ns_death_time_annotation::default_missing_return_strategy(),
																	compiler,ns_include_unchanged);
							summary_series.generate_censoring_annotations(metadata,censoring_set);
					
							try{
								ns_image_server_results_file movement_timeseries(image_server->results_storage.movement_timeseries_data(by_hand_annotation_integration_strategy[bhais],
									(ns_death_time_annotation::ns_multiworm_censoring_strategy)censoring_strategy,
									ns_death_time_annotation::ns_censoring_minimize_missing_times,
									ns_include_unchanged,
									results_subject,"single_region","movement_timeseries",sql));
								ns_acquire_for_scope<std::ostream> movement_out(movement_timeseries.output());
								ns_worm_movement_measurement_summary::out_header(movement_out());
								summary_series.to_file(metadata,movement_out());
								movement_out.release();
							}catch(ns_ex & ex){
								censoring_file_io_problems.push_back(ex);
							}


						}
					}
					set.add(censoring_set);
				}
			}
			catch(ns_ex & ex){
				censoring_problem = ex;
			}


			//output worm movement and death time annotations to disk

			ns_image_server_results_file censoring_results(image_server->results_storage.machine_death_times(results_subject,ns_image_server_results_storage::ns_censoring_and_movement_transitions,
						"time_path_image_analysis",sql));
			ns_image_server_results_file state_results(image_server->results_storage.machine_death_times(results_subject,ns_image_server_results_storage::ns_worm_position_annotations,
						"time_path_image_analysis",sql));

			ns_acquire_for_scope<std::ostream> censoring_out(censoring_results.output());
			ns_acquire_for_scope<std::ostream> state_out(state_results.output());
			set.write_split_file_column_format(censoring_out(),state_out());
			censoring_out.release();
			state_out.release();

			//ns_image_server_results_file results(image_server->results_storage.machine_death_times(results_subject,"time_path_image_analysis",sql));
			//ns_acquire_for_scope<ostream> tp_ia_out(results.output());
			//set.write(tp_ia_out());
			//tp_ia_out.release();



			//output worm movement path summaries to disk

			//when outputting analysis files for movement detection, we can use by-hand annotations of movement cessation times to align
			//movement quantification to gold-standard events.
			//ns_region_metadata metadata;
			
			time_path_image_analyzer.add_by_hand_annotations(by_hand_region_annotations.annotations);

			ns_image_server_results_subject sub;
			sub.region_id = job.region_id;	
			try{
				ns_acquire_for_scope<ostream> o(image_server->results_storage.time_path_image_analysis_quantification(sub,"detailed",false,sql).output());
				if (time_path_image_analyzer.size() > 0){
					time_path_image_analyzer.group(0).paths[0].write_detailed_movement_quantification_analysis_header(o());
					time_path_image_analyzer.write_detailed_movement_quantification_analysis_data(metadata,o(),false);
				}
				else o() << "(No Paths in Solution)\n";
		
				o() << "\n";
				o.release();
			}
			catch(ns_ex & ex){
				image_server->register_server_event(ex,&sql);
			}

			if (0){
				//generate optimization training set 
				vector<double> thresholds(25);
				//log scale between .1 and .0001
				for (unsigned int i = 0; i < 25; i++)
					thresholds[i] = pow(10,-1-(3.0*i/25));
				vector<double> hold_times(12);
				hold_times[0] = 0;
				hold_times[1] = 60*15;
				for (unsigned int i = 0; i < 10; i++)
					hold_times[i+2] = i*60*60;

				ns_acquire_for_scope<ostream> o2(image_server->results_storage.time_path_image_analysis_quantification(sub,"optimization_stats",false,sql).output());
				ns_analyzed_image_time_path::write_analysis_optimization_data_header(o2());
				o2() << "\n";
				time_path_image_analyzer.write_analysis_optimization_data(thresholds,hold_times,metadata,o2());
			}

			//update db stats
			sql << "UPDATE sample_region_image_info SET "
				<< "latest_movement_rebuild_timestamp = UNIX_TIMESTAMP(NOW()),"
				<< "last_timepoint_in_latest_movement_rebuild = " << time_path_image_analyzer.last_timepoint_in_analysis() << ","
				<< "number_of_timepoints_in_latest_movement_rebuild = " << time_path_image_analyzer.number_of_timepoints_in_analysis()
				<< " WHERE id = " << job.region_id;
			sql.send_query();
			if (!censoring_file_io_problems.empty()){
				ns_ex ex("One or more file i/o issues encountered during analysis:");
				for (unsigned long i = 0; i < censoring_file_io_problems.size(); i++)
					ex << "(1)" << censoring_file_io_problems[i].text();
				image_server->register_server_event(ex,&sql);
			}
			if (!censoring_problem.text().empty())
				throw ns_ex("A problem occurred when trying to generate censoring information: ") << censoring_problem.text() << ". Deaths were estimated but no censoring data written";
			
		//	image_server->performance_statistics.register_job_duration(ns_process_movement_paths_visualization,tm.stop());
			break;
		}
		case ns_maintenance_generate_movement_posture_visualization:{
			ns_high_precision_timer tm;
			tm.start();
			ns_time_path_solution solution;
			solution.load_from_db(job.region_id,sql,true);
			ns_time_path_image_movement_analyzer analyzer;
			const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(job.region_id,sql));

			ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
				ns_get_death_time_estimator_from_posture_analysis_model(
				image_server->get_posture_analysis_model_for_region(job.region_id,sql)));
			analyzer.load_completed_analysis(job.region_id,solution, time_series_denoising_parameters,&death_time_estimator(),sql);
			death_time_estimator.release();
			analyzer.generate_movement_posture_visualizations(false,job.region_id,solution,sql);
			image_server->performance_statistics.register_job_duration(ns_process_movement_posture_visualization,tm.stop());
			break;
		}		
		case ns_maintenance_generate_movement_posture_aligned_visualization:{
			ns_high_precision_timer tm;
			tm.start();
			ns_time_path_solution solution;
			solution.load_from_db(job.region_id,sql,true);
			const ns_time_series_denoising_parameters time_series_denoising_parameters(ns_time_series_denoising_parameters::load_from_db(job.region_id,sql));

			ns_time_path_image_movement_analyzer analyzer;
			ns_acquire_for_scope<ns_analyzed_image_time_path_death_time_estimator> death_time_estimator(
				ns_get_death_time_estimator_from_posture_analysis_model(
				image_server->get_posture_analysis_model_for_region(job.region_id,sql)));
			analyzer.load_completed_analysis(job.region_id,solution,time_series_denoising_parameters,&death_time_estimator(),sql);
			death_time_estimator.release();
			analyzer.generate_death_aligned_movement_posture_visualizations(false,job.region_id,ns_movement_cessation,solution,sql);
			image_server->performance_statistics.register_job_duration(ns_process_movement_posture_aligned_visualization,tm.stop());
			break;
		}
	

		case ns_maintenance_rebuild_sample_time_table:{
			
			throw ns_ex("Depreciated!");
			/*
			if (job.region_id != 0)
				throw ns_ex("ns_processing_job_scheduler::Cannot rebuild sample image table for just a single region!");
			if (job.sample_id != 0)
				m.build_sample_time_relationship_table_from_captured_images(job.sample_id, 0, sql);
			else if (job.experiment_id != 0)
				m.rebuild_sample_time_relationship_table(sql, job.experiment_id);
			else 
				m.rebuild_sample_time_relationship_table(sql);
			break;*/
		}
		case ns_maintenance_check_for_file_errors:{
				ns_check_for_file_errors(job,sql);
			break;
		 }
		case ns_maintenance_determine_disk_usage:{
			if (job.experiment_id == 0 || job.sample_id != 0)
				throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Disk Usage stats can only be calculated on entire experiments");
			image_server->calculate_experiment_disk_usage(job.experiment_id,sql);
			break;
		}
		case ns_maintenance_compress_stored_images:{
			std::vector<std::string> image_columns_to_convert;
			image_columns_to_convert.push_back("image_id");
			image_columns_to_convert.push_back(ns_processing_step_db_column_name(ns_process_spatial)); 
			image_columns_to_convert.push_back(ns_processing_step_db_column_name(ns_process_worm_detection)); 
			const unsigned long number_of_images_to_run_per_batch(10);
			//translate experiment-wide or sample-wide jobs into a bunch of region jobs
			vector<unsigned long> region_ids_to_spawn;
			if (job.image_id == 0){
				if (job.region_id == 0 && job.sample_id == 0){
					if (job.experiment_id == 0)
						throw ns_ex("No experiment id specified for jpb");
					sql << "SELECT r.id FROM sample_region_image_info as r, capture_samples as s WHERE r.sample_id = s.id AND s.experiment_id = "
						<< job.experiment_id;
					ns_sql_result res;
					sql.get_rows(res);
					for (unsigned int i = 0; i < res.size(); i++){
						region_ids_to_spawn.push_back(atol(res[i][0].c_str()));
					}
				}
				else if (job.region_id == 0){
					if (job.experiment_id == 0)
						throw ns_ex("No experiment id specified for jpb");
					sql << "SELECT r.id FROM sample_region_image_info as r WHERE r.sample_id = " << job.sample_id;
					ns_sql_result res;
					sql.get_rows(res);
					for (unsigned int i = 0; i < res.size(); i++){
						region_ids_to_spawn.push_back(atol(res[i][0].c_str()));
					}
				}
				else region_ids_to_spawn.push_back(job.region_id);
				if (region_ids_to_spawn.size() > 0){
					image_server->register_server_event(ns_image_server_event("Spawning new jobs for ") << region_ids_to_spawn.size() << " region(s)",&sql);

					ns_processing_job new_job(job);
					new_job.maintenance_task = ns_maintenance_compress_stored_images;
				
					for (unsigned int i = 0; i < region_ids_to_spawn.size(); i++){
						sql << "SELECT id FROM sample_region_images WHERE region_info_id = " << region_ids_to_spawn[i];
						ns_sql_result region_images;
						sql.get_rows(region_images);
					
						new_job.region_id = region_ids_to_spawn[i];
						for (unsigned int j = 0; j < image_columns_to_convert.size(); j++){
							for (unsigned int k=0; k < region_images.size();k+=number_of_images_to_run_per_batch){
								new_job.id = 0;
								new_job.subregion_position.x  = k;
								new_job.image_id = j+1;
								new_job.save_to_db(sql);
							}
						}
					}
					ns_image_server_push_job_scheduler::request_job_queue_discovery(sql);
					break;
				}
				else image_server->register_server_event(ns_image_server_event("No subject images produced!"),&sql);
				break;
			}
			else{
				unsigned int column_to_process = job.image_id-1;
				if (column_to_process >= image_columns_to_convert.size())
					throw ns_ex("Invalid image column specification");

				sql << "SELECT " << image_columns_to_convert[column_to_process] << " FROM sample_region_images WHERE region_info_id = " << job.region_id;
				ns_sql_result images;
				sql.get_rows(images);
				unsigned long start_i(job.subregion_position.x);
				unsigned long stop_i(start_i + number_of_images_to_run_per_batch);
				if (start_i > images.size())
					start_i = images.size();
				if (stop_i > images.size())
					stop_i = images.size();
				if (start_i==stop_i){
					image_server->register_server_event(ns_image_server_event("No subject images produced!"),&sql);
					break;
				}
				
				cerr << "Compressing images " << start_i << "-" << stop_i << ":";
				ns_image_standard image;
				image.use_more_memory_to_avoid_reallocations(true);
				for (unsigned int i = start_i; i < stop_i; i++){
					try{
						cerr << i - start_i << "...";
						ns_64_bit id(ns_atoi64(images[i][0].c_str()));
						if (id == 0)
							continue;
						ns_image_server_image im;
						im.load_from_db(id,&sql);
						ns_image_server_image old_im(im);
						ns_image_type t(ns_get_image_type(im.filename));
						if (t == ns_jp2k || t == ns_jpeg)
							continue;
						ns_image_storage_source_handle<ns_8_bit> im_source (image_server->image_storage.request_from_storage(im,&sql));
						im_source.input_stream().pump(image,1024);
						bool b;
						im.filename += ".jp2";
						ns_image_storage_reciever_handle<ns_8_bit> im_dest(image_server->image_storage.request_storage(
									im, ns_jp2k, 2048, &sql, b, false, false));
						string compression_rate = image_server->get_cluster_constant_value("jp2k_compression_rate",".03",&sql);
						float compression_rate_f = atof(compression_rate.c_str());
						if (compression_rate_f <= 0)
							throw ns_ex("Invalid compression rate: ") << compression_rate_f ;
						image.set_output_compression(compression_rate_f);
						image.pump(&im_dest.output_stream(),2048);
						im.save_to_db(im.id,&sql);
						image_server->image_storage.delete_from_storage(old_im,ns_delete_long_term,&sql);
					}
					catch(ns_ex & ex){
						image_server->register_server_event(ex,&sql);
					}
				}
			}
			break;
		}
		case ns_maintenance_rebuild_worm_movement_table:{
			
			throw ns_ex("Depreciated!");
			/*
			if (job.region_id != 0)
				m.build_worm_movement_table_from_sample_time_relationships(job.sample_id, sql, job.region_id, 0, 0);
			else if (job.sample_id != 0)
				m.build_worm_movement_table_from_sample_time_relationships(job.sample_id, sql, 0,0,0);
			else if (job.experiment_id != 0)
				m.build_worm_movement_table_from_experiment_sample_time_relationships(job.experiment_id, sql);
			else m.build_worm_movement_table_from_experiment_sample_time_relationships(sql);
			break;
			*/
		}
		case ns_maintenance_generate_animal_storyboard:{
				ns_experiment_storyboard s;
				std::vector<ns_experiment_storyboard_spec> specs;
				ns_storyboard_spec_from_job (job,specs);
				const unsigned long neighbor_distance_to_juxtipose(atol(image_server->get_cluster_constant_value("storyboard_neighbor_distance_to_juxtipose_in_pixels","50",&sql).c_str()));
				vector<char> storyboard_has_valid_worms(specs.size(),0);
				
				bool empty_storyboard(false);

				for (unsigned int j = 0; j < specs.size(); j++){
				//	continue;
					specs[j].minimum_distance_to_juxtipose_neighbors = neighbor_distance_to_juxtipose;
					if (specs.size() > 1)
						cerr << "Compiling storyboard outline " << j+1 << " of " << specs.size() << "\n";
					if (!s.create_storyboard_metadata_from_machine_annotations(specs[j],sql)){
						empty_storyboard = true;
						continue;
					}
					else 
						storyboard_has_valid_worms[j] = true;
				
					ns_experiment_storyboard_manager man;
					man.delete_metadata_from_db(specs[j],sql);
					man.save_metadata_to_db(specs[j],s,"xml",sql);
					//reload the storyboard just to confirm it still works
					if (1){
						ns_experiment_storyboard s2;
						try{
							ns_experiment_storyboard_manager man2;
							man2.load_metadata_from_db(specs[j],s2,sql);
						}
						catch(ns_ex & ex){
							std::string r;
							if (r.size() == 0)
								ex << "\nns_experiment_storyboard::compare()::Found no differences between the storyboards.";
							else ex << "\n" << s.compare(s2).text();	
							throw ex;
						}
						ns_ex ex(s.compare(s2).text());
						if (ex.text().size() > 0)
							throw ex;
					}
					
				}
				if (empty_storyboard){
					for (unsigned int j = 0; j < specs.size(); j++){
						if (storyboard_has_valid_worms[j]){
							ns_experiment_storyboard_manager man;
							man.delete_metadata_from_db(specs[j],sql);
						}
					}
					throw ns_ex("The storyboard could not be generated as no dead or potentially dead worms were identified");
				}
				std::vector<ns_ex> errors;
				//if this is a job for a specific region or sample, just do the work
				if (job.region_id != 0 || job.sample_id != 0){
					for (unsigned int j = 0; j < specs.size(); j++){
						if (specs.size() > 1)
							cerr << "Generating storyboard type " << j+1 << " 1-" << specs.size() << " of " << specs.size() << "\n";
						if (!s.create_storyboard_metadata_from_machine_annotations(specs[j],sql))
							break;
						ns_experiment_storyboard_manager man;
						man.load_metadata_from_db(specs[j],s,sql);
						ns_image_standard ima;
							for (unsigned int i = 0; i < man.number_of_sub_images(); i++){
								try{
									s.draw(i,ima,true,sql);
									man.save_image_to_db(i,specs[j],ima,sql);
								}
								catch(ns_ex & ex){
									errors.push_back(ex);
								}
							}
					}
				}
				//if this is an experiment job, divvy up the tasks among the cluster
				else{
					ns_experiment_storyboard_manager man;
					man.load_metadata_from_db(specs[0],s,sql);

					ns_processing_job j(job);
					j.maintenance_task = ns_maintenance_generate_animal_storyboard_subimage;
					for (unsigned int i = 0; i < man.number_of_sub_images(); i+=5){
						j.id = 0;
						j.image_id = i;
						try{
							j.save_to_db(sql);
						}
						catch(ns_ex & ex){
							errors.push_back(ex);
						}
					}
					//update db stats
					sql << "UPDATE experiments SET "
						<< "latest_storyboard_build_timestamp = UNIX_TIMESTAMP(NOW()),"
						<< "last_timepoint_in_latest_storyboard_build = " << s.last_timepoint_in_storyboard << ","
						<< "number_of_regions_in_latest_storyboard_build = " << s.number_of_regions_in_storyboard
						<< " WHERE id = " << job.experiment_id;
						sql.send_query();
					ns_image_server_push_job_scheduler::request_job_queue_discovery(sql);
				}
				if (errors.size() > 0){
					//register all the errors but only throw the first one
					for (unsigned long i = 1; i < errors.size(); i++){
						image_server->register_server_event(ns_image_server::ns_register_in_central_db,errors[i]);
					}
					throw errors[0];
				}
				
				break;
		}
		case ns_maintenance_generate_animal_storyboard_subimage:{
			std::vector<ns_experiment_storyboard_spec> specs;
			ns_storyboard_spec_from_job (job,specs);
			const ns_64_bit start (job.image_id),
							    stop(job.image_id+5);
			for (unsigned int j = 0; j < specs.size(); j++){
				ns_experiment_storyboard s;
				ns_experiment_storyboard_manager man;
				man.load_metadata_from_db(specs[j],s,sql);	
				image_server->register_server_event(ns_image_server_event("Rendering a type ") <<(j+1) << " storyboard, divisions " << start+1 << "-" << stop+1 << " of " << s.divisions.size() << " (job " << job.id <<")\n",&sql);
				ns_image_standard ima;
				for (long i = start; i < stop && i < man.number_of_sub_images(); i++){
					cerr << "Rendering division " << i << ":";
					s.draw(i,ima,true,sql);
					cerr << "\n";
					man.save_image_to_db(i,specs[j],ima,sql);
				}
			}
			break;
		}
																
		case ns_maintenance_delete_files_from_disk_request:
			 delete_job_ = false;
			 ns_handle_file_delete_request(job,sql);
			break;
		case ns_maintenance_delete_files_from_disk_action:
			 parent_job = ns_handle_file_delete_action(job,sql);
			break;
		case ns_maintenance_delete_images_from_database:
			ns_handle_image_metadata_delete_action(job,sql);
			break;
		case ns_maintenance_generate_subregion_mask:{
			
			ns_image_server_captured_image_region region_image;
			region_image.region_info_id = job.region_id;
			sql << "SELECT e.id, r.name, e.name,s.name,s.device_name,s.id FROM experiments as e, capture_samples as s, sample_region_image_info as r "
				   "WHERE e.id = s.experiment_id AND s.id = r.sample_id AND r.id = " << job.region_id;
			ns_sql_result res;
			sql.get_rows(res);
			if (res.empty())
				throw ns_ex("Could not find region in db:")<< job.region_id;
			region_image.experiment_id = atol(res[0][0].c_str());
			region_image.region_name = res[0][1];
			region_image.experiment_name = res[0][2];
			region_image.sample_name = res[0][3];
			region_image.device_name = res[0][4];
			region_image.sample_id = atol(res[0][5].c_str());
			region_image.region_images_id = 1;
			region_image.captured_images_id = 1;
			region_image.capture_time = 1;
			region_image.capture_images_image_id  = 1;
	//		cout << region_image.filename(&sql);


			ns_worm_multi_frame_interpolation mfi;

			ns_image_standard heat_map, static_mask;
		
			bool had_to_use_local_storage;
		
			//calculate heat map
			sql << "SELECT number_of_frames_used_to_mask_stationary_objects FROM sample_region_image_info WHERE id = " << region_image.region_info_id;
			//ns_sql_result res;
			sql.get_rows(res);
			if (res.size() == 0)
				throw ns_ex("ns_image_processing_pipeline::calculate_static_mask_and_heat_map::Could not load region information for region ") << region_image.region_info_id;
			const unsigned long number_of_frames_used_to_mask_stationary_objects(atol(res[0][0].c_str()));

			mfi.load_all_region_worms(region_image.region_info_id,sql,false);
			mfi.generate_heat_map(heat_map,number_of_frames_used_to_mask_stationary_objects,sql);
			//save the heat map to disk
			ns_image_server_image a_vis = region_image.create_storage_for_processed_image(ns_process_heat_map,ns_tiff,&sql);
			ns_image_storage_reciever_handle<ns_8_bit> a_vis_o = image_server->image_storage.request_storage(
														a_vis,
														ns_tiff, 1024,&sql,had_to_use_local_storage,false,false);
			heat_map.pump(a_vis_o.output_stream(),1024);
			a_vis.mark_as_finished_processing(&sql);
	
			
			
			ns_worm_multi_frame_interpolation::generate_static_mask_from_heatmap(heat_map,static_mask);

			//save the static mask to disk
			ns_image_server_image b_vis = region_image.create_storage_for_processed_image(ns_process_static_mask,ns_tiff,&sql);
			ns_image_storage_reciever_handle<ns_8_bit> b_vis_o = image_server->image_storage.request_storage(
													b_vis,
													ns_tiff, 1024,&sql,had_to_use_local_storage,false,false);
			static_mask.pump(b_vis_o.output_stream(),1024);
			b_vis.mark_as_finished_processing(&sql);

			sql << "UPDATE sample_region_images SET currently_under_processing=0 WHERE region_info_id = " << region_image.region_info_id;
			sql.send_query();
			sql << "UPDATE worm_movement SET problem=0,calculated=0 WHERE region_info_id = " << region_image.region_info_id;
			sql.send_query();
			sql.send_query("COMMIT");

			break;
		}
		default: throw ns_ex("ns_processing_job_scheduler::Unknown maintenance task");
	}
	return true;
}

bool ns_processing_job_image_processor::run_job(ns_sql & sql){
	if (job.mask_id == 0)
		throw ns_ex("ns_processing_job_scheduler::run_job_from_push_queue()::Operation not implemented.");
	ns_image_server_image source_image;
	source_image.id = job.image_id;
	const float image_resolution(pipeline->process_mask(source_image,job.mask_id,sql));
	
	//if the mask is assigned to a capture sample, use it to generate regions for that sample.
	sql << "SELECT id FROM capture_samples WHERE mask_id = " << job.mask_id;
	ns_sql_result res;
	sql.get_rows(res);
	vector<ns_ex> exceptions;
	for (std::vector<ns_ex>::size_type i = 0; i < res.size(); i++){
		try{
			pipeline->generate_sample_regions_from_mask(atol(res[i][0].c_str()),image_resolution,sql);
		}
		catch(ns_ex & ex){
			exceptions.push_back(ex);
		}
	}
	for (std::vector<ns_ex>::size_type i = 0; i < exceptions.size(); i++){
		image_server->register_server_event(exceptions[i],&sql);
	}
	
	return true;
}
