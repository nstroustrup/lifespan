#include "ns_capture_device_manager.h"
#include "ns_image_server.h"
#include "ns_image_server_dispatcher.h"
#include "ns_barcode.h"
using namespace std;
#include <iostream>


string ns_system_boot_time_str(){
	ns_external_execute exec;
	ns_external_execute_options opt;
	opt.binary = false;
	opt.discard_stderr = true;
	opt.discard_stdin = true;
	opt.discard_stdout = false;
	opt.take_stderr_handle = true;
	opt.take_stdin_handle = true;
	opt.take_stdout_handle = true;
	string time_command,arg;
	#ifdef _WIN32 
		time_command = "net";
		arg = "statistics workstation";
	#else
	time_command = "/usr/bin/who";
		arg = "-b";
	#endif
	exec.run(time_command,arg,opt);
	char buf[1024];
	string res;
	unsigned l(0);
	while(true){
		l = exec.read_stdout(buf,1024);
		if (l != 0){
			for (unsigned int i = 0; i < l; i++){
				res+=buf[i];
			}
		}
		if (l == 0)
			break;
	}

	string::size_type start = 0;
	#ifdef _WIN32 
	start = res.find("Statistics since");
	if (start == res.npos)
		throw ns_ex("ns_system_boot_time_str::Could not parse boot time statistics.");
	start += 16;
	#endif
	string res2(res);
	res.resize(0);
	for (std::string::size_type i = start; i < res2.size(); i++){
		if (res2[i]=='\n')
			break;
		res+=res2[i];
	}
	return res;
};

void ns_image_server_device_manager::test_balancing_(){
	clear();
	attach_simulated_device("device_1");
	attach_simulated_device("device_2");
	attach_simulated_device("device_3");
	attach_simulated_device("device_4");
	ns_acquire_for_scope<ns_sql > sql(image_server.new_sql_connection(__FILE__,__LINE__));
	unsigned long time_offset = ns_current_time();
	set_autoscan_interval_and_balance("device_1",20*60,&sql());
	cerr << "Balancing 1\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time - time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}

	set_autoscan_interval_and_balance("device_2",20*60,&sql());
	cerr << "Balancing 2\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time - time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}
	set_autoscan_interval_and_balance("device_3",20*60,&sql());
	cerr << "Balancing 3\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time - time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}
	set_autoscan_interval_and_balance("device_4",20*60,&sql());
	cerr << "Balancing 4\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time- time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}
	set_autoscan_interval_and_balance("device_3",0,&sql());
	cerr << "Removing 3\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time- time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}	

	set_autoscan_interval_and_balance("device_1",0,&sql());
	cerr << "Removing 1\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time- time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}	
	set_autoscan_interval_and_balance("device_2",0,&sql());
	cerr << "Removing 2\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time- time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}	
	set_autoscan_interval_and_balance("device_4",0,&sql());
	cerr << "Removing 4\n";
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); p++){
		if (p->second->autoscan_interval != 0) cerr << p->first << ":" << (p->second->next_autoscan_time- time_offset)/60  << " : " << p->second->autoscan_interval << "\n";
	}
	sql.release();
}

ns_image_server_device_manager::~ns_image_server_device_manager(){
	
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	for (unsigned int i = 0; i < devices_pending_deletion.size(); i++){
		delete devices_pending_deletion[i];
	}
	devices_pending_deletion.clear(); 
	for (unsigned int i = 0; i < deleted_devices.size(); i++) {
		delete deleted_devices[i];
	}
	deleted_devices.clear();
	lock.release();
}

unsigned long ns_image_server_device_manager::number_of_attached_devices() const{
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	unsigned long ret((unsigned long)devices.size());
	lock.release();
	return ret;
}
void ns_image_server_device_manager::request_device_list(ns_device_name_list & list) const{
	list.resize(0);

	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

	for (ns_device_list::const_iterator p = devices.begin(); p != devices.end(); ++p)
		list.push_back(ns_device_summary(p->first,p->second->device.is_simulated_device(),
		p->second->device.unknown_identity(),p->second->device.paused > 0,p->second->currently_scanning,p->second->last_capture_start_time, 
		p->second->autoscan_interval,p->second->last_autoscan_time, p->second->next_autoscan_time));
	lock.release();
}

bool ns_image_server_device_manager::device_is_in_error_state(const std::string & name)const{
	std::string tmp;
	return device_is_in_error_state(name,tmp);
}

void ns_image_server_device_manager::register_autoscan_clash(const std::string & device_name, const bool clash){
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

		ns_device_list::const_iterator p = devices.find(device_name);
		if (!(p== devices.end())){
				p->second->autoscan_schedule_clash_detected = clash;
				lock.release();
				return;
		}
		for (unsigned int i = 0; i < devices_pending_deletion.size(); i++){
			if (devices_pending_deletion[i]->device.name == device_name){
				devices_pending_deletion[i]->autoscan_schedule_clash_detected = clash;
				lock.release();
				return;
			}
		}
		throw ns_ex("ns_image_server_device_manager::mark_device_as_finished_scanning()::Unknown device name:") << device_name;
}

void ns_image_server_device_manager::mark_device_as_finished_scanning(const std::string & device_name){
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

		ns_device_list::const_iterator p = devices.find(device_name);
		if (!(p== devices.end())){
			if (p->second->capture_thread_needs_cleanup){
					p->second->capture_thread.detach();
					p->second->capture_thread_needs_cleanup = false;
					p->second->currently_scanning = false;
					p->second->last_capture_start_time = 0;
			}
			else{
				if (p->second->currently_scanning){
					cerr << "Encountered a running thread that did not nead its thread cleaned!\n";
				}
				else{
					cerr << "Attempting to mark a device as finished when is not currently running\n";
				}
			}
				lock.release();
				return;
		}
		
		for (unsigned int i = 0; i < devices_pending_deletion.size(); i++){
			if (devices_pending_deletion[i]->device.name == device_name){
				devices_pending_deletion[i]->capture_thread.detach();
				devices_pending_deletion[i]->capture_thread_needs_cleanup = false;
				devices_pending_deletion[i]->currently_scanning = false;
				devices_pending_deletion[i]->last_capture_start_time = 0;
				lock.release();
				return;
			}
		}
		throw ns_ex("ns_image_server_device_manager::mark_device_as_finished_scanning()::Unknown device name:") << device_name;
}

class ns_start_time_comparer{
public:
	bool operator()(const ns_image_server_device_manager::ns_device_list::iterator & a, const ns_image_server_device_manager::ns_device_list::iterator & b) const{
		return a->second->next_autoscan_time < b->second->next_autoscan_time;
	};
};
bool ns_image_server_device_manager::set_autoscan_interval_and_balance(const std::string & device_name, int interval_in_seconds,ns_sql_connection * sql){

	//we want to balance scanners within the same incubator, so we need to get all the assignments
	std::map<std::string,std::string> incubator_assignments;
	*sql << "SELECT device_name, incubator_name FROM device_inventory";
	ns_sql_result res;
	sql->get_rows(res);
	for (unsigned int i = 0; i < res.size(); ++i)
		incubator_assignments[res[i][0]] = res[i][1];


	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

	
	ns_device_list::iterator device_to_update(devices.find(device_name));
	if (device_to_update == devices.end())
		throw ns_ex("ns_image_server_device_manager::set_autoscan_interval_and_balance()::Could not find device ") << device_name;
	if (device_to_update->second->autoscan_interval == interval_in_seconds){
		lock.release();
		return false;
	}


	
	unsigned long update_interval(interval_in_seconds);
	if (interval_in_seconds == 0)
		update_interval = device_to_update->second->autoscan_interval;

	const std::string incubator(incubator_assignments[device_name]);

	//find all devices that need to be re-balanced
	vector<ns_device_list::iterator> devices_to_balance;
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); ++p){
		if (p == device_to_update || 
			incubator_assignments[p->first] != incubator ||
			p->second->autoscan_interval != update_interval) continue;

			devices_to_balance.push_back(p);
		
	}

	unsigned long current_time(ns_current_time());
	if (devices_to_balance.size() == 0){

		device_to_update->second->autoscan_interval = interval_in_seconds;
		if (interval_in_seconds == 0){
			device_to_update->second->next_autoscan_time = 0;
			return true;
		}

		if (device_to_update->second->next_autoscan_time == 0){
			device_to_update->second->next_autoscan_time = device_to_update->second->last_capture_start_time + interval_in_seconds;
			if (device_to_update->second->next_autoscan_time < current_time)
				device_to_update->second->next_autoscan_time = current_time;
		}

		lock.release();
		return true;
	}

	

	if (devices_to_balance.size() == 1){
		device_to_update->second->autoscan_interval = interval_in_seconds;
		if (interval_in_seconds == 0){
			device_to_update->second->next_autoscan_time = 0;
			return true;
		}

		if (devices_to_balance[0]->second->next_autoscan_time == 0)
			devices_to_balance[0]->second->next_autoscan_time = devices_to_balance[0]->second->last_capture_start_time+interval_in_seconds;

		if (devices_to_balance[0]->second->next_autoscan_time < current_time)
				devices_to_balance[0]->second->next_autoscan_time = current_time; 


		bool schedule_before_current(devices_to_balance[0]->second->next_autoscan_time-current_time > interval_in_seconds/2);
		
		if (schedule_before_current)
			device_to_update->second->next_autoscan_time = devices_to_balance[0]->second->next_autoscan_time - interval_in_seconds/2;
		else 
			device_to_update->second->next_autoscan_time = devices_to_balance[0]->second->next_autoscan_time + interval_in_seconds/2;

		lock.release();
		return true;
	}

	std::sort(devices_to_balance.begin(),devices_to_balance.end(),ns_start_time_comparer());

	//if we are inserting the device into a balanced set of devices, add it to the list 
	//in the largest gap in the schedule.
	if (interval_in_seconds != 0){
		unsigned long largest_gap_interval(0);
		unsigned long largest_gap(devices_to_balance[1]->second->next_autoscan_time-devices_to_balance[0]->second->next_autoscan_time);
		for (int i = 1; i < (int)devices_to_balance.size()-1; ++i){
			const unsigned long gap(devices_to_balance[i+1]->second->next_autoscan_time-devices_to_balance[i]->second->next_autoscan_time);
			if (gap >= largest_gap){
				largest_gap = i;
				largest_gap_interval = gap;
			}
		}
		const unsigned long wrap_around_gap ((devices_to_balance[0]->second->next_autoscan_time +  interval_in_seconds) - devices_to_balance[devices_to_balance.size()-1]->second->next_autoscan_time);
		if (wrap_around_gap	>= largest_gap_interval){
			largest_gap = devices_to_balance.size()-1;
		}
		devices_to_balance.resize(devices_to_balance.size()+1);
		//insert the new device at the correct time
		for (int i = devices_to_balance.size()-1; i > largest_gap+1; --i)
			devices_to_balance[i]=devices_to_balance[i-1];
		devices_to_balance[largest_gap+1]=device_to_update;
	}
	else{
		//if we are removing the device from the list, update that device.
		//we still need to rebalance the remaining devices.
		device_to_update->second->next_autoscan_time = 0;
		device_to_update->second->autoscan_interval = 0;
	}

	//go ahead and balance all the devices
	unsigned long offset_interval = update_interval/(devices_to_balance.size());
	unsigned long start_offset = devices_to_balance[0]->second->next_autoscan_time;
	if (start_offset == 0)
		start_offset = devices_to_balance[0]->second->last_capture_start_time+offset_interval;
	if (start_offset + offset_interval < current_time)
		start_offset = current_time;
	
	for (unsigned int i = 0; i < devices_to_balance.size(); i++){
		devices_to_balance[i]->second->next_autoscan_time = start_offset + i*offset_interval;
		devices_to_balance[i]->second->autoscan_interval = update_interval;
	}

	lock.release();
	return true;
}

bool ns_image_server_device_manager::set_autoscan_interval(const std::string & device_name, const int interval_in_seconds,const unsigned long start_time){
	
		
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	bool changed(false);
	ns_device_list::const_iterator p = devices.find(device_name);
	if (!(p== devices.end())){
		if (p->second->autoscan_schedule_clash_detected){
			lock.release();
			return false;
		}

		changed = (p->second->autoscan_interval != interval_in_seconds);
		p->second->autoscan_interval = interval_in_seconds;
		if (start_time != 0)	p->second->next_autoscan_time = start_time;
		else if (changed)		p->second->next_autoscan_time = p->second->last_capture_start_time + interval_in_seconds;

		lock.release();
		return changed;
	}
	
	for (unsigned int i = 0; i < devices_pending_deletion.size(); i++){
		if (devices_pending_deletion[i]->device.name != device_name){
			if (devices_pending_deletion[i]->autoscan_schedule_clash_detected){
				lock.release();
				return false;
			}
			changed = (	devices_pending_deletion[i]->autoscan_interval == interval_in_seconds);
			devices_pending_deletion[i]->autoscan_interval = interval_in_seconds;
			if (start_time != 0) 	p->second->next_autoscan_time = start_time;
			else if (changed)		devices_pending_deletion[i]->next_autoscan_time = p->second->last_capture_start_time + interval_in_seconds;
			lock.release();
			return changed;
		}
	}
	throw ns_ex("ns_image_server_device_manager::set_pause_state()::Unknown device name:") << device_name;
}

bool ns_image_server_device_manager::set_pause_state(const std::string & device_name, const int state){
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
		bool changed(false);
		ns_device_list::const_iterator p = devices.find(device_name);
		if (!(p== devices.end())){
				changed = (p->second->device.paused != state);
				p->second->device.paused = state;
				lock.release();
				return changed;
		}
		
		for (unsigned int i = 0; i < devices_pending_deletion.size(); i++){
			if (devices_pending_deletion[i]->device.name != device_name){
				changed = (	devices_pending_deletion[i]->device.paused == state);
				devices_pending_deletion[i]->device.paused = state;
				lock.release();
				return changed;
			}
		}
		throw ns_ex("ns_image_server_device_manager::set_pause_state()::Unknown device name:") << device_name;
}


bool ns_image_server_device_manager::device_is_in_error_state(const std::string & name, std::string & error)const{
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
		ns_device_list::const_iterator p = devices.find(name);
		if (p == devices.end()){
			lock.release();
			throw ns_ex("ns_image_server_device_manager::device_is_in_error_state()::Unknown device name:") << name;
		}
		error = p->second->device.last_capture_failure_text;
		const bool ret(p->second->device.is_in_error_state());
		lock.release();
		return ret;
}

void ns_image_server_device_manager::clear(){
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	while(!devices.empty()){
		if (devices.begin()->second->currently_scanning){
			devices.begin()->second->pending_deletion = true;
			devices_pending_deletion.push_back(devices.begin()->second);
		}
		else{
			ns_safe_delete(devices.begin()->second);
		}
		devices.erase(devices.begin());
	}
	lock.release();
}

void ns_image_server_device_manager::reset_all_devices(){
		for (ns_device_list::iterator p = devices.begin(); p != devices.end(); ++p){
			try{
				pause_to_keep_usb_sane();
				p->second->device.send_hardware_reset();
			}
			catch(ns_ex & ex){
				cerr << "Could not reset " << p->second->device.name << ": " << ex.text() << "\n";
			}
		}
		for (std::vector<ns_image_server_device_manager_device *>::iterator p = devices_pending_deletion.begin(); p != devices_pending_deletion.end();++p){
			try{
				pause_to_keep_usb_sane();
				(*p)->device.send_hardware_reset();
			}
			catch(ns_ex & ex){
				cerr << "Could not reset " << (*p)->device.name << ": " << ex.text() << "\n";
			}
		}
}

void ns_image_server_device_manager::wait_for_all_scans_to_complete(){
	clear();
	while(!devices_pending_deletion.empty()){
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
		std::vector<ns_image_server_device_manager_device *>::iterator p;
		for (p = devices_pending_deletion.begin(); p != devices_pending_deletion.end();){
			if (!(*p)->currently_scanning){
				ns_safe_delete((*p));
				*p = 0;
				p = devices_pending_deletion.erase(p);
			}
			else break;
		}
		if (p == devices_pending_deletion.end()) break;
		ns_image_server_device_manager_device * d(*p);
		if (!devices_pending_deletion.empty())
			std::cerr << "Waiting for " << devices_pending_deletion.size() << " devices...\n";
		lock.release();
		//XXX Since we  dont' want to hold the lock for the duration of the block
		//we introduce a subtle race condition here.
		//The thread tracking data might have been detached() before we get to the next line
		try{
			if (d == 0)
				throw ns_ex("Encountered a deleted device!");
			if (!d->capture_thread_needs_cleanup)
				throw ns_ex("Server would like to wait on thread but its metadata has already been cleared up.");
			cerr << "Waiting for " << d->device.name << "\n";
			d->capture_thread.block_on_finish();
			d->capture_thread_needs_cleanup = false;
			d->currently_scanning = false;
		}
		catch(ns_ex & ex){
			lock.get(__FILE__,__LINE__);
			d->currently_scanning = false;
			d->capture_thread_needs_cleanup = false;
			lock.release();
			ns_ex er("Problem while waiting for device ");
			er << d->device.name << ": " << ex.text() << ex.type();
			image_server.register_server_event(ns_image_server::ns_register_in_local_db,er);
		}
	}
	cerr << "Done waiting for devices.\n";

}

bool ns_image_server_device_manager::device_is_currently_scanning(const std::string & name)const{
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
		ns_device_list::const_iterator p = devices.find(name);
		if (p == devices.end()){
			lock.release();
			throw ns_ex("ns_image_server_device_manager::device_is_currently_scanning()::Unknown device name: ") << name;
		}
		const bool ret(p->second->currently_scanning);
		lock.release();
		return ret;
}
void ns_image_server_device_manager::add_device(const std::string & name, const std::string & hardware_address, const ns_capture_device::ns_device_flag device_flags){
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

	ns_device_list::iterator p(devices.insert(ns_device_list::value_type(name,new ns_image_server_device_manager_device(name,hardware_address,device_flags))).first);

	lock.release();
}

void ns_image_server_device_manager::remove_device(const std::string & name){
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

	ns_device_list::iterator p = devices.find(name);
	if (p == devices.end()){
		lock.release();
		throw ns_ex("ns_image_server_device_manager::remove_device()::Unknown device name:") << name;
	}
	if (p->second->currently_scanning){
		p->second->pending_deletion = true;
		devices_pending_deletion.push_back(p->second);
	}
	else deleted_devices.push_back(p->second);
	devices.erase(p);
	lock.release();
}



ns_thread_return_type ns_image_server_device_manager::run_capture_on_device_internal(void * thread_arguments){
	ns_image_server_device_manager_aysnch_argument * arg(static_cast<ns_image_server_device_manager_aysnch_argument *>(thread_arguments));

	std::string capture_device_name(arg->capture_arguments->device_name);
	try{

		arg->manager->run_capture_on_device_asynch(*(arg->capture_arguments));

	}
	catch(std::exception & exception){
		cerr << "d2";
		ns_ex exp(exception);
		if (arg == 0)
			exp <<"thread_start_capture()::Somebody deleted the arguments object!! ";
	
		exp << "Error in capture_thread for device '" << capture_device_name << "':";
		
		cerr << "d3";
		if (arg != 0){
			try{
				ns_64_bit event_id = image_server.register_server_event(ns_image_server::ns_register_in_local_db,exp);
				if (arg->capture_arguments->capture_specification.capture_schedule_entry_id != 0){
					
					cerr << "d4a";
					ns_acquire_for_scope<ns_local_buffer_connection> sql(image_server.new_local_buffer_connection(__FILE__,__LINE__));
					sql() << "UPDATE buffered_capture_schedule SET problem = " << event_id << " WHERE id = " << arg->capture_arguments->capture_specification.capture_schedule_entry_id;
					sql().send_query();
					sql.release();
				}
				else cerr << "d4b";
			}
			catch(ns_ex & ex2){
				cerr << "\nthread_start_capture()::Could not report error (" << exp.text() << ") to the database due to the following error: " << ex2.text() << "\n";
			}
		}

		if (exp.type() == ns_memory_allocation){
			try{
				cerr << "\nTrying to free memory on error...\n";
				image_server.image_storage.cache.clear_cache_without_cleanup();
			}
			catch(ns_ex & exp2){
				try{
					image_server.register_server_event(ns_image_server::ns_register_in_local_db,exp2);
				}
				catch(std::exception & e){
					ns_ex ex3(e);
					cerr << "\nthread_start_capture()::Could not register cache clearing error: " << ex3.text() <<"\n";
				}
				catch(...){
					cerr << "\nthread_start_capture()::Could not register cache clearing error for unknown reasons.\n";
				}
			}
		}
	}
	

	
	ns_safe_delete(arg->capture_arguments->capture_specification.volatile_storage);

	try{
		//cerr << "d5";
		image_server.device_manager.mark_device_as_finished_scanning(arg->capture_arguments->device_name);
	}
	catch(ns_ex & ex){
		cerr << "Could not mark device as finished: " << ex.text() << "\n";
	}
	catch(...){
		cerr << "Could not mark device as finished for an unknown reason!\n";
	}
	
	ns_safe_delete(arg->capture_arguments);
	ns_safe_delete(arg);

	return 0;
}

std::string ns_image_server_device_manager::autoscan_parameters(ns_sql & sql){
	string default_autoscan_parameters("--mode=Gray --format=tiff --source=\"TPU8X10\" --resolution=3200 --depth=16 "
		   "-l .2in -t .2in -x 2in -y 8.2in");
	try{
		return image_server.get_cluster_constant_value("autoscan_parameters",default_autoscan_parameters,&sql);
	}
	catch(ns_ex & ex){
		try{
			image_server.register_server_event(ex,&sql);
			return default_autoscan_parameters;
		}
		catch(...){}
		std::cerr << ex.text() << "\n";
		return default_autoscan_parameters;
	}
	catch(...){
		std::cerr << "An unknown error occurred while trying to load autoscan parameters!\n";
		return default_autoscan_parameters;
	}

}
void ns_image_server_device_manager::run_pending_autoscans(ns_image_server_dispatcher * d, ns_sql & sql){
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	vector<std::string> devices_to_start;
	const unsigned long t(ns_current_time());

	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); ++p){
		//fix any improperly scheduled autoscans
		if (p->second->autoscan_schedule_clash_detected){
			p->second->autoscan_interval = 0;
			p->second->next_autoscan_time = 0;
			sql << "UPDATE devices SET autoscan_interval = 0, next_autoscan_time = 0 WHERE name = '" << p->second->device.name << "'";
			sql.send_query();
			p->second->autoscan_schedule_clash_detected = false;
		}
		if (p->second->device.paused || p->second->autoscan_interval == 0 || p->second->currently_scanning) continue;
	
		if (p->second->next_autoscan_time <= t){
			p->second->last_autoscan_time = t;
			p->second->next_autoscan_time = t + p->second->autoscan_interval;
			devices_to_start.push_back(p->second->device.name);
		}
	}
	lock.release();
	//ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
	for (unsigned int i = 0; i < devices_to_start.size(); i++){
		ns_capture_thread_arguments * arg(new ns_capture_thread_arguments);
		arg->buffered_capture_scheduler = &d->buffered_capture_scheduler;
		arg->device_name = devices_to_start[i];
		arg->capture_specification.capture_parameters = autoscan_parameters(sql);
		cerr << "Sending autoscan parameters : " << arg->capture_specification.capture_parameters << "\n";
		arg->capture_specification.volatile_storage = 0;
		arg->capture_specification.image.captured_images_id = 0;
		arg->capture_specification.capture_schedule_entry_id = 0;
		arg->capture_specification.speed_regulator.initialize_for_capture(0,0);
		
		image_server.register_server_event(ns_image_server_event("Starting autoscan capture on ") << devices_to_start[i] << "...",&sql);
		run_capture_on_device(arg);
	}
//	sql.release();
}
void ns_image_server_device_manager::run_capture_on_device(ns_capture_thread_arguments * a){
	ns_image_server_device_manager_aysnch_argument * arg(0);
	ns_device_list::const_iterator p;
	try{
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

		p = devices.find(a->device_name);
		if (p == devices.end()){
			lock.release();
			throw ns_ex("ns_image_server_device_manager::run_capture_on_device()::Unknown device name:") << a->device_name;
		}
	
		lock.release();
		arg = new ns_image_server_device_manager_aysnch_argument;
		arg->capture_arguments = a;
	}
	catch(...){
		ns_safe_delete(arg);
		ns_safe_delete(a);
		throw;
	}

	try{
		arg->manager = this;
		p->second->capture_thread.run(ns_image_server_device_manager::run_capture_on_device_internal,arg);
		p->second->capture_thread_needs_cleanup = true;
	}
	catch(...){
		ns_safe_delete(arg->capture_arguments);
		ns_safe_delete(arg);
		throw;
	}
}


void ns_image_server_device_manager::pause_to_keep_usb_sane(){
	usb_access_lock.wait_to_acquire(__FILE__,__LINE__);
	ns_thread::sleep(2);
	usb_access_lock.release();
}


void ns_image_server_device_manager::run_capture_on_device_asynch(ns_capture_thread_arguments & arguments){
	//#ifndef _WIN32
    //    umask(0x01FC);
	//#endif

	ns_image_server_device_manager_device * device;
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);

	ns_device_list::iterator p(devices.find(arguments.device_name));
	if (p==devices.end())
		throw ns_ex("The device ") << arguments.device_name << " has been removed from the system.  The requested capture cannot be completed.";

	device = p->second;
	if (device->currently_scanning)
		throw ns_ex("The device ") << arguments.device_name << " already has a scan running.  The requested capture cannot be completed.";

	device->currently_scanning = true;
	device->last_capture_start_time = ns_current_time();
	lock.release();


	ns_64_bit problem_id=0;
	//complete the hardware capture

	ns_acquire_for_scope<ns_local_buffer_connection> sql(image_server.new_local_buffer_connection(__FILE__,__LINE__));
	//attempt to run the capture
	try{
		pause_to_keep_usb_sane();
		device->device.capture(arguments.capture_specification);
		//indicate that the last scan completed successfully
		device->device.last_capture_failure_text.resize(0);
	}
	catch(std::exception & exception){
		ns_ex ex(exception);
		try{

			if (ex.text() != device->device.last_capture_failure_text){
				//generate new alerts on new errors
				device->device.error_state_has_been_recognized = false;
			}
			
			try{
				pause_to_keep_usb_sane();
				device->device.send_hardware_reset();
			}
			catch(ns_ex & ex){
				image_server.register_server_event(ex,&sql());
			}
			device->device.last_capture_failure_text = ex.text();

			if (arguments.capture_specification.image.captured_images_id != 0)
				image_server.image_storage.delete_from_storage(arguments.capture_specification.image,ns_delete_volatile,&sql());
			
			
			problem_id = image_server.register_server_event(ex,&sql());
			if (arguments.capture_specification.capture_schedule_entry_id != 0){
				sql() << "UPDATE buffered_capture_schedule SET problem = " << problem_id << " WHERE id = " << arguments.capture_specification.capture_schedule_entry_id;
				sql().send_query();
			}
		}
		catch(ns_ex & ex2){
			ex << " Futher error generated by error handler: " << ex2.text();
		}
	}

	//update db to show that the scan has finished.
	arguments.buffered_capture_scheduler->register_scan_as_finished(device->device.name,arguments.capture_specification,problem_id,sql());
	
	if (arguments.capture_specification.speed_regulator.warnings.size() > 0){
			image_server.register_server_event(ns_ex("During Capture on ") << arguments.device_name 
				<< " the speed regulator threw an error: " << arguments.capture_specification.speed_regulator.warnings[0],&sql());
	}

	//cerr << "d1";
	sql.release();
}
struct ns_device_error_info{
	ns_device_error_info(){}
	ns_device_error_info(const std::string & name_,const std::string & error_text_):name(name_),error_text(error_text_){}
	std::string name,
		 error_text;
};
void ns_image_server_device_manager::scan_and_report_problems( const std::vector<std::string> & devices_to_suppress, const std::vector<std::string> & experiments_to_suppress,ns_sql & sql){

	vector<ns_device_error_info> newly_broken_devices;
	vector<ns_device_error_info> still_broken_devices;
	vector<ns_device_error_info> newly_recovered_devices;
	
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); ++p){
	
		ns_capture_device & device(p->second->device);
		if (device.paused) continue;
		//find new errors
		if (device.is_in_error_state()){
			if (!device.error_state_has_been_recognized){
				device.error_state_has_been_recognized = true;
				newly_broken_devices.push_back(ns_device_error_info(device.name,device.last_capture_failure_text));
			}
			else
				still_broken_devices.push_back(ns_device_error_info(device.name,device.last_capture_failure_text));
		}
		else{
			//find scanners that have repaired themselves
			if (device.error_state_has_been_recognized){
				device.error_state_has_been_recognized = false;
				newly_recovered_devices.push_back(ns_device_error_info(device.name,device.last_capture_failure_text));
			}
		}
	}

	lock.release();

	for (vector<ns_device_error_info>::iterator p = newly_broken_devices.begin(); p != newly_broken_devices.end();){
		sql << "UPDATE devices SET in_recognized_error_state = 1, error_text='" << sql.escape_string(p->error_text) << "' WHERE name='" << p->name << "'";
		sql.send_query();
		//removed suppressed devices from alert list
		bool suppress(false);
		for (unsigned int i = 0; i < devices_to_suppress.size(); i++){
			if (p->name == devices_to_suppress[i]){
				suppress = true;
				break;
			}
		}
		if (suppress)
			p = newly_broken_devices.erase(p);
		else p++;
	}
		for (vector<ns_device_error_info>::iterator p = newly_recovered_devices.begin(); p != newly_recovered_devices.end();){
		sql << "UPDATE devices SET in_recognized_error_state = 0, error_text='' WHERE name='" <<  p->name << "'";
		sql.send_query();
		
		//removed suppressed devices from alert list
		bool suppress(false);
		for (unsigned int i = 0; i < devices_to_suppress.size(); i++){
			if (p->name == devices_to_suppress[i]){
				suppress = true;
				break;
			}
		}
		if (suppress)
			p = newly_recovered_devices.erase(p);
		else p++;
	}
	

	//something new has happened!
	/*if (newly_broken_devices.size()>0 || newly_recovered_devices.size()> 0){
		try{
			//we want to make the server more sensitive to later problems, so we reset the rate limit innterval
			image_server.alert_handler.reset_alert_time_limit(ns_alert_handler::ns_device_error,sql);
		}
		catch(ns_ex & ex){
			cerr << "Could not reset device error alert timing: " << ex.text() << "\n";
		}
	}*/

	//something is broken
	if (newly_broken_devices.size() > 0 /*|| still_broken_devices.size() > 0*/){
		string broken_device_alert_summary_text,
			   broken_device_alert_detailed_text;
		try{
			if(newly_broken_devices.size() == 1){
				broken_device_alert_summary_text+="Device reports an error: ";
				broken_device_alert_detailed_text+="Device reports an error: ";
			}
			else{
				broken_device_alert_summary_text+="Devices report an error: ";
				broken_device_alert_detailed_text+="Devices report an error: ";
			}

			for (unsigned int i = 0; i < newly_broken_devices.size(); i++){
				broken_device_alert_summary_text+= newly_broken_devices[i].name + ",";	
				broken_device_alert_detailed_text+= newly_broken_devices[i].name + ": " + newly_broken_devices[i].error_text + "\n\n";	

			}
			/*for (unsigned int i = 0; i < still_broken_devices.size(); i++)
				broken_device_alert_text+= still_broken_devices[i].name + ",";*/
			broken_device_alert_summary_text.resize(broken_device_alert_summary_text.size()-1);  //remove trailing comma
				
			ns_alert alert(broken_device_alert_summary_text,
				broken_device_alert_detailed_text,
				ns_alert::ns_device_error,
				ns_alert::get_notification_type(ns_alert::ns_device_error,image_server.act_as_an_image_capture_server()),
				ns_alert::ns_rate_limited);

			image_server.alert_handler.submit_alert(alert,sql);
		}
		catch(ns_ex & ex){
			cerr << "Could not submit alert: " << broken_device_alert_summary_text << " : " << ex.text();
		}
	}	

	if (newly_recovered_devices.size()> 0){
		string alert_text;
		if (newly_recovered_devices.size() == 1)
			alert_text+="Device is back online: ";
		else alert_text+="Devices are back online: ";
		for (unsigned int i = 0; i < newly_recovered_devices.size(); i++)
			alert_text+= newly_recovered_devices[i].name + ",";
		alert_text.resize(alert_text.size()-1); //remove trailing comma
		try{	
			ns_alert alert(alert_text,
				alert_text,
				ns_alert::ns_device_error,
				ns_alert::get_notification_type(ns_alert::ns_device_error,image_server.act_as_an_image_capture_server()),
				ns_alert::ns_not_rate_limited);
			image_server.alert_handler.submit_alert(alert,sql);
		}
		catch(ns_ex & ex){
			cerr << "Could not submit alert: " << alert_text << " : " << ex.text();
		}
	}
}

class ns_scanner_ider{
public:
	ns_scanner_ider():capture_error_count(0){}
	unsigned long number_of_failed_capture_attempts(){return capture_error_count;}
	
	//ONLY RUN THIS WHILE HOLDING THE DEVICE MANAGER DEVICE LOCK
	void read_barcode(ns_image_server_device_manager_device & device_info){
		try{
			error_text.resize(0);
			hardware_alias = device_info.device.hardware_alias;
			temp_output_filename = std::string("barcode=") + device_info.device.hardware_alias + ".tif";   
			//remove illegal characters
			for (unsigned long i = 0; i < temp_output_filename.size(); i++)
				if (temp_output_filename[i] == ';' || temp_output_filename[i] == ':' ||
					temp_output_filename[i] == ',' || temp_output_filename[i] == '|' ||
					temp_output_filename[i] == '\'' || temp_output_filename[i] == '"' ||
					temp_output_filename[i] == '$')
					temp_output_filename[i] = '_';

			device_info.currently_scanning = true;
			device_info.last_capture_start_time = ns_current_time();
			device_info.capture_thread.run(run_scanner_ider,this);
			device_info.capture_thread_needs_cleanup = true;
		}
		catch(ns_ex & ex){
			error_text = ex.text();
			if (error_text.size() == 0)
				error_text = "Yikes: an unknown error was thrown.";
		}
		catch(std::exception & ex){
			error_text = ex.what();
			if (error_text.size() == 0)
				error_text = "Yikes: an unknown error was thrown.";
		}
	}
	
	void rename_output_file(const string & name){

		image_server.image_storage.request_from_volatile_storage(temp_output_filename).input_stream().pump(
		image_server.image_storage.request_volatile_storage<ns_8_bit>(name,1024,false).output_stream(),1024);
		image_server.image_storage.delete_from_volatile_storage(temp_output_filename);
	}

	const std::string & device_name()const
		{return recovered_device_name;}
	std::string error()const {return error_text;}
	bool failed_to_read_barcode() const {return (error_text.size() > 0);}
	private:
	unsigned long capture_error_count;
	static ns_thread_return_type  run_scanner_ider( void * scanner_ider){
		ns_scanner_ider * ider(static_cast<ns_scanner_ider*>(scanner_ider));
		try{
			ider->run();
			if (!ider->failed_to_read_barcode() &&
				ider->device_name().size() < 2)
				throw ns_ex("Device name \"") << ider->device_name() << " is too short.  Names must be at least 3 characters.";
			ider->capture_error_count= 0;
		}
		catch(ns_ex & ex){
			ider->capture_error_count++;
			ider->error_text = ex.text();
			if (ider->error_text.size() == 0)
				ider->error_text = "Yikes: an unknown error was thrown.";
		}
		catch(std::exception & ex){
			ider->capture_error_count++;
			ider->error_text = ex.what();
			if (ider->error_text.size() == 0)
				ider->error_text = "Yikes: an unknown error was thrown.";
		}
		return 0;
	}

	void run(){

		ns_capture_device device;
		recovered_device_name.clear();
		//capture scanner name barcode
		ns_image_capture_specification spec;
		device.hardware_alias = hardware_alias;
		//spec.capture_parameters = "-d " + scanners[i];
		spec.capture_parameters += " --mode Gray --format tiff --resolution 200 " + image_server.scanner_list_coord();
		spec.volatile_storage = 0;
		
		spec.volatile_storage = image_server.image_storage.request_volatile_binary_output(temp_output_filename); 
		try{
			device.capture(spec);
			ns_safe_delete(spec.volatile_storage);

		}
		catch(...){
			ns_safe_delete(spec.volatile_storage);
			throw;
		}
		//open scanner name barcode and process it.
		ns_image_standard bar_image;
		image_server.image_storage.request_from_volatile_storage(temp_output_filename).input_stream().pump(bar_image,512);

		try{
			//try to parse while producing a nice visualization
			std::string parse_filename = ns_dir::extract_filename_without_extension(temp_output_filename) + "_parsed." + ns_dir::extract_extension(temp_output_filename);
			recovered_device_name = ns_barcode_decode(bar_image,parse_filename);
		}
		catch(ns_ex & ex){
			//if the visualization cannot be made, try again to parse without it
			cerr << "Could not make barcode visualization, trying to decode without it...";
			recovered_device_name = ns_barcode_decode(bar_image);
		}
	}
	std::string error_text;

	std::string hardware_alias;
	std::string temp_output_filename;
	std::string recovered_device_name;

};


void ns_format_device_address(std::string & str){
	//add double slashes
	for (std::string::iterator p = str.begin(); p != str.end(); p++){
		if (*p == '\\'){
			p = str.insert(p,'\\');
			p++;
		}
	}
	//add device type
	str = "epson2:" + str;
}

bool ns_image_server_device_manager::hotplug_new_devices(const bool rescan_bad_barcodes, const bool verbose){
	
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	if (hotplug_running){
		lock.release();
		image_server.register_server_event(ns_image_server::ns_register_in_local_db,ns_image_server_event("Ignoring multiple hotplug requests."));
		return false;
	}
	hotplug_running = true;
	lock.release();
	
	vector<ns_image_server_device_manager_device *> orphaned_devices;
	vector<ns_image_server_device_manager_device *> devices_to_identify;

	try{
		bool changes_made(false);
		std::vector<ns_device_hardware_info> all_hardware;
		if(verbose)
			image_server.register_server_event(ns_image_server::ns_register_in_local_db,ns_image_server_event("Looking for hardware changes (hotplug)..."));
		pause_to_keep_usb_sane();
		lock.get(__FILE__,__LINE__);
		//create a list of the hardware addresses of scanners attached to the system
		#ifndef _WIN32
		ns_get_scanner_hardware_address_list(all_hardware);
		#endif

		//prepare to match up each hardware address to a known scanner
		map<std::string,ns_image_server_device_manager_device *> hardware_address_assignments;
		for (unsigned int i = 0; i < all_hardware.size(); i++) {
			if (all_hardware[i].vendor != "0x04b8") {	//only use epson scanners, to prevent attempts at scanning from random devices	
				if (verbose)
					image_server.register_server_event(ns_image_server::ns_register_in_local_db, ns_image_server_event("Skipping device from unknown vendor:") << all_hardware[i].vendor << " " << all_hardware[i].product << " " << all_hardware[i].address);
				continue;
			}
			ns_format_device_address(all_hardware[i].address);
			hardware_address_assignments[all_hardware[i].address] = 0;
		}

		//check to see if any devices have been removed from the cluster,
		//and update the local registry

		//copy all the devices out of the map so we can iterate through and delete them.
		{
			vector<ns_image_server_device_manager_device *> pre_existing_devices;
			for (ns_device_list::iterator device = devices.begin(); device != devices.end();++device)
				pre_existing_devices.push_back(device->second);
			devices.clear();
			try{

				unsigned int dd = 0;
				//match up each hardware address to a known scanner
				for (vector<ns_image_server_device_manager_device *>::iterator device = pre_existing_devices.begin(); device != pre_existing_devices.end();++device){
				//	cerr << dd << "/" << pre_existing_devices.size() << "\n";
					dd++;
					//simulated scanners don't need to get matched up to hardware addresses
					if ((*device)->device.is_simulated_device()){
						devices.insert(ns_device_list::value_type((*device)->device.name,*device));
						//cerr << "Device " << (*device)->device.name << " remains connected as a simulated device.\n";
						continue;
					}
					map<std::string,ns_image_server_device_manager_device *>::iterator hardware_assignment(hardware_address_assignments.find((*device)->device.hardware_alias));
					
					//if the scanner matches an extant hardware address, mark that hardware address as recognized, put the device back into the current device list
					if (hardware_assignment != hardware_address_assignments.end()){
						if ((*device)->device.unknown_identity() && rescan_bad_barcodes){
							if (
								(*device)->currently_scanning){
								//cerr << "Unknown device should be re-identified, but doing so is impossible as it is currently running.\n";
								hardware_assignment->second = *device;
								std::pair<ns_device_list::iterator,bool> r (devices.insert(ns_device_list::value_type((*device)->device.name,*device)));
								if (!r.second)
									throw ns_ex("Very bad error: Unknown device \"") <<  (*device)->device.name << " connected at " << (*device)->device.hardware_alias << " was almost duplicated in the device map.  Shut down the server and debug!";
							}
							else{
								//cerr << "Unknown device " << (*device)->device.name << " remains connected at " << (*device)->device.hardware_alias << " but it's barcode will be scanned anyway.\n";
								(*device)->device.set_default_unidentified_name();
								devices_to_identify.push_back(*device);
								hardware_assignment->second = *device;
							}
						}
						else{
							//cerr << "Device " << (*device)->device.name << " remains connected at " << (*device)->device.hardware_alias << ".\n";
							hardware_assignment->second = *device;
							std::pair<ns_device_list::iterator,bool> r(devices.insert(ns_device_list::value_type((*device)->device.name,*device)));
							if (!r.second)
								throw ns_ex("Very bad error: Device \"") <<  (*device)->device.name << " connected at " << (*device)->device.hardware_alias << " was almost duplicated in the device map.  Shut down the server and debug!";
						}
					}
					//if the scanner doens't have an extant hardware address, move it to the orphaned list.
					else{
						//cerr << "Device " << (*device)->device.name << " appears to have been disconnected.\n";
						orphaned_devices.push_back(*device);
						changes_made = true;
					}
				}
				//all pre_existing devices have now been transfered either to the orphaned or recognized arrays.
				pre_existing_devices.clear();
				lock.release();
			}
			catch(...){
				for (unsigned int i = 0; i < pre_existing_devices.size(); i++)
					ns_safe_delete(pre_existing_devices[i]);
				orphaned_devices.resize(0);
				devices_to_identify.resize(0);
				throw;
			}
		}
		
		vector<ns_image_server_event> events_to_submit;

		lock.get(__FILE__,__LINE__);
		//cerr << "handling orphans\n";
		//handle orphaned devices
		for (unsigned int i = 0; i < orphaned_devices.size(); ++i){
			events_to_submit.push_back(ns_image_server_event("Device ") << orphaned_devices[i]->device.name << " appears to have been unplugged.  Removing it from the registry.");
			//a scanner probably wont be both disconnected and still scanning
			//but one can imagine an error state in which this is the case.
			//thus we handle it by moving the scanner to the devices_pending_deletion purgatory
			//in hopes the situation will resolve later.
			if (orphaned_devices[i]->currently_scanning){
				orphaned_devices[i]->pending_deletion = true;
				devices_pending_deletion.push_back(orphaned_devices[i]);
			}
			else ns_safe_delete(orphaned_devices[i]);
		}
		orphaned_devices.resize(0);

		//release the lock and then do any buffered i/o
		lock.release();
		//cerr << "Registering events\n";
		for (unsigned int i = 0; i < events_to_submit.size(); i++)
			image_server.register_server_event(ns_image_server::ns_register_in_local_db,events_to_submit[i]);
		events_to_submit.clear();

		//identify any unidentified hardware addresses
		int dd = 0;
		for (map<std::string,ns_image_server_device_manager_device *>::iterator p = hardware_address_assignments.begin(); p != hardware_address_assignments.end(); ++p){
		//	
			//if a hardware address has already been matched with a device, no further action need be taken
			if (p->second != 0)
				continue;

			changes_made = true;

			ns_image_server_device_manager_device * dev(new ns_image_server_device_manager_device("",p->first,ns_capture_device::ns_unknown_identity));
			
			dev->device.set_default_unidentified_name();	
		
			//make sure nobody grabs the current scanner before we're done with it.
			dev->device.last_capture_failure_text = "Device has not had barcode scanned yet";
			dev->device.error_state_has_been_recognized = true;
			devices_to_identify.push_back(dev);
		}
		lock.release(); 
	//	cerr << "Capturing barcodes\n";
		//find the identities of the new devices.
		identify_new_devices(devices_to_identify);
		hotplug_running = false;
		return changes_made;
	}
	catch(...){
		for (unsigned int i = 0; i < orphaned_devices.size(); ++i){
			ns_safe_delete(orphaned_devices[i]);
		}
		for (unsigned int i = 0; i < devices_to_identify.size(); ++i){
			ns_safe_delete(devices_to_identify[i]);
		}
		hotplug_running = false;
		throw;
	}
}

void ns_image_server_device_manager::identify_new_devices(std::vector<ns_image_server_device_manager_device *> & unidentified_devices){
	
	ns_ex stored_ex;
	ns_image_server_event ev;
	if (unidentified_devices.size() > 0)
		ev << "Requesting barcodes from " << (unsigned long)unidentified_devices.size() << " hardware addresses:\n";

	std::vector<ns_scanner_ider> scanner_identifiers(unidentified_devices.size());

	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	for (unsigned int i = 0; i < scanner_identifiers.size(); i++) {
		ev << " " << unidentified_devices[i]->device.hardware_alias << "\n";
	}
	lock.release();

	image_server.register_server_event(ns_image_server::ns_register_in_local_db, ev);

	lock.get(__FILE__, __LINE__);
	ev.clear_text();
	//request barcode from scanner
	for (unsigned int i = 0; i < scanner_identifiers.size(); i++){
		try{
			try{
				pause_to_keep_usb_sane();
				unidentified_devices[i]->device.send_hardware_reset();
			}
			catch(ns_ex & ex){
				ev << ex.text() << "\n";
			}
		}
		catch(ns_ex & ex){
			ev << ex.text() << "\n";
		}
		pause_to_keep_usb_sane();
		scanner_identifiers[i].read_barcode(*unidentified_devices[i]);
	}
	lock.release();

	image_server.register_server_event(ns_image_server::ns_register_in_local_db, ev);

	bool finished_reading_barcodes(false);
	while(!finished_reading_barcodes){
		finished_reading_barcodes = true;
		//wait for all barcodes to arrive
		for (unsigned int i = 0; i < unidentified_devices.size(); i++){
			if (unidentified_devices[i]->currently_scanning){ 

				if (unidentified_devices[i]->capture_thread_needs_cleanup){
					unidentified_devices[i]->capture_thread.block_on_finish();
					unidentified_devices[i]->capture_thread_needs_cleanup = false;
				}
				unidentified_devices[i]->currently_scanning = false;

				unidentified_devices[i]->last_capture_start_time = 0;
				//we detect if there is a capture error.  Sometimes the first request to a scanner fails (due to some state not being reset
				//in the usb drivers).  We give each scanner 3 tries to work.

				if (scanner_identifiers[i].number_of_failed_capture_attempts()){
					if (scanner_identifiers[i].number_of_failed_capture_attempts() < 2){
						image_server.register_server_event(ns_image_server::ns_register_in_local_db, ns_image_server_event("Could not read barcode from address") <<
							unidentified_devices[i]->device.hardware_alias << ": " << scanner_identifiers[i].error() << "\nRetrying...");
						finished_reading_barcodes = false;
						scanner_identifiers[i].read_barcode(*unidentified_devices[i]);
						//keep USB sane
					}
					else {
						image_server.register_server_event(ns_image_server::ns_register_in_local_db,ns_image_server_event("Could not read barcode from address ") << 	unidentified_devices[i]->device.hardware_alias);
					}
				}
			}
			else{
				if (unidentified_devices[i]->capture_thread_needs_cleanup){
					unidentified_devices[i]->capture_thread.detach();
					unidentified_devices[i]->capture_thread_needs_cleanup = false;
				}
			}
		}
	}

	//All the scanners have acquired their barcodes
	//Nothing will have messed up our pointers to devices because other threads must respect the currently_scanning flag
	for (unsigned int i = 0; i < unidentified_devices.size(); i++){
		if (!scanner_identifiers[i].failed_to_read_barcode()){

			ns_image_server_event ev(unidentified_devices[i]->device.hardware_alias);
			ev << " identified itself as " <<  scanner_identifiers[i].device_name();
			try{
				image_server.register_server_event(ns_image_server::ns_register_in_local_db,ev);
			}

			catch(...){
				cerr << "Error registering scanner identification: " << ev.text() << "\n";
			}
			
			unidentified_devices[i]->device.name = scanner_identifiers[i].device_name();
			//mark the device as identified
			unidentified_devices[i]->device.device_flags = ns_capture_device::ns_none;
			
		}
		else{
			//the scanner didn't produce an intelligible response, so we give up

			ns_image_server_event ev("The device at ");
			ev << unidentified_devices[i]->device.hardware_alias;
			ev << " did not produce a legible barcode";
			if (scanner_identifiers[i].error().empty())
				ev << ". The barcode was \"" << scanner_identifiers[i].device_name() << "\"";
			else ev << ", instead producing the error: " << scanner_identifiers[i].error();
			try{
				image_server.register_server_event(ns_image_server::ns_register_in_local_db,ev);
			}
			catch(...){
				cerr << "Error registering scanner identification:" << ev.text() << "\n";
			}
		}
		
		//all errors should be new after a hotplug
		unidentified_devices[i]->device.last_capture_failure_text.clear();
		unidentified_devices[i]->device.error_state_has_been_recognized = false;
		unidentified_devices[i]->pending_deletion = false;

		lock.get(__FILE__,__LINE__);	
		std::pair<ns_device_list::iterator,bool> r(devices.insert(ns_device_list::value_type(unidentified_devices[i]->device.name,unidentified_devices[i])));
		if (!r.second)
			throw ns_ex("Very bad error: Newly Discovered Device \"") <<  unidentified_devices[i]->device.name << " connected at " << unidentified_devices[i]->device.hardware_alias << " was almost duplicated in the device map.  Shut down the server and debug!";
		lock.release();


		if (!scanner_identifiers[i].failed_to_read_barcode()){
			try{
				scanner_identifiers[i].rename_output_file(std::string("barcode=") + scanner_identifiers[i].device_name() + ".tif");
			}
			catch(ns_ex & ex){
				cerr << "Could not re-label barcode: " << ex.text();
			}
		}
//		else cerr << "Since the scanner produced an error, the barcode labeling will not be attempted.\n";
	}

}

void ns_image_server_device_manager::attach_simulated_device(const std::string & device_name){
	if (device_name.size() < 2)
		throw ns_ex("Simulated device name is too short: ") << device_name;

	//add simulator scanner name if it doesn't already exist
	bool simulator_present(false);
	ns_device_list::iterator p(devices.find(device_name));

	if (p != devices.end()) return;
		add_device(device_name,"",ns_capture_device::ns_simulated_device);
}

void ns_image_server_device_manager::save_last_known_device_configuration(){
	ofstream * out(image_server.write_device_configuration_file());
	try{
		ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
		*out << "# ns_image_server: Last known device configuration for host " << image_server.host_name_out() << "\n";
		if (devices.empty())
			*out << "#\n" << "# No devices were identified.\n";
		*out << "!" << ns_system_boot_time_str() << "\n";
		unsigned long good_device_count(0),
					  unknown_device_count(0),
					  simulated_device_count(0);
		for (ns_device_list::iterator p = devices.begin(); p != devices.end(); ++p){
			if (p->second->device.unknown_identity())
				*out << "# Unidentifiable device at ";
			else if (p->second->device.is_simulated_device())
				*out << "# Simulated device:";
			*out << p->second->device.hardware_alias << "\t" << p->second->device.name << "\n";
		}
		out->close();
		delete out;
		lock.release();
	}
	catch(...){
		delete out;
		throw;
	}
}
void ns_image_server_device_manager::output_connected_devices(ostream & o) {
	ns_acquire_lock_for_scope lock(device_list_access_lock, __FILE__, __LINE__);
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); ++p) {
		o << p->second->device.name << "\t@" << p->second->device.hardware_alias << "\n";
	}
	lock.release();
}

bool ns_image_server_device_manager::load_last_known_device_configuration(){

	ns_acquire_for_scope<ifstream> in(image_server.read_device_configuration_file());
	if (in().fail()){
		in.release();
		return false;
	}
	string file_specified_last_boot_time;
	while(true){
		char first(in().get());
		if (in().fail())
			break;
		//allow comments
		if (first == '#'){
			while (!in().fail() && first != '\n') 
				first = in().get();
			if (in().fail())
				throw ns_ex("ns_image_server::load_last_known_device_configuration()::Invalid syntax");
			else
				continue;
		}
		if (first == '!'){
			while(true){
				char a = in().get();
				if (in().fail())
					throw ns_ex("ns_image_server::load_last_known_device_configuration()::Invalid syntax");
				if (a=='\n') break;
				file_specified_last_boot_time += a;
			}

			continue;
		}
		std::string hardware,name;
		in() >> hardware;
		if (in().fail()){
			add_device(name,hardware);
			break;
		}
		hardware = string() + first + hardware;
		in() >> name;
		if (in().fail())
			throw ns_ex("ns_image_server::load_last_known_device_configuration()::Invalid syntax");
		if (name.size() < 3 || hardware.size() < 4)
			throw ns_ex("Unknown device specification: \"") << name << "\" \"" << hardware << "\"";
		add_device(name,hardware);
		while(true){
			char a(in().get());
			if (in().fail() || a == '\n')
				break;
		}
	}
	in().close();
	in.release();
	string boot_time(ns_system_boot_time_str());
	if (file_specified_last_boot_time != boot_time){
		ns_image_server_event ev("A reboot appears to have occurred (system log: ");
		ev << boot_time;
		ev << ").  Rebuilding device list.";
		image_server.register_server_event_no_db(ev);
		return false;
	}

	return true;
}
void ns_image_server_device_manager::clear_device_list_and_identify_all_hardware(){
	//clear device information
	ns_acquire_lock_for_scope lock(device_list_access_lock,__FILE__,__LINE__);
	for (ns_device_list::iterator p = devices.begin(); p != devices.end(); ++p){
		if (p->second->currently_scanning)
			throw ns_ex("Could not assemble scanner identities as a scan is currently running on ") << p->second->device.name;
	}
	devices.clear();
	lock.release();
	hotplug_new_devices();
}



ns_lock ns_global_usb_lock("ns_global_usb_lock");