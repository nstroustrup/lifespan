#ifndef NS_MANAGED_POINTER
#define NS_MANAGED_POINTER
/*
#include <iostream>
#include <map>
#include "ns_lock.h"
template<class pointer_type>
class ns_managed_pointer{

public:
	void take(pointer_type * pointer){
		if (pointer == 0)
			return;
		ns_acquire_lock_for_scope lock(managed_pointer_list_lock,__FILE__,__LINE__);
		typename std::map<pointer_type *,unsigned long>::iterator p = ownership_counts.find(pointer);

		if (p == ownership_counts.end())
			ownership_counts[pointer] = 1;

		else p->second++;
		lock.release();
	}
	void release(pointer_type ** pointer){
		if (*pointer == 0)
			return;
		ns_acquire_lock_for_scope lock(managed_pointer_list_lock,__FILE__,__LINE__);
		typename std::map<pointer_type *, unsigned long>::iterator p = ownership_counts.find(*pointer);
		if (p== ownership_counts.end()){
			//throw ns_ex("ns_managed_pointer::Releasing unheld managed pointer.");
			std::cerr << "ns_managed_pointer::release()::Releasing unheld managed pointer!\n";
			lock.release();
			return;
		}

		if (p->second == 1){
			delete *pointer;
			*pointer = 0;
			ownership_counts.erase(p);
		}
		else p->second--;
		lock.release();
	}
	static std::map<pointer_type *, unsigned long> ownership_counts;
	static ns_lock managed_pointer_list_lock;
};

template<class pointer_type>
std::map<pointer_type *, unsigned long> ns_managed_pointer<pointer_type>::ownership_counts;
template<class pointer_type>
ns_lock ns_managed_pointer<pointer_type>::managed_pointer_list_lock("ns_managed_pointer_lock");

template<class handle_type,class close_function_object>
class ns_managed_handle{

public:
	void take(const handle_type & handle){
		ownership_count_lock.wait_to_acquire(__FILE__,__LINE__);
		typename std::map<handle_type,unsigned long>::iterator p = ownership_counts.find(handle);

		if (p == ownership_counts.end())
			ownership_counts[handle] = 1;

		else p->second++;
		ownership_count_lock.release();
	}
	void release(const handle_type handle){
		ownership_count_lock.wait_to_acquire(__FILE__,__LINE__);
		typename std::map<handle_type, unsigned long>::iterator p = ownership_counts.find(handle);
		if (p== ownership_counts.end()){
			ownership_count_lock.release();
			//throw ns_ex("ns_managed_pointer::Releasing unheld managed pointer.");
			std::cerr << "ns_managed_pointer::release()::Releasing unheld managed handle!\n";
			return;
		}

		if (p->second == 1){
			ownership_counts.erase(p);
			ownership_count_lock.release();
			close_function_object obj;
			obj(handle);
		}
		else{
			p->second--;
			ownership_count_lock.release();
		}
	}
	static std::map<handle_type, unsigned long> ownership_counts;
	
	static ns_lock ownership_count_lock;
};

template<class handle_type,class close_function_object>
std::map<handle_type, unsigned long> ns_managed_handle<handle_type,close_function_object>::ownership_counts;

template<class handle_type,class close_function_object>
ns_lock ns_managed_handle<handle_type,close_function_object>::ownership_count_lock("ns_mh::o");
*/
#endif
