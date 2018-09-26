#ifndef NS_IMAGE_STORAGE_HANDLER_H
#define NS_IMAGE_STORAGE_HANDLER_H

#include "ns_image_storage.h"
#include <algorithm>
#include <map>
#include <fstream>
#include "ns_simple_image_cache.h"
#include "ns_image_server_alerts.h"
#include "ns_file_location_specification.h"

#include "ns_image_server_sql.h"

void ns_image_handler_submit_alert(const ns_alert::ns_alert_type & alert, const std::string & text, const std::string & detailed_text, ns_image_server_sql * sql);
void ns_image_handler_submit_alert_to_central_db(const ns_alert::ns_alert_type & alert, const std::string & text, const std::string & detailed_text);

//void ns_image_handler_submit_buffered_alert(const ns_alert_handler::alert_type & alert, const std::string & text);
//void ns_image_handler_submit_alert(const ns_alert_handler::alert_type & alert, const std::string & text);
//void ns_image_handler_submit_buffered_alert(const ns_alert_handler::alert_type & alert, const std::string & text);


void ns_image_handler_register_server_event_to_central_db(const ns_ex & ev);
void ns_image_handler_register_server_event_to_central_db(const ns_image_server_event & ev);


void ns_image_handler_register_server_event(const ns_image_server_event & ev,ns_image_server_sql * sql);
void ns_image_handler_register_server_event(const ns_ex & ev, ns_image_server_sql * sql);



ns_performance_statistics_analyzer & performance_statistics();

ns_64_bit ns_image_storage_handler_host_id();
long ns_image_storage_handler_server_timeout_interval();
std::string ns_image_server_scratch_directory();
std::string ns_image_server_miscellaneous_directory();
std::string ns_image_server_cache_directory();
void ns_move_experiment_partition(ns_64_bit experiment_id, const std::string & partition, ns_sql & sql);
ns_sql * ns_get_sql_connection();
ns_local_buffer_connection * ns_get_local_buffer_connection();

std::string ns_get_default_partition();

class ns_image_server_results_file;

#pragma warning(disable: 4355) //our use of this in constructor is valid, so we suppress the error message

//when servers want to store an image, they should acquire a ns_image_storage_reciever from the appropraite
//image storage handler object, and write to it.
//This decouples decisions of where images should go with the operations that produce images.
//Also, decision making as to where files go is centralized.
class ns_image_storage_handler{
public:
	typedef ns_8_bit ns_component;

	void set_file_permissions_readable_by_other(const bool readable) {
	  output_file_permissions = readable ?ns_dir::ns_group_read: ns_dir::ns_no_special_permissions;
	}

	ns_image_storage_handler() :network_lock("ns_ish::network"),
		request_storage_lock("ns_ish::storage"),
		experiment_partition_cache_lock("ns_ish::partition"),
		cache(512 * 1024), experiment_partition_cache_update_period(5 * 60), last_check_showed_write_access_to_long_term_storage(false), time_of_last_successful_write_check(0),
		experiment_partition_cache_last_update_time(0), verbosity(ns_standard), simulate_long_term_storage_errors(false) {}

	void set_directories(const std::string & _volatile_storage_directory, const std::string & _long_term_storage_directory);
	void update_volatile_storage_directory_for_parallel_processes(unsigned long system_parallel_process_id);

	typedef enum { ns_forbid_volatile, ns_allow_volatile, ns_require_volatile } ns_volatile_storage_behavior;

	ns_image_storage_reciever_handle<ns_component> request_storage(ns_image_server_captured_image_region & captured_image_region, const ns_image_type & image_type, const float compression_ratio, const unsigned long max_line_length, ns_image_server_sql * sql, const ns_volatile_storage_behavior volatile_storage_behavior) const;
	ns_image_storage_reciever_handle<ns_component> request_storage_ci(ns_image_server_captured_image & captured_image, const ns_image_type & image_type, const float compression_ratio, const unsigned long max_line_length, ns_image_server_sql * sql, ns_image_server_image & output_image, bool & had_to_use_local_storage, const ns_volatile_storage_behavior volatile_storage_behavior) const;
	ns_image_storage_reciever_handle<ns_component> request_storage(ns_image_server_image & image, const ns_image_type & image_type, const float compression_ratio,const unsigned long max_line_length, ns_image_server_sql * sql, bool & had_to_use_volatile_storage, const bool report_to_db, const ns_volatile_storage_behavior volatile_storage_behavior) const;
	ns_image_storage_reciever_handle<ns_16_bit> request_storage_16_bit(ns_image_server_image & image, const ns_image_type & image_type, const float compression_ratio, const unsigned long max_line_length, ns_image_server_sql * sql, bool & had_to_use_volatile_storage, const bool report_to_db, const ns_volatile_storage_behavior volatile_storage_behavior) const;
	ns_image_storage_reciever_handle<float> request_storage_float(ns_image_server_image & image, const ns_image_type & image_type, const float compression_ratio, const unsigned long max_line_length, ns_image_server_sql * sql, bool & had_to_use_volatile_storage, const bool report_to_db, const ns_volatile_storage_behavior volatile_storage_behavior) const;


	bool long_term_storage_was_recently_writeable(const unsigned long time_cutoff_in_seconds = 0) const;
	unsigned long time_of_last_successful_long_term_storage_write()const {return time_of_last_successful_write_check;}
	bool test_connection_to_long_term_storage(const bool test_for_write_access);

	bool image_exists(ns_image_server_image & image, ns_image_server_sql * sql, bool only_long_term_storage=false) const;

	bool assign_unique_filename(ns_image_server_image & image, ns_image_server_sql * sql) const;
	std::ifstream * request_metadata_from_disk(ns_image_server_image & image,const bool binary,ns_image_server_sql * sql)const;

	void fix_orphaned_captured_images(ns_image_server_sql * sql)const;

	std::ofstream * request_metadata_output(ns_image_server_image & image, const ns_image_type & image_type, const bool binary,ns_image_server_sql * sql) const;

		
	
	double get_experiment_video_size_on_disk(const ns_64_bit  experiment_id,ns_sql & sql) const;
	double get_region_metadata_size_on_disk(const ns_64_bit  region_id,ns_sql & sql) const;
	double get_region_images_size_on_disk(const ns_64_bit region_id,const ns_processing_task t,ns_sql & sql) const;
	double get_sample_images_size_on_disk(const ns_64_bit region_id,const ns_processing_task t,ns_sql & sql) const;


	
	std::ofstream * request_binary_output_for_captured_image(const ns_image_server_captured_image & captured_image, const ns_image_server_image & image, bool volatile_storage,ns_image_server_sql * sql) const;
	ns_image_server_image create_image_db_record_for_captured_image(ns_image_server_captured_image & image, ns_image_server_sql * sql, const ns_image_type & image_type = ns_tiff) const;

	std::ofstream * request_miscellaneous_storage(const std::string & filename);

	std::ofstream * request_volatile_binary_output(const std::string & filename);

	template<class ns_bit_depth>
	ns_image_storage_reciever_handle<ns_bit_depth>  request_volatile_storage(const std::string & filename, const unsigned long max_line_length, const ns_image_type & image_type,const bool report_to_db = true) const{
		std::string dir = volatile_storage_directory + DIR_CHAR_STR + ns_image_server_scratch_directory();

		std::string fname = dir + DIR_CHAR_STR + filename;
		ns_dir::convert_slashes(fname);
		ns_dir::convert_slashes(dir);
		ns_dir::create_directory_recursive(dir);

		if (!ns_dir::file_is_writeable(fname)){
	//		ns_image_handler_submit_alert(ns_alert_handler::ns_volatile_storage_error,"Could not access volatile storage(7).");
			throw ns_ex("ns_image_storage_handler::request_volatile_storage()::Volatile storage location is not writeable: ") << fname << ns_file_io;
		}

		if (verbosity >= ns_verbose)
			ns_image_handler_register_server_event_to_central_db(ns_image_server_event("ns_image_storage_handler::Opening ") << fname << " for output.");
		return ns_image_storage_reciever_handle<ns_bit_depth>(new ns_image_storage_reciever_to_disk<ns_bit_depth>(max_line_length, fname,image_type,true));
	}

	

	std::ifstream * request_from_volatile_storage_raw(const std::string & filename) const;

	ns_image_storage_reciever_handle<ns_component> request_local_cache_storage(const std::string & filename, const ns_image_type & image_type, const unsigned long max_line_length, const bool report_to_db = true) const;
	ns_image_storage_reciever_handle<float> request_local_cache_storage_float(const std::string & filename, const ns_image_type & image_type, const unsigned long max_line_length, const bool report_to_db = true) const;



	unsigned long request_local_cache_file_size(const std::string & filename) const;
	
	ns_image_storage_source_handle<ns_component> request_from_local_cache(const std::string & filename, const bool report_to_db=true)const;
	ns_image_storage_source_handle<float> request_from_local_cache_float(const std::string & filename, const bool report_to_db = true)const;


	std::string add_to_local_cache(ns_image_server_image & image, const ns_image_type & image_type, ns_image_server_sql * sql)const;

	ns_image_storage_source_handle<ns_component> request_from_volatile_storage(const std::string & filename,const bool report_to_db=true)const;
	bool delete_from_volatile_storage(const std::string & filename)const;

	bool delete_from_local_cache(const std::string & filename)const;

	void clear_local_cache()const;
	

	bool delete_from_storage(ns_image_server_captured_image & image,const ns_file_deletion_type & type, ns_image_server_sql * sql)const;
	bool delete_from_storage(ns_image_server_image & image,const ns_file_deletion_type & type,ns_image_server_sql * sql)const;

	void delete_file_specification(const ns_file_location_specification & spec,const ns_file_deletion_type & type)const;


	ns_image_storage_source_handle<ns_component> request_from_storage(ns_image_server_captured_image_region & region_image, ns_image_server_sql * sql)const;
	ns_image_storage_source_handle<ns_component> request_from_storage(ns_image_server_captured_image_region & region_image, const ns_processing_task & task, ns_image_server_sql * sql)const;
	ns_image_storage_source_handle<ns_component> request_from_storage(ns_image_server_image & image, const ns_processing_task & task, ns_image_server_sql * sql)const;

	ns_image_storage_source_handle<ns_component> request_from_storage(ns_image_server_image & image, ns_image_server_sql * sql)const;

	ns_image_storage_source_handle<ns_component> request_from_storage(ns_image_server_captured_image & captured_image, ns_image_server_sql * sql)const;
	
	typedef enum {ns_volatile_storage,ns_long_term_storage,ns_volatile_and_long_term_storage} ns_storage_location;

	template<class ns_comp,bool low_memory_single_line_reads = false>
	ns_image_storage_source_handle<ns_comp, low_memory_single_line_reads> request_from_storage_n_bits(ns_image_server_captured_image & captured_image, ns_image_server_sql * sql,const ns_storage_location &location)const{
		ns_image_server_image im;
		im.id = captured_image.capture_images_image_id;
		//cerr << "loading image info...";
		im.load_from_db(im.id,sql);
	//	im.load_from_db(region_image.region_images_image_id,sql);
	//	cerr << "loading image...";

		return request_from_storage_n_bits<ns_comp, low_memory_single_line_reads>(im,sql,location);
	}
	unsigned long free_space_in_volatile_storage_in_mb() const{
		return ns_dir::get_free_disk_space(volatile_storage_directory);
	}
	bool long_term_storage_is_accessible(ns_file_location_specification file_location, const char * file, const unsigned long line) const{
		if (simulate_long_term_storage_errors || !ns_dir::file_exists(file_location.long_term_directory)){
			ns_thread::sleep(10);
			if (simulate_long_term_storage_errors || !ns_dir::file_exists(file_location.long_term_directory)){
				ns_image_handler_submit_alert_to_central_db(
					ns_alert::ns_long_term_storage_error,
					"Could not access long term storage",
					std::string("Could not access long term storage (") + file + "::" + ns_to_string(line) + ")");
				return false;
			}
		}
		return true;
	}


	template<class ns_comp, bool low_memory_single_line_reads = false >
	ns_image_storage_source_handle<ns_comp, low_memory_single_line_reads> request_from_storage_n_bits(ns_image_server_image & image, ns_image_server_sql * sql,const ns_storage_location & location) const{
		ns_ex stored_error[3];
	
		ns_file_location_specification file_location(look_up_image_location_no_extension_alteration(image,sql));

		std::string  input_filename;
		//try and get the data from volatile storage (to reduce network traffic)
		if ((location == ns_volatile_storage || location == ns_volatile_and_long_term_storage) &&
			volatile_storage_directory.size() != 0 && ns_dir::file_exists(file_location.volatile_directory)){	
			if (ns_dir::file_exists(file_location.absolute_volatile_filename())){
				try{
					if (verbosity > ns_standard)
						ns_image_handler_register_server_event(ns_image_server_event("ns_image_storage_handler::Opening VT ") << file_location.absolute_volatile_filename() << " for input." << ns_ts_minor_event,sql);
					ns_image_storage_source_from_disk<ns_comp, low_memory_single_line_reads> * tmp;
					tmp = new ns_image_storage_source_from_disk<ns_comp, low_memory_single_line_reads>(file_location.absolute_volatile_filename(),true);
					return ns_image_storage_source_handle<ns_comp, low_memory_single_line_reads>(tmp);
				}
				catch(ns_ex & ex){
					//if we are allowed to search for the image in long term storage, look there.
					if (location == ns_volatile_and_long_term_storage)
						stored_error[0] = ex;
					else throw ex;
				}
			}
		}
		if (location == ns_long_term_storage || location == ns_volatile_and_long_term_storage){
			if(!long_term_storage_is_accessible(file_location,__FILE__,__LINE__))
				throw ns_ex("Could not access long term storage.");
			
			//try and get the data from long term storage
			if (long_term_storage_directory.size() != 0 && ns_dir::file_exists(file_location.long_term_directory)){	
				try{

					if (simulate_long_term_storage_errors) throw ns_ex("Simulated error");
					if (ns_dir::file_exists(file_location.absolute_long_term_filename())){
					//	ns_image_handler_register_server_event(ns_image_server_event("ns_image_storage_handler::Opening LT ",false) << display_filename << " for input." << ns_ts_minor_event);
						return ns_image_storage_source_handle<ns_comp, low_memory_single_line_reads>(new ns_image_storage_source_from_disk<ns_comp, low_memory_single_line_reads>(file_location.absolute_long_term_filename(),false));
					}
				}
				catch(ns_ex & ex){
				
					*sql << "UPDATE " << sql->table_prefix() << "images SET problem=1 WHERE id = " << image.id;
					sql->send_query();
					throw ns_ex("ns_image_storage_handler::request_from_storage_n_bis()::file ") << image.path <<
						DIR_CHAR_STR << image.filename << " could not be opened: " << ex.text() << ns_file_io;
				}
			}
		}
		*sql << "UPDATE  " << sql->table_prefix() << "images SET problem=1 WHERE id = " << image.id;
		sql->send_query();
		throw ns_ex("ns_image_storage_handler::request_from_storage_n_bis()::file ") << image.filename << " could not be opened for reading: File does not exist in " << file_location_string(location) << ":" << image.path << ns_file_io;
	}
	
	///Cache frequently used masks in memory so they aren't reloaded each time over the network.
	//locked so we can access it from multiple threads simultaneously
	typedef ns_simple_image_cache cache_t;
	ns_simple_image_cache cache;

	inline std::string get_partition_for_experiment(const ns_64_bit experiment_id,ns_image_server_sql * sql,bool request_from_db_on_miss=true) const{
		return get_partition_for_experiment_int(experiment_id,sql,request_from_db_on_miss,true);
	}

	ns_image_server_image get_storage_for_path(const ns_file_location_specification & region_spec, const unsigned long path_id, const unsigned long path_group_id,
			const ns_64_bit region_info_id, const std::string & region_name, const std::string & experiment_name, const std::string & sample_name,const bool flow) const;
	
	static ns_image_server_image get_storage_for_specification(const ns_file_location_specification & spec);


	void set_experiment_partition_cache_update_period(unsigned long seconds);

	void move_experiment_partition(ns_64_bit experiment_id, const std::string & partition, ns_sql & sql);
	
	void refresh_experiment_partition_cache(ns_image_server_sql * sql);

	ns_file_location_specification get_file_specification_for_image(ns_image_server_image & image,ns_image_server_sql * sql) const;
	bool move_file(const ns_file_location_specification & source, const ns_file_location_specification & dest,bool volatile_storage);
	ns_image_server_image get_region_movement_metadata_info(ns_64_bit region_info_id,const std::string & data_source,ns_sql & sql) const;

	ns_file_location_specification get_path_for_region(ns_64_bit region_image_info_id,ns_image_server_sql * sql, const ns_processing_task task= ns_unprocessed) const;
	ns_file_location_specification get_base_path_for_region(ns_64_bit region_image_info_id,ns_image_server_sql * sql) const;
	ns_file_location_specification get_detection_data_path_for_region(ns_64_bit region_image_info_id, ns_image_server_sql * sql) const;

	ns_file_location_specification get_file_specification_for_path_data(const ns_file_location_specification & region_spec) const;
	ns_file_location_specification get_file_specification_for_movement_data(ns_64_bit region_info_id, const std::string & data, ns_image_server_sql * sql) const;

	static ns_file_location_specification convert_results_file_to_location(const ns_image_server_results_file * f);
	
	ns_file_location_specification get_path_for_sample(ns_64_bit sample_id, ns_image_server_sql * sql) const;
	ns_file_location_specification get_path_for_sample_captured_images(ns_64_bit sample_id, bool small_images, ns_image_server_sql * sql) const;


	ns_file_location_specification get_path_for_experiment(ns_64_bit experiment_id, ns_image_server_sql * sql) const;
	ns_file_location_specification get_path_for_video_storage(ns_64_bit experiment_id, ns_image_server_sql * sql) const;
	ns_file_location_specification get_storyboard_path(const ns_64_bit & experiment_id, const ns_64_bit & region_id, const ns_64_bit & subimage_id, const std::string & filename_suffix, const ns_image_type & type, ns_sql & sql, const bool just_path) const;

	ns_64_bit create_file_deletion_job(const ns_64_bit parent_processing_job_id,ns_sql & sql)const;
	void delete_file_deletion_job(const ns_64_bit deletion_job_id, ns_sql & sql)const;
	void submit_file_deletion_request(const ns_64_bit deletion_job_id,const ns_file_location_specification & spec, ns_sql & sql)const;
	void get_file_deletion_requests(const ns_64_bit deletion_job_id, ns_64_bit & parent_job_id, std::vector<ns_file_location_specification> & specs, ns_sql & sql)const;
	
	std::string get_relative_path_for_video(const ns_file_location_specification & spec, const bool for_sample_video, const bool only_base=false)const;
	std::string get_absolute_path_for_video(const ns_file_location_specification & spec, const bool for_sample_video, const bool only_base=false)const;
	std::string get_absolute_path_for_video_image(const ns_64_bit experiment_id, const std::string & rel_path, const std::string & filename,ns_sql & sql)const;
	
	typedef enum{ns_quiet=0,ns_standard=2,ns_deletion_events=4,ns_verbose=6} ns_verbosity_level;

	//be careful with this.  Setting a high verbosity level will cause image servers to contact the central server more often.
	//This is usually fine and great for debugging but is bad for servers attempting to capture images while the central server is unreachable.
	void set_verbosity(const ns_verbosity_level verbose_){verbosity=verbose_;}

	

	static const std::string file_location_string(const ns_storage_location & loc){
		switch(loc){
			case ns_volatile_storage: return "volatile storage";
			case ns_long_term_storage: return "long term storage";
			case ns_volatile_and_long_term_storage:return "volatile or long term storage";
			default: return "Unknown storage location";
		}
	}
	std::string movement_file_directory(ns_64_bit region_info_id,ns_image_server_sql * sql, std::string & absolute_directory_prefix) const;
	mutable bool simulate_long_term_storage_errors;
private:
	std::string get_storage_to_open(ns_image_server_image & image, const ns_image_type & image_type, const unsigned long max_line_length, ns_image_server_sql * sql, bool & had_to_use_local_storage, const bool report_to_db, const ns_volatile_storage_behavior volatile_storage_behavior) const;

	ns_dir::ns_output_file_permissions output_file_permissions;
	ns_file_location_specification look_up_image_location(ns_image_server_image & image,ns_image_server_sql * sql,const ns_image_type & image_type, const bool alter_extension=true) const;
	ns_file_location_specification look_up_image_location_no_extension_alteration(ns_image_server_image & image, ns_image_server_sql * sql) const;
	ns_file_location_specification compile_absolute_paths_from_relative(const std::string & rel_path, const std::string & partition, const std::string & filename) const ;
	
	void refresh_experiment_partition_cache_int(ns_image_server_sql * sql,const bool get_lock = true) const;

	std::string get_partition_for_experiment_int(const ns_64_bit experiment_id, ns_image_server_sql * sql,bool request_from_db_on_miss=true, const bool get_lock=true) const;

	mutable std::map<ns_64_bit,std::string> experiment_partition_cache;
	unsigned long experiment_partition_cache_update_period;
	mutable unsigned long experiment_partition_cache_last_update_time;
	mutable ns_lock experiment_partition_cache_lock;

	ns_socket_connection connect_to_fileserver_node(ns_sql & sql);

	ns_lock request_storage_lock;

	ns_lock network_lock;
	ns_socket socket;

	ns_verbosity_level verbosity;

	bool currently_sending_over_network;

	std::string long_term_storage_directory,
		   volatile_storage_directory,
			du_path;
	bool last_check_showed_write_access_to_long_term_storage;
	unsigned long time_of_last_successful_write_check;
};
#pragma warning(default: 4355) 
#endif
