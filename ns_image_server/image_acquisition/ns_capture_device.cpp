#include "ns_ex.h"
#include "ns_capture_device.h"
#include "ns_high_precision_timer.h"
#include "ns_sql.h"
#include "ns_thread.h"
#include "ns_image_server_dispatcher.h"
#include "ns_asynchronous_read.h"
#include <iomanip>
#include "ns_usb.h"
using namespace std;


#define capture_buffer_size 1024*1024
class ns_scanner_list_compiler{
public:
	ns_scanner_list_compiler():delayed_ex_thrown(false){}

	void get_scanner_list(const string & scanner_command, vector<string> & scanner_names){	
		
		ns_external_execute exec;
		string output;

		ns_asynchronous_read_info<ns_scanner_list_compiler> read_info;
		ns_external_execute_options opt;
		opt.binary = true;

		ns_acquire_lock_for_scope lock(ns_global_usb_lock,__FILE__,__LINE__,false);
		if (ns_use_global_usb_lock)
			lock.get(__FILE__,__LINE__);

		exec.run(scanner_command, "-q", opt);
		//exec.finished_writing_to_stdin();
		//read from stdio asynchronously.
	
		//cerr << "Running exec";
		ns_asynchronous_read asynchronous_read(&exec);
		read_info.asynch = &asynchronous_read;
		read_info.reciever = this;
	
		ns_thread stderr_thread(asynchronous_read_start<ns_scanner_list_compiler>,&read_info);
		 
		int i = -1;
		unsigned int total_bytes_read = 0;
		char * buf = new char[capture_buffer_size+1];
		bool first_scan(true);
		try{
			//read from stdout
			total_bytes_read = 0;
			while (true){

			//	cerr << "Reading from stdout...\n";
				i = exec.read_stdout(buf,capture_buffer_size);
				if (!first_scan){
					if (ns_use_global_usb_lock)
						lock.release();
					first_scan = false;
				}
				buf[i] = 0;
				total_bytes_read += i;
				if (i == 0){
			//		cerr << "Done reading from stdout";
					break;
				}
				output+= buf;
			}
		//	cerr << "finishing stdout read";
			delete[] buf;
			buf = 0;
			#ifdef WIN32
			//THE NEXT FOUR LINES ARE A HACK
			exec.finished_reading_from_stdout();
			if (total_bytes_read != 0){
				ns_thread::sleep(1);
				exec.finished_reading_from_stderr();
			}
			#endif
			//For some reason the asynchronous read doesn't stop even once the child process
			//terminates.  So we assume that one second after the child has finished writing to cout,
			//it's not going to write to stderr and so we kill it.

		}
		catch(std::exception & exception){
			ns_ex ex(exception);
			//if an exception occurs hold off until the asynchronous stderr thread finishes.
			//Throwing an exception before then will orphan the sterr object as its pointers
			//back to this object will be destroyed.
			if (buf != 0)
				delete[] buf;
			//cerr << ex.text()<< "\n";
			
			throw_delayed_exception(ns_ex("Main Read Thread::") << ex.text());
			//even though we need to wait for it to stop, let's hurry things up a little bit by breaking stderr's pipe.
			exec.finished_reading_from_stderr();
		}
	//	cerr << output;
		//wait at most ten seconds
		//exec.finished_reading_from_stderr();
	//	cerr << "Blocking on stderr_thread finish";
		stderr_thread.block_on_finish(0);
		//this will close stderr, which hasn't been done yet.
		exec.release_io();
		exec.wait_for_termination();
		//check to see if any errors occurred during asynchronous read.
		if (delayed_ex_thrown)
			throw delayed_ex;

		if (asynchronous_read.result().size() != 0){
			string res = asynchronous_read.result();
			if (res.size() != 0)
				throw ns_ex("Error reading scanner list: '") << res << "'";
		}
		string current_line;
		for (unsigned int i = 0; i < output.size(); i++){
			if (output[i] == '\n'){
				string::size_type pos = current_line.find_last_of(" ");
				if (pos != current_line.npos){
					scanner_names.push_back(current_line.substr(pos+1));
				}
				current_line = "";
			}
			else current_line+=output[i];
		}
	}

	void throw_delayed_exception(const ns_ex & ex){
		delayed_ex_thrown = true;
		delayed_ex = ex;
	}
private:
	ns_ex delayed_ex;
	bool delayed_ex_thrown;
};

void ns_get_scanner_hardware_address_list(vector<string> & scanner_names){
	ns_scanner_list_compiler comp;
	comp.get_scanner_list(image_server.scanner_list_command(),scanner_names);
}

bool ns_image_capture_specification::capture_parameters_specifiy_16_bit(const string & s){
	string::size_type i = s.find("16");
	if (i == s.npos)
		return false;
	if (i != s.size()-1 && s[i+2] != ' ') //if the next character isn't a space
		return false;
	string::size_type j = s.rfind("depth",i);  //if depth wasn't specified
	if (j== s.npos)
		return false;
	for (string::size_type k = j+5; k < i; k++)
		if (s[k] != '=' && s[k] != '"' && s[k] != ' ')
			return false;
	return true;

}
template<class ns_component>
class ns_output_simulated_scan_image{
public:
	static void create(const string & filename){
		ns_image_whole<ns_component> im;
		ns_image_properties prop;
		prop.height = 666;
		prop.width = 1000;
		prop.components = 1;
		prop.resolution = 3200;
		im.prepare_to_recieve_image(prop);
		const ns_component white(-1);
		for (unsigned int y = 0; y < prop.height; y++)
			for (unsigned int x = 0; x < prop.width; x++)
				im[y][x] = 0;
		for (unsigned int y = 0; y < prop.height; y++)
				im[y][y] = white;
		for (unsigned int y = 0; y < prop.height; y++)
				im[prop.height - y - 1][100+y] = white;
		for (unsigned int x = 0; x < prop.width; x++)
			im[prop.height/2][x] = white;
		for (unsigned int y = 0; y < prop.height; y++)
			im[y][800] = white;

		ns_image_storage_reciever_handle<ns_component> out(image_server.image_storage.request_volatile_storage<ns_component>(filename,1024,false));
		im.pump(out.output_stream(),1024);
	}
};
#ifndef NS_MINIMAL_SERVER_BUILD
void ns_capture_device::capture(ns_image_capture_specification & c){	
	//run capture
	if (device_flags== ns_capture_device::ns_simulated_device){
		c.total_time_during_read = 0;
		c.time_spent_writing_to_disk = 0;
		c.time_spent_reading_from_device = 0;
		c.total_time_spent_during_programmed_delay = 0;
		ns_high_precision_timer total_clock;
		ns_high_precision_timer read_clock;
		ns_high_precision_timer write_clock;


		c.time_at_imaging_start = ns_current_time();
		total_clock.start();
		if (c.capture_parameters.find("error") != string::npos || c.capture_parameters.find("Flatbed") != std::string::npos)
			throw ns_ex("The simulated device has prodced a simulated error!");

		std::string output_filename(name);
		output_filename += "=";
		output_filename += "simulator_scan_source.tif";

		if (c.image.specified_16_bit)
			ns_output_simulated_scan_image<ns_16_bit>::create(output_filename);
		else ns_output_simulated_scan_image<ns_8_bit>::create(output_filename);
		
		ns_acquire_lock_for_scope lock(ns_global_usb_lock,__FILE__,__LINE__,false);
		if (ns_use_global_usb_lock)
			lock.get(__FILE__,__LINE__);

		ifstream * in = image_server.image_storage.request_from_volatile_storage_raw(output_filename);
		unsigned long total_bytes_read=0;
		try{
			char a;
			read_clock.start();
			write_clock.start();
			
			while(true){
				a = in->get();
				if (total_bytes_read==0){
					if (ns_use_global_usb_lock)
						lock.release();
				}
				if (in->fail())
					break;
				total_bytes_read++;
				if (c.volatile_storage != 0)
					*c.volatile_storage << a;
			}
			for (unsigned int i = 0; i < 15; i++){
				cerr << "(-)";
				ns_thread::sleep(1);
			}
			c.time_spent_reading_from_device+=read_clock.stop();
			c.time_spent_writing_to_disk	+=write_clock.stop();

			if (c.volatile_storage != 0){
				
				c.time_at_imaging_stop = ns_current_time();
				c.volatile_storage->close();
				ns_safe_delete(c.volatile_storage);
			}
			in->close();
			delete in;
		}
		catch(...){
			delete in;
			throw;
		}
		
		cerr << "\n" << std::fixed << std::setprecision(2) << (float)total_bytes_read/(float)(1024*1024) << "MB read from [" << this->name << "]\n";
		return;
	}
	string parameters;
	//specifiy scanimage hardware name
	#ifdef WIN32
	parameters += "-d \"";
	parameters += hardware_alias + "\"";
	#else
	parameters += "-d ";
	parameters += hardware_alias + " ";
        #endif
	//specify scan parameters
	parameters += c.capture_parameters;

	
	ns_external_execute exec;

	ns_asynchronous_read_info<ns_capture_device> read_info;
	
	ns_external_execute_options opt;
	opt.binary = true;
	exec.run(image_server.capture_command(), parameters, opt);

	//exec.finished_writing_to_stdin();
	//read from stdio asynchronously.
	ns_asynchronous_read asynchronous_read(&exec);
	
	read_info.asynch = &asynchronous_read;
	read_info.reciever = this;

	ns_thread stderr_thread(asynchronous_read_start<ns_capture_device>,&read_info);

	int i = -1;
	unsigned int total_bytes_read = 0;
	string result;
	string error;
	char * buf = new char[capture_buffer_size];
	unsigned int counter = 50;
	char b('-');
	if(this->name.size() > 0)
		b=this->name[0];

	c.total_time_during_read = 0;
	c.time_spent_writing_to_disk = 0;
	c.time_spent_reading_from_device = 0;
	c.total_time_spent_during_programmed_delay = 0;
	ns_high_precision_timer total_clock;
	ns_high_precision_timer read_clock;
	ns_high_precision_timer write_clock;
	try{
		//read from stdout
		total_bytes_read = 0;
		//cerr << "Reading from stdout...\n";
		try{
			while (true){
				if (total_bytes_read != 0){
					read_clock.start();
				}
				i = exec.read_stdout(buf,capture_buffer_size);
				
				if (total_bytes_read == 0){
					c.speed_regulator.register_start();
					c.time_at_imaging_start = ns_current_time();
					total_clock.start();
				}
				else
					c.time_spent_reading_from_device += read_clock.stop();
				
				c.speed_regulator.register_data_as_recieved(i);
				total_bytes_read += i;
				if (i == 0){
					c.total_time_during_read = 	total_clock.stop();
					c.time_at_imaging_stop = ns_current_time();
					c.speed_regulator.register_stop();
					break;
				}
				if (c.volatile_storage != 0){
					write_clock.start();
					c.volatile_storage->write(buf,i);
					c.time_spent_writing_to_disk += write_clock.stop();
				}
				c.total_time_spent_during_programmed_delay += c.speed_regulator.run_delay_if_necessary();
			
				if (counter >= 50){
					counter = 0;
					cerr << '<' << b << '>';
				}
				counter++;
			}
		}
		catch(...){
			c.time_at_imaging_stop = ns_current_time();
			throw;
		}
		exec.finished_reading_from_stdout();
		delete[] buf;
		buf = 0;
	    
		if (c.volatile_storage != 0){
			c.volatile_storage->close();
			ns_safe_delete(c.volatile_storage);
		}
		
		cerr << "\n" << std::fixed << std::setprecision(2) << (float)total_bytes_read/(float)(1024*1024) << "MB read from [" << this->name << "]\n";
		//THE NEXT FOUR LINES ARE A HACK
		#ifdef WIN32
		if (total_bytes_read != 0){
			ns_thread::sleep(1);
			exec.finished_reading_from_stderr();
		}
		#endif
		//For some reason the asynchronous read doesn't stop even once the child process
		//terminates.  So we assume that one second after the child has finished writing to cout,
		//it's not going to write to stderr and so we kill it.

	}
	catch(std::exception & exception){
		ns_ex ex2(exception);
		ns_ex ex(" The command: " );
		ex << parameters << " generated an error:" << ex2.text(); 
		//if an exception occurs hold off until the asynchronous stderr thread finishes.
		//Throwing an exception before then will orphan the sterr object as its pointers
		//back to this object will be destroyed.
		stderr_thread.block_on_finish();
		if (buf != 0)
			delete[] buf;
		if (c.volatile_storage != 0){
			c.volatile_storage->close();
			c.volatile_storage = 0;
		}
		
		//cerr << ex.text()<< "\n";
		throw_delayed_exception(ex);
		//even though we need to wait for it to stop, let's hurry things up a little bit by breaking stderr's pipe.
		exec.finished_reading_from_stderr();
	}
       
	stderr_thread.block_on_finish();
	exec.finished_reading_from_stderr();
	//this will close stderr, which hasn't been done yet.
	//cerr << "Waiting";
	exec.release_io();
	exec.wait_for_termination();
       
	//check to see if any exceptions were thrown during asynchronous read.
	handle_delayed_exception();
	
	if (asynchronous_read.result().size() != 0){
		string res = asynchronous_read.result();
		//ignore resize errors
		if (res.find("scanimage: rounded value of") == res.npos){
		  ns_ex ex("ns_capture_device: Scanimage failed on device \"");
		  ex << this->name << "\" with the following error: \"" << asynchronous_read.result() << "\"";
		  ex << ".  The command was :" << parameters;
		  throw ex;

		}
		else
			cerr << "[" << res << "]\n";
	
	}
	
}
	//cerr << "Capture returned " << ":" << result << "\n";
	//ns_thread::sleep(3*500 + rand()%1000);
void ns_capture_device::turn_off_lamp(){
	ns_image_capture_specification spec;
	spec.volatile_storage = 0;
	spec.capture_parameters = " -l 0in -t 0in -x 250 -y 250 --mode=Gray --speed=yes --format=tiff --source=\"Flatbed\" --resolution=100 --depth=8 ";
	spec.capture_schedule_entry_id = 0;
	spec.image.captured_images_id = 0;
	capture(spec);
}


bool ns_capture_device::send_hardware_reset() const{
	if (is_simulated_device())
		return true;
	ns_device_hardware_location hl(hardware_location());
	//cerr << "Sending reset signal to " << name << ":" << hardware_alias << " (" << hl.bus << "," << hl.device_id <<"\n";

	try{
		ns_acquire_lock_for_scope lock(ns_global_usb_lock,__FILE__,__LINE__,false);
		if (ns_use_global_usb_lock)
			lock.get(__FILE__,__LINE__);

		bool success(ns_usb_control::reset_device(hl.bus,hl.device_id));

		lock.release();
		if (!success)
			cerr << "Failed to send reset signal to device " << name << " (" << hardware_alias << ")\n";
		return false;
	}
	catch(ns_ex & ex){
		cerr << "Failed to send reset signal to device " << name << " (" << hardware_alias << "): " << ex.text() << "\n";
		return false;
	}
}
ns_device_hardware_location ns_capture_device::hardware_location() const{
	std::string::size_type p(hardware_alias.find("epson2:libusb:"));
	if (p == std::string::npos)
		throw ns_ex("Could not parse hardware alias for device '") << name << "': '" << hardware_alias << "' (header)\n";
	unsigned in_bus(1);
	std::string bus,device_id;
	for (int i = 14; i < (int)hardware_alias.size(); ++i){
		if(in_bus){
			if (hardware_alias[i] == ':'){
				in_bus = 0;
				continue;
			}
			bus+=hardware_alias[i];
		}
		else{
			device_id+=hardware_alias[i];
		}
	}
	if (bus.size() == 0 || device_id.size() == 0)
			throw ns_ex("Could not parse hardware alias for device '") << name << "': '" << hardware_alias << "' (data)\n";
	ns_device_hardware_location l;
	l.bus = atoi(bus.c_str());
	l.device_id = atoi(device_id.c_str());
	return l;
}




void ns_image_capture_device_speed_regulator::initialize_for_capture(const unsigned long total_number_of_expected_bytes_, 
							const unsigned long desired_scan_duration_){

	total_number_of_expected_bytes = total_number_of_expected_bytes_;
	desired_scan_duration = desired_scan_duration_;
	warnings.clear();
//	cerr << "Speed regulator has recieved a request for an image of " << total_number_of_expected_bytes/1000 << " kb to take "
//		 << desired_scan_duration << " seconds.\n";
}

void ns_image_capture_device_speed_regulator::register_data_as_recieved(unsigned long bytes){
	

	bytes_read_so_far+=bytes;
	packets_recieved++;
	//if we had a bad estimate of the total number of expected bytes, then we've overshot
	//the timing and don't want to inject any more delay.
	if (total_number_of_expected_bytes <= bytes_read_so_far ||
		bytes_read_so_far == 0 || packets_recieved < 10){
		pending_delay = 0;
		return;
	}

	//see if we've finished a decile.  If so, record it.
	ns_64_bit current_time(scan_duration.absolute_time()/1000);  //in ms

	const ns_64_bit total_capture_duration_in_ms(current_time-start_time);

	unsigned long cur_decile(bytes_read_so_far/(total_number_of_expected_bytes/10));
	if (cur_decile > 0 && cur_decile < 10 && decile_times[cur_decile] == 0)
		decile_times[cur_decile-1] = total_capture_duration_in_ms;
	

	//calculate how much delay to inject.

	//cerr << "The capture has run " << total_capture_duration_in_ms << "s so far.\n";

	const ns_64_bit ts(total_capture_duration_in_ms-delay_time_injected);  //in milliseconds
	//cerr << ts << "ms of that has been spent scanning.\n";

	const ns_64_bit total_time(1000*(ns_64_bit)desired_scan_duration),
			  remaining_scanner_time(ts*total_number_of_expected_bytes/bytes_read_so_far),
			  injected_delay_time(delay_time_injected+pending_delay);

	const ns_64_bit total_desired_remaining_delay_in_ms=total_time-remaining_scanner_time-injected_delay_time;

	if (total_time < remaining_scanner_time + injected_delay_time){
			//	cerr << "We've overshot!  No more delays.\n";
				pending_delay = 0;
				return;
	}

	ns_64_bit delay_for_current_chunk(
		(bytes*total_desired_remaining_delay_in_ms)/
								((ns_64_bit)(total_number_of_expected_bytes - (bytes_read_so_far - bytes)))
			);
	
	if (!ns_regulate_scanner_speed){
		previous_pending_delay = 0;
		pending_delay = 0;
		return;
	}
	//cerr << "Thus the current chunk needs " << delay_for_current_chunk << " ms delay\n";
	previous_pending_delay = pending_delay;
	pending_delay += delay_for_current_chunk;
}
void ns_image_capture_device_speed_regulator::register_start(){
	start_time = scan_duration.absolute_time()/1000;
//	stop_time = 0;
	bytes_read_so_far = 0;
	delay_time_injected = 0;
	pending_delay = 0;
	decile_times.resize(0);
	decile_times.resize(10,0);
	packets_recieved = 0;
	previous_pending_delay = 0;
	warnings.clear();
}
void ns_image_capture_device_speed_regulator::register_stop(){

	ns_64_bit current_time(scan_duration.absolute_time()/1000);  //in ms

	const ns_64_bit total_capture_duration_in_ms(current_time-start_time);
	decile_times[9] = total_capture_duration_in_ms;

//	stop_time = ns_current_time();
}
ns_64_bit ns_image_capture_device_speed_regulator::run_delay_if_necessary(){
	//if (pending_delay != 0)
	//	cerr << "Pending delay: " << pending_delay << "\n";
	//delays of less than 60 milliseconds will be pretty unreliable.  Group them together.
	if (pending_delay < 200)
		return 0;
	//if we are delaying less than a second, use the microsecond delay
	if (pending_delay < 1000){
		const ns_64_bit time_to_delay=pending_delay;
		delay_time_injected+=pending_delay;
		pending_delay = 0;
		//cerr << "Injecting a " << time_to_delay << " millisecond delay";
		ns_thread::sleep_microseconds(1000*time_to_delay);
	//	cerr << ".\n";
		return time_to_delay;
	}
	//if we are delaying more than a second, use the second delay.
	else{
		const unsigned long seconds_to_sleep(pending_delay/1000);
		if (seconds_to_sleep > 15){
			ns_64_bit current_time(scan_duration.absolute_time()/1000);  //in ms

			const ns_64_bit total_capture_duration_in_ms(current_time-start_time);
			ns_ex ex("Yikes! Speed regulation requested a ");
			ex << seconds_to_sleep << "delay.  Details: "
			<< this->bytes_read_so_far << " bytes read so far. " 
			<< (unsigned long)this->delay_time_injected << " ms of delay injected. " 
			<< (unsigned long)this->desired_scan_duration << " second scan requested. "
			<< (unsigned long)this->previous_pending_delay << " ms delay previously pending " 
			<< (unsigned long)this->pending_delay << " ms delay now pending " 
			<< (unsigned long)total_capture_duration_in_ms << " total capture time. "
			<< this->total_number_of_expected_bytes << " total number of expected bytes.";
			warnings.push_back(ex.text());
			pending_delay = 0;
			return 0;
		}
	//	cerr << "Injecting a " << seconds_to_sleep << " second delay";
		ns_thread::sleep(seconds_to_sleep);
	//	cerr << ".\n";
		pending_delay-=1000*seconds_to_sleep;
		delay_time_injected+=1000*seconds_to_sleep;
		return 1000*seconds_to_sleep;
	}
}


#endif
