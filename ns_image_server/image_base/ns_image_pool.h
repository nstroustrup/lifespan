#ifndef NS_IMAGE_POOL
#define NS_IMAGE_POOL
#include "ns_image.h"
#include <stack>

class ns_default_resizer{
public:
	ns_image_properties parse_initialization(const ns_image_properties & t){ return t;}
	template<class T>
	void resize_after_initialization(const ns_image_properties & t){;}
};

class ns_overallocation_resizer{
public:
	ns_overallocation_resizer(const ns_image_properties & overallocation_size_):overallocation_size(overallocation_size_){}
	ns_overallocation_resizer	():overallocation_size(0,0,0,0){}
	void set_size(const ns_image_properties & overallocation_size_){overallocation_size = overallocation_size_;}
	ns_image_properties parse_initialization(const ns_image_properties & t){ return overallocation_size;}
	template<class T>
	void resize_after_initialization(const ns_image_properties & t, T & v){v.resize(t);}
private:
	ns_image_properties overallocation_size;
}; 

class ns_wasteful_overallocation_resizer {
public:
	ns_wasteful_overallocation_resizer(const ns_image_properties & overallocation_size_) :overallocation_size(overallocation_size_) {}
	ns_wasteful_overallocation_resizer() :overallocation_size(0, 0, 0, 0) {}
	void set_size(const ns_image_properties & overallocation_size_) { overallocation_size = overallocation_size_; }
	ns_image_properties parse_initialization(const ns_image_properties & t) { return overallocation_size; }
	template<class T>
	void resize_after_initialization(const ns_image_properties & t, T & v) { 
		v.use_more_memory_to_avoid_reallocations(true);
		v.resize(t); 
	}
private:
	ns_image_properties overallocation_size;
};

//#define NS_DEBUG_POOL
template<class T, class dT = ns_default_resizer, bool is_locking=true>
class ns_image_pool{
private:
	//dissalow copy constructor
	ns_image_pool(const ns_image_pool & p);
public:
	ns_image_pool():number_checked_out(0),pre_allocated(0),min_stack_size_in_history(0),access_lock("al"){
		clear_history();
	}
	ns_image_pool(const unsigned long i):number_checked_out(0),pre_allocated(0),min_stack_size_in_history(0), access_lock("al") {
		clear_history();pre_allocate(i);
	}
	~ns_image_pool(){
		clear();
	}
	void pre_allocate(const unsigned long s){
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		for (unsigned int i = 0; i < s; i++)
			pool.push(new T);

		if (is_locking) access_lock.release();
	}
	template<class M>
	void pre_allocate(const unsigned long s, const M & m){
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		pre_allocated+=s;
		for (unsigned int i = 0; i < s; i++){
			pool.push(new T(resizer.parse_initialization(m)));
			resizer.resize_after_initialization(m,pool.top());
		}
		if (is_locking) access_lock.release();
	}
	T * get(){
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		number_checked_out++;
		if (pool.empty()){	
			T * a(new T);
			check_out(a);
			if (is_locking)access_lock.release();
			return a;
		}
		T * a(pool.top());
		pool.pop();
		if (is_locking)access_lock.release();
		return a;
	}
	unsigned long number_of_items_checked_out() const { return number_checked_out; }
	void clear(){
		//empty pool handles its own locking
		empty_pool();
		#ifdef NS_DEBUG_POOL
			if (checked_out.size() > 0){
				std::cerr << "ns_image_pool::~ns_image_pool()::" << checked_out.size() << " objects remain checked out.  This is likely a memory leak.\n";
			}
		#endif
	}
	template<class M>
	T * get(const M & m){
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		number_checked_out++;
		#ifdef NS_DEBUG_POOL
		std::cerr << "Number Checked out: " << number_checked_out << "\n";
		#endif
		if (pool.empty()){
			try{
				T * a(new T(resizer.parse_initialization(m)));
				resizer.resize_after_initialization(m,*a);
				check_out(a);
				if (is_locking) access_lock.release();
				return a;
			}
			catch(std::bad_alloc){
				if (is_locking) access_lock.release();
				//empty pool does its own locking
				empty_pool();
				throw ns_ex("ns_pool_allocator(): Ran out of memory with ") << number_checked_out << " checked out objects\n";
			}
		}
		T * a(pool.top());
		pool.pop();
		resizer.resize_after_initialization(m,*a);
		check_out(a);
		if (is_locking) access_lock.release();
		return a;
	}
	void release(T *p){
		if (p==0)
			throw ns_ex("Checking in null pointer!");
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		number_checked_out--;
		//delete p;
		check_in(p);
		pool.push(p);
		if (is_locking) access_lock.release();
	}
	void mark_stack_size_waypoint_and_trim(){
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		update_stack_size_history();
	//	if (min_stack_size_in_history != 0){
	//		std::cout << "ns_image_pool()::Freeing " << min_stack_size_in_history << " out of " << pool.size() << " pooled objects, with " << number_checked_out << " checked out\n";
	//	}
		trim_stack();
		if (is_locking) access_lock.release();
	}
	void empty_pool(){
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		while(!pool.empty()){	
			T * t(pool.top());
			delete t;
			pool.pop();
		}
		if (is_locking) access_lock.release();
	}

	void set_resizer(const dT & resizer_){
		if (is_locking) access_lock.wait_to_acquire(__FILE__, __LINE__);
		resizer = resizer_;
		if (is_locking) access_lock.release();
	}
	//const unsigned long & number_checked_out(){return checked_out;}
private:
	ns_lock access_lock;
	enum{ns_stack_history_size=10};
	unsigned long number_checked_out;
	unsigned long pre_allocated;
	std::stack<T *> pool;
	#ifdef NS_DEBUG_POOL
	std::set<T *> checked_out;
	void check_out(T * t){
		checked_out.insert(checked_out.end(),t);
	}
	void check_in(T * t){
		typename std::set<T *>::iterator p = checked_out.find(t);
		if (p == checked_out.end())
			throw ns_ex("Checking in an invalid pointer!");
		else checked_out.erase(p);
	}
	#else
	void check_out(T * t){}
	void check_in(T * t){}
	#endif
	std::size_t stack_size_history[ns_stack_history_size];
	std::size_t min_stack_size_in_history;
	inline void trim_stack(){
		for (int i = 0; i < min_stack_size_in_history && !pool.empty(); i++){
			T * t(pool.top());
			delete t;
			pool.pop();
		}
	}
	void clear_history(){
		for (unsigned int i = 0; i < ns_stack_history_size; i++)
			stack_size_history[i] = 0;
	}
	inline void update_stack_size_history(){
		min_stack_size_in_history = pool.size();
		for (int i = ns_stack_history_size-2; i >= 0; i--){
			stack_size_history[i+1] = stack_size_history[i];
			if (stack_size_history[i] < min_stack_size_in_history)
				min_stack_size_in_history = stack_size_history[i];
		}
		stack_size_history[0] = pool.size();
	}
	dT resizer;
};
#endif