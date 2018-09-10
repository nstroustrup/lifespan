#ifndef NS_SINGLE_THREAD_COORDINATOR
#define NS_SINGLE_THREAD_COORDINATOR
#include "ns_thread.h"

class ns_single_thread_coordinator{
public:
	ns_single_thread_coordinator():thread_status_lock("tsl"),thread_is_running(false),thread_needs_cleanup(false){}
	~ns_single_thread_coordinator(){
		ns_acquire_lock_for_scope lock(thread_status_lock,__FILE__,__LINE__);
		if (thread_needs_cleanup)
			cleanup(false);
		lock.release();
	}

	void run(ns_thread_function_pointer fptr, void * args){
		ns_acquire_lock_for_scope lock(thread_status_lock,__FILE__,__LINE__);
		if (thread_needs_cleanup){
			cleanup(false);
		}
		thread.run(fptr,args);
		thread_is_running = true;
		thread_needs_cleanup=true;
		lock.release();
	}
	//to be called by asynchronous thread when it is done.
	void report_as_finished(){
		thread_status_lock.wait_to_acquire(__FILE__,__LINE__);
		thread_is_running=false;
		thread_status_lock.release();
	}

	//for other threads to check if the asynchronous thread is still running
	bool is_running() const{
		
		thread_status_lock.wait_to_acquire(__FILE__,__LINE__);
		bool cur_state = thread_is_running;
		thread_status_lock.release();
		return cur_state;
	}

	void block_on_finish(bool lock=true){
		if (lock) thread_status_lock.wait_to_acquire(__FILE__,__LINE__);
		if (thread_needs_cleanup){
			thread.block_on_finish();
			thread_needs_cleanup = false;
			thread_is_running = false;
			if (lock) thread_status_lock.release();
		}else if (lock) thread_status_lock.release();
	}
private:
	inline void cleanup(bool lock=true){
		block_on_finish(lock);
	}
	ns_thread thread;
	mutable ns_lock thread_status_lock;
	bool thread_is_running;
	bool thread_needs_cleanup;
};


#endif

