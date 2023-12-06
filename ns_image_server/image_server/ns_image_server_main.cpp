#include "ns_ex.h"
#include "ns_image_server.h"
#include "ns_image.h"
#include "ns_jpeg.h"
#include "ns_tiff.h"
#include "ns_socket.h"
#include "ns_thread.h"
#include "ns_image_server.h"
#include "ns_image_server_message.h"
#include "ns_image_server_dispatcher.h"
#ifndef NS_ONLY_IMAGE_ACQUISITION
#include "ns_spatial_avg.h"
#endif
#include "ns_ini.h"
#include <time.h>
#include "ns_dir.h"
#include <vector>
#include <string>
#include "ns_thread_pool.h"


#ifdef _WIN32
#include <windows.h>
#include <windowsx.h>
#include <shellapi.h>
#include "resource_server.h"
#else
#include <sys/types.h>
#include <sys/stat.h>
#endif
#include "ns_ex.h"



using namespace std;



//send NS_TIMER requests every /interval/ seconds forever until exit_requested global is set
ns_thread_return_type timer_thread(void * inter){
	unsigned int * interval_p = reinterpret_cast<unsigned int *>(inter);
	unsigned long interval = *interval_p;
	delete interval_p;

	if (interval == 0)
		interval = 15;
	try{
		ns_thread::sleep(interval);
		unsigned int error_count(0);
		unsigned long last_ping_time(0);
		while(true){
			image_server.exit_lock.wait_to_acquire(__FILE__,__LINE__);
				if (image_server.exit_happening_now){
					image_server.exit_lock.release();
					break;
				}
			image_server.exit_lock.release();
			unsigned long current_time = ns_current_time();
			if (current_time - last_ping_time > interval){
				last_ping_time = current_time;
				try{

       				cout.flush();
					ns_socket cli;
					ns_socket_connection con = cli.connect("127.0.0.1", image_server.dispatcher_port());
					ns_image_server_message m(con);
					//if the server should always be on the lookout for processing jobs,
					//check for new jobs regularly
					if (image_server.run_autonomously())
						m.send_message(NS_LOCAL_CHECK_FOR_WORK,"");
					//otherwise, just "ping" the server so it keeps its records in order.
					else
						m.send_message(NS_TIMER,"");
					con.close();
					error_count = 0;
				}
				catch(ns_ex & ex){
					ns_ex ex2("Event in timer thread:(");
					ex2 << error_count << "):";
					ex2 << ex.text();
					ex2 << ex.type();
					image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
					if (error_count >= 10)
						throw ex;
					error_count++;
				}
			}
			ns_thread::sleep(1);
		}
		return 0;
	}
	catch(ns_ex & ex){
		ns_ex ex2("Fatal error in timer thread: ");
		ex2 << ex.text();
		image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex2);
		return 0;
	}
}



#ifdef _WIN32
//from How To Obtain a Console Window Handle (HWND)
//http://support.microsoft.com/kb/124103
HWND GetConsoleHwnd(void)
{
	#define MY_BUFSIZE 1024 // Buffer size for console window titles.
	HWND hwndFound;         // This is what is returned to the caller.
	char pszNewWindowTitle[MY_BUFSIZE]; // Contains fabricated
								   // WindowTitle.
	char pszOldWindowTitle[MY_BUFSIZE]; // Contains original
								   // WindowTitle.

	// Fetch current window title.

	GetConsoleTitle(pszOldWindowTitle, MY_BUFSIZE);

	// Format a "unique" NewWindowTitle.

	wsprintf(pszNewWindowTitle,"%d/%d",
		   GetTickCount(),
		   GetCurrentProcessId());

	// Change current window title.

	SetConsoleTitle(pszNewWindowTitle);

	// Ensure window title has been updated.

	Sleep(40);

	// Look for NewWindowTitle.

	hwndFound=FindWindow(NULL, pszNewWindowTitle);

	// Restore original window title.

	SetConsoleTitle(pszOldWindowTitle);

	return(hwndFound);
}
NOTIFYICONDATA notify_icon_info;
HMENU toolbar_menu;


bool ns_window_hidden = false;
HWND ns_window_to_hide;

void destroy_icons(){
	Shell_NotifyIcon (NIM_DELETE, &notify_icon_info);
}



void set_hostname_on_menu(const string & hostname){
	ModifyMenu(toolbar_menu,ID_CONTEXTMENU_HOSTNAME,MF_BYCOMMAND,MF_STRING | MF_DISABLED,hostname.c_str());
}
#endif
void ns_shutdown_dispatcher(){
	cerr << "Attempting controlled shutdown...\n";
	#ifdef _WIN32
			ModifyMenu(toolbar_menu,ID_CONTEXTMENU_IMAGESERVER,MF_BYCOMMAND,MF_STRING | MF_DISABLED,"Shutting down...");
			ModifyMenu(toolbar_menu,ID_CONTEXTMENU_STROUSTR,MF_BYCOMMAND,MF_STRING | MF_GRAYED,"Updating database, please be patient.");
	#endif
	image_server.shut_down_host();
}

#ifdef _WIN32

void ns_show_popup_menu(HWND hWnd){
HMENU submenu = GetSubMenu(toolbar_menu, 0);
					POINT pt;
					GetCursorPos(&pt);
				//	ClientToScreen(hWnd, (LPPOINT) &pt);
					TrackPopupMenu(submenu,TPM_RIGHTALIGN | TPM_BOTTOMALIGN,pt.x,pt.y,0,hWnd,0);

}
unsigned int quit_counter = 0;
//http://msdn2.microsoft.com/en-us/library/bb384843(VS.90).aspx
LRESULT CALLBACK handle_windows_message(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam){
	switch (message)
		{
		case WM_PAINT:
			return 0;
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		case WM_QUIT:
			PostQuitMessage(0);
			return 0;
		case WM_COMMAND:{
			WORD id = LOWORD(wParam);
			switch(id){
				case ID_CONTEXTMENU_HIDE:
					if (!ns_window_hidden)	ShowWindow(ns_window_to_hide,SW_HIDE);
					else  ShowWindow(ns_window_to_hide,SW_SHOW);
						ns_window_hidden = !ns_window_hidden;
					break;
				case ID_CONTEXTMENU_QUIT:
					if (!image_server.exit_happening_now){
						//if we throw an exception here, we'll start unwinding the stack and kick the foundation out of any
						//processing job threads that are running during the shutdown
						try{
							ShowWindow(ns_window_to_hide,SW_HIDE);
							ns_window_hidden = true;
							ns_shutdown_dispatcher();
						}
						catch(ns_ex & ex){
							image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ex);
						}
						catch(...){
							image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_ex("Unknown error in handle_windows_message() callback during shutdown."));
						}
					}
					else{
						quit_counter++;
						if (quit_counter > 1){
							cerr << "Forced Quit.\n";
							destroy_icons();
							exit(1);
						}
					}
				break;
			}
			return 0;
		}
		case WM_USER + 1:{
			switch(lParam){
				case WM_LBUTTONDOWN:
					ns_show_popup_menu(hWnd);
					break;
				case WM_LBUTTONUP:
					break;
				case WM_LBUTTONDBLCLK:break;
				case WM_MBUTTONDOWN :break;
				case WM_MBUTTONUP:break;
				case WM_MBUTTONDBLCLK:break;
				case WM_RBUTTONDOWN:
					ns_show_popup_menu(hWnd);

				case WM_RBUTTONUP:

				break;
				case WM_RBUTTONDBLCLK:
					printf("\a");

				break;
			}
			return 0;
			}
		default:
			return DefWindowProc(hWnd, message, wParam, lParam);
		}
}

struct run_dispatcher_t{
	ns_image_server_dispatcher * dispatcher;
	bool restarting_after_crash;
	HWND window_to_close;
};

ns_thread_return_type run_dispatcher(void * dispatcher){
	run_dispatcher_t * d = static_cast<run_dispatcher_t *>(dispatcher);
	d->dispatcher->run();

	SendMessage(d->window_to_close,WM_QUIT,0,0);
	return true;
}

void windows_message_loop(){

	MSG msg;
	BOOL bRet;
	while((bRet = GetMessage(&msg, 0, 0, 0)) != 0){
		if (bRet == -1)
			break;

		TranslateMessage(&msg);
		DispatchMessage(&msg);
		if(msg.message == WM_QUIT || msg.message == WM_DESTROY)
			break;
	}
	return;
}

HWND create_message_window(HINSTANCE hInstance){
	HINSTANCE hCurrentInstance = hInstance;//GetModuleHandle(NULL);
	WNDCLASSEX wex = {0};
	wex.cbSize = sizeof(WNDCLASSEX);
	wex.lpfnWndProc = handle_windows_message;
	wex.hInstance = NULL;
	wex.lpszClassName = "NS_IM_SERV";
	if((RegisterClassEx(&wex)) == NULL)
		throw ns_ex("ns_image_server::Could not register message handling window class");

	HWND wnd = CreateWindow("NS_IM_SERV", NULL, 0, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, HWND_MESSAGE, NULL, hInstance,  NULL);
	if(wnd == NULL)
		throw ns_ex("ns_image_server::Could not create message handling window");
	return wnd;
}

void destroy_message_window(HWND wnd){
	DestroyWindow(wnd);
	UnregisterClass("NS_IM_SERV", GetModuleHandle(NULL));
}

void handle_icons(HINSTANCE hInstance, HWND current_window){
	notify_icon_info.cbSize = sizeof(NOTIFYICONDATA);
	notify_icon_info.hWnd = current_window;
	notify_icon_info.uID = 100 ;
	notify_icon_info.uFlags = NIF_MESSAGE | NIF_ICON | NIF_TIP ;
	notify_icon_info.uCallbackMessage = WM_USER + 1;
	strcpy ( notify_icon_info.szTip, "Lifespan Machine Server");

	// save the pointer to the icon list and set the initial
	// default icon.
	notify_icon_info.hIcon = LoadIcon(hInstance,MAKEINTRESOURCE(113));
	Shell_NotifyIcon (NIM_ADD,&notify_icon_info);

	toolbar_menu = LoadMenu(hInstance,MAKEINTRESOURCE(105));


	if ( !SetWindowLong(current_window, GWL_EXSTYLE, GetWindowLong(current_window,GWL_EXSTYLE) | WS_EX_TOOLWINDOW )){
		ns_ex ex;
		ex.append_windows_error();
		//throw ex;
	}
}


#endif
#ifdef _WIN32
ns_os_signal_handler_return_type exit_signal_handler(ns_os_signal_handler_signal s){
#else
ns_os_signal_handler_return_type exit_signal_handler(ns_os_signal_handler_signal signal,siginfo_t *info, void * context){
#endif
	if (!image_server.exit_has_been_requested){
		try{
		ns_shutdown_dispatcher();
		image_server.os_signal_handler.set_signal_handler(ns_interrupt,exit_signal_handler);
		}
		catch(ns_ex & ex){
			cerr << ex.text() << "\n";
		}
		return ns_os_signal_handler_return_value;
	}
	else{
		cerr << "Forcing shutdown.\n";
		exit(1);
	}
}
#ifndef _WIN32
class ns_image_server_crash_daemon{
public:
  typedef enum{ns_ok,ns_crash_daemon_triggered_a_restart} ns_crash_daemon_result;
	 ns_crash_daemon_result start(int daemon_port){
		port = daemon_port;
		if (daemon_is_running()){
		  image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("The Daemon is already running.\n"));
		  return ns_ok;
		}

		//on linux, fork and return the apropriate options
		ns_external_execute_options opts;
		opts.take_stderr_handle = false;
		opts.take_stdin_handle = false;
		opts.take_stdout_handle = false;
		opts.only_fork = true;
		ns_external_execute exec;
		bool is_parent_thread(exec.run("","",opts));
		if (is_parent_thread)
			return ns_ok; //continue program flow
		split_from_parent_process();
		ns_socket socket;
		image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Starting Crash Daemon..."));
		socket.listen(port,1024);
		bool stop_daemon(false),restart_server(false);
		while(!stop_daemon){
			ns_socket_connection socket_connection;
			try{
				socket_connection = socket.accept();
			}
			catch(ns_ex & ex){
			  image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("The Crash Daemon encountered an error: ") << ex.text());
				break;
			}
			catch(...){
			  image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("The Crash Daemon encountered an unknown error with the socket."));
			  break;
			}
			ns_image_server_message message(socket_connection);
			message.get();
			ns_message_request req(message.request());
			image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("Crash Daemon received the following request:") << ns_message_request_to_string(req));
			switch(req){
				case NS_QUIT:{
					stop_daemon = true;

				}
				case NS_NULL: break;
				case NS_RELAUNCH_SERVER:
				  //	launch_new_server();
					stop_daemon = true;
					restart_server = true;
			}
			socket_connection.close();
		}
		socket.close_socket();
		if (!restart_server){
			image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("The Crash Daemon is shutting down"));

			exit(0);
		}
		else{
		  image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("The Crash Daemon is morphing into a replacement image server!"));
		  return ns_crash_daemon_triggered_a_restart;
		}
	};
  static bool daemon_is_running(){
    try{
      send_message_to_daemon(NS_NULL);
      return true;
    }
      catch(...){
	return false;
      }
  }
	static void request_restart_from_daemon(){
		send_message_to_daemon(NS_RELAUNCH_SERVER);
	}
	static void request_daemon_shutdown(){
		send_message_to_daemon(NS_QUIT);
	}

	private:
	static void send_message_to_daemon(const ns_message_request m){
		ns_socket socket;
		ns_socket_connection socket_connection;
		try{
			socket_connection = socket.connect("localhost",port);
		}
		catch(ns_ex & ex){
			throw ns_ex("Could not contact crash daemon: ") << ex.text();
		}
		catch(...){
			throw ns_ex("Could not contact crash daemon for unknown reasons.");
		}
		ns_image_server_message message(socket_connection);
		message.send_message(m,"");
		socket_connection.close();
	}
	static void split_from_parent_process(){
		setsid();
	}
	static void launch_new_server(){
	  image_server.register_server_event(ns_image_server::ns_register_in_central_db_with_fallback,ns_image_server_event("The Crash Daemon is starting a new server instance."));
		ns_external_execute_options opts;
		opts.take_stderr_handle = false;
		opts.take_stdin_handle = false;
		opts.take_stdout_handle = false;
		opts.only_fork = true;
		ns_external_execute exec;
		bool is_parent_thread(exec.run("","",opts));
		if (is_parent_thread)
			return;
		split_from_parent_process();
		execlp("ns_image_server","crash_restart",(char*)NULL);
	}
	static int port;
};
int ns_image_server_crash_daemon::port=0;

void ns_server_panic_restart(){
	system("killall -s SIGKILL scanimage");
	try{
		ns_image_server_crash_daemon::request_restart_from_daemon();
	}
	catch(ns_ex & ex){
		cerr << "Could not contact restart daemon: " << ex.text();
	}
	catch(...){
		cerr << "Could not contact restart daemon for an unknown reason.";
	}
	exit(1);

}
ns_os_signal_handler_return_type segmentation_fault_signal_handler(ns_os_signal_handler_signal signal,siginfo_t *info, void * context){
	ns_server_panic_restart();
}
ns_os_signal_handler_return_type bus_error_signal_handler(ns_os_signal_handler_signal signal,siginfo_t *info, void * context){
	ns_server_panic_restart();
}
ns_os_signal_handler_return_type illegal_instruction_signal_handler(ns_os_signal_handler_signal signal,siginfo_t *info, void * context){
	ns_server_panic_restart();
}
ns_os_signal_handler_return_type floating_point_error_signal_handler(ns_os_signal_handler_signal signal,siginfo_t *info, void * context){
	ns_server_panic_restart();
}
#endif


void ns_terminate(){
	return;
}

typedef enum {ns_none,ns_start, ns_stop, ns_help, ns_restart, ns_status, ns_hotplug,
			  ns_reset_devices,ns_reload_models,ns_submit_experiment,ns_test_email,ns_test_alert, ns_test_rate_limited_alert,ns_wrap_m4v,
			  ns_restarting_after_a_crash,ns_trigger_segfault_in_main_thread,ns_trigger_segfault_in_dispatcher_thread, ns_run_pending_image_transfers,
	      ns_clear_local_db_buffer_cleanly,ns_clear_local_db_buffer_dangerously,ns_simulate_central_db_connection_error,ns_fix_orphaned_captured_images,
	ns_update_sql,ns_output_image_buffer_info,ns_stop_checking_central_db,ns_start_checking_central_db,ns_output_sql_debug, ns_additional_host_description,
	ns_max_run_time_in_seconds, ns_number_of_processing_cores, ns_idle_queue_check_limit, ns_max_memory_to_use,ns_ini_file_location, ns_ignore_multithreaded_jobs,ns_create_and_configure_sql_db,ns_override_sql_db, 
	ns_simulate_long_term_storage_connection_error, ns_max_number_of_jobs_to_process} ns_cl_command;

#ifndef NS_ONLY_IMAGE_ACQUISITION
#ifdef NS_USE_INTEL_IPP
#include "ipp.h"
#endif
#include "ns_gaussian_pyramid.h"
bool ns_test_ipp() {

	ns_gaussian_pyramid p;
	ns_image_standard im;
	const unsigned long d(400);
	im.init(ns_image_properties(d, d, 1));
	for (int i = 0; i < d; i++)
		for (int j = 0; j < d; j++)
			im[i][j] = 0;
	p.calculate(im,ns_vector_2i(0,0),ns_vector_2i(d,d));
	return p.num_current_pyramid_levels > 0;
}
#endif
int main(int argc, char ** argv){
	#ifndef NS_ONLY_IMAGE_ACQUISITION
	#ifdef NS_USE_INTEL_IPP
	ippInit();
	#endif
	#endif


	std::map<std::string,ns_cl_command> commands;
	commands["start"] = ns_start;
	commands["stop"] = ns_stop;
	commands["help"] = ns_help;
	commands["status"] = ns_status;
	commands["restart"] = ns_restart;
	commands["hotplug"] = ns_hotplug;
	commands["reset_devices"] = ns_reset_devices;
	commands["reload_models"] = ns_reload_models;
	commands["submit_experiment"] = ns_submit_experiment;
	commands["test_email"] = ns_test_email;
	commands["test_alert"] = ns_test_alert;
	commands["test_rate_limited_alert"] = ns_test_rate_limited_alert;
	commands["wrap_m4v"] = ns_wrap_m4v;
	commands["crash_restart"] = ns_restarting_after_a_crash;
	commands ["trigger_segfault"] = ns_trigger_segfault_in_main_thread;
	commands["trigger_dispatcher_segfault"] = ns_trigger_segfault_in_dispatcher_thread;
	commands["run_pending_image_transfers"] = ns_run_pending_image_transfers;
	commands["clear_local_db_buffer_cleanly"] = ns_clear_local_db_buffer_cleanly;
	commands["clear_local_db_buffer_dangerously"] = ns_clear_local_db_buffer_dangerously;
	commands["simulate_central_db_connection_error"] = ns_simulate_central_db_connection_error;
	commands["fix_orphaned_captured_images"] = ns_fix_orphaned_captured_images;
	commands["output_image_buffer_info"] = ns_output_image_buffer_info;
	commands["start_checking_central_db"] = ns_start_checking_central_db;
	commands["stop_checking_central_db"] = ns_stop_checking_central_db;
	commands["update_sql"] = ns_update_sql;
	commands["additional_host_description"] = ns_additional_host_description;
	commands["max_run_time_in_seconds"] = ns_max_run_time_in_seconds;
	commands["max_number_of_jobs_to_process"] = ns_max_number_of_jobs_to_process;
	commands["output_sql_debug"] = ns_output_sql_debug;
	commands["number_of_processor_cores_to_use"] = ns_number_of_processing_cores;
	commands["override_sql_db"] = ns_override_sql_db;
	commands["max_memory_to_use"] = ns_max_memory_to_use;
	commands["idle_queue_check_limit"] = ns_idle_queue_check_limit;
	commands["ignore_multicore_jobs"] = ns_ignore_multithreaded_jobs;
	commands["ini_file_location"] = ns_ini_file_location;
	commands["create_and_configure_sql_db"] = ns_create_and_configure_sql_db;
	commands["simulate_long_term_storage_connection_error"] = ns_simulate_long_term_storage_connection_error;

	ns_ex command_line_usage;

	command_line_usage << "Usage: " << argv[0] << " [Option] [value] [Option] [value]";

	command_line_usage << "\nOptions:\n"
		<< "**Basic server control functions**\n"
		<< "create_and_configure_sql_db [schema filename]: Configure sql databases for first time use.\n"
		<< "start:  start an instance of the image server on the local machine (default)\n"
		<< "status: check to see if an instance of the image server is running on the local machine\n"
		<< "stop:   request that the currently running local instance of the image server terminate\n"
		<< "restart:request that the currently running local instance of the image server terminate,\n"
		"        and launch a new instance in current process\n"
		<< "hotplug:request the currently running local instance of the image server check for hardware\n"
		"        changes to image acquisition devices attached to local machine\n"
		"ini_file_location [value]: explicitly specify the location and filename of an ns_image_server.ini\n"
		<< "\n**Runtime control limits**\n"
		<< "max_run_time_in_seconds [value]: Specify a maximum time to run; useful for running on a HPC cluster)\n"
		<< "max_number_of_jobs_to_process [value]:  Specify a maximum number of jobs to process; (useful for\n"
		"     running on a HPC cluster)\n"
		<< "idle_queue_check_limit [value]: specify a maximum number of times to check an idle processing queue\n"
		"     before giving up and shutting down.  Overrides the value in ns_image_server.ini of \n"
		"     number_of_times_to_check_empty_processing_job_queue_before_stopping\n"
		<< "number_of_processor_cores_to_use [value] : specify the number of processing cores to use, overriding\n"
		"      the value of number_of_processing_nodes specified in the ns_image_server.ini file\n"
		"max_memory_to_use [value]:  Specify an ideal memory allocation limit, in megabytes overriding the value \n"
		"      in ns_image_server.ini\n"
		"ignore_multicore_jobs: Do not run multi-core jobs such as movement analysis\n"
#ifndef _WIN32
		"daemon: run as a background process\n"
#endif

		<< "\n**Advanced control functions**\n"
		<< "stop_checking_central_db: Cease attempting to connect to the central db.\n"
		<< "start_checking_central_db: Restart attempts to connect to the central db.\n"
		<< "output_sql_debug: request that a running server output its sql debug information\n"
		<< "reset_devices : request the currently running local instance of the image server reset its\n"
		"       image acquisition device registry and build it from scratch\n"
		<< "reload_models : request the currently running local instance of the image server clear its\n"
		"       cache of worm detection models so they are reloaded from disk.\n"
		<< "additional_host_description [text]: optionally specify extra commentary to describe the \n"
		"       current host (e.g when running in an HPC cluster)\n"
		<< "run_pending_image_transfers: Start up the server, transfer any pending images to the central\n"
		"       file server, and shut down\n"
		<< "clear_local_db_buffer_cleanly: Clear all information from the local database after \n"
		"       synchronizing it with the central db.\n"
		<< "clear_local_db_buffer_dangerously: Clear all information from the local database without\n"
		"       synchronizing.\n"
		<< "fix_orphaned_captured_images: Go through the volatile storage and fix database records for \n"
		"       images orphaned by a previous bug in the lifespan machine software\n"
		<< "update_sql [optional database]: update the sql database schema to match the most recent version. \n"
		"       No changes are made if the schema is already up-to-data. A database name can be specified\n"
		<< "override_sql_db [database]: explictly specify the database to use, overriding the \n"
		"        central_sql_databases value in the ns_image_server.ini file\n"

		<< "\n**Redundant, test, or debug functions**\n"
		<< "submit_experiment: Test and submit an experiment specification XML file to the cluster\n"
		<< "       By default this only outputs a summary of the proposed experiment to stdout\n"
		<< "       With sub-option 'u': actually submit the experiment specification to the database \n"
		<< "       With sub-option 'f' (which implies 'u'): force overwriting of existing experiments\n"
		<< "       With sub-option 'a' (which implies 'u'): extend current experiment using submitted schedule\n"
		<< "       Options combine, e.g. au\n"
		<< "test_email : send a test alert email from the current node\n"
		<< "test_alert : send a test alert to be processed by the cluster\n"
		<< "test_rate_limited_alert : send a test alert to be processed by the cluster\n"
		<< "wrap_m4v:  wrap the specified m4v stream in an mp4 wrapper with multiple frame rates.\n"
		<< "trigger_segfault: Trigger a segfault to test the crash daemon\n"
		<< "trigger_dispatcher_segfault: Trigger a segfault in the dispatcher to test the crash daemon\n"
		<< "simulate_central_db_connection_error: Simulate a broken connection to the central database.\n"
		<< "simulate_long_term_storage_connection_error: Simulate a lost connection to long term file storage.\n"
		<< "output_image_buffer_info: Output information about the state of each scanner's locally \n"
			"       buffered images.\n";

	std::string schema_name, ini_file_location, schema_filename;
	bool no_schema_name_specified(false);
	unsigned long max_run_time(0), max_job_num(0), number_of_processing_cores(-1), idle_queue_check_limit(-1), memory_allocation_limit(-1);

	try {

		ns_sql::load_sql_library();
		

		//set default options for command line arguments
		ns_cl_command command(ns_start);
		bool upload_experiment_spec_to_db(false);
		ns_experiment_capture_specification::ns_handle_existing_experiment schedule_submission_behavior = ns_experiment_capture_specification::ns_stop;
		std::string input_filename;
		std::string override_sql_db;
		bool sql_update_requested(false);
		bool schema_installation_requested(false);
		ns_cl_command post_dispatcher_init_command(ns_none);
		bool restarting_after_crash(false);

		//first, look for commandline arguments that modify the server behavior.
		//there can be multiple arguments like this.
		for (int i = 1; i < argc; i++) {
			std::string command_str(argv[i]);



			//run as background daemon if requested.
			if (command_str == "daemon") {
#ifdef _WIN32
				throw ns_ex("The image server cannot explicity start as dameon under windows.");
#else
				if (::daemon(1, 0) != 0)
					throw ns_ex("Could not launch as a daemon!");
				continue;
#endif
			}
			std::map<std::string, ns_cl_command>::iterator p = commands.find(command_str);
			if (p == commands.end() || p->second == ns_help) {
				ns_ex ex;
				if (p == commands.end())
					ex << "Unknown command line argument: " << command_str << "\n";
				ex << command_line_usage.text();
				throw ex;
			}
			else if (p->second == ns_ignore_multithreaded_jobs) {
				image_server.do_not_run_multithreaded_jobs = true;
			}
			//grab extra information for arguments that are longer than one argument
			else if (p->second == ns_wrap_m4v) {
				if (i + 1 >= argc) throw ns_ex("M4v filename must be specified");
				input_filename = argv[i + 1];
				i++;
			}
			else if (p->second == ns_override_sql_db) {
					if (i + 1 >= argc) throw ns_ex("database name must be specified");
					override_sql_db = argv[i + 1];
					i++;
			}
			else if (p->second == ns_update_sql) {
				sql_update_requested = true;
				if (i + 1 == argc)  //default
					no_schema_name_specified = true;
				else {
					schema_name = argv[i + 1];
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_create_and_configure_sql_db) {
				schema_installation_requested = true;
				if (i + 1 == argc)  //default
					;
				else {
					schema_filename = argv[i + 1];
					std::cout << "Using schema file " << schema_filename << "\n";
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_additional_host_description) {
				if (i + 1 == argc)  //default
					throw ns_ex("if additional_host_description is specified, a value must be provided");
				else {
					std::string additional_host_description = argv[i + 1];
					image_server.set_additional_host_description(additional_host_description);
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_max_number_of_jobs_to_process) {
				if (i + 1 == argc)  //default
					throw ns_ex("if max_number_of_jobs_to_process is specified, a value must be provided");
				else {
					std::string  tmp = argv[i + 1];
					max_job_num = atol(tmp.c_str());
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_max_run_time_in_seconds) {
				if (i + 1 == argc)  //default
					throw ns_ex("if max_run_time_in_seconds is specified, a value must be provided");
				else {
					std::string tmp = argv[i + 1];
					max_run_time = atol(tmp.c_str());
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_number_of_processing_cores) {
				if (i + 1 == argc)  //default
					throw ns_ex("if number_of_processing_cores is specified, a value must be provided");
				else {
					std::string tmp = argv[i + 1];
					number_of_processing_cores = atol(tmp.c_str());
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_idle_queue_check_limit) {
				if (i + 1 == argc)  //default
					throw ns_ex("if ns_idle_queue_check_limit is specified, a value must be provided");
				else {
					std::string tmp = argv[i + 1];
					idle_queue_check_limit = atol(tmp.c_str());
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_max_memory_to_use) {
				if (i + 1 == argc)  //default
					throw ns_ex("if ns_max_memory_to_use is specified, a value must be provided");
				else {
					std::string tmp = argv[i + 1];
					memory_allocation_limit = atol(tmp.c_str());
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_ini_file_location) {
				if (i + 1 == argc)  //default
					throw ns_ex("if ns_ini_file_location is specified, a value must be provided");
				else {
					ini_file_location = argv[i + 1];
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			else if (p->second == ns_submit_experiment) {
				command = ns_submit_experiment;
				if (i + 1 == argc) throw ns_ex("Output type and filename must be specified for schedule submission");

				if (i + 2 == argc) {
					input_filename = argv[i + 1];
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
				else  {
					input_filename = argv[i + 1];
					string opt(argv[i + 2]);
					if (opt == "f" || opt.size() == 2 && (opt[0] == 'f' || opt[1] == 'f')) {
					
						schedule_submission_behavior = ns_experiment_capture_specification::ns_overwrite;
					}
					if (opt == "a" || opt.size() == 2 && (opt[0] == 'a' || opt[1] == 'a')) {
					
						schedule_submission_behavior = ns_experiment_capture_specification::ns_append;
					}
					if (opt == "u" || opt.size() == 2 && (opt[0] == 'u' || opt[1] == 'u')) {
						
						upload_experiment_spec_to_db = true;
					}
					if ((opt[0] != 'f' && opt[0] != 'a' && opt[0] != 'u') ||
						opt.size() == 2 && (opt[1] != 'f' && opt[1] != 'a' && opt[1] != 'u') ||
						opt.size() > 2)
						throw ns_ex("Unknown capture schedule submission flag: ") << opt;

					i += 2; // "consume" the next  two arguments so they don't get interpreted as a command-string.
				}
			}
			//any other arguments dictate a specific action to be taken by the image server.  This can only be specified once; so fail if multiple such
			//commands are provided
			else {
				if (command == ns_start) {
					command = p->second;
				}
				else {
					ns_ex ex;

					ex << "Could not parse argument after : " << command_str << "\n";
					ex << command_line_usage.text();
					throw ex;
				}
			}
		}
		//check to see that no other nodes are running on this machine.
		if (command == ns_start || command == ns_restart) {
			if (image_server.server_currently_running() && command == ns_start)
				throw ns_ex("An instance of the image server is already running.");
		}

		//load setup information from ini file.  Provide any multiprocess options that may have
		//been specified at the command line, as these options determine how the ini file values should be interpreted
		//by default, ini_file_location == "", and default locations are checked
		image_server.load_constants(ns_image_server::ns_image_server_type, ini_file_location);
		image_server.set_image_processing_run_limits(max_run_time, max_job_num);
		image_server.set_resource_limits(idle_queue_check_limit, memory_allocation_limit, number_of_processing_cores);


		//execute any commands requested at the command line\n";
		switch (command) {
		case ns_start: break;
		case ns_help: break;
		case ns_output_sql_debug:
			if (!image_server.send_message_to_running_server(NS_OUTPUT_SQL_LOCK_INFORMATION))
				std::cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0; break;

		case ns_status: {
			if (image_server.server_currently_running())
				std::cout << "The image server is running\n";
			else std::cout << "The image server is not running.\n";
			return 0;
		}
		case ns_stop: {
			if (!image_server.send_message_to_running_server(NS_QUIT))
				throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			//	ns_request_shutdown_of_all_spawned_nodes(child_processes);
			std::cout << "Image Server halt requested.  Waiting for termination...";
			while (image_server.server_currently_running()) {
				ns_thread::sleep(1);
				std::cerr << ".";
			}
			return 0;
			//		ns_wait_for_all_spawned_nodes(child_processes);
		}
		case ns_simulate_central_db_connection_error: {
			if (!image_server.send_message_to_running_server(NS_SIMULATE_CENTRL_DB_CONNECTION_ERROR))
				cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_simulate_long_term_storage_connection_error: {
			if (!image_server.send_message_to_running_server(NS_SIMULATE_LONG_TERM_STORAGE_ERROR))
				cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_output_image_buffer_info: {
			if (!image_server.send_message_to_running_server(NS_OUTPUT_IMAGE_BUFFER_INFO))
				cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_stop_checking_central_db: {
			if (!image_server.send_message_to_running_server(NS_STOP_CHECKING_CENTRAL_DB))
				cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_start_checking_central_db: {
			if (!image_server.send_message_to_running_server(NS_START_CHECKING_CENTRAL_DB))
				cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_restart: {

			if (!image_server.send_message_to_running_server(NS_QUIT))
				cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			else {
				cerr << "Waiting for the termination of the image server running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
				while (image_server.server_currently_running()) {
					ns_thread::sleep(1);
					std::cerr << ".";
				}
				cerr << "Giving the previous image server a few extra seconds to wrap everything up...";
				for (unsigned int i = 0; i < 20; i++) {
					ns_thread::sleep(1);
					std::cerr << ".";
				}
			}
			break;
		}

		case ns_hotplug: {
			if (!image_server.send_message_to_running_server(NS_HOTPLUG_NEW_DEVICES))
				throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_reset_devices: {
			if (!image_server.send_message_to_running_server(NS_RESET_DEVICES))
				throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_reload_models: {
			if (!image_server.send_message_to_running_server(NS_RELOAD_MODELS))
				throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		case ns_run_pending_image_transfers: {
			post_dispatcher_init_command = ns_run_pending_image_transfers;
			break;
		}
		case ns_submit_experiment: {
			break; //we handle this later, after connecting to the db
		}

		case ns_wrap_m4v: {
			if (!image_server.send_message_to_running_server(NS_WRAP_M4V, argv[2])) {
				std::string output_basename = ns_dir::extract_filename_without_extension(input_filename);
				ns_wrap_m4v_stream(input_filename, output_basename);
			}
			return 0;
		}
		case ns_trigger_segfault_in_main_thread: {
			post_dispatcher_init_command = ns_trigger_segfault_in_main_thread;
			break;
		}
		case ns_trigger_segfault_in_dispatcher_thread: {
			post_dispatcher_init_command = ns_trigger_segfault_in_dispatcher_thread;
			break;
		}
		case ns_clear_local_db_buffer_cleanly:
			if (!image_server.send_message_to_running_server(NS_CLEAR_DB_BUF_CLEAN))
				throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		case ns_clear_local_db_buffer_dangerously:
			if (!image_server.send_message_to_running_server(NS_CLEAR_DB_BUF_DIRTY))
				throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			return 0;
		}
		image_server.set_main_thread_internal_id();

		string splash;
		splash += "=============ns_image_server==============\n";
		splash += "==                                      ==\n";
		splash += "== Image Capture and Processing Server  ==\n";
		splash += " ==               "
			+ ns_to_string(image_server.software_version_major())
			+ "." + ns_to_string(image_server.software_version_minor())
			+ "." + ns_to_string(image_server.software_version_compile())
			+ "                ==\n";
		splash += " ==                                    ==\n";
		splash += " ==        Nicholas Stroustrup         ==\n";
		splash += " ==    Center for Genomic Regulation   ==\n";
		splash += " ==          Barcelona, 2022           ==\n";
		splash += " ========================================\n";
		std::cout << splash;

		std::vector<std::pair<std::string, std::string> > quotes;

		//update table formats to newest version, if requested
		image_server.os_signal_handler.set_signal_handler(ns_interrupt, exit_signal_handler);
		if (schema_installation_requested) {
			if (image_server.act_as_an_image_capture_server())
				image_server.create_and_configure_sql_database(true, "");
			std::cout << "Do you also want to configure the central (remote) mysql database?  This is not needed if you are adding a server to an existing cluster.\n(y/n):";
			bool set_up_central(false);
			while (true) {
			  string b;
			  getline(cin,b);
			  cout << "\n";
			  if (b.size() > 0 && b[0] == 'y') {
				  set_up_central = true;
				  break;
			  }
			  if (b.size() > 0 &&  (b[0] == 'n' || b[0] == 'q' || b[0] == 'c'))
			    throw ns_ex("The request was cancelled by the user.");
			  cout << "Unknown response: \"" << b << "\".  Please type y or n :";
			}
			if (set_up_central) {
				image_server.create_and_configure_sql_database(false, schema_filename);
				image_server.switch_to_default_db();
			}
			
			std::cout << "Checking for recent sql schema updates...\n";
			override_sql_db = "";
			sql_update_requested = true; 
		}
		if (sql_update_requested) {
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));

			if (!override_sql_db.empty()) {
				no_schema_name_specified = false;
				schema_name = override_sql_db;
			}
			if (!no_schema_name_specified)
				image_server.upgrade_tables(&sql(), false, schema_name, schema_name == "image_server_buffer");
			else {
				image_server.upgrade_tables(&sql(), false, image_server.current_sql_database(), false);
				if (image_server.act_as_an_image_capture_server()) {
					ns_acquire_for_scope <ns_local_buffer_connection> local_sql(image_server.new_local_buffer_connection(__FILE__, __LINE__, false));
					bool local_buffer_exists(false);
					try {
						image_server.check_for_local_sql_database_access(&local_sql());
						//check for an empty local buffer database
						local_sql() << "Show tables";
						ns_sql_result res;
						local_sql().get_rows(res);
						local_buffer_exists = !res.empty();
					}
					catch (ns_ex & ex) {
						local_buffer_exists = false;
					}
					if (local_buffer_exists)
					image_server.upgrade_tables(&local_sql(), false, image_server.current_local_buffer_database(), true);
					local_sql.release();
				}
			}
			sql.release();
			return 0;
		}

		if (command == ns_submit_experiment) {
			std::cout << "Attempting to submit an experiment schedule.\n";
			std::vector<std::string> warnings;
			ns_experiment_capture_specification spec;
			spec.load_from_xml_file(input_filename);
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
			ns_image_server::submit_capture_schedule_specification(spec,
				warnings, sql(),
				schedule_submission_behavior,
				upload_experiment_spec_to_db,
				std::string(""),
				true);
			return 0;
		}

		//if we are acting as a capture server, we need to have access to the local sql buffer.
		if (image_server.act_as_an_image_capture_server()) {
			ns_acquire_for_scope <ns_local_buffer_connection> local_sql(0);
			//check for a missing local buffer database
			try {
				local_sql.attach(image_server.new_local_buffer_connection(__FILE__, __LINE__, false));
			       
				image_server.check_for_local_sql_database_access(&local_sql());
			}
			catch (ns_ex & ex) {
				throw ns_ex("The local sql buffer schema cannot be accessed.  If this is the first time you are running the server, please run the command: ns_image_server create_and_configure_sql_db.");
			}
			//check for an empty local buffer database
			local_sql() << "Show tables";
			ns_sql_result res;
			local_sql().get_rows(res);
			if (!res.empty())  //we only check to upgrade local buffer db if it has already been created!  otherwise, we will create it in a later step.
			{
				//check for an out of date local buffer database
				if (image_server.upgrade_tables(&local_sql(), true, image_server.current_local_buffer_database(), true))
					throw ns_ex("The current local buffer database schema is out of date.  Please run the command: ns_image_server update_sql");
			}
			else{
			  image_server.set_up_local_buffer();

			}
			local_sql.release();
		}
		#ifndef _WIN32
		//if running as an image capture server, launch a second process that moniters the first, and takes over if the first becomes unresponsive.
		//start a crash daemon to handle server crashes
		ns_image_server_crash_daemon crash_daemon;
		
		while (true) {
		  if (crash_daemon.start(image_server.server_crash_daemon_port()) == ns_image_server_crash_daemon::ns_ok)
		    break;
		  else {
		    ns_acquire_for_scope<ns_image_server_sql> sql;
		    try {
		      sql.attach(image_server.new_sql_connection(__FILE__, __LINE__));
		    }
		    catch (...) {
		      sql.attach(image_server.new_local_buffer_connection(__FILE__, __LINE__, false));
		    }
		    if (sql().connected_to_central_database()) {
		      image_server.alert_handler.initialize(image_server.mail_from_address(),*static_cast<ns_sql *>(&sql()));
		      std::string text("The image server node ");
		      text += image_server.host_name_out() + " restarted after a fatal error at ";
		      text += ns_format_time_string_for_human(ns_current_time());
		      ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
		      image_server.alert_handler.initialize(image_server.mail_from_address(),sql());
		      
		      ns_alert alert(text,
				     text,
				     ns_alert::ns_server_crash,
				     ns_alert::get_notification_type(ns_alert::ns_server_crash, false),
				     ns_alert::ns_rate_limited
				     );
		      
		      image_server.alert_handler.submit_alert(alert, *static_cast<ns_sql *>(&sql()));
		    }
		    else {
		      image_server.register_server_event(ns_image_server_event("The image server has restarted after a crash."), &sql());
		    }
		    
		  }
		}
		
		//register a swath or error handlers to allow recovery from segfaults and such under linux
		image_server.os_signal_handler.set_signal_handler(ns_segmentation_fault, segmentation_fault_signal_handler);
		image_server.os_signal_handler.set_signal_handler(ns_bus_error, bus_error_signal_handler);
		image_server.os_signal_handler.set_signal_handler(ns_illegal_instruction, illegal_instruction_signal_handler);
		image_server.os_signal_handler.set_signal_handler(ns_floating_point_error, floating_point_error_signal_handler);
		while (!ns_image_server_crash_daemon::daemon_is_running()) {
		  ns_thread::sleep(1);
		}
		
#endif
		//don't act as an image capture server if we just want to copy over images.
		if (post_dispatcher_init_command == ns_run_pending_image_transfers) {
			image_server.override_ini_specified_image_capture_server_behavior(false);
		}

		
		ns_acquire_for_scope<ns_image_server_sql> sql;
		try {
			sql.attach(image_server.new_sql_connection(__FILE__, __LINE__, 0, false));
			//check for a missing central database
			try {
				image_server.check_for_sql_database_access(&sql());
			}
			catch (ns_ex & ex) {
				throw ns_ex("The central sql database schema cannot be accessed.  If this is the first time you are running the server, please run the command: ns_image_server create_and_configure_sql_db.");
			}
		}
		catch (ns_ex & ex) {
			if (image_server.act_as_processing_node())
				throw ns_ex("Could not contact central database: ") << ex.text();

			//if the central database is not available, attempt to fallback to local database.
			image_server.register_server_event_no_db(ns_image_server_event("Could not contact central database: ") << ex.text());
			sql.attach(image_server.new_local_buffer_connection(__FILE__, __LINE__));
			image_server.register_server_event(ns_image_server_event("Could not contact central database: ") << ex.text() << ", falling back to local buffer.", &sql());
		}
		

		if (sql().connected_to_central_database()) {
			//check for an empty central database
			sql() << "Show tables";
			ns_sql_result res;
			sql().get_rows(res);
			if (res.empty())
				throw ns_ex("The central sql database schema is empty.  If this is the first time you are running the server, please run the command: ns_image_server create_and_configure_sql_db.");
			
			//switch to differed db if specified at commandline
			if (!override_sql_db.empty())
				image_server.set_sql_database(override_sql_db, true, &sql());

			//check old tables
			if (image_server.upgrade_tables(&sql(), true, image_server.current_sql_database(), false))
				throw ns_ex("The current central database schema is out of date.  Please run the command: ns_image_server update_sql");

			
		}

		image_server.image_storage.refresh_experiment_partition_cache(&sql());
		if (sql().connected_to_central_database()) {
			//we are (finally!) connected to the central db!  Register this node.
			image_server.register_host(&sql(), true, true);  //we need to get the host id in order to be able to look for a request to change databases.
			image_server.request_database_from_db_and_switch_to_it(*static_cast<ns_sql *>(&sql()), true);  //this registers host in the new db as well

			image_server.register_server_event(ns_image_server_event("Connected to ") << image_server.sql_info(&sql()),&sql());

			image_server.clear_performance_statistics(*static_cast<ns_sql *>(&sql()));
			image_server.clear_old_server_events(*static_cast<ns_sql *>(&sql()));
			image_server.load_quotes(quotes, *static_cast<ns_sql *>(&sql()));
			image_server.alert_handler.initialize(image_server.mail_from_address(),*static_cast<ns_sql *>(&sql()));
			image_server.alert_handler.reset_all_alert_time_limits(*static_cast<ns_sql *>(&sql()));
			ns_death_time_annotation_flag::get_flags_from_db(&sql());
		}

		const bool register_and_run_simulated_devices(image_server.register_and_run_simulated_devices(&sql()));
		srand(ns_current_time());
		std::string quote;
		if (quotes.size() == 0)
			quote = "The oracle is silent.\n\n";
		else {
			std::pair<std::string, std::string> & q(quotes[rand() % quotes.size()]);
			if (q.first.size() == 0)
				quote = "The oracle is silent.\n\n";
			else {
				quote = "\"" + q.first + "\"\n";
				quote += "\t--";
				quote += q.second + "\n\n";
			}
		}
		std::cout << quote;

#ifdef _WIN32
		if (image_server.hide_window()) {

			HWND console_hwnd = GetConsoleHwnd();
			ShowWindow(console_hwnd, SW_HIDE);
			ns_window_hidden = !ns_window_hidden;
		}

#endif

#ifndef _WIN32

		int res = umask(S_IWOTH);//S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
		cout.setf(ios::oct);
		//	cout << "Changing mask from " << res << " to " << res2 << "\n";
		cout.setf(ios::dec);
#endif


		//first we check which port to open.
		//we do this first because it also gives us an idea of how many parallel processes are running on this system
		//and lets us obtain a unique system_parallel_process_id that we will subsequently report to the db when register_host() is called

		ns_image_server_dispatcher dispatch(true);


		const unsigned long dispatcher_offset_time(image_server.dispatcher_refresh_interval());

		const bool try_multiple_ports(restarting_after_crash || image_server_const.allow_multiple_processes_per_system());
		ns_ex stored_ex;
		try {
			if (!try_multiple_ports) {
				image_server.set_up_model_directory();
				dispatch.init(image_server.dispatcher_port(), dispatcher_socket_queue_length);
			}
			else {
				if (restarting_after_crash) {
					std::cerr << "Searching for the next available port after crash.\n";
				}
				const long first_port(image_server.dispatcher_port());

				bool found_available_socket(false);
				for (unsigned int i = 1; i < 1024; i++) {
					for (unsigned int j = 0; j < 2; j++) {
						try {
							dispatch.init(image_server.dispatcher_port(), dispatcher_socket_queue_length);
							found_available_socket = true;
							break;
						}
						catch (ns_ex & ex) {
							//cerr << "Could not bind to " << image_server.dispatcher_port() << ": " << ex.text();
							ns_thread::sleep_milliseconds(100);
						}
					}
					if (found_available_socket)
						break;
					else {
						image_server.increment_dispatcher_port();
						//if a port is blocked, we assume that another server is running there (unless we are restarting after a crash)
						if (!(i == 1 && restarting_after_crash))
							image_server.increment_system_parallel_process_id();
					}
				}
				if (!found_available_socket)
					throw ns_ex("Could not bind to any sockets, ") << first_port << " to " << image_server.dispatcher_port();
			}
			if (!image_server.act_as_an_image_capture_server())
				image_server.image_storage.update_volatile_storage_directory_for_parallel_processes(image_server.system_parallel_process_id());
		}
		catch (ns_ex & ex) {
			//we want to wait to throw the error until after we've registered in the db, so that the error can be recorded there.
			stored_ex = ex;

		}


		if (sql().connected_to_central_database()) {

			if (!stored_ex.text().empty())
				throw stored_ex;

			image_server.update_processing_status("Starting...", 0, 0, &sql());
			image_server.register_server_event(ns_image_server_event("Launching server..."), &sql(), true);
			image_server.add_subtext_to_current_event(splash, &sql(), true);
			image_server.add_subtext_to_current_event(quote, &sql(), true);

			if (image_server.new_software_release_available() && image_server.halt_on_new_software_release()) {

				image_server.register_server_event(ns_image_server_event("A more recent version of server software was found running on the cluster.  This server is outdated and is halting now."), &sql());
#ifdef _WIN32
				image_server.update_software = true;
#endif
				throw ns_ex("Updated software detected on the cluster.");
			}
			image_server.clear_processing_status(&sql());
		}



#ifdef NS_USE_INTEL_IPP
		if (image_server.act_as_processing_node()){

			ns_image_server_event t_event("Testing Intel Performance Primitives...");
			if (sql().connected_to_central_database())
				image_server.register_server_event(t_event, &sql());
			else image_server.register_server_event_no_db(t_event);
			ns_thread::sleep(1);
			if (ns_test_ipp()) {
				if (sql().connected_to_central_database())
					image_server.add_subtext_to_current_event(" ",&sql());
				else cerr << " ";
			}
		}

#endif

		if (image_server.act_as_an_image_capture_server()) {
			if (!image_server.mail_path().empty() && !ns_dir::file_exists(image_server.mail_path()))
				throw ns_ex("The mail program ") << image_server.mail_path() << " does not appear to exist.  In the ns_image_server.ini configuration file, please set mail_path your POSIX mail program.  To disable alerts, set mail_path as blank (no value).";
		}

		image_server.register_server_event(ns_image_server_event("Clearing local image cache"), &sql());
		image_server.image_storage.clear_local_cache();


		image_server.image_storage.test_connection_to_long_term_storage(true);
		if (!image_server.image_storage.long_term_storage_was_recently_writeable() && image_server.act_as_processing_node()) {
			if (image_server.act_as_an_image_capture_server()) {
				image_server.set_processing_node_behavior(false);
				image_server.register_server_event(ns_image_server_event("Cannot connect to long term storage.  This server will not act as an image processing node."), &sql());
			}
			else throw ns_ex("Cannot connect to long term storage.");
		}

		//handle requested commandline commands that requre a database
		switch (command) {
		case ns_fix_orphaned_captured_images: {
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
			image_server.image_storage.fix_orphaned_captured_images(&sql());
			sql.release();
			return 0;
		}

		case ns_test_email: {
			std::cout << "Trying to send a test alert email...";
			std::string text("Image server node ");
			text += image_server.host_name_out();
			text += " has succesfully sent an email."; 
			{
				ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
				image_server.alert_handler.initialize(image_server.mail_from_address(), sql());
				image_server.alert_handler.submit_desperate_alert(text,true);
				sql.release();
			}
			std::cout << "Done.  Check to see if an email was recieved.\n";
			return 0;
		}
		case ns_test_alert: {
			std::string text("At ");
			text += ns_format_time_string_for_human(ns_current_time()) + " a cluster node submitted a test alert.";
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
			image_server.alert_handler.initialize(image_server.mail_from_address(),sql());
			ns_alert alert(text,
				text + "  If you are recieving this message, it has been handled successfully.",
				ns_alert::ns_test_alert
				, ns_alert::get_notification_type(ns_alert::ns_test_alert, false),
				ns_alert::ns_not_rate_limited
			);

			image_server.alert_handler.submit_alert(alert, sql());
			sql.release();
			return 0;
		}
		case ns_test_rate_limited_alert: {
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
			image_server.alert_handler.initialize(image_server.mail_from_address(),sql());
			std::string text("At ");
			text += ns_format_time_string_for_human(ns_current_time()) + " a cluster node submitted a rate limited test alert.";
			ns_alert alert(text,
				text + "  If you are recieving this message, it has been handled successfully.",
				ns_alert::ns_test_alert
				, ns_alert::get_notification_type(ns_alert::ns_test_alert, false),
				ns_alert::ns_rate_limited
			);

			image_server.alert_handler.submit_alert(alert, sql());
			sql.release();
			return 0;
		}

		case ns_restarting_after_a_crash: {
			std::string text("The image server node ");
			text += image_server.host_name_out() + " restarted after a fatal error at ";
			text += ns_format_time_string_for_human(ns_current_time());
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__, __LINE__));
			image_server.alert_handler.initialize(image_server.mail_from_address(),sql());

			ns_alert alert(text,
				text,
				ns_alert::ns_server_crash,
				ns_alert::get_notification_type(ns_alert::ns_server_crash, true),
				ns_alert::ns_rate_limited
			);

			image_server.alert_handler.submit_alert(alert, sql());
			sql.release();
			restarting_after_crash = true;
			break;
		}
		}


		//	else image_server.image_storage.set_verbosity(ns_image_storage_handler::ns_deletion_events);



		if (post_dispatcher_init_command == ns_trigger_segfault_in_main_thread) {
			cerr << "Triggering Segfault...\n";
			char * a(0);
			(*a)++;
		}
		if (image_server_const.allow_multiple_processes_per_system() && image_server_const.get_additional_host_description().empty())
			throw ns_ex("If allow_multiple_processes_per_system is set in the ns_image_server.ini file, a unique value of additional_host_description must be provided to each instance of ns_image_server as a commandline argument!");


		if (post_dispatcher_init_command == ns_trigger_segfault_in_dispatcher_thread)
			dispatch.trigger_segfault_on_next_timer();

		//search for devices
		if (image_server.act_as_an_image_capture_server()){
			image_server.set_up_local_buffer();
			if (image_server.remember_barcodes_across_sessions() && image_server.device_manager.load_last_known_device_configuration()){
				cerr << "Last known configuration:\n";
				image_server.device_manager.output_connected_devices(cerr);
				cerr << "\n";
				image_server.device_manager.hotplug_new_devices(true,true,false);
			}
			else {
				if (!image_server.remember_barcodes_across_sessions()) cerr << "No previous device information was loaded because remember_barcodes_across_sessions is set to false.\n";
				image_server.device_manager.clear_device_list_and_identify_all_hardware();
			}
		}
		else image_server.register_server_event(ns_image_server_event("Not searching for attached devices."),&sql());

		if (image_server.act_as_an_image_capture_server() && register_and_run_simulated_devices){
			if (image_server.simulated_device_name().size() > 2)
				image_server.device_manager.attach_simulated_device(image_server.simulated_device_name());
		}
		if (image_server.act_as_an_image_capture_server())
			image_server.device_manager.save_last_known_device_configuration();

		if (sql().connected_to_central_database())
			image_server.register_devices(false, &sql());

		if (!image_server.act_as_processing_node()){
			image_server.register_server_event(ns_image_server_event("Not acting as a processing node."),&sql());
		//	image_server.image_storage.set_verbosity(ns_image_storage_handler::ns_verbose);
		}

		//play nice on multi-use machine.
		//the latest capture sample registration code can fully utilize a computer's resources and reduce GUI responsiveness.
		if (image_server.act_as_processing_node()){
			image_server.process_priority.set_priority(ns_process_priority::ns_below_normal);
		}
		if (image_server.act_as_an_image_capture_server()){
			bool success(image_server.process_priority.set_priority(ns_process_priority::ns_high));
			if (!success){
				image_server.register_server_event(ns_image_server_event(std::string("The server could not increase its scheduling priority.  "
					"Consider running ns_image_server with administrative privileges, or manually increasing priority to -10 using the command\n"
					"renice -15 -p ") +  ns_to_string(ns_thread::ns_get_process_id())),&sql());
			}
			ns_thread current_thread(ns_thread::get_current_thread());
			current_thread.set_priority(NS_THREAD_LOW);
		}

		//connect the timer sql connection
		//otherwise we will re-register the host upon the timer thread noticing it isn't connected
		if (sql().connected_to_central_database()) {
			dispatch.connect_timer_sql_connection();
		}


		unsigned int * timer_interval = new unsigned int(image_server.dispatcher_refresh_interval());


		if (post_dispatcher_init_command == ns_run_pending_image_transfers){
			dispatch.buffered_capture_scheduler.image_capture_data_manager.handle_pending_transfers_to_long_term_storage_using_db_names();
			//try to update info to central db
			if (sql().connected_to_central_database()) {
				ns_acquire_for_scope<ns_local_buffer_connection> local_buffer_connection(image_server.new_local_buffer_connection(__FILE__, __LINE__));
				ns_acquire_for_scope<ns_sql> sql2(image_server.new_sql_connection(__FILE__, __LINE__, 0, false));
				dispatch.buffered_capture_scheduler.commit_local_changes_to_central_server(local_buffer_connection(), sql2());
				local_buffer_connection.release();
				sql2.release();

			}
		}
		sql.release();
		{

			const unsigned long dispatcher_offset_time(image_server.dispatcher_refresh_interval());
			if (dispatcher_offset_time > 0)
				ns_thread::sleep(dispatcher_offset_time);

			ns_thread timer(timer_thread,reinterpret_cast<void *>(timer_interval));
			#ifdef _WIN32
			run_dispatcher_t rd;
			rd.dispatcher =&dispatch;
			rd.restarting_after_crash = restarting_after_crash;

			HWND message_hwnd = create_message_window(GetModuleHandle(NULL));
			rd.window_to_close = message_hwnd;
			handle_icons(GetModuleHandle(NULL), message_hwnd);
			set_hostname_on_menu(image_server.host_name_out());

			//set_terminate(ns_terminator);

			ns_thread dispatcher_thread(run_dispatcher,&rd);
			windows_message_loop();

	//		image_server.exit_has_been_requested = true;
			timer.block_on_finish();
			dispatcher_thread.block_on_finish();
			#else
			dispatch.run();
			image_server.exit_has_been_requested = true;
			timer.block_on_finish();
			#endif
		}
		//just leak this; the whole process is shutting down anyway.
		//delete timer_interval;

		#ifndef _WIN32
		ns_socket::global_clean();
		#endif

		cerr << "Terminating...\n";
		}
	catch(ns_ex & ex){
	  #ifdef _WIN32
		//if (!console_window_created)
		//	ns_make_windows_console_window();
		#endif

		cerr << "Server Root Exception: " << ex.text() << "\n";
		#ifdef _WIN32
		//argv.clear();
		for (unsigned int i = 5; i > 0; i--){
			cerr << i << "...";
			ns_thread::sleep(1);
		}

		FreeConsole();
		destroy_icons();
		#endif
	}
	#ifdef _WIN32
		ns_socket::global_clean();
		destroy_icons();
	/*	if (image_server.update_software && image_server.handle_software_updates()){
			try{
				ns_update_software();
			}
			catch(ns_ex & ex){
				cerr << ex.text() << "\n";
			}

		}*/
	#endif
	image_server.clear();
	ns_sql::unload_sql_library();


	return 0;
}
