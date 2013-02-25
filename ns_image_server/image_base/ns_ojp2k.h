#ifndef NS_OJP2K
#define NS_OJP2K
#include "ns_image.h"

struct ns_ojp2k_initialization{
	static void init();
	static void cleanup();
	static bool jp2k_initialized;
};

//ns_8_bit_jp2k_image_input_file contains all the code to interact 
//with the JASPER jppeg2000 library
//It is encapsulated as its own (non-templated) class so that
//the implemenentation (which requires the inclusion of the JASPER header file
//can be kept in a separate .cpp file.  The JASPER header contains a bunch of
//of #define statments that conflict with other libraries inlcuding mysql
struct ns_jp2k_data;
class ns_8_bit_ojp2k_image_input_file{
protected:
	ns_jp2k_data * data;
	int jas_matrix_width,jas_matrix_height;
	unsigned long lines_read;
	ns_image_properties properties;
public:
	ns_8_bit_ojp2k_image_input_file():lines_read(0),data(0){ ns_ojp2k_initialization::init();}
	
	void open_file_i(const std::string & filename);
	void close_i();

	bool read_line_i(ns_8_bit * buffer);
	bool read_line_i(ns_16_bit * buffer)
		{throw ns_ex("ns_8_bit_jp2k_image_input_file()::16 bit images not implemented");}

	const unsigned int read_lines_i(ns_8_bit ** buffer,const unsigned int n);
	const unsigned int read_lines_i(ns_16_bit ** buffer,const unsigned int n)
		{throw ns_ex("ns_8_bit_jp2k_image_input_file()::16 bit images not implemented");}

};


template<class ns_component>
class ns_ojp2k_image_input_file: public ns_image_input_file<ns_component>, private ns_8_bit_ojp2k_image_input_file{
	
public:
	//~ns_image_input_file(){close();}
	//open and close files

	~ns_ojp2k_image_input_file(){close();}

	void open_file(const std::string & filename){ open_file_i(filename);ns_image_input_file<ns_component>::_properties = ns_8_bit_ojp2k_image_input_file::properties;}
	void open_mem(const void *)
		{throw ns_ex("ns_jp2k_image_input_file::Opening from memory is not supported.")<< ns_file_io;}
	
	void close(){close_i();}

	void seek_to_beginning(){
		lines_read = 0;
	}
	//read in a single line
	bool read_line(ns_component * buffer){return read_line_i(buffer);}

	//read a position/component from the specified buffer.
	ns_component * operator()(const unsigned long x, const  unsigned int component, ns_component * buffer)const{		
		return &buffer[ns_image_input_file<ns_component>::_properties.components*x+component];
	}
	//read in multiple lines
	const unsigned int read_lines(ns_component ** buffer,const unsigned int n){return read_lines_i(buffer,n);}

};

//ns_8_bit_jp2k_image_output_file contains all the code to interact 
//with the JASPER jppeg2000 library
//It is encapsulated as its own (non-templated) class so that
//the implemenentation (which requires the inclusion of the JASPER header file
//can be kept in a separate .cpp file.  The JASPER header contains a bunch of
//of #define statments that conflict with other libraries inlcuding mysql
struct ns_jp2k_output_data;
class ns_8_bit_ojp2k_image_output_file{	
protected:
	
	ns_jp2k_output_data * data;
	ns_image_properties properties;
	unsigned long lines_written;

public:
	ns_8_bit_ojp2k_image_output_file():lines_written(0),data(0){
		ns_ojp2k_initialization::init();
	}
	void open_file_i(const std::string & filename, const ns_image_properties & properties);

	void close_i();
		
	//write a single line
	bool write_line_i(const ns_8_bit * buffer);
	
	bool write_line_i(const ns_16_bit * buffer)
		{throw ns_ex("ns_8_bit_jp2k_image_output_file()::16 bit images not implemented");}

	void write_lines_i(const ns_8_bit ** buffer,const unsigned int n);
	
	void write_lines_i(const ns_16_bit ** buffer,const unsigned int n)
		{throw ns_ex("ns_8_bit_jp2k_image_output_file()::16 bit images not implemented");}

};

template<class ns_component>
class ns_ojp2k_image_output_file: public ns_image_output_file<ns_component>, private ns_8_bit_ojp2k_image_output_file{
public:


	~ns_ojp2k_image_output_file(){
		close();
	}

	void open_file(const std::string & filename, const ns_image_properties & properties){open_file_i(filename,properties); ns_image_output_file<ns_component>::_properties = properties;}

	void open_mem(const void * v, const ns_image_properties & properties){
		throw ns_ex("ns_jp2k_output_file::Opening from memory is not supported.")<< ns_file_io;
	}

	void close(){close_i();}
		
	//write a single line
	bool write_line(const ns_component * buffer){return write_line_i(buffer);}
	void write_lines(const ns_component ** buffer,const unsigned int n){write_lines(buffer,n);}

	//read a position/component from the specified buffer.
	virtual ns_component * operator()(const unsigned long x, const unsigned int component, ns_component * buffer)const{
			return &buffer[ns_image_output_file<ns_component>::_properties.components*x+component];
	}
};

#endif
