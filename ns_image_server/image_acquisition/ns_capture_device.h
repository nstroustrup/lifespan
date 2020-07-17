#ifndef NS_CAPTURE_DEVICE_H
#define NS_CAPTURE_DEVICE_H

#include "ns_thread.h"
#include "ns_image_server_images.h"
#include "ns_high_precision_timer.h"
#include <fstream>
#include <string>

class ns_image_server_dispatcher;
class ns_capture_device;

extern ns_lock ns_global_usb_lock;
#define ns_use_global_usb_lock true

#define ns_regulate_scanner_speed false

struct ns_image_capture_device_speed_regulator_byte_record{
	ns_image_capture_device_speed_regulator_byte_record(){}
	ns_image_capture_device_speed_regulator_byte_record(const unsigned long bytes, const unsigned long t):cumulative_bytes(bytes),time(t){}
	unsigned long cumulative_bytes,
			time;
};
class ns_image_capture_device_speed_regulator{
public:
	ns_image_capture_device_speed_regulator():decile_times(10,0), total_number_of_expected_bytes(0), desired_scan_duration(0),
		bytes_read_so_far(0), pending_delay(0), previous_pending_delay(0), delay_time_injected(0), packets_received(0) {}
	void initialize_for_capture(const unsigned long total_number_of_expected_bytes_, 
								const unsigned long desired_scan_duration_);  //in seconds
	void register_data_as_received(unsigned long bytes);
	void register_start();
	void register_stop();
	ns_64_bit run_delay_if_necessary();  //returns the delay time in milliseconds

	std::vector<ns_64_bit> decile_times;

	unsigned long total_bytes_read()const{return bytes_read_so_far;}
	std::vector<std::string> warnings;
private:
	std::vector<ns_image_capture_device_speed_regulator_byte_record> byte_record;
	ns_64_bit start_time;		  //in milliseconds

	unsigned long total_number_of_expected_bytes, 
					desired_scan_duration;		//in seconds

	unsigned long bytes_read_so_far;

	ns_64_bit pending_delay;	//in milliseconds
	ns_64_bit previous_pending_delay;
	ns_64_bit delay_time_injected;	//in milliseconds

	
	ns_high_precision_timer scan_duration;

	unsigned long packets_received;
};

struct ns_image_capture_specification{
	ns_image_capture_specification():turn_off_lamp_after_capture(false),
									volatile_storage(0),
									time_at_imaging_start(0),
									time_at_imaging_stop(0),
									time_spent_reading_from_device(0),
									time_spent_writing_to_disk(0),
									total_time_during_read(0){}
	//information about the image being captured
	ns_image_server_captured_image image;
	std::ofstream * volatile_storage;
	ns_64_bit capture_schedule_entry_id;
	//parameterse to send to the capture device
	std::string capture_parameters;
	bool turn_off_lamp_after_capture;
	unsigned long time_at_imaging_start,
				  time_at_imaging_stop;
	
	ns_64_bit time_spent_reading_from_device;			//in microseconds
	ns_64_bit time_spent_writing_to_disk;				//in microseconds
	ns_64_bit total_time_during_read;					//in microseconds
	ns_64_bit total_time_spent_during_programmed_delay; //in milliseconds!

	static bool capture_parameters_specifiy_16_bit(const std::string & );
	ns_image_capture_device_speed_regulator speed_regulator;
};
class ns_buffered_capture_scheduler;

//structure to pass info between main thread and device capture thread
struct ns_capture_thread_arguments{
	ns_capture_thread_arguments():buffered_capture_scheduler(0){}
	//pointer back to the dispatcher so thread can mark itself finished via thte capture_thread pointer.
//	ns_image_server_dispatcher * calling_dispatcher;
	ns_buffered_capture_scheduler * buffered_capture_scheduler;
	std::string device_name;
	ns_image_capture_specification capture_specification;
};

struct ns_device_summary{
	ns_device_summary(){}
	ns_device_summary(const std::string & name_, const bool sim, const bool unknown_id,
					  const bool paused_,const bool currently_scanning_,const unsigned long last_capture_start_time_,
					  const unsigned long autoscan_interval_, const unsigned long last_autoscan_time_, const unsigned long next_autoscan_time_, bool preview_capture_requested_):name(name_),simulated_device(sim),
					  unknown_identity(unknown_id),paused(paused_),currently_scanning(currently_scanning_),
					  last_capture_start_time(last_capture_start_time_),autoscan_interval(autoscan_interval_),last_autoscan_time(last_autoscan_time_),next_autoscan_time(next_autoscan_time_), preview_capture_requested(preview_capture_requested_){}
	std::string name;
	bool simulated_device;
	bool unknown_identity,
		paused,
		currently_scanning;
	unsigned long last_capture_start_time,
		autoscan_interval,
		last_autoscan_time,
		next_autoscan_time;
	long preview_capture_requested;
};

struct ns_device_hardware_info {
	std::string vendor, product, address;
};
struct ns_device_hardware_location{
	int bus,device_id;
};

class ns_capture_device{
public:
	
	typedef enum{ns_none,ns_unknown_identity=1,ns_simulated_device=2} ns_device_flag;
	typedef enum {ns_no_preview=0,ns_transparency_preview=1,ns_reflective_preview=2} ns_device_preview_type;


	ns_capture_device():delayed_exception(0),preview_request(ns_no_preview),error_state_has_been_recognized(false),device_flags(ns_none),paused(false){}
	ns_capture_device(const std::string & name_,const std::string & hardware_alias_, const ns_device_flag flags_):name(name_),hardware_alias(hardware_alias_),device_flags(flags_),
						delayed_exception(0),preview_request(ns_no_preview),error_state_has_been_recognized(false),paused(false){}


	std::string name,
			hardware_alias;
	//parses the hardware alias to find the usb bus and device id.
	ns_device_hardware_location hardware_location() const;
	bool send_hardware_reset() const;
	
	int paused;

	ns_device_flag device_flags;

	void set_default_unidentified_name(){name = std::string("Unidentified::") + hardware_alias;}

	std::string last_capture_failure_text;
	bool is_in_error_state() const {return last_capture_failure_text.size() != 0;}
	bool error_state_has_been_recognized;

	void request_preview_scan(ns_device_preview_type type){preview_request = type;}

	bool unknown_identity() const {return (device_flags&ns_unknown_identity)!=0;}
	bool is_simulated_device() const {return (device_flags&ns_simulated_device)!=0;}


	#ifndef NS_MINIMAL_SERVER_BUILD
	void capture(ns_image_capture_specification & c);
	void turn_off_lamp();
	#endif

	void throw_delayed_exception(const ns_ex & ex){
		if (delayed_exception != 0)
			delete delayed_exception;
		delayed_exception = new ns_ex();
		*delayed_exception = ex;
	}
	void handle_delayed_exception(){
		if (delayed_exception != 0)
			throw *delayed_exception;
	}
	~ns_capture_device(){
		
		if (delayed_exception != 0){
			delete delayed_exception;
			delayed_exception = 0;
		}

	}
private:
	ns_ex * delayed_exception;
		
	ns_device_preview_type preview_request;
};


void ns_get_scanner_hardware_address_list(std::vector<ns_device_hardware_info> & scaner_names);

#endif
