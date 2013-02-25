#ifndef NS_SINGLE_THREAD_COORDINATOR
#define NS_SINGLE_THREAD_COORDINATOR
#include "ns_thread.h"

class ns_single_thread_coordinator{
public:
	ns_single_thread_coordinator():thread_is_running(false),thread_needs_cleanup(false){}
	~ns_single_thread_coordinator(){
		if (thread_needs_cleanup)
			cleanup();
	}

	void run(ns_thread_function_pointer fptr, void * args){
		if (thread_needs_cleanup)
			cleanup();
		thread.run(fptr,args);
		thread_is_running = true;
		thread_needs_cleanup=true;
	}
	//to be called by asynchronous thread when it is done.
	void report_as_finished(){thread_is_running=false;}

	//for other threads to check if the asynchronous thread is still running
	bool is_running() const{return thread_is_running;}

	void block_on_finish(){
		if (thread_needs_cleanup){
			thread.block_on_finish();
			thread_needs_cleanup=false;
		}
	}
private:
	inline void cleanup(){
		block_on_finish();
	}
	ns_thread thread;
	bool thread_is_running;
	bool thread_needs_cleanup;
};


#endif

