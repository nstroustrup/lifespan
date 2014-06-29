#ifndef NS_IMAGE_TOOLS
#define NS_IMAGE_TOOLS

#include <vector>
#include <algorithm>
#include <errno.h>
#include <string>
#include <map>
#include <sys/stat.h>
#include <cmath>

//#include "ns_sql.h"
#include "ns_image.h"
//#include "ns_image_server.h"
#include "ns_dir.h"
#include "ns_image_easy_io.h"
#include "ns_vector_bitmap_interface.h"

#pragma warning(disable: 4355)

///Stores statistics used in successive rounds of adaptive image thresholding
struct ns_thresholding_region_stats{
	ns_64_bit brightness_sum[2];
	ns_64_bit count[2];
};

//Calculates statistics used in successive rounds of adaptive image thresholding
template<class ns_component, class ns_whole_image_type>
inline void thresholding_find_image_stats(ns_thresholding_region_stats & stats, const ns_component & threshold, const ns_whole_image_type & image){
	for (unsigned int i = 0; i < 2; i++){
			stats.count[i] = 0;
			stats.brightness_sum[i] = 0;
		}

	//ignore edges, because doing so makes code simpler and shouldn't dramatically effect the results
	for (unsigned int y = 1; y < image.properties().height-1; y++){
		for (unsigned int x = 1; x < image.properties().width-1; x++){
			if (image[y][x] == 0)  //0 is considered "not in the image" as a result of masking
				continue;
			//cout << (int)image[y][x] << " ";
			if (image[y][x] >= threshold){
				stats.brightness_sum[1]+= image[y][x];
				stats.count[1]++;
			}
			else{
				stats.brightness_sum[0]+= image[y][x];
				stats.count[0]++;
			}
		}
	}

}

///Determines the optimal threshold value that separates foreground from background objects.
///Chooses a random threshold to separate objects into two classes, calculates statistics about each class
///and then updates the threshold in a way that maximizes the differences between two classses.
///This algorithm isn't currently used as in practice it doesn't work as well as that implemented in ns_difference_thresholder.
template<class ns_component, class ns_whole_image_type>
ns_component find_adaptive_threshold(const ns_whole_image_type & image){
	
	ns_component current_threshold,
		previous_threshold(0);				 

	unsigned int max = (unsigned int)pow((float)2,(int)(8*sizeof(ns_component)));
	current_threshold= max/2;
	ns_thresholding_region_stats stats;

	unsigned int round = 0;
	//return 128;

	while (abs((long)current_threshold - (long)previous_threshold) > 2){
		
		//cerr << "Adaptive Threshold Round " << round <<": threshold = " << (int)current_threshold << "\n";
		thresholding_find_image_stats(stats, current_threshold, image);
		
		previous_threshold = current_threshold;
	//	cerr << "  Above: " << stats.count[1] << ", Below: " << stats.count[0] << "\n";
		//handle cases where one category is empty
		if (stats.count[1] == 0){
			if (current_threshold > 1)
				current_threshold=(ns_component)(current_threshold*.8);
			else return 1;
		}
		else if (stats.count[0] == 0){
			if (current_threshold < 255)
				current_threshold = (ns_component)(256 - (256 - current_threshold)*.8);
			else return 255;
		}
		else{
			double avg_b[2];
			avg_b[0] = (double)stats.brightness_sum[0]/stats.count[0];
			avg_b[1] = (double)stats.brightness_sum[1]/stats.count[1];
			current_threshold = (ns_component)(ns_component)(avg_b[0] + avg_b[1])/2;
		
		}
		
		round++;
		if (round > 10)
			throw ns_ex("find_adaptive_threshold::Clustering considered deadlocked after 10 rounds.");
	}
	return current_threshold;
}

///Converts the colorspace of an image from black and white to color, or vice-versa
template<class ns_component, class recieving_buffer_t>
class ns_image_stream_color_converter : public ns_image_stream_processor<ns_image_stream_color_converter<ns_component, recieving_buffer_t> >{
public:
	 typedef ns_component component_type;
	 typedef recieving_buffer_t storage_type;

	 ns_image_stream_color_converter(const long max_line_block_height):ns_image_stream_processor<ns_image_stream_color_converter<ns_component, recieving_buffer_t> >(max_line_block_height,this),just_pass_through(false),output_components(3){}

	void set_output_components(const unsigned int components){
		output_components = components;
	}
	template<class recieving_t>
	void prepare_to_recieve_image(const ns_image_properties & properties, recieving_t & output_reciever){
		ns_image_stream_processor<ns_image_stream_color_converter<ns_component, recieving_buffer_t> >::_properties = properties;
		init(properties);
		
		//prepare reciever to recieve a color image.
		ns_image_properties color_prop = properties;
		color_prop.components = output_components;
		output_reciever.prepare_to_recieve_image(color_prop);
	}

	bool init(const ns_image_properties & properties){
		if (properties.components == 3 && output_components == 3)
			throw ns_ex("ns_image_stream_color_converter: Feeding a color image into a B&W->Color converter!");
		if (properties.components == 1 && output_components == 1)
			throw ns_ex("ns_image_stream_color_converter: Feeding a b&w image into a Color->B&W converter!");
		//if one is translating color to color, don't be silly; just pass it straight through!
		if (properties.components == output_components)
			just_pass_through = true;
		else{
			ns_image_stream_buffer_properties bufp;
			bufp.height = ns_image_stream_processor<ns_image_stream_color_converter<ns_component, recieving_buffer_t> >::_max_line_block_height;
			bufp.width = properties.width*properties.components;
			in_buffer.resize(bufp);
		}
		return true;
	}

	template<class reciever_t>
	recieving_buffer_t * provide_buffer(const ns_image_stream_buffer_properties & p, reciever_t & output_reciever){
		//have data written in after current data
		if (just_pass_through)
			return output_reciever.provide_buffer(p);
		else return &in_buffer;
	}

	template<class reciever_t>
	void recieve_and_send_lines(const recieving_buffer_t & lines, const unsigned long height, reciever_t & output_reciever){
		if (just_pass_through){
			output_reciever.recieve_lines(lines,height);
			return;
		}

		ns_image_stream_buffer_properties bufp(ns_image_stream_processor<ns_image_stream_color_converter<ns_component, recieving_buffer_t> >::_properties.width*
											  output_components,height);
		recieving_buffer_t * outbuf = output_reciever.provide_buffer(bufp);

		if (output_components == 3){
			//translating B&W -> color
			for (unsigned int y = 0; y < height; y++){
				for (unsigned int x = 0; x < ns_image_stream_processor<ns_image_stream_color_converter<ns_component, recieving_buffer_t> >::_properties.width; x++){
					(*outbuf)[y][3*x] = in_buffer[y][x];
					(*outbuf)[y][3*x+1] = in_buffer[y][x];
					(*outbuf)[y][3*x+2] = in_buffer[y][x];
				}
			}
		}
		else{
			//translating color -> B&W
			ns_component c;
			for (unsigned int y = 0; y < height; y++){
				for (unsigned int x = 0; x < ns_image_stream_processor<ns_image_stream_color_converter<ns_component, recieving_buffer_t> >::_properties.width; x++){
					c = (ns_component)((long)in_buffer[y][3*x]+(long)in_buffer[y][3*x+1]+(long)in_buffer[y][3*x+2])/3;
		//			if (c != 0)
		//				cerr << (int)in_buffer[y][3*x] << "*" << (int)in_buffer[y][3*x+1] << "*" << (int)in_buffer[y][3*x+2]<< "=" << (int)c << "\n";
					(*outbuf)[y][x] = c;
				}
			}
		}

		output_reciever.recieve_lines(*outbuf,height);

	}
	template<class recieve_t>
	void finish_recieving_image(recieve_t & output_reciever){
		output_reciever.finish_recieving_image();
	}

	bool just_pass_through;
	unsigned int output_components;
	recieving_buffer_t in_buffer;
};

/*
template<class read_buffer, class write_buffer>
class ns_image_stream_buffer_converter : public ns_image_stream_processor<read_buffer, write_buffer>{
public:
	 ns_image_stream_buffer_converter(const long max_line_block_height):ns_image_stream_processor<read_buffer, write_buffer>(max_line_block_height){}

	void init(const ns_image_properties & properties){
		ns_image_stream_buffer_properties bufp;
		bufp.height = _max_line_block_height;
		bufp.width = properties.width*properties.components;
		in_buf.resize(bufp);
	}
	
	read_buffer * provide_buffer(const ns_image_stream_buffer_properties & p){
		return &in_buf;
	}
	
	void recieve_lines(const read_buffer & lines, const unsigned long height){
		ns_image_stream_buffer_properties bufp;
		bufp.height = _max_line_block_height;
		bufp.width = _properties.width*_properties.components;
		write_buffer * buf = output_reciever->provide_buffer(bufp);
		for (unsigned int y = 0; y < height; y++)
			for (unsigned int x = 0; x < bufp.width; x++)
				(*buf)[y][x] = in_buf[y][x];
		output_reciever->recieve_lines(*buf,height);
	}

	void finish_recieving_image(){output_reciever->finish_recieving_image();}

private:
	read_buffer in_buf;
};
*/

///Recieves an image from an ns_image_stream_sender and
///and outputs it to two separate ns_image_stream_recievers.  
///Implements a T junction in a pipeline.
template<class ns_component, class reciever1_storage_1,class reciever1_t, class reciever2_t>
class ns_image_stream_splitter : public ns_image_stream_reciever<reciever1_storage_1>{
public:
	ns_image_stream_splitter(const long max_line_block_height):ns_image_stream_reciever<reciever1_storage_1>(max_line_block_height,this),
		output_1(0),output_2(0){}

	void bind(reciever1_t & b1, reciever2_t & b2){

		output_1 = &b1;
		output_2 = &b2;
	}

	bool init(const ns_image_properties & properties){		
		if (output_1 == 0 || output_2 == 0)
			throw ns_ex("ns_image_stream_splitter: You must bind output before initialization.");
		ns_image_stream_buffer_properties bufp;
		bufp.height = ns_image_stream_reciever<reciever1_storage_1>::_max_line_block_height;
		bufp.width = properties.width*properties.components;
		output_1->prepare_to_recieve_image(properties);
		output_2->prepare_to_recieve_image(properties);
		return true;
	}
	
	reciever1_storage_1 * provide_buffer(const ns_image_stream_buffer_properties & p){
		output_1->output_buffer = output_1->provide_buffer(p);
		return output_1->output_buffer;
	}
	
	void recieve_lines(const reciever1_storage_1 & lines, const unsigned long height){
	
		ns_image_stream_buffer_properties bufp;
		bufp.height = ns_image_stream_reciever<reciever1_storage_1>::_properties.width*ns_image_stream_reciever<reciever1_storage_1>::_properties.components;
		bufp.width = height;
		output_2->output_buffer = output_2->provide_buffer(bufp);

		for (unsigned int y = 0; y < height; y++)
			for (unsigned int x = 0; x < bufp.height; x++)
				(*output_2->output_buffer)[y][x] = (*output_1->output_buffer)[y][x];
			
		output_1->recieve_lines(*output_1->output_buffer,height);
		output_2->recieve_lines(*output_2->output_buffer,height);

	}

	void finish_recieving_image(){
		output_1->finish_recieving_image();
		output_2->finish_recieving_image();
	}

private:
	reciever1_t* output_1;
	reciever2_t* output_2;
};


///Using the specified pixel value, outputs a thresholded copy of the image.
template<class ns_component>
class ns_image_stream_apply_threshold : public ns_image_stream_processor<ns_image_stream_apply_threshold<ns_component> >{
public:				
	typedef ns_image_stream_static_buffer<ns_component> storage_type;
	typedef ns_component component_type;

	ns_image_stream_apply_threshold (const long max_line_block_height):
	  ns_image_stream_processor<ns_image_stream_apply_threshold<ns_component> >(max_line_block_height,this),threshold(0){
	  }
	  void set_threshold(const ns_component t){
		threshold = t;
	  }
	  
	 template<class recieving_t>
	 void prepare_to_recieve_image(const ns_image_properties & properties, recieving_t & reciever){
		this->default_prepare_to_recieve_image(properties,reciever);
	 }
	
   
	 bool init(const ns_image_properties & properties){
		 if (properties.width == 0 && properties.height == 0){
			in_buffer.resize(ns_image_stream_buffer_properties(0,0));
			return false;
		}
		ns_image_stream_buffer_properties bufp;
		bufp.width = properties.width*properties.components;
		bufp.height = ns_image_stream_processor<ns_image_stream_apply_threshold<ns_component> >::_max_line_block_height;
		return in_buffer.resize(bufp);
	}
	template<class recieve_t>
	ns_image_stream_static_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & p, recieve_t & output_reciever){
		return &in_buffer;
	}
	
	template<class recieve_t>
	void recieve_and_send_lines(const ns_image_stream_static_buffer<ns_component> & lines, const unsigned long height, recieve_t & output_reciever){
		ns_image_stream_buffer_properties bufp;
		bufp.width = ns_image_stream_processor<ns_image_stream_apply_threshold<ns_component> >::_properties.width*
						ns_image_stream_processor<ns_image_stream_apply_threshold<ns_component> >::_properties.components;
		bufp.height = height;

		output_reciever.output_buffer = output_reciever.provide_buffer(bufp);
		for (unsigned int y = 0; y < height; y++)
			for (unsigned int x = 0; x < bufp.width; x+=ns_image_stream_processor<ns_image_stream_apply_threshold<ns_component> >::_properties.components){
			//	if (lines[y][x] != 0)
				//	cerr << (int) lines[y][x] << " ";
				(*output_reciever.output_buffer)[y][x] = (ns_component)255*(lines[y][x] >= threshold);
			}

		output_reciever.recieve_lines(*output_reciever.output_buffer,height);
	}
	template<class recieve_t>
	void finish_recieving_image(recieve_t & output_reciever){
		output_reciever.finish_recieving_image();
	}

private:


	//used to store initial lines as kernal_height lines must be read in before processing can start.
	ns_image_stream_static_buffer<ns_component> in_buffer;
	ns_component threshold;
};

///Provided with an image's histogram, stretches the intensity values to occupy the images total dynamic range
///A certain percentage of pixels can be saturated out, as specified by the loss parameter.
template<class whole_image_type, class histogram_t>
void stretch_levels(whole_image_type & image, const histogram_t & histogram, const float loss){
	unsigned int min = 1000000;
	unsigned int max = 0;
	unsigned int sum = 0;
	const float max_sum(((float)1.0-loss)* (histogram.number_of_pixels()-histogram[0]));
	for (unsigned int i = 1; i < histogram.size(); i++){
		if (histogram[i] != 0 && min > i)
			min = i;

		sum+=histogram[i];
		if (sum >= max_sum){
			max = i;
			break;
		}
	}
	if (max == 0)
		max = 255;
	#pragma warning(disable: 4244)  //certain template arguments will generate floating point approximation errors.
								//this cant be fixed without adding extra template specifications, which would be a pain.
								//So, we just kill the warning.
	
	if (max == min){
		//we have a bitmap
		for (unsigned int y = 0; y < image.properties().height; y++){
			for (unsigned int x = 0; x < image.properties().width*image.properties().components; x++){
				//if (image[y][x] >= max)
				//	image[y][x] = 255;
				//else 
			image[y][x] = (typename whole_image_type::component_type)(255*(image[y][x] == max));
			}
		}
		return;
	}
	float f = (histogram.size()-1)/((float)(max-min));
	for (unsigned int y = 0; y < image.properties().height; y++){
		for (unsigned int x = 0; x < image.properties().width*image.properties().components; x++){
			//if (image[y][x] >= max)
			//	image[y][x] = 255;
			//else 
			image[y][x] = (typename whole_image_type::component_type)(f*(image[y][x] - min)+1);
		}
	}
	#pragma warning(default: 4244)
}

///information detailing the tile size of a collage of images, allowing
///sub-images to be extracted from the image collage.
struct ns_collage_info{
	unsigned int tiles_per_row,
				 tile_width,
				 tile_height;

	void image_locations_in_collage(const unsigned long number_of_objects,std::vector<ns_vector_2i> & positions) const{
		if (tile_height == 0)
			throw ns_ex("ns_extract_images_from_collage::Collage_extraction requested for images with tile_height = 0");
		if (tile_width == 0)
			throw ns_ex("ns_extract_images_from_collage::Collage_extraction requested for images with tile_width = 0");
		if (tiles_per_row == 0)
			throw ns_ex("ns_extract_images_from_collage::Collage_extraction requested for images with tiles_per_row = 0");
		positions.resize(number_of_objects);
		for (unsigned int i = 0; i < number_of_objects; i++){
			positions[i].x = (i%tiles_per_row)*tile_width;
			positions[i].y = (i/tiles_per_row)*tile_height;
		}

}
};


template<class whole_image>
void ns_extract_images_from_collage(const ns_collage_info &info, const whole_image & image, std::vector<whole_image *> & out){
	if (info.tile_height == 0)
		throw ns_ex("ns_extract_images_from_collage::Collage_extraction requested for images with 0 height!");
	unsigned int number_of_images = (image.properties().height/info.tile_height)*info.tiles_per_row;
	unsigned int left_over_lines = image.properties().height%info.tile_height;
	if (left_over_lines != 0)
		throw ns_ex("ns_extract_images_from_collage::Collage extraction requested for image with invalid height (") << image.properties().height << "vs" << info.tile_height << ")";
	if (image.properties().width/info.tile_width != info.tiles_per_row || 
		image.properties().width%info.tile_width != 0)
		throw ns_ex("ns_extract_images_from_collage::Collage extraction requested for image with invalid width");
	if (info.tile_height == 1 && info.tile_width == 1 && info.tiles_per_row == 1 && image.properties().height == 1){
		out.resize(0);
		return;
	}
	out.resize(number_of_images,0);
	for (unsigned int i = 0; i < number_of_images; i++)
		out[i] = new whole_image;
	try{
		for (unsigned int i = 0; i < number_of_images; i++){
			out[i]->prepare_to_recieve_image(ns_image_properties(info.tile_height,info.tile_width,image.properties().components));
			const unsigned int row(i/info.tiles_per_row),
				col(i%info.tiles_per_row),
				c(image.properties().components);
			for (unsigned int y = 0; y < info.tile_height; y++)
				for (unsigned int x = 0; x < info.tile_width*c; x++)
					(*out[i])[y][x] = image[row*info.tile_height+y][c*col*info.tile_width+x];
		}
	}
	catch(...){
		
		for (unsigned int i = 0; i < number_of_images; i++){
			if (out[i] != 0){
				delete out[i];
				out[i] = 0;
			}
		}
		throw;
	}
}

template<class T1>
class ns_collage_image_sort_mixed{
public:
	//returns true if a is less than b.
	bool operator()(const ns_image_whole<T1> * a, const ns_image_whole<T1> * b){
		return (a->properties().height < b->properties().height) || ((a->properties().height >= b->properties().height) && (a->properties().width < b->properties().width));
	}
};

template<class T1>
class ns_collage_image_sort_height{
public:
	//returns true if a is less than b.
	bool operator()(const ns_image_whole<T1> * a, const ns_image_whole<T1> * b){
		return (a->properties().height < b->properties().height);
	}
};
template<class T1>
class ns_collage_image_sort_width{
public:
	//returns true if a is less than b.
	bool operator()(const ns_image_whole<T1> * a, const ns_image_whole<T1> * b){
		return (a->properties().width < b->properties().width);
	}
};

struct ns_packed_collage_position{
	ns_packed_collage_position(){}
	ns_packed_collage_position(const ns_vector_2i & p, const ns_vector_2i & s, const ns_vector_2i & bp):pos(p),size(s),original_bitmap_position(bp){}
	ns_vector_2i pos,size;
	ns_vector_2i original_bitmap_position;//position of the image in the source bitmap; ie if the object is the worm, it's
										  //original coordinates
};

template<class T1, class T2>
std::vector<ns_packed_collage_position >
ns_pack_in_images(const std::vector<const ns_image_whole<T1> *> & images, 
					   ns_image_whole<T2> & reciever,
					   const unsigned long output_buffer_height,
					   const unsigned long background_val=0, 
					   const unsigned long desired_width = 1200){
   
    unsigned long output_width = desired_width*images[0]->properties().components;
	//make sure all images fit
	for (unsigned int i = 0; i < images.size(); i++)
		if (images[i]->properties().components*images[i]->properties().width > output_width)
			output_width = images[i]->properties().components*images[i]->properties().width;
	
	std::vector<ns_packed_collage_position> positions;
	unsigned long total_height = 0,
				  total_width = 0;

	ns_packed_collage_position cur(ns_vector_2i(0,0),ns_vector_2i(0,0),ns_vector_2i(0,0));
	unsigned long cur_row_height=0;
	//find image locations
	for (unsigned int i = 0; i < images.size(); ){
		//we hit the end of a row
		if (cur.pos.x + images[i]->properties().components*images[i]->properties().width > output_width){
			
			if ((unsigned int)cur.pos.x > total_width)
				total_width = cur.pos.x;

			cur.pos.x=0;
			cur.pos.y+=cur_row_height;
			
			total_height += cur_row_height;
			cur_row_height = 0;
			continue;
		}

		if (cur_row_height < images[i]->properties().height)
			cur_row_height = images[i]->properties().height;
		cur.size = ns_vector_2i(images[i]->properties().width,images[i]->properties().height);
		positions.push_back(cur);
		cur.pos.x+=images[i]->properties().components*images[i]->properties().width;
		i++;
	}
	if ((unsigned int)cur.pos.x > total_width)
		total_width = cur.pos.x;
	total_height += cur_row_height;


	ns_image_properties prop(images[0]->properties());
	prop.width = total_width/images[0]->properties().components;
	prop.height = total_height;
	reciever.prepare_to_recieve_image(prop);
	
	for (unsigned y = 0; y < prop.height; y++)
		for (unsigned x = 0; x < prop.width*prop.components; x++)
			reciever[y][x] = 0;//(T2)background_val;
	
	for (unsigned int i = 0; i < images.size(); i++){
		for (unsigned y = 0; y < images[i]->properties().height; y++)
			for (unsigned x = 0; x < images[i]->properties().width*images[i]->properties().components; x++)
				reciever[y+positions[i].pos.y][x+positions[i].pos.x] = (*images[i])[y][x];

	}
	for (unsigned int i = 0; i < positions.size(); i++)
		positions[i].pos.x/=prop.components;
	return positions;
}

///takes a collection of images and juxtaposes all of them in a collage.
//higher aspect ratio makes taller images
template<class T1, class T2>
ns_collage_info ns_compile_collage(const std::vector<const ns_image_whole<T1> *> & images, 
							 T2 & reciever, 
							 const unsigned long output_buffer_height, 
							 const unsigned long scaling_factor=1, 
							 const unsigned long background_val=0, 
							 const bool resize_if_necissary = false, 
							 const float aspect_ratio = 1){
	if (images.size() == 0){
		//if there are no images, return a 1 pixel by 1 pixel black image.
		ns_image_properties outprops(1,1,1,72);
		reciever.prepare_to_recieve_image(outprops);
		ns_image_stream_buffer_properties bufprops(1,1);
		reciever.output_buffer = reciever.provide_buffer(bufprops);
		#pragma warning(disable: 4800 4244)
		(*reciever.output_buffer)[0][0] = background_val;
		#pragma warning(default: 4800 4244)
		reciever.recieve_lines(*reciever.output_buffer,1);
		reciever.finish_recieving_image();
		ns_collage_info null;
		null.tile_height = 1;
		null.tile_width = 1;
		null.tiles_per_row = 1;
		return null;
	}

	//find the size of each block, (largest width x largest height)
	unsigned long block_width=0;
	unsigned long block_height=0;

	for (unsigned int i = 0; i < images.size(); i++){
		if (images[i]->properties().width > block_width)
			block_width = images[i]->properties().width;
		if (images[i]->properties().height > block_height)
			block_height = images[i]->properties().height;
	}


	unsigned long shrink_factor = 1;	
	unsigned long number_blocks_y = (unsigned long)(((float)sqrt((float)images.size()))*aspect_ratio);
	if (number_blocks_y > images.size())
		number_blocks_y = (unsigned long)images.size();
	if (number_blocks_y == 0)
		number_blocks_y = 1;
	unsigned long number_blocks_x = (unsigned long)ceil(((float)images.size())/(float)number_blocks_y);
	if (number_blocks_x == 0)
		number_blocks_x = 1;
	//cerr << "blocks: " << number_blocks_x << "/" << number_blocks_y << "\n";
	unsigned int total_image_area = number_blocks_x*block_width * number_blocks_y*block_height;

	if (resize_if_necissary && total_image_area > NS_IMAGE_WHOLE_MAXIMUM_AREA){
		shrink_factor = (unsigned int)ceil((1.0*total_image_area) / (1.0*NS_IMAGE_WHOLE_MAXIMUM_AREA));

		block_height /= shrink_factor;
		block_width /= shrink_factor;
	//	cerr << "ns_make_collage::Enormous collage requested, shrinking by a factor of " << shrink_factor << "\n";
	}

	block_width++;  //draw a one pixel border around objects.
	block_height++;

	ns_image_properties outprops;
	outprops.width = number_blocks_x*block_width;
	outprops.height = number_blocks_y*block_height;
	outprops.components = 1;
	for (unsigned int i = 0; i < images.size(); i++)
		if (images[i]->properties().components == 3){
			outprops.components = 3;
			break;
		}
	outprops.resolution = images[0]->properties().resolution;
	reciever.prepare_to_recieve_image(outprops);
	ns_image_stream_buffer_properties bufprops;
	unsigned char _c(outprops.components);

	for (unsigned long y = 0; y < outprops.height;){
		unsigned long lines_to_send = outprops.height - y; 
		if (lines_to_send > output_buffer_height)
			lines_to_send = output_buffer_height;
		bufprops.width = outprops.width*outprops.components;
		bufprops.height = lines_to_send;
		reciever.output_buffer = reciever.provide_buffer(bufprops);
		for (unsigned long _y = 0; _y < lines_to_send; _y++)

			//write data to the buffer
			for (unsigned int x = 0; x < outprops.width; x++){
				unsigned long image_id = ((y+_y)/block_height)*number_blocks_x + x/block_width;

				if (image_id >= images.size()){  //if you have run out of images, just output black.
					for (unsigned int c = 0; c < _c; c++){
						#pragma warning(disable: 4800 4244)
							(*reciever.output_buffer)[_y][outprops.components*x+c] = background_val;
						#pragma warning(default: 4800 4244)
						//if (c == 2)
						//	(*buf)[_y][outprops.components*x+c] = 255;
					}
					continue;
				}
			
				unsigned long region_coord[2];
				region_coord[0] = (x		 % (block_width) )*shrink_factor;
				region_coord[1] = ((y+_y) % (block_height))*shrink_factor;	
				
				//if you are no longer outputtig an image
				if (region_coord[0] >= images[image_id]->properties().width ||
					region_coord[1] >= images[image_id]->properties().height )
				{
					for (unsigned int c = 0; c < (unsigned)_c; c++){
						#pragma warning(disable: 4800 4244)
						(*reciever.output_buffer)[_y][x*outprops.components+c] = background_val;
						#pragma warning(default: 4800 4244)
						//if (c == 2)
						//	(*buf)[_y][outprops.components*x+c] = 255;
					}
					continue;
				}
			//	cerr << "reg: " << region_id << "\n";
				
				#pragma warning(disable: 4800 4244)  //certain template arguments will generate floating point approximation errors.
								//this cant be fixed without adding extra template specifications, which would be a pain.
								//So, we just kill the warning.
				const ns_image_whole<T1>  * cur_image = images[image_id];
				int w = cur_image->properties().width;
				int h =  cur_image->properties().height;
				if (_c == 3 && cur_image->properties().components == 1){
					for (unsigned int c = 0; c < outprops.components; c++){
						int r_x = region_coord[0];
						int r_y = region_coord[1];
						//cerr << "Accessing " << r_x << "," << r_y <<"\n";
						(*reciever.output_buffer)[_y][x*outprops.components+c] = ((unsigned long)(*cur_image)[r_y][r_x]);
					}

				}
				else{
					for (unsigned int c = 0; c < outprops.components; c++){
						int r_x = region_coord[0]*outprops.components+ c;
						int r_y = region_coord[1];
						//cerr << "Accessing " << r_x << "," << r_y <<"\n";
						(*reciever.output_buffer)[_y][x*outprops.components+c] = ((unsigned long)(*cur_image)[r_y][r_x]);
					}
				}
				#pragma warning(default: 4800 4244)
			}
		//cerr <<  y << "\n";
		reciever.recieve_lines(*reciever.output_buffer,lines_to_send);
		y+=lines_to_send;
	}
	reciever.finish_recieving_image();

	ns_collage_info info;
	info.tiles_per_row = number_blocks_x;
	info.tile_width = block_width;
	info.tile_height = block_height;
	return info;
		
}

template<class T1, class T2>
ns_collage_info ns_make_collage(const std::vector<const ns_image_whole<T1> *> & images, 
							 T2 & reciever, 
							 const unsigned long output_buffer_height, 
							 const unsigned long scaling_factor=1, 
							 const unsigned long background_val=0, 
							 const bool resize_if_necissary = false, 
							 const float aspect_ratio = 1){
	return ns_compile_collage(images,reciever,output_buffer_height,scaling_factor,background_val,resize_if_necissary,aspect_ratio);
}

template<class T1, class T2>
ns_collage_info ns_make_collage(const std::vector<ns_image_whole<T1> *> & images, T2 & reciever, const unsigned long output_buffer_height, const unsigned long scaling_factor=1, const unsigned long background_val=0, const bool resize_if_necissary = false, const float aspect_ratio = 1){
	std::vector<const ns_image_whole<T1> * > im(images.size());
	for (unsigned int i = 0; i < images.size(); i++)
		im[i] = images[i];
	return ns_compile_collage<T1,T2>(im,reciever,output_buffer_height,scaling_factor,background_val,resize_if_necissary,aspect_ratio);
}
template<class T1, class T2>
ns_collage_info ns_make_collage(const std::vector<const ns_image_whole<T1> > & images, T2 & reciever, const unsigned long output_buffer_height, const unsigned long scaling_factor=1, const unsigned long background_val=0, const bool resize_if_necissary = false, const float aspect_ratio = 1){
	std::vector<const ns_image_whole<T1> * > im(images.size());
	for (unsigned int i = 0; i < images.size(); i++)
		im[i] = &images[i];
	return ns_compile_collage<T1,T2>(im,reciever,output_buffer_height,scaling_factor,background_val,resize_if_necissary,aspect_ratio);
}

template<class T1, class T2>
ns_collage_info ns_make_collage(const std::vector<ns_image_whole<T1> > & images, T2 & reciever, const unsigned long output_buffer_height, const unsigned long scaling_factor=1, const unsigned long background_val=0, const bool resize_if_necissary = false, const float aspect_ratio = 1){
	std::vector<const ns_image_whole<T1> *> im(images.size());
	for (unsigned int i = 0; i < images.size(); i++)
		im[i] = &images[i];
	return ns_compile_collage<T1,T2>(im,reciever,output_buffer_height,scaling_factor,background_val,resize_if_necissary,aspect_ratio);
}

//const std::vector<const ns_image_whole<T1> *> & images, T2 & reciever,
			

template<class T1, class T2>
std::vector<ns_packed_collage_position>
ns_make_packed_collage(const std::vector<const ns_image_whole<T1> *> & images, 
							 ns_image_whole<T2> & reciever, 
							 const unsigned long background_val=0, 
							 const unsigned long desired_width=1200,
							 const bool sort_for_compression=true){
	if (images.size() == 0){
		//if there are no images, return a 1 pixel by 1 pixel black image.
		ns_image_properties outprops(1,1,1);
		reciever.prepare_to_recieve_image(outprops);
		ns_image_stream_buffer_properties bufprops(1,1);
		reciever.output_buffer = reciever.provide_buffer(bufprops);
		(*reciever.output_buffer)[0][0] = (T2)background_val;
		return std::vector<ns_packed_collage_position>();
	}
	std::vector<const ns_image_whole<T1> *> skinny;
	std::vector<const ns_image_whole<T1> *> fat;
	std::vector<const ns_image_whole<T1> *> square;
	std::vector<std::vector<const ns_image_whole<T1> *> > groups;
	std::vector< unsigned long> group_aspect_ratios;
	if (sort_for_compression){
		for (unsigned int i = 0; i < images.size(); i++){
			if (images[i]->properties().height == 0) 
				throw ns_ex("ns_make_packed_collage::Passed an image with height zero!");
			float f = (float)images[i]->properties().width/(float)images[i]->properties().height;
			if (f <= 0.67) skinny.push_back(images[i]);
			else if (f >= 1.5) fat.push_back(images[i]);
			else square.push_back(images[i]);
		}
		//cerr << skinny.size() << " skinny, " << fat.size() << " fat, " << square.size() << ", square.\n";
		//sort all in ascending dimensions
		sort(skinny.begin(),skinny.end(),ns_collage_image_sort_height<T1>());
		sort(fat.begin(),fat.end(),ns_collage_image_sort_width<T1>());
		sort(square.begin(),square.end(),ns_collage_image_sort_width<T1>());
	//	cerr << "sk: " << skinny.size() << " fat: " << fat.size() << " square: " << square.size() <<"\n";
	}
	else square.assign(images.begin(),images.end());

	//look for the largest image in east dimention
	unsigned long max_width=0,
				  max_height=0;
	for (unsigned int i = 0; i < images.size(); i++){
		if (images[i]->properties().width > max_width)
			max_width = images[i]->properties().width;
		if (images[i]->properties().height > max_height)
			max_height = images[i]->properties().height;
	}
	//cerr << max_width;
	//cerr << max_height;
	//group skinny objects correctly
	unsigned long start = 0,
				  cumulative = 0;
	for (unsigned int i = 0; i < skinny.size(); i++){
		cumulative+=skinny[i]->properties().width;
		if (cumulative > max_width){
			const unsigned long s = (unsigned long)groups.size();
			groups.resize(s+1);
			for (unsigned int j = start; j < i; j++){
				groups[s].push_back(skinny[j]);
				//cerr << "s,";
			}
			group_aspect_ratios.push_back(10000000);
			//cerr << group_aspect_ratios[s];
			start = i;
			cumulative = skinny[i]->properties().width;
		}
	}
	if (start < skinny.size()){
		const unsigned long s = (unsigned long)groups.size();
		groups.resize(s+1);
		for (unsigned int j = start; j < skinny.size(); j++){
			groups[s].push_back(skinny[j]);
		//	cerr << "s.";
		}
		group_aspect_ratios.push_back(100000);
		//cerr << group_aspect_ratios[s];
	}

	start = 0;
	cumulative = 0;
	for (unsigned int i = 0; i < fat.size(); i++){
		cumulative+=fat[i]->properties().height;
		if (cumulative > max_height){
			unsigned long s = (unsigned long)groups.size();
			groups.resize(s+1);
			for (unsigned int j = start; j < i; j++){
				groups[s].push_back(fat[j]);
				//cerr << "f,";
			}
			group_aspect_ratios.push_back(0);
			//cerr << group_aspect_ratios[s];
			start = i;
			cumulative = fat[i]->properties().height;
		}
	}
	if (start < fat.size()){
		const unsigned long s = (unsigned long)groups.size();
		groups.resize(s+1);
		for (unsigned int j = start; j < fat.size(); j++){
			groups[s].push_back(fat[j]);
			//cerr << "f.";
		}
		group_aspect_ratios.push_back(0);
		//cerr << group_aspect_ratios[s];
	}
	
	unsigned long cumulative_width = 0,
				  cumulative_height = 0;
	start = 0;
	for (unsigned int i = 0; i < square.size(); i++){
		cumulative_width+=square[i]->properties().width;
		cumulative_height+=square[i]->properties().height;
		if (cumulative_width > max_width || cumulative_height > max_height){
			unsigned long s = (unsigned long)groups.size();
			groups.resize(s+1);
			group_aspect_ratios.push_back(800);
			for (unsigned int j = start; j < i; j++){
				groups[s].push_back(square[j]);
			//	cerr << "q,";
			}
			//cerr << group_aspect_ratios[s];
			start = i;
			cumulative_width = square[i]->properties().width;
			cumulative_height = square[i]->properties().height;
		}
	}
	if (start < square.size()){
		const unsigned long s = (unsigned long)groups.size();
		groups.resize(s+1);
		group_aspect_ratios.push_back(800);
		for (unsigned int j = start; j < square.size(); j++){
				groups[s].push_back(square[j]);
			//	cerr << "q.";
		}
		//cerr << group_aspect_ratios[s];
	}
	//cerr << "\n";
	std::vector<std::vector<ns_packed_collage_position> >image_positions_within_group;
	std::vector<ns_image_whole<T1> > group_images(groups.size());
	std::vector<const ns_image_whole<T1> *> group_images_p(groups.size());
	for (unsigned int i = 0; i < groups.size(); i++){
		image_positions_within_group.push_back(ns_pack_in_images(groups[i],group_images[i], 512,background_val,desired_width));
		//cerr << "Group " << i << " has " << groups[i].size() << " items, pack returned " << image_positions_within_group[image_positions_within_group.size()-1].size() << " items.\n";
	}

	for (unsigned int i = 0; i < group_images.size(); i++)
		group_images_p[i] = &group_images[i];
	std::vector<ns_packed_collage_position> group_positions(ns_pack_in_images(group_images_p,reciever,512,background_val,desired_width));
	//cerr << group_images.size() << " groups added to collage; pack returned " << group_positions.size() << "\n";

	//now we need to link the images to their position in the packed collage.
	//to do this we need to add the image's position within each group collage
	//to the group's position in the overall collage
	if (group_positions.size() != group_images.size())
		throw ns_ex("ns_make_packed_collage::Incorrect number of positions returned by ns_pack_in_images()!");
	
	//go through and link up the images to their group id;
	std::vector<ns_vector_2i> image_group_memberships(images.size()); //(group id, index in group)
	for (unsigned int i = 0; i < images.size(); i++){
		bool found = false;
		for (unsigned int g = 0; g < groups.size(); g++){
			//cerr << "groups[" << g << "].size() == " << groups[g].size() << "\n";
			for (unsigned int j = 0; !found && j < groups[g].size() ; j++){
			//	cerr << "images[" << i << "]" << images[i] << " v groups[" << g << "][" << j << "]"  << groups[g][j] <<"\n";
				if (images[i] == groups[g][j]){
			//		cerr << "Yes!";
					image_group_memberships[i].x =g;
					image_group_memberships[i].y = j;
					found = true;
					break;
				}
			}
		}
		if (!found)
				throw ns_ex("ns_make_packed_collage::Could not match up image:") << i;
	}
	
	std::vector<ns_packed_collage_position> absolute_image_positions(images.size());
	for(unsigned int i = 0; i < images.size(); i++){
		absolute_image_positions[i].size = ns_vector_2i(images[i]->properties().width,images[i]->properties().height);
		//cerr << "Image " << i << "\n";
		//cerr << "\tgroup " << image_group_memberships[i].x << ":" << group_positions[ image_group_memberships[i].x].pos << "\n";
		//cerr << "\tgroup " << image_group_memberships[i].x << "[" << image_group_memberships[i].y << "]:" << image_positions_within_group[ image_group_memberships[i].x][image_group_memberships[i].y].pos << "\n";
		absolute_image_positions[i].pos = group_positions[ image_group_memberships[i].x].pos + image_positions_within_group[ image_group_memberships[i].x][image_group_memberships[i].y].pos;
	}
	return absolute_image_positions;
}


template<class T1, class T2>
std::vector<ns_packed_collage_position>
ns_make_packed_collage(const std::vector<const ns_image_whole<T1> > & images, ns_image_whole<T2> & reciever, const unsigned long background_val=0, const unsigned int desired_width=800, const bool sort_for_compression=true){
	std::vector<const ns_image_whole<T1> * > im(images.size());
	for (unsigned int i = 0; i < images.size(); i++)
		im[i] = &images[i];
	return ns_make_packed_collage(im,reciever,background_val,desired_width,sort_for_compression);
}
template<class T1, class T2>
std::vector<ns_packed_collage_position>
ns_make_packed_collage(const std::vector<ns_image_whole<T1> > & images, ns_image_whole<T2> & reciever, const unsigned long background_val=0, const unsigned int desired_width=800,const bool sort_for_compression=true){
	std::vector<const ns_image_whole<T1> *> im(images.size());
	for (unsigned int i = 0; i < images.size(); i++)
		im[i] = &images[i];
	return ns_make_packed_collage(im,reciever,background_val,desired_width,sort_for_compression);
}


///Collages can be made from tree structures of images, allowing fine control
///over the relative position of images in the collage.
template<class T1>
struct ns_collage_tree_node{
	ns_collage_tree_node():image(0){}
	ns_collage_tree_node(const ns_image_whole<T1> & image):image(&image){}
	const ns_image_whole<T1> * image;

	void add(const ns_image_whole<T1> & image){images.push_back(ns_collage_tree_node(image));}
	void add(const ns_collage_tree_node & tree_node){images.push_back(tree_node);}

	std::vector <ns_collage_tree_node> images;

};

///takes a collection of images and juxtaposes all of them in a collage, based on the tree structure specified by root.
template<class T1, class T2>
ns_collage_info ns_make_collage(const ns_collage_tree_node<T1> & root, T2 & reciever, const unsigned long output_buffer_height,const unsigned long_scaling_factor=1, const unsigned long background_val=0, const bool resize_if_necissary = false){
	if (root.image != 0){
		root.image->pump(reciever,output_buffer_height);
		ns_collage_info info;
		info.tiles_per_row = 1;
		info.tile_width = root.image->properties().width;
		info.tile_height = root.image->properties().height;
		return info;
	}
	if (root.images.size() == 0)
		throw ns_ex("make_collage_from_tree::Empty collage tree node encountered.");
	if (root.images.size() == 1)
		return ns_make_collage(root.images[0],reciever,output_buffer_height,long_scaling_factor,background_val,resize_if_necissary);

	std::vector<ns_image_whole<T1> > compound_images(root.images.size());
	for (unsigned int i = 0; i < root.images.size(); i++)
		ns_make_collage(root.images[i],compound_images[i],output_buffer_height,long_scaling_factor,background_val,resize_if_necissary);
	std::vector<const ns_image_whole<T1> *> im(compound_images.size());
	for (unsigned int i = 0; i < im.size(); i++)
		im[i] = &compound_images[i];
	return ns_compile_collage(im,reciever,output_buffer_height,long_scaling_factor,background_val,resize_if_necissary);
}

///takes a collection of images and juxtaposes all of them in a collage, based on the tree structure specified by root.
template<class T1, class T2>
ns_collage_info ns_make_collage(const ns_collage_tree_node<const T1> & root, T2 & reciever, const unsigned long output_buffer_height,const unsigned long_scaling_factor=1, const unsigned long background_val=0, const bool resize_if_necissary = false){
	if (root.image != 0){
		root.image->pump(reciever,output_buffer_height);
		ns_collage_info info;
		info.tiles_per_row = 1;
		info.tile_width = root.image->properties().width;
		info.tile_height = root.image->properties().height;
		return info;
	}
	if (root.images.size() == 0)
		throw ns_ex("make_collage_from_tree::Empty collage tree node encountered.");
	if (root.images.size() == 1)
		return ns_compile_collage(root.images[0],reciever,output_buffer_height,long_scaling_factor,background_val,resize_if_necissary);

	std::vector<ns_image_whole<T1> > compound_images(root.images.size());
	for (unsigned int i = 0; i < root.images.size(); i++)
		ns_compile_collage(root.images[i],compound_images[i],output_buffer_height,long_scaling_factor,background_val,resize_if_necissary);
	return ns_compile_collage(compound_images,reciever,output_buffer_height,long_scaling_factor,background_val,resize_if_necissary);
}

///takes a collection of images and juxtaposes all of them in a collage, based on the tree structure specified by root.
template<class T1, class T2>
void ns_make_packed_collage(const ns_collage_tree_node<T1> & root, ns_image_whole<T2> & reciever, const unsigned long background_val=0, const unsigned int desired_width=800, const bool sort_for_compression=true){
	if (root.image != 0){
		root.image->pump(reciever,512);
		return;
	}
	if (root.images.size() == 0) 
		throw ns_ex("make_collage_from_tree::Empty collage tree node encountered.");
	if (root.images.size() == 1) return ns_make_packed_collage(root.images[0],reciever,background_val, desired_width);

	std::vector<ns_image_whole<T1> > compound_images(root.images.size());
	for (unsigned int i = 0; i < root.images.size(); i++)
		ns_make_packed_collage(root.images[i],compound_images[i],background_val,desired_width);
	ns_make_packed_collage(compound_images,reciever,background_val,desired_width,sort_for_compression);
}

///takes a collection of images and juxtaposes all of them in a collage, based on the tree structure specified by root.
template<class T1, class T2>
void ns_make_packed_collage(const ns_collage_tree_node<const T1> & root, ns_image_whole<T2> & reciever, const unsigned long background_val=0, const unsigned int desired_width=800, const bool sort_for_compression=true){
	if (root.image != 0){
		root.image->pump(reciever,512);
		return;
	}
	if (root.images.size() == 0) throw ns_ex("make_collage_from_tree::Empty collage tree node encountered.");
	if (root.images.size() == 1) return ns_make_packed_collage(root.images[0],reciever,background_val, desired_width);

	std::vector<const ns_image_whole<T1> > compound_images(root.images.size());
	for (unsigned int i = 0; i < root.images.size(); i++)
		ns_make_packed_collage(root.images[i],compound_images[i],background_val,desired_width);
	ns_make_packed_collage(compound_images,reciever,background_val,desired_width,sort_for_compression);
}

///morphological filter implementation used by dialation and erosion operators
template<int k_h_width, class whole_image, class whole_image2, bool foreground>
void ns_morph(const whole_image & source, whole_image2 &dest){
	if (source.properties().width < 2*k_h_width+1 || source.properties().height < 2*k_h_width+1)
		throw ns_ex("ns_morph: Kernal larger than image!");

	dest.prepare_to_recieve_image(source.properties());
	unsigned int num_white;

	
	for (int y = k_h_width; y < (int)source.properties().height - k_h_width - 1; y++){
		num_white = 0;
		//load in kernal mass (#white pixels)
		for (int _y = -k_h_width; _y <= k_h_width; _y++){
			for (int x = 0; x <= 2*k_h_width; x++){
				#pragma warning(disable: 4800)
				num_white += (((bool)source[y+_y][x]) == foreground);
				#pragma warning(default: 4800)

			}
		}
		//cerr << "Found " << num_white << "on left border.\n";
		//fill in left border
		if (num_white > 0)
			for (int x = 0; x <= k_h_width; x++) dest[y][x] = foreground;
		else
			for (int x = 0; x <= k_h_width; x++) dest[y][x] = !foreground;
		
		for (int _y = -k_h_width; _y <= k_h_width; _y++)
			#pragma warning(disable: 4800)
			num_white -= (((bool)source[y+_y][0]) == foreground);
			#pragma warning(default: 4800)

		//fill in center
		for (unsigned int x = k_h_width+1; x < source.properties().width - k_h_width-1; x++){
			//update kernal mass
			for (int _y = -k_h_width; _y <= k_h_width; _y++)
				#pragma warning(disable: 4800)
				num_white += (((bool)source[y+_y][x+k_h_width]) == foreground);
				#pragma warning(default: 4800)
	//		cerr << "After right edge we now have " << num_white << "\n";
			
			#pragma warning(disable: 4800)
			dest[y][x] = !(((bool)num_white) ^ ((bool)foreground));
			#pragma warning(default: 4800)

			//if num_white > 0, dest <- foreground
			//if num_white == 0, dest <- !foreground

			//update kernal mass
			for (int _y = -k_h_width; _y <= k_h_width; _y++)
				#pragma warning(disable: 4800)
				num_white -= (((bool)source[y+_y][x-k_h_width]) == foreground);
				#pragma warning(default: 4800)
		//	cerr << "After left edge we now have " << num_white << "\n";
		}
	//	cerr << "\n";
		//fill in right
		for (unsigned int x = source.properties().width - k_h_width -1; x < source.properties().width; x++)
			dest[y][x] = source[y][source.properties().width - k_h_width - 2];
	}
	//fill in top
	
	for (int y = 0; y < k_h_width; y++)
		for (unsigned int x = 0; x < source.properties().width; x++)
			dest[y][x] = dest[k_h_width ][x];
	//fill in bottom
	
	for (int y = (int)source.properties().height - k_h_width - 1; y < (int)source.properties().height; y++)
		for (unsigned int x = 0; x < source.properties().width; x++)
			dest[y][x] = dest[source.properties().height - k_h_width - 2][x];
}

//calculates the morphological erosion operation on the specified bitmap
template<int k_h_width, class whole_image, class whole_image2 > 
void inline ns_erode(const whole_image & source, whole_image2 &dest){
	ns_morph<k_h_width,whole_image,whole_image2,false>(source,dest);
}
//calculates the morphological dialation operation on the specified bitmap
template<int k_h_width, class whole_image, class whole_image2>
void inline ns_dilate(const whole_image & source, whole_image2 &dest){
	ns_morph<k_h_width,whole_image,whole_image2,true>(source,dest);
}

//calculates the morphological close operation on the specified bitmap
template<int k_h_width, class whole_image, class whole_image2>
void inline ns_close(const whole_image & source, whole_image2 &dest){
	whole_image2 temp;
	ns_dilate<k_h_width>(source,temp);
	ns_erode<k_h_width>(temp,dest);
}


//calculates the morphological open operation on the specified bitmap
template<int k_h_width, class whole_image, class whole_image2>
void inline ns_open(const whole_image & source, whole_image2 &dest){
	whole_image2 temp;
	ns_erode<k_h_width>(source,temp);
	ns_dilate<k_h_width>(temp,dest);
}

template<class ns_component, class ns_component_2>
void ns_zhang_thinning(const ns_image_whole<ns_component> & in, ns_image_whole<ns_component_2> & out){
	bool pixels_changed = true;

	//std::string num = ns_to_string(rand());
	//std::string b = "garbage_in_" + num + ".tif";
	out.prepare_to_recieve_image(in.properties());
	for (unsigned int y = 0; y < in.properties().height; y++)
		for (unsigned int x = 0; x < in.properties().width; x++)
			out[y][x] = in[y][x];

	//ns_save_image(b,out);

	ns_zhang_thinning(out);

	
	//zhang thinning can produce 't' junctions where the point at the intersection
	//of two straight lines has four neighbors.  We run this filter to
	//produce a topology that only has 3 neighbors (as per all our mesh algorithms)
	for (unsigned int y = 1; y < out.properties().height-1; y++){
		for (unsigned int x = 1;x < out.properties().width-1; x++){
			if (out[y-1][x] && out[y][x] && out[y+1][x] &&
				out[y][x-1] && out[y][x+1]){
				out[y-1][x] = 0;
				out[y-1][x+1] = 1;
			}		
		}
	}

	/*//zhang thinning can produce junctions where four pixels are contiguous to one another.
	//We run this filter to produce a topology that only has 3 neighbors (as per all our mesh algorithms)
	for (unsigned int y = 1; y < out.properties().height-1; y++){
		for (unsigned int x = 1;x < out.properties().width-1; x++){
			if (out[y][x] && out[y][x+1] && 
				out[y+1][x] && out[y+1][x+1]){
				out[y][x] = 0;
				if (out[y-1][x-1]&&!out[y-1][x])
				out[y][x-1] = 1;
			}		
		}
	}*/

	//std::string a = "garbage_out_out_" + num + ".tif";
	//ns_save_image(a,out);
}

///Zhang T.Y., Suen C.Y., A Fast Parallel Algorithm for Thinning Digital Patterns. Communications of the ACM v27 n3 March 1984
template<class ns_component>
void ns_zhang_thinning(ns_image_whole<ns_component> & image){
	//create a temporary space one larger than the image
	//thus we don't worry about border cases
	ns_image_standard im;
	ns_image_properties p(image.properties());
	ns_image_properties pp(p);
	pp.height+=2;
	pp.width+=2;
	im.prepare_to_recieve_image(pp);

	//ns_image_standard vis;
	//vis.prepare_to_recieve_image(pp);
	//for (unsigned int y = 0; y < pp.height; y++)
	//	for (unsigned int x = 0; x < pp.width; x++)
	//		vis[y][x] = 0;

	const unsigned int h(p.height+1),
					   w(p.width+1);

	//fill borders with 0
	for (unsigned int y = 0; y < pp.height; y++){
		im[y][0]=0;
		im[y][pp.width-1]=0;
	}
	for (unsigned int x = 0; x < pp.width; x++){
		im[0][x]=0;
		im[pp.height-1][x]=0;
	}
	//fill center with image
	for (unsigned int y = 0; y < p.height;y++)
		for (unsigned int x = 0; x < p.width; x++)
			im[y+1][x+1] = (ns_8_bit)(image[y][x] != 0); 

	//unsigned int round = 1;
	while(true){
		bool pixels_changed = false;
		//first subiteration
		//mark all to-be-deleted pixels as 2
		for (unsigned int y = 1; y < h; y++){
			for (unsigned int x = 1; x < w; x++){
				const bool p1 = im[y]  [x]  !=0, p2 = im[y-1][x]  !=0, p3 = im[y-1][x+1]!=0,
						   p4 = im[y]  [x+1]!=0, p5 = im[y+1][x+1]!=0, p6 = im[y+1][x]  !=0,
						   p7 = im[y+1][x-1]!=0, p8 = im[y]  [x-1]!=0, p9 = im[y-1][x-1]!=0;
				ns_8_bit B = (ns_8_bit)p2 + (ns_8_bit)p3 + (ns_8_bit)p4 + (ns_8_bit)p5  + (ns_8_bit)p6 + (ns_8_bit)p7 + (ns_8_bit)p8 + (ns_8_bit)p9;
				ns_8_bit A = (ns_8_bit)(!p2 && p3) + (ns_8_bit)(!p3 && p4) + (ns_8_bit)(!p4 && p5) + (ns_8_bit)(!p5 && p6) + 
							 (ns_8_bit)(!p6 && p7) + (ns_8_bit)(!p7 && p8) + (ns_8_bit)(!p8 && p9) + (ns_8_bit)(!p9 && p2);
				//first subiteration
				im[y][x] += (ns_8_bit)
					   ( p1 &&
					   (2 <= B && B <= 6) &&
					   (A == 1) &&
					   (p2&&p4&&p6) == 0 &&
					   (p4&&p6&&p8) == 0
					   );
		//		if (im[y][x] == 2)
		//			vis[y][x] = round;
				pixels_changed = (pixels_changed ||  im[y][x]==2);

			}
		}
		if (!pixels_changed) break;

		//pixels to be deleted have the value 2  (10b)
		//pixels not to be deleted have the value 1  (01b).
		//Thus, a bitwise AND operation with 01b will set 10b values to zero
		//but retain 01b values
		for (unsigned int y = 1; y < h; y++)
			for (unsigned int x = 1; x < w; x++)
				im[y][x]&=1;

		//second subiteration
		//mark all to-be-deleted pixels as 2
		for (unsigned int y = 1; y < h; y++){
			for (unsigned int x = 1; x < w; x++){
			const bool p1 = im[y]  [x]  !=0, p2 = im[y-1][x]  !=0, p3 = im[y-1][x+1]!=0,
						   p4 = im[y]  [x+1]!=0, p5 = im[y+1][x+1]!=0, p6 = im[y+1][x]  !=0,
						   p7 = im[y+1][x-1]!=0, p8 = im[y]  [x-1]!=0, p9 = im[y-1][x-1]!=0;
				ns_8_bit B = (ns_8_bit)p2 + (ns_8_bit)p3 + (ns_8_bit)p4 + (ns_8_bit)p5  + (ns_8_bit)p6 + (ns_8_bit)p7 + (ns_8_bit)p8 + (ns_8_bit)p9;
				ns_8_bit A = (ns_8_bit)(!p2 && p3) + (ns_8_bit)(!p3 && p4) + (ns_8_bit)(!p4 && p5) + (ns_8_bit)(!p5 && p6) + 
							 (ns_8_bit)(!p6 && p7) + (ns_8_bit)(!p7 && p8) + (ns_8_bit)(!p8 && p9) + (ns_8_bit)(!p9 && p2);
				//first subiteration
				im[y][x] += (ns_8_bit)
					   ( p1 &&
					   (2 <= B && B <= 6) &&
					   (A == 1) &&
					   (p2&&p4&&p8) == 0 &&
					   (p2&&p6&&p8) == 0
					   );
			//	if (im[y][x] == 2)
			//		vis[y][x] = round;
				pixels_changed = (pixels_changed || im[y][x]==2);
			}
		}
	
		if (!pixels_changed) break;

		for (unsigned int y = 1; y < h; y++)
			for (unsigned int x = 1; x < w; x++)
				im[y][x]&=1;	
	//	round++;
	}

	//copy processed image back
	for (unsigned int y = 0; y < p.height;y++)
		for (unsigned int x = 0; x < p.width; x++)
			 image[y][x] = (ns_component)(im[y+1][x+1]!=0);

//	std::string num = ns_to_string(rand());
	//std::string a = "garbage_" + num + ".tif";
	//ns_save_image(a,vis);
	//std::string b = "garbage_out" + num + ".tif";
	//ns_save_image(b,image);
}
/*
template<class ns_component, char offset>
class ns_row_contains_light{
	public:
	static inline bool run(const unsigned int x, const unsigned int y, const ns_image_whole<ns_component> & im){
		return im[y][x+offset]!=0 || ns_row_contains_light<ns_component,offset-1>::run(x,y,im);
	}
};
template<class ns_component>
class ns_row_contains_light<ns_component,0>{
	public:
	static inline bool run(const unsigned int x, const unsigned int y, const ns_image_whole<ns_component> & im){
		return im[y][x]!=0;
	}
};
template<class ns_component, char offset>
class ns_column_contains_light{
	public:
	static inline bool run(const unsigned int x, const unsigned int y, const ns_image_whole<ns_component> & im){
		return im[y+offset][x]!=0 || ns_column_contains_light<ns_component,offset-1>::run(x,y,im);
	}
};
template<class ns_component>
class ns_column_contains_light<ns_component,0>{
	public:
	static inline bool run(const unsigned int x, const unsigned int y, const ns_image_whole<ns_component> & im){
		return im[y][x]!=0;
	}
};

template<class ns_component, char kernal_size>
inline bool ns_pixel_is_surrounded_by_dark(const unsigned int x, const unsigned int y, const ns_image_whole<ns_component> & im){
	if (ns_row_contains_light<ns_component,2*kernal_size+1>::run(x-kernal_size,y-kernal_size,im))
		return false;

	if (ns_column_contains_light<ns_component,2*kernal_size+1>::run(x-kernal_size,y-kernal_size,im))
		return false;
	
	if (ns_column_contains_light<ns_component,2*kernal_size+1>::run(x+kernal_size,y-kernal_size,im))
		return false;
	
	if (ns_row_contains_light<ns_component,2*kernal_size+1>::run(x-kernal_size,y+kernal_size,im))
		return false;

	return true;
}

*/

template<class ns_component, char kernal_size>
inline bool ns_pixel_is_surrounded_by_dark(const unsigned int x, const unsigned int y, const ns_image_whole<ns_component> & im){
	for (int _x = -kernal_size; _x <= kernal_size; _x++)
		if (im[y-kernal_size][x+_x])
			return false;
	
	for (int _y = -kernal_size; _y <= kernal_size; _y++){
		if (im[y+_y][x-kernal_size] ||
		    im[y+_y][x+kernal_size])
			return false;
	}
	
	for (int _x = -kernal_size; _x <= kernal_size; _x++)
		if (im[y+kernal_size][x+_x]) return false;
	return true;
}

template<class ns_component, char kernal_size>
void ns_remove_small_objects(ns_image_whole<ns_component> & im){
	const unsigned kw(kernal_size+kernal_size+1);
	const int h( im.properties().height-kernal_size),
						w( im.properties().width-kernal_size);
	for (int y =kernal_size; y < h-1; y++){
		for (int x =kernal_size; x < w-1; x++){

			if (ns_pixel_is_surrounded_by_dark<ns_component,kernal_size>(x,y,im))
				for (int _y = -kernal_size; _y <= kernal_size; _y++){
					memset(&im[y+_y][x-kernal_size],0,kw);
					//for (int _x = -kernal_size; _x <= kernal_size; _x++){
					//	im[y+_y][x+_x] = 0;
					//}
				}
		}
	}
}

template<class ns_component>
inline void ns_crop_lower_intensity(ns_image_whole<ns_component> & image,const ns_component thresh){
	if (thresh == 0) return;
	const unsigned long h = image.properties().height,
						w = image.properties().width;
	for (unsigned int y = 0; y < h; y++)
		for (unsigned int x = 0; x < w; x++)
			if (image[y][x] < thresh)
				image[y][x] = 0;
}

void ns_process_dynamic_stretch(ns_image_standard & im);


struct ns_bitmap_overlap_calculation_results{
	ns_bitmap_overlap_calculation_results():image_1_area(0),
											image_2_area(0),
											overlap_area(0),
											image_1_absolute_area(0),
											image_2_absolute_area(0){}
	unsigned long image_1_area,  //total object area of image_1 in the two images' bounding rectangles' overlap
				  image_2_area,  //total object area of image_2 in the two images' bounding rectangles' overlap
				  overlap_area;  //total area of object overlap in the two images' bounding rectangles' overlap

	unsigned long image_1_absolute_area,  //total object area of image_1
				  image_2_absolute_area;  //total object area of image_2
};
template<class ns_component>
ns_bitmap_overlap_calculation_results ns_calculate_bitmap_overlap(const ns_image_whole<ns_component> & im1, const ns_vector_2i & im1_offset, const ns_image_whole<ns_component> & im2, const ns_vector_2i & im2_offset, ns_image_standard * vis=0){

	ns_bitmap_overlap_calculation_results res;
	if (!ns_rectangle_intersect<ns_vector_2i>(im1_offset,im1_offset+ns_vector_2i(im1.properties().width,im1.properties().height),
								im2_offset,im2_offset+ns_vector_2i(im2.properties().width,im2.properties().height))){
			if (vis!=0){
				vis->prepare_to_recieve_image(ns_image_properties(1,1,3));
				(*vis)[0][0] = 0;
				(*vis)[0][1] = 0;
				(*vis)[0][2] = 0;
			}
		return res;
	}

	ns_vector_2i bl(im1_offset.x,im1_offset.y),
				 tr(im1_offset.x+im1.properties().width,
					im1_offset.y+im1.properties().height);

	//find upper left hand corner of overlapping region shared byy both worm and neighbor
	//Coordinates stored as the distance from (0,0) in the original image from which both regions were lifted
	//(ie absolute to the image rather than relative to the neighbor or the worm region)
	if (bl.x < im2_offset.x) bl.x = im2_offset.x;
	if (bl.y < im2_offset.y) bl.y = im2_offset.y;
	if (tr.x > im2_offset.x + (int)im2.properties().width)  tr.x = im2_offset.x + (int)im2.properties().width;
	if (tr.y > im2_offset.y + (int)im2.properties().height) tr.y = im2_offset.y + (int)im2.properties().height;

	//find the coordinates where we will start and stop looking for overlap
	unsigned long area_overlap = 0;
	for (int y = (int)bl.y; y < (int)tr.y; y++){
		for (int x = (int)bl.x; x < (int)tr.x; x++){
			res.image_1_area+=im1[y-im1_offset.y][x-im1_offset.x];
			res.image_2_area+=im2[y-im2_offset.y][x-im2_offset.x];
			if (im1[y-im1_offset.y][x-im1_offset.x]!=0 &&
				im2[y-im2_offset.y][x-im2_offset.x]!=0)
				res.overlap_area++;
		}
	}

	for (unsigned y = 0; y < im1.properties().height; y++){
		for (unsigned x = 0; x < im1.properties().width; x++){
			res.image_1_absolute_area+=im1[y][x]!=0;
		}
	}
	for (unsigned y = 0; y < im2.properties().height; y++){
		for (unsigned x = 0; x < im2.properties().width; x++){
			res.image_2_absolute_area+=im2[y][x]!=0;
		}
	}

	if (vis != 0){
		ns_vector_2i bl_i(im1_offset.x,im1_offset.y),
				 tr_i(im1_offset.x+im1.properties().width,
					im1_offset.y+im1.properties().height);
		if (bl_i.x > im2_offset.x ) bl_i.x = im2_offset.x;
		if (bl_i.y > im2_offset.y ) bl_i.y = im2_offset.y;
		if (tr_i.x < im2_offset.x + (int)im2.properties().width) tr_i.x  = im2_offset.x + (int)im2.properties().width;
		if (tr_i.y < im2_offset.y + (int)im2.properties().height) tr_i.y  = im2_offset.y + (int)im2.properties().height;
		vis->prepare_to_recieve_image(ns_image_properties(tr_i.y-bl_i.y,tr_i.x-bl_i.x,3));

		ns_vector_2i im1_offset_i(im1_offset - bl_i),
					 im2_offset_i(im2_offset - bl_i);
		for (int y = (int)bl.y; y < (int)tr.y; y++){
			for (int x = (int)bl.x; x < (int)tr.x; x++){
				if (y >= im1_offset_i.y &&
					x >= im1_offset_i.x &&
					y-im1_offset_i.y < (int)im1.properties().height &&
					x-im1_offset_i.x < (int)im1.properties().width)
					(*vis)[y-bl.y][3*(x-bl.x)  ] = 255*(unsigned char)im1[y-im1_offset_i.y][x-im1_offset_i.x];
				else
					(*vis)[y-bl.y][3*(x-bl.x)  ] = 0;
				if (y >= im2_offset_i.y &&
					x >= im2_offset_i.x &&
					y-im2_offset_i.y < (int)im2.properties().height &&
					x-im2_offset_i.x < (int)im2.properties().width)
					(*vis)[y-bl.y][3*(x-bl.x) + 1] = 255*(unsigned char)im2[y-im2_offset_i.y][x-im2_offset_i.x];
				else (*vis)[y-bl.y][3*(x-bl.x) + 1] = 0;
				(*vis)[y-bl.y][3*(x-bl.x) + 2] = 0;
			}
		}
	}
	return res;
}



#endif
