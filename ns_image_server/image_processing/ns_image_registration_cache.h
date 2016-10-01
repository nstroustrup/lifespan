#ifndef NS_IMAGE_REGISTRATION_CACHE
#define NS_IMAGE_REGISTRATION_CACHE
#include "ns_image.h"
#include "ns_buffered_random_access_image.h"
#include "ns_image_storage_handler.h"
#include "ns_simple_cache.h"

#define NS_MAX_CAPTURED_IMAGE_REGISTRATION_VERTICAL_OFFSET 200

#define NS_SECONDARY_DOWNSAMPLE_FACTOR 4

typedef std::vector<ns_64_bit> ns_image_registation_profile_dimension;
typedef enum {ns_no_registration,ns_threshold_registration,ns_sum_registration,ns_full_registration,ns_compound_registration} ns_registration_method;
	 
struct ns_downsampling_sizes{
	unsigned long downsample_factor,
			      downsample_factor_2;
	ns_image_properties downsampled,
						downsampled_2;
};

//#include"ns_image_easy_io.h"


typedef ns_image_buffered_multi_line_random_access_input_image<ns_8_bit,ns_image_storage_source<ns_8_bit> > ns_registration_disk_buffer;


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
		s.downsampled = s.downsampled_2 = whole_image_properties;
		if (spec_downsample_factor == 0){
			s.downsample_factor= sqrt((double)((whole_image_properties.width*(ns_64_bit)whole_image_properties.height)/(max_average_dimention*max_average_dimention)));
			if (s.downsample_factor > 20)
				s.downsample_factor = 20;
		}
		else s.downsample_factor=spec_downsample_factor;

		s.downsample_factor_2 = NS_SECONDARY_DOWNSAMPLE_FACTOR;
			
		s.downsampled.width=ceil(whole_image_properties.width/(float)s.downsample_factor);
		s.downsampled.height=ceil(whole_image_properties.height/(float)s.downsample_factor);

		s.downsampled_2.width=ceil(whole_image_properties.width/(float)NS_SECONDARY_DOWNSAMPLE_FACTOR);
		s.downsampled_2.height=ceil(whole_image_properties.height/(float)NS_SECONDARY_DOWNSAMPLE_FACTOR);
		return s;

	 }

	 
	static void ns_fast_downsample(ns_registration_disk_buffer & source,ns_image_standard & downsample_1, ns_image_storage_reciever_handle<ns_8_bit> & downsample_2,ns_image_storage_reciever_handle<ns_8_bit> &  whole_image_out, const unsigned long max_average_dimention, const unsigned long spec_downsample_factor=0){
		ns_downsampling_sizes sizes(ns_image_registration_profile<ns_8_bit>::calculate_downsampled_sizes(source.properties(),max_average_dimention,spec_downsample_factor));
		downsample_1.init(sizes.downsampled);
		downsample_2.output_stream().init(sizes.downsampled_2);
		whole_image_out.output_stream().init(source.properties());
		ns_image_stream_static_buffer<ns_8_bit> downsampled_2_buf(ns_image_stream_buffer_properties(sizes.downsampled_2.width,1024));
		ns_image_stream_static_buffer<ns_8_bit> whole_buf(ns_image_stream_buffer_properties(source.properties().width,256));

		//book-keeping for buffering
		long downsampled_2_buf_height = 0,
			 downsampled_2_buf_lines_written = 0,
			 whole_buf_height = 0,
			 whole_buf_lines_written = 0;

		for (long y = 0; y < source.properties().height; y++){
			source.make_line_available(y);
			//write downsample 1 to memory
			if (y%sizes.downsample_factor == 0){
				for (long x = 0; x < source.properties().width; x+=sizes.downsample_factor)
					downsample_1[y/sizes.downsample_factor][x/sizes.downsample_factor] = source[y][x];
			}
			//write downsample to disk, with buffering
			if (y%sizes.downsample_factor_2 == 0){
				for (long x = 0; x < source.properties().width; x+=sizes.downsample_factor_2)
					downsampled_2_buf[y/sizes.downsample_factor_2 - downsampled_2_buf_lines_written][x/sizes.downsample_factor_2] = source[y][x];
				downsampled_2_buf_height++;
				if (downsampled_2_buf_height==downsampled_2_buf.properties().height){
					downsample_2.output_stream().recieve_lines(downsampled_2_buf,downsampled_2_buf_height);
					downsampled_2_buf_lines_written+=downsampled_2_buf_height;
					downsampled_2_buf_height = 0;
				}
			}
			//write whole image to disk, with buffering.
			for (long x = 0; x < source.properties().width; x++)
					whole_buf[y-whole_buf_lines_written][x] = source[y][x];
			whole_buf_height++;
			if (whole_buf_height==whole_buf.properties().height){
				whole_image_out.output_stream().recieve_lines(whole_buf,whole_buf_height);
				whole_buf_lines_written+=whole_buf_height;
				whole_buf_height = 0;
			}

		}
		if (downsampled_2_buf_height> 0){
			downsample_2.output_stream().recieve_lines(downsampled_2_buf,downsampled_2_buf_height);
		}
		downsample_2.output_stream().finish_recieving_image();
		if (whole_buf_height> 0){
			whole_image_out.output_stream().recieve_lines(whole_buf,whole_buf_height);
		}
		whole_image_out.output_stream().finish_recieving_image();
	}
};



struct ns_image_registration_profile_data_source {
	 unsigned long max_average_dimention;
	ns_sql * sql;
	const ns_image_storage_handler * image_storage;
//	unsigned long downsample_factor;
};


class ns_disk_buffered_image_registration_profile : public ns_image_registration_profile<ns_registration_disk_buffer>, public ns_simple_cache_data<ns_image_server_image, ns_image_registration_profile_data_source> {
public:
	ns_disk_buffered_image_registration_profile():whole_image_source(0),
													downsampled_image_2_source(0){}
	ns_image_server_image image_record;
	ns_64_bit size_in_memory_in_kbytes() const {
		return (whole_image.properties().width*
			whole_image.properties().height*
			whole_image.properties().components *
			sizeof(ns_8_bit)) / 1024;
	}

	void load_from_external_source(const ns_image_server_image & im, ns_image_registration_profile_data_source & data_source) {
			image_record = im;
			ns_image_storage_source_handle<ns_8_bit> source(data_source.image_storage->request_from_storage(image_record, data_source.sql));
			ns_registration_disk_buffer whole_image_long_term_storage;
			whole_image_long_term_storage.assign_buffer_source(source.input_stream(),0,1024);

			//ns_image_standard image;
			//source.input_stream().pump(image,1024);
			const ns_downsampling_sizes downsampling_sizes(calculate_downsampled_sizes(whole_image_long_term_storage.properties(), data_source.max_average_dimention));
			this->downsampling_factor = downsampling_sizes.downsample_factor;
			
		
			//image.resample(downsampling_sizes.downsampled,downsampled_image);
			//image.resample(downsampling_sizes.downsampled_2,downsampled_2);
	//		throw ns_ex("SHA");
			whole_filename = std::string("registration_cache_")+ns_to_string(im.id);
			downsampled_filename = whole_filename + "_downsampled.tif";
			whole_filename += ".tif";
			{
				ns_image_storage_reciever_handle<ns_8_bit> whole_image_out(data_source.image_storage->request_local_cache_storage(whole_filename,ns_tiff_lzw,256,false));
				ns_image_storage_reciever_handle<ns_8_bit> downsample_2_out(data_source.image_storage->request_local_cache_storage(downsampled_filename, ns_tiff_lzw, 1024,false));
				//no need to do linear interpolation.  Nobody sees these images and any aliasing will be handled by comparrison between the less downsampled copies
				ns_fast_downsample(whole_image_long_term_storage,downsampled_image,downsample_2_out,whole_image_out, data_source.max_average_dimention);
			}

		//	throw ns_ex("WHA");
			whole_image_source = data_source.image_storage->request_from_local_cache(whole_filename,false);
			whole_image.assign_buffer_source(whole_image_source.input_stream(),NS_MAX_CAPTURED_IMAGE_REGISTRATION_VERTICAL_OFFSET,1024);
		
			
			downsampled_image_2_source = data_source.image_storage->request_from_local_cache(downsampled_filename,false);
			downsampled_image_2.assign_buffer_source(downsampled_image_2_source.input_stream(),NS_MAX_CAPTURED_IMAGE_REGISTRATION_VERTICAL_OFFSET,1024);
	}
	void clean_up(ns_image_registration_profile_data_source & data_source){
		downsampled_image.clear();
		downsampled_image_2.clear();
		downsampled_image_2_source.clear();
		whole_image.clear();
		whole_image_source.clear();
		data_source.image_storage->delete_from_local_cache(whole_filename);
		data_source.image_storage->delete_from_local_cache(downsampled_filename);
	}
	ns_64_bit id() const { return image_record.id; }
	ns_64_bit to_id(const ns_image_server_image & im) const { return im.id; }
private:	
	std::string whole_filename,
		downsampled_filename;
	ns_image_storage_source_handle<ns_8_bit> whole_image_source,
											 downsampled_image_2_source;
};

class ns_memory_image_registration_profile : public ns_image_registration_profile< ns_image_standard >, public ns_simple_cache_data<ns_image_server_image, ns_image_registration_profile_data_source> {
public:
	ns_image_server_image image_record;
	ns_64_bit size_in_memory_in_kbytes() const {
		return (whole_image.properties().width*
			whole_image.properties().height*
			whole_image.properties().components *
			sizeof(ns_8_bit)) / 1024;
	}

	void load_from_external_source(const ns_image_server_image & im, ns_image_registration_profile_data_source & data_source){
			image_record = im;
			ns_image_storage_source_handle<ns_8_bit> source(data_source.image_storage->request_from_storage(image_record,data_source.sql));
			const ns_downsampling_sizes downsampling_sizes(calculate_downsampled_sizes(source.input_stream().properties(), data_source.max_average_dimention));
			this->downsampling_factor = downsampling_sizes.downsample_factor;

			source.input_stream().pump(whole_image,1024);

			whole_image.resample(downsampling_sizes.downsampled,downsampled_image);
			downsampled_image.resample(downsampling_sizes.downsampled_2,downsampled_image_2);
	}
	ns_64_bit id() const { return image_record.id; }
	ns_64_bit to_id(const ns_image_server_image & im) const { return im.id; }

	void cleanup(ns_image_storage_handler * image_storage){whole_image.clear();downsampled_image.clear();downsampled_image_2.clear();}
};

class test_class : public ns_simple_cache_data<ns_image_server_image, ns_image_registration_profile_data_source> {
public:

};
using ns_image_registration_profile_cache = ns_simple_cache<ns_disk_buffered_image_registration_profile,true> ;

#endif
