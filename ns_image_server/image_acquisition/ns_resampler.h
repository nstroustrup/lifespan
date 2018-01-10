#ifndef NS_RESAMPLER
#define NS_RESAMPLER

#include "ns_image_stream_buffers.h"
#include "ns_image.h"
#include <vector>

#pragma warning(disable: 4355)
template<class ns_component>
class ns_resampler : public ns_image_stream_processor<ns_resampler<ns_component> >{
public:
	typedef ns_image_stream_sliding_offset_buffer<ns_component> storage_type;

	typedef ns_component component_type;


	void set_resize_factor(const ns_vector_2d resize_factor_){resize_factor = resize_factor_;}
	void set_maximum_dimentions(const ns_vector_2i max_){maximum_output_dimentions = max_;}

	ns_resampler(const long max_line_block_height):
	  resize_factor(1.0,1.0),
	  maximum_output_dimentions(0,0),
	  ns_image_stream_processor<ns_resampler<ns_component> >(max_line_block_height, this){}

	#pragma warning(default: 4355)
	  template<class reciever_t>
	  inline void prepare_to_recieve_image(const ns_image_properties & properties, reciever_t & reciever){
		ns_image_stream_processor<ns_resampler<ns_component> >::_properties = properties;
		init(properties);
	//	std::cerr << "Resize factor = " << resize_factor << "\n";
	//	std::cerr << "Output Properties  = " << output_properties.width << "," << output_properties.height << "\n";
		reciever.prepare_to_recieve_image(output_properties);
	  }

	 bool init(const ns_image_properties & properties){

		 if (!(maximum_output_dimentions == ns_vector_2i(0,0))){
			 //find the limiting dimension
			double resize_x((double)maximum_output_dimentions.x/(double)properties.width),
					resize_y((double)maximum_output_dimentions.y/(double)properties.height);
			double resize = resize_x;

			if (resize > resize_y)
				resize = resize_y;
			//if the limiting dimension requires the image to be shrunk, mark it so.
			if (resize < 1)
				resize_factor.x = resize_factor.y = resize;
		 }	
		 if (resize_factor.x > 1){
			 resize_factor.x = 1;
		//	cerr << "ns_resampler::Expansion is not yet implemented";
		 } 
		 if (resize_factor.y > 1){
			 resize_factor.y = 1;
		//	cerr << "ns_resampler::Expansion is not yet implemented";
		 }
		output_properties = properties;
		output_properties.width=(unsigned long)(output_properties.width*resize_factor.x);
		output_properties.height=(unsigned long)(output_properties.height*resize_factor.y);
		output_properties.resolution=(float)(output_properties.resolution*resize_factor.y);
	

		current_output_y_coordinate = 0;
    	current_top_of_buffer_in_input_y_coordinates = 0;
	 	input_buffer_height = 0;

		kernal_radius = (unsigned int)(0.5/resize_factor.y)+1;
		kernal_height = 2*kernal_radius;

		ns_image_stream_buffer_properties bufp;
		bufp.width = properties.width*properties.components;
		bufp.height = 2*ns_image_stream_processor<ns_resampler<ns_component> >::_max_line_block_height + kernal_height;
		in_buffer.resize(bufp);
		in_buffer.set_offset(0);
		return true;
	}

	template<class reciever_t>
	ns_image_stream_sliding_offset_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & p, reciever_t & reciever){

		in_buffer.set_offset(input_buffer_height);
		return &in_buffer;

	}
	template<class reciever_t>
	void recieve_and_send_lines(const ns_image_stream_sliding_offset_buffer<ns_component> & lines, const unsigned long height, reciever_t & output_reciever){
		ns_image_properties & input_properties(ns_image_stream_processor<ns_resampler<ns_component> >::_properties);

		//std::cerr << (lines_received*100)/im_properties.height << "%...";

		input_buffer_height+= height;
		current_top_of_buffer_in_input_y_coordinates+=height;
		
		const long greatest_output_y_we_can_produce((long)(floor((resize_factor.y*current_top_of_buffer_in_input_y_coordinates)-0.5)));
		const long output_lines_to_produce(greatest_output_y_we_can_produce - current_output_y_coordinate);
		
		//std::cerr << "\t\treceived   " << height << " (" << current_top_of_buffer_in_input_y_coordinates<< "/" << input_properties.height << ")\n";
		if (output_lines_to_produce <= 0) return;

	//	std::cerr << "\t\t\t\tOutputting " << output_lines_to_produce << " (" <<  current_output_y_coordinate << "/" << output_properties.height << ")\n";

		fill_output_buffer_from_input(lines,output_reciever,output_lines_to_produce);
		

		//we have now written lines of the output image up to but not including (current_output_y_coordinate + output_lines_to_produce).
		current_output_y_coordinate+=output_lines_to_produce;

		//we now no longer need lines below what is needed to calculate the new current output y coordinate
		const long lines_to_pop((long)(floor((current_output_y_coordinate-0.5)/resize_factor.y)- current_bottom_of_buffer_in_input_y_coordinates()));
		input_buffer_height-=lines_to_pop;
		in_buffer.step(lines_to_pop);


	}
	template<class reciever_t>
	void finish_recieving_image(reciever_t & output_reciever){
		long remaining_output_lines = output_properties.height - current_output_y_coordinate;
		if (remaining_output_lines > 0)
			fill_output_buffer_from_input(in_buffer,output_reciever, remaining_output_lines);
		output_reciever.finish_recieving_image();
	}
private:
	
	inline long current_bottom_of_buffer_in_input_y_coordinates(){return current_top_of_buffer_in_input_y_coordinates - input_buffer_height;}

	
	template<class reciever_t>
	void fill_output_buffer_from_input(const ns_image_stream_sliding_offset_buffer<ns_component> & lines, reciever_t & output_reciever, const long output_lines_to_produce){
		ns_image_properties & input_properties(ns_image_stream_processor<ns_resampler<ns_component> >::_properties);
		
		in_buffer.set_offset(0);

		ns_image_stream_buffer_properties bufp;
		bufp.height = (unsigned long)(output_lines_to_produce);
		bufp.width = (unsigned long)(output_properties.width*output_properties.components);
		output_reciever.output_buffer  = output_reciever.provide_buffer(bufp);

		const unsigned int w(this->properties().width);
		const unsigned int h(this->properties().height);
		const unsigned int c(this->properties().components);

		for (long y = current_output_y_coordinate; y < (current_output_y_coordinate + output_lines_to_produce); y++){
			long y_0 = (long)((y-.5)/resize_factor.y);
			long y_1 = (long)((y+.5)/resize_factor.y);
			if (y_0 < current_bottom_of_buffer_in_input_y_coordinates())
				y_0 = current_bottom_of_buffer_in_input_y_coordinates();
			if (y_1 >= current_top_of_buffer_in_input_y_coordinates)
				y_1 = (long)current_top_of_buffer_in_input_y_coordinates-1;
	//		cerr << y_0-current_bottom_of_buffer_in_input_y_coordinates() << "-" 
		//		<< y_1-current_bottom_of_buffer_in_input_y_coordinates()  << "\n";
			for (unsigned int x = 0; x < output_properties.width; x++){
				//we calculate the range of pixels that are used to calculate the output pixel (x,y)
				//which is a block (y_0,x_0) -> (y_1,x_1)
				long x_0 = (long)((x-.5)/resize_factor.x);
				long x_1 = (long)((x+.5)/resize_factor.x);
				if (x_0 < 0)
					x_0 = 0;
				if (x_1 >= (long)input_properties.width)
					x_1 = (long)input_properties.width-1;
				//cerr << x_0 << ":" << x_1 << "," << y_0 << ":"<<y_1<<"\n";
				int area = (y_1-y_0+1)*(x_1-x_0+1);
				if (area == 0){
			//		cerr << "AREA ZERO!!";
					for (unsigned int _c = 0; _c < c; _c++)
						(*output_reciever.output_buffer)[y-current_output_y_coordinate][c*x+_c] = 0;
					continue;
				}

				for (unsigned int _c = 0; _c < c; _c++){
					long sum(0);
					for (int dy = y_0; dy <= y_1; dy++){
						for (int dx = x_0; dx <= x_1; dx++)
							sum+=lines[dy-current_bottom_of_buffer_in_input_y_coordinates()][c*dx+_c];
					}
					(*output_reciever.output_buffer)[y-current_output_y_coordinate][c*x+_c] = (ns_8_bit)(sum/area);
				}
			}
		}
		output_reciever.recieve_lines(*output_reciever.output_buffer,output_lines_to_produce);

	}
    long current_output_y_coordinate;
    long current_top_of_buffer_in_input_y_coordinates;
	long input_buffer_height;

	ns_vector_2d resize_factor;
	ns_vector_2i maximum_output_dimentions;
	ns_image_properties output_properties;

	unsigned int kernal_height,
			     kernal_radius;

	//used to store initial lines as kernal_height lines must be read in before processing can start.
	ns_image_stream_sliding_offset_buffer<ns_component> in_buffer;
	#ifdef NS_SPATIAL_AVG_TRACK_TIME
	unsigned long computation_time_spent,
				  output_time_spent;
	#endif

};

#endif
