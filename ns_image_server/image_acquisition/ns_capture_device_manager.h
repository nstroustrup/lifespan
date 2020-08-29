#ifndef NS_CAPTURE_DEVICE_MANAGER
#define NS_CAPTURE_DEVICE_MANAGER

#include "ns_capture_device.h"
#include "ns_thread.h"
#include <iostream>

class ns_image_server_device_manager;
struct ns_image_server_device_manager_aysnch_argument{
	ns_image_server_device_manager * manager;
	ns_capture_thread_arguments * capture_arguments;
};
struct ns_image_server_device_manager_device{

	ns_image_server_device_manager_device():currently_scanning(false),pending_deletion(false),
											last_capture_start_time(0),capture_thread_needs_cleanup(false),
											autoscan_interval(0),last_autoscan_time(0),autoscan_schedule_clash_detected(false){}
	ns_image_server_device_manager_device(const std::string & name,const std::string & hardware_address, const ns_capture_device::ns_device_flag flags):
		device(name,hardware_address,flags),currently_scanning(false),
			pending_deletion(false),last_capture_start_time(0),capture_thread_needs_cleanup(false),
			autoscan_interval(0),last_autoscan_time(0),next_autoscan_time(0),autoscan_schedule_clash_detected(false){}

	ns_capture_device device;
	bool currently_scanning;
	unsigned long last_capture_start_time;

	unsigned long autoscan_interval;
	unsigned long last_autoscan_time;
	unsigned long next_autoscan_time;
	bool autoscan_schedule_clash_detected;

	ns_thread capture_thread;
	bool pending_deletion;
	bool capture_thread_needs_cleanup;
	~ns_image_server_device_manager_device(){if(capture_thread_needs_cleanup){capture_thread.detach();capture_thread_needs_cleanup=false;}}
};


class ns_image_server_device_manager{
public:



	ns_image_server_device_manager():device_list_access_lock("ns_isdm::device"),usb_access_lock("ns_isdm::usb"),hotplug_running(false){}
	void clear();
	void wait_for_all_scans_to_complete();
	bool device_is_currently_scanning(const std::string & name) const;
	bool device_is_in_error_state(const std::string & name, std::string & error) const;
	bool device_is_in_error_state(const std::string & name)const;

	//ns_capture_device::ns_device_preview_type preview_requested_on_device();
	//void request_preview(ns_capture_device::ns_device_preview_type & type, const std::string & name);
	void run_capture_on_device(ns_capture_thread_arguments * args);

	void output_connected_devices(std::ostream & o);

	void scan_and_report_problems(const std::vector<std::string> & devices_to_suppress, const std::vector<std::string> & experiments_to_suppress,ns_sql & sql);

	void attach_simulated_device(const std::string & device_name);

	void run_pending_autoscans(ns_image_server_dispatcher * d, ns_sql & sql);

	///assemble_scanner_identities querries the USB interface to find any attached SANE scanner devices.
	void clear_device_list_and_identify_all_hardware();

	void mark_device_as_finished_scanning(const std::string & device_name);

	void register_autoscan_clash(const std::string & device_name, const bool clash=true);

	void reset_all_devices();

	void test_balancing_();

	bool set_pause_state(const std::string & device_name, const int state);
	
	bool set_autoscan_interval(const std::string & device_name, const int interval_in_seconds,const unsigned long start_time=0);
	
	bool set_autoscan_interval_and_balance(const std::string & device_name, const int interval_in_seconds,ns_sql_connection * sql);

	///hotplug_new_devices() checks the USB interface to see if any changes have occured since it was last checked,
	///and updates the device registry accordingly.

	bool hotplug_new_devices(const bool rescan_bad_barcodes,const bool verbose,const bool hotplug_due_to_confusion);
	///Scanners can report back their human-readable names via a bar-code placed on their surface.  add_barcoded_hardware handles
	///this identification process, requesting the specified hardware addresses to read their barcode, interpreting it and updating
	///the device registry accordingly

	void identify_new_devices(std::vector<ns_image_server_device_manager_device *> & devices_to_identify);
	///Updates the cetnral SQL database to reflect currently attached devices

	void save_last_known_device_configuration();
	bool load_last_known_device_configuration();

	
	typedef std::vector<ns_device_summary> ns_device_name_list;
	void request_device_list(ns_device_name_list & list) const;
	unsigned long number_of_attached_devices() const;
	~ns_image_server_device_manager();
	
	typedef std::map<std::string,ns_image_server_device_manager_device *> ns_device_list;

	
	void pause_to_keep_usb_sane();
private: 
	void add_device(const std::string & name, const std::string & hardware_address, const ns_capture_device::ns_device_flag device=ns_capture_device::ns_none);
	void remove_device(const std::string & name);

	ns_thread_return_type static run_capture_on_device_internal(void *);
	void run_capture_on_device_asynch(ns_capture_thread_arguments & arg);

	//to allow hot-swapping, all reads or writes to the list of devices attached must first acquire the device_list_access_lock.
	mutable ns_lock device_list_access_lock;
	mutable ns_lock usb_access_lock;

	bool hotplug_running;

	ns_device_list devices; //all image capture devices connected to the host
	std::vector<ns_image_server_device_manager_device *> devices_pending_deletion; //all image capture devices connected to the host
	std::vector<ns_image_server_device_manager_device *> deleted_devices; //all image capture devices that were deleted from host
	static std::string autoscan_parameters(ns_sql & sql);
	unsigned long number_of_confused_hotplugs;
};

#endif
