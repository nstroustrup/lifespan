#include "ns_ojp2k.h"
#include <stdio.h>
#include <limits.h>
#include "ns_xmp_encoder.h"
#include <fstream>


void ns_load_xml_information_from_ojp2k_xmp(const std::string filename, ns_image_properties & properties) {

	std::ifstream xmp_f(filename.c_str());
	if (!xmp_f.fail()) {
		ns_xmp_tiff_data xmp_tiff_data;

		std::string xmp_input;
		xmp_input.reserve(2000);
		char a;
		while (true) {
			xmp_f.read(&a, 1);
			if (xmp_f.fail())
				break;
			xmp_input += a;

		}

		ns_long_string_xmp_encoder::read(xmp_input, properties.description, &xmp_tiff_data);
		properties.resolution = xmp_tiff_data.xresolution;
		if (xmp_tiff_data.xresolution != xmp_tiff_data.yresolution)
			throw ns_ex("Unmatching x and y resolutions encountered.  This is not implemented.");
		properties.resolution = xmp_tiff_data.xresolution;
	}
	else {

		std::cerr << "Warning: Could not open ojp2k metadata file " << filename << ".  Continuing with defaults.";
		properties.resolution = 3200;
	}
}

void ns_ojp2k_setup_output(const ns_image_properties & properties,const float compression_ratio,const std::string & filename, const unsigned long rows_per_strip , ns_jp2k_output_data * data,const char bit_depth) {

	if (properties.height == 0)
		throw (ns_ex("Cannot create an image with 0 height!"));
	if (properties.width == 0)
		throw (ns_ex("Cannot create an image with 0 width!"));
	if (properties.components == 0)
		throw (ns_ex("Cannot create an image with 0 pixel components!"));
	opj_set_default_encoder_parameters(&data->parameters);
	
	//if (compression_ratio == 1)
	//	data->parameters.irreversible = 1;
	//else 
		data->parameters.irreversible = 0;
	
	data->parameters.cp_tx0 = 0;
	data->parameters.cp_ty0 = 0;
	unsigned long tile_height;
	if (properties.height < rows_per_strip)
		throw ns_ex("Too many rows per strip!");
	data->parameters.tile_size_on = OPJ_TRUE;
	data->using_tiles = true;
	data->parameters.cp_tdx = properties.width;
	data->parameters.cp_tdy = rows_per_strip;
	tile_height = rows_per_strip;
	
	/*
	if (properties.width >= 2048)	data->parameters.cp_tdx = 2048;
	else	data->parameters.cp_tdx = pow(2, floor(log2(properties.width)));
	if (properties.height >= 2048)	data->parameters.cp_tdy = 2048;
	else	data->parameters.cp_tdy = pow(2, floor(log2(properties.height)));
	*/
	
	data->parameters.tcp_numlayers = 1;
	if (compression_ratio == 0 || compression_ratio==1)
		data->parameters.tcp_rates[0] = 0;
	else
	data->parameters.tcp_rates[0] = 1.0 / compression_ratio;
	data->parameters.tcp_rates[1] = 0;
	
	data->parameters.roi_compno = -1;
	//data->parameters.roi_shift = 0;
	//data->parameters.res_spec = 0;
	//data->parameters.index_on = NULL;
	//data->parameters.image_offset_x0 = 0;
	//data->parameters.image_offset_y0 = 0;
	//data->parameters.subsampling_dx = 0;
	//data->parameters.subsampling_dy = 0;
	
	data->parameters.cp_disto_alloc = 1;
	//data->parameters.cp_fixed_alloc = 0;
	//data->parameters.cp_fixed_quality = 0;
	//data->parameters.cp_matrice = 0;
	data->parameters.prog_order = OPJ_LRCP;
	//data->parameters.numpocs = 0;
	data->parameters.numresolution = 8;
	data->parameters.cod_format = 1;//jp2
	//data->parameters.cp_comment = 0;
	//data->parameters.csty = 0;
	//data->parameters.mode = 0;
	//data->parameters.max_comp_size = 0;
	data->parameters.tcp_mct = (properties.components == 3) ? 1 : 0;
	opj_image_comptparm prm[3];
	memset(prm, 0, 3*sizeof(opj_image_comptparm));
	for (unsigned int i = 0; i < properties.components; i++) {
		prm[i].dx = prm[i].dy = 1;
		prm[i].w = properties.width;
		prm[i].h = properties.height;
		prm[i].x0 = prm[i].y0 = 0;
		prm[i].prec = 8 * bit_depth;
		prm[i].bpp = 8 * bit_depth;
		prm[i].sgnd = 0;
	}
	
	OPJ_COLOR_SPACE color_space((properties.components == 3) ? OPJ_CLRSPC_SRGB : OPJ_CLRSPC_GRAY);
	data->image = opj_image_tile_create(properties.components, &prm[0], color_space);
	data->image->x0 = 0;
	data->image->y0 = 0;
	data->image->x1 = properties.width;
	data->image->y1 = properties.height;
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
	if (!opj_setup_encoder(data->codec, &(data->parameters), data->image))
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
	//don't output any informational openjpeg messages.
	//std::cerr << "ns_jp2k::Info: " << msg;
}

std::string ns_oj2k_xmp_filename(const std::string &filename) {
	return filename + ".xmp";
}
void ns_jp2k_out_error_callback(const char *msg, void *client_data){
	ns_jp2k_output_data * c = (ns_jp2k_output_data *)client_data;
	c->ex = new ns_ex("ns_jp2k::Error: ");
	*c->ex << msg;
}
#include "ns_image_easy_io.h"
#include <fstream>
#include "ns_high_precision_timer.h"
//run a diagnostic
void ns_openjpeg_test(const std::string & input_filename,  const std::string & output_basename) {
	try {
		ns_image_standard im;
		std::string base_path(output_basename);
		int fold_compression[16] = { 0,1,2,4,8,16,20,25,30,35,40,50,55,60,80,100 };
		int max_diff[16];
		ns_64_bit total_diff[16];
		ns_64_bit file_size[16];
		ns_64_bit read_time[16];
		ns_high_precision_timer tm;
		tm.start();
		ns_load_image(input_filename, im);
		read_time[0] = tm.stop();
		total_diff[0] = max_diff[0] = 0;

		for (int i = 1; i < 16; i++) {
			std::string fn = base_path + ns_to_string(fold_compression[i]) + ".jp2";
			ns_save_image(fn, im, 1.0 / fold_compression[i]);
			ns_image_standard reread;
			tm.start();
			ns_load_image(fn, reread);
			read_time[i] = tm.stop();
			max_diff[i] = 0;
			total_diff[i] = 0;
			file_size[i] = ns_dir::get_file_size(fn);
			for (unsigned int y = 0; y < reread.properties().height; y++)
				for (unsigned int x = 0; x < reread.properties().width; x++) {
					unsigned long p = abs((long)im[y][x] - (long)reread[y][x]);
					if (max_diff[i] < p)
						max_diff[i] = p;
					total_diff[i] += p;
				}
		}
		file_size[0] = file_size[1];

		std::ofstream out((base_path + "quant.csv").c_str());
		out << "fold_compression,max_diff,total_diff,file_size,read_time_ms\n";
		for (int i = 0; i < 16; i++) {
			out << fold_compression[i] << "," << max_diff[i] << "," << total_diff[i] << "," << file_size[i] << "," << read_time[i]/1000 <<  "\n";
		}
		out.close();
	}
	catch (ns_ex & ex) {
		std::cerr << ex.text();
		char a;
		std::cin >> a;
	}
}
