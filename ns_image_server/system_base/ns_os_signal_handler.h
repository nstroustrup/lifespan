#ifndef NS_OS_SIGNAL_HANDLER
#define NS_OS_SIGNAL_HANDLER

#ifndef WIN32
#include <sys/types.h>
#include <sys/stat.h>
#include <string.h>
#endif
#include "ns_ex.h"

#ifdef _WIN32 
	typedef int ns_os_signal_handler_return_type;
	typedef unsigned long ns_os_signal_handler_signal;
	//typedef PHANDLER_ROUTINE ns_os_signal_handler_function_pointer;
	#define ns_os_signal_handler_return_value 1

	typedef ns_os_signal_handler_return_type (*ns_os_signal_handler_function_pointer)(ns_os_signal_handler_signal);
	typedef enum{ns_interrupt,ns_number_of_os_signal_types} ns_os_signal_type;
#else

	#include <unistd.h>  
	#include <sys/types.h> 
	#include <signal.h>
	typedef void ns_os_signal_handler_return_type;
	typedef int ns_os_signal_handler_signal;
	#define ns_os_signal_handler_return_value /**/

	typedef ns_os_signal_handler_return_type (*ns_os_signal_handler_function_pointer)(ns_os_signal_handler_signal signal,siginfo_t *info, void * context);
	typedef enum{ns_interrupt,ns_segmentation_fault,
				 ns_bus_error,ns_illegal_instruction,
				 ns_floating_point_error,ns_number_of_os_signal_types} ns_os_signal_type;
	
#endif

class ns_os_signal_handler{
public:
#ifndef WIN32
	static int get_os_signal(const ns_os_signal_type & t){
		switch(t){
			case ns_interrupt: return SIGINT;
			case ns_segmentation_fault: return SIGSEGV;
			case ns_bus_error: return SIGBUS;
			case ns_illegal_instruction: return SIGKILL;
			case ns_floating_point_error: return SIGFPE;
		}
		throw ns_ex("Unkonwn signal type:") << (int)t;
	}
#endif
	void set_signal_handler(ns_os_signal_type signal_type ,ns_os_signal_handler_function_pointer p){
		#ifdef _WIN32 
			if (signal_type != ns_interrupt)
				throw ns_ex("Unknown signal:") << (int)signal_type;
			//SetProcessShutdownParameters(0x100,0);
			if (SetConsoleCtrlHandler((PHANDLER_ROUTINE)p,TRUE)==0)
					throw ns_ex("Could not register exit handler.");
		#else
			memset( &actions[(int)signal_type], 0, sizeof( struct sigaction ));
			actions[(int)signal_type].sa_sigaction = p; 
			actions[(int)signal_type].sa_flags = SA_RESTART | SA_SIGINFO; 
			sigaction(get_os_signal(signal_type),&actions[(int)signal_type],0);
		#endif
	}
private:
	#ifndef WIN32
		struct sigaction actions[ns_number_of_os_signal_types];
	#endif
};

#endif
