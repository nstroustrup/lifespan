#ifndef NS_SPATIAL_AVG
#define NS_SPATIAL_AVG

#include "ns_image_stream_buffers.h"
#include "ns_image.h"
#include <vector>
#include <algorithm>



//#define NS_SPATIAL_AVG_TRACK_TIME



#ifdef NS_SPATIAL_AVG_TRACK_TIME
#include <time.h>
#endif

//There are two implementations of the median filter
//1) A homebrew version that allows the median to be calculated in horizontal chunks of arbitrary height.
//   This allows large images to be processed correctly.
//2) An optimized algorithm found in Reference: S. Perreault and P. Hébert, "Median Filtering in Constant Time",
//   IEEE Transactions on Image Processing, September 2007.
//   This is 40 times (7sec vs 280 sec!) faster than option 1, but requires the allocation of 3 times the memory requirement of the entire image.
//	 TBA: It should be possible to make this 1x the memory requirement of the entire image but care must be taken
//   to get the CPU caching to work right.
//if NS_OPTIMIZE_SPEED_OVER_MEMORY is defined, the second algorithm is used.

#define NS_OPTIMIZE_SPEED_OVER_MEMORY

template<class ns_component, bool calculate_difference>
class ns_spatial_median_calculator_operation_calc{
public:
	inline static ns_component run(const ns_component & a, const ns_component & b){
		return abs((int)a-(int)b)*(a<b);
	}
};
template<class ns_component>
class ns_spatial_median_calculator_operation_calc<ns_component,false>{
public:
	 inline static ns_component run(const ns_component & a, const ns_component & b){
		return b;
	}
};

#include "ctmf.h"

///Calculates the spatial median filter output of an image
///Implemented to allow streaming (ie the entire image never needs to be loaded into memory)
#pragma warning(disable: 4355)
template<class ns_component, bool calculate_difference>
class ns_spatial_median_calculator : public ns_image_stream_processor<ns_spatial_median_calculator<ns_component, calculate_difference> >{
public:
	typedef  ns_image_stream_static_offset_buffer<ns_component> storage_type;

	typedef ns_component component_type;

	ns_spatial_median_calculator(const long max_line_block_height, const long kernal_height_):
	  kernal_height(2*(kernal_height_/2)+1),
	  kernal_radius(kernal_height_/2),
													   //the largest possible buffer size occurs when reading the first line,
													   //when _kernal_height - 1 lines have previously been received (necissitating their buffering
													   //and then max_line_block_height is received.
		ns_image_stream_processor<ns_spatial_median_calculator<ns_component,calculate_difference> >(max_line_block_height, this){
	  	
		 if (kernal_radius <= 0)
			 throw ns_ex("ns_spatial_median_calculator::Cannot set kernal radius to zero");
	  }

	#pragma warning(default: 4355)
	  template<class reciever_t>
	  inline void prepare_to_recieve_image(const ns_image_properties & properties, reciever_t & reciever){
			this->default_prepare_to_recieve_image(properties,reciever);
	  }

	 bool init(const ns_image_properties & properties){
		 if (properties.width != 0 && properties.height != 0){
			 if (properties.width < (unsigned long)kernal_height)
				 throw ns_ex("ns_spatial_median_calculator::Attempting to run a median filter on an image whose width is smaller than the kernal height::Width ") 
				 			<< properties.width << " vs. kernal height " << kernal_height;
			 if (properties.height< (unsigned long)kernal_height)
				 throw ns_ex("ns_spatial_median_calculator::Attempting to run a median filter on an image whose height is smaller than the kernal height::Width ") 
				 			<< properties.height << " vs. kernal height " << kernal_height;
		 }
		input_buffer_height = 0;
		lines_received = 0;
		lines_processed = 0;
		image.prepare_to_recieve_image(properties);

		#ifdef NS_SPATIAL_AVG_TRACK_TIME
		computation_time_spent = 0;
		output_time_spent = 0;
		#endif
		return true;
	}

	~ns_spatial_median_calculator(){
	}
	template<class reciever_t>
	ns_image_stream_static_offset_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & p, reciever_t & reciever){
		return image.provide_buffer(p);
	}
	template<class reciever_t>
	void recieve_and_send_lines(const ns_image_stream_static_offset_buffer<ns_component> & lines, const unsigned long height, reciever_t & output_reciever){
		image.recieve_lines(lines,height);
	}

	template<class reciever_t>
	void finish_recieving_image(reciever_t & output_reciever){

		image.finish_recieving_image();

		#ifdef NS_SPATIAL_AVG_TRACK_TIME
		unsigned long start_time=ns_current_time(),
					stop_time;
		#endif


		const unsigned long w(image.properties().width),
							h(image.properties().height);
		unsigned char * src = 0,
					  * dst	=0;
		try{
			src =  new unsigned char[w*h];
			dst = new unsigned char[w*h];

			for (unsigned long y = 0; y < h; y++){
				for (unsigned long x = 0; x < w; x++){
					src[w*y+x] = image[y][x];
				}
			}



			//for (unsigned int y = 0; y < image.properties().height; y++)
			//	for (unsigned int x = 0; x < image.properties().width; x++)
			//		dst[image.properties().width*y+x] = src[image.properties().width*y+x];

			ctmf(src, dst,w,h,w,w,2*kernal_radius,1,512*1024);
			delete[] src;
			src = 0;

			/*for (unsigned int y = 0; y < image.properties().height; y++)
				for (unsigned int x = 0; x < image.properties().width; x++)
					dst[image.properties().width*y+x] = src[image.properties().width*y+x];*/

			output_reciever.prepare_to_recieve_image(image.properties());
			ns_image_stream_buffer_properties buf_prop;

			buf_prop.width = image.properties().width;
			unsigned long block_height = 1024;
			for (unsigned long lines_sent = 0; lines_sent < h;){
				unsigned long lines_to_send = h - lines_sent;
				if (lines_to_send > block_height)
					lines_to_send = block_height;

				buf_prop.height = lines_to_send;
				//get the reciever's buffer into which data will be written.
				output_reciever.output_buffer = output_reciever.provide_buffer(buf_prop);
				//write data to the buffer
				for (unsigned int y = 0; y < lines_to_send; y++)
					for (unsigned int x = 0; x < w; x++){
						if (image[lines_sent+y][x]==0) //values outside the mask should remain so after the median filter
							(*output_reciever.output_buffer)[y][x] = 0;
						else
						(*output_reciever.output_buffer)[y][x] = dst[w*(lines_sent+y)+x];
					}
				//inform the reciever that the data has been written.
				output_reciever.recieve_lines(*output_reciever.output_buffer,lines_to_send);
				lines_sent+=lines_to_send;
			}
			delete[] dst;
		}
		catch(...){
			if (src != 0)
				delete src;
			if (dst != 0)
				delete dst;
			throw;
		}

		#ifdef NS_SPATIAL_AVG_TRACK_TIME

		stop_time =  ns_current_time();
		std::cerr << "\nComputation time: " << stop_time-start_time << "s\n";
		#endif

		output_reciever.finish_recieving_image();
	}

private:

	ns_image_whole<ns_component> image;
	ns_spatial_median_calculator_operation_calc<ns_component,calculate_difference> desired_sub;
	const long kernal_height,
					    kernal_radius;
	unsigned long input_buffer_height;
	unsigned long lines_received,
				  lines_processed;

	#ifdef NS_SPATIAL_AVG_TRACK_TIME
	unsigned long computation_time_spent,
				  output_time_spent;
	#endif

	ns_component temp;
};

#endif