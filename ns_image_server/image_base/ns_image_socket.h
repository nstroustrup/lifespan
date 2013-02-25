/*
*NOTE:  These classes should be able to send images with different bit depths.
*However, if you need to *match* template instantiations for each bit depth on
*both ends of a socket.  if a ns_image_socket_reciever<ns_8_bit> sends an 8 bit image
*to a ns_image_socket_sender<ns_16_bit>, bad things will happen.
*/

#ifndef NS_IMAGE_SOCKET_H
#define NS_IMAGE_SOCKET_H

#include "ns_image.h"
#include "ns_socket.h"

///returns the size(in bytes) of a ns_image_properties object
unsigned int ns_image_properties_length_as_char();
///converts an ns_image_properties object to a series of characters, in preparation
///for sending it over a socket
unsigned int ns_image_properties_to_char(const ns_image_properties & p, char *& buf);

///convers the specified character buffer into an ns_image_properties object
///after transmission over a socket
ns_image_properties ns_char_to_image_properties(const char * buf,const int header_length);


#pragma warning(disable: 4355)
///Sends an image stream over a socket
///Cross-platform implemtations using both windows and linux sockets
template<class ns_component>
class ns_image_socket_sender: public ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >{
public:
	
	ns_image_socket_sender(const long max_line_block_height)
		:socket_connection(0),byte_resize_factor(sizeof(ns_component)/sizeof(char)),ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >(max_line_block_height,this){}
	#pragma warning(default: 4355)

	void bind_socket(ns_socket_connection & connection){
		socket_connection = &connection;

	}

	///prepare to send an image over the socket.
	bool init(const ns_image_properties & properties){
		if (socket_connection == 0)
			throw ns_ex("ns_image_socket_reciever::Attempting to send an image before a socket was specified!");

		ns_image_stream_buffer_properties bprop;
		bprop.height = ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_max_line_block_height;
		bprop.width = properties.width*properties.components;
		bool resized(buffer.resize(bprop));

		//inform the reciever of the size of the image.
		char * buf;
		int len = ns_image_properties_to_char(properties, buf);
		
		socket_connection->write(reinterpret_cast<char *>(&len), sizeof(len));
		socket_connection->write(buf, len);
		delete buf;
		my_lines_sent = 0;
		return resized;
	}
	
	ns_image_stream_static_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & properties){
		return &buffer;
	}
	void recieve_lines(const ns_image_stream_static_buffer<ns_component> & buffer, const unsigned long height){
		//note that if we're sending bytes other than 8 bit chars, we'll recast the pointer
		//and send correspondinly more bits due to byte_resize_factor.

		//bitmaps have a template specialization to handle them.
		//cerr << "Sending line " << height <<" lines...\n";
		//XXX
		for (unsigned int i = 0; i < height; i++){
			//cerr << "Sending " << buffer.properties().width*byte_resize_factor << " bytes.\n";
			socket_connection->write(reinterpret_cast<const char *>(buffer[i]), buffer.properties().width*byte_resize_factor);
		}
		my_lines_sent += height;
		//cerr << my_lines_sent << " lines sent.\n";
		//XXX
	}
	void finish_recieving_image(){}
protected:
	ns_socket_connection * socket_connection;
	ns_image_stream_static_buffer<ns_component> buffer;
		//if we're sending images whose size are larger than a single byte, we'll need to send a correspondingly
		//larger number of bytes per line.
	char byte_resize_factor;
	unsigned long my_lines_sent;
};

//specialization for bitmaps
/*template<class read_buffer>
void ns_image_socket_reciever<ns_bit,read_buffer>::recieve_lines(const read_buffer & buffer, const unsigned long height){
	throw ns_ex("ns_image_socket_reciever::No template specialization for sending bools yet.");
}*/

#pragma warning(disable:4355)
///Recieves an image stream transmitted over a socket.
///Cross-platform implementation using both windows and linux sockets
template<class ns_component>
class ns_image_socket_reciever : public ns_image_stream_sender< ns_component, ns_image_socket_reciever< ns_component > >{
public:
	
	ns_image_socket_reciever():socket_connection(0),byte_resize_factor(sizeof(ns_component)/sizeof(char)),ns_image_stream_sender<ns_component, ns_image_socket_reciever<ns_component> >(ns_image_properties(0,0,0), this){}
	
#pragma warning(default:4355)
	void bind_socket(ns_socket_connection & connection){
		socket_connection = &connection;
	}

	void init_send(){
		if (socket_connection == 0)
			throw ns_ex("ns_image_socket_sender::Attempting to recieve an image before a socket was specified!");

		//retrieve the image size from the socket
		int header_length(0);
		if (sizeof(header_length) != socket_connection->read(reinterpret_cast<char *>(&header_length),sizeof(header_length))){
			throw ns_ex("Read improper length header!");
		}

		char * buf = new char[header_length];
		socket_connection->read(buf,header_length);
		ns_image_stream_sender< ns_component, ns_image_socket_reciever< ns_component > >::_properties = ns_char_to_image_properties(buf,header_length);
		delete buf;

		ns_image_stream_buffer_properties bprop;
		bprop.height = 512;
		bprop.width = ns_image_stream_sender< ns_component, ns_image_socket_reciever< ns_component > >::_properties.width*ns_image_stream_sender< ns_component, ns_image_socket_reciever< ns_component > >::_properties.components;
		buffer.resize(bprop);
		my_lines_recieved = 0;

	}
	template<class write_buffer>
	 void send_lines(write_buffer & lines, unsigned int count){
		//recieve in chunks of 512 lines
		unsigned long lines_recieved_so_far = 0;
		while (lines_recieved_so_far < count){
			unsigned long lines_to_recieve = count - lines_recieved_so_far;
			if (lines_to_recieve > 512)
				lines_to_recieve = 512;

			char * buf;
			for (unsigned int i = 0; i < lines_to_recieve; i++){
				buf = reinterpret_cast<char *>(lines[i]);
				//cerr << "Recieving " << buffer.properties().width*byte_resize_factor << " bytes.\n";
				socket_connection->read(&buf[lines_recieved_so_far],buffer.properties().width*byte_resize_factor);
			}
			lines_recieved_so_far += lines_to_recieve;
		}
		my_lines_recieved += count;
		//std::cerr << "Recieved " << my_lines_recieved << " lines.\n";
		//XXX
	}

	ns_image_stream_static_buffer<ns_component> buffer;
	ns_socket_connection * socket_connection;
	char byte_resize_factor;
	unsigned long my_lines_recieved;
};

//specialization for bitmaps
/*template<class write_buffer>
void ns_image_socket_sender<ns_bit,write_buffer>send_lines(write_buffer & lines, unsigned int count){
	throw ns_ex("ns_image_socket_reciever::No template specialization for recieving bools yet.");
}*/

#endif
