#ifndef NS_SIMPLE_IMAGE_CACHE
#define NS_SIMPLE_IMAGE_CACHE
#include "ns_image_server.h"

class ns_simple_image_cache_element{
public:
	bool is_swapped_to_disk(){return filename_on_disk.size()!=0;}
	ns_image_standard image;
	std::string filename_on_disk;
	unsigned long size_in_memory;
	unsigned long last_access_time; //UNIX_TIME in seconds
	ns_cache_reference reference;
};

typedef unsigned long ns_image_cache_reference;

class ns_simple_image_cache{
public:
	ns_simple_image_cache(const unsigned long max_cache_size_):max_cache_reference(0),max_cache_size(max_cache_size_),current_cache_size(0){}
	ns_image_standard & request_new_cache_entry(const ns_image_properties &prop, ns_cache_reference & reference){
		ns_simple_image_cache_element & e(cache[max_cache_reference]);
		e.reference = 
			reference = max_cache_reference;
		max_cache_reference++;
		e.size_in_memory=prop.width*prop.height*prop.components;
		last_access_time = ns_current_time();
		current_cache_size+=e.size_in_memory;
		if(current_cache_size > max_cache_size)
			shrink_cache();
		return e.image;
	}
	ns_image_standard & get_image(const ns_cache_reference & reference){
		ns_simple_image_cache_element & e(cache[reference]);
		if (e.is_swapped_to_disk())
			swap_from_disk(e);
		return e.image;
	}

private:
	std::map<ns_cache_reference,ns_simple_image_cache_element> cache;
	void swap_to_disk(ns_simple_image_cache_element & e){
		e.filename_on_disk = "im_" + reference + ".tif";
		ns_image_storage_reciever_handle<ns_8_bit> & out (image_storage->request_local_cache_storage(e.filename_on_disk,1024));
		e.image.pump(out.input_stream(),1024);
		e.image.clear();
	}
	void swap_from_disk(ns_simple_image_cache_element & e){
		ns_image_storage_source_handle<ns_8_bit> & in(image_storage->request_from_local_cache_storage(e.filename_on_disk,1024));
		e.last_access_time = ns_current_time();
		current_cache_size+=e.size_in_memory;
		if(current_cache_size > max_cache_size)
			shrink_cache();
		in.output_stream().pump(e.image,1024);
		e.filename_on_disk.resize(0);
	}
	void shrink_cache(){
		unsigned long current_cache_size

	}
	unsigned long max_cache_reference;
	unsigned long current_cache_size;
	const unsigned long max_cache_size;
};

#endif
