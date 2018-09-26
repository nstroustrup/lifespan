#ifndef NS_BUFFERED_CAPTURE_SCHEDULER
#define NS_BUFFERED_CAPTURE_SCHEDULER
#include "ns_image_server.h"
#include "ns_image_capture_data_manager.h"


struct ns_table_column_spec{
	ns_table_column_spec(){}
	ns_table_column_spec(const std::string & n,long * i):name(n),index(i){}
	std::string name;
	long * index;
};

class ns_table_format_processor{
public:
	void load_column_names_from_db(const std::string & table_name,ns_image_server_sql * sql);
	bool loaded();
	void load_if_needed(const std::string & table_name,ns_image_server_sql * sql){
		if (!loaded())
			load_column_names_from_db(table_name,sql);
		}
	void get_column_name_indicies(const std::vector<ns_table_column_spec> & desired_columns);
	std::vector<std::string> column_names;
	long time_stamp_column_id;

private:
	void sql_get_column_names(const std::string & table_name, std::vector<std::string> & column_names, ns_image_server_sql * sql);
};

struct ns_buffered_capture_schedule_table_info{
	long id_column,
		problem_column,
		captured_image_id_column,
		timestamp_column,
		time_at_finish_column,
		transfer_status_column,
		experiment_id_column,
		sample_id_column,
		time_at_imaging_start_column;
	void load_if_needed(ns_image_server_sql * sql){
		if (!table_format.loaded())
			load(sql);
	}
	void load(ns_image_server_sql * sql){
		table_format.load_column_names_from_db("buffered_capture_schedule",sql);
		std::vector<ns_table_column_spec> spec;
		spec.push_back(ns_table_column_spec("id",&id_column));
		spec.push_back(ns_table_column_spec("problem",&problem_column));
		spec.push_back(ns_table_column_spec("captured_image_id",&captured_image_id_column));
		spec.push_back(ns_table_column_spec("problem",&problem_column));
		spec.push_back(ns_table_column_spec("time_stamp",&timestamp_column));
		spec.push_back(ns_table_column_spec("time_at_finish",&time_at_finish_column));
		spec.push_back(ns_table_column_spec("transferred_to_long_term_storage", &transfer_status_column));
		spec.push_back(ns_table_column_spec("experiment_id", &experiment_id_column));
		spec.push_back(ns_table_column_spec("sample_id", &sample_id_column));
		spec.push_back(ns_table_column_spec("time_at_imaging_start", &time_at_imaging_start_column));
		
		table_format.get_column_name_indicies(spec);
	}
	ns_table_format_processor table_format;
};

struct ns_capture_schedule_table_info{
	long id_column,
		 time_stamp_column,
		 scheduled_time_column,
		 problem_column,
		 missed_column,
		 censored_column,
		 transferred_to_long_term_storage_column,
		 time_during_transfer_to_long_term_storage_column,
		 time_during_deletion_from_local_storage_column,
		 captured_image_id_column;

	void load_if_needed(ns_image_server_sql * sql){
		if (!table_format.loaded())
			load(sql);
	}
	void load(ns_image_server_sql * sql){
		table_format.load_column_names_from_db("capture_schedule",sql);
		std::vector<ns_table_column_spec> spec;
		spec.push_back(ns_table_column_spec("id",&id_column));
		spec.push_back(ns_table_column_spec("problem",&problem_column));
		spec.push_back(ns_table_column_spec("scheduled_time",&scheduled_time_column));
		spec.push_back(ns_table_column_spec("problem",&problem_column));
		spec.push_back(ns_table_column_spec("missed",&missed_column));
		spec.push_back(ns_table_column_spec("censored",&censored_column));
		spec.push_back(ns_table_column_spec("transferred_to_long_term_storage",&transferred_to_long_term_storage_column));
		spec.push_back(ns_table_column_spec("transferred_to_long_term_storage",&transferred_to_long_term_storage_column));
		spec.push_back(ns_table_column_spec("time_during_transfer_to_long_term_storage",&time_during_transfer_to_long_term_storage_column));
		spec.push_back(ns_table_column_spec("time_during_deletion_from_local_storage",&time_during_deletion_from_local_storage_column));
		spec.push_back(ns_table_column_spec("time_stamp", &time_stamp_column));
		spec.push_back(ns_table_column_spec("captured_image_id",&captured_image_id_column));
		table_format.get_column_name_indicies(spec);
	}
	ns_table_format_processor table_format;
};

struct ns_synchronized_time{
	ns_synchronized_time(){}
	ns_synchronized_time(const unsigned long local,const unsigned long remote):local_time(local),remote_time(remote){}
	unsigned long local_time,
				  remote_time;
};
class ns_buffered_capture_scheduler{
public:
	enum {ns_default_update_time=413096400};
	//time_of_last_update_from_central_db must be set to the past to trigger an update when an image server starts.
	//however, mysql timestamp collumns can't handle '0' values and so we chose something larger
	ns_buffered_capture_scheduler():image_capture_data_manager(image_server.image_storage),time_of_last_update_from_central_db(ns_default_update_time,ns_default_update_time),buffer_capture_scheduler_lock("ns_buffer_capture_scheduler_lock")
	{}


	void clear_local_cache(ns_local_buffer_connection & local_buffer_sql);
	void register_scan_as_finished(const std::string & device_name, ns_image_capture_specification & spec, const ns_64_bit problem_id, ns_local_buffer_connection & local_buffer_sql);

	ns_synchronized_time time_since_last_db_update()const {return ns_synchronized_time(ns_current_time()-time_of_last_update_from_central_db.local_time,0);}

	void update_local_buffer_from_central_server(ns_image_server_device_manager::ns_device_name_list & connected_devices,ns_local_buffer_connection & local_buffer_sql, ns_sql & central_db);
	void commit_local_changes_to_central_server(ns_local_buffer_connection & local_buffer_sql, ns_sql & central_db);
	void run_pending_scans(const ns_image_server_device_manager::ns_device_name_list & devices, ns_local_buffer_connection & sql);
	
	ns_image_capture_data_manager image_capture_data_manager;

	void get_last_update_time(ns_local_buffer_connection & local_buffer_sql);
	static void store_last_update_time_in_db(const ns_synchronized_time & time,ns_local_buffer_connection & sql);
	
private:
	ns_image_server_device_manager * device_manager;

	void commit_all_local_schedule_changes_to_central_db(const ns_synchronized_time & update_start_time,ns_local_buffer_connection & local_buffer_sql, ns_sql & central_db);
	void commit_all_local_non_schedule_changes_to_central_db(const ns_synchronized_time & update_start_time,ns_local_buffer_connection & local_buffer_sql, ns_sql & central_db);
	
	bool run_pending_scans(const std::string & device_name, ns_local_buffer_connection & local_buffer_sql);
	ns_synchronized_time time_of_last_update_from_central_db;

	ns_capture_schedule_table_info capture_schedule;
	ns_buffered_capture_schedule_table_info buffered_capture_schedule;
	ns_table_format_processor experiments,
							  capture_samples;
	ns_lock buffer_capture_scheduler_lock;
		
};

#endif
