#ifndef NS_SIMPLE_CACHE
#define NS_SIMPLE_CACHE

#include "ns_ex.h"
#include "ns_thread.h"
#include <map>

//#define NS_VERBOSE_IMAGE_CACHE

#ifdef  NS_VERBOSE_IMAGE_CACHE
#include <iostream>
#endif


template<class data_t, bool locked>
class ns_simple_cache_internal_object;

template<class data_t, bool locked>
class ns_simple_cache;

//an example base class for the data stored in the cache
template<class id_t, class external_source_t>
class ns_simple_cache_data {
public:
	virtual ns_64_bit size_in_memory_in_kbytes()const = 0;

	virtual void load_from_external_source(const id_t & id, external_source_t & external_source) = 0;

	virtual ns_64_bit to_id(const id_t & id) const = 0;
	virtual ns_64_bit id() const =0 ;

	virtual void clean_up(typename external_source_t & external_source) = 0;

	typedef id_t id_type;
	typedef external_source_t external_source_type;
};

template<class data_t, bool locked>
class ns_simple_cache_data_handle {
public:
	ns_simple_cache_data_handle():obj(0), cache(0), data(0) {}
	data_t & operator()() { return *data; }
	const data_t &operator()() const { return *data; }
	~ns_simple_cache_data_handle();
	void release() { check_in(); }

	//friend data_t;

//	template<class data_t, bool locked> friend class ns_simple_cache;
//private:
	data_t * data; //the image cached in memory.
	void check_out(bool write, ns_simple_cache_internal_object<data_t,locked> * o, ns_simple_cache<data_t, locked> *c);
	void check_in();
	ns_simple_cache_internal_object< data_t, locked> * obj;
	ns_simple_cache< data_t, locked> * cache;
	bool write;

};

template<class data_t, bool locked>
class ns_simple_cache_internal_object {
public:
	ns_simple_cache_internal_object() :number_waiting_or_write(0), 
		object_write_lock("owl"), size_in_memory(0), do_not_delete(false), 
		to_erase(false), to_be_deleted(false), number_checked_out_by_any_threads(0) {}

	typedef data_t data_type;

	unsigned long last_access; //the last time (in UNIX time seconds) that the image was requested from the cache

	data_t data;

	ns_lock object_write_lock;
	//protected by the write lock but not the main lock
	int number_waiting_for_write_lock;

	int number_checked_out_by_any_threads;
	bool to_be_deleted;

	//friend data_t;
	//friend class ns_simple_cache<data_t,locked>;
};

///ns_image_cache implements a local cache for images loaded from the central file server.
///After an image is requested, ns_image_cache maintains a copy in memory for later use.
///Maximum memory usage can be specified (images in memory run ~750Mb and a few of these can
///exausts windows virtual memory default limits).  When the cache size is exceeded older images are deleted.
template<class data_t, bool locked>
class ns_simple_cache {

//	friend data_t;

public:

	ns_simple_cache(const unsigned long & max_memory_usage_) :
		disk_cached_cleared(false),
		max_memory_usage_in_kb(max_memory_usage_),
		current_memory_usage_in_kb(0),
		lock("clock") {}

	typedef ns_simple_cache_data_handle<const typename data_t, locked> const_handle_t;
	typedef ns_simple_cache_data_handle<typename data_t, locked> handle_t;

	typedef typename data_t::external_source_type external_source_type;

	void set_memory_allocation_limit_in_kb(const unsigned long & max) {//in kilobytes
		max_memory_usage = max;
	}
	void get_for_read(const typename data_t::id_type & id, const_handle_t & cache_object, typename data_t::external_source_type & external_source) {
		get_image(id, &cache_object, external_source, true);
	}

	void get_for_write(const typename data_t::id_type & id, handle_t & cache_object, typename data_t::external_source_type & external_source) {
		get_image(id, &cache_object, external_source, false);
	}

	void clear_cache(typename data_t::external_source_type * external_source) {
		//clear memory cache and all records

		if (locked)lock.wait_to_acquire(__FILE__, __LINE__);

		//get all write locks!
		//is it really that simple? 
		for (cache_t::iterator p = data_cache.begin(); p != data_cache.end(); p++) {
			p->second.object_write_lock.wait_to_acquire(__FILE__, __LINE__);
			if (external_source != 0)
				p->second.data.clean_up(*external_source);
			p->second.object_write_lock.release();
		}
		this->current_memory_usage_in_kb = 0;
		data_cache.clear();
		if (locked)lock.release();
	}

	//issue:this will lock all threads
	void remove_old_images(unsigned long age_in_seconds, typename data_t::external_source_type & external_source) {
		//clear memory cache and all records

		if (locked)lock.wait_to_acquire(__FILE__, __LINE__);
		const unsigned long current_time(ns_current_time());
		//get all write locks!
		//is it really that simple? 
		for (cache_t::iterator p = data_cache.begin(); p != data_cache.end();) {
			p->second.object_write_lock.wait_to_acquire(__FILE__, __LINE__);
			if (p->second.last_access + age_in_seconds < current_time) {
				current_memory_usage_in_kb -= p->second.data.size_in_memory_in_kbytes();
				p->second.data.clean_up(external_source);
				p->second.object_write_lock.release();
				p = data_cache.erase(p);
			}
			else {
				p->second.object_write_lock.release();
				p++;
			}
		}
		if (locked)lock.release();
	}

	typedef std::map<ns_64_bit, ns_simple_cache_internal_object<data_t, locked> > cache_t;


//private:
	bool disk_cached_cleared;
	cache_t data_cache;
	unsigned long max_memory_usage_in_kb;
	unsigned long current_memory_usage_in_kb;

	typedef typename cache_t::value_type map_pair_t;
	typedef typename ns_simple_cache_internal_object<data_t, locked> internal_object_t;

	//data_type_t will be either data_type or const data_type depending on whether 
	template<class data_type_t>
	void get_image(const typename data_t::id_type & id,
		ns_simple_cache_data_handle< data_type_t, locked> * image,
		typename data_t::external_source_type & external_source,
		bool read_only) {
		bool initial_read_request(read_only);
		//check to see if we have this in the cache somewhere
		bool currently_have_write_lock(false);

		//used to convert ids.
		const data_type_t dt;

		while (true) {
			//grab the lock needed to access anything
			if (locked)lock.wait_to_acquire(__FILE__, __LINE__);
			try {
				//look for the image in the cache.
				typename cache_t::iterator p = data_cache.find(dt.to_id(id));
				if (p != data_cache.end()) {
					//some cleanup needs to be done.
					if (p->second.to_be_deleted)
						read_only = false;

					if (!read_only) {
						//if we want write access, we need to wait until anyone currently reading is finished
						//and then block out anyone who wants to read.

						//if we have (or don't need) the write lock, we're done!  check out the object and return it.
						if (!locked || currently_have_write_lock) {
							p->second.number_waiting_for_write_lock++;  //mark that the lock is held
							if (locked) {
								//now we need to wait for any threads that are still reading.
								//we minimize the # of locks by polling just a here.
								while (p->second.number_checked_out_by_any_threads > 0) {
									ns_thread::sleep_microseconds(100);
								}
							}
							//ok we've been asked to do some housekeeping by deleting this record and trying again.
							if (p->second.to_be_deleted) {
								p->second.object_write_lock.release();
								data_cache.erase(p);
								lock.release();
								read_only = initial_read_request;
								continue; //do another round
							}
							//we have exclusive write access to a record no-one is reading!  We're done. 
							data_t * a(0);
							typename ns_simple_cache_internal_object<data_t,true>::data_type * b(0);
							a = b;
							ns_simple_cache_internal_object<data_t, locked> tt;

							image->check_out(!read_only,&tt,this);
							if (locked)lock.release();
							return;
						}
						// we need to get the write lock.  let's wait for grab it but then we'll need to 
						// redo the search in the map structure just in case the previous write deleted it been deleted
						if (locked) {
							p->second.object_write_lock.wait_to_acquire(__FILE__, __LINE__);  //this wont be given up until the object ns_simple_cache object is destructed!
							currently_have_write_lock = true;
							lock.release(); //keep the write lock (to catch any readers) but release the read lock (so that the readers can progress and get stuck on the wait lock)
							continue;
						}
					}
					//ok, we are looking only for read access, so we just need to confirm
					//that nobody is currently writing.  this is much the same as when we wanted write access before
					else //if we haven't picked up the write lock in the last iteration of the loop, we need to confirm
						 //that no other threads are writing to this objects, and wait for the write lock if they are.
						if (locked) {
							//we need to wait until no threads are writing to this object
							if (p->second.number_waiting_for_write_lock > 0) {
								if (!currently_have_write_lock)  //we need to block all others that want to write.
									p->second.object_write_lock.wait_to_acquire(__FILE__, __LINE__);
								currently_have_write_lock = true;
								lock.release();
								continue; //start over from the beginning of the request, as anything could have changed during the previous write
							}

							//we have the write lock, so nobody else does!  check it out!
							image->check_out(true,&p->second, this);
							if (currently_have_write_lock) p->second.object_write_lock.release();
							if (locked)lock.release();
							break;
						}
				}
			}
			catch (...) {
				if (locked) lock.release();
			}

		}

		//We couldn't find the image in the cache!  We need to load it from disk and add it to the cache
		try {
			typename cache_t::iterator p;
			//create a new cache entry for the new image, and reserve it for writing
			//{
				internal_object_t foo;
				const ns_64_bit id_num = dt.to_id(id);
				//xxx unneccisary double lookup--easy to fix when bugs elsewhere are identified
				data_cache[id_num] = foo;
				p = data_cache.find(id_num);
				////std::pair<const ns_64_bit, internal_object_t > obj( id_num, foo);
				//auto obj = make_pair(id_num, foo);
				//p = data_cache.insert(data_cache.begin(), obj)->first;
			//}
			p->second.object_write_lock.wait_to_acquire(__FILE__, __LINE__);
			p->second.number_waiting_for_write_lock++;
			image->check_out(true,&p->second, this);  //check it out for writing.
			lock.release();//release the lock so everything else can read and write other objects in the background while we load
							//this one in.


			try {
				p->second.data.load_from_external_source(id, external_source);
			}
			catch (...) {
				p->second.to_be_deleted = true;  //we can't delete this now, because we've already given up the main lock.  but we can flag it for deletion next time it's encountered
				throw;
			}
#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Inserting new image with size " << image_size << " into the cache.  \nCache status is currently " << current_memory_usage / 1024 << "/" << max_memory_usage / 1024 << "\n";
#endif
			//ok! We've loaded the data into the new position in the map.  populate it with interesting info.
			p->second.last_access = ns_current_time();
			current_memory_usage_in_kb += p->second.data.size_in_memory_in_kbytes();

			if (!read_only)
				return;  //we already have write access and everything loaded into cached_object, give it to the user.

							//lets give up write access manually here, and pass the remaining object, no read only, on to the user
			p->second.number_waiting_for_write_lock--;
			image->write = false;
			return;

		}
		catch (ns_ex & ex) {
			//any error that happens here is likely due to cache problems, rather than any problem with the source file.  flag as such.
			ns_ex ex2(ex);
			ex2 << ns_cache;
			throw ex2;
		}
	}


	void make_room_in_memory(const unsigned long new_image_memory_req_in_kb, typename data_t::external_source_type & external_source) {
#ifdef NS_VERBOSE_IMAGE_CACHE
		if (current_memory_usage + new_image_memory_req > max_memory_usage) {
			std::cerr << "Memory cache " << current_memory_usage / 1024 << "/" << max_memory_usage / 1024 << " cannot fit new entry (" << new_image_memory_req << ").  Deleting oldest entries.\n";
		}
#endif
		ns_64_bit target_id(-1);
		while (current_memory_usage_in_kb + new_image_memory_req_in_kb > max_memory_usage_in_kb) {
			if (locked)lock.wait_to_acquire(__FILE__, __LINE__);
			typename cache_t::iterator oldest_in_memory = data_cache.begin();
			bool found(false);

			for (typename cache_t::iterator p = data_cache.begin(); p != data_cache.end(); p++) {
				if (p->first == im.id)  //don't delete the record of the image for which we're trying to make room.
					continue;
				if (p == target_id) {
					oldest_in_memory = p;
					found = true;
					break;
				}
				if (p->second.last_access <= oldest_in_memory->second.last_access) {
					found = true;
					oldest_in_memory = p;
				}
			}

			if (!found) {
#ifdef NS_VERBOSE_IMAGE_CACHE
				std::cerr << "Accepting cache overflow.\n";
#endif
				//memory is full.  We roll with it and allow the memory cache to get too large
				if (locked) lock.release();
				return;
			}
			//we have a write lock on a specific object, but it's been modified somehow and can't be found.  
			if (locked && target_id != -1 && oldest_in_memory->second.id != target_id) {
				lock.release();
				throw ns_ex("The cache seems to have been corrupted!")
			}

			//we have the object but we need to grab its wait lock
			if (locked && target_id == -1) {
				if (locked) {
					//if someone else is writing to it, there's a chance it could have been deleted, so we need to wait and then go back
					//and load it again
					if (oldest_in_memory->second.number_waiting_to_write > 0) {
						target_id = oldest_in_memory->second.id;
						oldest_in_memory->second.number_waiting_to_write++;
						lock.release();
						oldest_in_memory->second.object_write_lock.wait_to_acquire(__FILE__, __LINE__);
						continue;
					}
				}
			}
			//ok we've finally found the oldest in memory and we hold its write lock.

#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Deleting image of size " << oldest_in_memory->second.size_in_memory / 1024 << " dated " << ns_format_time_string(oldest_in_memory->second.last_access) << "\n";
#endif
			current_memory_usage -= oldest_in_memory->second.data.size_in_memory_in_kb();
			oldest_in_memory().clean_up(external_source);
			data_cache.erase(oldest_in_memory);
		}
		lock.release();
		return;
	}
//private:
	ns_lock lock;
};

template<class data_t, bool locked>
inline void ns_simple_cache_data_handle<data_t, locked>::check_out(bool write, ns_simple_cache_internal_object<data_t, locked> * o, ns_simple_cache<data_t, locked> *c) {
	//not locked as the cache object will be locked while checking this object out
	obj = o;
	cache = c;
	image = &obj->image;
	obj->number_checked_out_by_any_threads++;
	obj->last_access = ns_current_time();
	if (write)
		obj->being_written_to = write = true;
	else write = false;
}

template<class data_t, bool locked>
inline void ns_simple_cache_data_handle<data_t,locked>::check_in() {
	//we release this first, so that anyone holding the main lock
	//and waiting for the write lock will continue;
	if (locked && write)
		obj->number_waiting_for_write_lock--;
	if (locked && write)
		obj->object_write_lock.release();

	if (locked) cache->lock.wait_to_acquire(__FILE__, __LINE__);
	obj->number_checked_out_by_any_threads--;
	if (locked) cache->lock.release();
	obj = 0;
	cache = 0;
	image = 0;
}


template<class data_t, bool locked>
inline ns_simple_cache_data_handle<data_t, locked>::~ns_simple_cache_data_handle() {
	if (data == 0 || obj == 0 || cache == 0)
		return;
	check_in();
}


void ns_test_simple_cache(const char * debug_output);


#endif