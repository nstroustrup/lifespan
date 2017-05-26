#ifndef NS_IMAGE_SERVER_H
#define NS_IMAGE_SERVER_H
#include "ns_os_signal_handler.h"
#include "ns_ex.h"
#include "ns_single_thread_coordinator.h"
#include "ns_sql.h"
//#include "ns_image_server_dispatcher.h"
#include "ns_image_storage_handler.h"
#include "ns_image_server_images.h"
#include "ns_capture_device.h"
#include "ns_performance_statistics.h"
#include "ns_image_server_alerts.h"
#include "ns_svm_model_specification.h"
#include "ns_movement_state.h"
#include "ns_sql_table_lock_manager.h"

#ifndef NS_MINIMAL_SERVER_BUILD
#include "ns_posture_analysis_models.h"
#include "ns_image_server_automated_job_scheduler.h"
#include "ns_time_path_solver_parameters.h"
#include "ns_capture_device_manager.h"
#endif

#include "ns_image_registration_cache.h"

#include <iostream>
#include <fstream>
#include "ns_image_server_results_storage.h"
#include "ns_get_double.h"

///attached scanners have both human-readable names as well as
///USB-assigned hardware addresses.
struct ns_device_name{
	std::string name,
		hardware_alias;
};


struct ns_multiprocess_control_options{

	ns_multiprocess_control_options():
		process_id(0),
			total_number_of_processes(1),
			compile_videos(true),
			handle_software_updates(true),
			manage_capture_devices(true),
			dispatcher_port_offset(0),
			only_run_single_process(false){}

	unsigned long process_id,
				  total_number_of_processes,
				  dispatcher_port_offset;

	bool compile_videos,
		 handle_software_updates,
		 manage_capture_devices,
		 only_run_single_process;

	unsigned long port(const unsigned long base_port) const{
		return dispatcher_port_offset + base_port;
	}
	std::string host_name(const std::string & base_hostname) const{
		if (total_number_of_processes == 1)
			return base_hostname;
		return base_hostname + "::" + ns_to_string(process_id);
	}
	std::string simulated_device_name(const std::string & name) const{
		return host_name(name);
	}

	std::string volatile_storage(const std::string & base_storage) const{
		if (total_number_of_processes == 1)
			return base_storage;
		return base_storage + DIR_CHAR_STR + ns_to_string(process_id);
	}

	bool from_parameter(const std::string & parameter);
	void to_parameters(std::vector<std::string> & parameters);
};

struct ns_posture_analysis_model_entry_source {
	ns_posture_analysis_model::ns_posture_analysis_method analysis_method;
	std::string model_directory;
	//worm_detection_model_directory()
	void set_directory(const std::string long_term_storage_directory, const std::string & worm_detection_dir);
};

class ns_posture_analysis_model_entry : public ns_simple_cache_data<std::string, ns_posture_analysis_model_entry_source,std::string> {
public:
	ns_posture_analysis_model model_specification;
	std::string name;
	template <class a,class b, bool c>
	friend class ns_simple_cache;
private:
	ns_64_bit size_in_memory_in_kbytes() const {	return 0; }
	void load_from_external_source(const std::string & name, ns_posture_analysis_model_entry_source & external_source);
	static std::string to_id(const std::string & n) { return n; }
	const std::string & id() const { return name; }
	void clean_up(ns_posture_analysis_model_entry_source & external_source) {}
};

class ns_thread_output_state{
public:
	ns_thread_output_state() :internal_thread_id(0), external_thread_id(0),last_event_sql_id(0),thread_specific_logfile(0) {}
	ns_64_bit internal_thread_id,  //all threads get a unique thread id
			  external_thread_id,  //threads that need it get a unique id to register output to disk and database
			  last_event_sql_id;
	std::ofstream * thread_specific_logfile;
	bool separate_output() { return thread_specific_logfile != 0; }
	~ns_thread_output_state() { ns_safe_delete(thread_specific_logfile); }
};

void ns_write_experimental_data_in_database_to_file(const unsigned long experiment_id, const std::string & output_directory,ns_sql & sql);
void ns_zip_experimental_data(const std::string & output_directory,bool delete_original=false);
bool ns_update_db_using_experimental_data_from_file(const std::string new_database,bool use_existing_database, const std::string & output_directory,ns_sql & sql);

///ns_image_server is the portal through which all aspects of an image server node communicates and coordinate with each other
///It provides access to all filesystem i/o and SQL connections, as well as managing image-capture devices and logging any errors encountered
class ns_image_server{
public:
	ns_image_server();
	#ifndef NS_MINIMAL_SERVER_BUILD
	ns_image_server_device_manager device_manager;

	void set_multiprocess_control_options(const ns_multiprocess_control_options & opt){multiprocess_control_options = opt;}
	const ns_multiprocess_control_options & get_multiprocess_control_options() const{ return multiprocess_control_options;}
	
	#endif
	///image server nodes check the cluster to see if they are running the latest version of the software,
	///which is specified by (major).(minor).(software_version_compile).
	unsigned int software_version_major(){return 1;}
	///image server nodes check the cluster to see if they are running the latest version of the software,
	///which is specified by (major).(minor).(software_version_compile).
	unsigned int software_version_minor(){return 9;}
	///image server nodes check the cluster to see if they are running the latest version of the software,
	///which is specified by (major).(minor).(software_version_compile).
	unsigned int software_version_compile(){return _software_version_compile;}
	bool scan_for_problems_now();
	unsigned long mean_problem_scan_interval_in_seconds(){return 10*60;}
	inline void set_pause_status(bool p){server_pause_status=p;}
	inline bool server_is_paused(){return server_pause_status;}

	///A variety of user-configurable options are specified in the image server ini file.
	///load_constants() loads these from disk.  A log file is also opened,
	///at a filename whose suffix is specified by log_suffix.  (This allows worm terminal instances
	///to run on the same system as image server nodes)
	typedef enum {ns_image_server_type,ns_worm_terminal_type,ns_sever_updater_type} ns_image_server_exec_type;
	void load_constants(const ns_image_server_exec_type & log_suffix, const std::string & ini_file_location="", bool reject_incorrect_fields=true);

	///Hosts coordinate with each other through the sql database.  Register_host() updates the database with the current nodes
	///IP information, software version, attached image-capture devices, etc.
	void register_host(ns_image_server_sql * sql, bool overwrite_current_entry=true, bool respect_existing_database_choice=true);
	void update_device_status_in_db(ns_sql & sql) const;
	void unregister_host(ns_image_server_sql * sql);

	///new_software_release_available returns true if the current image server node is running software that is older than the
	///most recent version present in the image server cluster.
	bool new_software_release_available();
	bool new_software_release_available(ns_sql& sql);
	///Returns true if the ini file specified that the current image server node should halt if it detects it is running outdated software
	const bool halt_on_new_software_release(){return _halt_on_new_software_release;}

	std::string default_partition()const{return "partition_000";}
	
	void register_devices(const bool verbose, ns_image_server_sql * sql);

	void set_up_local_buffer();
	void set_up_model_directory();
	///Provides a live sql connection to the central database.  
	ns_sql * new_sql_connection(const std::string & source_file, const unsigned int source_line, const unsigned int retry_count=10) const;
	ns_sql *new_sql_connection_no_lock_or_retry(const std::string & source_file, const unsigned int source_line) const;

	ns_sql_table_lock_manager sql_table_lock_manager;

	ns_local_buffer_connection * new_local_buffer_connection(const std::string & source_file, const unsigned int source_line);
	ns_local_buffer_connection * new_local_buffer_connection_no_lock_or_retry(const std::string & source_file, const unsigned int source_line) const;

	void set_sql_database(const std::string & database_name="",const bool report_to_db=true);
	void reconnect_sql_connection(ns_sql * sql);
	
	void check_for_sql_database_access(ns_image_server_sql * sql) const;

	const std::string &current_sql_database() const{return *sql_database_choice;}

	bool register_and_run_simulated_devices(ns_image_server_sql * sql);

	///shuts down the current image server node.
	void shut_down_host();
	///pauses the current host such that it does not take new jobs
	void pause_host(); 
	void pause_host(ns_image_server_sql * sql) const;


	///returns the id of the current image server node, as specified in the central sql database.
	const ns_64_bit host_id() const{ return _host_id;}

	static ns_movement_event movement_event_used_for_animal_storyboards(){return ns_movement_cessation;}


	///if true, the user (or the image server itself) has requested the image server node stop
	bool exit_requested;

	///if true, the server will attempt to install new version of image server software when available
	bool update_software;

	///(Windows installations only)returns true if the ini specifies the image server node should start minimized.
	bool hide_window(){return _hide_window;}
	///Returns the path to the ini-specified font to be used in visualizations
	const std::string  font_file() const {return NS_DEFAULT_FONT;}
	///Returns the ip address of the current image server node
	const std::string & dispatcher_ip() const{ return host_ip;}
	///Returns the ini-specified port the image server node should listen open to recieve outside commands.
	const unsigned int dispatcher_port() const{ return _dispatcher_port;}
	void increment_dispatcher_port() { _dispatcher_port++;}

	bool allow_multiple_processes_per_system() const { return _allow_multiple_processes_per_system; }

	///Returns the ini-specified name of the subdirectory in which orphaned images are stored
	static std::string orphaned_image_directory() { return "orphaned_images";}

	///Returns the ini-specified name of the subdirectory in which uploaded image masks are stored
	///until they are assigned to a specific capture sample
	static std::string unclaimed_masks_directory() { return "unclaimed_masks";}

	///Returns the ini-specified name of the subdirectory in which uploaded image masks are stored
	///until they are assigned to a specific capture sample
	static std::string masks_directory() {return "masks";}

	///Returns the ini-specified name of the subdirectory in which temporary images (most notably the
	///barcode images collected to deduce scanner identies by assemble_scanner_identities() ) are stored
	static std::string scratch_directory() {return "temp";}


	const std::string & simulated_device_name(){return _simulated_device_name;}

	#ifdef _WIN32 
	void set_console_window_title(const std::string & title="") const;
	#endif

	static std::string miscellaneous_directory() {return "misc";}

	///Returns the ini-specified name of the subdirectory that local image caches use for
	///disk storage (implemented by the ns_image_cache class)
	const std::string & cache_directory() const {return _cache_subdirectory;}

	///Image caches can quickly exceed Windows default virtual memory limits, so we
	///specify the maximum amount of memory to be allocated before swapping out to disk
	static unsigned long maximum_image_cache_memory_size() {return 1024*128; }//128 megabytes
	
	///Returns the ini-specified name of the subdirectory in which SVM model files are stored
	static inline std::string position_analysis_model_directory() { return "models" DIR_CHAR_STR "position_analysis_models";}
	static inline std::string worm_detection_model_directory() { return "models" DIR_CHAR_STR "worm_detection_models";}
	static inline std::string posture_analysis_model_directory() { return "models" DIR_CHAR_STR "posture_analysis_models";}

	///Returns the ini-specified path of the program used to initiate image capture
	///(for example, the path to SANE's scanimage)
	inline const std::string & capture_command() const { return _capture_command;}

	///Returns the ini-specified path of the program used to query the USB interface
	///to collect information about attached devices
	inline const std::string & scanner_list_command() const {return _scanner_list_command;}

	///The maximum amount of time that can be elapsed before a delayed scan is canceled.
	///This cutoff prevents multiple scans to be taken in quick succession, which could
	///heat up the scanner.
	//Returns result in minutes (value in ini file is specified in minutes)
	const unsigned long maximum_allowed_remote_scan_delay(){ return _maximum_allowed_remote_scan_delay;}
	const unsigned long maximum_allowed_local_scan_delay(){ return _maximum_allowed_local_scan_delay;}
	void update_scan_delays_from_db(ns_image_server_sql * con);

	///Returns the interval at which the server should check the database for new processing jobs
	///and new image acquisition jobs
	inline const unsigned int &dispatcher_refresh_interval() const {return _dispatcher_refresh_interval;}

	///Returns the time interval for which the server should wait for a response to the database
	inline const unsigned long &server_timeout_interval() const {return _server_timeout_interval;}


	//void delete_stored_image(const unsigned long image_id, ns_sql & sql);

	///image storage is responsible for serving all file i/o requests, maintaining
	///coherence in the central file server and coordinating network image requests.
	ns_image_storage_handler image_storage;

	ns_image_server_results_storage results_storage;

	///Scans the USB interface to check if any capture devices are attached.  If any are found
	///they are asked to provide their names via capture of their barcodes.
	const inline bool act_as_an_image_capture_server() const {return _act_as_an_image_capture_server;}
	const inline void override_ini_specified_image_capture_server_behavior(const bool act_as_a_capture_server){
		_act_as_an_image_capture_server = act_as_a_capture_server;
	}

	void toggle_central_mysql_server_connection_error_simulation()const;
	

	void calculate_experiment_disk_usage(const ns_64_bit experiment_id,ns_sql & sql) const;
	void clear_old_server_events(ns_sql & sql);

	///returns true if the ini specified that the server automatically look for
	///new processing jobs after the current job is finished.
	///If false, the server has to be actively contacted via
	///a NS_CHECK_FOR_WORK command set to the port the dispatcher is listening on.
	bool run_autonomously() const {return _run_autonomously;}

	///returns true if the ini specified that the server perform
	///processing jobs.
	bool act_as_processing_node() const {return _act_as_processing_node;}

	void set_processing_node_behavior(const bool act_as_processing_node){_act_as_processing_node = act_as_processing_node;}

	///returns a human-readible description of the sql server the image server is using.
	std::string sql_info(ns_sql & sql){
		if (sql_server_addresses.empty()) throw ns_ex("sql_info()::No sql sever specified"); 
		return sql_user + std::string("@") + sql.hostname() + std::string(".") + *sql_database_choice;
	}

	///Scanners identify themselves by scanning a barcode on their surface.
	///The default location of this barcode on the scanner surface is returned.
	const std::string & scanner_list_coord(){return _scanner_list_coord;}

	///Returns true if the an image server is already running on the current machine
	bool server_currently_running(){
		return send_message_to_running_server(NS_NULL);
	}
	///Attempts to contact an image server on the default port, and if successful,
	///sends it the specified message.
	bool send_message_to_running_server(const ns_message_request & req, const std::string & data="",const long port=-1){
		long p(port);
		if(port == -1) p = dispatcher_port();

		ns_socket socket;
		ns_socket_connection con;
		try{
			con = socket.connect(host_ip,p);
			ns_image_server_message message(con);
			message.send_message(req,data);
			con.close();
			return true;
		}
		catch(ns_ex & ex){
			ex.text(); //to remove warnings
			return false;
		}
	};
	void clear(){
		if(cleared)
			return;
		image_registration_profile_cache.clear_cache_without_cleanup();
		image_storage.cache.clear_cache_without_cleanup();
		#ifndef NS_MINIMAL_SERVER_BUILD
		device_manager.clear();
		#endif
		event_log.close();
		cleared = true;
	}
	~ns_image_server(){
		 clear();
	}
	///Returns the ini-specified host name of the current image server.
	const std::string host_name_out() const{
		if (allow_multiple_processes_per_system())
			return host_name + '@' + system_host_name + std::string("-") + ns_to_string(system_parallel_process_id());
		else
			return host_name + '@' + system_host_name;
	}
	///loads quotes from the database to display to the user;
	void load_quotes(std::vector<std::pair<std::string,std::string> > & quotes, ns_sql & con);

	///returns the path where the latest release of the software is stored.
	///used by the automatic update module if installed.
	const std::string latest_release_path() const {return _latest_release_path;}
	///returns the path to the utility that converts xvid m4v files to mp4 files.
	const std::string & video_compiler_filename() const {return _video_compiler_filename;}
	const std::string & video_ppt_compiler_filename() const {return _video_ppt_compiler_filename;}
	const std::string & mail_path() const{return _mail_path;}

	///returns true if the ini file specifies that the current client should compile videos.
	///This is set to false, for example, on linux as MP4Box hasn't been successfully compiled there yet.
	bool compile_videos() const { return _compile_videos;}

	///the path to base directory of the central file server
	std::string long_term_storage_directory;

	//the path to the base directory where analyzed results are stored
	std::string results_storage_directory;

	typedef ns_simple_cache<ns_posture_analysis_model_entry, std::string, true> ns_posture_analysis_model_cache;

#ifndef NS_MINIMAL_SERVER_BUILD
	///Given the name of an SVM machine learning model specification, returns the
	///model file as loaded from the default model directory on the central file server
	void get_worm_detection_model(const std::string & name, ns_worm_detection_model_cache::const_handle_t & handle);
	
	void get_posture_analysis_model_for_region(const ns_64_bit region_info_id, typename ns_posture_analysis_model_cache::const_handle_t & it, ns_sql & sql);
	
	ns_time_path_solver_parameters get_position_analysis_model(const std::string & model_name,bool create_default_if_does_not_exist=false,const ns_64_bit region_info_id_for_default=0, ns_sql * sql_for_default=0) const;
	#endif
	///Clear all SVM machine learning models from the model cache so they are
	///reloaded from the disk
	void clear_model_cache(){
		worm_detection_model_cache.clear_cache_without_cleanup();
#ifndef NS_MINIMAL_SERVER_BUILD
		posture_analysis_model_cache.clear_cache_without_cleanup();
#endif
	}

	 void get_requested_database_from_db();

	///Searches the default SVM machine learning model directory on the central file server and returns
	///a list of all models present there
	void load_all_worm_detection_models(std::vector<ns_worm_detection_model_cache::const_handle_t> & spec);

	ns_64_bit make_record_for_new_sample_mask(const ns_64_bit sample_id, ns_sql & sql);

	std::string capture_preview_parameters(const ns_capture_device::ns_device_preview_type & type,ns_sql & sql);

	std::string video_compilation_parameters(const std::string & input_file, const std::string & output_file, const unsigned long number_of_frames, const std::string& fps, ns_sql & sql)const;

	std::string get_cluster_constant_value(const std::string & key, const std::string & default_value, ns_image_server_sql * sql)const;
	void set_cluster_constant_value(const std::string & key, const std::string & value, ns_image_server_sql * sql, const int time_stamp=-1);
	
	#ifndef NS_MINIMAL_SERVER_BUILD
		void perform_experiment_maintenance(ns_sql & sql) const;
		static void process_experiment_capture_schedule_specification(const std::string & input_file,std::vector<std::string> & warnings,const bool overwrite_previous_experiment=false,const bool submit_to_db=false,const std::string & summary_output_file="",const bool output_to_stdout = false);

	void start_autoscans_for_device(const std::string & device_name,ns_sql & sql);	
	#endif

	bool upgrade_tables(ns_sql & sql,const bool just_test_if_needed,const std::string & schema_name);
	std::ofstream * write_device_configuration_file() const;
	std::ifstream * read_device_configuration_file() const;

	void wait_for_pending_threads();
	void handle_alerts();

	void get_alert_suppression_lists(std::vector<std::string> & devices_to_suppress, std::vector<std::string> & experiments_to_suppress,ns_image_server_sql * sql);

	ns_alert_handler alert_handler;
	ns_lock alert_handler_lock;
	void register_alerts_as_handled();
	unsigned long maximum_number_of_processing_threads() const{ return maximum_number_of_processing_threads_;}
	unsigned handle_software_updates()const{return multiprocess_control_options.handle_software_updates;}

	
	ns_simple_cache<ns_image_fast_registration_profile, ns_64_bit,true> image_registration_profile_cache;
	
	ns_vector_2i max_terminal_window_size;
	unsigned long terminal_hand_annotation_resize_factor;

	unsigned long automated_job_timeout_in_seconds(){ return 15*60;}
	unsigned long automated_job_interval(){ return 30*60;}

	const std::vector<std::string> & allowed_sql_databases() const{
		return possible_sql_databases;
	}

	///updates the database with the specified server event.
	ns_64_bit register_server_event(const ns_image_server_event & s_event, ns_image_server_sql * sql,const bool no_display=false)const;
	ns_64_bit register_server_event(const ns_ex & s_event, ns_image_server_sql * sql)const;
	void register_server_event_no_db(const ns_image_server_event & s_event,bool no_double_endline=false)const;
	
	typedef enum{ns_register_in_local_db, ns_register_in_central_db, ns_register_in_central_db_with_fallback} ns_register_type;

	ns_64_bit register_server_event(const ns_register_type type,const ns_image_server_event & s_event) const;
	ns_64_bit register_server_event(const ns_register_type type,const ns_ex & ex)const;
	void add_subtext_to_current_event(const std::string & str, ns_image_server_sql * sql,bool suppress_display=false, const ns_64_bit impersonate_using_internal_thread_id = 0) const {
		add_subtext_to_current_event(str.c_str(), sql,suppress_display);
	}
	void add_subtext_to_current_event(const char * str, ns_image_server_sql * sql, bool suppress_display=false, const ns_64_bit impersonate_using_internal_thread_id=0) const;
	void add_subtext_to_current_event(const ns_image_server_event & s_event, ns_image_server_sql * sql,bool display_date=true,bool suppress_display=false, const ns_64_bit impersonate_using_internal_thread_id = 0) const;
	void log_current_thread_output_in_separate_file() const;



	ns_os_signal_handler os_signal_handler;
	inline long server_crash_daemon_port() const{return _server_crash_daemon_port;}

	unsigned long local_buffer_commit_frequency_in_seconds(){return 10;}

	//unsigned long processing_node_id() const {
	//	return processing_node_id_;
	//}
	inline bool verbose_debug_output() const {return _verbose_debug_output;}
	inline double & terminal_window_scale_factor() {return _terminal_window_scale_factor;}
	inline const unsigned long & maximum_memory_allocation_in_mb()const{return  _maximum_memory_allocation_in_mb;}

	ns_process_priority process_priority;

	void register_job_duration(const ns_processing_task action, const ns_64_bit microseconds)  {
		ns_acquire_lock_for_scope lock(performance_stats_lock,__FILE__,__LINE__);
		performance_statistics.register_job_duration(action, microseconds); 
		lock.release();
	}
	void register_job_duration(const ns_performance_statistics_analyzer::ns_operation_state state, const ns_64_bit microseconds) {
		ns_acquire_lock_for_scope lock(performance_stats_lock, __FILE__, __LINE__); 
		performance_statistics.register_job_duration(state, microseconds);
		lock.release();
	}
	void update_performance_statistics_to_db(ns_sql & sql) {
		ns_acquire_lock_for_scope lock(performance_stats_lock, __FILE__, __LINE__);
		performance_statistics.update_db(host_id(), sql);
		lock.release();
	}
	void clear_performance_statistics(ns_sql & sql) {
		ns_acquire_lock_for_scope lock(performance_stats_lock, __FILE__, __LINE__);
		performance_statistics.clear_db(host_id(), sql);
		lock.release();
	}

	//ordered by the system thread id;
	mutable std::map<ns_64_bit, ns_thread_output_state> thread_states;
	mutable unsigned long max_internal_thread_id; //all threads get a unique thread id
	mutable unsigned long max_external_thread_id; //threads that need it get a unique id to register output to disk and database
	std::map<ns_64_bit, ns_thread_output_state>::iterator get_current_thread_state_info(const ns_64_bit thread_to_impersonate=0) const;

	ns_64_bit main_thread_id() { return _main_thread_id; }
	void set_main_thread_id();
	std::string get_system_host_name() const {return system_host_name;}
	void set_additional_host_description(const std::string & d) { additional_host_description = d; }
	void set_image_processing_run_limits(const unsigned long maximum_runtime_in_seconds_, const unsigned long maximum_number_of_jobs_to_process_) { 
		maximum_runtime_in_seconds = maximum_runtime_in_seconds_;
		maximum_number_of_jobs_to_process = maximum_number_of_jobs_to_process_;
	}
	void set_resource_limits(const int idle_queue_check_limit, int memory_allocation_limit, int number_of_processing_cores) {
		if (idle_queue_check_limit != -1)
			number_of_times_to_check_empty_job_queue_before_stopping = idle_queue_check_limit;
		if (memory_allocation_limit != -1)
			_maximum_memory_allocation_in_mb = memory_allocation_limit;
		if (number_of_processing_cores != -1)
			maximum_number_of_processing_threads_ = number_of_processing_cores;
	}
	void reset_image_processing_run_data() {
		ns_acquire_lock_for_scope lock(processing_run_counter_lock, __FILE__, __LINE__);
		processing_server_start_time = ns_current_time();
		number_of_processing_jobs_run = 0;
		lock.release();
	}
	unsigned long system_parallel_process_id() const { return _system_parallel_process_id; }
	void increment_system_parallel_process_id() { _system_parallel_process_id++; }
	const std::string & get_additional_host_description() const { return additional_host_description; }
	bool processing_time_is_exceeded() const { return (maximum_runtime_in_seconds> 0) && ((ns_current_time() - processing_server_start_time) >= maximum_runtime_in_seconds); }
	bool number_of_jobs_processed_is_exceeded() const { 
		ns_acquire_lock_for_scope lock(processing_run_counter_lock, __FILE__, __LINE__);
		bool val = (maximum_number_of_jobs_to_process> 0) && (number_of_processing_jobs_run >= maximum_number_of_jobs_to_process); 
		lock.release();
		return val;
	}
	void increment_job_processing_counter(unsigned long i = 1) { 
		ns_acquire_lock_for_scope lock(processing_run_counter_lock, __FILE__, __LINE__); 
		number_of_processing_jobs_run += i;
		lock.release();
	}
	long number_of_remaining_processing_jobs_before_max_is_hit() const {
		ns_acquire_lock_for_scope lock(processing_run_counter_lock, __FILE__, __LINE__);
		long val;
		if (maximum_number_of_jobs_to_process == 0)
			val = LONG_MAX;
		else val = maximum_number_of_jobs_to_process - number_of_processing_jobs_run;
		lock.release();
		return val;
	}
	void reset_empty_job_queue_check_count() { empty_job_queue_check_count = 0; }
	void incremenent_empty_job_queue_check_count() { empty_job_queue_check_count++; }
	bool empty_job_queue_check_count_is_exceeded() { return empty_job_queue_check_count>=number_of_times_to_check_empty_job_queue_before_stopping; }
private:
	ns_64_bit _main_thread_id;
	static void open_log_file(const ns_image_server::ns_image_server_exec_type & exec_type, unsigned long thread_id, const std::string & volatile_directory, const std::string & file_name, std::ofstream & out);
	ns_performance_statistics_analyzer performance_statistics;
	mutable ns_lock performance_stats_lock;

	ns_multiprocess_control_options multiprocess_control_options;
	unsigned long maximum_number_of_processing_threads_;
	bool alert_handler_running;

	unsigned long maximum_runtime_in_seconds, maximum_number_of_jobs_to_process;
	unsigned long processing_server_start_time, number_of_processing_jobs_run;
	unsigned long number_of_times_to_check_empty_job_queue_before_stopping, empty_job_queue_check_count;
	mutable ns_lock processing_run_counter_lock;

	//ns_lock alert_handler_lock;
	ns_single_thread_coordinator alert_handler_thread;
	
	std::string host_name_suffix;
	std::vector<std::string> sql_server_addresses;
	std::vector<std::string> possible_sql_databases;
	std::vector<std::string>::const_iterator sql_database_choice;  ///the name of the database to use on the mysql server

	std::string base_host_name, ///host name (specified in the ini file) of the current host
		host_name,
		system_host_name,		//host name of the computer/system running the software
		additional_host_description, //additional host name information that can be supplied at the commandline (for running in HPC cluster environments)
		host_ip, ///ip address of the current host
		sql_user, ///username with which to connect to the sql server
		sql_pwd;  ///password with which to connect to the sql server

	//if multiple processes are running on the system, this is unique for each one.
	unsigned long _system_parallel_process_id;

	std::string local_buffer_ip, ///ip address of the current host
				local_buffer_user, ///username with which to connect to the sql server
				local_buffer_pwd,  ///password with which to connect to the sql server
				local_buffer_db;

	bool _verbose_debug_output;
	double _terminal_window_scale_factor;

	std::string _capture_command,  ///path of the command used to initiate image capture.

		volatile_storage_directory,
		_log_filename, ///filename of the local log-file used to document errors and activity.
		_scanner_list_command,
		_scanner_list_coord,
		_video_compiler_filename,
		_video_ppt_compiler_filename,
		_latest_release_path,
		_simulated_device_name,
		_mail_path,
		_ethernet_interface,
		_cache_subdirectory;
	bool _allow_multiple_processes_per_system;

	unsigned long _maximum_memory_allocation_in_mb; 

	unsigned long next_scan_for_problems_time;
	unsigned int _dispatcher_port;
	unsigned int _server_crash_daemon_port;
	ns_64_bit _host_id; ///database id of the current host
	unsigned int _dispatcher_refresh_interval; ///the period at which the dispatcher checks the db for work, in miliseconds.
	unsigned long _server_timeout_interval; ///the period after which servers are considered offline (in seconds)

	//in minutes
	unsigned long _maximum_allowed_remote_scan_delay; ///the maximum amount a scan can be delayed before it is considered missed and discarded.
	unsigned long _maximum_allowed_local_scan_delay; ///the maximum amount a scan can be delayed before it is considered missed and discarded.

	bool _run_autonomously;
	bool _hide_window;
	//if true, each scanner is asked to provide its name, rather than taking the values in the ini file for granted.
	bool _act_as_an_image_capture_server,
		 _act_as_processing_node,
		 _halt_on_new_software_release;
	bool _compile_videos;
	bool server_pause_status;



	///std::ostream used to write to the event log
	mutable std::ofstream event_log;
	std::string _font_file;

#ifndef NS_MINIMAL_SERVER_BUILD
	///memory cache of all used SVM models
	ns_worm_detection_model_cache worm_detection_model_cache;
	
	ns_posture_analysis_model_cache posture_analysis_model_cache;
#endif


	///registers the specified device in the central database.
	static void register_device(const ns_device_summary & device, ns_image_server_sql * con);

	///used to prevent multiple threads from simultaneously registering server events in the mysql database
	mutable ns_lock server_event_lock;
	///used to coordinate multiple threads' communication with the database.
	mutable ns_lock sql_lock;

	mutable ns_lock local_buffer_sql_lock;
	std::vector<ns_thread_output_state> thread_output_states;

	///used to coordinate the simulator scan (to run test scans without accessing hardware) disk access
	ns_lock simulator_scan_lock;

	


	unsigned int _software_version_compile;
	
	#ifndef NS_MINIMAL_SERVER_BUILD
	mutable ns_image_server_automated_job_scheduler automated_job_scheduler;
	#endif
	
	bool cleared;
};

///All behavior is coordinated through a global instance of the image server.
///This object needs to be defined somewhere in the execution unit.
extern ns_image_server image_server;
extern const ns_image_server & image_server_const;


///void ns_image_server_delete_image(const unsigned long image_id, ns_sql & sql);

//std::string ns_directory_from_image_info(const long experiment_id, const long sample_id, const std::string &experiment_name, const std::string &sample_name, ns_sql & sql);

///if desired_string != "", then it is appended to the destination std::string.
///if desired_string == "", then fall_back_prefix and fallback_int are appended to the destination std::string.
void add_string_or_number(std::string & destination, const std::string & desired_string, const std::string & fallback_prefix, const ns_64_bit fallback_int);

#ifdef _WIN32 
void ns_update_software(const bool just_launch=false);
#endif

void ns_wrap_m4v_stream(const std::string & m4v_filename, const std::string & output_basename);

#endif
