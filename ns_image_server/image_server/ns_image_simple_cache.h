#ifndef NS_IMAGE_SIMPLE_CACHE
#define NS_IMAGE_SIMPLE_CACHE
#include "ns_image.h"
#include "ns_image_storage.h"
#include <map>

//#define NS_VERBOSE_IMAGE_CACHE

#ifdef  NS_VERBOSE_IMAGE_CACHE
#include <iostream>
#endif
class ns_image_storage_handler;



template<class ns_component>
struct ns_image_simple_cache_object{
	ns_image_simple_cache_object():size_in_memory(0),do_not_delete(false),to_erase(false){}
	unsigned long last_access; //the last time (in UNIX time seconds) that the image was requested from the cache
	bool do_not_delete; //if images are open for reading, they can't be swapped to the disk
							   //as the disk copy would not reflect any changes made to the active copy.
	unsigned long size_in_memory; //if size_in_memory != 0, the image is present in memory and is of the specified size.

	ns_image_whole<ns_component> image; //the image cached in memory.
	unsigned long image_uncompressed_size(){ return image_uncompressed_size(image.properties());  }
	static unsigned long image_uncompressed_size(ns_image_properties & p){ 
		return(p.width*
		p.height*
		p.components)/1024;
	}
	bool to_erase;
};
ns_image_storage_source_handle<ns_8_bit> ns_storage_request_from_storage(ns_image_storage_handler * image_storage, ns_image_server_image & im,ns_sql & sql);

///ns_image_cache implements a local cache for images loaded from the central file server.
///After an image is requested, ns_image_cache maintains a copy in memory for later use.
///Maximum memory usage can be specified (images in memory run ~750Mb and a few of these can
///exausts windows virtual memory default limits).  When the cache size is exceeded older images are deleted.
template<class ns_component>
class ns_image_simple_cache{
public:
	ns_image_simple_cache(ns_image_storage_handler * image_storage, const unsigned long & max_memory_usage_):
	  disk_cached_cleared(false),storage(image_storage),max_memory_usage(max_memory_usage_),current_memory_usage(0){}

	void set_memory_allocation_limit(const unsigned long & max){//in kilobytes
		max_memory_usage = max;
	}
	void clear_memory_cache(){
		ns_image_server_image im;
		im.id = 0;
		make_room_in_memory_for_new_image(im,max_memory_usage);
	}
	const ns_image_whole<ns_component> & get_for_read(ns_image_server_image & image, ns_sql & sql){
		return get_image(image,sql,true);
	}

	ns_image_whole<ns_component> & get_for_write(ns_image_server_image & image, ns_sql & sql){
		return get_image(image,sql,false);
	}

	void finished_writing(ns_image_server_image & image, ns_sql & sql){
		typename cache_t::iterator p = images.find(image.id);
		if (p == images.end() || p->second.do_not_cache_to_disk == false)
			throw ns_ex("ns_image_cache::Specified image has not been checked out for writing.");
		p->second.do_not_delete = false;
	}
	void clear_cache(){
		//clear memory cache and all records
		this->current_memory_usage = 0;
		images.clear();
	}

private:
	bool disk_cached_cleared;
	typedef std::map<ns_64_bit,ns_image_simple_cache_object<ns_component> > cache_t;
	cache_t images;
	ns_image_storage_handler * storage;
	unsigned long max_memory_usage;
	unsigned long current_memory_usage;

	ns_image_whole<ns_component> & get_image(ns_image_server_image & image, ns_sql & sql, const bool read_only){
		//check to see if we have this in the cache somewhere
		typename cache_t::iterator p = images.find(image.id);
		if (p != images.end()){
			if (!read_only)
				p->second.do_not_delete = true;
			return p->second.image;
		}
		
		
		//We couldn't find the image in the cache, load it from disk
		try{	

			ns_image_storage_source_handle<ns_component> im(ns_storage_request_from_storage(storage,image,sql));

			//clean out the cache to make room for the new image
			
			const unsigned long image_size(ns_image_simple_cache_object<ns_component>::image_uncompressed_size(im.input_stream().properties()));
			ns_image_server_image imc;
			imc.id = 0;
			make_room_in_memory_for_new_image(imc,image_size);

			//create a cache entry for the new image
			std::pair<typename cache_t::iterator,bool> r = images.insert(typename cache_t::value_type(image.id,ns_image_simple_cache_object<ns_component>()));
			p = r.first;
			try{
				im.input_stream().pump(p->second.image,1024);
			}
			catch(...){
				images.erase(p);
				throw;
			}
			#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Inserting new image with size " << image_size << " into the cache.  \nCache status is currently "<< current_memory_usage/1024 << "/" << max_memory_usage/1024 << "\n"; 
			#endif
			p->second.last_access = ns_current_time();
			p->second.size_in_memory = image_size;
			current_memory_usage+=image_size;
			if (!read_only)
				p->second.do_not_delete = true;
			return p->second.image;
			
		}
		catch(ns_ex & ex){
			//any error that happens here is likely due to cache problems, rather than any problem with the source file.  flag as such.
			ns_ex ex2(ex);
			ex2 << ns_cache;
			throw ex2;
		}
	}

	
	//invalidates all references to the cache
	void make_room_in_memory_for_new_image(const ns_image_server_image & im, const unsigned long new_image_memory_req){
		#ifdef NS_VERBOSE_IMAGE_CACHE
		if(current_memory_usage + new_image_memory_req > max_memory_usage){
			std::cerr << "Memory cache " << current_memory_usage/1024 << "/" << max_memory_usage/1024 << " cannot fit new entry (" << new_image_memory_req << ").  Deleting oldest entries.\n";
		}
		#endif
		while(current_memory_usage + new_image_memory_req > max_memory_usage){
			typename cache_t::iterator oldest_in_memory = images.begin();
			bool found(false);
			unsigned int num_locked = 0;
			for (typename cache_t::iterator p = images.begin(); p != images.end(); p++){
				if (p->first == im.id)  //don't delete the record of the image for which we're trying to make room.
					continue;
				if (p->second.do_not_delete){
					continue;
				}
				if(p->second.last_access <= oldest_in_memory->second.last_access){
					found = true;
					oldest_in_memory = p;
				}
			}
		
			if (!found){
				#ifdef NS_VERBOSE_IMAGE_CACHE
				std::cerr << "Accepting cache overflow.\n";
				#endif
				//memory is full.  We roll with it and allow the memory cache to get too large
				return;
			}
			#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Deleting image of size " << oldest_in_memory->second.size_in_memory/1024 << " dated " << ns_format_time_string(oldest_in_memory->second.last_access) << "\n";
			#endif
			current_memory_usage-=oldest_in_memory->second.size_in_memory;
			images.erase(oldest_in_memory);
		}
		return;
	}
};


#endif
