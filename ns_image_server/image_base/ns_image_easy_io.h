#ifndef NS_IMAGE_EASY_IO
#define NS_IMAGE_EASY_IO
#include "ns_image.h"
#include "ns_jpeg.h"
#include "ns_tiff.h"
#include "ns_ojp2k.h"
#include "ns_dir.h"

template<class whole_image>
void ns_load_image(const std::string & filename, whole_image & image){		
	std::string extension = ns_dir::extract_extension(filename);
	for (unsigned int i =0; i < extension.size(); i++)
		extension[i] = tolower(extension[i]);
	//open jpeg
	if (extension == "jpg"){
		ns_jpeg_image_input_file<typename whole_image::component_type> jpeg_in;
		jpeg_in.open_file(filename);
		ns_image_stream_file_source<typename whole_image::component_type> file_source(jpeg_in);
		file_source.pump(image,128);
		return;
	}
	//open tiff
	if (extension == "tif" || extension == "tiff"){
		ns_tiff_image_input_file<typename whole_image::component_type> tiff_in;
		tiff_in.open_file(filename);
		ns_image_stream_file_source<typename whole_image::component_type > file_source(tiff_in);
		file_source.pump(image,128);
		return;
	}
	if (extension == "jp2" || extension == "jpk"){
		ns_ojp2k_image_input_file<typename whole_image::component_type>  jp2k_in;
		jp2k_in.open_file(filename);
		ns_image_stream_file_source<typename whole_image::component_type > file_source(jp2k_in);
		file_source.pump(image,128);
		return;
	}
	throw ns_ex("ns_load_image::Unknown file extension: ") << extension;
}		

template<class ns_component>
void ns_save_image(const std::string & filename, const ns_image_whole<ns_component> & image, float compression_ratio = -1) {
	std::string extension = ns_dir::extract_extension(filename);
	//savejpeg
	if (extension == "jpg"){
		ns_jpeg_image_output_file<ns_component> jpeg_out;	
		if (compression_ratio == -1)
			compression_ratio = .8;
		ns_image_stream_file_sink<ns_component > file_sink(filename,jpeg_out,1024, compression_ratio);
		image.pump(file_sink,1024);
		return;
	}
	//save tiff
	if (extension == "tif" || extension == "tiff"){
		ns_tiff_compression_type t(ns_tiff_compression_zip);
		if (sizeof(ns_component) == 1)
			t = ns_tiff_compression_lzw;
		ns_tiff_image_output_file<ns_component> tiff_out(t);
		ns_image_stream_file_sink<ns_component> file_sink(filename,tiff_out,1024,1);
		image.pump(file_sink,1024);
		return;
	}
	if (extension == "jp2" || extension=="jpk"){
		ns_ojp2k_image_output_file<ns_component> jp2k_out;
		if (compression_ratio == -1)
			compression_ratio = .05;
		ns_image_stream_file_sink<ns_component> file_sink(filename,jp2k_out,1024, compression_ratio);
		image.pump(file_sink,1024); 
		return;
	}
	throw ns_ex("ns_save_image::Unknown file extension: ") << extension;
}


#endif
