#ifndef NS_IMAGE_CACHE
#define NS_IMAGE_CACHE
#include "ns_image.h"
#include "ns_image_storage.h"
#include <map>

#define NS_VERBOSE_IMAGE_CACHE

#ifdef  NS_VERBOSE_IMAGE_CACHE
#include <iostream>
#endif
class ns_image_storage_handler;



template<class ns_component>
struct ns_image_cache_object{
	ns_image_cache_object():size_in_memory(0),size_on_disk(0),do_not_cache_to_disk(false),to_erase(false){}
	unsigned long last_access; //the last time (in UNIX time seconds) that the image was requested from the cache
	std::string filename_on_disk;   //If specified, the image has been swapped to disk under the specified filename
	bool do_not_cache_to_disk; //if images are open for reading, they can't be swapped to the disk
							   //as the disk copy would not reflect any changes made to the active copy.
	unsigned long size_in_memory; //if size_in_memory != 0, the image is present in memory and is of the specified size.
	unsigned long size_on_disk;

	ns_image_whole<ns_component> image; //the image cached in memory.
	unsigned long image_uncompressed_size(){ return image_uncompressed_size(image.properties());  }
	static unsigned long image_uncompressed_size(ns_image_properties & p){ 
		return(p.width*
		p.height*
		p.components)/1024;
	}
	bool to_erase;
};


bool ns_storage_delete_from_local_cache(ns_image_storage_handler * image_storage,const std::string & filename);
bool ns_storage_delete_from_local_cache(ns_image_storage_handler * image_storage,const std::string & filename);
unsigned long ns_storage_request_local_cache_file_size(ns_image_storage_handler * image_storage,const std::string & filename);

void ns_full_drive_cache_panic(const ns_ex & ex);

ns_image_storage_source_handle<ns_8_bit>  ns_storage_request_from_local_cache(ns_image_storage_handler * image_storage, const std::string & filename);
//ns_image_storage_source_handle<ns_16_bit>  ns_storage_request_from_local_cache(ns_image_storage_handler* image_storage, const std::string & filename);

ns_image_storage_source_handle<ns_8_bit> ns_storage_request_from_storage(ns_image_storage_handler * image_storage, ns_image_server_image & im,ns_sql & sql);
//ns_image_storage_source_handle<ns_16_bit> ns_storage_request_from_storage(ns_image_storage_handler * image_storage, ns_image_server_image & im,ns_sql & sql);

ns_image_storage_reciever_handle<ns_8_bit> ns_storage_request_local_cache_storage(ns_image_storage_handler * image_storage, const std::string & filename, const unsigned long max_line_length, const bool report_to_db = true);
//ns_image_storage_reciever_handle<ns_16_bit> ns_storage_request_local_cache_storage(ns_image_storage_handler * image_storage, const std::string & filename, const unsigned long max_line_length, const bool report_to_db = true);

///ns_image_cache implements a local cache for images loaded from the central file server.
///After an image is requested, ns_image_cache maintains a copy in memory for later use.
///Maximum memory usage can be specified (images in memory run ~750Mb and a few of these can
///exausts windows virtual memory default limits).  When the maximum memory usage is reached
///images are swapped in and out out to the disk cache.
///Disk swapping is done according to temporal priority--the most rececently accessed images are
///least likely to be swapped out of memory.
template<class ns_component>
class ns_image_cache{
public:
	ns_image_cache(ns_image_storage_handler * image_storage, const unsigned long & max_memory_usage_, const unsigned long & max_disk_usage_):
	  disk_cached_cleared(false),storage(image_storage),max_memory_usage(max_memory_usage_),current_memory_usage(0),max_disk_usage(max_disk_usage_),current_disk_usage(0){}


	void set_memory_allocation_limit(const unsigned long & max){//in kilobytes
		max_memory_usage = max;
	}
	void set_disk_usage_limit(const unsigned long & max){//in kilobytes
		max_disk_usage = max;
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
		p->second.do_not_cache_to_disk = false;
	}
	void clear_cache(){
		for (typename cache_t::iterator p = images.begin(); p != images.end();++p){
			ns_storage_delete_from_local_cache(storage,p->second.filename_on_disk);
		}
		//clear memory cache and all records
		this->current_disk_usage = 0;
		this->current_memory_usage = 0;
		images.clear();
	}

private:
	bool disk_cached_cleared;
	typedef std::map<unsigned long,ns_image_cache_object<ns_component> > cache_t;
	cache_t images;
	ns_image_storage_handler * storage;
	unsigned long max_memory_usage;
	unsigned long max_disk_usage;
	unsigned long current_memory_usage;
	unsigned long current_disk_usage;

	ns_image_whole<ns_component> & get_image(ns_image_server_image & image, ns_sql & sql, const bool read_only){
		//check to see if we have this in the cache somewhere
		typename cache_t::iterator p = images.find(image.id);
		unsigned long image_size;

		try{	
			if (p != images.end()){
				if (p->second.size_in_memory == 0){
					//if the image has been cached to disk, load it back in
					if (p->second.size_on_disk== 0)
						throw ns_ex("ns_image_cache::An image appears to have been transfered to the disk cache, but data seems to have been written") << ns_cache;
				
						ns_image_storage_source_handle<ns_component> im(ns_storage_request_from_local_cache(storage,p->second.filename_on_disk));
						const unsigned long im_s(ns_image_cache_object<ns_component>::image_uncompressed_size(im.input_stream().properties()));
						make_room_in_memory_for_new_image(image,im_s);
						//the previous operation might have scrambled our iterators, so we refind the correct one.
						p = images.find(image.id);
						if (p == images.end())
							throw ns_ex("ns_image_cache::get_image()::The cache records were scrambled after memory cleanup!") << ns_cache;
						p->second.size_in_memory = im_s;
						im.input_stream().pump(p->second.image,512);
						current_memory_usage+=p->second.size_in_memory;

					}
					p->second.last_access = ns_current_time();
					//if the image can be modified, the copy of the image on disk is invalidated.
					if (!read_only){
						ns_storage_delete_from_local_cache(storage,p->second.filename_on_disk);
						p->second.filename_on_disk.resize(0);
						p->second.do_not_cache_to_disk = true;
					}
				return p->second.image;
			}

			//since we're here, we couldn't find the image in the cache.  First thing we do is to make room for it in memory.

			//if the cache grows too large, transfer old images to disk.
			//that has gone unused the longest
			ns_image_storage_source_handle<ns_component> im(ns_storage_request_from_storage(storage,image,sql));
			image_size = ns_image_cache_object<ns_component>::image_uncompressed_size(im.input_stream().properties());
			ns_image_server_image imc;
			imc.id = 0;
			make_room_in_memory_for_new_image(imc,image_size);
			im.clear();
			
		}
		catch(ns_ex & ex){
			//any error that happens here is likely due to cache problems, rather than any problem with the source file.  flag as such.
			ns_ex ex2(ex);
			ex2 << ns_cache;
			throw ex2;
		}

		//now we create a cache entry and load the file to disk
		std::pair<typename cache_t::iterator,bool> r = images.insert(typename cache_t::value_type(image.id,ns_image_cache_object<ns_component>()));
		p = r.first;
		try{
			ns_image_storage_source_handle<ns_component> im(ns_storage_request_from_storage(storage,image,sql));
			im.input_stream().pump(p->second.image,1024);
		}
		catch(...){
			images.erase(p);
			throw;
		}
		p->second.last_access = ns_current_time();
		p->second.size_in_memory = image_size;
		current_memory_usage+=image_size;
		if (!read_only)
			p->second.do_not_cache_to_disk = true;
		return p->second.image;
	}

	
	//invalidates all references to the cache
	void make_room_in_memory_for_new_image(const ns_image_server_image & im, const unsigned long new_image_memory_req){


		while(current_memory_usage + new_image_memory_req > max_memory_usage){
			
			#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Memory cache exceeded.  Moving files to disk\n";
			#endif
			if (images.size() == 0)
				throw ns_ex("ns_image_cache::Cache size exceeded on an empty cache");
			typename cache_t::iterator oldest_in_memory = images.end();
			unsigned int num_locked = 0;
			for (typename cache_t::iterator p = images.begin(); p != images.end(); p++){
				if (p->first == im.id)
					continue;
				if (p->second.size_in_memory == 0 )
					continue; //this image is already cached on disk
				if (p->second.do_not_cache_to_disk){
					num_locked++;
					continue;
				}
				if(oldest_in_memory == images.end() || p->second.last_access <= oldest_in_memory->second.last_access)
					oldest_in_memory = p;
			}
		
			if (oldest_in_memory == images.end()){
				//memory is full.  We roll with it and allow the memory cache to get too large
				return;
			}
		
			write_image_to_disk(oldest_in_memory,im);
				
		}
		return;
	}

	//invalidates all pointers to the image map!
	void write_image_to_disk(typename cache_t::iterator & subject,const ns_image_server_image & image_not_to_delete){
		const unsigned long new_disk_req(subject->second.size_in_memory);
		bool can_write_to_disk(true);

		//if the job has already been done, we're golden!
		if (subject->second.size_on_disk > 0){
			#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Image has already been transferred to disk.\n";
			#endif
			subject->second.image.resize(ns_image_properties(0,0,0));
			current_memory_usage-=subject->second.size_in_memory;
			subject->second.size_in_memory = 0;
			return;

		}

		//start deleting files from disk until there is space for the current one
		while(current_disk_usage + new_disk_req > max_disk_usage){
			
			#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Disk cache size exceeded.  Cleaning up...\n";
			#endif
			if (images.size() == 0)
				throw ns_ex("ns_image_cache::write_image_to_disk()::Disk cache size exceeded on an empty cache");

			//identify the oldest image written to disk
			typename cache_t::iterator oldest_on_disk = images.end();
			unsigned int num_locked = 0;
			for (typename cache_t::iterator p = images.begin(); p != images.end(); p++){
				if (p->first == image_not_to_delete.id) continue; //don't delete image if it will be needed (ie if it is the one we're transferring into memory)
				if (p->second.size_on_disk == 0) continue; //nothing to be gained by deleting this one from disk
				if (p == subject) continue; //this image is the one we're trying to write to disk!
				if(oldest_on_disk == images.end() || p->second.last_access <= oldest_on_disk->second.last_access)
					oldest_on_disk = p;
			}
			if (oldest_on_disk == images.end())
				break;

			//delete the image
			
			#ifdef NS_VERBOSE_IMAGE_CACHE
			std::cerr << "Deleting file " << oldest_on_disk->second.filename_on_disk << "\n";
			#endif
			ns_storage_delete_from_local_cache(storage,oldest_on_disk->second.filename_on_disk);
			current_disk_usage-=oldest_on_disk->second.size_on_disk;
			oldest_on_disk->second.size_on_disk = 0;
			oldest_on_disk->second.to_erase = true;
		}
				
		
		//now write the subject to disk
		if (subject->second.filename_on_disk.size() == 0)
			subject->second.filename_on_disk = "image_cache=" + ns_to_string(subject->second.last_access) + "=" + ns_to_string(ns_current_time()) + "=" + ns_to_string(subject->first) + ".tif";
		
		#ifdef NS_VERBOSE_IMAGE_CACHE
		std::cerr << "Caching file to disk:" << subject->second.filename_on_disk << "\n";
		#endif
		try{	
			ns_image_storage_reciever_handle<ns_component> r( ns_storage_request_local_cache_storage(storage,subject->second.filename_on_disk,512,false));
			subject->second.image.pump(r.output_stream(),512);
		}
		catch(ns_ex & ex){
			ns_ex ex3("Cache problems: ");
			ex3 << ex.text() << ns_cache;
			ns_full_drive_cache_panic(ex3);
			throw ex3;
		}
		subject->second.size_on_disk=subject->second.size_in_memory;
		current_disk_usage+=subject->second.size_in_memory;

		subject->second.image.resize(ns_image_properties(0,0,0));
		current_memory_usage-=subject->second.size_in_memory;
		subject->second.size_in_memory = 0;

		//now go through and delete everything that needs to be deleted.
		bool deleted_something(true);
		#ifdef NS_VERBOSE_IMAGE_CACHE
		std::cerr << "Commiting pending image cache deletions:\n";
		#endif
		while(deleted_something){
			deleted_something = false;
			for (typename cache_t::iterator p = images.begin(); p != images.end(); p++){
				if (p->second.to_erase){
					
					#ifdef NS_VERBOSE_IMAGE_CACHE
					std::cerr << "Deleting record for " << p->second.filename_on_disk << "\n";
					#endif
					images.erase(p);
					deleted_something = true;
					break;
				}
			}
		}
	}
};


#endif
