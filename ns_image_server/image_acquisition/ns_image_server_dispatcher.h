#ifndef NS_IMAGE_SERVER_DISPATCHER
#define NS_IMAGE_SERVER_DISPATCHER

#include "ns_socket.h"
#include "ns_thread.h"
#include "ns_image_server_message.h"
#include "ns_sql.h"
#include "ns_image_server_images.h"
#include "ns_processing_job_scheduler.h"
#include "ns_processing_job_push_scheduler.h"
#include "ns_image_capture_data_manager.h"
#include "ns_thread_pool.h"
#include <string>
#include <vector>
#include <list>

#include "ns_buffered_capture_scheduler.h"

#define dispatcher_socket_queue_length 2048

struct ns_remote_dispatcher_request{
	ns_remote_dispatcher_request(ns_socket_connection & c):connection(c),message(c){}
	ns_image_server_message message;
	ns_socket_connection connection;
};

struct ns_processig_thread_pool_status_info {
	struct ns_processig_thread_pool_status_info():lock("ptpsi"), number_of_multi_threaded_jobs_running(0){}
	unsigned long number_of_multi_threaded_jobs_running;
	ns_lock lock;
};
struct ns_dispatcher_job_pool_external_data {
	ns_dispatcher_job_pool_external_data():image_server(0){}

	const ns_image_server * image_server;

	ns_processig_thread_pool_status_info status_info;
};

struct ns_dispatcher_job_pool_persistant_data {
	ns_dispatcher_job_pool_persistant_data() :thread_id(0),sql(0), job_scheduler(0){}
	ns_64_bit thread_id;
	ns_sql * sql;
	ns_processing_job_scheduler * job_scheduler;
};
class ns_dispatcher_job_pool_job {
public:
	ns_processing_job job;
	ns_dispatcher_job_pool_job(const ns_processing_job & j, ns_dispatcher_job_pool_external_data & d) :job(j), external_data(&d){}
	ns_dispatcher_job_pool_external_data * external_data;
	void operator()(ns_dispatcher_job_pool_persistant_data & persistant_data);
};


class ns_image_server_dispatcher;

struct ns_image_reciever_spec{
	ns_image_reciever_spec(ns_image_server_message & m, ns_socket_connection & s,  ns_image_server_dispatcher * d):message(m),socket_connection(s),dispatcher(d){}
	ns_image_server_message message;
	ns_image_server_dispatcher * dispatcher;

	ns_socket_connection socket_connection;
};

class ns_image_server_dispatcher{
	friend class ns_capture_device;
public:
	//allow_processing: whether the dispatcher should run imaging processing jobs
	//note that initial compression of raw tifs is always performed regardless of allow_processing value.
	ns_image_server_dispatcher(const bool _allow_processing):
	  allow_processing(_allow_processing),delayed_exception(0),processing_lock("ns_isd::processing"),first_device_capture_run(true),
		  message_handling_lock("ns_isd::message"),memory_allocation_error_count(0),clean_clear_local_db_requested(false),
		  hotplug_lock("ns_isd::hotplug"),device_capture_management_lock("ns_dml"),time_of_last_scan_for_problems(0),hotplug_running(false),work_sql_connection(0),currently_unable_to_connect_to_the_central_db(false),actively_avoid_connecting_to_central_db(false),timer_sql_connection(0),work_sql_management_lock("ns_isd::work_sql"),timer_sql_management_lock("ns_isd::timer_sql"),trigger_segfault(false){}

	void init(const unsigned int port,const unsigned int socket_queue_length);
	void register_device(const ns_device_name &device);

	void run();
	void handle_remote_requests();

	void connect_timer_sql_connection();

	//the server is regularly poked; when a NS_CHECK_FOR_NEW_WORK command is received this function is run (on_timer()) is called also
	void start_looking_for_new_work();

	//The server is regularly poked; each time a NS_TIMER command is received on_timer() is run
	void on_timer();

	void run_device_capture_management();

	//returns true if a capture was started
	bool run_pending_captures_on_device(const std::string &device_name, ns_sql & sql);

	//writes a preview of the entire device FOV (used to determine sample dimentions when setting up an experiment)
	bool get_whole_scanner_preview(const ns_capture_device::ns_device_preview_type preview_type,const std::string & device_name);

	void process_image_request(const unsigned int image_id, const std::string & host_ip, const int host_port);

	void recieve_image(ns_image_server_message & message,ns_socket_connection & con);

	//returns true if work was found
	bool  look_for_work();

	static ns_thread_return_type thread_start_look_for_work(void * dispatcher_pointer);

	static ns_thread_return_type thread_start_recieving_image(void * recieve_info);

	void throw_delayed_exception(const ns_ex & ex){
		if (delayed_exception != 0)
			delete delayed_exception;
		delayed_exception = new ns_ex();
		*delayed_exception = ex;
	}
	void handle_delayed_exception();
	~ns_image_server_dispatcher();
	void clear_for_termination();

	ns_buffered_capture_scheduler buffered_capture_scheduler;
	void trigger_segfault_on_next_timer(){
		trigger_segfault = true;
	}
	
	ns_single_thread_coordinator schedule_error_check_thread;
	unsigned long time_of_last_scan_for_problems;

private:
	bool trigger_segfault;
	void handle_central_connection_error(ns_ex & ex);	
	void recieve_image_thread(ns_image_server_message & message);
	void scan_for_problems(ns_sql & sql);
	void handle_memory_allocation_error();
	void register_succesful_operation();
	unsigned long memory_allocation_error_count;
	bool allow_processing;

	bool clean_clear_local_db_requested;
	
	ns_thread_pool<ns_dispatcher_job_pool_job, ns_dispatcher_job_pool_persistant_data> processing_thread_pool;
	ns_dispatcher_job_pool_external_data processing_pool_external_data;

	ns_single_thread_coordinator processing_job_scheduler_thread;
	ns_single_thread_coordinator message_handling_thread;

	ns_lock message_handling_lock;
	ns_lock device_capture_management_lock;

	ns_lock work_sql_management_lock;
	ns_lock timer_sql_management_lock;
	std::list<ns_remote_dispatcher_request> pending_remote_requests;

	bool currently_unable_to_connect_to_the_central_db;
	bool actively_avoid_connecting_to_central_db;
	bool first_device_capture_run;

	ns_socket incomming_socket;

	ns_ex * delayed_exception;

	void wait_for_local_jobs();
	void run_hotplug(const bool rescan_bad_barcodes=true,const bool verbose=true);


	ns_lock processing_lock;
	ns_lock hotplug_lock;
	bool hotplug_running;

	unsigned int random_number_seed;

	ns_sql * work_sql_connection,
		   * timer_sql_connection;
};



#endif
