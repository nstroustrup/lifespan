#ifndef NS_BUFFERED_RANDOM_ACCESS_IMAGE_H
#define NS_BUFFERED_RANDOM_ACCESS_IMAGE_H
#include "ns_image.h"

template<class ns_component>
class ns_image_buffered_random_access_output_image: public ns_image_stream_reciever<ns_image_stream_sliding_offset_buffer<ns_component> >{
public:

	typedef ns_component component_type;
	typedef ns_image_stream_sliding_offset_buffer<ns_component> storage_type;

	//initialize as an empty image
	ns_image_buffered_random_access_output_image(unsigned int max_height):ns_image_stream_reciever<ns_image_stream_sliding_offset_buffer<ns_component> >(0,this),
							lines_received(0),max_buffer_height(max_height),lines_flushed(0){}

	//deconstruction of buffer handled by buffer class

	//STREAM_RECIEVER ABILITIES
	//allocate buffer for entire image and prepare to recieve it
	bool init(const ns_image_properties & properties){
		bool resized(false);
		ns_image_stream_buffer_properties prop;
		prop.height = max_buffer_height;
		prop.width = properties.width*properties.components;

		//cerr << "Resizing buffer to " << prop.width << "," << prop.height << "\n";
		if ( !(image_buffer.properties() == prop)){
			if (prop.width != image_buffer.properties().width || prop.height != image_buffer.properties().height){
				image_buffer.resize(prop);
				resized = true;
			}
			ns_image_stream_reciever<ns_image_stream_sliding_offset_buffer<ns_component> >::_properties =  properties;
		}
		lines_received = 0;
		image_buffer.set_offset(0);
		return resized;
	}

	inline void resize(const ns_image_properties & properties){init(properties);}

	ns_image_stream_sliding_offset_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		image_buffer.set_offset(lines_received-lines_flushed);
		return &image_buffer;
	}


	//after the data is written to the provide_buffer() buffer, recieve_lines() is called.
	void recieve_lines(const ns_image_stream_sliding_offset_buffer<ns_component> & lines, const unsigned long height){
		lines_received+=height;
	}

	//when entire image is loaded, there is nothing to do.
	void finish_recieving_image(){image_buffer.set_offset(0);}

	const inline ns_image_properties & properties() const{
			return ns_image_stream_reciever<ns_image_stream_sliding_offset_buffer<ns_component> >::_properties;
	}

	template<class buff2>
	void init_flush_recipient(ns_image_stream_reciever<buff2> & reciever){
		reciever.prepare_to_recieve_image(properties());
	}

	template<class reciever_t>
	void flush_buffer(unsigned int lines,reciever_t & reciever){

	//	image_buffer.set_offset(0);
		ns_image_stream_buffer_properties prop;
		prop.width = image_buffer.properties().width;
		prop.height = lines;
		reciever.output_buffer = reciever.provide_buffer(prop);
		for (unsigned int y = 0; y < lines; y++)
			for (unsigned int x = 0; x < prop.width; x++)
				(*reciever.output_buffer)[y][x] = image_buffer[y+lines_flushed][x];
//		image_buffer.step(lines);
		lines_flushed+=lines;
		
//		unsigned long off(lines_flushed%image_buffer.properties().height);
//		image_buffer.set_offset(-off);
		reciever.recieve_lines(*reciever.output_buffer,prop.height);
	}

	///random pixel access
	const inline ns_component * operator[](const unsigned long y)const{
	//	if (y >= max_buffer_height)
	//		throw ns_ex("Yikes!");
		return image_buffer[y];
	}
	///random pixel access
	inline ns_component * operator[](const unsigned long y){
	//	if (y >= max_buffer_height)
	//		throw ns_ex("Yikes!");
		return image_buffer[y];
	}

	void clear(){
		resize(ns_image_properties(0,0,1,0));
	}
	void reset_max_buffer_height(const unsigned long height) {
		ns_image_stream_buffer_properties p(image_buffer.properties());
		p.height = height;
		image_buffer.resize(p);
		lines_flushed = 0;
		lines_received = 0;
		max_buffer_height = height;
	}
private:
	ns_image_stream_sliding_offset_buffer<ns_component> image_buffer;
	long lines_flushed;
	unsigned long lines_received;
	unsigned long max_buffer_height;

};


template<class ns_component,class image_source_t>
class ns_image_buffered_random_access_input_image{
public:

	typedef ns_component component_type;
	typedef ns_image_stream_static_buffer<ns_component> storage_type;

	//initialize as an empty image
	ns_image_buffered_random_access_input_image(unsigned int max_height):buffer_top(0),max_buffer_height(max_height),image_source(0),cur_buffer_height(0){}


	inline void resize(const ns_image_properties & properties){init(properties);}

	inline void seek_to_beginning(){image_source->seek_to_beginning(); assign_buffer_source(*image_source);}

	const inline ns_image_properties & properties() const{
			return _properties;
	}

	void assign_buffer_source(image_source_t & sender_){
		image_source =&sender_;
		init(sender_.properties());
	}

	///random pixel access
	inline ns_component * operator[](const unsigned long y){
		make_line_available(y);
		return image_buffer[y-(buffer_top-cur_buffer_height)];
	}

	void clear(){
		image_source = 0;
		resize(ns_image_properties(0,0,1,0));
	}
private:
	ns_image_stream_static_buffer<ns_component> image_buffer;
	unsigned long buffer_top;
	unsigned long max_buffer_height;
	unsigned long cur_buffer_height;
	image_source_t * image_source;
	ns_image_properties _properties;

	void make_line_available(const unsigned long line_num){
		while(line_num >= buffer_top){
			//cerr << "Requested line " << line_num;
			//always load as much as possible into the buffer 
			long lines_to_load = (long)_properties.height - (long)buffer_top; 
			if (lines_to_load <= 0)
				throw ns_ex("Invalid line access requested: ") << line_num;
			if ((unsigned long)lines_to_load > max_buffer_height)
				lines_to_load = max_buffer_height;
			image_source->send_lines(image_buffer, lines_to_load);
			buffer_top+=lines_to_load;
			cur_buffer_height = lines_to_load;
			//cerr << ", Loaded line up to " << buffer_top << "\n";
		}
	}

	void init(const ns_image_properties & properties){
		ns_image_stream_buffer_properties prop;
		prop.height = max_buffer_height;
		if (properties.width == 0)
			prop.height = 0;
		prop.width = properties.width*properties.components;

		//cerr << "Resizing buffer to " << prop.width << "," << prop.height << "\n";
		if ( !(image_buffer.properties() == prop)){
			image_buffer.resize(prop);
			_properties =  properties;
		}
	//	if (sender != 0)
		//	sender->send_lines(image_buffer, max_buffer_height);
		buffer_top = 0;
		cur_buffer_height = 0;
	}

};

template<class ns_component, class image_source_t>
class ns_image_buffered_multi_line_random_access_input_image : public ns_image_stream_sender<ns_component, ns_image_buffered_multi_line_random_access_input_image<ns_component, image_source_t>, unsigned long >{
public:

	typedef ns_component component_type;
	typedef ns_image_stream_static_offset_buffer<ns_component> storage_type;

	//initialize as an empty image
	ns_image_buffered_multi_line_random_access_input_image():buffer_bottom(0),total_buffer_height(0),previous_lines_required(0),image_source(0),lines_received(0),ns_image_stream_sender<ns_component,ns_image_buffered_multi_line_random_access_input_image<ns_component, image_source_t>, unsigned long >(ns_image_properties(0,0,0),this){}
	

	inline void resize(const ns_image_properties & properties){init(properties);}

	internal_state_t seek_to_beginning(){
		buffer_bottom = 0;
		lines_received = 0;
		sender_internal_state = image_source->seek_to_beginning(); 
		assign_buffer_source(*image_source,previous_lines_required,total_buffer_height);
		return 0;
	}

	const inline ns_image_properties & properties() const{
			return _properties;
	}

	void assign_buffer_source(image_source_t & sender_,const unsigned int previous_lines_required_,const unsigned int total_buffer_height_){
		previous_lines_required = previous_lines_required_;
		total_buffer_height = total_buffer_height_;

		image_source =&sender_;
		init(sender_.properties());
	}

	///random pixel access
	inline ns_component * operator[](const unsigned long y){
		//make_line_available(y);
		//subtracting -buffer_bottom implicit in offset_buffer setting
		return image_buffer[y];
	}	
	inline ns_component * safe_access(const unsigned long y){
		make_line_available(y);
		return image_buffer[y];
	}

	void clear(){
		image_source = 0;
		resize(ns_image_properties(0,0,1,0));
	}

	 void make_line_available(const unsigned long line_num){  //this can't be const, as we modify not just the internal buffer but the state of the file handle for the disk buffer.
		if (line_num < lines_received)
			return;
		//calculate how much buffer we'll need to retain
		long new_bottom = (long)line_num - (long)previous_lines_required;
		if (new_bottom < 0) new_bottom = 0;

		long lines_to_copy((long)lines_received - new_bottom);
		if (lines_to_copy < 0) lines_to_copy = 0;
		//copy the new bottom of the buffer over--we may need to retain a certain number of lines
		image_buffer.set_offset(0);
		for (long y = 0; y < lines_to_copy; y++){
			for (unsigned long x = 0; x < image_buffer.properties().width; x++){
				image_buffer[y][x] = image_buffer[y+new_bottom-buffer_bottom][x];
			}
		}

		//now obtain new lines and place them in buffer at desired offset
		image_buffer.set_offset(lines_to_copy);
		//fill up the buffer
		long lines_to_load = (long)total_buffer_height - lines_to_copy; 
		if (lines_to_load <= 0)
			throw ns_ex("Invalid line access requested: ") << line_num;
		if (lines_to_load+lines_received > _properties.height)
			lines_to_load = _properties.height - lines_received;
		
		image_source->send_lines(image_buffer, lines_to_load, sender_internal_state);
		buffer_bottom = new_bottom;
		lines_received+=lines_to_load;
		
		image_buffer.set_offset(-(long)buffer_bottom);
	}

	//sender functions
	internal_state_t init_send(){
		lines_sent = 0;
		sender_internal_state = seek_to_beginning();
		return 0;
	}
	internal_state_t init_send_const() const {
		throw ns_ex("Invalid use of const");
	}

	template<class write_buffer>
	void send_lines(write_buffer & lines, const unsigned int count,internal_state_t & unused_state){
		unsigned long to_send = count;
		if (count + lines_sent > ns_image_stream_sender<ns_component,ns_image_buffered_multi_line_random_access_input_image<ns_component,image_source_t>, internal_state_t>::_properties.height)
			to_send = ns_image_stream_sender<ns_component,ns_image_buffered_multi_line_random_access_input_image<ns_component, image_source_t>,internal_state_t >::_properties.height - lines_sent;

		for (unsigned long y = 0; y < to_send; y++){
			this->make_line_available(y+lines_sent);
			//memcpy(lines[y],image_buffer[y+lines_sent],sizeof(ns_component)*lines.properties().width);
			for (unsigned int x = 0; x < lines.properties().width; x++){
				lines[y][x] = image_buffer[y+lines_sent][x];
				//cout << x << " ";
			}
			//cout << y+lines_sent << ":\n";
		}

		lines_sent+=count;
	}


private:
	internal_state_t sender_internal_state;

	unsigned long lines_sent;
	storage_type image_buffer;
	//the maximum number of lines previous to the one most recently requested that the buffer will retain
	unsigned long previous_lines_required;
	//the total height of the buffer allocated
	unsigned long total_buffer_height;
	//current position (relative to the sender) of the local buffer
	unsigned long buffer_bottom;
	//the total number of lines read from the sender
	unsigned long lines_received;
	image_source_t * image_source;
	//total size of the image being read from the sender
	ns_image_properties _properties;

	

	void init(const ns_image_properties & properties){
		
		ns_image_stream_buffer_properties prop;
		prop.height = total_buffer_height;
		if (properties.width == 0)
			prop.height = 0;
		prop.width = properties.width*properties.components;

		//cerr << "Resizing buffer to " << prop.width << "," << prop.height << "\n";
		if ( !(image_buffer.properties() == prop)){
			image_buffer.resize(prop);
			_properties =  properties;
		}
		buffer_bottom = 0;
		lines_received = 0;
		
		ns_image_stream_sender<ns_component,ns_image_buffered_multi_line_random_access_input_image<ns_component, image_source_t>, internal_state_t>::_properties = properties;
	}


};

#endif
