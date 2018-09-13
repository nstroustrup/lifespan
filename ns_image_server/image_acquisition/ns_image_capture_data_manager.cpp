#include "ns_image_capture_data_manager.h"
#include "ns_image_server.h"
#include "ns_image_tools.h"
#include "ns_processing_job_push_scheduler.h"
#include <ctime>
#include "ns_high_precision_timer.h"
#include "ns_process_16_bit_images.h"

using namespace std;
void ns_image_capture_data_manager::initialize_capture_start(ns_image_capture_specification & capture_specification, ns_local_buffer_connection & sql){
	capture_specification.image.capture_images_image_id = 0;
	try{
			//if a sixteen bit image is requested, we'll have to downsample it.  Mark the initial
		//saved copy as temporary.
		if (capture_specification.capture_parameters_specifiy_16_bit(capture_specification.capture_parameters))
			capture_specification.image.specified_16_bit = true;

		//create an image in the db for the captured image
		ns_image_server_image capture_images_image(storage_handler->create_image_db_record_for_captured_image(capture_specification.image,&sql));
		capture_specification.image.capture_images_image_id = capture_images_image.id;

	

		//create a capture image record in the captured_images table
		capture_specification.image.save(&sql,ns_image_server_captured_image::ns_mark_as_busy);

		//open local file in which to write data
		capture_specification.volatile_storage = storage_handler->request_binary_output_for_captured_image(capture_specification.image,capture_images_image,true,&sql);
		
		//if pending capture exists, grab it from the DB and process it.
		sql.send_query("COMMIT");
		sql.send_query("BEGIN");
		string query =  "UPDATE buffered_capture_schedule SET time_at_start = " +ns_to_string(ns_current_time())
			+" WHERE id= \"" + ns_to_string(capture_specification.capture_schedule_entry_id) + "\"";
		//cerr << query << "\n";
		sql.send_query(query);

		sql.send_query("COMMIT");
	}
	catch(...){
		try{
			if (capture_specification.image.capture_images_image_id!= 0)
			storage_handler->delete_from_storage(capture_specification.image,ns_delete_volatile,&sql);
		}
		catch(ns_ex & ex){
			cerr << ex.text() << "\n";
		}
		sql << "UPDATE buffered_captured_images SET image_id = 0 WHERE id = " << capture_specification.image.captured_images_id;
		sql.send_query();	
		sql << "DELETE FROM buffered_images WHERE id = " << capture_specification.image.capture_images_image_id;
		sql.send_query();
		if (capture_specification.volatile_storage != 0){
			delete capture_specification.volatile_storage;
			capture_specification.volatile_storage = 0;
		}
		throw;
	}
}

void ns_image_capture_data_manager::register_capture_stop(ns_image_capture_specification & capture_specification, const ns_64_bit problem_id, ns_local_buffer_connection & sql){
	if (capture_specification.capture_schedule_entry_id == 0)
		return;
	long int current_time = ns_current_time();
	sql << "UPDATE buffered_capture_schedule SET time_at_imaging_start = " << capture_specification.time_at_imaging_start
		<< ",time_at_finish = " << capture_specification.time_at_imaging_stop
		<< ",time_spent_reading_from_device= '" << capture_specification.time_spent_reading_from_device 
		<< "',time_spent_writing_to_disk= '" << capture_specification.time_spent_writing_to_disk
		<< "',total_time_during_read= '" << capture_specification.total_time_during_read
		<< "',total_time_spent_during_programmed_delay= '" << capture_specification.total_time_spent_during_programmed_delay
		<< "'";
	for (unsigned int i = 0; i < capture_specification.speed_regulator.decile_times.size(); i++)
		sql << ",scanning_time_for_decile_" << ns_to_string(i) << "='" << capture_specification.speed_regulator.decile_times[i] << "'";
		
	sql << ", captured_image_id = " << capture_specification.image.captured_images_id 
		<< ", problem = " << problem_id;
	if (problem_id == 0){
		if (ns_image_capture_specification::capture_parameters_specifiy_16_bit(capture_specification.capture_parameters))
			sql << ", transferred_to_long_term_storage = " << (int)ns_on_local_server_in_16bit;
		else 
			sql << ", transferred_to_long_term_storage = " << (int)ns_on_local_server_in_8bit;
	}
	sql <<" WHERE id=" << capture_specification.capture_schedule_entry_id;
	//cerr << sql.query() << "\n";
	//cerr << "<";
	sql.send_query();
	sql.send_query("COMMIT");

	if (problem_id == 0){
		sql << "UPDATE buffered_capture_samples as s, buffered_capture_schedule as c SET s.raw_image_size_in_bytes = " << capture_specification.speed_regulator.total_bytes_read() 
			<< " WHERE c.id = " << capture_specification.capture_schedule_entry_id << " AND s.id = c.sample_id";
		sql.send_query();
		sql.send_query("COMMIT");
	}
}

bool ns_image_capture_data_manager::transfer_data_to_long_term_storage(ns_image_server_captured_image & image,
	ns_64_bit & time_during_transfer_to_long_term_storage,
	ns_64_bit & time_during_deletion_from_local_storage,
	const ns_vector_2<ns_16_bit> & conversion_16_bit_bounds,
	ns_transfer_behavior & behavior,
	ns_local_buffer_connection & sql){

	if (image.capture_images_image_id == 0)
		throw ns_ex("transfer_data_to_long_term_storage() was passed an image with no captured image image id");
	if (image.captured_images_id == 0)
		throw ns_ex("transfer_data_to_long_term_storage() was passed an image with no captured image id");

	bool had_to_use_local_storage(false);
	if (image.specified_16_bit){
		try{
			sql.send_query("COMMIT");
		//	cerr << "Attempting to convert open 16 bit copy...\n";
			ns_image_storage_source_handle<ns_16_bit> high_depth(storage_handler->request_from_storage_n_bits<ns_16_bit>(image,&sql,ns_image_storage_handler::ns_volatile_storage));
			image.specified_16_bit = false;
			ns_image_server_image output_image;
			ns_image_storage_reciever_handle<ns_8_bit> low_depth(storage_handler->request_storage_ci(image,ns_tiff,1.0,1024,&sql, output_image,had_to_use_local_storage,true));
			output_image.id = image.capture_images_image_id;

			ns_image_server_image small_image(image.make_small_image_storage(&sql));

			bool had_to_use_local_storage_2;
			ns_image_storage_reciever_handle<ns_8_bit> small_image_output(storage_handler->request_storage(small_image,ns_jpeg, NS_DEFAULT_JPEG_COMPRESSION, 1024,&sql,had_to_use_local_storage_2,false,true));

			ns_image_process_16_bit<ns_features_are_light, ns_image_stream_static_offset_buffer<ns_16_bit> > processor(1024);

			processor.set_small_image_output(small_image_output);
			processor.set_crop_range(conversion_16_bit_bounds);
			
			ns_image_stream_binding< ns_image_process_16_bit<ns_features_are_light, ns_image_stream_static_offset_buffer<ns_16_bit> >,
									 ns_image_storage_reciever<ns_8_bit> > binding(processor,low_depth.output_stream(),1024);
			
		//	cerr << "Attempting to write to 8 bit copy...\n";
			ns_high_precision_timer hptimer;
			hptimer.start();
			//cerr << "pump\n";
			high_depth.input_stream().pump(binding,1024);
		//	cerr << "done\n";
			time_during_transfer_to_long_term_storage = hptimer.stop();
			image.specified_16_bit = true;
			hptimer.start();
		//	cerr << "Delete";
			storage_handler->delete_from_storage(image,ns_delete_volatile,&sql);
			time_during_deletion_from_local_storage = hptimer.stop();
			image.specified_16_bit = false;
			string partition = storage_handler->get_partition_for_experiment(image.experiment_id,&sql);
		//	cerr << "Commit\n";
			
			small_image.save_to_db(0,&sql);
			output_image.save_to_db(output_image.id, &sql, false);
			image.capture_images_small_image_id = small_image.id;
			sql.send_query("COMMIT");
			
			//we no longer calculate image statistics here, as that data is not cached locally.
			//instead it is now calculated during mask application.
	
			sql << "UPDATE buffered_captured_images SET small_image_id=" << small_image.id << " WHERE id = " << image.captured_images_id;
			sql.send_query();

			sql.send_query("COMMIT");

			if (had_to_use_local_storage_2 != had_to_use_local_storage){
				image_server.register_server_event(
					ns_image_server_event("ns_image_capture_data_manager::transfer_data_to_long_term_storage()::"
					"During transfer, the small and large copies of a captured image ended up in different places: Large: ")
					<< (had_to_use_local_storage?"Local":"Long Term") << " Small: " << (had_to_use_local_storage_2?"Local":"Long Term"),&sql);
			}
		}
		catch(ns_ex  ex){
			image.specified_16_bit = true;
			if (ex.type() != ns_sql_fatal)
				sql.send_query("ROLLBACK");
			cerr << "File conversion exception(1) found: " << ex.text() << "\n";
			ns_ex file_conversion_ex(ex);
			file_conversion_ex << ns_file_io;
			throw file_conversion_ex;
		}
		catch(std::exception & e){
			cerr << "File conversion exception(2) found: " << e.what() << "\n";
			image.specified_16_bit = true;
			sql.send_query("ROLLBACK");
			throw ns_ex(e);

		}
		catch(...){
			cerr << "File conversion exception(3) found (Uknown)\n";
			image.specified_16_bit = true;
			sql.send_query("ROLLBACK");
			throw;
		}
	}
	else {
		if (behavior == ns_convert_and_compress_locally)
			throw ns_ex("Cannot transfer an 8 bit image when convert_and_compress_locally is specified");
		
		if (!storage_handler->test_connection_to_long_term_storage(true)) {
			behavior = ns_convert_and_compress_locally;
			return true;
		}

		ns_image_storage_source_handle<ns_8_bit> in(storage_handler->request_from_storage(image,&sql));
		try{
			ns_image_server_image output_image;
			ns_image_storage_reciever_handle<ns_8_bit> out(storage_handler->request_storage_ci(image,ns_tiff,1.0,1024,&sql,output_image,had_to_use_local_storage,false));
			output_image.id = image.capture_images_image_id;
			if (had_to_use_local_storage)
				return had_to_use_local_storage;
			in.input_stream().pump(out.output_stream(),1024);
			storage_handler->delete_from_storage(image,ns_delete_volatile,&sql);
			output_image.save_to_db(output_image.id, &sql, false);
			string partition = storage_handler->get_partition_for_experiment(image.experiment_id,&sql);
			sql.send_query("COMMIT");
		}
		catch(ns_ex & ex){
			cerr << "\nCould not move 8 bit copy because : " << ex.text() << "\n";
			throw;
		}

	}
	return had_to_use_local_storage;
}


void ns_image_capture_data_manager::transfer_image_to_long_term_storage(const std::string & device_name,ns_64_bit capture_schedule_entry_id, ns_image_server_captured_image & image, ns_transfer_behavior & behavior, ns_local_buffer_connection & sql){
	ns_acquire_lock_for_scope device_lock(device_transfer_state_lock,__FILE__,__LINE__);
	if (device_transfer_in_progress(device_name)){
		transfer_status_debugger.set_status(device_name,ns_transfer_status("Device was busy"));
		device_lock.release();
		return;
	}
	transfer_status_debugger.set_status(device_name,ns_transfer_status("Starting transfer attempt"));
	set_device_transfer_state(true,device_name);
	device_lock.release();
	try{
		transfer_image_to_long_term_storage_locked(capture_schedule_entry_id,image,behavior,sql);
		device_lock.get(__FILE__,__LINE__);
		set_device_transfer_state(false,device_name);
		device_lock.release();
	}
	catch (ns_ex & ex){
		device_lock.get(__FILE__,__LINE__);
		set_device_transfer_state(false,device_name);
		device_lock.release();
		transfer_status_debugger.set_status(device_name,ns_transfer_status(std::string("Error:") + ex.text()));
		throw;
	}
	catch(...){

		device_lock.get(__FILE__,__LINE__);
		set_device_transfer_state(false,device_name);
		device_lock.release();
		throw;
	}
}

void ns_image_capture_data_manager::transfer_image_to_long_term_storage_locked(ns_64_bit capture_schedule_entry_id, ns_image_server_captured_image & image, ns_transfer_behavior & behavior, ns_local_buffer_connection & sql) {

	//First we check what state the current image is in
	sql << "SELECT transferred_to_long_term_storage, sample_id FROM buffered_capture_schedule WHERE id = " << capture_schedule_entry_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_capture_data_manager::transfer_image_to_long_term_storage()::Could not find capture schedule id ") << capture_schedule_entry_id << " in the db.";
	ns_64_bit sample_id = ns_atois64(res[0][1].c_str());
	//cerr << "Coordinating Transfer\n";
	ns_capture_image_status transfer_status((ns_capture_image_status)atoi(res[0][0].c_str()));
	switch (transfer_status) {
	case ns_not_finished:
		throw ns_ex("ns_image_capture_data_manager::transfer_image_to_long_term_storage()::Transfer requested on image whose capture has not yet finished.");
	case ns_transferred_to_long_term_storage:
		throw ns_ex("ns_image_capture_data_manager::transfer_image_to_long_term_storage()::Transfer requested on image that has already been transferred.");
	case ns_on_local_server_in_16bit:
		image.specified_16_bit = true;
		break;
	case ns_on_local_server_in_8bit:
		image.specified_16_bit = false;
		break;
	default:
		throw ns_ex("ns_image_capture_data_manager::transfer_image_to_long_term_storage()::Requested transfer subject is in unknown transfer state:") << (int)transfer_status;
	}
	if (behavior == ns_convert_and_compress_locally && transfer_status == ns_on_local_server_in_8bit) {
		transfer_status_debugger.set_status(image.device_name, ns_transfer_status("Waiting to transfer locally cached 8 bit image"));
		return;	//give up; there is nothing useful to do with 8 bit images until we can transfer them to the long term storage.
	}

	sql << "SELECT conversion_16_bit_low_bound, conversion_16_bit_high_bound FROM buffered_capture_samples WHERE id = " << sample_id;
	ns_sql_result res2;
	sql.get_rows(res2);
	if (res2.size() == 0)
		throw ns_ex("Could not identify sample ") << sample_id;
	ns_vector_2<ns_16_bit> conversion_16_bit_bounds(atoi(res2[0][0].c_str()), atoi(res2[0][1].c_str()));
	if (conversion_16_bit_bounds.y == 0) {
		conversion_16_bit_bounds.x = 0;  //DEFAULT crop bounds
		conversion_16_bit_bounds.y = 200;  //DEFAULT crop bounds
	}

	ns_64_bit time_during_transfer_to_long_term_storage;
	ns_64_bit time_during_deletion_from_local_storage;

	bool had_to_use_local_storage;
	had_to_use_local_storage = transfer_data_to_long_term_storage(image, time_during_transfer_to_long_term_storage, time_during_deletion_from_local_storage, conversion_16_bit_bounds, behavior, sql);

	transfer_status = ns_transferred_to_long_term_storage;
	if (had_to_use_local_storage) {
		transfer_status = ns_on_local_server_in_8bit;
		transfer_status_debugger.set_status(image.device_name, ns_transfer_status("Converted to 8 bit and stored locally"));
	}
	else
		transfer_status_debugger.set_status(image.device_name,ns_transfer_status("Successfully transferred image to long term storage"));
	

	sql << "UPDATE buffered_capture_schedule SET transferred_to_long_term_storage = " << (int)transfer_status;
	if (!had_to_use_local_storage){
		sql //<< ", problem = 0"
			<< ", time_during_transfer_to_long_term_storage = '" << time_during_transfer_to_long_term_storage
			<< "', time_during_deletion_from_local_storage = '" << time_during_deletion_from_local_storage << "'";
	}
	//else sql << ", problem = 1";
	sql <<" WHERE id = " << capture_schedule_entry_id;
	sql.send_query();

	if (!had_to_use_local_storage){	
		image_server.alert_handler.reset_alert_time_limit(ns_alert::ns_long_term_storage_error,&sql);
		image_server.alert_handler.reset_alert_time_limit(ns_alert::ns_volatile_storage_error,&sql);
	}

	//mark capture sample being finished if its transfer is complete
	if (had_to_use_local_storage)
		image.save(&sql,ns_image_server_captured_image::ns_mark_as_busy);
	else image.save(&sql,ns_image_server_captured_image::ns_mark_as_not_busy);

	sql.send_query("COMMIT");
	
	if (!had_to_use_local_storage){
		captured_image_list_lock.wait_to_acquire(__FILE__, __LINE__);
		newly_captured_images_for_which_to_schedule_jobs.push_back(image);
		captured_image_list_lock.release();
	}
}

struct ns_hptlts_arguments{
	std::vector<std::string> device_names;
	ns_image_capture_data_manager * capture_data_manager;
};

void ns_image_capture_data_manager::wait_for_transfer_finish(){
	ns_acquire_lock_for_scope lock(pending_transfers_lock,__FILE__,__LINE__);	
	if (!pending_transfers_thread.is_running()){
		lock.release();
		return;
	}
	lock.release();
	pending_transfers_thread.block_on_finish();
}

ns_thread_return_type ns_image_capture_data_manager::thread_start_handle_pending_transfers_to_long_term_storage(void * thread_arguments){

	ns_thread current_thread(ns_thread::get_current_thread());
	current_thread.set_priority(NS_THREAD_LOW);
	ns_hptlts_arguments * arg(static_cast<ns_hptlts_arguments *>(thread_arguments));
	ns_thread_return_type ret = 0;
	
	ns_transfer_behavior transfer_behavior = ns_try_to_transfer_to_long_term_storage;
	if (!arg->capture_data_manager->storage_handler->test_connection_to_long_term_storage(true))
		transfer_behavior = ns_convert_and_compress_locally;

	for (unsigned int i = 0; i < arg->device_names.size(); i++){
		try{
		
			ns_acquire_lock_for_scope device_lock(arg->capture_data_manager->device_transfer_state_lock,__FILE__,__LINE__);
			if (arg->capture_data_manager->device_transfer_in_progress(arg->device_names[i])){
				device_lock.release();
					
				arg->capture_data_manager->transfer_status_debugger.set_status(arg->device_names[i],ns_transfer_status("Transfer already in progress"));
				continue;
			}
			arg->capture_data_manager->set_device_transfer_state(true,arg->device_names[i]);
			device_lock.release();

			unsigned long ret(arg->capture_data_manager->handle_pending_transfers(arg->device_names[i], transfer_behavior));
			if (ret == 1) //sql error.  Stop trying for this round.
				break;
		}
		catch(ns_ex & ex){
			cerr << "\nns_image_capture_data_manager::thread_start_handle_pending_transfers_to_long_term_storage()::" << ex.text() << "\n";
		}
		catch(...){
			cerr << "\nns_image_capture_data_manager::thread_start_handle_pending_transfers_to_long_term_storage():: error!\n";
		}
	
		ns_acquire_lock_for_scope device_lock(arg->capture_data_manager->device_transfer_state_lock,__FILE__,__LINE__);
		arg->capture_data_manager->set_device_transfer_state(false,arg->device_names[i]);
		device_lock.release();
	}
	

	//ns_acquire_lock_for_scope pending_transfers_lock(arg->capture_data_manager->pending_transfers_lock,__FILE__,__LINE__);
	arg->capture_data_manager->pending_transfers_thread.report_as_finished();
	//pending_transfers_lock.release();

	ns_safe_delete(arg);
	return ret;
}


unsigned long ns_image_capture_data_manager::handle_pending_transfers(const string & device_name,ns_transfer_behavior & behavior){
			
	//cerr << "\nHandling Pending Transfers\n";
	//return 0;
	try{
		//we want to avoid opening lots of sql connections, so we do the initial check for pending transfers
		//on a shared connection.  Only if we find images needing to be transferred do we make a new sql connection.
		ns_acquire_lock_for_scope sql_lock(check_sql_lock,__FILE__,__LINE__);
		if (check_sql != 0){
			try{
				check_sql->clear_query();
				check_sql->check_connection();
			}
			catch(...){
				ns_safe_delete(check_sql);
			}
		}
		if (check_sql==0)
			check_sql = image_server.new_local_buffer_connection_no_lock_or_retry(__FILE__,__LINE__);
		if (check_sql == 0){
			sql_lock.release();
			return 1;
		}
	
		unsigned long current_time = ns_current_time();

		//We don't have to worry about concurrency problems, as a device is owned by only one cluster node.
		check_sql->send_query("BEGIN");
		*check_sql << "SELECT cs.id, cs.captured_image_id, cs.scheduled_time, "
			"cs.sample_id, cs.transferred_to_long_term_storage "
			 << " FROM buffered_capture_schedule as cs, buffered_capture_samples as s "
			 << "WHERE s.device_name = '" << device_name
			 << "' AND s.id = cs.sample_id"
			 << " AND cs.time_at_start != 0"
			 << " AND cs.time_at_finish != 0"
			 << " AND cs.problem = 0"
			 << " AND (cs.transferred_to_long_term_storage = " << (int)ns_on_local_server_in_16bit
			 << "      || cs.transferred_to_long_term_storage = " << (int)ns_on_local_server_in_8bit << ") ";
	//	std::string q(check_sql->query());
		ns_sql_result events;
		check_sql->get_rows(events);
		sql_lock.release();
		//do not use check_sql after this lock is released!

		if (events.size() > 0){
			ns_acquire_for_scope<ns_local_buffer_connection> sql(image_server.new_local_buffer_connection(__FILE__,__LINE__));
			for (unsigned int i = 0; i < events.size(); i++){
				if (!storage_handler->test_connection_to_long_term_storage(true))
					behavior = ns_convert_and_compress_locally;
				if (image_server.exit_has_been_requested)
					break;
			
				ns_image_server_event ev;
				if (behavior == ns_try_to_transfer_to_long_term_storage)
					ev << "Processing a pending image transfer to long term storage: " << device_name << "@" << ns_format_time_string_for_human(atol(events[i][2].c_str()));
				else if (behavior == ns_convert_and_compress_locally)
					ev << "No access to long term storage; compressing captured image and caching locally: " << device_name << "@" << ns_format_time_string_for_human(atol(events[i][2].c_str()));

				image_server.register_server_event(ev,&sql());
				ns_image_server_captured_image im;
				im.captured_images_id = ns_atoi64(events[i][1].c_str());
				im.load_from_db(im.captured_images_id,&sql());
				ns_64_bit capture_schedule_id = ns_atoi64(events[i][0].c_str());
				try{
					transfer_image_to_long_term_storage_locked(capture_schedule_id,im, behavior,sql());
				}
				catch(ns_ex & ex){
					cerr << "Error processing capture: " << ex.text() << "\n";
					cerr << "Details: ";
					for (unsigned int k = 0; k < events[i].size(); k++)
						cerr << events[i][k] << ", ";
					cerr << "\n";
					ns_64_bit problem_id(0);
					try{
						problem_id = image_server.register_server_event(ex,&sql());
				
					}
					catch(...){}

					if(problem_id == 0)
						problem_id = 1;
					sql() << "UPDATE buffered_capture_schedule SET problem = " << problem_id << " WHERE id = " << capture_schedule_id;
					sql().send_query();	
					sql() << "UPDATE buffered_captured_images SET problem = " << problem_id << " WHERE id = " << im.captured_images_id;
					sql().send_query();
					sql().send_query("COMMIT");
				}
				catch(...){
					cerr << "Error!\n";
					ns_64_bit problem_id(image_server.register_server_event(ns_ex("ns_image_capture_data_manager::handle_pending_transfers()::Unknown Transfer Problem"),&sql()));
					if(problem_id == 0)
						problem_id = 1;
					sql() << "UPDATE buffered_capture_schedule SET problem = " << problem_id << " WHERE id = " << capture_schedule_id;
					sql().send_query();	
					sql() << "UPDATE buffered_captured_images SET problem = " << problem_id << " WHERE id = " << im.captured_images_id;
					sql().send_query();
					sql().send_query("COMMIT");
				}
			}
			sql.release();
		}
	}
	catch(ns_ex & ex){
		try{
			ns_image_server_event ev("ns_image_capture_data_manager::");
			ev << ex.text();
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ev);
		}
		catch(...){
			cerr << "\nns_image_capture_data_manager::Could not register server event " << ex.text() << "\n";
		}
	}
	catch(std::exception & e){
		ns_ex ex(e);
		try{
			ns_image_server_event ev("ns_image_capture_data_manager::");
			ev << ex.text();
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ev);
		}
		catch(...){
			cerr << "\nns_image_capture_data_manager::Could not register server event " << ex.text() << "\n";
		}
	}
	catch(...){
		cerr << "\nns_image_capture_data_manager::An unknown exception occured.\n";
	}
	return 0;
}
ns_image_capture_data_manager::~ns_image_capture_data_manager(){
	if (check_sql != 0)
		ns_safe_delete(check_sql);
}


void ns_image_capture_data_manager::set_device_transfer_state(const bool state,const std::string & name){
	device_transfer_state[name] = state;
}
bool ns_image_capture_data_manager::device_transfer_in_progress(const std::string & name){
	std::map<string,bool>::iterator p = device_transfer_state.find(name);
	if (p == device_transfer_state.end()){
		device_transfer_state[name] = false;
		return false;
	}
	else return p->second;
}

bool ns_image_capture_data_manager::handle_pending_transfers_to_long_term_storage_using_db_names(){
	if (!storage_handler->test_connection_to_long_term_storage(true))
		return false;
	ns_acquire_for_scope<ns_local_buffer_connection> sql(image_server.new_local_buffer_connection(__FILE__,__LINE__));
	sql() << "SELECT DISTINCT s.device_name FROM buffered_capture_samples as s, buffered_capture_schedule as sh "
			 "WHERE sh.id = s.id";
	ns_sql_result res;
	sql().get_rows(res);
	sql.release();
	std::vector<std::string> device_names(res.size());
	for (unsigned int i = 0; i < res.size(); i++)
		device_names[i] = res[i][0];
	return handle_pending_transfers_to_long_term_storage(device_names);

}

bool ns_image_capture_data_manager::handle_pending_transfers_to_long_term_storage(const std::vector<std::string> & device_names ){
	//cerr << "Handling transfer for " << device_name << "\n";
	//NOTE! we originally checked to see if we're connected to long term storage here,
	//but one failure mode is that the function connected_to_long_term_storage()
	//starts being extremely slow.  hence we have moved that out of the main thread
	//if (!storage_handler->connected_to_long_term_storage())
	//	return false;

	//now we check to see if any threads are doing device-specific operations

	//first we check to see if any other threads are looking for pending jobs
	ns_acquire_lock_for_scope pt_lock(pending_transfers_lock,__FILE__,__LINE__);
	
	if (pending_transfers_thread.is_running()){
		pt_lock.release();
		return false;
	}

	ns_hptlts_arguments * arg = new ns_hptlts_arguments;
	try{
		arg->capture_data_manager = this;
		arg->device_names.resize(device_names.size());
		std::copy(device_names.begin(),device_names.end(),arg->device_names.begin());
		pending_transfers_thread.run(thread_start_handle_pending_transfers_to_long_term_storage,arg);
		pt_lock.release();
		return true;
	}
	catch(...){
		ns_safe_delete(arg);
		pt_lock.release();
		throw;
	}
}
