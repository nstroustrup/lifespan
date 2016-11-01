#include "ns_thread.h"
#include "ns_ex.h"
#ifndef _WIN32
#include <sys/wait.h>
#include <signal.h>
#include <unistd.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/syscall.h>
#endif
#include <cerrno>
#include <string.h>
#include <stdio.h>

#ifdef _DEBUG
//#define NS_THREAD_DEBUG
#endif

#ifdef NS_THREAD_DEBUG
#include "ns_dir.h"
#include "ns_high_precision_timer.h"
#endif


#ifdef NS_THREAD_DEBUG
bool ns_output_lock_init(false);
class ns_output_lock : public ns_lock{
public:
	ns_output_lock(const std::string & n):ns_lock(n){ns_output_lock_init = true;}
	~ns_output_lock(){ns_output_lock_init = false;}
};
ns_output_lock output_lock("ns_lo");
#endif

/*******************************************
ns_thread: a multi-platform thread library
This library provides a thin wrapper on POSIX
and Windows threads to provide generic threading support
for Windows, Linux, and OSX machines.
(C) Nicholas Stroustrup 2006
*******************************************/
#include <iostream>
using namespace std;

void ns_simple_execute(const std::string & command,const std::vector<std::string> & args){

	#ifdef _WIN32  
		PROCESS_INFORMATION proc_info;
		STARTUPINFO start_info;
		ZeroMemory(static_cast<void *>(&proc_info),sizeof(PROCESS_INFORMATION));
		ZeroMemory(static_cast<void *>(&start_info),sizeof(STARTUPINFO));
		start_info.cb = sizeof(STARTUPINFO);
		string commandline(command);
		for (unsigned int i = 0; i < args.size(); i++){
			commandline += " ";
			commandline+=args[i];
		}

		char * cmd = new char[command.size() + 1];
	   for (unsigned int i = 0; i < command.size(); i++)
	     cmd[i] = command[i];
	   cmd[command.size()] = 0;

		if (0 == CreateProcess((LPSTR)command.c_str(),
				  (LPSTR)commandline.c_str(),
				  NULL,
				  NULL,
				  TRUE,
				  0,
				  NULL,
				  NULL,
				  &start_info,
				  &proc_info)){
			delete[] cmd;
			ns_ex ex("ns_external_execute::Could not create new process: ");
			ex << command << "(";
			ex.append_windows_error();
			ex << ")";
			throw ex;
		}


	#else
		char  ** argv = new char* [args.size()+1];
		for (unsigned int i = 0; i < args.size(); i++){
		  argv[i] = new char[args[i].size()+1];
		  strcpy(argv[i],args[i].c_str());
		  strcat(argv[i],0);
		}
		argv[args.size()] = 0;
		execv(command.c_str(),argv);
		for (unsigned int i = 0; i < args.size(); i++){
		  delete argv[i];
		}
		delete argv;
		throw ns_ex("ns_external_execute::Could not create new process: ") << command;;
	#endif
}

unsigned long ns_thread::ns_get_process_id(){
	#ifdef _WIN32
		return GetCurrentProcessId();
	#else
		return getpid();
	#endif
}

void ns_thread::run( ns_thread_function_pointer fptr, void * args){
	#ifdef _WIN32 
		handle.close();
		HANDLE h(::CreateThread (
            0, // security attributes
            0, // stack size
            (LPTHREAD_START_ROUTINE)(fptr),
            args,
            0,
            &handle.id));
		if (h == NULL){
			ns_ex ex("Could not create thread: ");
			ex.append_windows_error();
			throw ex;
		}
		handle.set_handle(h);
	#else
		int ret = pthread_create( &handle, NULL, fptr, args);
		if (ret != 0)
			throw ns_ex("Could not create thread.");

	#endif
}

void ns_thread::set_priority(const ns_thread_priority p){
	
	#ifdef _WIN32 
		int win_p;
		switch (p){
			case NS_THREAD_LOW: win_p = THREAD_PRIORITY_IDLE; break;
			case NS_THREAD_NORMAL: win_p = THREAD_PRIORITY_NORMAL; break;
			case NS_THREAD_HIGH: win_p = THREAD_PRIORITY_HIGHEST; break;
			default: throw ns_ex("Unknown thread priority level specified");
		}
		int ret = SetThreadPriority(handle.handle(),win_p);
		if (ret == 0)
			throw ns_ex("Could not set thread priority");
	#else


	#endif

}

void ns_thread::close(){
	#ifdef _WIN32 
		//can't terminate threads in windows?
		handle.close();
	#else
		int ret = pthread_cancel(handle);
		if (ret != 0)
			throw ns_ex("Could not close thread.");
	#endif
}
void ns_thread::block_on_finish(const unsigned int wait_time){
	#ifdef _WIN32 
		DWORD _wait_time = INFINITE;
		if (wait_time != 0)
			_wait_time = wait_time;
		DWORD ret = WaitForSingleObject (handle.handle(), _wait_time);
		if (ret == WAIT_FAILED)
			throw ns_ex("Could not wait for thread.");
		handle.close();
	#else
		 int ret = pthread_join(handle, NULL);
		 if (ret != 0){
			ns_ex ex("Could not wait for thread.");
			switch(ret){
			case EINVAL: ex << "EINVAL"; break;
			case ESRCH: ex << "ERSCH"; break;
			case EDEADLK: ex << "EDEADLK"; break;
			default: ex << "Unknown pthread error";

			}
			throw ex;
		}

	#endif
}

void ns_thread::sleep(const unsigned int seconds){
#ifdef _WIN32 
	::Sleep(seconds*1000);

#else
	::sleep(seconds);
#endif
}

void ns_thread::sleep_milliseconds(const ns_64_bit & milliseconds){
	#ifdef _WIN32 
	::Sleep(milliseconds);
#else
	timespec t, res;
	t.tv_sec = milliseconds / 1000;
	t.tv_nsec = (milliseconds % 1000) * 1000 * 1000;
	::nanosleep(&t,&res);
#endif
}

#ifdef _WIN32 
void ns_thread_handle_closer::operator()(const HANDLE & h){
	int ret = ::CloseHandle(h);
	if (ret == 0)
		throw ns_ex("Could not close thread.");
}
#endif

void ns_thread::detach(){
	#ifdef _WIN32 
	handle.close();
	//do nothing
	#else
		int ret = pthread_detach(handle);
		if (ret != 0)
			throw ns_ex() << "Error detaching thread!";
	#endif
}

bool ns_thread::still_running(){
	#ifdef _WIN32 
		DWORD exit_code;
		int ret = GetExitCodeThread(handle.handle(),&exit_code);
		if (ret != 0)
			throw ns_ex() << "Error getting thread state!";
		if (exit_code == STILL_ACTIVE)
			return true;
		else return false;
	#else
		throw ns_ex("ns_thread::still_running() called on a POSIX thread!");
	#endif
}
#ifdef _WIN32 
ns_thread_handle::ns_thread_handle(const ns_thread_handle & h){
			id = h.id;
			if (h.have_handle)
				set_handle(h.handle_); 
			else{
				have_handle = false;
				handle_ = h.handle_;
			}
		}
#endif
ns_64_bit ns_thread::current_thread_id() {
#ifdef _WIN32
	return GetCurrentThreadId();
#else

        pid_t tid;
	tid = (pid_t) syscall (SYS_gettid);
	return (ns_64_bit)tid;
#endif

}
ns_thread ns_thread::get_current_thread(){

	ns_thread self;

	#ifdef _WIN32 
	//GetCurrentThread() returns a "pseudohandle" which cannot be closed.
		self.handle.set_unmanaged_handle(GetCurrentThread());
	#else
		self.handle = pthread_self();
	#endif

	return self;
}

void ns_external_execute::wait_for_termination(){
	#ifdef _WIN32 
	DWORD result;
	/*result = ::WaitForInputIdle(proc_info.hProcess,INFINITE);
	ns_ex ex("ns_external_execute::Could not wait for process input idle state: ");
	switch(result){
		case WAIT_FAILED: 
			ex << " Failed: ";
			ex.append_windows_error();
			throw ex;
		case WAIT_TIMEOUT:
			ex << " Timeout: ";
			ex.append_windows_error();
			throw ex;
	}*/

	result = ::WaitForSingleObject(proc_info.hProcess, INFINITE);
	ns_ex ex2("ns_external_execute::Could not wait for process termination: ");
	switch(result){
		case WAIT_FAILED: 
			ex2 << " Failed: ";
			ex2.append_windows_error();
			throw ex2;
		case WAIT_ABANDONED:
			ex2 << " Abandoned ";
			ex2.append_windows_error();
			throw ex2;
		case WAIT_TIMEOUT:
			ex2 << " Timeout: ";
			ex2.append_windows_error();
			throw ex2;
	}
	CloseHandle(proc_info.hProcess);
	CloseHandle(proc_info.hThread);
	#else
		if (child_pid != -1){
		//	cerr << "Waiting for termination";
			int res = ::waitpid(child_pid,0,0);
			//cerr << "Done.";
			int cp = child_pid;
			child_pid=-1;
			if (res <= 0)
				throw ns_ex("ns_external_execute::Could not wait for child (pid=") << cp << ") termination.";
		}
	#endif
}

bool ns_external_execute::run(const std::string & command, const std::string & parameters, const ns_external_execute_options & opt){

	opts = opt;
	#ifdef _WIN32 
	char p[2];
	if (opt.take_stdin_handle)	p[0] = 'w';
	else		p[0] = 'r';
	if (opt.binary) p[1] = 'b';
	else		p[1] = 't';

	//std::cerr << "Executing Command: " << com << "\n\n";

	std::string com = "\"";
	com += command;
	com+="\" ";
	if (parameters.size() > 0)
		com+=parameters;
	

	//	com = std::string("\"") + com + "\"";
	SECURITY_ATTRIBUTES sattr;
	sattr.nLength = sizeof(SECURITY_ATTRIBUTES);
	sattr.bInheritHandle = TRUE;
	sattr.lpSecurityDescriptor = NULL;
	if (opt.take_stdout_handle){
	if (! CreatePipe(&hChildStdoutRd, &hChildStdoutWr, &sattr, 0))
		throw ns_ex("ns_external_execute::Could not create pipe STDOUT for new child process.");
		SetHandleInformation(hChildStdoutRd, HANDLE_FLAG_INHERIT, 0);
	}
	else{
		hChildStdoutRd = (HANDLE)-1;
		hChildStdoutWr = (HANDLE)-1;
	}
	if (opt.take_stderr_handle){
	if (! CreatePipe(&hChildStderrRd, &hChildStderrWr, &sattr, 0))
	 throw ns_ex("ns_external_execute::Could not create pipe STDERR for new child process.");
	SetHandleInformation(hChildStderrRd, HANDLE_FLAG_INHERIT, 0);
	}
	else{
	hChildStderrWr = (HANDLE)-1;
		hChildStderrRd = (HANDLE)-1;
	}

	if (opt.take_stdin_handle){
	if (!CreatePipe(&hChildStdinRd, &hChildStdinWr, &sattr, 0))
		throw ns_ex("ns_external_execute::Could not create STDIN pipe for new child process.");
		SetHandleInformation(hChildStdinWr, HANDLE_FLAG_INHERIT,0);
	}
	else{
		hChildStdinWr = (HANDLE)-1;
		hChildStdinRd = (HANDLE)-1;
	}


	//create process
	STARTUPINFO start_info;
	ZeroMemory(static_cast<void *>(&proc_info),sizeof(PROCESS_INFORMATION));
	ZeroMemory(static_cast<void *>(&start_info),sizeof(STARTUPINFO));
	start_info.cb = sizeof(STARTUPINFO);

	start_info.hStdError = hChildStderrWr;
	start_info.hStdOutput = hChildStdoutWr;
	start_info.hStdInput = hChildStdinRd;
	if (opt.take_stderr_handle || opt.take_stdin_handle || opt.take_stdout_handle)
		start_info.dwFlags |= STARTF_USESTDHANDLES;

	char * cmd = new char[com.size() + 1];
	for (unsigned int i = 0; i < com.size(); i++)
		cmd[i] = com[i];
	//com._Copy_s(cmd,com.size(),com.size());
	cmd[com.size()] = 0;


	if (0 == CreateProcess(
		NULL, //lpApplicationName,
		cmd, //lpCommandLine
		  NULL, //lpProcessAttributes
		  NULL, //lpThreadAttributes
		  TRUE, //bInheritHandles
		  0,	//dwCreationFlags
		  NULL, //lpEnvironment
		  NULL, //lpCurrentDirectory
		  &start_info, //lpStartupInfo,
		  &proc_info //lpProcessInformation
		  )){
			delete[] cmd;
			ns_ex ex("ns_external_execute::Could not create new process:  ");
			ex << command << " (";
			ex.append_windows_error();
			ex << ")";
			throw ex;
	}
	else{
		delete[] cmd;

		if (opt.discard_stdout)
			finished_reading_from_stdout();
		if (opt.discard_stderr)
			finished_reading_from_stderr();
		if (opt.discard_stdin)
			finished_writing_to_stdin();
	}
	closed = false;
	return true;

	#else
	if (opt.take_stdout_handle && pipe(c_stdout) != 0)
		throw ns_ex("ns_external_execute::Could not open stdout pipe.");
	if (opt.take_stderr_handle && pipe(c_stderr) != 0)
		throw ns_ex("ns_external_execute::Could not open sterr pipe.");
	if (opt.take_stdin_handle && pipe(c_stdin) != 0)
		throw ns_ex("ns_external_execute::Could not open stin pipe.");
	int pid = fork();
	if (pid == -1)
		throw ns_ex("ns_external_execute::Could not fork off a process: ") << strerror(errno);
	if (opt.only_fork)
		return pid!=0;

	//child process
	if (pid == 0){

		std::string com = command;
		std::vector<std::string> s_parameters;
		bool in_str = false;
		std::string cur_string;
		s_parameters.push_back(command);
		for (int i = 0; i < parameters.size(); i++){
			if (parameters[i] == '"'){
				in_str = !in_str;
				continue;
			}
			if (!in_str && parameters[i] == ' '){
				if (cur_string.size() != 0){
					s_parameters.push_back(cur_string);
					cur_string.resize(0);
				}
			}
			else cur_string+=parameters[i];
		}
		if (cur_string.size() != 0)
			s_parameters.push_back(cur_string);


		if (opt.take_stdout_handle){
			if (dup2(c_stdout[1],fileno(stdout)) == -1)
					throw ns_ex("ns_external_execute::Could not redirect stdout");
			::close(c_stdout[1]);
		}
		if (opt.take_stderr_handle){
			if (dup2(c_stderr[1],fileno(stderr)) == -1)
			throw ns_ex("ns_external_execute::Could not redirect stderr");
			::close(c_stderr[1]);
		}
		if(opt.take_stdin_handle){
			if (dup2(c_stdin[0],fileno(stdin)) == -1)
			throw ns_ex("ns_external_execute::Could not redirect stdin");
			::close(c_stdin[0]);
		}



		char * * param = new char *[s_parameters.size()+1];
		for (unsigned int i = 0; i < s_parameters.size(); i++){
			param[i] = new char[s_parameters[i].size()+1];
			strcpy(param[i],s_parameters[i].c_str());
		}
		param[s_parameters.size()] = 0;
		if (execv(command.c_str(),param) == -1){
			for (unsigned int i = 0; i < s_parameters.size(); i++){
				delete[] param[i];
			}
			delete[] param;
			throw ns_ex("ns_external_execute: Could not execute specified command.:") << com;

		}
		return false;
	}
	//parent process
	else{
		child_pid = pid;	
		if (opt.take_stdin_handle)
			::close(c_stdin[0]);
		if (opt.take_stdout_handle)
			::close(c_stdout[1]);
		if (opt.take_stderr_handle)
	  		::close(c_stderr[1]);
	  	//if we don't want to write anything to the forked process, close the pipe.
		if (opt.discard_stdout)
			finished_reading_from_stdout();
		if (opt.discard_stderr)
			finished_reading_from_stderr();
		if (opt.discard_stdin)
			finished_writing_to_stdin();
	//   exit(0);
	  	
	}

	return true;
	#endif
}

void ns_timeout_process_handle::terminate(){
			
	#ifdef _WIN32 
	::TerminateProcess(process_handle,1);
	#else
		//we need to test that at least one of the file descriptors is
		//still available.  If we just go ahead with killing the PID
		//without confirming it is still there, we might terminate
		//a new, unrelated process created with a reused pid
	//	if (::close(stdio_pipe)==0 || !::close(stderr_pipe)==0 || ::close(stdin_pipe)==0){
			::kill( pid, SIGKILL );
			int status;
			::waitpid(pid,&status,0);
	//	}
	#endif
}

void ns_timeout_runner::run(){
	//cerr << "Starting timeout\n";
	ns_thread::sleep(timeout);
	//check to see if the timeout is still needed
//	cerr << "Requesting termination\n";
	if (termination_daemon().request_permission_to_terminate(true)){
	//	cerr << "Got it!";
		handle.terminate();
	}
	//cerr << "Ending timeout\n";
}

void ns_timeout_runner::start_timeout(){
	ns_thread r(run_timeout,this);
}

void  ns_process_termination_manager::cancel_timeout(){
	if (!started_timeout)
		return;
	termination_daemon().cancel_timeout();
}
bool  ns_process_termination_manager::request_permission_to_terminate(){
	return termination_daemon().request_permission_to_terminate(false);
}
bool  ns_process_termination_manager::process_has_been_terminated(){
	
	return termination_daemon().process_has_been_terminated();
}

void ns_process_termination_manager::start_timeout(unsigned long seconds,ns_timeout_process_handle & handle){
	if (started_timeout)
		throw ns_ex("ns_process_termination_manager::Cannot register multiple timeout intervals");

	started_timeout = true;
	ns_timeout_runner * timeout_runner(new ns_timeout_runner(seconds,handle,termination_daemon));

	//start timeout runner
	timeout_runner->start_timeout();

}
void ns_process_termination_daemon::cancel_timeout(){
	//cerr << "Canceling timeout\n";
	process_termination_lock.wait_to_acquire(__FILE__,__LINE__);
	timeout_cancellation_requested = true;
	process_termination_lock.release();
}

ns_process_termination_manager::ns_process_termination_manager():termination_daemon(new ns_process_termination_daemon()),started_timeout(false){}
bool ns_process_termination_daemon::request_permission_to_terminate(bool caller_is_timeout_runner){
	process_termination_lock.wait_to_acquire(__FILE__,__LINE__);

	const bool refuse_termination(caller_is_timeout_runner && timeout_cancellation_requested);

	const bool grant_permission = !process_terminated && !refuse_termination;

	if (!refuse_termination)
		process_terminated = true;

	process_termination_lock.release();
	return grant_permission;
}

ns_thread_return_type ns_timeout_runner::run_timeout(void * d){
	ns_timeout_runner * m(static_cast<ns_timeout_runner *>(d));
	try{
		//clear up any thread metadata that would be left over when we delete ourselves.
		ns_thread::get_current_thread().detach();
		
		m->run();

		delete m;
	}
	catch(ns_ex & ex){
		std::cerr << "ns_timeout_manager_error::" << ex.text() << "\n";
	}
	catch(...){
		std::cerr << "ns_timeout_manager_error::Unknown Error\n";
	}
	return 0;
}

void ns_external_execute::start_timeout(unsigned long seconds){
	ns_timeout_process_handle handle(get_handle());
	timeout_manager.start_timeout(seconds,handle);
};
ns_timeout_process_handle ns_external_execute::get_handle(){
	ns_timeout_process_handle handle;
	#ifdef _WIN32 
			handle.process_handle = proc_info.hProcess;
	#else
			handle.pid = child_pid;
			handle.stdio_pipe = c_stdout[0];
			handle.stderr_pipe = c_stderr[0];
			handle.stdin_pipe = c_stdin[1];
	#endif
	return handle;
}

void ns_external_execute::terminate_process(){
	timeout_manager.cancel_timeout();
	if (!timeout_manager.request_permission_to_terminate())
		return;
	ns_timeout_process_handle handle(get_handle());
	handle.terminate();
#ifdef _WIN32 
	closed = true;
#endif
}

bool ns_external_execute::timed_out(){
	return timeout_manager.process_has_been_terminated();
}
unsigned int ns_external_execute::read_stdout(char * a,const unsigned int length){
 
	if (!opts.take_stdout_handle)
		throw ns_ex("Attempting to read stdout from a process not opened for reading stdout.");

	#ifdef _WIN32 	
		DWORD bytes_read;
		if (ReadFile(hChildStdoutRd, a, length, &bytes_read,NULL) == 0){
				if (GetLastError() == ERROR_BROKEN_PIPE)
					return 0;
			ns_ex ex("ns_external_execute::Error during stdout read: ");
			ex.append_windows_error();
			throw ex;
		}
		return bytes_read;
	#else
		int res = read(c_stdout[0],a,length);
		if (res == -1)
			throw ns_ex("ns_external_execute::Error during stdout read:  ") << strerror(errno);
		//	cerr << "Done (" << res << ")";
		//cerr << "Read " << res << " bytes from stdout.\n";
	//	finished_reading_from_stdout();
		return res;
	#endif
}

unsigned int ns_external_execute::read_stderr(char * a,const unsigned int length){
  
	if (!opts.take_stderr_handle)
		throw ns_ex("Attempting to read stderr from a process not opened for reading stderr.");

	#ifdef _WIN32 
		DWORD bytes_read;
		//cerr << "Blocking on stderr readfile.\n";
		if (ReadFile(hChildStderrRd, a, length, &bytes_read,NULL) == 0){
				if (GetLastError() == ERROR_BROKEN_PIPE)
					return 0;
			ns_ex ex("ns_external_execute::Error during sterr read:  ");
			ex << strerror(errno);
			ex.append_windows_error();
			throw ex;
		}
		//	cerr << "Stderr readfile returned " << bytes_read << " bytes.\n";
		return bytes_read;
	#else
		int res = read(c_stderr[0],a,length);
		//"Read " << res << " bytes from stderr.\n";
		if (res == -1)
			throw ns_ex("ns_external_execute::Error during stderr read:  ") << strerror(errno);
		//finished_reading_from_stderr();
		//	cerr << "done.";
		return res;
	#endif
}

/*bool ns_external_execute::fail(){
	#ifdef _WIN32 
		
	#else
		return (ferror(fstr) || feof(fstr));
	#endif
}*/
unsigned int ns_external_execute::write_stdin(char * a, const unsigned int length){
	if (!opts.take_stdin_handle)
		throw ns_ex("Attempting to write to a process not opened for writing");
	#ifdef _WIN32 
		DWORD bytes_written;
		if (WriteFile(hChildStdinWr, a, length,&bytes_written,NULL) == 0){
		  ns_ex ex("ns_external_execute::Error during read: ");
			ex.append_windows_error();
			throw ex;
		}
		return bytes_written;
	#else
		return write(c_stdin[1],a,length);
	#endif
}

void ns_external_execute::finished_writing_to_stdin(){
	if (!opts.take_stdin_handle)
		return;
	#ifdef _WIN32 
			if (hChildStdinWr != 0 && hChildStdinWr != (HANDLE)-1)
				if (!CloseHandle(hChildStdinWr))
					throw ns_ex("ns_external_execute::Closing child's STDIN failed.");
			//if (hChildStdinRd != 0)
			//	if (!CloseHandle(hChildStdinRd))
			//		throw ns_ex("ns_external_execute::Closing STDIN failed.");
			if (hChildStdoutWr!= 0)
				if (!CloseHandle(hChildStdoutWr)) 
					throw ns_ex("ns_external_execute::Closing child's STDOUT failed.");
		hChildStdinRd = 0;
		hChildStdinWr = 0;
		
		hChildStdoutWr = 0;
	#else
		if (c_stdin[1] != -1)
		  ::close(c_stdin[1]);
		c_stdin[1] = -1;
	#endif
}

void ns_external_execute::finished_reading_from_stdout(){
	if (!opts.take_stdout_handle)
		return;

	#ifdef _WIN32 
		
		if (hChildStdoutRd!= 0)
			if (!CloseHandle(hChildStdoutRd)) 
			throw ns_ex("ns_external_execute::Closing child's STDOUT failed.");

		hChildStdoutRd = 0;
	#else
		if (c_stdout[0] != -1)
		  ::close(c_stdout[0]);
		c_stdout[0] = -1;
	#endif
}
void ns_external_execute::finished_reading_from_stderr(){
	if (!opts.take_stderr_handle)
		return;
	
	#ifdef _WIN32 
		if (hChildStderrWr!= 0){
			if (!CloseHandle(hChildStderrWr)){
				hChildStderrWr = 0;
				throw ns_ex("ns_external_execute::Closing child's STDOUT failed.");
			}
			hChildStderrWr = 0;
		}
		if (hChildStderrRd!= 0){
			if (!CloseHandle(hChildStderrRd)){
				hChildStderrRd = 0;
				throw ns_ex("ns_external_execute::Closing child's STDOUT failed.");
			}
			hChildStderrRd = 0;
		}
	#else
		if (c_stderr[0] != -1)
		  ::close(c_stderr[0]);
		c_stderr[0] = -1;
	#endif
}
void  ns_external_execute::release_io(){
	
	#ifdef _WIN32 
	if (closed) return;
	closed = true;
	#endif
	if (timeout_manager.process_has_been_terminated())
		return;
	finished_writing_to_stdin();
	finished_reading_from_stdout();
	finished_reading_from_stderr();	
}

ns_timeout_runner::ns_timeout_runner(unsigned long timeout_,ns_timeout_process_handle & handle_,ns_process_termination_daemon_ptr & daemon):
		timeout(timeout_),handle(handle_),termination_daemon(daemon),started(false),start_lock("ns_start_timeout_thread_lock"){}

ns_external_execute::~ns_external_execute(){
	release_io();

}

ns_process_termination_daemon & ns_process_termination_daemon_ptr::operator()(){
	if (daemon==0)
		throw ns_ex("ns_process_termination_daemon_ptr()::Accessing cleared pointer!");
	return *daemon;
}
void ns_process_termination_daemon_ptr::acquire(ns_process_termination_daemon * d){
	daemon = d;
	daemon->ptr_count++;
}
void ns_process_termination_daemon_ptr::release(){
	if (daemon==0)
		return;
	daemon->ptr_count--;
	if (daemon->ptr_count < 0)
		throw ns_ex("ns_process_termination_daemon_ptr::Removing already cleared pointer reference");
	if (daemon->ptr_count == 0)
		delete daemon;
	daemon = 0;
}
ns_process_termination_daemon_ptr::~ns_process_termination_daemon_ptr(){
	release();
}


void ns_lock::init(ns_mutex_handle & mutex){
#ifdef _WIN32 
	InitializeCriticalSection(&mutex);
#else
	if (pthread_mutex_init(&mutex,0))
		throw ns_ex("ns_lock(") << name << ")::Cannot initialize mutex!";
#endif
}

//we need to get (or create) the mutex handle for the specified lock name.
void ns_lock::init(const std::string & lock_name_){
	currently_holding = false;
	name = lock_name_;
	init(this->mutex_handle);
}

void ns_lock::wait_to_acquire(ns_mutex_handle & mutex){
#ifdef _WIN32 
	EnterCriticalSection(&mutex);
#else
	int status = pthread_mutex_lock(&mutex);
	if (status){
	  std::string err;
	  switch(status){
		  case EINVAL: err = "EINVAL"; break;
		  case EBUSY: err = "EBUSY"; break;
		  case EAGAIN: err = "EGAGAIN"; break;
		  case EDEADLK: err = "EDEADLK"; break;
		  case EPERM: err = "EPERM"; break;
		  default: err = "Unknown(" + ns_to_string(status) +")"; break;
	  }
	  throw ns_ex("ns_lock(") << name << ")::An error occured waiting on lock:" << err;
	}
#endif
}

#ifdef NS_THREAD_DEBUG
std::string ns_shorten_filename(const std::string & s){
	string r(ns_dir::extract_filename(s));
	string::size_type p(r.find("image_server"));
	if (p!=string::npos)
		r.replace(p,p+9,"is");
	return r;
}
#endif

void ns_lock::wait_to_acquire(const char * source_file, const unsigned int source_line){
//	if (currently_holding)
	//	throw ns_ex("ns_lock(") << name << ")::Attempting to acquire a lock that is already held! Acquired at (" << acquire_source_file << "::" << acquire_source_line << "), re-requested at(" << source_file << "::" << source_line << ")";
	#ifdef NS_THREAD_DEBUG	
		ns_high_precision_timer t;
		t.start();
		if (ns_output_lock_init && this != &output_lock){
			output_lock.wait_to_acquire(__FILE__,__LINE__);
			cerr << "[" << name << " " << ns_shorten_filename(source_file) << " " << source_line << "]"; 
			output_lock.release();
		}
	#endif

	wait_to_acquire(mutex_handle);

	#ifdef NS_THREAD_DEBUG
		if (ns_output_lock_init && this != &output_lock){
			ns_64_bit dur(t.stop());
			cerr << "{" << dur/1000 << "}\n";
		}
	#endif
	currently_holding = true;
	#if NS_DEBUG_LOCK
	acquire_source_file = source_file;
	acquire_source_line = source_line;
	#endif
}

bool ns_lock::try_to_acquire(const char * source_file, const unsigned int source_line){
//	if (currently_holding)
//		throw ns_ex("ns_lock(") << name << ")::Attempting to acquire a lock that is already held! Acquired at (" << acquire_source_file << "::" << acquire_source_line << "), re-requested at("  << source_file << "::" << source_line << ")";

	#ifdef NS_THREAD_DEBUG	
		ns_high_precision_timer t;
		t.start();
		if (ns_output_lock_init && this != &output_lock){
			output_lock.wait_to_acquire(__FILE__,__LINE__);
			cerr << "[" << name << " " << ns_shorten_filename(source_file) << " " << source_line << "]"; 
			output_lock.release();
		}
	#endif

#ifdef _WIN32 
	currently_holding = TryEnterCriticalSection(&mutex_handle) != 0; 
#else
	unsigned int ret = pthread_mutex_trylock(&mutex_handle);
	currently_holding = (ret == 0);
	if (ret!= 0 && ret != EBUSY){
		ns_ex ex("ns_lock(");
		ex << name << ")::An error occured while trying lock:";
		switch(ret){
			case EINVAL: ex << "EINVAL"; break;
			case EAGAIN:ex << "AGAIN"; break;

			case	EDEADLK: ex << "DEADLK"; break;
			case	EPERM:ex << "EPERM"; break;
			default: ex << ret;
		}
		throw ex;
	}
#endif
	
	#if NS_DEBUG_LOCK
	if (currently_holding){
		acquire_source_file = source_file;
		acquire_source_line = source_line;
	}
	#endif
	#ifdef NS_THREAD_DEBUG
		if (ns_output_lock_init && this != &output_lock){
			ns_64_bit dur(t.stop());
			cerr << "{" << dur/1000 << ";";
			if (currently_holding)
				cerr << "Taken}\n";
			else cerr << "Busy}\n";
		}
	#endif
	return currently_holding;
}

void ns_lock::release(ns_mutex_handle & mutex){

#ifdef _WIN32 
		 LeaveCriticalSection(&mutex);
#else
		if (pthread_mutex_unlock(&mutex))
			throw ns_ex("ns_lock(") << name << ")::An error occured while releasing lock";
#endif

}

void ns_lock::release(){	
	//if (currently_holding){
		release(mutex_handle);		 
		currently_holding = false;
//	}
}

ns_lock::~ns_lock(){
#ifdef _WIN32 
	 DeleteCriticalSection(&mutex_handle);
#else
	pthread_mutex_destroy(&mutex_handle);
#endif
}
ns_process_termination_manager::~ns_process_termination_manager(){}


bool ns_try_to_acquire_lock_for_scope::try_to_get(const char * file, const unsigned int line){
	if (!currently_held){
		 currently_held = lock->try_to_acquire(__FILE__,__LINE__);
	}
	return currently_held;
}
void ns_try_to_acquire_lock_for_scope::release(){
	if (currently_held){
		lock->release();
		currently_held = false;
	}
}
ns_try_to_acquire_lock_for_scope::~ns_try_to_acquire_lock_for_scope(){
	#ifdef NS_THREAD_DEBUG
		if (currently_held){
			output_lock.wait_to_acquire(__FILE__,__LINE__);
			std::cerr << "**Releasing Lock in destructor**\n";
			output_lock.release();
		}
	#endif
	release();
}


void ns_acquire_lock_for_scope::get(const char * file, const unsigned int line){
	if (!currently_held){
		lock->wait_to_acquire(file,line);
		currently_held=true;
	}
}
void ns_acquire_lock_for_scope::release(){
	if (currently_held){
		lock->release();
		currently_held = false;
	}
}
ns_acquire_lock_for_scope::~ns_acquire_lock_for_scope(){
	#ifdef NS_THREAD_DEBUG
		if (currently_held){
			output_lock.wait_to_acquire(__FILE__,__LINE__);
			std::cerr << "**Releasing Lock in destructor**\n";
			output_lock.release();
		}
	#endif
	release();
}
ns_acquire_lock_for_scope::ns_acquire_lock_for_scope(ns_lock & lock_,const char * file, const unsigned int line,const bool acquire_immediately):currently_held(false){
	lock = &lock_;
	if (acquire_immediately)
		get(__FILE__,__LINE__);
}
	


bool ns_process_priority::set_priority(const ns_process_priority::ns_priority requested_priority){

	#ifdef _WIN32 
		BOOL ret;
		if (requested_priority != ns_background && current_priority == ns_background) 
			SetPriorityClass(GetCurrentProcess(),PROCESS_MODE_BACKGROUND_END);
		 switch(requested_priority){
			 case ns_above_normal: ret=SetPriorityClass(GetCurrentProcess(),ABOVE_NORMAL_PRIORITY_CLASS); break;
			 case ns_below_normal: ret=SetPriorityClass(GetCurrentProcess(),BELOW_NORMAL_PRIORITY_CLASS); break;
			 case ns_high: ret=SetPriorityClass(GetCurrentProcess(),HIGH_PRIORITY_CLASS); break;
			 case ns_idle: ret=SetPriorityClass(GetCurrentProcess(),IDLE_PRIORITY_CLASS); break;
			 case ns_normal: ret=SetPriorityClass(GetCurrentProcess(),NORMAL_PRIORITY_CLASS); break;
			 case ns_background: ret=SetPriorityClass(GetCurrentProcess(),PROCESS_MODE_BACKGROUND_BEGIN); break;
			 case ns_realtime: ret=SetPriorityClass(GetCurrentProcess(),REALTIME_PRIORITY_CLASS); break;
			 default: throw ns_ex("ns_process_priority::set_priority()::Unknown priority requested");
		 }
		 return ret!=0;
	#else
		
		int priority;

		switch(requested_priority){
			 case ns_idle: priority=20; break;
			 case ns_background: priority=15; break;
			 case ns_below_normal: priority=10; break;
			 case ns_normal: priority=0; break;
			 case ns_above_normal: priority=-10; break;
			 case ns_high: priority=-15; break;
			 case ns_realtime: priority=-20; break;
			 default: throw ns_ex("ns_process_priority::set_priority()::Unknown priority requested");
		 }

		int which = PRIO_PROCESS;
		id_t pid;
		pid = getpid();
		int ret = setpriority(which, pid, priority);
		return ret==0;
	#endif
}
