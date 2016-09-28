#ifndef NS_JPEG
#define NS_JPEG

#include "ns_image.h"
#include <stdio.h>
#include "jpeglib.h"
#include <iostream>
#include <string>

///
///All objects using the global jpeg engine use ns_jpeg_library_user to coordinate usage
///
class ns_jpeg_library_user{
	protected:
	jpeg_error_mgr & error_manager(){if (!init_)init();return error_manager_;}
	
	private:
		static jpeg_error_mgr error_manager_;
		static void init();
		static bool init_;
};


///
///decodes image information from jpeg files
///
template<class ns_component>
class ns_jpeg_image_input_file: public ns_image_input_file<ns_component>, private ns_jpeg_library_user{
public:

	ns_jpeg_image_input_file():fp(0),_open(false){cinfo.err = &error_manager();}

	void open_file(const std::string & filename);
	void open_mem(const void *);
	void close();

	unsigned long seek_to_beginning() { close_file(); open_file(open_filename); return 0; }

	//read in a single line
	bool read_line(ns_component * buffer);
	const unsigned int read_lines(ns_component ** buffer, const unsigned int n);
	~ns_jpeg_image_input_file(){ try{if (_open) close();}catch(...){std::cerr << "~ns_jpeg_image_input_file() attempted to throw an exception!";}}

	ns_component * operator()(const unsigned long x, const unsigned int component, ns_component * buffer) const{return &(buffer[x*ns_image_input_file<ns_component>::_properties.components + component]);}
	J_COLOR_SPACE color_space(){return _color_space;}
private:
	void close_file();
	void start_decompression();

	FILE *fp;
	mutable jpeg_decompress_struct cinfo;
	bool _open;
	J_COLOR_SPACE _color_space;
	std::string open_filename;
};
///
///encodes image information and writes to a jpeg file
///
template<class ns_component>
class ns_jpeg_image_output_file : public ns_image_output_file<ns_component>, private ns_jpeg_library_user{
public:
	ns_jpeg_image_output_file():fp(0),_open(false){cinfo.err = &error_manager();}	
	
	void open_file(const std::string & filename, const ns_image_properties & properties);
	void open_mem(const void *, const ns_image_properties & properties);
	void close();

	bool write_line(const ns_component * buffer);
	void write_lines(const ns_component ** buffer,const unsigned int n);

	ns_component * operator()(const unsigned long x, const unsigned int component, ns_component * buffer)const {return &(buffer[x*ns_image_output_file<ns_component>::_properties.components + component]);}
	~ns_jpeg_image_output_file(){
		try{
			if (_open) 
				close();
		
		}
		catch(...){
			std::cerr << "~ns_jpeg_image_output_file() attempted to throw an exception!";
		}
	}

private:
	void start_decompression(const ns_image_properties & properties);

	FILE *fp;
	mutable jpeg_compress_struct cinfo;
	bool _open;
};


//***********************ns_jpeg_image_input_filemember functions****************************
template<class ns_component>
void ns_jpeg_image_input_file<ns_component> ::open_file(const std::string & filename){

	fp = fopen(filename.c_str(), "rb");
	if (fp == NULL)
		throw ns_ex() << ns_file_io << "Could not open file " << filename << "\n";
	start_decompression();
}

template<class ns_component>
void ns_jpeg_image_input_file<ns_component> ::open_mem(const void *){
	throw ns_ex() << ns_file_io << "ns_jpeg_image_input_file::Opening from memory is not supported.";
}

template<class ns_component>
void ns_jpeg_image_input_file<ns_component> ::close(){
	open_filename.clear();
	close_file();
}
template<class ns_component>
void ns_jpeg_image_input_file<ns_component> ::close_file(){
	if (_open){
    	_open = false;
		try{
    		jpeg_finish_decompress(&cinfo);
   	 		jpeg_destroy_decompress(&cinfo);
			if (fp != 0){
				fclose(fp);
				fp = 0;
			}
		}
		catch(ns_ex & ex){
			std::cerr << "ns_jpeg_image_input_file<ns_component>::close_file() wants to throw an exception but cannot : " << ex.text();
		}
		catch(std::exception & e){
			ns_ex ex(e);
			std::cerr << "ns_jpeg_image_input_file<ns_component>::close_file() wants to throw an exception but cannot : " << ex.text();
		}
	}
}



template<class ns_component>
bool ns_jpeg_image_input_file<ns_component> ::read_line(ns_component * buffer){
	int lines_read = 0;
	ns_component * buf[1];
	buf[0] = buffer;
	while (lines_read != 1){
		lines_read  = jpeg_read_scanlines(&cinfo, buf,1);
	}
	return true;
}
template<>
bool ns_jpeg_image_input_file<ns_16_bit> ::read_line(ns_16_bit * buffer);

template<>
bool ns_jpeg_image_input_file<float> ::read_line(float * buffer);

template<class ns_component>
const unsigned int  ns_jpeg_image_input_file<ns_component> ::read_lines(ns_component ** buffer, const unsigned int n){
	unsigned int lines_read = 0;
	unsigned int left;
	while (lines_read < n)
		lines_read  += jpeg_read_scanlines(&cinfo, &(buffer[lines_read]),n - lines_read);
	return lines_read;
}

template<class ns_component>
void ns_jpeg_image_input_file<ns_component> ::start_decompression(){
	//std::cout << "Creating Decompression." << std::endl;
	jpeg_create_decompress(&cinfo);
	//std::cout << "Initializing Stdio" << std::endl;
	jpeg_stdio_src(&cinfo, fp);

	//std::cout << "Reading Header" << std::endl;
	jpeg_read_header(&cinfo, TRUE);
	//std::cout << "Starting Decompression" << std::endl;
	jpeg_start_decompress(&cinfo);
	
	_open = true;

	ns_image_input_file<ns_component>::_properties.width = cinfo.output_width;
	ns_image_input_file<ns_component>::_properties.height = cinfo.output_height;
	ns_image_input_file<ns_component>::_properties.components = cinfo.output_components;
	_color_space = cinfo.out_color_space;
}


//***********************ns_jpeg_image_output_file member functions****************************
template<class ns_component>
void ns_jpeg_image_output_file<ns_component>:: open_file(const std::string & filename, const ns_image_properties & properties){
	fp = fopen(filename.c_str(), "wb");
	if (fp == NULL){
		throw ns_ex("Could not open output file ") << ns_file_io << filename << "\n";
	}
	_open = true;
	start_decompression(properties);
}

template<class ns_component>
void ns_jpeg_image_output_file<ns_component>:: open_mem(const void *, const ns_image_properties & properties){
	throw ns_ex("ns_jpeg_image_output_file::Opening from memory is not supported.") << ns_file_io;
}

template<class ns_component>
bool ns_jpeg_image_output_file<ns_component>:: write_line(const ns_component * buffer){
	ns_8_bit * buf[1] = {reinterpret_cast<ns_8_bit *>(const_cast<ns_component *>(buffer))};
    jpeg_write_scanlines(&cinfo, buf, 1);
	return true;
}

template<class ns_component>
void ns_jpeg_image_output_file<ns_component>::write_lines(const ns_component ** buffer, const unsigned int n){
    unsigned int written = 0;
	ns_8_bit ** buf = reinterpret_cast<ns_8_bit **>(const_cast<ns_component **>(buffer));
	while (written < n)
		written += jpeg_write_scanlines(&cinfo, &(buf[written]), n);
}

template<class ns_component>
void ns_jpeg_image_output_file<ns_component>:: close(){
	if (_open){
		_open = false;
		try{
			jpeg_finish_compress(&cinfo);
			jpeg_destroy_compress(&cinfo);
		}
		catch(ns_ex & ex){
			std::cerr << "ns_jpeg_image_input_file<ns_component>::close_file() wants to throw an exception but cannot : " << ex.text();
		}
		catch(std::exception e){
			ns_ex ex(e);
			std::cerr << "ns_jpeg_image_input_file<ns_component>::close_file() wants to throw an exception but cannot : " << ex.text();
		}
		catch(...){
			std::cerr << "ns_jpeg_image_input_file<ns_component>::close_file()::Unknown exception\n";
		}
	}
	if (fp != 0){
		fclose(fp);
		fp = 0;
	}
}


template<class ns_component>
void ns_jpeg_image_output_file<ns_component>:: start_decompression(const ns_image_properties & properties){
	ns_image_output_file<ns_component>::_properties = properties;
	//std::cout << "Starting compression encoder" << std::endl;
	//set up output encoder
	jpeg_create_compress(&cinfo);
	//std::cout << "Starting compression output." << std::endl;
	jpeg_stdio_dest(&cinfo, fp);
	//std::cout << "Set Destination" << std::endl;
	cinfo.image_width = properties.width;
	cinfo.image_height = properties.height;
	cinfo.input_components = properties.components;
	if (properties.components == 1)
		cinfo.in_color_space = JCS_GRAYSCALE;
	else if (properties.components == 3)
		cinfo.in_color_space = JCS_RGB;
	else throw ns_ex() << ns_file_io << "JPEG requested with an unknown number of components: " << properties.components;

	//std::cout << "Setting Defaults" << std::endl;
	jpeg_set_defaults (&cinfo);
	jpeg_set_quality(&cinfo,(int)(properties.compression*100),TRUE);
	//std::cout << "Starting Compression" << std::endl;
	jpeg_start_compress(&cinfo, TRUE);
}



#endif
