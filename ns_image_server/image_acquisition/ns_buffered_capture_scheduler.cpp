#include "ns_buffered_capture_scheduler.h"
#include "ns_processing_job_push_scheduler.h"
#include <set>


void ns_buffered_capture_scheduler::get_last_update_time(ns_local_buffer_connection & local_buffer_sql){
	time_of_last_update_from_central_db.local_time = atol(image_server.get_cluster_constant_value("local_time_of_last_buffer_upload_to_central_db",ns_to_string(ns_default_update_time),&local_buffer_sql).c_str());
	time_of_last_update_from_central_db.remote_time = atol(image_server.get_cluster_constant_value("central_time_of_last_buffer_upload_to_central_db",ns_to_string(ns_default_update_time),&local_buffer_sql).c_str());
}


void ns_table_format_processor::load_column_names_from_db(const std::string & table_name,ns_image_server_sql * sql){
	sql_get_column_names(table_name,column_names,sql);
	for (unsigned int i = 0; i < column_names.size(); i++){
		if(column_names[i] == "time_stamp"){
			time_stamp_column_id = i;
			break;
		}
	}
}

bool ns_table_format_processor::loaded(){return !column_names.empty();}

void ns_table_format_processor::get_column_name_indicies(const std::vector<ns_table_column_spec> & spec){
	unsigned long num_found(0);

	for (long i = 0; i < column_names.size() && num_found < column_names.size(); i++){
		for (long j = 0; j < spec.size(); j++){
			if (column_names[i] == spec[j].name){
				*spec[j].index = i;
				num_found++;
			}
		}
	}
	if (num_found != spec.size())
		throw ns_ex("ns_column_name_info::get_column_name_indicies::Could not find all columns!");
}

void ns_table_format_processor::sql_get_column_names(const std::string & table_name, std::vector<std::string> & column_names, ns_image_server_sql * sql){
	*sql << "DESCRIBE " << table_name;
	ns_sql_result res;
	sql->get_rows(res);
	column_names.resize(res.size());
	for (unsigned int i = 0; i < res.size(); i++)
		column_names[i] = res[i][0];

	if (column_names.size() == 0)
		throw ns_ex("No column names received for table ") << table_name << "!";
}




void ns_get_all_column_data_from_table(const std::string & table_name, const std::vector<std::string> & column_names, const std::string & where_clause, ns_sql_result & data, ns_image_server_sql * sql){
	*sql << "SELECT `" << column_names[0] << "`";
		for (unsigned int i = 1; i < column_names.size(); i++)
			*sql << ", `" << column_names[i] << "`";
		*sql << " FROM " << table_name <<" " << where_clause;
		sql->get_rows(data);
}

struct ns_db_key_mapping{
	ns_ex error;
	ns_db_key_mapping():central_problem_id(0),local_problem_id(0){}
	ns_image_server_captured_image central_captured_image,
								   local_captured_image;
	ns_image_server_image central_image,
		local_image,
		central_small_image,
		local_small_image;
	ns_image_capture_data_manager::ns_capture_image_status local_transfer_status,
															central_transfer_status;
	ns_64_bit central_problem_id,
				  local_problem_id;
};

void ns_buffered_capture_scheduler::clear_local_cache(ns_local_buffer_connection & sql){
	sql.send_query("TRUNCATE buffered_capture_samples");
	sql.send_query("TRUNCATE buffered_capture_schedule");
	sql.send_query("TRUNCATE buffered_captured_images");
	sql.send_query("TRUNCATE buffered_constants");
	sql.send_query("TRUNCATE buffered_experiments");
	sql.send_query("TRUNCATE buffered_host_event_log");
	sql.send_query("TRUNCATE buffered_images");
	time_of_last_update_from_central_db.local_time = 
		time_of_last_update_from_central_db.remote_time = ns_default_update_time;
}


void ns_buffered_capture_scheduler::commit_all_local_non_schedule_changes_to_central_db(const ns_synchronized_time & update_start_time,ns_local_buffer_connection & local_buffer_sql, ns_sql & central_db){
	
	//ns_sql_full_table_lock lock(local_buffer_sql,"buffered_host_event_log");
	local_buffer_sql << "SELECT event, time, minor, id FROM buffered_host_event_log WHERE time < " << update_start_time.local_time;
	ns_sql_result events;
	local_buffer_sql.get_rows(events);
	for (unsigned int i = 0; i < events.size(); i++){
		ns_image_server_event ev;
		ev << events[i][0];
		if (events[i][2] != "0") ev << ns_ts_minor_event;
		ev.set_time(atol(events[i][1].c_str()));
		image_server.register_server_event(ev,&central_db,true);
	}
	local_buffer_sql << "DELETE FROM buffered_host_event_log WHERE time < " << update_start_time.local_time;
	local_buffer_sql.send_query();
	//lock.unlock();
}



void ns_buffered_capture_scheduler::commit_all_local_schedule_changes_to_central_db(const ns_synchronized_time & update_start_time,ns_local_buffer_connection & local_buffer_sql, ns_sql & central_db){
	if (time_of_last_update_from_central_db.local_time == ns_default_update_time) 
		get_last_update_time(local_buffer_sql);
	buffered_capture_schedule.load_if_needed(&local_buffer_sql);


	//we need to copy all locally cached schedule and captured image data to the central db
	//The central and locally cached capture_schedule rows have identical ids, as we copied the former from the latter.
	//However, the locally cached captured_images and images have unique ids specific to the local cache.
	//Thus, when we update the central db, we need to translate these local ids to the central ids,
	//creating new records in the central db when necissary.
	//Note that this is a one way process--we never update the local db with any central ids.  Central ids may already be taken in the local db,
	//used when new images are created during image capture.
	//keeping things uni-directional avoids several issues.

	ns_sql_result updated_data;

	const std::string altered_data_condition(
		std::string("time_stamp= 0 || (time_stamp > FROM_UNIXTIME(") + ns_to_string(time_of_last_update_from_central_db.local_time) +
					") AND time_stamp <= FROM_UNIXTIME(" + ns_to_string(update_start_time.local_time) + ")) ");

	const unsigned long new_timestamp(time_of_last_update_from_central_db.remote_time);

	ns_get_all_column_data_from_table("buffered_capture_schedule",buffered_capture_schedule.table_format.column_names, 
		std::string("WHERE ") + altered_data_condition + " AND uploaded_to_central_db =0",
		updated_data,&local_buffer_sql);

	//first, set up mappings between local db records and central db records
	//if central db records do not exist for images, create new ones.
	std::vector<ns_db_key_mapping> mappings(updated_data.size());
	if (updated_data.size() > 8) // don't report routine entry updates involving only a small number of rows
		image_server.register_server_event(ns_image_server_event("ns_buffered_capture_scheduler::Committing ") << updated_data.size() << " recorded capture events to the central database.",&central_db);
	std::vector<ns_ex *> errors;
	{
		for (unsigned long i = 0; i < updated_data.size(); i++) {
			try {
				mappings[i].local_transfer_status = (ns_image_capture_data_manager::ns_capture_image_status)atol(updated_data[i][buffered_capture_schedule.transfer_status_column].c_str());
				mappings[i].central_transfer_status = ns_image_capture_data_manager::ns_not_finished;
				mappings[i].local_captured_image.captured_images_id = ns_atoi64(updated_data[i][buffered_capture_schedule.captured_image_id_column].c_str());
				mappings[i].local_captured_image.experiment_id = ns_atoi64(updated_data[i][buffered_capture_schedule.experiment_id_column].c_str());
				mappings[i].local_captured_image.sample_id = ns_atoi64(updated_data[i][buffered_capture_schedule.sample_id_column].c_str());
				mappings[i].local_captured_image.capture_time = ns_atoi64(updated_data[i][buffered_capture_schedule.time_at_imaging_start_column].c_str());

				mappings[i].local_problem_id = ns_atoi64(updated_data[i][buffered_capture_schedule.problem_column].c_str());

				mappings[i].central_captured_image.captured_images_id = 0;
				mappings[i].central_image.id = 0;
				mappings[i].central_small_image.id = 0;
				mappings[i].central_problem_id = 0;
				if (mappings[i].local_captured_image.captured_images_id != 0 || mappings[i].local_problem_id) {
					//load local image info
					if (!mappings[i].local_captured_image.load_from_db(mappings[i].local_captured_image.captured_images_id, &local_buffer_sql))
						throw ns_ex("Could not identify local captured image in the local db");
					if (mappings[i].local_captured_image.capture_images_image_id != 0) {
						mappings[i].local_image.load_from_db(mappings[i].local_captured_image.capture_images_image_id, &local_buffer_sql);

						if (mappings[i].local_captured_image.capture_images_small_image_id != 0)
							mappings[i].local_small_image.load_from_db(mappings[i].local_captured_image.capture_images_small_image_id, &local_buffer_sql);
					}
					//look in central db to see if a record already exists for the captured_image associated with the local schedule entry
					central_db << "SELECT captured_image_id,problem,transferred_to_long_term_storage FROM capture_schedule WHERE id = " << updated_data[i][buffered_capture_schedule.id_column];
					ns_sql_result res;
					central_db.get_rows(res);
					if (res.size() == 0)
						throw ns_ex("Could not find capture schedule entry in central db for sample id ") << updated_data[i][buffered_capture_schedule.id_column] << " finishing at time " << updated_data[i][buffered_capture_schedule.time_at_finish_column];

					mappings[i].central_captured_image.captured_images_id = ns_atoi64(res[0][0].c_str());
					mappings[i].central_problem_id = ns_atoi64(res[0][1].c_str());
					mappings[i].central_transfer_status = (ns_image_capture_data_manager::ns_capture_image_status)atol(res[0][2].c_str());
				}

				//if a captured image record is stored locally, find or create the central records for it
				if (mappings[i].local_captured_image.captured_images_id != 0) {
					const bool need_to_make_new_capture_image_in_central_db(mappings[i].central_captured_image.captured_images_id == 0);
					bool need_to_make_new_image_in_central_db(true);
					bool need_to_make_new_small_image_in_central_db(true);
					//if possible, load image info in central db.
					if (!need_to_make_new_capture_image_in_central_db) {
						mappings[i].central_captured_image.experiment_id = ns_atoi64(updated_data[i][buffered_capture_schedule.experiment_id_column].c_str());
						mappings[i].central_captured_image.sample_id = ns_atoi64(updated_data[i][buffered_capture_schedule.sample_id_column].c_str());
						mappings[i].central_captured_image.capture_time = ns_atoi64(updated_data[i][buffered_capture_schedule.time_at_imaging_start_column].c_str());
						if (!mappings[i].central_captured_image.load_from_db(mappings[i].central_captured_image.captured_images_id, &central_db)) {
							mappings[i].central_captured_image.load_from_db(mappings[i].central_captured_image.captured_images_id, &central_db);
							throw ns_ex("Could not load captured image in central db");
						}
						if (mappings[i].central_captured_image.capture_images_image_id != 0) {
							try {
								if (!mappings[i].central_image.load_from_db(mappings[i].central_captured_image.capture_images_image_id, &central_db))
									throw ns_ex("Could not load information on ci from db");
								need_to_make_new_image_in_central_db = false;
							}
							catch (ns_ex & ex) {
								image_server.register_server_event(ex, &central_db);
							}
						}
						if (mappings[i].central_captured_image.capture_images_small_image_id != 0) {
							try {
								mappings[i].central_small_image.load_from_db(mappings[i].central_captured_image.capture_images_small_image_id, &central_db);
								need_to_make_new_small_image_in_central_db = false;
							}
							catch (ns_ex & ex) {
								image_server.register_server_event(ex, &central_db);
							}
						}
					}
					else {
						mappings[i].central_captured_image = mappings[i].local_captured_image;
					}
					if (need_to_make_new_image_in_central_db) {
						//otherwise, prepare to make new entries in central db
						mappings[i].central_image = mappings[i].local_image;
						mappings[i].central_image.save_to_db(0, &central_db, false);
						mappings[i].central_captured_image.capture_images_image_id = mappings[i].central_image.id;
					}
					if (need_to_make_new_small_image_in_central_db) {
						//otherwise, prepare to make new entries in central db
						mappings[i].central_small_image = mappings[i].local_small_image;
						mappings[i].central_small_image.save_to_db(0, &central_db, false);
						mappings[i].central_captured_image.capture_images_small_image_id = mappings[i].central_small_image.id;
					}
					if (need_to_make_new_capture_image_in_central_db) {
						mappings[i].central_captured_image.captured_images_id = 0;
						mappings[i].central_captured_image.experiment_id = ns_atoi64(updated_data[i][buffered_capture_schedule.experiment_id_column].c_str());
						mappings[i].central_captured_image.sample_id = ns_atoi64(updated_data[i][buffered_capture_schedule.sample_id_column].c_str());
						mappings[i].central_captured_image.capture_time = ns_atoi64(updated_data[i][buffered_capture_schedule.time_at_imaging_start_column].c_str());
						mappings[i].central_captured_image.save(&central_db,ns_image_server_captured_image::ns_mark_as_busy);
					}
					else {
						if (mappings[i].central_captured_image.experiment_id == 0) {
							//make sure sample, experiment, and capture time are set in the db
							mappings[i].central_captured_image.experiment_id = ns_atoi64(updated_data[i][buffered_capture_schedule.experiment_id_column].c_str());
							mappings[i].central_captured_image.sample_id = ns_atoi64(updated_data[i][buffered_capture_schedule.sample_id_column].c_str());
							mappings[i].central_captured_image.capture_time = ns_atoi64(updated_data[i][buffered_capture_schedule.time_at_imaging_start_column].c_str());
							mappings[i].central_captured_image.save(&central_db, ns_image_server_captured_image::ns_mark_as_busy);
						}

					}
				}


				if (mappings[i].local_problem_id != 0 && mappings[i].central_problem_id == 0) {
					local_buffer_sql << "SELECT id,event,time,minor FROM buffered_host_event_log WHERE id = " << updated_data[i][buffered_capture_schedule.problem_column];
					ns_sql_result res;
					local_buffer_sql.get_rows(res);
					if (res.size() == 0) {
						mappings[i].central_problem_id = image_server.register_server_event(ns_ex("Could not find problem id ") << updated_data[i][buffered_capture_schedule.problem_column] << " in local database buffer!", &central_db);
					}
					else {
						ns_image_server_event ev;
						ev << res[0][1];
						if (res[0][3] != "0") ev << ns_ts_minor_event;
						ev.set_time(atol(res[0][2].c_str()));
						mappings[i].central_problem_id = image_server.register_server_event(ev, &central_db);
					}
				}
			}
			catch (ns_ex & ex) {
				mappings[i].error << "Error while making mapping: " << ex.text();
				errors.push_back(&mappings[i].error);
			}
		}
	}
	std::vector<ns_image_server_captured_image> newly_captured_images_for_which_to_schedule_jobs;
	//at this stage, we have all local and central db records matched up in the mappings object.
	//we go through and update the central records based on the local ones.
	for (unsigned long i = 0; i < updated_data.size(); i++){
		if (mappings[i].error.text().size() > 0)
			continue;
		bool rename_images_with_central_ids = false;
		if (mappings[i].local_transfer_status == ns_image_capture_data_manager::ns_transferred_to_long_term_storage) {
			if (image_server.image_storage.test_connection_to_long_term_storage(true)) {
				rename_images_with_central_ids = true;
			}
			else std::cerr << "Images need to be renamed, but this cannot be done because file server is missing.\n";
		}
		try{

			if (mappings[i].local_captured_image.captured_images_id != 0){
				//update central_db with any changes to the locally cached images.
				//if images need to be renamed with their central ids, do so.

				const bool mark_images_as_busy(mappings[i].local_transfer_status == ns_image_capture_data_manager::ns_transferred_to_long_term_storage && !rename_images_with_central_ids);

				ns_64_bit tmp_id = mappings[i].central_image.id;
				if (rename_images_with_central_ids) {

					if (!mappings[i].central_captured_image.load_from_db(mappings[i].central_captured_image.captured_images_id, &central_db))
						throw ns_ex("could not load captured image in central db");
					mappings[i].central_image = mappings[i].central_captured_image.make_large_image_storage(&central_db);
					ns_file_location_specification destination(image_server.image_storage.get_file_specification_for_image(mappings[i].central_image, &central_db));
					ns_file_location_specification source(image_server.image_storage.get_file_specification_for_image(mappings[i].local_image, &central_db));
					if (!image_server.image_storage.move_file(source, destination, false))
						throw ns_ex("Could not find image in long term storage, to update name with central db data");
				}
				else {
					mappings[i].central_image = mappings[i].local_image;
				}

				mappings[i].central_image.save_to_db(tmp_id, &central_db, mark_images_as_busy);

				tmp_id = mappings[i].central_small_image.id;
				if (rename_images_with_central_ids) {
					mappings[i].central_small_image = mappings[i].central_captured_image.make_small_image_storage(&central_db);
					ns_file_location_specification destination(image_server.image_storage.get_file_specification_for_image(mappings[i].central_small_image, &central_db));
					ns_file_location_specification source(image_server.image_storage.get_file_specification_for_image(mappings[i].local_small_image, &central_db));
					if (!image_server.image_storage.move_file(source, destination, false))
						throw ns_ex("Could not find image in long term storage, to update name with central db data");
					mappings[i].local_transfer_status == ns_image_capture_data_manager::ns_transfer_complete;
				}
				else mappings[i].central_small_image = mappings[i].local_small_image;
				mappings[i].central_small_image.save_to_db(tmp_id, &central_db, mark_images_as_busy);

				tmp_id = mappings[i].central_captured_image.captured_images_id;
				mappings[i].central_captured_image = mappings[i].local_captured_image;
				mappings[i].central_captured_image.captured_images_id = tmp_id;
				mappings[i].central_captured_image.capture_images_image_id = mappings[i].central_image.id;
				mappings[i].central_captured_image.capture_images_small_image_id = mappings[i].central_small_image.id;
				mappings[i].central_captured_image.save(&central_db, mark_images_as_busy? ns_image_server_captured_image::ns_mark_as_busy: ns_image_server_captured_image::ns_mark_as_not_busy);
			}

			//update capture schedule
			central_db << "Update capture_schedule SET ";
			for (unsigned int j = 0; j < buffered_capture_schedule.table_format.column_names.size(); ++j){
				if (j == buffered_capture_schedule.id_column ||
					j == buffered_capture_schedule.captured_image_id_column ||
					j == buffered_capture_schedule.problem_column || 
					j == buffered_capture_schedule.timestamp_column ||
					j == buffered_capture_schedule.transfer_status_column) 
					continue;
				central_db  << "`" << buffered_capture_schedule.table_format.column_names[j] << "`='" << central_db.escape_string(updated_data[i][j]) << "',";
			}
			central_db << "captured_image_id = " << mappings[i].central_captured_image.captured_images_id 
					   << ", problem = " << mappings[i].central_problem_id;
			if (rename_images_with_central_ids)
				central_db << ", transferred_to_long_term_storage = " << (int)ns_image_capture_data_manager::ns_transfer_complete;
			else central_db << ", transferred_to_long_term_storage = " << updated_data[i][buffered_capture_schedule.transfer_status_column];

			//we set the central db record timestamp as old so that it does not trigger a re-download when the server tries to update its cache
			central_db << ", time_stamp=FROM_UNIXTIME("<< new_timestamp <<") ";
			central_db << " WHERE id = " << updated_data[i][buffered_capture_schedule.id_column];
			//std::cerr << central_db.query() << "\n";
			central_db.send_query();

			//set the local db record timestamp as old also
			local_buffer_sql << "UPDATE buffered_capture_schedule SET ";
			if (rename_images_with_central_ids)
				local_buffer_sql << "transferred_to_long_term_storage = " << (int)ns_image_capture_data_manager::ns_transfer_complete << ",";
			local_buffer_sql << "time_stamp = FROM_UNIXTIME("<< new_timestamp
					<<") WHERE id = " << updated_data[i][buffered_capture_schedule.id_column];
			local_buffer_sql.send_query();

			//if the capture is completed (eg. time_at_finish is set) and we have transfered the image and all metadata to the central db
			//(eg the transfer status column is set appropriately)
			//then delete the local copy to keep the cache small and fast.
			if (updated_data[i][buffered_capture_schedule.time_at_finish_column] != "0" && 
				mappings[i].local_transfer_status == ns_image_capture_data_manager::ns_transfer_complete){

				//remember to check for any jobs that might be waiting for new images
				newly_captured_images_for_which_to_schedule_jobs.push_back(mappings[i].central_captured_image);

				local_buffer_sql << "DELETE FROM buffered_capture_schedule WHERE id = " << updated_data[i][buffered_capture_schedule.id_column];
				local_buffer_sql.send_query();
				local_buffer_sql << "DELETE FROM buffered_captured_images WHERE id = " << mappings[i].local_captured_image.captured_images_id;
				local_buffer_sql.send_query();
				local_buffer_sql << "DELETE FROM buffered_images WHERE id = " << mappings[i].local_image.id;
				local_buffer_sql.send_query();
				local_buffer_sql << "DELETE FROM buffered_host_event_log WHERE id = " << mappings[i].local_problem_id;
				local_buffer_sql.send_query();
			}
		}
		catch(ns_ex & ex){
			mappings[i].error << "Error during central update: " << ex.text();
			errors.push_back(&mappings[i].error);
		}
	}
	for (unsigned int i = 0; i < mappings.size(); i++){
		if (mappings[i].error.text().size() > 0){

			try {
				{
				ns_64_bit local_problem_id = image_server.register_server_event(ns_ex("Could not update central db: ") << mappings[i].error.text(), &local_buffer_sql);
				local_buffer_sql << "UPDATE buffered_capture_schedule SET uploaded_to_central_db="<< local_problem_id << ", time_stamp = FROM_UNIXTIME(" << new_timestamp << ") WHERE id = " << updated_data[i][buffered_capture_schedule.id_column];
				local_buffer_sql.send_query();
				}
				{
					ns_64_bit central_problem_id = image_server.register_server_event(ns_ex("Could not update central db: ") << mappings[i].error.text(), &central_db);
					central_db << "UPDATE capture_schedule SET uploaded_to_central_db=" << central_problem_id << ", time_stamp = FROM_UNIXTIME(" << new_timestamp << ") WHERE id = " << updated_data[i][buffered_capture_schedule.id_column];
					central_db.send_query();
				}
			 }
			 catch (...) {
				 //do nothing.
			 }
		}

	}
	//update modified sample data.
	updated_data.resize(0);
	if (capture_samples.column_names.size() == 0){
		capture_samples.load_column_names_from_db("capture_samples",&central_db);
		if (capture_samples.column_names.size() == 0)
			throw ns_ex("ns_buffered_capture_scheduler::commit_all_local_schedule_changes_to_central_db()::Capture sample table appears to have no columns!");
	}
	if (capture_samples.column_names[0] != "id")
		throw ns_ex("ns_buffered_capture_scheduler::commit_all_local_schedule_changes_to_central_db()::Capture sample table does not have its id in the first column!");

	ns_get_all_column_data_from_table("buffered_capture_samples",capture_samples.column_names, 
		std::string("WHERE ") + altered_data_condition,
		updated_data,&local_buffer_sql);
	if (capture_samples.time_stamp_column_id == -1)
		throw ns_ex("Could not find capture sample time stamp column!");
	for (unsigned int i = 0; i < updated_data.size(); i++){
		central_db << "UPDATE capture_samples SET ";
		//skip id column; we don't want to cause any unneccisary db shuffling by changing ids (even if we are changing the ids to the value they already are)
		central_db  << capture_samples.column_names[1] << "='" << central_db.escape_string(updated_data[i][1]);
		for (unsigned int j = 2; j < capture_samples.column_names.size(); ++j){
			if (j == capture_samples.time_stamp_column_id)
				continue;
			else central_db  << "',`" << capture_samples.column_names[j] << "`='" << central_db.escape_string(updated_data[i][j]);
		}
		central_db << "',time_stamp=FROM_UNIXTIME(" << new_timestamp << ") ";
		central_db << "WHERE id = " << updated_data[i][0];
		central_db.send_query();
	}
	//now go through and report all the new images uploaded to the central database,
	//to see if they match any jobs.
	if (!newly_captured_images_for_which_to_schedule_jobs.empty()) {
		try {
			//report new image to database (as it might be ready for processing)
			ns_image_server_push_job_scheduler job_scheduler;

			job_scheduler.report_capture_sample_image(newly_captured_images_for_which_to_schedule_jobs, central_db);
		}
		catch (ns_ex & ex) {
			image_server.register_server_event(ns_image_server_event("Problem encountered while reporting captured images: ") << ex.text(), &local_buffer_sql);
			throw ex;
		}
	}

}


std::string ns_compile_sql_where_clause(const std::set<ns_64_bit> & ids, const std::string column_name){
		std::string r;
		if (!ids.empty()){
			r += " (id=";
			r += ns_to_string(*ids.begin());
				
			for (std::set<ns_64_bit>::const_iterator p = ++ids.begin(); p!= ids.end(); p++){
				r += " OR ";
				r+=column_name;
				r+= "=";
				r += ns_to_string(*p);
			}
			r += ")";
		}
		return r;
}

void ns_buffered_capture_scheduler::commit_local_changes_to_central_server(ns_local_buffer_connection & local_buffer, ns_sql & central_db){
	ns_acquire_lock_for_scope lock(buffer_capture_scheduler_lock,__FILE__,__LINE__);

	local_buffer.clear_query();
	central_db.clear_query();
	std::string local_time = local_buffer.get_value("SELECT UNIX_TIMESTAMP(NOW())"),
			central_time = central_db.get_value("SELECT UNIX_TIMESTAMP(NOW())");

	const ns_synchronized_time update_start_time(atol(local_time.c_str()),atol(central_time.c_str()));
	//now we update the local buffer to the central node.
	commit_all_local_schedule_changes_to_central_db(update_start_time,local_buffer,central_db);
	commit_all_local_non_schedule_changes_to_central_db(update_start_time,local_buffer,central_db);
	lock.release();
}
void ns_buffered_capture_scheduler::update_local_buffer_from_central_server(ns_image_server_device_manager::ns_device_name_list & connected_devices,ns_local_buffer_connection & local_buffer, ns_sql & central_db){
	
	if (connected_devices.size() == 0)
		return;

	ns_acquire_lock_for_scope lock(buffer_capture_scheduler_lock,__FILE__,__LINE__);

	local_buffer.clear_query();
	central_db.clear_query();
	
	std::string local_time = local_buffer.get_value("SELECT UNIX_TIMESTAMP(NOW())"),
				central_time = central_db.get_value("SELECT UNIX_TIMESTAMP(NOW())");

	const ns_synchronized_time update_start_time(atol(local_time.c_str())-10,atol(central_time.c_str())-10);//go ten seconds into the past
																											//to make sure all writes
																											//are committed

	//now we update the local buffer to the central node.
	commit_all_local_schedule_changes_to_central_db(update_start_time,local_buffer,central_db);
	//now that all the local buffer data is reflected in the central database, we check to see if there is any new data in the central database.
	//if so, we wipe the local buffer and update everything.

	capture_schedule.load_if_needed(&central_db);
	//get any new or updated capture schedule events

	central_db << "SELECT sched.id, samp.id, samp.experiment_id, UNIX_TIMESTAMP(sched.time_stamp),UNIX_TIMESTAMP(samp.time_stamp)";

	for (unsigned int i = 0; i < capture_schedule.table_format.column_names.size(); i++)
		central_db << ",`sched`.`" << capture_schedule.table_format.column_names[i] << "`";
		
	central_db << " FROM capture_schedule as sched, capture_samples as samp "
			   << "WHERE (samp.device_name='" << connected_devices[0].name << "'";

	for (unsigned int i = 1; i < connected_devices.size(); i++)
		central_db << " OR samp.device_name='" << connected_devices[i].name << "'";
			
	central_db << ")"
			    << " AND sched.time_at_start = 0 "
			    << " AND sched.sample_id = samp.id "
				<< " AND sched.time_at_finish = 0 "
				//here, we could bring the entire local database completely up to date
				//but only scans in the future will make any difference, so we only download
				//those who are still scheduled for the future
				//this old command would fully update the database, as time_of_last_update_from_central_db
				//would be set to 0
				//<< " AND sched.scheduled_time > " << (time_of_last_update_from_central_db.remote_time-image_server.maximum_allowed_local_scan_delay())  //only get events in the future
				//however, now we only grab the future, relevant scans.
				<< " AND sched.scheduled_time > " << (update_start_time.remote_time-image_server.maximum_allowed_local_scan_delay())  //only get events in the future
				
				<< " AND sched.time_stamp > FROM_UNIXTIME(" << time_of_last_update_from_central_db.remote_time <<") "
				<< " AND sched.time_stamp <= FROM_UNIXTIME(" << update_start_time.remote_time << ") "
				<< " ORDER BY sched.scheduled_time ASC";


	ns_sql_result new_schedule;
	central_db.get_rows(new_schedule);
	std::set<ns_64_bit> altered_experiment_ids;
	std::set<ns_64_bit> altered_sample_ids;
	for (unsigned int i = 0; i < new_schedule.size(); i++){
	//	if (atol(new_schedule[i][4].c_str()) > central_time_of_last_update_from_central_db){
			altered_sample_ids.insert(ns_atoi64(new_schedule[i][1].c_str()));
			altered_experiment_ids.insert(ns_atoi64(new_schedule[i][2].c_str()));
	//	}
	}
	const unsigned long new_timestamp(update_start_time.local_time);

	if (new_schedule.size() != 0){
		if (new_schedule.size() > 4)
			image_server.register_server_event(ns_image_server_event("ns_buffered_capture_scheduler::") 
				<< new_schedule.size() << " new capture schedule entries found.  Updating local buffer...",&central_db);
		
		//if samples or experiments have changed or added, update them.
		//we need to do this *before* updating the capture schedule,
		//as the addition of a capture schedule item might trigger a scan immediately
		//and that scan will fail if the sample and experiemnts information isn't already in the local database.
		if (altered_sample_ids.size() > 0){
			capture_samples.load_if_needed("capture_samples",&central_db);
			experiments.load_if_needed("experiments",&central_db);
			std::string sample_where_clause(std::string(" WHERE ") + ns_compile_sql_where_clause(altered_sample_ids,"id")),
						experiment_where_clause(std::string(" WHERE ") + ns_compile_sql_where_clause(altered_experiment_ids,"id"));
			
			ns_sql_result capture_sample_data;
			ns_get_all_column_data_from_table("capture_samples",capture_samples.column_names,sample_where_clause,capture_sample_data,&central_db);
			ns_sql_result experiment_data;
			ns_get_all_column_data_from_table("experiments",experiments.column_names,experiment_where_clause,experiment_data,&central_db);
		
			if (capture_sample_data.size() > 0)
				image_server.register_server_event(ns_image_server_event("Caching ") << capture_sample_data.size() << " samples in the local db.",&central_db);
			//local_buffer_db.send_query("DELETE FROM buffered_capture_samples");
			if (capture_samples.time_stamp_column_id == -1)
					throw ns_ex("Could not find capture sample time stamp column!");
			long r(-5);
			for (unsigned int i = 0; i < capture_sample_data.size(); i++) {
				long r1 = (100 * i) / capture_sample_data.size();
				if (r1 - r >= 5) {
					image_server.add_subtext_to_current_event(ns_to_string(r1) + "%...", &central_db);
					r = r1;
				}

				std::string values;

				values += "`";
				values += capture_samples.column_names[0] + "`='" + local_buffer.escape_string(capture_sample_data[i][0]) + "'";
				for (unsigned int j = 1; j < capture_samples.column_names.size(); j++) {
					if (j == capture_samples.time_stamp_column_id)	//we need to update the local time stamp here, so that if there might be a clock asynchrony between the
						continue;									//central server and local server that would allow remote timestamps to be in the future according to local
																	//which would trigger the local server to update the central in the next check, ad infinitum
					values += std::string(",`") + capture_samples.column_names[j] + "`='" + local_buffer.escape_string(capture_sample_data[i][j]) + "'";
				}
				values += std::string(",`time_stamp`=FROM_UNIXTIME(") + ns_to_string(new_timestamp) + ")";
				local_buffer << "INSERT INTO buffered_capture_samples SET " << values
					<< " ON DUPLICATE KEY UPDATE " << values;
				local_buffer.send_query();
			}
			if (capture_sample_data.size() > 0)
				image_server.add_subtext_to_current_event("Done.\n", &central_db);
			//local_buffer.send_query("DELETE FROM buffered_experiments");
			for(unsigned int i = 0; i < experiment_data.size(); i++){
				std::string values;
				values += "`";
				values += experiments.column_names[0] + "`='" + local_buffer.escape_string(experiment_data[i][0]) + "'";
				for (unsigned int j = 1; j < experiments.column_names.size(); j++){
					if (experiments.time_stamp_column_id == j)
						continue;
					values += std::string(",`") + experiments.column_names[j] + "`='" + local_buffer.escape_string(experiment_data[i][j]) + "'";
				}
				values += std::string(",time_stamp=FROM_UNIXTIME(") + ns_to_string(new_timestamp) + ")";

				local_buffer << "INSERT INTO buffered_experiments SET " << values;
				local_buffer << " ON DUPLICATE KEY UPDATE " << values;
				local_buffer.send_query();
			}
		}
		if (new_schedule.size() > 0)
		image_server.register_server_event(ns_image_server_event("Caching ") << new_schedule.size() << " scheduled time points in the local db...", &central_db);
		long last_displayed_percent = -5;
		for (unsigned int i = 0; i < new_schedule.size(); i++){
			const long percent((100*i)/new_schedule.size());
				if (percent >= last_displayed_percent+5){
					image_server.add_subtext_to_current_event(ns_to_string(percent) + "%...", &central_db);
					last_displayed_percent = percent;
				}
			std::string all_values;
			all_values += "`";
			all_values += capture_schedule.table_format.column_names[0] + "`='" + local_buffer.escape_string(new_schedule[i][5]) + "'";		
			for (unsigned int j = 1; j < capture_schedule.table_format.column_names.size(); j++){
				if (j == capture_schedule.time_stamp_column)
					continue;
				all_values += std::string( ", `") + capture_schedule.table_format.column_names[j] + "`='" + local_buffer.escape_string(new_schedule[i][5+j]) + "'";
			}
			all_values+=std::string(",time_stamp=FROM_UNIXTIME(") + ns_to_string(new_timestamp) + ")";
			

			std::string update_values;
			update_values +=
				// std::string("problem=") + new_schedule[i][5+capture_schedule.problem_column] + ","
							 std::string("scheduled_time=") + new_schedule[i][5+capture_schedule.scheduled_time_column] + ","
						   + std::string("missed=") + new_schedule[i][5+capture_schedule.missed_column] + ","
						   + std::string("censored=") + new_schedule[i][5+capture_schedule.censored_column] +","
						//   + std::string("transferred_to_long_term_storage=") + new_schedule[i][5+capture_schedule.transferred_to_long_term_storage_column] +","
						   + std::string("time_during_transfer_to_long_term_storage=") + new_schedule[i][5+capture_schedule.time_during_transfer_to_long_term_storage_column] +","
						   + std::string("time_during_deletion_from_local_storage=") + new_schedule[i][5+capture_schedule.time_during_deletion_from_local_storage_column] + ","
						   + std::string("time_stamp=FROM_UNIXTIME(") + ns_to_string(update_start_time.local_time) + ")";


			local_buffer << "INSERT INTO buffered_capture_schedule SET " << all_values
						 << " ON DUPLICATE KEY UPDATE " << update_values;
			local_buffer.send_query();
		}
		if (new_schedule.size() > 0)
			image_server.add_subtext_to_current_event("Done.\n", &central_db);
	}
	//if no changes to the schedule were made, look to see find changes made to any capture samples
	else{

		ns_sql_result capture_sample_data;
		ns_get_all_column_data_from_table("capture_samples",capture_samples.column_names,
			std::string("WHERE time_stamp >= FROM_UNIXTIME(") + ns_to_string(time_of_last_update_from_central_db.remote_time) +") "
			" AND time_stamp < FROM_UNIXTIME(" + ns_to_string(update_start_time.remote_time) +") "
				,capture_sample_data,&central_db);
		if (capture_sample_data.size() > 0){
			image_server.register_server_event(ns_image_server_event("Caching ") << capture_sample_data.size() << " samples in the local db...",&central_db);
			//local_buffer_db.send_query("DELETE FROM buffered_capture_samples");
			for (unsigned int i = 0; i < capture_sample_data.size(); i++) {
				std::string values;
				values += "`";
				values += capture_samples.column_names[0] + "`='" + local_buffer.escape_string(capture_sample_data[i][0]) + "'";
				for (unsigned int j = 1; j < capture_samples.column_names.size(); j++)
					values += std::string(",`") + capture_samples.column_names[j] + "`='" + local_buffer.escape_string(capture_sample_data[i][j]) + "'";

				local_buffer << "INSERT INTO buffered_capture_samples SET " << values
					<< " ON DUPLICATE KEY UPDATE " << values;
				local_buffer.send_query();
			}
			image_server.add_subtext_to_current_event("Done.\n", &central_db);
		}
	}
		
	local_buffer.send_query("COMMIT");
	//lock.unlock();

	commit_all_local_non_schedule_changes_to_central_db(update_start_time,local_buffer,central_db);

	central_db << "SELECT k,v FROM constants WHERE time_stamp > FROM_UNIXTIME(" << time_of_last_update_from_central_db.remote_time << ")";
	ns_sql_result cres;
	central_db.get_rows(cres);
	std::vector<std::string> notable_constants;
	for (unsigned int i = 0; i < cres.size(); i++) {
		if (cres[i][0].find("duration_until_next_") == cres[i][0].npos &&
			cres[i][0].find("_alert_submission") == cres[i][0].npos &&
			cres[i][0] != "last_missed_scan_check_time" &&
			cres[i][0] != "image_metadata_deletion_lock" &&
			cres[i][0].find("job_discovery_lock") == cres[i][0].npos)
			notable_constants.push_back(cres[i][0] + "=" + cres[i][1]);
	}

	if (!notable_constants.empty()){
		ns_image_server_event ev("Updating constants in local buffer :\n");
		
		for (unsigned int i = 0; i < notable_constants.size(); i++)
			ev << "  " << notable_constants[i] << "\n";
		image_server.register_server_event(ev, &central_db);
	}
	for (unsigned int i = 0; i < cres.size(); i++)
		image_server.set_cluster_constant_value(local_buffer.escape_string(cres[i][0]),local_buffer.escape_string(cres[i][1]),&local_buffer,update_start_time.local_time);
	time_of_last_update_from_central_db = update_start_time;		
	store_last_update_time_in_db(time_of_last_update_from_central_db,local_buffer);

	lock.release();
}


void ns_buffered_capture_scheduler::store_last_update_time_in_db(const ns_synchronized_time & time,ns_local_buffer_connection & sql){
	image_server.set_cluster_constant_value("local_time_of_last_buffer_upload_to_central_db",ns_to_string(time.local_time),&sql);
	image_server.set_cluster_constant_value("central_time_of_last_buffer_upload_to_central_db",ns_to_string(time.remote_time),&sql);
}

void ns_buffered_capture_scheduler::register_scan_as_finished(const std::string & device_name, ns_image_capture_specification & spec, const ns_64_bit problem_id, ns_local_buffer_connection & local_buffer_sql){
		
	image_capture_data_manager.register_capture_stop(spec,problem_id,local_buffer_sql);
	
	ns_image_server_event ev;
	if (spec.capture_schedule_entry_id == 0)
		ev << "Autocapture";
	else ev << "Capture";
	ev << " on [" << device_name << "] completed.";
	image_server.register_server_event(ev,&local_buffer_sql);
}


/*
void ns_buffered_capture_scheduler::transfer_to_long_term_storage(const unsigned long central_db_schedule_id,ns_sql & sql){
	//attempt to transfer the buffered image to long term storage
if (!image_server.exit_requested){

	ns_ex transfer_to_long_term_storage_error;
	try{		
		if (!problem_id && 	arguments.capture_specification.capture_schedule_entry_id != 0){
			if (device->device.name==image_server.simulated_device_name() &&
				arguments.capture_specification.capture_parameters.find("long_term_storage_err") != string::npos)
				throw ns_ex("Simulated scan simulated a long_term_storage_error!");
					
			arguments.image_capture_data_manager->transfer_image_to_long_term_storage(device->device.name,arguments.capture_specification.capture_schedule_entry_id,arguments.capture_specification.image,sql());
		}
	}
	catch(ns_ex & ex){
		transfer_to_long_term_storage_error = ex;
	}
	catch(std::exception & ex){
		transfer_to_long_term_storage_error = ex;
	}
	catch(...){
		transfer_to_long_term_storage_error << "Unknown exception thrown during transfer to long term storage.";
	}
			
	//mark the capture_sample image no longer being processed (free it up for processing).
		
			
	if (transfer_to_long_term_storage_error.text().size() !=0){
		cerr << "\nThrowing file conversion exception!\n";
		cerr.flush();
		throw transfer_to_long_term_storage_error;
	}
}*/


bool ns_buffered_capture_scheduler::run_pending_scans(const std::string & device_name, ns_local_buffer_connection & local_buffer_sql){
	image_server.alert_handler.buffer_all_alerts_locally(true);
	//cerr << "PC " << d.name << "\n";
	//only run one capture at a time on a device.
	if (image_server.device_manager.device_is_currently_scanning(device_name))
		return false;
	//cerr << ">";
	ns_capture_thread_arguments * ca(0);
	//we assume that only one cluster node has possession of the cluster, so no race conditions can exist.
	//sql.set_autocommit(false);
	try{
		unsigned long current_time = ns_current_time();
		//check db to see if any captures are pending

		local_buffer_sql.send_query("BEGIN");
		local_buffer_sql << "SELECT sched.experiment_id, samp.parameters, samp.name, samp.id, exp.name, sched.id, sched.scheduled_time, samp.turn_off_lamp_after_capture, samp.raw_image_size_in_bytes, samp.desired_capture_duration_in_seconds, samp.image_resolution_dpi,samp.size_x,samp.size_y "
				<< "FROM buffered_capture_schedule as sched, buffered_capture_samples as samp, buffered_experiments as exp "
				<< "WHERE samp.device_name = '" << device_name
				<< "' AND sched.scheduled_time < '" << (long int)current_time
				<< "' AND sched.time_at_start = '0' "
				<< " AND sched.sample_id = samp.id "
				<< " AND sched.experiment_id = exp.id "
				<< " AND sched.missed = 0 "
				<< " AND sched.time_at_finish = 0 "
				<< " AND sched.censored = 0 "
				<< "ORDER BY sched.scheduled_time DESC ";
		//we assume that only one cluster node has possession of the cluster, so no race conditions can exist.
		//	 << "FOR UPDATE " ;
		ns_sql_result events;
		local_buffer_sql.get_rows(events);
		bool ret = false;
		if (events.size() == 0){
			//No jobs to process
			local_buffer_sql.send_query("COMMIT");
			//cerr << "No jobs found on " << d.name << "\n";
			
			//cerr << "Leaving " << d.name << " search.\n";
			return ret;
		}
		
		//cancel any pending autoscans on this device
		if (image_server.device_manager.set_autoscan_interval(device_name, 0)){
			image_server.device_manager.register_autoscan_clash(device_name);
			//image_server.update_device_status_in_db(sql);
			std::string txt("A scan was scheduled on a device currently set to run autoscans: ");
			txt+=device_name;
			image_server.register_server_event(ns_image_server_event(txt),&local_buffer_sql);
		//	image_server.alert_handler.submit_locally_buffered_alert(ns_buffered_alert(txt,false,true));
		}

		unsigned long int scheduled_start_time = atol(events[0][6].c_str());

		//first, we flag any scans that waited for too long before occuring
		if (image_server.maximum_allowed_local_scan_delay() == 0)
			image_server.update_scan_delays_from_db(&local_buffer_sql);

		if (image_server.maximum_allowed_local_scan_delay() != 0 &&
			current_time - scheduled_start_time > 60*image_server.maximum_allowed_local_scan_delay()){
			//mark missed captures as missed.
			for (unsigned int i = 0; i < events.size(); i++){
				std::string query =  "UPDATE buffered_capture_schedule SET missed = 1 WHERE id= \"" + events[i][5] + "\"";
				local_buffer_sql.send_query(query);
				local_buffer_sql.send_query("COMMIT");
				return false;
			}
		}
		image_server.register_server_event(ns_image_server_event("Starting capture on ") << device_name << " of " <<  events[0][4] << "::" << events[0][2] << "...",&local_buffer_sql);
		ns_capture_thread_arguments * ca = new ns_capture_thread_arguments;
		ca->capture_specification.volatile_storage = 0;
		//prepare to start capture in its own thread
		ca->device_name = device_name;
		ca->capture_specification.capture_parameters = events[0][1];
		ca->capture_specification.turn_off_lamp_after_capture= events[0][7] == "1";
		//ca->calling_dispatcher = this;
		ca->buffered_capture_scheduler = this;
		unsigned long raw_image_size_in_bytes(atol(events[0][8].c_str()));
		const unsigned long desired_capture_duration_in_seconds(atol(events[0][9].c_str()));
		const double im_res(atof(events[0][10].c_str()));
		const ns_vector_2d im_size(atof(events[0][11].c_str()),atof(events[0][12].c_str()));
		if (raw_image_size_in_bytes == 0){
			//guess size of image from its size and resolution
			unsigned long bytes(2*(unsigned long)(im_size.x*im_size.y*im_res*im_res) + 16*1024);  //*2 because it is 16 bit
			if (bytes > 2*300*1024*1024)  //if the size doesn't make any sense (e.g taking up more than a third of the scanner)
				bytes = 0;				//give up and just don't do speed regulation for the first scan.
			raw_image_size_in_bytes = bytes;
		}
		ca->capture_specification.speed_regulator.initialize_for_capture(raw_image_size_in_bytes,desired_capture_duration_in_seconds);
		
		//capture will need to mark job as done by setting time_at_finish
		ca->capture_specification.capture_schedule_entry_id= ns_atoi64(events[0][5].c_str());
		ca->capture_specification.image.experiment_id = ns_atoi64(events[0][0].c_str());
		ca->capture_specification.image.sample_id = ns_atoi64(events[0][3].c_str());
		ca->capture_specification.image.device_name = device_name;
		ca->capture_specification.image.capture_time = atol(events[0][6].c_str());
		ca->capture_specification.image.experiment_name = events[0][4];
		ca->capture_specification.image.sample_name= events[0][2];
	
		//create local storage linked to newly created capture_image and image records in the local db
		image_capture_data_manager.initialize_capture_start(ca->capture_specification,local_buffer_sql);

		//capture thread will delete ca.
		image_server.device_manager.run_capture_on_device(ca);
		//We now abandon the thread.
		//d.capture_thread->block_on_finish();
		//see if anything untoward happened during capture
		return true;
	}
	catch(std::exception & exception){
		ns_ex ex(exception);
		
		std::cerr << "Found run_pending_captures exception " << ex.text() << "\n";
		//sql.set_autocommit(true);
		//OK, something went wrong with the capture.
		//Give up for now but swallow exception so that 
		//the dispatcher can try again later.
		local_buffer_sql.clear_query();
		ns_64_bit event_id = image_server.register_server_event(ex,&local_buffer_sql);
		
		if (ca != 0){
			local_buffer_sql << "UPDATE capture_schedule SET problem = " << event_id << " WHERE id = " << ca->capture_specification.capture_schedule_entry_id;
			local_buffer_sql.send_query();
			ns_safe_delete(ca);
		}
		return true;
	}
	
}

void ns_buffered_capture_scheduler::run_pending_scans(const ns_image_server_device_manager::ns_device_name_list & devices, ns_local_buffer_connection & sql){
		
	try{
				
		const bool handle_simulated_devices(image_server.register_and_run_simulated_devices(&sql));

		for (unsigned int i = 0; i < devices.size(); i++){
			if (image_server.currently_experiencing_a_disk_storage_emergency)
				break; 
			if (image_server.exit_has_been_requested)
				break;
			if (devices[i].simulated_device && !handle_simulated_devices)
				continue;
			if (image_server.device_manager.device_is_currently_scanning(devices[i].name))
				continue;
			if (devices[i].paused)
				continue;
				
			if (run_pending_scans(devices[i].name,sql))
				ns_thread::sleep(5);//wait three seconds in between requesting scans to keep USB chatter sane.
		}
	}
	catch(ns_ex & ex){
		image_server.register_server_event(ex,&sql);
	}

}

