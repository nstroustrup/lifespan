#ifndef NS_PROCESS_16_BIT
#define NS_PROCESS_16_BIT
#include "ns_image.h"
#include "ns_image_statistics.h"
#include "ns_resampler.h"

typedef enum{ns_features_are_dark,ns_features_are_light} ns_feature_intensity;
///Takes a 16 bit image as input and fills the 8-bit output from vales [0-set_crop_value] of the input.
template<ns_feature_intensity features, class storage_buffer>
class ns_image_process_16_bit : public ns_image_stream_reciever<ns_image_stream_static_buffer<ns_16_bit> >, ns_image_stream_sender<ns_8_bit,ns_image_process_16_bit<features, storage_buffer>,unsigned long>{
public:	
	typedef ns_image_stream_static_buffer<ns_16_bit> storage_type;
	typedef ns_16_bit component_type;
	ns_image_process_16_bit(const long max_line_block_height):ns_image_stream_sender<ns_8_bit,ns_image_process_16_bit<features, storage_buffer>,unsigned long>(ns_image_properties(0,0,0),this),
															  ns_image_stream_reciever<ns_image_stream_static_buffer<ns_16_bit> >(max_line_block_height,this),
															  _max_line_block_height(max_line_block_height),
															  small_image_output_buffer(0),
															  resampler_binding(max_line_block_height),
															  resampler(max_line_block_height),
															  crop_value(64),resample_image_to_output(false){}
	ns_image_statistics image_statistics;
	unsigned long init_send() { return 0; }
	unsigned long init_send_const() const { return 0; }
	unsigned long seek_to_beginning() { return 0; }
	void finish_send(){}

	template<class storage_buffer_2>
	void send_lines(storage_buffer_2 & output, const unsigned int lines_to_send, unsigned long & unusued_external_state){
		ns_image_stream_buffer_properties prop;
		prop.height = lines_to_send;
		prop.width = buffer.properties().width;
		//if we also want to output a resampled copy, send data on to the resampler and the reciever
		if (resample_image_to_output){
			ns_resampler<ns_8_bit>::storage_type * rbuf(resampler_binding.provide_buffer(prop));
			if (features== ns_features_are_dark){
				for (unsigned int y = 0; y < lines_to_send; y++)
					for (unsigned int x = 0; x < buffer.properties().width; x++){
						unsigned long c = buffer[y][x];
						c = (255*c)/crop_value;
						c/=256;
						if (c > 255)
							c = 255;
						output[y][x] = (ns_8_bit)(c);
						*(rbuf)[y][x] = (ns_8_bit)(c);
						image_statistics.histogram[(ns_8_bit)(c)]++;
				}
			}
			else{
				for (unsigned int y = 0; y < lines_to_send; y++)
					for (unsigned int x = 0; x < buffer.properties().width; x++){
						unsigned long c = USHRT_MAX-buffer[y][x];
						c = (255*c)/crop_value;
						c/=256;
						if (c > 255)
							c = 255;
						output[y][x] = (ns_8_bit)(255-c);
		
						(*rbuf)[y][x] = (ns_8_bit)(255-c);
						image_statistics.histogram[(ns_8_bit)(255-c)]++;
					}
			}
			resampler_binding.recieve_lines(*rbuf,lines_to_send);
		}
		//otherwise just output the data to the reciever
		else{
			if (features== ns_features_are_dark){
				for (unsigned int y = 0; y < lines_to_send; y++)
					for (unsigned int x = 0; x < buffer.properties().width; x++){
						unsigned long c = buffer[y][x];
						c = (255*c)/crop_value;
						c/=256;
						if (c > 255)
							c = 255;
						output[y][x] = (ns_8_bit)(c);
						image_statistics.histogram[(ns_8_bit)(c)]++;
					}
			}
			else{
				for (unsigned int y = 0; y < lines_to_send; y++)
					for (unsigned int x = 0; x < buffer.properties().width; x++){
						unsigned long c = USHRT_MAX-buffer[y][x];
						c = (255*c)/crop_value;
						c/=256;
						if (c > 255)
							c = 255;
						output[y][x] = (ns_8_bit)(255-c);
						image_statistics.histogram[(ns_8_bit)(255-c)]++;
				}
			}
		}
	}
	void set_crop_value(const ns_16_bit & highest_input_value){
		crop_value = highest_input_value;
	}
	
	template<class reciever_t>
	inline void prepare_to_recieve_image(const ns_image_properties & properties, reciever_t & reciever){
		if (resample_image_to_output){
			resampler.set_maximum_dimentions(ns_image_server_captured_image::small_image_maximum_dimensions());
			resampler_binding.prepare_to_recieve_image(properties);
		}
		reciever.prepare_to_recieve_image(properties);
		init(properties);
	}

	void set_small_image_output(ns_image_storage_reciever_handle<ns_8_bit> & small_image_output){
		resample_image_to_output = true;
		small_image_output_buffer= &small_image_output;
		resampler_binding.bind(&resampler,&small_image_output_buffer->output_stream());
	}
			

	bool init(const ns_image_properties & properties){
		

		image_statistics.size.x = properties.width;
		image_statistics.size.y = properties.height;

		ns_image_stream_buffer_properties prop;
		prop.height = _max_line_block_height;
		prop.width = properties.width*properties.components;
		bool resized(false);
		if (!(buffer.properties() == prop)){
			if (buffer.properties().height != prop.height || buffer.properties().width != prop.width){
				buffer.resize(prop);
				resized = true;
			}
			ns_image_stream_reciever<ns_image_stream_static_buffer<ns_16_bit> >::_properties =  properties;
		}
		return resized;
	}

	ns_image_stream_static_buffer<ns_16_bit> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		//buffer_properties is ignored as this data has been specified by init
		return &buffer;
	}
	template<class reciever_t>
	ns_image_stream_static_buffer<ns_16_bit> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties, reciever_t & rec){
		//buffer_properties is ignored as this data has been specified by init
		return &buffer;
	}


	void recieve_lines(const ns_image_stream_static_buffer<ns_16_bit> & buffer, const unsigned long height){}	

	template<class reciever_t>
	void recieve_and_send_lines(const ns_image_stream_static_buffer<ns_16_bit> & buffer, const unsigned long height, reciever_t & rec){
		ns_image_stream_buffer_properties p;
		p.height = height;
		p.width = buffer.properties().width;
		typename reciever_t::storage_type * buf = rec.provide_buffer(p);
		unsigned long unused_internal_state;
		send_lines(*buf,height, unused_internal_state);
		rec.recieve_lines(*buf,height);
	}

	void finish_recieving_image(){resampler_binding.finish_recieving_image();}
	template<class reciever_t>
	void finish_recieving_image(reciever_t & t){
		t.finish_recieving_image();
		if (resample_image_to_output)
			resampler_binding.finish_recieving_image();
	}
	
private:
	bool resample_image_to_output;
	ns_resampler<ns_8_bit> resampler;
	ns_image_stream_binding< ns_resampler<ns_8_bit>, ns_image_storage_reciever<ns_8_bit> > resampler_binding;
	ns_image_storage_reciever_handle<ns_8_bit> * small_image_output_buffer;
	ns_16_bit crop_value;
	ns_image_stream_static_buffer<ns_16_bit> buffer;
	unsigned int _max_line_block_height;
};

///open a 16 bit tiff image, extract the darkpixels into a new 8 bit image.
template<ns_feature_intensity features>
void ns_convert_16_bit_tif(const std::string & input_file, const std::string & output_file){
	ns_tiff_image_input_file<ns_16_bit> tiff_in;
	tiff_in.open_file( input_file);
	ns_image_stream_file_source<ns_16_bit > file_source(tiff_in);

	ns_tiff_image_output_file<ns_8_bit> tiff_out;
	ns_image_stream_file_sink<ns_8_bit > file_sink(output_file,tiff_out,1024);


	ns_image_process_16_bit<features, ns_image_stream_static_offset_buffer<ns_16_bit> > processor(1024);
	ns_image_stream_binding< ns_image_process_16_bit<features, ns_image_stream_static_offset_buffer<ns_16_bit> >,
							 ns_image_stream_file_sink<ns_8_bit > > binding(processor,file_sink,1024);

	file_source.pump(binding,1024);
}

#endif
