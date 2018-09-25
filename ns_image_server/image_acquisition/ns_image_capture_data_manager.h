#ifndef NS_CAPTURE_DATA_MANAGER_H
#define NS_CAPTURE_DATA_MANAGER_H
#include "ns_image_storage_handler.h"
#include "ns_capture_device.h"
#include "ns_single_thread_coordinator.h"

struct ns_transfer_status{
	ns_transfer_status():time(0){}
	ns_transfer_status(const std::string & s):time(ns_current_time()),status(s){}
	std::string status;
	unsigned long time;
};
struct ns_transfer_status_debugger{
	ns_transfer_status_debugger():lock("tsd_lock"){}
	void set_status(const std::string & device,const ns_transfer_status & s){
		lock.wait_to_acquire(__FILE__,__LINE__);
		status[device][s.status] = s;
		lock.release();
	}
	void print_status(){
		lock.wait_to_acquire(__FILE__,__LINE__);
		std::string res;
		res = "Local Image Buffer Status:\n";
		for (ns_status_list::const_iterator p = status.begin(); p!=status.end(); p++){
			res = res + p->first + ":" + "\n";
			for (ns_status_type_list::const_iterator q = p->second.begin(); q != p->second.end();  q++)
				res = res + "\t" + ns_format_time_string(q->second.time) + " : " + q->second.status + "\n";
		}
		lock.release();
		try{
			ns_image_handler_register_server_event_to_central_db(ns_ex(res));
		}
		catch(...){
			std::cout << res;
		}
	}
private:
	ns_lock lock;
	typedef std::map<std::string,ns_transfer_status> ns_status_type_list;
	typedef std::map<std::string,ns_status_type_list> ns_status_list;
	ns_status_list status;
};


class ns_image_capture_data_manager{
public:
	typedef enum{ns_not_finished,ns_on_local_server_in_16bit,ns_on_local_server_in_8bit,ns_transferred_to_long_term_storage,ns_fatal_problem} ns_capture_image_status;
	typedef enum { ns_try_to_transfer_to_long_term_storage, ns_convert_and_compress_locally } ns_transfer_behavior;

	ns_image_capture_data_manager(ns_image_storage_handler & storage_handler_):check_sql_lock("icdm::sql"),check_sql(0),device_transfer_state_lock("icdm::dev"),storage_handler(&storage_handler_),pending_transfers_lock("ns_icd::transfer"){}
	
	void initialize_capture_start(ns_image_capture_specification & capture_specification, ns_local_buffer_connection & local_buffer_sql);

	void register_capture_stop(ns_image_capture_specification & capture_specification, const ns_64_bit problem_id, ns_local_buffer_connection & local_buffer_sql);
	
	void transfer_image_to_long_term_storage(const std::string & device_name,ns_64_bit capture_schedule_entry_id,ns_image_server_captured_image & image, ns_transfer_behavior & behavior, ns_local_buffer_connection & sql);

	bool handle_pending_transfers_to_long_term_storage(const std::vector<std::string> & device_names);
	
	bool handle_pending_transfers_to_long_term_storage_using_db_names();

	void wait_for_transfer_finish();
	~ns_image_capture_data_manager();
	ns_transfer_status_debugger transfer_status_debugger;
	
private:

	ns_local_buffer_connection * check_sql;
	bool transfer_in_progress_for_device(const std::string & device);

	bool transfer_image_to_long_term_storage_locked(ns_64_bit capture_schedule_entry_id, ns_image_server_captured_image & image, ns_transfer_behavior & behavior,ns_local_buffer_connection & sql);

	static ns_thread_return_type thread_start_handle_pending_transfers_to_long_term_storage(void * thread_arguments);

	unsigned long handle_pending_transfers(const std::string & device_name, ns_transfer_behavior & behavior);

	bool transfer_data_to_long_term_storage(ns_image_server_captured_image & image,
									unsigned long long & time_during_transfer_to_long_term_storage,
									unsigned long long & time_during_deletion_from_local_storage, 
									const ns_vector_2<ns_16_bit> & conversion_16_bit_bounds,
									ns_transfer_behavior & behavior,
									ns_local_buffer_connection & sql);

	ns_single_thread_coordinator pending_transfers_thread;
	ns_lock pending_transfers_lock;
	ns_image_storage_handler * storage_handler;
	bool device_transfer_in_progress(const std::string & name);
	void set_device_transfer_state(const bool state,const std::string & name);
	std::map<std::string, bool> device_transfer_state;
	ns_lock device_transfer_state_lock;
	ns_lock check_sql_lock;
};
#endif
