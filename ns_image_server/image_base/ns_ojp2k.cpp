#include "ns_ojp2k.h"
#include <stdio.h>

#include "openjpeg.h"

#include "ns_xmp_encoder.h"

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

struct ns_jp2k_data{
	ns_jp2k_data():dinfo(0),image(0),ex(0){memset(&event_mgr, 0, sizeof(opj_event_mgr_t));}
	opj_dinfo_t* dinfo;
	opj_event_mgr_t event_mgr;	
	opj_image_t *image;
	ns_ex * ex;
};
//from openjpeg mailing list https://groups.google.com/forum/#!msg/openjpeg/7RZRPmzdE_M/eQGojBtOAawJ
#define JP2_XML 0x786D6C20

void ns_jp2k_in_error_callback(const char *msg, void *client_data){
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

void ns_8_bit_ojp2k_image_input_file::open_file_i(const std::string & filename){
	if (data != 0)
		throw ns_ex("Opening already open file");
	data = new ns_jp2k_data;

	opj_dparameters_t parameters;
	opj_set_default_decoder_parameters(&parameters);

	//configure error manager	
	data->event_mgr.error_handler = ns_jp2k_in_error_callback;
	data->event_mgr.warning_handler = ns_jp2k_warning_callback;
	data->event_mgr.info_handler = 0;
	
	data->dinfo = opj_create_decompress(CODEC_JP2);
	opj_set_event_mgr((opj_common_ptr)data->dinfo, &data->event_mgr, this);

	/* setup the decoder decoding parameters using the current image and user parameters */
	opj_setup_decoder(data->dinfo, &parameters);

	//load data from file
	FILE *fsrc(fopen(filename.c_str(), "rb"));
	if (!fsrc)
		throw ns_ex("Could not load file " ) << filename;

	fseek(fsrc, 0, SEEK_END);
	unsigned long file_length = ftell(fsrc);
	fseek(fsrc, 0, SEEK_SET);
	unsigned char *src(new unsigned char[file_length]);
	if (fread(src, 1, file_length, fsrc) != (size_t)file_length){
		free(src);
		fclose(fsrc);
		throw ns_ex("Could not load enough data from file " ) << filename;
	}
	fclose(fsrc);

	//open bitstream
	opj_cio_t *cio(opj_cio_open((opj_common_ptr)data->dinfo, src, file_length));


	opj_codestream_info_t cstr_info; 
	data->image = opj_decode_with_info(data->dinfo, cio, &cstr_info);
	
	if(!data->image) {
		opj_destroy_decompress(data->dinfo);
		opj_cio_close(cio);
		free(src);
		throw ns_ex("Could not decode image!");
	}
	std::string xmp_data;
	if (cstr_info.xmp_data != 0){
		unsigned long iln = strlen((const char *)cstr_info.xmp_data);
		for (unsigned int i = 0; i < iln; i++){
			xmp_data+=cstr_info.xmp_data[i];
		}
		ns_xmp_tiff_data tiff_data;
		ns_long_string_xmp_encoder::read(xmp_data,properties.description,&tiff_data);
		if (tiff_data.xresolution != tiff_data.yresolution)
			throw ns_ex("Encountered an XMP specification with different x and y resolutions");
		properties.resolution = tiff_data.xresolution;
		free(cstr_info.xmp_data);
		cstr_info.xmp_data = 0;
	}

	free(src);
	opj_cio_close(cio);

	if(data->image->color_space == CLRSPC_SYCC)
		throw ns_ex("Can only handle rgb or grayscale images.");

	properties.width = cstr_info.image_w;
	properties.height = cstr_info.image_h;
	int depth = data->image->comps[0].prec;
	properties.components = cstr_info.numcomps;
	if (depth != 8)
		throw ns_ex("ns_jp2k_image_input_file::open()::16 bit images not yet supported");
	lines_read = 0;
}

void ns_8_bit_ojp2k_image_input_file::close_i(){
	if (data != 0){
		opj_destroy_decompress(data->dinfo);
		opj_image_destroy(data->image);
		delete data;
		data = 0;
	}
	lines_read = 0;
}

bool ns_8_bit_ojp2k_image_input_file::read_line_i(ns_8_bit * buffer){
	return read_lines_i(&buffer,1)>0;
}

//read in multiple lines
const unsigned int ns_8_bit_ojp2k_image_input_file::read_lines_i(ns_8_bit ** buffer,const unsigned int n){	
	if (data == 0)
		throw ns_ex("ns_jp2k_image::Reading from an unopened file!");

	int adjust(data->image->comps[0].sgnd ? 1 << (data->image->comps[0].prec - 1) : 0);
		
	if (data->image->numcomps == 3){
		for (unsigned long y = 0; y < n; y++) 
			for (unsigned long x = 0; x < properties.width; x++){
				buffer[y][3*x] = data->image->comps[0].data[properties.width*(y+lines_read)+x] + adjust;
				buffer[y][3*x+1] = data->image->comps[1].data[properties.width*(y+lines_read)+x] + adjust;
				buffer[y][3*x+2] = data->image->comps[2].data[properties.width*(y+lines_read)+x] + adjust;
			}
	}
	else{
		for (unsigned long y = 0; y < n; y++) 
			for (unsigned long x = 0; x < properties.width; x++){
				buffer[y][x] = data->image->comps[0].data[properties.width*(y+lines_read)+x] + adjust;
			}
	}
	lines_read+=n;
	return n;
}

	
	

struct ns_jp2k_output_data{
	ns_jp2k_output_data():image(0),output_file(0),ex(0){	memset(&event_mgr, 0, sizeof(opj_event_mgr_t));}
	opj_image_t *image;
	opj_event_mgr_t event_mgr;
	FILE * output_file;
	opj_cparameters_t parameters;
	std::string comment;
	ns_ex * ex;
};

void ns_jp2k_out_error_callback(const char *msg, void *client_data){
	ns_jp2k_output_data * c = (ns_jp2k_output_data *)client_data;
	c->ex = new ns_ex("ns_jp2k::Error: ");
	*c->ex << msg;
}

void ns_8_bit_ojp2k_image_output_file::open_file_i(const std::string & filename, const ns_image_properties & properties_){
	if (data != 0)
		throw ns_ex("Opening already-open file");
	data = new ns_jp2k_output_data;

	lines_written = 0;

	this->properties = properties_;

	memset(&data->event_mgr, 0, sizeof(opj_event_mgr_t));
	data->event_mgr.error_handler = ns_jp2k_out_error_callback;
	data->event_mgr.warning_handler = ns_jp2k_warning_callback;
	data->event_mgr.info_handler = 0;
	opj_set_default_encoder_parameters(&data->parameters);
	if (properties.compression == 1)
		data->parameters.irreversible = 0;
	else data->parameters.irreversible = 1;
	data->parameters.cblockw_init = 64;
	data->parameters.cblockh_init = 64;
//	data->parameters.csty |= 0x01;

	data->parameters.tcp_numlayers = 3;
	for (unsigned int i = 0; i < data->parameters.tcp_numlayers; i++){
		data->parameters.tcp_rates [i] = 1.0/properties.compression;
	}
//data->parameters.numresolution = 6;
	data->parameters.cp_disto_alloc = 1;
	//data->parameters.
	OPJ_COLOR_SPACE color_space((properties.components==3)?CLRSPC_SRGB:CLRSPC_GRAY);
	opj_image_cmptparm_t *cmptparm( new opj_image_cmptparm_t[properties.components * sizeof(opj_image_cmptparm_t)]);
	
	try{
			if (data->ex != 0)
				throw *data->ex;
			memset(&cmptparm[0], 0, properties.components * sizeof(opj_image_cmptparm_t));

		for(int i = 0; i < properties.components; i++) {		
			cmptparm[i].prec = 8;
			cmptparm[i].bpp = 8;
			cmptparm[i].sgnd = 0;
			cmptparm[i].dx = 1;
			cmptparm[i].dy = 1;
			cmptparm[i].w = properties.width;
			cmptparm[i].h = properties.height;
		}
		/* create the image */
		data->image = opj_image_create(properties.components, &cmptparm[0], color_space);	
		if (data->ex != 0)
				throw *data->ex;
	}
	catch(...){
		delete[] cmptparm;
		cmptparm = 0;
		throw;
	}
	
	delete[] cmptparm;

	if(!data->image) 
		throw ns_ex("Could not create an image");

	data->image->x0 = 0;
	data->image->y0 = 0;
	data->image->x1 = properties.width;
	data->image->y1 = properties.height;

	data->output_file = fopen(filename.c_str(), "wb");
	if (!data->output_file) 
		throw ns_ex("Could not open output file");
}

	void ns_8_bit_ojp2k_image_output_file::close_i(){
		if (data != 0){
			if (data->output_file != 0)
				fclose(data->output_file);
			if (data->image != 0)
				opj_image_destroy(data->image);
			if (data->ex != 0)
				delete data->ex;
			delete data;
			data = 0;
		}
	}
		
	//write a single line
	bool ns_8_bit_ojp2k_image_output_file::write_line_i(const ns_8_bit * buffer){
		const ns_8_bit * buf[1] = {buffer};
		write_lines_i(buf,1);
		return true;
	}
	
	void ns_8_bit_ojp2k_image_output_file::write_lines_i(const ns_8_bit ** buffer,const unsigned int n){
			
		unsigned char value = 0;
		for(int c = 0; c < properties.components; c++) {
			for (unsigned int y = 0; y < n; y++)
				for (unsigned int x = 0; x < properties.width; x++)
					data->image->comps[c].data[properties.width*(lines_written+y) + x] = buffer[y][properties.components*x + c];
		}

		lines_written+=n;

		if (lines_written >= properties.height){
				
			/* get a JP2 compressor handle */				
			opj_cinfo_t * cinfo(opj_create_compress(CODEC_JP2));
			if (data->ex != 0)
				throw *data->ex;
			try{
				/* catch events using our callbacks and give a local context */
				opj_set_event_mgr((opj_common_ptr)cinfo, &data->event_mgr, this);
				
				if (data->ex != 0)
					throw *data->ex;
				
			
		
				ns_xmp_tiff_data xmp_tiff_data;
				xmp_tiff_data.orientation = 1;
				xmp_tiff_data. xresolution = properties.resolution;
				xmp_tiff_data.yresolution = properties.resolution;
				xmp_tiff_data.resolution_unit = 2;
				
				std::string xmp_output;
				ns_long_string_xmp_encoder::write(properties.description,xmp_output,&xmp_tiff_data);
				if (xmp_output.size() == 0)
					cinfo->xmp_data = 0;
				else{
					cinfo->xmp_data = new unsigned char[xmp_output.length()+1];
					strcpy((char *)cinfo->xmp_data,xmp_output.c_str());
				}
				try{
					/* setup the encoder parameters using the current image and using user parameters */
					opj_setup_encoder(cinfo, &data->parameters, data->image);
					if (data->ex != 0)
						throw *data->ex;
					/* open a byte stream for writing */
					/* allocate memory for all tiles */
					opj_cio_t * cio(opj_cio_open((opj_common_ptr)cinfo, NULL, 0));

					try{
						if (data->ex != 0)
							throw *data->ex;

						/* encode the image */
						if (!opj_encode(cinfo, cio, data->image, NULL))
							throw ns_ex("Could not encode image");
						
						if (data->ex != 0)
							throw *data->ex;

						unsigned long codestream_length = cio_tell(cio);

						/* write the buffer to disk */
				
						size_t res = fwrite(cio->buffer, 1, codestream_length, data->output_file);
						if( res < (size_t)codestream_length ) 
 									throw ns_ex("Could not write data to file");
						fclose(data->output_file);
						data->output_file = 0;
					}
					catch(...){
						opj_cio_close(cio);
						throw;
					}
					/* close and free the byte stream */
					opj_cio_close(cio);
				}
				catch(...){
					opj_destroy_compress(cinfo);
					throw;
				}
			}
			catch(...){
				if (cinfo->xmp_data != 0)
				delete []cinfo->xmp_data;
				throw;
			}
			if (cinfo->xmp_data != 0)
				delete []cinfo->xmp_data;
			/* free remaining compression structures */
			opj_destroy_compress(cinfo);

			/* free image data */
			opj_image_destroy(data->image);
			data->image = 0;
		}
	}
