#include "ns_jpeg.h"
using namespace std;

jpeg_error_mgr ns_jpeg_library_user::error_manager_;

bool ns_jpeg_library_user::init_(false);

METHODDEF(void) ns_jpeg_output_message (j_common_ptr cinfo){
	char buffer[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message) (cinfo, buffer);
	if (ns_jpeg_library_user::verbose_output)
		cerr << "ns_jpeg::jpeglib message: " << buffer << "\n";
	
}

METHODDEF(void) ns_jpeg_error_exit (j_common_ptr cinfo){
	char buffer[JMSG_LENGTH_MAX];
	(*cinfo->err->format_message) (cinfo, buffer);
	if (ns_jpeg_library_user::verbose_output)
	std::cerr << "ns_jpeg::jpeglib error: " << buffer << "\n";
}
bool ns_jpeg_library_user::verbose_output = true;

void ns_jpeg_library_user::init(){
	jpeg_std_error(&error_manager_);
	//throw exceptions and write to stdout rather than the libjpeg's default of calling exit()
	error_manager_.error_exit = ns_jpeg_error_exit;
	error_manager_.output_message = ns_jpeg_output_message;
}

template<>
bool ns_jpeg_image_input_file<ns_16_bit,false> ::read_line(ns_16_bit * buffer){
	throw ns_ex("ns_jpeg_image_input_file::16-bit jpeg not supported.");
}

template<>
bool ns_jpeg_image_input_file<float, false> ::read_line(float * buffer) {
	throw ns_ex("ns_jpeg_image_input_file::floating point jpeg not supported.");

}
template<>
bool ns_jpeg_image_input_file<ns_16_bit,true> ::read_line(ns_16_bit * buffer) {
	throw ns_ex("ns_jpeg_image_input_file::16-bit jpeg not supported.");
}

template<>
bool ns_jpeg_image_input_file<float, true> ::read_line(float * buffer) {
	throw ns_ex("ns_jpeg_image_input_file::floating point jpeg not supported.");

}
