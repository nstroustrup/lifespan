#include "ns_image_server_dispatcher.h"
#include "ns_ex.h"
#include "ns_image_server.h"
#include "ns_sql.h"
#include <iostream>
#include <fstream>
#include <time.h>
#include "ns_dir.h"
#include "ns_image_socket.h"
#include "ns_tiff.h"
#include <iomanip>
#include "ns_processing_job_push_scheduler.h"
#ifndef NS_NO_XVID
#include "ns_xvid.h"
#endif
#include <stdlib.h> 
#include "ns_high_precision_timer.h"
#include "ns_sql.h"
using namespace std;

#ifdef _WIN32 
void destroy_icons();
#endif

//#define new new(_NORMAL_BLOCK,__FILE__,__LINE__)

 //  #ifdef _CRTDBG_MAP_ALLOC
   //inline void* __cdecl operator new(unsigned int s)
     //    { return ::operator new(s, _NORMAL_BLOCK, __FILE__, __LINE__); }
   //#endif /* _CRTDBG_MAP_ALLOC */


//string ns_image_server_image_capture_info::directory(ns_sql & sql){return ns_directory_from_image_info(experiment_id, sample_id, experiment_name,sample_name,sql);}


void ns_image_server_dispatcher::init(const unsigned int port,const unsigned int socket_queue_length){
  image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Dispatcher bound to ") << image_server.dispatcher_ip() << ":" << (int)image_server.dispatcher_port() << ". Using database " << image_server.sql_info());
	incomming_socket.listen(port,socket_queue_length);
}

void ns_image_server_dispatcher::connect_timer_sql_connection(){
		timer_sql_connection = image_server.new_sql_connection(__FILE__,__LINE__);
}
ns_thread_return_type handle_dispatcher_request(void * d){
	ns_image_server_dispatcher * dispatcher(static_cast<ns_image_server_dispatcher *>(d));
	dispatcher->handle_remote_requests();
	return 0;
}

void ns_image_server_dispatcher::run_hotplug(const bool rescan_bad_barcodes,const bool verbose){
	if (!image_server.act_as_an_image_capture_server()){
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Cannot execute hotplug command, as query_cluster_for_device_names is set to false in the config file."));
		return;
	}
	ns_acquire_lock_for_scope lock(hotplug_lock,__FILE__,__LINE__);
	
	if (hotplug_running){
		lock.release();
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Hotplug already in process."));
		return;
	}
	hotplug_running = true;
	lock.release();

	try{
		if (image_server.device_manager.hotplug_new_devices(rescan_bad_barcodes,verbose)){
			image_server.register_devices(verbose);
			image_server.device_manager.save_last_known_device_configuration();
		}
		hotplug_running = false;
	}
	catch(...){
		hotplug_running = false;
		throw;
	}
	
}

void ns_image_server_dispatcher::handle_remote_requests(){
	srand(ns_current_time());
	//empty the queue of pending messages
	while(true){
		if (pending_remote_requests.empty() || image_server.exit_requested){
			message_handling_thread.report_as_finished();

			return;
		}
		ns_acquire_lock_for_scope lock(message_handling_lock,__FILE__,__LINE__);
		ns_remote_dispatcher_request req(*pending_remote_requests.rbegin());
		pending_remote_requests.pop_back();
		lock.release();

		//srand(random_number_seed);
		//random_number_seed=rand();
		//double d(rand());
		//long s(300*d/(RAND_MAX+1));
		//cerr << "Sleeping for " << s << " ms....";
		//ns_thread::sleep(s);
	//	cerr << "Done.\n";
		try{
			try{
				ns_image_server_message &message(req.message);
				ns_socket_connection & socket_connection(req.connection);
		//	socket_connection.close();
		//	return;
				if (message.request() != NS_TIMER && message.request() != NS_CHECK_FOR_WORK && message.request() != NS_LOCAL_CHECK_FOR_WORK) {
					if (currently_unable_to_connect_to_the_central_db)
						image_server.register_server_event(ns_image_server::ns_register_in_local_db,ns_image_server_event("Recieved the message: ") << ns_message_request_to_string(message.request()));
					else image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("Recieved the message: ") << ns_message_request_to_string(message.request()));
				}
				//if (message.request() == NS_CHECK_FOR_WORK) cerr<< ".";

				switch(message.request()){
					case NS_NULL:
						socket_connection.close();
						//do nothing
						break;

					case NS_TIMER: 
						//no data to recieve.
						socket_connection.close();
						this->on_timer();
						break;

					case NS_CHECK_FOR_WORK:
						//don't accept outside requests for work if 
						//the server should run autonomously
						if (image_server.run_autonomously()){
							socket_connection.close();
							break;
						}
						//HERE, A BREAK IS DELIBERATELY OMITTED
					case NS_LOCAL_CHECK_FOR_WORK:
						socket_connection.close();
						this->on_timer();
						if (image_server.act_as_processing_node()){
							this->start_looking_for_new_work();
						}
						break;

					case NS_IMAGE_REQUEST:{
						//if possible, send the image
						ns_vars v;
						v.from_string(message.data());
						this->process_image_request(v.get_int("image_id"),v("host_ip"),v.get_int("host_port"));
						socket_connection.close();
						break;
					}
					case NS_CLEAR_DB_BUF_CLEAN:
						clean_clear_local_db_requested = true;
						break;
					case NS_CLEAR_DB_BUF_DIRTY:	
						try{
							
							ns_acquire_for_scope<ns_local_buffer_connection> local_buffer_connection(image_server.new_local_buffer_connection(__FILE__,__LINE__));
							ns_acquire_for_scope<ns_local_buffer_connection> sql(image_server.new_local_buffer_connection(__FILE__,__LINE__));
							this->buffered_capture_scheduler.clear_local_cache(sql());
							local_buffer_connection.release();
							sql.release();
						}
						catch(ns_ex & ex){
							image_server.register_server_event_no_db(ns_image_server_event(ex.text()));
						}
						break;
					case NS_RELOAD_MODELS:{
						socket_connection.close();
						image_server.clear_model_cache();
						break;
					}
					case NS_SIMULATE_CENTRL_DB_CONNECTION_ERROR:{
						image_server.toggle_central_mysql_server_connection_error_simulation();
						break;
																}
					case NS_OUTPUT_IMAGE_BUFFER_INFO:{
						this->buffered_capture_scheduler.image_capture_data_manager.transfer_status_debugger.print_status();
						break;
					}
					case NS_IMAGE_SEND:
						//a remote host is sending an image; save it.
						this->recieve_image(message,socket_connection);
						break;

					case NS_STATUS_REQUEST:
						//send information to remote host.
						break;
					case NS_HOTPLUG_NEW_DEVICES:
						socket_connection.close();
						run_hotplug();
						break;
					case NS_RESET_DEVICES:
						socket_connection.close();
						image_server.device_manager.clear_device_list_and_identify_all_hardware();
						image_server.register_devices();
						break;
					case NS_WRAP_M4V:{
						socket_connection.close();
						string input_filename = message.data();
						string output_basename = ns_dir::extract_filename_without_extension(input_filename);
						ns_wrap_m4v_stream(input_filename,output_basename);
						break;
					}
					case NS_QUIT:
						throw ns_ex("Error: Quit Command should have been caught by dispatcher!");
					default:
						throw ns_ex("Could not process remote message:") << (unsigned long)message.request();
				}	
			}
			catch(ns_ex & ex){
				if (image_server.act_as_an_image_capture_server())
					image_server.register_server_event(ns_image_server::ns_register_in_local_db,ex);
				else 
					image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);

			}
			catch(std::exception & e){
				ns_ex ex(e);
				if (image_server.act_as_an_image_capture_server())
					image_server.register_server_event(ns_image_server::ns_register_in_local_db,ex);
				else 
					image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
			}
		}
		catch(ns_ex & ex){
			image_server.register_server_event_no_db(ns_image_server_event("ns_image_server_dispatcher::handle_remote_requests()::register_server_event() threw an exception: ") << ex.text() << "\n");
		}
		catch(...){
			image_server.register_server_event_no_db(ns_image_server_event("ns_image_server_dispatcher::handle_remote_requests()::register_server_event() threw an unknown exception\n"));
		}

	}
	//we should never get here because the while() loop should end only in a return() call.
	image_server.register_server_event(ns_image_server::ns_register_in_local_db,ns_ex("ns_image_server_dispatcher::handle_remote_requests()::Code flow error!"));
	//ns_acquire_lock_for_scope lock(message_handling_lock,__FILE__,__LINE__);
	message_handling_thread.report_as_finished();
//	lock.release();
}

void ns_image_server_dispatcher::run(){
	try{
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Dispatcher refresh interval set to ",false) << image_server.dispatcher_refresh_interval() << " seconds.");
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Software Compilation Date: ") << __DATE__);
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Dispatcher started.",false));
		while(!image_server.exit_requested){
			ns_socket_connection socket_connection;
			try{
				socket_connection = incomming_socket.accept();
			}
			catch(...){
				//if we can't bind the socket we're sunk.
				image_server.exit_requested = true;
				throw;
			}
			
			ns_acquire_lock_for_scope lock(message_handling_lock,__FILE__,__LINE__);
			pending_remote_requests.push_front(ns_remote_dispatcher_request(socket_connection));

			try{
				pending_remote_requests.begin()->message.get();

			}
			catch(ns_ex & ex){
				lock.release();
				image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_ex("Error during remote request: ") << ex.text());
				
				socket_connection = incomming_socket.accept();
				socket_connection.close();
				continue;
			}
			ns_message_request req = pending_remote_requests.begin()->message.request();

			if (req == NS_QUIT){
				pending_remote_requests.begin()->connection.close();
				pending_remote_requests.pop_front();
				image_server.exit_requested = true;
			}
			//some jobs shouldn't have duplicates on the queue, as they only need to be run once in any short time interval
			else if (req == NS_CHECK_FOR_WORK ||
				req ==	NS_HOTPLUG_NEW_DEVICES ||
				req ==	NS_RESET_DEVICES ||
				req ==	NS_RELOAD_MODELS ||
				req ==	NS_LOCAL_CHECK_FOR_WORK ||
				req ==	NS_TIMER
				){
				list<ns_remote_dispatcher_request>::iterator p = pending_remote_requests.begin();
				p++;
				//if we find any duplicates, remove one of them and stop.
				for(;p != pending_remote_requests.end();p++){
					if (p->message.request() == req){
						pending_remote_requests.begin()->connection.close();
						pending_remote_requests.pop_front();
						
						//cerr << "Cancelling!\n";
						//ns_image_server_event ev("ns_image_server_dispatcher::Falling behind in remote request queue.");
						//ev.log = false;
						//image_server.register_server_event(ev);
						break;
					}
				}
			}
			//if there are jobs to do, spawn a new thread to handle them.
			if (!pending_remote_requests.empty() && !message_handling_thread.is_running() && !image_server.exit_requested){
				message_handling_thread.run(handle_dispatcher_request,this);
			}
			lock.release();
		}
		//cerr << "Leaving dispatcher::run() Loop";

	}
	catch(ns_ex & ex){
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
	}
	catch(std::exception & e){
		ns_ex ex(e);
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
	}	

	//shut down routines
	try{		
		if (!currently_unable_to_connect_to_the_central_db)
			image_server.unregister_host();
		//close all outstanding connections and clear the request queue
		ns_acquire_lock_for_scope lock(message_handling_lock,__FILE__,__LINE__);
		for(list<ns_remote_dispatcher_request>::iterator p = pending_remote_requests.begin(); p != pending_remote_requests.end();){
			cerr << "Closing unhandled connection: " << ns_message_request_to_string(p->message.request()) << "\n";
			p->connection.close();
			p = pending_remote_requests.erase(p);
		}
		
		if(message_handling_thread.is_running()){
			message_handling_thread.block_on_finish();
		}
		lock.release();
		incomming_socket.close_socket();

		//let the capture and processing threads finish before shutting down.
		wait_for_local_jobs();
		
		if (!currently_unable_to_connect_to_the_central_db){
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
			image_server.performance_statistics.update_db(image_server.host_id(),sql());
			sql.release();
		}
		image_server.device_manager.reset_all_devices();
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Shutting down."));
	}
	catch(ns_ex & ex){
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
	}
	catch(std::exception & e){
		ns_ex ex(e);
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
	}		
//	cerr << "Exiting Dispatcher Thread.";
}

void ns_image_server_dispatcher::handle_delayed_exception(){
	if (delayed_exception != 0){
		ns_ex ex(*delayed_exception);
		ns_safe_delete(delayed_exception);
		throw ex;
	}
}
void ns_image_server_dispatcher::clear_for_termination(){
	ns_acquire_lock_for_scope lock(processing_lock,__FILE__,__LINE__);
	if (processing_thread.is_running())
		processing_thread.block_on_finish();
	if (schedule_error_check_thread.is_running())
		schedule_error_check_thread.block_on_finish();
	lock.release();
	//ns_safe_delete(processing_thread);
	ns_safe_delete(delayed_exception);
	
	ns_acquire_lock_for_scope work_lock(work_sql_management_lock,__FILE__,__LINE__);
	ns_safe_delete(work_sql_connection);
	work_lock.release();
	ns_acquire_lock_for_scope timer_lock(timer_sql_management_lock,__FILE__,__LINE__);
	ns_safe_delete(timer_sql_connection);
	timer_lock.release();
}
ns_image_server_dispatcher::~ns_image_server_dispatcher(){
	clear_for_termination();
}

void ns_image_server_dispatcher::wait_for_local_jobs(){
	
	ns_acquire_lock_for_scope lock(processing_lock,__FILE__,__LINE__);
	if (processing_thread.is_running()){
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Exit requested.  Waiting for local jobs to finish..."));
		processing_thread.block_on_finish();
	}
	lock.release();
	if(schedule_error_check_thread.is_running())
		schedule_error_check_thread.block_on_finish();
	image_server.wait_for_pending_threads();
	image_server.device_manager.wait_for_all_scans_to_complete();
	buffered_capture_scheduler.image_capture_data_manager.wait_for_transfer_finish();
}

void ns_image_server_dispatcher::start_looking_for_new_work(){
	if (image_server.server_is_paused()){
		cerr << "p";
		return;
	}
	//ns_sql * sql = image_server.new_sql_connection();
	try{
		//when a processing thread is running, its handle is stored in processing_thread.
		ns_acquire_lock_for_scope lock(processing_lock,__FILE__,__LINE__);
		if (allow_processing && !processing_thread.is_running()){
			//get a job from the server
			cerr << ".";
			processing_thread.run(thread_start_look_for_work,this);
		}
		else cerr << ":";
		lock.release();
	}
	catch(std::exception & exception){
		ns_ex ex(exception);
		throw ex;
	}
}

void ns_image_server_dispatcher::handle_central_connection_error(ns_ex & ex){
	if (!currently_unable_to_connect_to_the_central_db){	
		image_server.register_server_event_no_db(ns_image_server_event("ns_image_server_dispatcher::Lost connection to the central SQL database.  (") << ex.text() << ") Waiting for the connection to be re-established...");
		image_server.alert_handler.buffer_all_alerts_locally(false);
		currently_unable_to_connect_to_the_central_db = true;
	}
	else{
		cout << "~";
		image_server.alert_handler.buffer_all_alerts_locally(false);
		return;
	}
}


//The server is regularly poked; each time a NS_TIMER command is recieved on_timer() is run
void ns_image_server_dispatcher::on_timer(){

	if (trigger_segfault){
		char * a(0);
		(*a)++;
	}
	
	//first we handle all capture device management.  This doesn't require access to the central database
	if (image_server.act_as_an_image_capture_server()){
		try{
			run_device_capture_management();
		}
		catch(ns_ex & ex){
			try{
				image_server.register_server_event(ns_image_server::ns_register_in_local_db,ex);
			}
			catch(ns_ex & ex){
				image_server.register_server_event_no_db(ns_image_server_event(ex.text()));
			}
		}
	}


	//we want to buffer all alerts locally and only submit them all at once after all
	//tasks are completed.  This is because submitting alerts requires acquiring 
	//the cluster-wide alert table lock, and there is a possibility of this taking a long time.
	//We don't want to delay any of the on_timer() operations, especially those involved
	//in data acquisition.

	image_server.alert_handler.buffer_all_alerts_locally(true);
	ns_try_to_acquire_lock_for_scope timer_sql_lock(timer_sql_management_lock);
	if (!timer_sql_lock.try_to_get(__FILE__,__LINE__))
		return; //if we can't get the lock, it's because the another thread is already trying to reconnect.  Just give up and let the other thread deal with it.
	try{
		if (timer_sql_connection == 0){
			try{
				timer_sql_connection = image_server.new_sql_connection(__FILE__,__LINE__,0);
			}
			catch(ns_ex & ex){
				handle_central_connection_error(ex);
				timer_sql_lock.release();
				return;
			}
			image_server.register_host();
		}

		bool try_to_reestablish_connection = false;

		if (currently_unable_to_connect_to_the_central_db)
			try_to_reestablish_connection = true;
		else{
			try{
				timer_sql_connection->clear_query();
				timer_sql_connection->check_connection();
				image_server.check_for_sql_database_access(timer_sql_connection);
			}
			catch(ns_ex ex){
				handle_central_connection_error(ex);
				try_to_reestablish_connection = true;
			}
		}

		if (try_to_reestablish_connection ){
			try{
				//if we've lost the connection, try to reconnect via conventional means
				image_server.reconnect_sql_connection(timer_sql_connection);
				timer_sql_connection->check_connection();	
				timer_sql_lock.release();
				image_server.register_server_event(ns_image_server_event("Recovered from a lost MySQL connection."),timer_sql_connection);
				image_server.register_host();
				image_server.alert_handler.buffer_all_alerts_locally(false);
				currently_unable_to_connect_to_the_central_db = false;
				return;
			}
			catch(ns_ex & ex){
				//ns_safe_delete(timer_sql_connection);
				//handle_central_connection_error(ex);
				timer_sql_lock.release();
				return;
			}
		}
		timer_sql_lock.release();
	}
	catch(...){
		timer_sql_lock.release();
		throw;
	}


	try{
		//if record of this server has been deleted, update it.
		*timer_sql_connection << "SELECT id, shutdown_requested, pause_requested, hotplug_requested,database_used FROM hosts WHERE id='" << image_server.host_id()<< "'";
		ns_sql_result h;
		timer_sql_connection->get_rows(h);
		if (h.size() == 0){
			image_server.load_constants(ns_image_server::ns_image_server_type,image_server.get_multiprocess_control_options());	
			image_server.register_server_event(ns_image_server_event("Found an inconsistent host record in the db: Refreshing it."),timer_sql_connection);
			image_server.register_host();
			image_server.register_devices();
			image_server.alert_handler.buffer_all_alerts_locally(false);
			return;
		}
	
		unsigned long file_storage_space(0);
		try{
				file_storage_space = image_server.image_storage.free_space_in_volatile_storage_in_mb();
		}
		catch(ns_ex & ex){
				image_server.register_server_event(
					ns_image_server_event("Could not establish free space on server: ") << ex.text() ,timer_sql_connection);
		}
		catch (...){
				
				image_server.register_server_event(
					ns_image_server_event("Could not establish free space on server: Reason Unknown"),timer_sql_connection);
		}

		try{
			if (!image_server.image_storage.long_term_storage_was_recently_writeable(15*60)){
				ns_alert alert("Host cannot transfer local buffer to long term storage",
							   "Host cannot transfer local buffer to long term storage",
							   ns_alert::ns_long_term_storage_error,
							   ns_alert::get_notification_type(ns_alert::ns_long_term_storage_error,image_server.act_as_an_image_capture_server()),
							   ns_alert::ns_rate_limited
							   );
				image_server.alert_handler.submit_locally_buffered_alert(alert);
			}
		
			if (file_storage_space < 1024*16){
				string text(image_server.host_name_out());
				text += ": Low Disk Space: ";
				if (file_storage_space > 1024){
					text += ns_to_string(file_storage_space/1024);
					text += "Gb Remaining";
				}
				else{
					text += ns_to_string(file_storage_space);
					text += "Mb Remaining";
				}
					ns_alert alert(text,
							   text,
							   ns_alert::ns_low_disk_space_warning,
							   ns_alert::get_notification_type(ns_alert::ns_low_disk_space_warning,image_server.act_as_an_image_capture_server()),
							   ns_alert::ns_rate_limited
							   );
				image_server.alert_handler.submit_locally_buffered_alert(alert);
			}

			if (image_server.scan_for_problems_now())
				scan_for_problems(*timer_sql_connection);

		}
		catch(ns_ex & ex){
			image_server.register_server_event(ex,timer_sql_connection);
		}
	
		bool shutdown_requested = (h[0][1] == "1");
		const bool hotplug_requested = (h[0][3] == "1");
		if (h[0][3] == "2"){
			clean_clear_local_db_requested = true;
		}
		std::string database_requested = h[0][4];
		
	
		//otherwise mark the host as still being online.
		*timer_sql_connection << "UPDATE hosts SET last_ping=UNIX_TIMESTAMP(), shutdown_requested=0, hotplug_requested=0, long_term_storage_enabled= "
			<< (image_server.image_storage.long_term_storage_was_recently_writeable()?"1":"0")
			<< ", time_of_last_successful_long_term_storage_write=" << image_server.image_storage.time_of_last_successful_long_term_storage_write()
			<< ", available_space_in_volatile_storage_in_mb = " << file_storage_space;
		*timer_sql_connection << " WHERE id = " << image_server.host_id();
		timer_sql_connection->send_query();
		timer_sql_connection->send_query("COMMIT");
		if (clean_clear_local_db_requested){
			clean_clear_local_db_requested = false;		
			try{
			
				ns_acquire_for_scope<ns_local_buffer_connection> local_buffer_connection(image_server.new_local_buffer_connection(__FILE__,__LINE__));
				ns_acquire_for_scope<ns_local_buffer_connection> sql(image_server.new_local_buffer_connection(__FILE__,__LINE__));
				buffered_capture_scheduler.commit_local_changes_to_central_server(sql(),*timer_sql_connection);
				buffered_capture_scheduler.clear_local_cache(sql());
				sql.release();
				local_buffer_connection.release();
			}
			catch(ns_ex & ex){
				image_server.register_server_event_no_db(ns_image_server_event(ex.text()));
			}
		}
		try{
			image_server.image_storage.refresh_experiment_partition_cache(timer_sql_connection);
		}
		catch(ns_ex & ex){
			image_server.register_server_event(ex,timer_sql_connection);
		}
		

		map<std::string, ns_capture_device::ns_device_preview_type> preview_requested;
		if (image_server.act_as_an_image_capture_server()){
			ns_image_server_device_manager::ns_device_name_list devices;
			try{
		
				image_server.device_manager.request_device_list(devices);
				//pair<scanner id, type of preview scan requested (0=none,1=transparency unit, 2=reflective)
				*timer_sql_connection << "SELECT name,preview_requested,pause_captures, simulated_device,autoscan_interval FROM devices WHERE host_id = " << image_server.host_id();
				ns_sql_result prev_res;
				timer_sql_connection->get_rows(prev_res);
				//check to see if any unrecognized devices are marked as attached to the current host
				bool device_record_problem = false;
				for (unsigned int i = 0; i < prev_res.size(); i++){
					bool found(false);
					for (unsigned int j = 0; j < devices.size(); j++){
						if (prev_res[i][0] == devices[j].name){
							found = true;
							break;
						}
					}
					if (!found){
						device_record_problem = true;
						break;
					}
				}
				if (device_record_problem || prev_res.size() != image_server.device_manager.number_of_attached_devices()){
				//	image_server.load_constants("server",image_server.get_multiprocess_control_options());
					ns_image_server_event ev("Found an inconsistent device record in the db.");
					ev << "Devices registered:";
					for (unsigned int k = 0; k < prev_res.size(); k++)
						ev << prev_res[k][0] << ",";
					ev << "; actual devices: ";
					for (unsigned k = 0; k < devices.size(); k++)
						ev << devices[k].name << ",";
					ev << "; refreshing record.";

					image_server.register_server_event(ev,timer_sql_connection);
				//	image_server.register_host();
					image_server.register_devices();
					return;
				}

				for (unsigned int i = 0; i < prev_res.size(); i++){
					preview_requested[prev_res[i][0]] = (ns_capture_device::ns_device_preview_type)atoi(prev_res[i][1].c_str());
					try{
						int pause_requested(atoi(prev_res[i][2].c_str()));

						if (image_server.device_manager.set_pause_state(prev_res[i][0],pause_requested)){
							if (pause_requested)
								image_server.register_server_event(ns_image_server_event("Pausing device ") << prev_res[i][0],timer_sql_connection);
							else{

								image_server.register_server_event(ns_image_server_event("Un-pausing device ") << prev_res[i][0],timer_sql_connection);

							}
						}
						int autoscan_interval(atoi(prev_res[i][4].c_str()));	
						if (image_server.device_manager.set_autoscan_interval_and_balance(prev_res[i][0],autoscan_interval,*timer_sql_connection)){
							if (autoscan_interval > 0)
								image_server.register_server_event(ns_image_server_event("Setting autoscan interval to ") << autoscan_interval << " on device " << prev_res[i][0],timer_sql_connection);
							else
								image_server.register_server_event(ns_image_server_event("Stopping autoscans on device ") << prev_res[i][0],timer_sql_connection);
						}
						//handle any pending queued scans
						image_server.start_autoscans_for_device(prev_res[i][0],*timer_sql_connection);
					}
					catch(ns_ex & ex){
						image_server.register_server_event(ns_image_server_event("Error setting pause state: ") << ex.text(),timer_sql_connection);
					}
					catch(...){
						image_server.register_server_event(ns_image_server_event("Error setting pause state: ") << "Unknown error!",timer_sql_connection);
					}
				}
		
			}
			catch(ns_ex & ex){
				image_server.register_server_event(ex,timer_sql_connection);
			}
			try{
				image_server.update_device_status_in_db(*timer_sql_connection);
			}
			catch(ns_ex & ex){
				image_server.register_server_event(ex,timer_sql_connection);
			}
			try{
				const bool handle_simulated_devices(image_server.register_and_run_simulated_devices(timer_sql_connection));

				for (unsigned int i = 0; i < devices.size(); i++){
						if (image_server.exit_requested)
							break;
						if (devices[i].simulated_device && !handle_simulated_devices)
							continue;
						if (image_server.device_manager.device_is_currently_scanning(devices[i].name))
							continue;
						if (devices[i].paused)
							continue;
					
						if (preview_requested[devices[i].name] != ns_capture_device::ns_no_preview){
							if (get_whole_scanner_preview(preview_requested[devices[i].name],devices[i].name))
								ns_thread::sleep(4);//wait four seconds in between requesting scans to keep USB chatter sane.
						}

						//if there's nothing else to do, see if we can run pending transfers
						//image_capture_data_manager.handle_pending_transfers_to_long_term_storage(devices[i].name);
					}
					image_server.device_manager.run_pending_autoscans(this,*timer_sql_connection);
				}
			catch(ns_ex & ex){
				image_server.register_server_event(ex,timer_sql_connection);
			}

			try{
				image_server.update_device_status_in_db(*timer_sql_connection);
			}
			catch(ns_ex & ex){
				image_server.register_server_event(ex,timer_sql_connection);
			}
			try{
				if (hotplug_requested)
					run_hotplug();
				std::vector<std::string> device_names(devices.size());
				for (unsigned int i = 0; i < devices.size(); i++)
					device_names[i] = devices[i].name;
				buffered_capture_scheduler.image_capture_data_manager.handle_pending_transfers_to_long_term_storage(device_names);
	
			}
			catch(ns_ex & ex){
				image_server.register_server_event(ex,timer_sql_connection);
			}

		}

		try{
			bool do_not_change_pause_status(false);
			if (!image_server.server_is_paused() && image_server.new_software_release_available(*timer_sql_connection)){
				if (image_server.halt_on_new_software_release()){
					image_server.register_server_event(ns_image_server_event("A more recent version of server software was found running on the cluster.  This server is outdated and is halting now."),timer_sql_connection);
					image_server.update_software = true;
					image_server.shut_down_host();
				}
				else{
					image_server.register_server_event(ns_image_server_event("A more recent version of server software was found running on the cluster.  This server is pausing image processing."),timer_sql_connection);
					image_server.set_pause_status(true);
					*timer_sql_connection << "UPDATE hosts SET pause_requested = 1 WHERE id = " << image_server.host_id();
					timer_sql_connection->send_query();
					do_not_change_pause_status = true;
				}
			}
			if(!do_not_change_pause_status){
				image_server.set_pause_status(h[0][2] == "1");
			}
		}
		catch(ns_ex & ex){
			image_server.register_server_event(ex,timer_sql_connection);
		}
		
		if (shutdown_requested)
			image_server.shut_down_host();
		if (!allow_processing)
				cerr << "'";

		timer_sql_connection->send_query("COMMIT");

		
		//we've gotten everything we need done; we can now allow alerts to be submitted
		image_server.alert_handler.buffer_all_alerts_locally(false);

		try{
			//we launch a new thread to submit buffered alerts and send emails based on the alert handler queue
			image_server.handle_alerts();
		}
		catch(ns_ex & ex){
			cerr << "Could not handle alerts: " << ex.text() << "\n";
			if (ex.type() == ns_sql_fatal){
				image_server.register_server_event_no_db(ns_image_server_event(ex.text()));
				return;
			}
			image_server.register_server_event(ex,timer_sql_connection);
		}
		catch(...){
			cerr << "Could not handle alerts for an unknown reason.\n";
			return;
		}	
		try{
			#ifdef NS_TRACK_PERFORMANCE_STATISTICS
			image_server.performance_statistics.merge(ns_image_allocation_performance_stats);
			#endif
			image_server.performance_statistics.update_db(image_server.host_id(),*timer_sql_connection);
		}
		catch(ns_ex & ex){
			cerr << "Could not update performance statistics: " << ex.text() << "\n";
			if (ex.type() == ns_sql_fatal){
				image_server.register_server_event_no_db(ns_image_server_event(ex.text()));
				return;
			}
			image_server.register_server_event(ex,timer_sql_connection);
		}
		catch(...){
			cerr << "Could not update performance stats for an unknown reason.\n";
		}
		try{
			if (image_server.current_sql_database() != database_requested){
				try{
					ns_acquire_lock_for_scope lock(processing_lock,__FILE__,__LINE__);
					if (processing_thread.is_running()){
						image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("Database Change Requested: waiting for jobs to finish."));
						processing_thread.block_on_finish();
					}
					image_server.set_sql_database(database_requested);
					if (work_sql_connection!=0)
						work_sql_connection->select_db(image_server.current_sql_database());
					if (timer_sql_connection!=0)
					timer_sql_connection->select_db(image_server.current_sql_database());

					lock.release();

				}
				catch(...){
					timer_sql_connection->clear_query();
					*timer_sql_connection << "UPDATE hosts SET database_used = '" << image_server.current_sql_database() << "' WHERE id=" << image_server.host_id();
					timer_sql_connection->send_query();
					throw;
				}
			}

		}
		catch(ns_ex & ex){
			if (ex.type() == ns_sql_fatal){
				image_server.register_server_event_no_db(ns_image_server_event(ex.text()));
				return;
			}
			cerr << "Could not manage database settings: " << ex.text() << "\n";
			image_server.register_server_event(ex,timer_sql_connection);
		}
		catch(...){
			cerr << "Could not manage database settings for an unknown reason!\n";
		}
	}
	catch(ns_ex & ex){
		ns_image_server_event ev(ex.text());
		image_server.register_server_event_no_db(ev);
		//no rethrow.
	}
}


bool ns_image_server_dispatcher::get_whole_scanner_preview(const ns_capture_device::ns_device_preview_type preview_type,const std::string & device_name){
	
	ns_capture_thread_arguments * ca = new ns_capture_thread_arguments;
	try{
		ca->capture_specification.volatile_storage = 0;
		ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));

		sql() << "UPDATE devices SET preview_requested=0 WHERE name='" << device_name << "'";
		sql().send_query();
		sql().send_query("COMMIT");
		ca->capture_specification.capture_parameters = image_server.capture_preview_parameters(preview_type,sql());
		sql.release();
		
		image_server.register_server_event(ns_image_server::ns_register_in_central_db,ns_image_server_event("Starting preview capture on ") << device_name << "...");
		//prepare to start capture in its own thread
		ca->buffered_capture_scheduler = &buffered_capture_scheduler;
		
		//capture will need to mark job as done by setting time_at_finish
		ca->device_name = device_name;
		ca->capture_specification.capture_schedule_entry_id=0;
		ca->capture_specification.image.experiment_id = 0;
		ca->capture_specification.image.sample_id = 0;
		ca->capture_specification.image.device_name = device_name;
		ca->capture_specification.image.capture_time = 0;
		ca->capture_specification.image.experiment_name = "";
		ca->capture_specification.image.sample_name= "";
		ca->capture_specification.image.capture_images_image_id = 0;
		ca->capture_specification.speed_regulator.initialize_for_capture(0,0);

		//get directory for captured image.
		string filename = ns_format_time_string(ns_current_time()) + "=" + device_name + "=preview.tif";
		ca->capture_specification.volatile_storage = image_server.image_storage.request_miscellaneous_storage(filename);
	
		//capture thread will delete ca.
		image_server.device_manager.run_capture_on_device(ca);

		return true;
	}
	catch(std::exception & exception){
		ns_safe_delete(ca);
		ns_ex ex(exception);
		ex << "Problem during preview: ";
		//OK, something went wrong with the capture.
		//Give up for now but swallow exception so that 
		//the dispatcher can try again later.
		image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
		return false;
	}
}

bool ns_image_server_dispatcher::run_pending_captures_on_device(const std::string & device_name, ns_sql & sql){
	throw ns_ex("ERR");
}

void ns_image_server_dispatcher::process_image_request(const unsigned int image_id, const string & host_ip, const int host_port){
	//send a file to the remote host that requests it
}
//saves a recieved image according to the filename and path information
//specified in its record in "images".
ns_thread_return_type ns_image_server_dispatcher::thread_start_recieving_image(void * recieve_image_spec){
	ns_image_reciever_spec * s(static_cast<ns_image_reciever_spec *>(recieve_image_spec));
	ns_image_reciever_spec sl(*s);
	try{
		ns_safe_delete(s);
		sl.dispatcher->recieve_image_thread(sl.message);
		sl.socket_connection.close();
	}
	catch(ns_ex & ex){
		if (ex.type() != ns_sql_fatal)
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
		else cerr << "Image Recieving Thread Error: " << ex.text();
		sl.socket_connection.close();
	}
	catch(std::exception & e){
		ns_ex ex(e);
		if (ex.type() != ns_sql_fatal)
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
		else cerr << "Image Recieving Thread Error: " << ex.text();
		sl.socket_connection.close();
	}
	return 0;
}
void ns_image_server_dispatcher::recieve_image(ns_image_server_message & message,ns_socket_connection & con){
	ns_image_reciever_spec * s(new ns_image_reciever_spec(message,con,this));
	ns_thread reciever_thread(thread_start_recieving_image,s);
	reciever_thread.detach();
}

void ns_image_server_dispatcher::recieve_image_thread(ns_image_server_message & message){
	//recieve an image being sent.
	ns_socket_connection  con = message.connection();
	ns_64_bit image_id = con.read_64bit_uint();
	unsigned long bit_depth = con.read_uint();
	if (bit_depth != 8)
		throw ns_ex() << "ns_image_server_dispatcher::Cannot recieve images of bit depth " << bit_depth;

	ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));

	//look to see if a record exists specifying information about the recieved image
	ns_image_server_image image;
	bool image_found = image.load_from_db(image_id,&sql());
	if (!image_found){
		image.path = ns_image_server::orphaned_image_directory();
		image.filename = "unknown_image_id_";
		image.filename += ns_to_string(image_id);
	}
	//check to see if an image with the specified name exists in storage.
	//if it does, find a new filename under which to save the file.
	bool renamed = image_server.image_storage.assign_unique_filename(image,&sql());
	if (renamed){
		sql() << "UPDATE images SET filename = '" << sql().escape_string(image.filename) << "' WHERE id = " << image_id;
		sql().send_query();
	}
	
	//cerr << "Binding socket...\n";
	ns_image_socket_reciever<ns_8_bit> reciever;
	reciever.bind_socket(con);
	//cerr << "Getting storage...\n";
	//get access to image storage
	bool had_to_use_local_storage;
	ns_image_storage_reciever_handle<ns_8_bit> image_storage = image_server.image_storage.request_storage(image,ns_tiff,512,&sql(),had_to_use_local_storage,false,true);
	
	sql().disconnect();
	sql.release();
	//cerr << "\nRecieving Pump.\n";
	reciever.pump(image_storage.output_stream(),512);
}


//Performs the following actions
//Check if any child threads have thrown exceptions recently.
//Look for raw capture jobs locally
//Look for plate image capture jobs locally
//Look for raw capture jobs on remote machines
//Look for plate image capture jobs on remote machines.
//All the BEGINing and COMMITing maintains atomic nature
//of job picking
bool ns_image_server_dispatcher::look_for_work(){
	bool action_performed(false);
	ns_acquire_lock_for_scope sql_lock(work_sql_management_lock,__FILE__,__LINE__);
	if (work_sql_connection == 0)
		work_sql_connection = image_server.new_sql_connection(__FILE__,__LINE__);
	try{
		work_sql_connection->clear_query();
		work_sql_connection->check_connection();
	}
	catch(ns_ex & ex){
		ns_image_server_event ev;
		ev << "Lost connection to mySQL server.  Reconnecting..." << ns_ts_sql_error;
		image_server.register_server_event_no_db(ev);
		ns_safe_delete(work_sql_connection);
		work_sql_connection = image_server.new_sql_connection(__FILE__,__LINE__);
	}
	sql_lock.release();
	
	try{
		image_server.perform_experiment_maintenance(*work_sql_connection);
		//search the server for an image processing task
		const bool first_in_first_out_job_queue (image_server.get_cluster_constant_value("job_queue_is_FIFO","false",work_sql_connection)!="false");
		action_performed = job_scheduler.run_a_job(*work_sql_connection,first_in_first_out_job_queue);
		if (action_performed)
			register_succesful_operation();
		work_sql_connection->send_query("COMMIT");
		work_sql_connection->send_query("UNLOCK TABLES");
		//if we're running as a screen saver, don't hog memory when
		//the user is on the computer
		if (!image_server.run_autonomously())
			image_server.image_storage.cache.clear_memory_cache();
		job_scheduler.clear_heap();

		//ns_thread::sleep(4*1000 + 39);
		//cerr << "Done working.\n";
		//notify the dispatcher that the current job is done.
	}
	catch(...){

		//any code needed to maintain lock integrity should go here
		if (work_sql_connection!= 0){
			work_sql_connection->send_query("COMMIT");
			//con->disconnect();
			//delete con;
			//con = 0;
		}
		if (!image_server.run_autonomously())
			image_server.image_storage.cache.clear_memory_cache();
		throw;
	}
	return action_performed;
}

void ns_image_server_dispatcher::register_succesful_operation(){
	if (memory_allocation_error_count > 0)
		memory_allocation_error_count--;
}
void ns_image_server_dispatcher::handle_memory_allocation_error(){
	memory_allocation_error_count++;
	image_server.image_storage.cache.clear_memory_cache();
	if (memory_allocation_error_count > 5){
		image_server.pause_host();	
		ns_ex ex("The host has recently encountered too many memory errors.  The host will pause until futher notice.");
		image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
	}
}

ns_thread_return_type ns_asynch_start_looking_for_new_work(void * dispatcher_pointer){
	ns_image_server_dispatcher * d = reinterpret_cast<ns_image_server_dispatcher *>(dispatcher_pointer);
	d->start_looking_for_new_work();
	ns_thread::get_current_thread().detach();
	return 0;
}

ns_thread_return_type ns_image_server_dispatcher::thread_start_look_for_work(void * dispatcher_pointer){
	ns_image_server_dispatcher * d = reinterpret_cast<ns_image_server_dispatcher *>(dispatcher_pointer);
	
	bool found_work(false);
	try{
		try{
			found_work = d->look_for_work();
		}
		catch(std::exception & exception){
			ns_ex ex(exception);
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
			if (ex.type() == ns_memory_allocation)
				d->handle_memory_allocation_error();			
		}
		

		//ns_acquire_lock_for_scope lock(d->processing_lock,__FILE__,__LINE__);
		d->processing_thread.report_as_finished();
		//if we found work, immediately look for another job.
		if (found_work && !image_server.exit_requested)
			ns_thread start_looking_for_new_work(ns_asynch_start_looking_for_new_work,dispatcher_pointer);

		return 0;
	}
	catch(std::exception & exception){
		ns_ex ex(exception);
		//self.detach();
        ex << ex.text() << "(Error in processing_thread)";
			if (ex.type() == ns_memory_allocation)
				image_server.image_storage.cache.clear_memory_cache();
		
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
		image_server.ns_image_server::shut_down_host();

		d->processing_thread.report_as_finished();

		return 0;
	}
}
ns_thread_return_type ns_scan_for_problems(void * d){

	ns_image_server_dispatcher * dispatcher(static_cast<ns_image_server_dispatcher *>(d));
	ns_acquire_for_scope<ns_sql> sql;

	bool can_connect_to_central_db(true);
	try{
		sql.attach(image_server.new_sql_connection(__FILE__,__LINE__,0));
	}
	catch(...){
		can_connect_to_central_db = false;
		dispatcher->schedule_error_check_thread.report_as_finished();
		return 0;
	}

	try{		
			unsigned long current_time = ns_current_time();
			//image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Checking for missed scans on cluster."));
			
			std::vector<std::string> devices_to_suppress,
							 experiments_to_suppress;

			image_server.get_alert_suppression_lists(devices_to_suppress,experiments_to_suppress,&sql());

			string conditions_for_missed;
			conditions_for_missed = 
					"c.censored = 0 AND c.time_at_finish = 0 AND c.time_at_start = 0 "
					"AND c.problem = 0 AND c.missed = 0 AND "
					"c.scheduled_time < ";
			conditions_for_missed += ns_to_string(current_time - 60*image_server.maximum_allowed_remote_scan_delay());

			sql() << "SELECT c.id, c.scheduled_time, s.device_name, e.name FROM capture_schedule as c, capture_samples as s, experiments as e WHERE " 
				<< conditions_for_missed << " AND c.sample_id = s.id AND s.experiment_id = e.id "
				"AND c.scheduled_time > " << dispatcher->time_of_last_scan_for_problems << " ORDER BY c.scheduled_time";
			ns_sql_result missed_schedule_events;
			sql().get_rows(missed_schedule_events);

			if (missed_schedule_events.size() > 0){
		
				//if there are missed scans, update them as missed

				string summary_text("Scheduled image captures are being missed.");
				string detailed_text("Scheduled image captures are being missed:\n");
				bool found_reportable_miss(false);
				string unsuppressed_text;
				for (unsigned int i = 0; i < missed_schedule_events.size(); i++){
					string tmp(ns_format_time_string_for_human(
						atol(missed_schedule_events[i][1].c_str())) 
						+ " " + missed_schedule_events[i][2] + " " + missed_schedule_events[i][3]);
					unsuppressed_text += tmp;

					bool suppressed_by_device(false);
					bool suppressed_by_experiment(false);
					for (unsigned int j = 0; j < devices_to_suppress.size(); j++){
						if (missed_schedule_events[i][2] == devices_to_suppress[j]){
							suppressed_by_device = true;
							break;
						}
					}		
				
					for (unsigned int j = 0; j < experiments_to_suppress.size(); j++){
								if (missed_schedule_events[0][3] == experiments_to_suppress[j]){
									suppressed_by_experiment = true;
									break;
								}
					}
				
					if (suppressed_by_device)
						unsuppressed_text += "(Suppressed by device request)";
					if (suppressed_by_experiment)
						unsuppressed_text += "(Suppressed by experiment request)";
					unsuppressed_text +="\n";

					if (suppressed_by_device || suppressed_by_experiment)
						continue;
					found_reportable_miss = true;

					detailed_text += tmp + "\n";
				}

				ns_image_server_event ev;
				if (missed_schedule_events.size() == 1) ev << "The image cluster has missed a scheduled image capture:";
				else ev << "The image cluster has missed " << (unsigned long)missed_schedule_events.size() << " scheduled image captures";
				ev << unsuppressed_text; 
				image_server.register_server_event(ns_image_server::ns_register_in_central_db,ev);

				sql() << "UPDATE capture_schedule as c SET missed = 1 WHERE " << conditions_for_missed;
				sql().send_query();
				sql().send_query("COMMIT");
				if (found_reportable_miss){
					try{
						ns_alert alert(summary_text,
							detailed_text,
							ns_alert::ns_missed_capture,
							ns_alert::get_notification_type(ns_alert::ns_missed_capture,image_server.act_as_an_image_capture_server()),
							ns_alert::ns_rate_limited);
						image_server.alert_handler.submit_alert(alert,sql());
					
					}
					catch(ns_ex & ex){
						cerr << "Could not submit alert: " << summary_text << " : " << ex.text();
					}
				}
			}
			sql.release();
			dispatcher->schedule_error_check_thread.report_as_finished();
		}
	catch(ns_ex & ex){
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
	}
	catch(...){
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_ex("Unknown exception in check_for_problems thread"));
		dispatcher->schedule_error_check_thread.report_as_finished();
	}
	return 0;
}
void ns_image_server_dispatcher::scan_for_problems(ns_sql & sql){

	std::vector<std::string> devices_to_suppress,
							 experiments_to_suppress;

	image_server.get_alert_suppression_lists(devices_to_suppress,experiments_to_suppress,&sql);
		
	image_server.device_manager.scan_and_report_problems(devices_to_suppress,experiments_to_suppress,sql);
	//automatically search for new scanners
	//if(!image_server.server_is_paused() && image_server.query_cluster_for_device_names())
	//	run_hotplug(false,false);
	
	//look for missed scans anywhere on the cluster (these will start to accumulate if, for example, a node with attach devices crashes.)
	unsigned long current_time = ns_current_time();
	if (image_server.maximum_allowed_remote_scan_delay() == 0)
		image_server.update_scan_delays_from_db(&sql);

	if (image_server.maximum_allowed_remote_scan_delay() != 0){
		if (!schedule_error_check_thread.is_running()){
			//In large databases, these querries require significant resources to process
			//Coordinate to avoid running them frequently.
	
			const unsigned long last_missed_scan_check_time = atol(image_server.get_cluster_constant_value_locked("last_missed_scan_check_time","0",&sql).c_str());

			if (last_missed_scan_check_time + 60*2 > ns_current_time()){
				image_server.release_cluster_constant_lock(&sql);
			}
			else{
				image_server.set_cluster_constant_value("last_missed_scan_check_time",ns_to_string(ns_current_time()),&sql);
				image_server.release_cluster_constant_lock(&sql);
				time_of_last_scan_for_problems = last_missed_scan_check_time;
				schedule_error_check_thread.run(ns_scan_for_problems,this);
			}
		}

	}
}


void ns_image_server_dispatcher::run_device_capture_management(){
	ns_try_to_acquire_lock_for_scope device_management_lock(device_capture_management_lock);

	if (!device_management_lock.try_to_get(__FILE__,__LINE__)){
		std::cerr << "Skipping device management for this cycle because the last run hasn't finished yet.\n";
		return;
	}
	ns_acquire_for_scope<ns_local_buffer_connection> local_buffer_connection(image_server.new_local_buffer_connection(__FILE__,__LINE__));
	if (first_device_capture_run){
		buffered_capture_scheduler.get_last_update_time(local_buffer_connection());
		first_device_capture_run = false;
	}
	ns_image_server_device_manager::ns_device_name_list devices;
	image_server.device_manager.request_device_list(devices);
	if (devices.size() == 0){
		device_management_lock.release();
		return;
	}
	if (!currently_unable_to_connect_to_the_central_db){
		try{
			//update the db at 10 second intervals, if needed
			if (buffered_capture_scheduler.time_since_last_db_update().local_time >= image_server.local_buffer_commit_frequency_in_seconds()){
				ns_acquire_for_scope<ns_sql> sql;
				bool can_connect_to_central_db(true);
				try{
					sql.attach(image_server.new_sql_connection(__FILE__,__LINE__,0));
				}
				catch(...){
					can_connect_to_central_db = false;
				}
				if (can_connect_to_central_db){

					buffered_capture_scheduler.update_local_buffer_from_central_server(devices,local_buffer_connection(),sql());

				}
			}
		}
		catch(ns_ex & ex){
			image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
		}
	}

	buffered_capture_scheduler.run_pending_scans(devices,local_buffer_connection());

	local_buffer_connection.release();
	device_management_lock.release();
}

