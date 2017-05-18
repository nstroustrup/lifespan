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
#include "ns_spatial_avg.h"
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
		while(image_server.exit_requested == false){
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
	cerr << "Attempting elegant shutdown...\n";
	#ifdef _WIN32 
			ModifyMenu(toolbar_menu,ID_CONTEXTMENU_FONTANAIMAGESERVER,MF_BYCOMMAND,MF_STRING | MF_DISABLED,"Shutting down...");
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
					if (image_server.exit_requested == false){
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
	strcpy ( notify_icon_info.szTip, "Fontana Imaging Server");
 
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
	if (image_server.exit_requested == false){
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
bool ns_multiprocess_control_options::from_parameter(const std::string & param){
	if (param == "single_process"){
		only_run_single_process = true; 
		return 1;
	}
	string::size_type p=param.find("=");
	if (p == param.npos)
		return 0;
	string key(param.substr(0,p)),
		   value(param.substr(p+1,param.npos));
	if (key=="--process_id")				{process_id = atol(value.c_str()); return 1;}
	if (key=="--total_number_of_processes")	{total_number_of_processes = atol(value.c_str()); return 1;}
	if (key=="--compile_videos")			{compile_videos = (value=="1"); return 1;}
	if (key=="--handle_software_updates")	{handle_software_updates = (value=="1"); return 1;}
	if (key=="--manage_capture_devices")	{manage_capture_devices = (value=="1"); return 1;}
	if (key=="--port_offset")	{dispatcher_port_offset = atol(value.c_str()); return 1;}
	return false;
}


void ns_multiprocess_control_options::to_parameters(std::vector<std::string> & param){
	param.push_back(string("--process_id=") + ns_to_string(process_id));
	param.push_back(string("--total_number_of_processes=") + ns_to_string(total_number_of_processes));
	param.push_back(string("--compile_videos=") + (compile_videos?"1":"0"));
	param.push_back(string("--handle_software_updates=") + (handle_software_updates?"1":"0"));
	param.push_back(string("--manage_capture_devices=") + (manage_capture_devices?"1":"0"));
	param.push_back(string("--port_offset=") + ns_to_string(dispatcher_port_offset));
}
#ifdef _WIN32 
HWND ns_make_windows_console_window(){

	AllocConsole();
	HANDLE stdout_handle = GetStdHandle(STD_OUTPUT_HANDLE),
		   stderr_handle = GetStdHandle(STD_ERROR_HANDLE),
		   stdin_handle = GetStdHandle(STD_INPUT_HANDLE);
	int stdout_handle_i = _open_osfhandle((intptr_t) stdout_handle, _O_TEXT),
		stderr_handle_i = _open_osfhandle((intptr_t) stderr_handle, _O_TEXT),
		stdin_handle_i = _open_osfhandle((intptr_t) stdin_handle, _O_TEXT);
	FILE * stdout_fp = _fdopen( stdout_handle_i, "w" ),
		 * stderr_fp = _fdopen( stderr_handle_i, "w" ),
		 * stdin_fp =  _fdopen( stdin_handle_i,  "r" );
	*stdout = *stdout_fp;
	*stdin = *stdin_fp;
	*stderr = *stderr_fp;

	freopen("CONOUT$", "w", stdout);
	freopen("CONOUT$", "w", stderr);
	freopen("CONIN$", "r", stdin);
	ios::sync_with_stdio();

	HWND console_hwnd = GetConsoleHwnd();
	ns_window_to_hide = console_hwnd;

	//remove close option from console window (close via toolbar icon)
	HMENU cur_menu = GetSystemMenu(console_hwnd, FALSE);
	DeleteMenu (cur_menu, SC_CLOSE,MF_BYCOMMAND);

	
	image_server.set_console_window_title();
	return console_hwnd;
}
#endif

struct ns_child_process_information{
	ns_external_execute execute;
	ns_multiprocess_control_options options;
};

void ns_request_shutdown_of_all_spawned_nodes(const std::vector<ns_child_process_information> &child_processes){
	for (unsigned int i = 0; i < child_processes.size(); ++i){
		try{
			unsigned long port(child_processes[i].options.port(image_server.dispatcher_port()));
			if (!image_server.send_message_to_running_server(NS_QUIT,"",port))
				throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << port << ".";
		}
		catch(ns_ex & ex){
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
		}
	}	
}
void ns_wait_for_all_spawned_nodes(std::vector<ns_child_process_information> &child_processes){
	for (unsigned int i = 0; i < child_processes.size(); ++i){
		try{
			cerr << "Waiting for node on port " << child_processes[i].options.port(image_server.dispatcher_port()) << "...\n";
			for (unsigned int i = 0; i < child_processes.size(); i++){
				try{child_processes[i].execute.wait_for_termination();
				}
				catch(...){}
			}
		}
		catch(ns_ex & ex){
			image_server.register_server_event(ns_image_server::ns_register_in_central_db,ex);
		}
	}	
}

void ns_terminate(){
	return;
}

ns_multiprocess_control_options ns_spawn_new_nodes(	const unsigned int & count,
													const ns_multiprocess_control_options & command_line_options,
													std::vector<ns_child_process_information> &child_processes, const bool allow_duplicate_port_assigments=false){
	
	if (count==0)
		return command_line_options;
	#ifdef _WIN32 
	//if we are a child process launched on windows, just return what was provided at the commandline
	if (command_line_options.process_id != 0)
		return command_line_options;
	#endif
	
	ns_multiprocess_control_options parent_options;
	parent_options.compile_videos = true;
	parent_options.handle_software_updates = true;
	parent_options.manage_capture_devices = true;
	unsigned long next_port_offset(1);

	//if we're the parent process, spawn off a bunch of child processes.
	child_processes.resize(count);
	for (int i = 0; i < count; i++){
		child_processes[i].options = command_line_options;
		child_processes[i].options.compile_videos = false;
		child_processes[i].options.handle_software_updates = false;
		child_processes[i].options.manage_capture_devices = false;
		child_processes[i].options.process_id = i+1;
		ns_socket s;
		bool good_bport(false);
		if (!allow_duplicate_port_assigments){
			//find the next good port
			while(1){
				try{
					s.listen(image_server.dispatcher_port() + next_port_offset,1024);
					s.close_socket();
					break;
				}
				catch(...){}
				next_port_offset++;
			}
		}
		child_processes[i].options.dispatcher_port_offset = next_port_offset;
		next_port_offset++;
		#ifdef _WIN32 
			//on windows, launch a new process passing instructions via the commandline
			vector<string> args;
			child_processes[i].options.to_parameters(args);
			string parameters;
			for (int j = 0; j < args.size(); j++){
				parameters+= args[j];
				if (j != args.size()-1)
					parameters+=" ";
			}

			ns_external_execute_options opts;
			opts.take_stderr_handle = false;
			opts.take_stdin_handle = false;
			opts.take_stdout_handle = false;
			
			char full_command_line[1024];
			GetModuleFileName(NULL,full_command_line,1024);
			child_processes[i].execute.run(full_command_line,parameters, opts);
			child_processes[i].execute.release_io();


		#else
			//on linux, fork and return the apropriate options
			ns_external_execute_options opts;
			opts.take_stderr_handle = false;
			opts.take_stdin_handle = false;
			opts.take_stdout_handle = false;
			opts.only_fork = true;
			bool is_parent_thread(child_processes[i].execute.run("","",opts));
			if (!is_parent_thread){
				ns_multiprocess_control_options opt(child_processes[i].options);
				child_processes.clear();
				return opt;
			}
		#endif
	}
	return parent_options;
}

typedef enum {ns_none,ns_start, ns_stop, ns_help, ns_restart, ns_status, ns_hotplug,
			  ns_reset_devices,ns_reload_models,ns_submit_experiment,ns_test_email,ns_test_alert, ns_test_rate_limited_alert,ns_wrap_m4v,
			  ns_restarting_after_a_crash,ns_trigger_segfault_in_main_thread,ns_trigger_segfault_in_dispatcher_thread, ns_run_pending_image_transfers,
	      ns_clear_local_db_buffer_cleanly,ns_clear_local_db_buffer_dangerously,ns_simulate_central_db_connection_error,ns_fix_orphaned_captured_images,ns_update_sql,ns_output_image_buffer_info,ns_stop_checking_central_db,ns_start_checking_central_db,ns_output_sql_debug, ns_additional_host_description, ns_max_run_time_in_seconds, ns_max_number_of_jobs_to_process} ns_cl_command;

ns_image_server_sql * ns_connect_to_available_sql_server(){
		try{
			//open a db connection to run a initialization routines
			return image_server.new_sql_connection(__FILE__,__LINE__,0);
		}
		catch(ns_ex & ex){
		//	throw ns_ex("Could not contact sql database; the attempt generated the following message: ") << ex.text();
			if (!image_server.act_as_an_image_capture_server())
				throw ns_ex("Could not contact central database: ") << ex.text();
			image_server.register_server_event_no_db(ns_image_server_event("Could not contact central database: ") << ex.text());
			ns_image_server_sql * sql(image_server.new_local_buffer_connection(__FILE__,__LINE__));
			image_server.register_server_event(ns_image_server_event("Could not contact central database: ") << ex.text() << ", falling back to local buffer.",sql);
			return sql;
		}
}
#ifdef NS_USE_INTEL_IPP
#include "ipp.h"
#endif

#include "ns_ojp2k.h"
#include "ns_image_registration.h"
#include "ns_optical_flow.h"
#ifdef _WIN32 
//int APIENTRY WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow){
	int main(int argc, char ** argv){
	/*int argc;
	std::vector<std::string> argv;
	argv.push_back("image_server_win32.exe");

	if (lpCmdLine != 0){
		
		int pos = 0;
		std::string temp;
		while(true){
			char a = lpCmdLine[pos];
			if (a == 0){
				if (temp.size() != 0) argv.push_back(temp);
				break;
			}
			else if (a == ' '){
				argv.push_back(temp);
				temp.resize(0);
			}
			else temp += a;
			pos++;
		}
	}
	argc = (int)argv.size();
	*/
	//bool console_window_created(false);

#else
int main(int argc, char * argv[]){
#endif

	#ifdef NS_USE_INTEL_IPP
	ippInit();
	#endif


	/*

	try {
		ns_opengl_test("c:\\server\\");
	}
	catch (ns_ex & ex) {
		std::cerr << ex.text() << "\n";
		char a;
		std::cin >> a;
	}*/
	//Image registration code test
	/*if(0){
	try{
	ns_image_registration<127,ns_8_bit> registration;
	ns_image_standard im1, im2;
	std::string dir("Y:\\image_server_storage\\partition_000\\2014_12_21_compound_scaling\\rise_d\\captured_images\\");
	ns_load_image(dir+"2014_12_21_compound_scaling=1035=rise_d=29839=1419150907=2014-12-21=03-35=5395566=235850499.tif",im1);
	ns_load_image(dir+"2014_12_21_compound_scaling=1035=rise_d=29839=1422236107=2015-01-25=20-35=5582815=238599372.tif",im2);
	ns_high_precision_timer t;
	t.start();
	ns_vector_2i diff (registration.register_images(im1,im2,ns_full_registration,ns_vector_2i(150,150)));
	ns_64_bit diff_time(t.stop());
	t.start();
	//ns_vector_2i diff2 (registration.register_images(im1,im2,ns_sum_registration,ns_vector_2i(50,50)));
	ns_vector_2i diff3 (registration.register_images(im1,im2,ns_threshold_registration,ns_vector_2i(150,150)));
	ns_64_bit diff3_time(t.stop());
	ofstream foo ("c:\\server\\out.txt");
	foo << diff.x << "," << diff.y << "\n";
	foo << diff_time << "\n";
	//foo << diff2.x << "," << diff2.y << "\n";
	foo << diff3.x << "," << diff3.y << "\n";
	foo << diff3_time << "\n";
	foo.close();
	cerr << "WHA";
	}
	catch(ns_ex & ex){
		cerr << ex.text() << "\n";
		cerr << "\n";
	}
	return 0;
	}
	*/

	


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
	bool is_master_node(false);
	std::string schema_name;
	try{


		
		ns_sql::load_sql_library();

		//we run multiple instances of the server node on each computer.
		//to do this we launch one process per instance at runtime.
		/*ns_multiprocess_control_options mp_options;
		for (unsigned int i = 1; i < argc; i++)
			mp_options.from_parameter(argv[i]);*/
		
		//load setup information from ini file.  Provide any multiprocess options that may have
		//been specified at the command line, as these options determine how the ini file values should be interpreted
		image_server.load_constants(ns_image_server::ns_image_server_type);		
		
	
		//make sure the multiprocess options are sane
		//mp_options.total_number_of_processes = image_server.number_of_node_processes_per_machine();
	//	mp_options.total_number_of_processes = 1;

		//set default options for command line arguments
		ns_cl_command command(ns_start);
		bool upload_experiment_spec_to_db(false),
			  overwrite_existing_experiments(false);
		std::string input_filename;

		//parse the command line
		for (int i = 1; i < argc; i++){
			std::string command_str(argv[i]);
			//ignore multi-process control parameters
			//ns_multiprocess_control_options opt_check;
			//if (opt_check.from_parameter(command_str))
			//	continue;

			//run as background daemon if requested.
			if (std::string(argv[i]) == "daemon"){
				#ifdef _WIN32 
				throw ns_ex("The image server cannot explicity start as dameon under windows.");
				#else
				if (::daemon(1,0)!=0)
					throw ns_ex("Could not launch as a daemon!");
				continue;
				#endif
			}




			std::map<std::string,ns_cl_command>::iterator p = commands.find(command_str);
			if (p == commands.end() || p->second == ns_help){
					ns_ex ex;
					if (p == commands.end())
						ex << "Unknown command line argument: " << command_str << "\n";
				
					ex	<< "Usage: " << argv[0] << " [start] [status] [stop] [restart] [hotplug] [reset_devices] [reload_models] [wrap_m4v] [submit_experiment [f,u] filename] [single_process] [help]";
					#ifndef _WIN32
					ex << " [daemon]";
					#endif
					ex << "\nOptions:\n"
						<< "start : start an instance of the image server on the local machine (default)\n"
						<< "status : check to see if an instance of the image server is running on the local machine\n"
						<< "stop : request that the currently running local instance of the image server terminate\n"
						<< "restart : request that the currently running local instance of the image server terminate, and launch a new instance in current process\n"
						<< "hotplug : request the currently running local instance of the image server check for hardware changes to image acquisition devices attached to local machine\n"
						<< "reset_devices : request the currently running local instance of the image server reset its image acquisition device registry and build it from scratch\n"
						<< "reload_models : request the currently running local instance of the image server clear its cache of worm detection models so they are reloaded from disk.\n"
						<< "additional_host_description [text]: optionally specify extra information to distinguish the current host (e.g when running in an HPC cluster)\n"
						<< "submit_experiment: Test and submit an experiment specification XML file to the cluster\n"
						<< "    By default this only outputs a summary of the proposed experiment to stdout\n"
						<< "    With sub-option 'u': actually submit the experiment specification to the database \n"
						<< "    With sub-option 'f' (which implies 'u'): force overwriting of existing experiments\n"
						<< "test_email : send a test alert email from the current node\n"
						<< "test_alert : send a test alert to be processed by the cluster\n"
						<< "test_rate_limited_alert : send a test alert to be processed by the cluster\n"
						<< "wrap_m4v:  wrap the specified m4v stream in an mp4 wrapper with multiple frame rates.\n"
						<< "trigger_segfault: Trigger a segfault to test the crash daemon\n"
						<< "trigger_dispatcher_segfault: Trigger a segfault in the dispatcher to test the crash daemon\n"
						<< "run_pending_image_transfers: Start up the server, transfer any pending images to the central file server, and shut down\n"
						<< "clear_local_db_buffer_cleanly: Clear all information from the local database after synchronizing it with the central db.\n"
						<< "clear_local_db_buffer_dangerously: Clear all information from the local database without synchronizing.\n"
						<< "simulate_central_db_connection_error: Simulate a broken connection to the central database.\n"
						<< "fix_orphaned_captured_images: Go through the volatile storage and fix database records for images orphaned by a previous bug in the lifespan machine software\n"
						<< "output_image_buffer_info: Output information about the state of each scanner's locally buffered images.\n"
						<< "stop_checking_central_db: Cease attempting to connect to the central db.\n"
						<< "start_checking_central_db: Restart attempts to connect to the central db.\n"
						<< "update_sql [schema name]: update the sql database schema to match the most recent version. No changes are made if the schema is already up-to-data.  Schema can be specified\n"
						<< "output_sql_debug: request that a running server output its sql debug information\n";
					#ifndef _WIN32
					ex << "daemon: run as a background process\n";
					#endif
					throw ex;
			}
			command = p->second;
			//grab extra information for arguments that are longer than one argument
			if (p->second == ns_wrap_m4v){
				if (i+1>= argc) throw ns_ex("M4v filename must be specified");
				input_filename = argv[i+1];
			}
			if (p->second == ns_update_sql){
				if (i+1 == argc)  //default
					schema_name = "image_server";
				else {
					schema_name = argv[i+1];
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			if (p->second == ns_additional_host_description) {
				if (i + 1 == argc)  //default
					throw ns_ex("if additional_host_description is specified, a value must be provided");
				else {
					std::string additional_host_description = argv[i+1];
					image_server.set_additional_host_description(additional_host_description);
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			unsigned long max_run_time(0), max_job_num(0);
			if (p->second == ns_max_number_of_jobs_to_process) {
				if (i + 1 == argc)  //default
					throw ns_ex("if max_number_of_jobs_to_process is specified, a value must be provided");
				else {
					std::string  tmp = argv[i + 1];
					max_job_num = atol(tmp.c_str());
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			if (p->second == ns_max_run_time_in_seconds) {
				if (i + 1 == argc)  //default
					throw ns_ex("if max_run_time_in_seconds is specified, a value must be provided");
				else {
					std::string tmp = argv[i + 1]; 
					max_run_time = atol(tmp.c_str());
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
			}
			image_server.set_image_processing_run_limits(max_run_time, max_job_num);

			if (p->second == ns_submit_experiment){
				
				if (i+1== argc) throw ns_ex("Output type and filename must be specified for schedule submission");
				
				if (i+2 == argc) {
					input_filename = argv[i+1];
					i++; // "consume" the next argument so it doesn't get interpreted as a command-string.
				}
				else if (i+3 == argc){
					string opt(argv[i+1]);
					if (opt == "f" || opt.size()==2 && (opt[0]=='f' || opt[1]=='f'))
						overwrite_existing_experiments = true;
					if (opt == "u" || opt.size()==2 && (opt[0]=='u' || opt[1]=='u'))
						upload_experiment_spec_to_db = true;
					input_filename = argv[i+2];
					i += 2; // "consume" the next  two arguments so they don't get interpreted as a command-string.
				}
			}
		}
	//	if (mp_options.only_run_single_process)
		//	mp_options.total_number_of_processes = 1;

		//std::vector<ns_child_process_information> child_processes;
		//if we're starting up a new server, launch all the desired processes
		if (command == ns_start || command == ns_restart){
			
			if (image_server.server_currently_running() && command == ns_start )
				throw ns_ex("An instance of the image server is already running.");


//			mp_options = ns_spawn_new_nodes(mp_options.total_number_of_processes-1,mp_options, child_processes,command==ns_restart);
			image_server.load_constants(ns_image_server::ns_image_server_type);
	//		image_server.set_multiprocess_control_options(mp_options);
		}
		is_master_node = true;
		//Starting here, all code is run by all processes
		
		ns_cl_command post_dispatcher_init_command(ns_none);
		//make a console window
		#ifdef _WIN32 
	//	HWND console_hwnd(ns_make_windows_console_window());
	//	console_window_created = true;
		#endif

		/*
		try{
			ns_image_whole<float> im;
			for ( int i = 200; i < 500; i++)
				for ( int j = 200; j < 500; j++)
				{
					ns_image_properties p(200 ,200, 1);
					cerr << p.width << "," << p.height << "\n";
					im.init(p);
					ns_gaussian_pyramid py;
					py.calculate(im);

				}
		
		
		
			}
		catch (ns_ex & ex) {
			cerr << ex.text();
		}
		cerr << "WHA";
		*/
		//ns_test_simple_cache("c:\\server\\cache_debug.txt");

		bool restarting_after_crash(false);
		//execute any commands requested at the command line\n";
		switch(command){
			case ns_start: break;
			case ns_help: break;
			case ns_output_sql_debug: 	
				if (!image_server.send_message_to_running_server(NS_OUTPUT_SQL_LOCK_INFORMATION))
				cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
				return 0; break;

			case ns_status:{
				if (image_server.server_currently_running())
						std::cout << "The image server is running\n";
				else std::cout << "The image server is not running.\n";
				return 0;
			}
			case ns_stop:{
				if (!image_server.send_message_to_running_server(NS_QUIT))
					throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
			//	ns_request_shutdown_of_all_spawned_nodes(child_processes);
				std::cout << "Image Server halt requested.  Waiting for termination...";
				while(image_server.server_currently_running()){
					ns_thread::sleep(1);
					std::cerr << ".";	
				}
				return 0;
		//		ns_wait_for_all_spawned_nodes(child_processes);
			}
			case ns_simulate_central_db_connection_error:{
					if (!image_server.send_message_to_running_server(NS_SIMULATE_CENTRL_DB_CONNECTION_ERROR))
					cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
				return 0;
			}
			case ns_output_image_buffer_info:{
					if (!image_server.send_message_to_running_server(NS_OUTPUT_IMAGE_BUFFER_INFO))
					cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
				return 0;
			}
			case ns_stop_checking_central_db:{
					if (!image_server.send_message_to_running_server(NS_STOP_CHECKING_CENTRAL_DB))
					cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
				return 0;
			}
			case ns_start_checking_central_db:{
					if (!image_server.send_message_to_running_server(NS_START_CHECKING_CENTRAL_DB))
					cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
				return 0;
			}				
			case ns_restart:{
			  
				if (!image_server.send_message_to_running_server(NS_QUIT))
					cerr << "No image server found running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
				else{
					cerr << "Waiting for the termination of the image server running at " << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";
					while(image_server.server_currently_running()){
						ns_thread::sleep(1);
						std::cerr << ".";		
					}
					cerr << "Giving the previous image server a few extra seconds to wrap everything up...";
					for (unsigned int i = 0; i < 20; i++){
						ns_thread::sleep(1);
						std::cerr << ".";		
					}
				}
				break;
			}
							
			case ns_hotplug:{
				if (!image_server.send_message_to_running_server(NS_HOTPLUG_NEW_DEVICES))
					throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";	
				return 0;
			}
			case ns_reset_devices:{
				if (!image_server.send_message_to_running_server(NS_RESET_DEVICES))
					throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";	
				return 0;
			 }
			case ns_reload_models:{
				if (!image_server.send_message_to_running_server(NS_RELOAD_MODELS))
					throw ns_ex("No image server found running at ") << image_server.dispatcher_ip() << ":" << image_server.dispatcher_port() << ".";	
				return 0;
			}	
			case ns_run_pending_image_transfers:{
				post_dispatcher_init_command = ns_run_pending_image_transfers;
				break;
			}
			case ns_submit_experiment:{
				std::vector<std::string> warnings;
				ns_image_server::process_experiment_capture_schedule_specification(input_filename,
					warnings,
					overwrite_existing_experiments,
					upload_experiment_spec_to_db,
					std::string(""),
					true);
				return 0;
			}
			
			case ns_wrap_m4v:{
				if (!image_server.send_message_to_running_server(NS_WRAP_M4V,argv[2])){
					std::string output_basename = ns_dir::extract_filename_without_extension(input_filename);
					ns_wrap_m4v_stream(input_filename,output_basename);
				}
				return 0;
			}
			case ns_trigger_segfault_in_main_thread:{
				post_dispatcher_init_command = ns_trigger_segfault_in_main_thread;
				break;
			}
			case ns_trigger_segfault_in_dispatcher_thread:{
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

			case ns_additional_host_description: break; //handled above
			//all of these require access to the sql database and will be handled
			//a little later in the startup process
			case ns_fix_orphaned_captured_images:
			case ns_update_sql:
			case ns_test_email:
			case ns_test_alert:
			case ns_test_rate_limited_alert:
			case ns_restarting_after_a_crash:
				break;
			default:
				throw ns_ex("Unhandled command:") << (int)command;
		}
		image_server.set_main_thread_id();

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
		splash += " == Center for Genomic Regulation 2017 ==\n";
		splash += " ==     Harvard Medical School 2015    ==\n";
		splash += " ==                                    ==\n";
		splash += " ========================================\n";
		std::cout << splash;
		
		std::cout << image_server.get_system_host_name() << "\n";

		std::vector<std::pair<std::string,std::string> > quotes;
	
		ns_acquire_for_scope<ns_image_server_sql> sql;
		
		image_server.os_signal_handler.set_signal_handler(ns_interrupt,exit_signal_handler);
		
		//ns_optical_flow flow;
		//flow.test();
		#ifndef _WIN32
			//start a crash daemon to handle server crashes
			ns_image_server_crash_daemon crash_daemon;
		       
			while(true){
			    if (crash_daemon.start(image_server.server_crash_daemon_port())==ns_image_server_crash_daemon::ns_ok)
			      break;
				else{
					if (sql.is_null())
						sql.attach(ns_connect_to_available_sql_server());

					if (sql().connected_to_central_database()){
						image_server.alert_handler.initialize(*static_cast<ns_sql *>(&sql()));
						std::string text("The image server node ");
						text += image_server.host_name_out() + " restarted after a fatal error at ";
						text += ns_format_time_string_for_human(ns_current_time());
						ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
						image_server.alert_handler.initialize(sql());

							ns_alert alert(text,
								text,
								ns_alert::ns_server_crash,
								ns_alert::get_notification_type(ns_alert::ns_server_crash,false),
								ns_alert::ns_rate_limited
								);

						image_server.alert_handler.submit_alert(alert,*static_cast<ns_sql *>(&sql()));
					}
					else{
						image_server.register_server_event(ns_image_server_event("The image server has restarted after a crash."),&sql());
					}
			    }
			}
			  
			//register a swath or error handlers to allow recovery from segfaults and such under linux
			image_server.os_signal_handler.set_signal_handler(ns_segmentation_fault,segmentation_fault_signal_handler);
			image_server.os_signal_handler.set_signal_handler(ns_bus_error,bus_error_signal_handler);
			image_server.os_signal_handler.set_signal_handler(ns_illegal_instruction,illegal_instruction_signal_handler);
			image_server.os_signal_handler.set_signal_handler(ns_floating_point_error,floating_point_error_signal_handler);
			while(!ns_image_server_crash_daemon::daemon_is_running()){
			  ns_thread::sleep(1);
			}
		#endif


		//don't act as an image capture server if we just want to copy over images.
		if (post_dispatcher_init_command == ns_run_pending_image_transfers){
			image_server.override_ini_specified_image_capture_server_behavior(false);
		}

		if (sql.is_null())
				sql.attach(ns_connect_to_available_sql_server());

		image_server.image_storage.refresh_experiment_partition_cache(&sql());
		if (sql().connected_to_central_database()){
			image_server.clear_performance_statistics(*static_cast<ns_sql *>(&sql()));
			image_server.clear_old_server_events(*static_cast<ns_sql *>(&sql()));
			image_server.load_quotes(quotes,*static_cast<ns_sql *>(&sql()));
			image_server.alert_handler.initialize(*static_cast<ns_sql *>(&sql()));	
			image_server.alert_handler.reset_all_alert_time_limits(*static_cast<ns_sql *>(&sql()));
			ns_death_time_annotation_flag::get_flags_from_db(*static_cast<ns_sql *>(&sql()));
		}
		
		const bool register_and_run_simulated_devices(image_server.register_and_run_simulated_devices(&sql()));
		srand(ns_current_time());
		std::string quote;
		if (quotes.size() == 0)
			quote = "The oracle is silent.\n\n";
		else{
			std::pair<std::string,std::string> & q(quotes[rand()%quotes.size()]);
			if (q.first.size() == 0)
				quote = "The oracle is silent.\n\n";
			else{
				quote = "\"" + q.first + "\"\n";
				quote += "\t--";
				quote += q.second + "\n\n";
			}
		}
		std::cout << quote;
			
		#ifdef _WIN32 
		if (image_server.hide_window()){

				HWND console_hwnd = GetConsoleHwnd();
				ShowWindow(console_hwnd,SW_HIDE);
				ns_window_hidden = !ns_window_hidden;
		}
		
		#endif

		#ifndef _WIN32
			
			int res =  umask(S_IWOTH);//S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH|S_IWOTH);
			cout.setf(ios::oct);
		//	cout << "Changing mask from " << res << " to " << res2 << "\n";
			cout.setf(ios::dec);
		#endif

		if (sql().connected_to_central_database()){
			image_server.get_requested_database_from_db();
			image_server.register_host(&sql());
			if (command != ns_update_sql) {  //we can't register events in the db until the db is updated
				image_server.register_server_event(ns_image_server_event("Launching server..."), &sql(), true);
				image_server.add_subtext_to_current_event(splash, &sql(), true);
				image_server.add_subtext_to_current_event(quote, &sql(), true);
			}
			if (image_server.new_software_release_available() && image_server.halt_on_new_software_release()){
				if (command != ns_update_sql)  //we can't register events in the db until the db is updated
					image_server.register_server_event(ns_image_server_event("A more recent version of server software was found running on the cluster.  This server is outdated and is halting now."),&sql());
				#ifdef _WIN32 
				image_server.update_software = true;
				#endif
				throw ns_ex("Updated software detected on the cluster.");
			}
		}

		image_server.image_storage.test_connection_to_long_term_storage(true);
		if (!image_server.image_storage.long_term_storage_was_recently_writeable() && image_server.act_as_processing_node()) {
			if (image_server.act_as_an_image_capture_server()) {
				image_server.set_processing_node_behavior(false);
				image_server.register_server_event(ns_image_server_event("Cannot connect to long term storage.  This server will not act as an image processing node."), &sql());
			}
			else throw ns_ex("Cannot connect to long term storage.");
		}

		//handle requested commandline commands that requre a database
		switch(command){
			case ns_fix_orphaned_captured_images:{
		  		ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
				image_server.image_storage.fix_orphaned_captured_images(&sql());
				sql.release();
				return 0;
			}
			case ns_update_sql:{
				ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
				image_server.upgrade_tables(sql(),false,schema_name);
				sql.release();
				return 0;
			}
			case ns_test_email:{
				std::string text("Image server node ");
				text += image_server.host_name_out();
				text += " has succesfully sent an email.";
				ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
				image_server.alert_handler.initialize(sql());
				image_server.alert_handler.submit_desperate_alert(text);
				sql.release();
				return 0;
			}
			case ns_test_alert:{
				std::string text("At ");
				text += ns_format_time_string_for_human (ns_current_time()) + " a cluster node submitted a test alert.";
				ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
				image_server.alert_handler.initialize(sql());
				ns_alert alert(text,
					text + "  If you are recieving this message, it has been handled successfully.",
					ns_alert::ns_test_alert
					,ns_alert::get_notification_type(ns_alert::ns_test_alert,false),
					ns_alert::ns_not_rate_limited
					);

				image_server.alert_handler.submit_alert(alert,sql());
				sql.release();
				return 0;
			}
			case ns_test_rate_limited_alert:{
				ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
					image_server.alert_handler.initialize(sql());		
					std::string text("At ");
					text += ns_format_time_string_for_human (ns_current_time()) + " a cluster node submitted a rate limited test alert.";
					ns_alert alert(text,
						text + "  If you are recieving this message, it has been handled successfully.",
						ns_alert::ns_test_alert
						,ns_alert::get_notification_type(ns_alert::ns_test_alert,false),
						ns_alert::ns_rate_limited
						);

					image_server.alert_handler.submit_alert(alert,sql());
					sql.release();
					return 0;
			}
				
			case ns_restarting_after_a_crash:{
				std::string text("The image server node ");
				text += image_server.host_name_out() + " restarted after a fatal error at ";
				text += ns_format_time_string_for_human(ns_current_time());
				ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
				image_server.alert_handler.initialize(sql());

					ns_alert alert(text,
						text,
						ns_alert::ns_server_crash,
						ns_alert::get_notification_type(ns_alert::ns_server_crash,true),
						ns_alert::ns_rate_limited
						);

				image_server.alert_handler.submit_alert(alert,sql());
				sql.release();
				restarting_after_crash = true;
				break;
			}
		}

		image_server.register_server_event(ns_image_server_event("Clearing local image cache"),&sql());
		image_server.image_storage.clear_local_cache();

	
	//	else image_server.image_storage.set_verbosity(ns_image_storage_handler::ns_deletion_events);
	
		

		if (post_dispatcher_init_command == ns_trigger_segfault_in_main_thread){
		  cerr << "Triggering Segfault...\n";
			char * a(0);
			(*a)++;
		}
		ns_image_server_dispatcher dispatch(true);

		const unsigned long dispatcher_offset_time(image_server.dispatcher_refresh_interval());

		if (!restarting_after_crash){
			if (is_master_node){
				image_server.set_up_model_directory();
			}
			dispatch.init(image_server.dispatcher_port(), dispatcher_socket_queue_length);
		}
		else{
			//if we are restarting after the crash, it is possible the previous 
			//socket may be inaccessible.  In this case we search around until we find an available port.
			cerr << "Searching for the next available port after crash.\n";
			bool found_available_socket(false);
			for (unsigned int i  =1; i < 1024; i++){
				for (unsigned int j = 0; j < 2; j++){
					try{
						dispatch.init(image_server.dispatcher_port(), dispatcher_socket_queue_length);
							found_available_socket = true;
							break;
					}
					catch(ns_ex & ex){
						cerr << "Could not bind to " << image_server.dispatcher_port() << ": " << ex.text();
						ns_thread::sleep(4);
					}
				}
				if (found_available_socket)
					break;
				else image_server.increment_dispatcher_port();
			}
		}
		if (post_dispatcher_init_command == ns_trigger_segfault_in_dispatcher_thread)
			dispatch.trigger_segfault_on_next_timer();

		
		ns_acquire_for_scope<ns_sql> sql_2(image_server.new_sql_connection(__FILE__,__LINE__));
		if (image_server.upgrade_tables(sql_2(),true,image_server.current_sql_database())){
			throw ns_ex("The current database schema is out of date.  Please run the command: ns_image_server update_sql [schema_name]");
		}
		sql_2.release();

		//search for devices
		if (image_server.act_as_an_image_capture_server()){
			image_server.set_up_local_buffer();
			if (image_server.device_manager.load_last_known_device_configuration()){
				cerr << "Last known configuration:\n";
				image_server.device_manager.output_connected_devices(cerr);
				cerr << "\n";
				image_server.device_manager.hotplug_new_devices();
			}
			else image_server.device_manager.clear_device_list_and_identify_all_hardware();
		}
		else image_server.register_server_event(ns_image_server_event("Not searching for attached devices."),&sql());
	
		if (register_and_run_simulated_devices){
			if (image_server.simulated_device_name().size() > 2)
				image_server.device_manager.attach_simulated_device(image_server.simulated_device_name());
		}
		if (image_server.act_as_an_image_capture_server())
			image_server.device_manager.save_last_known_device_configuration();

		if (sql().connected_to_central_database())
			image_server.register_devices(false,&sql());
		
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
		if (sql().connected_to_central_database())
			dispatch.connect_timer_sql_connection();
		
		sql.release();
		unsigned int * timer_interval = new unsigned int(image_server.dispatcher_refresh_interval());
		
		
		if (post_dispatcher_init_command == ns_run_pending_image_transfers){
			dispatch.buffered_capture_scheduler.image_capture_data_manager.handle_pending_transfers_to_long_term_storage_using_db_names();
		}
		else{

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

			image_server.exit_requested = true;
			timer.block_on_finish();
			dispatcher_thread.block_on_finish();
			#else
			dispatch.run();
			image_server.exit_requested = true;
			timer.block_on_finish();
			#endif
		}
		
		//cerr << "Clearing dispatcher\n";
		dispatch.clear_for_termination();
		#ifndef _WIN32
		ns_socket::global_clean();
		#endif
		
		//if (is_master_node){
		//	cerr << "Waiting for external processes...\n";
		//	ns_request_shutdown_of_all_spawned_nodes(child_processes);
		//	ns_wait_for_all_spawned_nodes(child_processes);
		//	#ifndef _WIN32
		//	ns_image_server_crash_daemon::request_daemon_shutdown();
		//	#endif
		//}
		
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
		if (is_master_node && image_server.update_software && image_server.handle_software_updates()){
			try{
				ns_update_software();
			}
			catch(ns_ex & ex){
				cerr << ex.text() << "\n";
			}

		}
	#endif
	image_server.clear();
	ns_sql::unload_sql_library();


	return 0;
}

