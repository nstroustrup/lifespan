#ifndef NS_PROCESS_MASK_REGIONS
#define NS_PROCESS_MASK_REGIONS

#include <vector>
#include <algorithm>
#include <errno.h>
#include <vector>
#include <string>
#include <map>
#include <sys/stat.h>
#include <cmath>

#include "ns_sql.h"
#include "ns_image.h"
#include "ns_image_server.h"
#include "ns_image_storage_handler.h"
#include "ns_dir.h"
#include "ns_tiff.h"
#include "ns_vector.h"
#define NS_INCLUDE_FONT
#ifdef NS_INCLUDE_FONT
#include "ns_font.h"
#endif

#include "ns_image_tools.h"
#include "ns_image_statistics.h"




///holds statistics about a region of an image mask used to break down
///a large image into sub-regions
struct ns_image_mask_region_stats{

	ns_image_mask_region_stats(){clear();}

	ns_64_bit x_sum, y_sum, pixel_count;
	unsigned long x_min,
				 y_min,
				 x_max,
				 y_max,
				 x_avg,
				 y_avg;

	void clear(){
		x_sum = 0;
		y_sum = 0;
		pixel_count = 0;
		y_max = 0;
		x_max = 0;
		y_min = 1000000;
		x_min = 1000000;
		y_avg = 0;
		x_avg = 0;
	}

	///given two regions, calculates the statistics corresponding to the union of both
	ns_image_mask_region_stats combine(const ns_image_mask_region_stats & l){
		ns_image_mask_region_stats n;
		n.x_sum = x_sum + l.x_sum;
		n.y_sum = y_sum + l.y_sum;

		if (x_max > l.x_max)
			n.x_max = x_max;
		else n.x_max = l.x_max;
		if (x_min < l.x_min)
			n.x_min = x_min;
		else n.x_min= l.x_min;

		if (y_max > l.y_max)
			n.y_max = y_max;
		else n.y_max = l.y_max;
		if (y_min < l.y_min)
			n.y_min = y_min;
		else n.y_min= l.y_min;

		n.pixel_count = pixel_count + l.pixel_count;

		n.x_avg = (unsigned long) (n.x_sum/n.pixel_count);
		n.y_avg = (unsigned long) (n.y_sum/n.pixel_count);
		return n;
	}
};

///holds book-keeping information about a region of an image mask used to break down
///a large image into sub-regions
template<class ns_component>
class ns_image_mask_region_info{
public:
	ns_image_mask_region_info():reciever(0), lines_sent_to_reciever(0),reciever_provided(false){clear();}
	//the pixel brightness ( 0- (2^n-1) ) of the specified region in the mask
	ns_component mask_value;
	//when an image is being split into masked region, output_stream refers to the output file for the specified mask region.
	ns_image_storage_reciever_handle<ns_component> reciever;
	unsigned long lines_sent_to_reciever;
	ns_image_properties reciever_image_properties;
	
	ns_image_statistics image_stats;

	bool reciever_provided;

	//when an image is being split into masked region, this buffer holds the image for the created masked region
	ns_image_stream_static_buffer<ns_component> output_buffer;
	//information about the masked region, length, height, position, etc.
	ns_image_mask_region_stats stats;
	
	void clear(){
		mask_value = 0;
		reciever_provided = false;
		stats.clear();
	}
	~ns_image_mask_region_info(){
		if (stats.pixel_count != 0){
			reciever.clear();

		}
	
	}
};

///ns_image_mask_info holds, manages, and stores statistics for each region of a mask image.
template<class ns_component>
class ns_image_mask_info{
public:
	ns_image_mask_info():region_info(256){clear();}

	///Loads the statistics for each region of a mask from the database
	void load_from_db(ns_64_bit mask_id, ns_sql & sql){
		clear();
		sql << "SELECT mask_value, x_min, y_min, x_max, y_max, y_average, x_average, pixel_count FROM image_mask_regions WHERE mask_id=" << mask_id;
		ns_sql_result region_data;
		sql.get_rows(region_data);
		for (unsigned long i = 0; i < region_data.size(); i++){
			unsigned int value = atol(region_data[i][0].c_str());
			if (value >= region_info.size()){
				if (value > 256*256)
					throw ns_ex("ns_image_mask_info: Region value specified (") << region_data[i][0] << ") is too large.";
				region_info.resize(value+1);
			}
				region_info[ value ].mask_value			= value;
				region_info[ value ].stats.x_min		= atol(region_data[i][1].c_str());
				region_info[ value ].stats.y_min		= atol(region_data[i][2].c_str());
				region_info[ value ].stats.x_max		= atol(region_data[i][3].c_str());
				region_info[ value ].stats.y_max		= atol(region_data[i][4].c_str());
				region_info[ value ].stats.x_avg		= atol(region_data[i][5].c_str());
				region_info[ value ].stats.y_avg		= atol(region_data[i][6].c_str());
				region_info[ value ].stats.pixel_count	= atol(region_data[i][7].c_str());
		}
	}

	///saves all information about each region in a mask to
	///the database.
	void save_to_db(ns_64_bit mask_id, ns_sql & sql) const{
		sql << "DELETE FROM image_mask_regions WHERE mask_id=" << mask_id;
		sql.send_query();
		sql.send_query("COMMIT");
		for (unsigned long i = 0; i < region_info.size(); i++){
			if (region_info[i].mask_value == 0)  //skip 0 values, as no mask can have this value.
				continue;

			sql << "INSERT INTO image_mask_regions SET";
			sql << "  mask_id = " << mask_id;
			sql << ", mask_value = " << region_info[i].mask_value;
			sql << ", x_min = " << region_info[i].stats.x_min;
			sql << ", x_max = " << region_info[i].stats.x_max;
			sql << ", y_min = " << region_info[i].stats.y_min;
			sql << ", y_max = " << region_info[i].stats.y_max;
			sql << ", x_average = " << region_info[i].stats.x_avg;
			sql << ", y_average = " << region_info[i].stats.y_avg;
			sql << ", pixel_count = " << (unsigned long)region_info[i].stats.pixel_count;
			sql.send_query();
		}
		sql.send_query("COMMIT");
	}

	///access region information for the mask
	inline ns_image_mask_region_info<ns_component> * operator[](const unsigned long i){
		return &region_info[i];
	}
	///access region information for the mask
	inline const ns_image_mask_region_info<ns_component> * operator[](const unsigned long i) const{
		return &region_info[i];
	}
	///ask how many regions could theoretically be present in an image
	///that is, the bit-depth of the image
	inline const unsigned long size() const{
		return static_cast<unsigned long>(region_info.size());
	}		
	///ask how many regions are specified in the mask
	///that is, how many different pixel intensities were specified
	inline const unsigned long number_of_regions(){
		unsigned long num = 0;
		for (unsigned int i = 0; i < region_info.size(); i++)
			if (region_info[i].stats.pixel_count != 0){
			//	cerr << "Found pixels of " << i << "\n";
				num++;
			}
		return num;
	}

	///cleares all region information
	void clear(){
		unsigned int s = (unsigned int)region_info.size();
		region_info.clear();
		region_info.resize(s);
	}
	///cleares all region information and explicity deallocates memory (used for debugging)
	void clear_heap(){
		region_info.clear();
	}
	~ns_image_mask_info(){
		clear();
	}
private:
	std::vector< ns_image_mask_region_info<ns_component> > region_info;
};


///Image masks are used to break down large images into sub-regions that can then be analyzed separately.
///ns_image_mask_analyzer analyzes a mask image to calculate statistics about the specified regions.
///It also can produce a visualzation image useful for debugging mask images.
template<class ns_component>
#pragma warning(disable: 4355)
class ns_image_mask_analyzer: public ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >{
public:

	ns_image_mask_analyzer(const long max_line_block_height):
		update_db(false),mask_id(0),ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >(max_line_block_height,this),mask_visualization_output(0),resize_factor(10){
			_server = &::image_server;
		}
#pragma warning(default: 4355)
	
	void set_resize_factor(const unsigned int i){
		resize_factor = i;
	}

	void register_visualization_output(ns_image_whole<ns_component> & image){
		mask_visualization_output = &image;
	}	
	

	///Prepare the mask analyzer to recieve the mask image.
	bool init(const ns_image_properties & properties){

		
		//reset all mask info
		_mask_info.clear();
		y = 0;

		if (mask_visualization_output != 0){
			ns_image_properties prop;
			prop.height = properties.height;
			prop.width = properties.width;
			prop.components = 3;
			mask_visualization_output->init(prop);
		}


		if (properties.width == 0 && properties.height == 0){
			buffer.resize(ns_image_stream_buffer_properties(0,0));
			return true;
		}
		//allocate the input buffer
		ns_image_stream_buffer_properties bufp;
		bufp.height = ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_max_line_block_height;
		bufp.width = properties.width*properties.components;
		buffer.resize(bufp);

		return false;

	}

	///access information about the mask's regions.
	const ns_image_mask_info<ns_component > & mask_info() const{
		return _mask_info;
	}
	///access information about the mask's regions.
	ns_image_mask_info<ns_component > & mask_info(){
		return _mask_info;
	}

	///Called by a ns_image_stream_sender to confirm that all image data has been sent. 
	///Region information is saved to disk and annotated in the database
	void finish_recieving_image(){
		if (y != ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.height)
			throw ns_ex("ns_image_mask_analyzer::finish_recieving_image() called before all lines received.");

		//we have collected all the raw data, but we do need to calculate the averages now it is all finished.
		for (unsigned int i = 0; i < _mask_info.size(); i++){
			if (_mask_info[i]->stats.pixel_count == 0)
				continue;
			//cerr << "SUM = " << _mask_info[i].stats.x_sum << "," << _mask_info[i].stats.y_sum << "\n";
			_mask_info[i]->stats.x_sum*=resize_factor*resize_factor;
			_mask_info[i]->stats.y_sum*=resize_factor*resize_factor;
			_mask_info[i]->stats.pixel_count*=resize_factor;
			_mask_info[i]->stats.y_max*=resize_factor;
			_mask_info[i]->stats.x_max*=resize_factor;
			_mask_info[i]->stats.y_min*=resize_factor;
			_mask_info[i]->stats.x_min*=resize_factor;


			_mask_info[i]->stats.x_avg = (unsigned long)(_mask_info[i]->stats.x_sum / _mask_info[i]->stats.pixel_count);
			_mask_info[i]->stats.y_avg = (unsigned long)(_mask_info[i]->stats.y_sum / _mask_info[i]->stats.pixel_count);
			_mask_info[i]->mask_value = i;
			//cerr << "Pixel value " << i << " has " << _mask_info[i].stats.pixel_count << " pixels.  Center: (" << _mask_info[i].stats.x_avg << "," << _mask_info[i].stats.y_avg << ")\n";
		}

	
		//handle visualization
		if(mask_visualization_output != 0){
			mask_visualization_output->finish_recieving_image();
			ns_image_standard im;
			ns_image_properties p(mask_visualization_output->properties());
			p.components = 1;
			im.prepare_to_recieve_image(p);
			for (unsigned int y = 0; y < p.height; y++){
				for (unsigned int x = 0; x < p.width; x++){
					im[y][x] = (*mask_visualization_output)[y][x];
				}
			}


	 		//go through and make pretty colors for visualization
			unsigned int num_regions = _mask_info.number_of_regions();
			
			ns_vector_3<ns_component> black(0,0,0);
			std::vector<ns_vector_3<ns_component> >vis_mapping((unsigned long)pow((float)2,(int)(8*sizeof(ns_component))),black);

			//some trickery to try and make closely related grayscale values take widely different colors

			std::vector<ns_component> region_values(num_regions);

			unsigned int j = 0;
			for (unsigned int i = 0; i < _mask_info.size(); i++){
				if (_mask_info[i]->stats.pixel_count != 0){
					region_values[j] = _mask_info[i]->mask_value;
					j++;
				}
			}
			std::sort(region_values.begin(),region_values.end());

			ns_component od = 1;
			ns_component d = 0;
			int count = 0;
			for (unsigned int i = 0; i < num_regions; i++){
				vis_mapping[region_values[i]] = ns_rainbow< ns_vector_3<ns_component> >(((float)count)/num_regions);
				count = num_regions - count -1;
				d++;
				if (d == 2){
					d = 0;
					count++;
				}
			}
            const long border_s(20);
			for (unsigned y = 0; y < ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.height; y++){
				for (int x = ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.width-1; x >= 0 ; x--){
					long y0((long)y-border_s),
						 y1(y+border_s),
						 x0((long)x-border_s),
						 x1(x+border_s);
					if (y0 < 0) y0 = 0;
					if (y1 > (long)ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.height)
						y1 =  (long)ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.height;
					if (x0 < 0) x0 = 0;
					if (x1 >  (long)ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.width)
						x1 =  (long)ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.width;

					
					const ns_component cur_index((*mask_visualization_output)[y][x]);

					bool found_diff(false);
					if (cur_index != 0){
						for (unsigned int dy = (unsigned int)y0; (unsigned int)dy < (unsigned int)y1 && !found_diff; dy++)
							for (unsigned int dx = (unsigned int)x0; (unsigned int)dx < (unsigned int)x1; dx++){
								if (im[dy][dx] !=  cur_index){
										found_diff = true;
										break;
								}
							}
					}

					if (found_diff){
						(*mask_visualization_output)[y][x*3] = 255;
						(*mask_visualization_output)[y][x*3+1] = 255;
						(*mask_visualization_output)[y][x*3+2] = 255;
					}
					else{
						//assign the output image the appropriate color.
						(*mask_visualization_output)[y][x*3] = vis_mapping[cur_index].x;
						(*mask_visualization_output)[y][x*3+1] = vis_mapping[cur_index].y;
						(*mask_visualization_output)[y][x*3+2] = vis_mapping[cur_index].z;
					}
				}
			}

			unsigned int line_width = ((ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.height + ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.width)/2)/300;
			if (line_width == 0)
				line_width = 1;
			//go through and draw a nice box around each region
			for (unsigned int i = 0; i < _mask_info.size(); i++){
				if (_mask_info[i]->stats.pixel_count == 0)
					continue;
				ns_vector_2i mmax = ns_vector_2i(_mask_info[i]->stats.x_max,_mask_info[i]->stats.y_max);
				ns_vector_2i mmin = ns_vector_2i(_mask_info[i]->stats.x_min,_mask_info[i]->stats.y_min);
				ns_vector_2i avg = (mmax + mmin)/2;
				mmax = mmax/resize_factor;
				mmin = mmin/resize_factor;
				avg = avg/resize_factor;


				mask_visualization_output->draw_line_color_thick(ns_vector_2i(avg.x+30,avg.y+30), ns_vector_2i(avg.x-30,avg.y-30),ns_color_8(255,255,255),line_width);
				mask_visualization_output->draw_line_color_thick(ns_vector_2i(avg.x-30,avg.y+30), ns_vector_2i(avg.x+30,avg.y-30),ns_color_8(255,255,255),line_width);
				
				mask_visualization_output->draw_line_color_thick(ns_vector_2i(mmin.x,mmin.y),ns_vector_2i(mmin.x,mmax.y),vis_mapping[i]*.9,line_width);
				mask_visualization_output->draw_line_color_thick(ns_vector_2i(mmin.x,mmax.y),ns_vector_2i(mmax.x,mmax.y),vis_mapping[i]*.9,line_width);
				mask_visualization_output->draw_line_color_thick(ns_vector_2i(mmax.x,mmax.y),ns_vector_2i(mmax.x,mmin.y),vis_mapping[i]*.9,line_width);
				mask_visualization_output->draw_line_color_thick(ns_vector_2i(mmax.x,mmin.y),ns_vector_2i(mmin.x,mmin.y),vis_mapping[i]*.9,line_width);
			}
			
			#ifdef NS_INCLUDE_FONT
			//now go through and label the regions.
			//cerr << "Loading fonts.\n";
			unsigned long image_height = mask_visualization_output->properties().height,
						  text_height(16);
			ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
			font_server.get_default_font().set_height(text_height);
			for (unsigned int i = 0; i < _mask_info.size(); i++){
				if (_mask_info[i]->stats.pixel_count != 0){
					text_height = image_height - _mask_info[i]->stats.y_avg -3;
					if (text_height > (_mask_info[i]->stats.y_max - _mask_info[i]->stats.y_min)/(resize_factor*3))
						text_height = (_mask_info[i]->stats.y_max - _mask_info[i]->stats.y_min)/(resize_factor*3);
					if (text_height < mask_visualization_output->properties().height/40)
						text_height = mask_visualization_output->properties().height/40;
					if (text_height < 12)
						text_height = 12;
					
					//cerr << "Drawing font at (" << _mask_info[i].stats.x_avg << "," << _mask_info[i].stats.y_avg << ")";
					font_server.get_default_font().draw(_mask_info[i]->stats.x_min/resize_factor+line_width,_mask_info[i]->stats.y_avg/resize_factor+line_width,ns_color_8(0,0,0),ns_to_string(_mask_info[i]->mask_value),*mask_visualization_output);
		
					//cerr << "..n";
					font_server.get_default_font().draw(_mask_info[i]->stats.x_min/resize_factor,_mask_info[i]->stats.y_avg/resize_factor,ns_color_8(255,255,255),ns_to_string(_mask_info[i]->mask_value),*mask_visualization_output);
				}
			}
			font_lock.release();
			#endif
			mask_visualization_output = 0;
		}
	}
	
	ns_image_stream_static_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		return &buffer;
	}
	

	///recieve mask lines from an ns_image_stream_sender
	void recieve_lines(const ns_image_stream_static_buffer<ns_component> & lines, const unsigned long height){
		ns_image_properties & im_properties(ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties);
		if (height + y > im_properties.height)
			throw ns_ex("ns_image_mask_region_info:: To many lines received!");

		for (unsigned int _y = 0; _y < height; _y++){
			for (unsigned int x = 0; x < im_properties.width; x++){

				unsigned int cur_y = _y + y;
				//if the pixel value is zero, there is no region here so move on.
				if (buffer[_y][x*im_properties.components] == 0)
					continue;
				
				//otherwise, the current pixel belongs to a region and so we must update that information.

				//first we get the region statistic object for the specified region
				ns_image_mask_region_stats * r = &(_mask_info[ buffer[_y][x*im_properties.components] ]->stats);
				//and then we update it.
				r->x_sum += x;
				r->y_sum += cur_y;
				//if (r->pixel_count == 0)
				//	cerr << "Accessing Pixel color" << (unsigned int)buffer[_y][x*_properties.components] << "\n";
				r->pixel_count++;
				if (r->y_min > cur_y)
					r->y_min = cur_y;
				if (r->y_max < cur_y)
					r->y_max = cur_y;
				if (r->x_min > x)
					r->x_min = x;
				if (r->x_max < x)
					r->x_max = x;			
			}
		}
		//output processed mask file to new location on disk
		//output_stream->recieve_lines(lines,height);

		//output visualization information if needed	
		if (mask_visualization_output != 0){
			
			ns_image_stream_buffer_properties bufp;
			bufp.height = height;
			bufp.width = lines.properties().width*3;
			ns_image_stream_static_offset_buffer<ns_8_bit> * buf = mask_visualization_output->provide_buffer(bufp);
			for (unsigned int y = 0; y < height; y++)
				for (unsigned int x = 0; x < lines.properties().width; x++)
					(*buf)[y][x] = lines[y][x];
			mask_visualization_output->recieve_lines(*buf,height);
		}

		y+=height;
	}




private:
	unsigned int y;
	///information about mask regions
	ns_image_mask_info<ns_component> _mask_info;
	//holds received image information as it is processed.
	ns_image_stream_static_buffer<ns_component> buffer;  
	//the database id of the current mask
	unsigned long mask_id;	

	unsigned int resize_factor;

	//a pointer to the current image server object, from which database and file-system access is obtained
	ns_image_server * _server;				

	 //the original filename of the mask
	std::string mask_filename;		

	//the output image to which a mask visualization is sent when set_visualization() is called.
	ns_image_whole<ns_component> * mask_visualization_output;

	 //if true, mask information is annotated in the database.
	bool update_db; 


};


///this class recieves an image and splits it into multiple files using
///the specified mask image as a reference.
///specifiy_mask() must be called prior to recieving the first image
#pragma warning(disable: 4355)
template<class ns_component, class output_buffer_t>
class ns_image_stream_mask_splitter: public ns_image_stream_reciever<ns_image_stream_static_buffer<ns_8_bit> >{
	public:
	ns_image_stream_mask_splitter(const long max_line_block_height):ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >(max_line_block_height,this),
		mask(0),registration_offset(0,0),resize_factor(10),stats(0){}

#pragma warning(default: 4355)
	ns_image_mask_info<ns_component> * mask_info(){return &_mask_info;}

	void specify_mask(const ns_image_standard & _mask){mask = &_mask;}
	void specificy_sample_image_statistics(ns_image_statistics & stats_){stats = &stats_;}
	void specify_registration_offset(const ns_vector_2i & offset){registration_offset = offset;}

	void set_resize_factor(const unsigned long i){resize_factor = i;}
	//to recieve a message, first the reciever must provide a buffer into which the data will be written.
	ns_image_stream_static_buffer<ns_8_bit>  * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		return &in_buffer;
	}

	bool init(const ns_image_properties & properties){
		source_image_properties = properties;
		if (mask == 0)
			throw ns_ex("ns_image_stream_mask_splitter::Mask must be provided before images can be masked.");
		if (mask->properties().height != properties.height/resize_factor || mask->properties().width != properties.width/resize_factor)
			throw ns_ex("ns_image_stream_mask_splitter::Image dimentions do not match mask dimentions!") << ns_file_io;

		//prepare to recieve file input
		ns_image_stream_buffer_properties bufp;
		bufp.height = _max_line_block_height;
		bufp.width = properties.width*properties.components;
		in_buffer.resize(bufp);

		if (stats != 0){
			stats->size.x = properties.width;
			stats->size.y = properties.height;
		}

		y = 0;



		//initialize all desired outputs
		for (unsigned int i = 0; i < _mask_info.size(); i++){
			if ((_mask_info)[i]->stats.pixel_count != 0){
				if ((_mask_info)[i]->reciever_provided == false){
					unsigned long v = (unsigned long)(_mask_info)[i]->stats.pixel_count;
					std::string units = "pixels";
					if ( (_mask_info)[i]->stats.pixel_count > 1024*1024){
						v = (unsigned long)((_mask_info)[i]->stats.pixel_count/(1024*1024));
						units = "megapixels";
					}

					throw ns_ex("ns_image_stream_mask_splitter::A mask image was found to contain regions of intensity ") << i 
						<< " of a size " << v << " " << units << " that is not present in the database mask record.  (Did you specifiy a mask for this region?)";
				}
				_mask_info[i]->reciever_image_properties.height = 	(_mask_info)[i]->stats.y_max-(_mask_info)[i]->stats.y_min+1;
				_mask_info[i]->reciever_image_properties.width =	(_mask_info)[i]->stats.x_max-(_mask_info)[i]->stats.x_min+1;
				_mask_info[i]->reciever_image_properties.components = properties.components;
				_mask_info[i]->reciever_image_properties.resolution = properties.resolution;
				
				_mask_info[i]->image_stats.size.x = _mask_info[i]->reciever_image_properties.width;
				_mask_info[i]->image_stats.size.y = _mask_info[i]->reciever_image_properties.height;


				_mask_info[i]->reciever.output_stream().prepare_to_recieve_image(_mask_info[i]->reciever_image_properties);
				_mask_info[i]->lines_sent_to_reciever = 0;
			}
		}
		
		//some mask areas may extend above the source image due to vertical offset.  We mark them black.
		for (unsigned int i = 0; i < _mask_info.size(); i++){
			if (_mask_info[i]->stats.pixel_count == 0) continue;
			int underlap_height(registration_offset.y - (int)_mask_info[i]->stats.y_min);
			//cerr << "R"<<i<< " underlap_height: " << underlap_height << "\n";
			if (underlap_height > 0){
				long width = (_mask_info[i]->stats.x_max-_mask_info[i]->stats.x_min+1)*_properties.components;
				long lines_sent = 0;

				while(lines_sent < underlap_height){
					long lines_to_send = underlap_height - lines_sent;
					if (lines_to_send > (long)_max_line_block_height)
						lines_to_send = (long)_max_line_block_height;

					ns_image_stream_buffer_properties bufp;
					bufp.height = lines_to_send;
					bufp.width = width;
					ns_image_stream_static_buffer<ns_component> * buf = _mask_info[i]->reciever.output_stream().provide_buffer(bufp);

					for (unsigned int y = 0; y < bufp.height; y++)
						for (unsigned int x = 0; x < bufp.width; x++)
							(*buf)[y][x] = 0;
					_mask_info[i]->reciever.output_stream().recieve_lines(*buf,lines_to_send);
					_mask_info[i]->lines_sent_to_reciever+=lines_to_send;
					lines_sent+=lines_to_send;
				}
				//cerr << "R"<<i<< " sent: " << lines_sent << "\n";
			}
		}
		return true;
	}
	
	void recieve_lines(const ns_image_stream_static_buffer<ns_component>  & lines, const unsigned long height){
		//update statistics for entire sample
		if (stats != 0){
			for (unsigned int y = 0; y < height; y++){
				for (unsigned int x = 0; x < lines.properties().width; x++){
					stats->histogram[lines[y][x]]++;
				}
			}
		}
		ns_image_stream_buffer_properties bufp;

			//on each line, look at each region to see if that region is present on the current line
		for (unsigned int i = 0; i < _mask_info.size(); i++) {
			if (_mask_info[i]->stats.pixel_count == 0)
				continue;

			for (long _y = 0; _y < (long)height; _y++) {
				if ((_y + y + registration_offset.y) < 0 || (_y + y + registration_offset.y) >= (long)mask->properties().height*(long)resize_factor)
					continue;
				if ((_y + y) >= (int)_mask_info[i]->stats.y_min - registration_offset.y
					&& (_y + y) <= (int)_mask_info[i]->stats.y_max - registration_offset.y) {

					bufp.width = (_mask_info[i]->stats.x_max - _mask_info[i]->stats.x_min + 1)*_properties.components;
					bufp.height = 1;
					ns_image_stream_static_buffer<ns_component> * buf = _mask_info[i]->reciever.output_stream().provide_buffer(bufp);

					unsigned char i_c = ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_properties.components,
						m_c = mask->properties().components;

					//if registration_offset.x < 0, we copy source[y][ -registration_offset.x -> width+registration_offset.x]
					// to destination[y][0 -> width+registration_offset.x]
					//
					//if registration_offset.x > 0, we copy source[y][ 0 -> width-registration_offset.x]
					// to destination[y][registration_offset.x -> width+registration_offset.x]
					//
					//everything else gets set to zero

					long source_x_offset(0),
						destination_x_offset(registration_offset.x);
					if (registration_offset.x > 0) {
						source_x_offset = -registration_offset.x;
						destination_x_offset = 0;
						for (int x = 0; x < registration_offset.x; x++) {
							(*buf)[0][x] = 0;
							_mask_info[i]->image_stats.histogram[0]++;
						}
					}
					//write out the current line, multipled by the mask
					for (unsigned int x = abs(registration_offset.x); x < bufp.width; x++) {

						//	(*buf)[0][x]  = lines[_y][((*mask_info)[i].stats.x_min+x)]* (ns_component)((*mask)[y+_y][(*mask_info)[i].stats.x_min+x] == i);
						(*buf)[0][x + destination_x_offset] =
							lines[_y][i_c*(_mask_info[i]->stats.x_min + source_x_offset) + x] *
							(ns_component)((*mask)[(y + _y + registration_offset.y) / resize_factor][(m_c*(_mask_info[i]->stats.x_min + destination_x_offset) + (x*m_c) / i_c) / resize_factor] == i);
						//update image statistics for each region
						_mask_info[i]->image_stats.histogram[(*buf)[0][x + destination_x_offset]]++;
					}
					if (registration_offset.x < 0) {
						for (int x = bufp.width + registration_offset.x; x < (long)bufp.width; x++) {
							(*buf)[0][x] = 0;
							_mask_info[i]->image_stats.histogram[0]++;
						}
					}
					_mask_info[i]->reciever.output_stream().recieve_lines(*buf, 1);
					_mask_info[i]->lines_sent_to_reciever++;
				}
			}
		}
	
		y+=height;
	}
	
	void finish_recieving_image(){
		ns_ex ex("ns_image_stream_mask_splitter::Incorrect number of lines sent to region: ");
		//cerr << "Finishing up!\n";
		//some masks may extend below the source image due to vertical offset.  We mark them black.
		for (unsigned int i = 0; i < _mask_info.size(); i++){
			if (_mask_info[i]->stats.pixel_count != 0){
				const long overlap_height((long)_mask_info[i]->stats.y_max - source_image_properties.height - (long)registration_offset.y + 1);

				if (overlap_height > 0){
				//	std::cerr << "R" << i << " sent: " << _mask_info[i]->lines_sent_to_reciever << "\n";
				//	std::cerr << (long)_mask_info[i]->stats.y_max << "-" << (long)_mask_info[i]->stats.y_min << "(" << (_mask_info[i]->stats.y_max - (long)_mask_info[i]->stats.y_min + 1) << "; " << _mask_info[i]->reciever_image_properties.height << " ) " << " y: " << y << " , siph" << source_image_properties.height << " r" << (long)registration_offset.y << "\n";
				//	std::cerr << "R" << i << " overlap: " << overlap_height << "\n";

					long width = (_mask_info[i]->stats.x_max-_mask_info[i]->stats.x_min+1)*_properties.components;
					long lines_sent = 0;
					while(lines_sent < overlap_height){
						long lines_to_send = overlap_height - lines_sent;
						if (lines_to_send > (long)_max_line_block_height)
							lines_to_send = (long)_max_line_block_height;

						ns_image_stream_buffer_properties bufp;
						bufp.height = lines_to_send;
						bufp.width = width;
						ns_image_stream_static_buffer<ns_component> * buf = _mask_info[i]->reciever.output_stream().provide_buffer(bufp);

						for (unsigned int y = 0; y < bufp.height; y++)
							for (unsigned int x = 0; x < bufp.width; x++)
								(*buf)[y][x] = 0;
						_mask_info[i]->reciever.output_stream().recieve_lines(*buf,bufp.height);
						lines_sent+=lines_to_send;
						_mask_info[i]->lines_sent_to_reciever+=lines_to_send;
					}
				}
			}
		}


		//close output files.
		for (unsigned int i = 0; i < _mask_info.size(); i++)
			if (_mask_info[i]->stats.pixel_count != 0)
					_mask_info[i]->reciever.output_stream().finish_recieving_image();

		//check for errors
		bool error(false);
		for (unsigned int i = 0; i < _mask_info.size(); i++){
		//	std::cerr << i << "\n";
			if (_mask_info[i]->lines_sent_to_reciever != _mask_info[i]->reciever_image_properties.height){
					error = true;
						ex << "Region "<< (int)i<< " was sent " <<  _mask_info[i]->lines_sent_to_reciever 
							<< " lines but it really needed " << _mask_info[i]->reciever_image_properties.height << " lines\n";
			}
		}
		if (error)
			throw ex;
	}

	void clear_heap(){
		_mask_info.clear_heap();
		in_buffer.resize(ns_image_stream_buffer_properties(0,0));
		//mask is ignored, as memory management is handled by mask creator.
	}

private:
	ns_image_properties source_image_properties;
	ns_image_mask_info<ns_component> _mask_info;
	ns_image_stream_static_buffer<ns_component> in_buffer;
	long y;

	unsigned long resize_factor;
	const ns_image_standard * mask;
	ns_image_statistics * stats;
	//sometimes masks are applied at a vertical offset from the original image
	//(for example, if image registration needs to be done.
	ns_vector_2i registration_offset;
};
#endif
