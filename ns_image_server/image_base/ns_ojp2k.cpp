#include "ns_ojp2k.h"
#include <stdio.h>

#include "ns_xmp_encoder.h"
#include "openjpeg.h"
#include <fstream>


void ns_ojp2k_setup_output(const ns_image_properties & properties,const std::string & filename, const unsigned long rows_per_strip , ns_jp2k_output_data * data) {

	if (properties.height == 0)
		throw (ns_ex("Cannot create an image with 0 height!"));
	if (properties.width == 0)
		throw (ns_ex("Cannot create an image with 0 width!"));
	if (properties.components == 0)
		throw (ns_ex("Cannot create an image with 0 pixel components!"));

	

	if (properties.compression == 1)
		data->parameters.irreversible = 0;
	else data->parameters.irreversible = 1;
	data->parameters.cblockw_init = 64;
	data->parameters.cblockh_init = 64;

	data->parameters.cp_tx0 = 0;
	data->parameters.cp_ty0 = 0;
	data->parameters.tile_size_on = OPJ_TRUE;
	data->parameters.cp_tdx = properties.width;
	data->parameters.cp_tdy = rows_per_strip;

	data->parameters.tcp_numlayers = 1;
	data->parameters.tcp_rates[0] = 1.0 / properties.compression;

	data->parameters.cp_disto_alloc = 1;

	opj_image_comptparm prm;
	prm.dx = prm.dy = 1;
	prm.w = properties.width;
	prm.h = properties.height;
	prm.x0 = prm.y0 = 0;
	prm.prec = 8;
	prm.bpp = 8;
	prm.sgnd = 0;

	OPJ_COLOR_SPACE color_space((properties.components == 3) ? OPJ_CLRSPC_SRGB : OPJ_CLRSPC_GRAY);
	data->image = opj_image_create(properties.components, &prm, color_space);
	if (data->image == 0)
		throw ns_ex("openjpeg::Could not create image!");

	/* get a JP2 compressor handle */
	data->codec = opj_create_compress(OPJ_CODEC_JP2);
	if (data->codec == 0)
		throw ns_ex("ns_opj2k::Could not create codec");

	/* catch events using our callbacks and give a local context */
	opj_set_info_handler(data->codec, ns_jp2k_info_callback, data);
	opj_set_warning_handler(data->codec, ns_jp2k_warning_callback, data);
	opj_set_error_handler(data->codec, ns_jp2k_error_callback, data);


	/* setup the encoder parameters using the current image and using user parameters */
	if (!opj_setup_encoder(data->codec, &data->parameters, data->image))
		throw ns_ex("openjpeg::Could not set up encoder.");

	/* open a byte stream for writing */
	/* allocate memory for all tiles */
	data->stream = opj_stream_create_default_file_stream(filename.c_str(), false);
	if (data->stream == 0)
		throw ns_ex("openjpeg: could not open file ") << filename;


	if (!opj_start_compress(data->codec, data->image, data->stream))
		throw ns_ex("openjpeg: could not start compression");



	ns_xmp_tiff_data xmp_tiff_data;
	xmp_tiff_data.orientation = 1;
	xmp_tiff_data.xresolution = properties.resolution;
	xmp_tiff_data.yresolution = properties.resolution;
	xmp_tiff_data.resolution_unit = 2;

	std::string xmp_output;
	ns_long_string_xmp_encoder::write(properties.description, xmp_output, &xmp_tiff_data);
	std::string xmp_filename(ns_oj2k_xmp_filename(filename));
	std::ofstream xmp_f(xmp_filename.c_str());
	if (xmp_f.fail())
		throw ns_ex("Could not open ojp2k metadata file") << xmp_filename;
	xmp_f << xmp_output;
	xmp_f.close();
}

void ns_ojp2k_initialization::init(){
	if (!jp2k_initialized) {
	//	if (jas_init())
	//		throw ns_ex("ns_jp2k_image_input_file()::Could not initialize jasper jpeg2000 library");
		jp2k_initialized=true;
	}
}
void ns_ojp2k_initialization::cleanup(){
	if (jp2k_initialized){
	//	jas_cleanup();
		jp2k_initialized = false;
	}
}
bool ns_ojp2k_initialization::jp2k_initialized = false;


//from openjpeg mailing list https://groups.google.com/forum/#!msg/openjpeg/7RZRPmzdE_M/eQGojBtOAawJ
#define JP2_XML 0x786D6C20

void ns_jp2k_error_callback(const char *msg, void *client_data){
	ns_jp2k_data * c = (ns_jp2k_data *)client_data;
	c->ex = new ns_ex("ns_jp2k::Error: ");
	*c->ex << msg;
}
void ns_jp2k_warning_callback(const char *msg, void *client_data){
	std::cerr << "ns_jp2k::Warning: " << msg;
}
void ns_jp2k_info_callback(const char *msg, void *client_data){
	std::cerr << "ns_jp2k::Info: " << msg;
}

std::string ns_oj2k_xmp_filename(const std::string &filename) {
	return filename + ".xmp";
}
void ns_jp2k_out_error_callback(const char *msg, void *client_data){
	ns_jp2k_output_data * c = (ns_jp2k_output_data *)client_data;
	c->ex = new ns_ex("ns_jp2k::Error: ");
	*c->ex << msg;
}
