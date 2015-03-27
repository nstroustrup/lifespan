#ifndef NS_IMAGE_REGISTRATION_CACHE
#define NS_IMAGE_REGISTRATION_CACHE
#include "ns_image.h"
#include "ns_buffered_random_access_image.h"
#include "ns_image_storage_handler.h"

#define NS_MAX_CAPTURED_IMAGE_REGISTRATION_VERTICAL_OFFSET 200

#define NS_SECONDARY_DOWNSAMPLE_FACTOR 4

typedef std::vector<ns_64_bit> ns_image_registation_profile_dimension;
typedef enum {ns_no_registration,ns_threshold_registration,ns_sum_registration,ns_full_registration,ns_compound_registration} ns_registration_method;
	 
struct ns_downsampling_sizes{
	unsigned long downsample_factor;
	ns_image_properties downsampled,
						downsampled_2;
};

template<class image_type>
struct ns_image_registration_profile{
	 ns_image_registration_profile():registration_method(ns_no_registration),average(0),downsampling_factor(0),last_accessed_timestamp(0){}
	 ns_image_registation_profile_dimension horizontal_profile,
											vertical_profile;
	 ns_64_bit average;

	 unsigned long downsampling_factor;
	 ns_registration_method registration_method;

	 image_type whole_image,
				downsampled_image_2;

	 ns_image_standard downsampled_image;

	 unsigned long last_accessed_timestamp;

	 static ns_downsampling_sizes calculate_downsampled_sizes(const ns_image_properties & whole_image_properties,const unsigned long max_average_dimention,const unsigned long spec_downsample_factor=0){
		ns_downsampling_sizes s;

		if (spec_downsample_factor == 0){
			s.downsample_factor= sqrt((double)((whole_image_properties.width*(ns_64_bit)whole_image_properties.height)/(max_average_dimention*max_average_dimention)));
			if (s.downsample_factor > 20)
				s.downsample_factor = 20;
		}
		else s.downsample_factor=spec_downsample_factor;
			
		s.downsampled.width=whole_image_properties.width/s.downsample_factor;
		s.downsampled.height=whole_image_properties.height/s.downsample_factor;

		s.downsampled_2.width=whole_image_properties.width/NS_SECONDARY_DOWNSAMPLE_FACTOR;
		s.downsampled_2.height=whole_image_properties.height/NS_SECONDARY_DOWNSAMPLE_FACTOR;
		return s;

	 }
};

typedef ns_image_buffered_multi_line_random_access_input_image<ns_8_bit,ns_image_storage_source<ns_8_bit> > ns_registration_disk_buffer;
class ns_disk_buffered_image_registration_profile : public ns_image_registration_profile<ns_registration_disk_buffer>{
public:
	ns_disk_buffered_image_registration_profile():whole_image_source(0),
													downsampled_image_2_source(0){}
	void prepare_images(ns_image_server_image & im,const unsigned long max_average_dimention,ns_sql & sql,ns_image_storage_handler * image_storage,const unsigned long downsample_factor=0){

			ns_image_storage_source_handle<ns_8_bit> source(image_storage->request_from_storage(im,&sql));

			ns_image_standard image;
			source.input_stream().pump(image,1024);
			const ns_downsampling_sizes downsampling_sizes(calculate_downsampled_sizes(image.properties(),max_average_dimention,downsample_factor));
			this->downsampling_factor = downsampling_sizes.downsample_factor;

			image.resample(downsampling_sizes.downsampled,downsampled_image);

			ns_image_standard downsampled_2;
			image.resample(downsampling_sizes.downsampled_2,downsampled_2);
		

			whole_filename = std::string("registration_cache_")+ns_to_string(im.id);
			downsampled_filename = whole_filename + "_downsampled.tif";
			whole_filename += ".tif";
			
			const unsigned long buffer_height=256;
			{
				ns_image_storage_reciever_handle<ns_8_bit> out(image_storage->request_local_cache_storage(whole_filename,buffer_height,false));
				image.pump(out.output_stream(),buffer_height);
				out.clear();
				whole_image_source = image_storage->request_from_local_cache(whole_filename,false);
				whole_image.assign_buffer_source(whole_image_source.input_stream(),NS_MAX_CAPTURED_IMAGE_REGISTRATION_VERTICAL_OFFSET,buffer_height);
			}
			{
				ns_image_storage_reciever_handle<ns_8_bit> out(image_storage->request_local_cache_storage(downsampled_filename,buffer_height,false));
				downsampled_2.pump(out.output_stream(),buffer_height);
				out.clear();
				downsampled_image_2_source = image_storage->request_from_local_cache(downsampled_filename,false);
				downsampled_image_2.assign_buffer_source(downsampled_image_2_source.input_stream(),NS_MAX_CAPTURED_IMAGE_REGISTRATION_VERTICAL_OFFSET,buffer_height);
			}
	}
	void cleanup(ns_image_storage_handler * image_storage){
		downsampled_image.clear();
		downsampled_image_2.clear();
		downsampled_image_2_source.clear();
		whole_image.clear();
		whole_image_source.clear();
		image_storage->delete_from_local_cache(whole_filename);
		image_storage->delete_from_local_cache(downsampled_filename);
	}
private:	
	std::string whole_filename,
		downsampled_filename;
	ns_image_storage_source_handle<ns_8_bit> whole_image_source,
											 downsampled_image_2_source;
};

class ns_memory_image_registration_profile : public ns_image_registration_profile< ns_image_standard >{
public:
	void prepare_images(ns_image_server_image & im,const unsigned long max_average_dimention,ns_sql & sql,ns_image_storage_handler * image_storage,const unsigned long downsample_factor=0){

			ns_image_storage_source_handle<ns_8_bit> source(image_storage->request_from_storage(im,&sql));
			const ns_downsampling_sizes downsampling_sizes(calculate_downsampled_sizes(source.input_stream().properties(),max_average_dimention,downsample_factor));
			this->downsampling_factor = downsampling_sizes.downsample_factor;

			source.input_stream().pump(whole_image,1024);

			whole_image.resample(downsampling_sizes.downsampled,downsampled_image);
			whole_image.resample(downsampling_sizes.downsampled_2,downsampled_image_2);
	}
	void cleanup(ns_image_storage_handler * image_storage){whole_image.clear();downsampled_image.clear();downsampled_image_2.clear();}
};


template<class profile_type>
class ns_image_registration_profile_cache{
public:
	ns_image_registration_profile_cache(const ns_64_bit max_size_in_mb ):cache_size(0),max_size_in_megabytes(max_size_in_mb){}
	profile_type * get(const ns_64_bit & id){
	  typename cache_type::iterator p(cache.find(id));
		if (p != cache.end()){
			p->second->last_accessed_timestamp = ns_current_time();
			return p->second;
		}
		return 0;
	}
	void remove_old_images(unsigned long max_age_in_seconds,ns_image_storage_handler * image_storage){
		const unsigned long cur_time(ns_current_time());
		std::vector<typename cache_type::iterator> to_delete;
		to_delete.reserve(5);
		for (typename cache_type::iterator p = cache.begin(); p!=cache.end();p++){
			if (cur_time - p->second->last_accessed_timestamp > max_age_in_seconds){
				p->second->cleanup(image_storage);
				to_delete.push_back(p);
			}
		}
		for (unsigned long i = 0; i < to_delete.size(); i++)
		  cache.erase(to_delete[i]);
		
	}
	void insert(const ns_64_bit id,profile_type * profile,ns_image_storage_handler * image_storage){
		ns_64_bit profile_size;
		
		profile->last_accessed_timestamp = ns_current_time();

		cache_size++;
		cache[id] = profile;
	}
	void cleanup(ns_image_storage_handler * image_storage){
	  //std::cerr << "Cleaning out local image registration cache...\n";
		for (typename cache_type::iterator p = cache.begin(); p!=cache.end();p++){
			p->second->cleanup(image_storage);
		}
	}
	void clear(){
		for (typename cache_type::iterator p = cache.begin(); p!=cache.end();p++)
			delete p->second;
		
		cache.clear();
		cache_size = 0;
	}
	~ns_image_registration_profile_cache(){clear();}
private:
	typedef std::map<ns_64_bit,profile_type *> cache_type;
	cache_type cache;
	ns_64_bit cache_size,
		max_size_in_megabytes;
};


#endif
