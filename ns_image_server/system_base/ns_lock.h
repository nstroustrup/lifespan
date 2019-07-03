#ifndef NS_LOCK_H
#define NS_LOCK_H

#include "ns_ex.h"
#include <string>
#include <map>
#include <vector>

#ifdef _WIN32 
	#include <winsock2.h>
	#include <windows.h>
	typedef CRITICAL_SECTION ns_mutex_handle;
#else
	typedef pthread_mutex_t ns_mutex_handle;
#endif

#undef NS_DEBUG_LOCK

class ns_lock{
public:
 ns_lock():mute_debug_output(false),currently_holding(false){}
 ns_lock(const std::string & lock_name_):mute_debug_output(false), currently_holding(false){init(lock_name_);}
	void init(const std::string & lock_name);
	~ns_lock();

	void wait_to_acquire(const char * source_file, const unsigned int source_line);
	bool try_to_acquire(const char * source_file, const unsigned int source_line);
	ns_lock(const ns_lock & lock) {
	  //std::cerr << "Copying lock!\n";
		init(lock.name + "(1)");
	}
	void release();
	bool mute_debug_output;
private:

	bool currently_holding;
	std::string name;
	//the handle of the current mutex;
	ns_mutex_handle mutex_handle;

	void wait_to_acquire(ns_mutex_handle & mutex);
	void init(ns_mutex_handle & mutex);
	void release(ns_mutex_handle & mutex);

	//a global list of all locks and the mutex to edit them.
	static std::map<std::string,ns_mutex_handle*> * mutex_list;
	static ns_mutex_handle mutex_list_lock;

	#ifdef NS_DEBUG_LOCK
		std::string acquire_source_file;
		unsigned int acquire_source_line;
	#endif
};

class ns_acquire_lock_for_scope{
public:
	ns_acquire_lock_for_scope(ns_lock & lock_,const char * file, const unsigned int line,const bool acquire_immediately=true);
	void get(const char * file, const unsigned int line);
	void release();
	~ns_acquire_lock_for_scope();
private:
	ns_lock * lock;
	bool  currently_held;
};

class ns_try_to_acquire_lock_for_scope{
public:
	ns_try_to_acquire_lock_for_scope(ns_lock & lock_):currently_held(false),lock(&lock_){}
	bool try_to_get(const char * file, const unsigned int line);
	void release();
	~ns_try_to_acquire_lock_for_scope();
private:
	ns_lock * lock;
	bool  currently_held;

};
#endif
