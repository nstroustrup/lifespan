#ifndef NS_OJP2K
#define NS_OJP2K
#include "ns_image.h"
#define OPJ_STATIC
#include "openjpeg.h"

struct ns_ojp2k_initialization{
	static void init();
	static void cleanup();
	static bool jp2k_initialized;
};

std::string ns_oj2k_xmp_filename(const std::string &filename);

void ns_jp2k_error_callback(const char *msg, void *client_data);
void ns_jp2k_warning_callback(const char *msg, void *client_data);
void ns_jp2k_info_callback(const char *msg, void *client_data);

//ns_8_bit_jp2k_image_input_file contains all the code to interact 
//with the JASPER jppeg2000 library
//It is encapsulated as its own (non-templated) class so that
//the implemenentation (which requires the inclusion of the JASPER header file
//can be kept in a separate .cpp file.  The JASPER header contains a bunch of
//of #define statments that conflict with other libraries inlcuding mysql
struct ns_jp2k_data {
	ns_jp2k_data() :codec(0), image(0), ex(0), cstr_index(0) {}

	opj_stream_t * stream;
	opj_codec_t* codec;
	opj_image_t *image;
	opj_codestream_index_t* cstr_index;
	ns_ex * ex;

	~ns_jp2k_data() {
		ns_safe_delete(ex);
		if (codec != 0) {
			opj_destroy_codec(codec);
			codec = 0;
		}
		if (stream != 0) {
			opj_stream_destroy(stream);
			stream = 0;
		}
		if (cstr_index != 0) {
			opj_destroy_cstr_index(&cstr_index);
			cstr_index = 0;
		}
		if (image != 0) {
			opj_image_destroy(image);
			image = 0;
		}
	}
};


template<class ns_component>
class ns_ojp2k_image_input_file : public ns_image_input_file<ns_component>{
protected:
	ns_jp2k_data * data;
	int jas_matrix_width,jas_matrix_height;
	unsigned long lines_read;
	ns_image_properties properties;
	std::string opened_filename;
public:
	ns_ojp2k_image_input_file():lines_read(0),data(0){ ns_ojp2k_initialization::init();}
	
	void open_file(const std::string & filename) {
		if (data != 0)
			throw ns_ex("Opening unclosed file!");
		data = new ns_jp2k_data();
		opened_filename = filename;
		data->stream = opj_stream_create_default_file_stream(filename.c_str(), 1);
		if (!data->stream)
			throw ns_ex("openjpeg::Could not open file ") << filename;


		data->codec = opj_create_decompress(OPJ_CODEC_JP2);
		if (data->codec == 0)
			throw ns_ex("openjpeg::Could not create codec!");

		opj_set_info_handler(data->codec, ns_jp2k_info_callback, 0);
		opj_set_warning_handler(data->codec, ns_jp2k_warning_callback, 00);
		opj_set_error_handler(data->codec, ns_jp2k_error_callback, 00);

	
		opj_dparameters_t parameters;
		opj_set_default_decoder_parameters(&parameters);
		parameters.decod_format = 1;
		parameters.cp_layer = 1;

		if (!opj_setup_decoder(data->codec, &parameters))
			throw ns_ex("openjpeg::Could not set up decoder");

		if (!opj_read_header(data->stream, data->codec, &data->image))
			throw ns_ex("openjpeg::Could not read header");
		

		this->properties.width = data->image->x1 - data->image->x0;
		this->properties.height = data->image->y1 - data->image->y0;
		this->properties.components = data->image->numcomps;

	}
	void close_file() {}
	void close() { ns_safe_delete(data); }
	void seek_to_beginning() {
		close_file();
		open_file(opened_filename);
	}
	ns_component * operator()(const unsigned long x, const  unsigned int component, ns_component * buffer)const {
		return &(buffer[ns_image_input_file<ns_component>::_properties.components*x + component]);
	}
	bool read_line(ns_component * buffer) { throw ns_ex("NOT IMPLEMENTED"); }
	const unsigned int read_lines(ns_component ** buffer, const unsigned int n) { throw ns_ex("NOT IMPLEMENTED"); }
	void open_mem(const void *){ throw ns_ex("NOT IMPLEMENTED"); }
	~ns_ojp2k_image_input_file() { close(); }
};



//ns_8_bit_jp2k_image_output_file contains all the code to interact 
//with the openjpeg library
#define NS_OPJEG_WIDTH ns_image_output_file<ns_component>::_properties.width*ns_image_output_file<ns_component>::_properties.components


struct ns_jp2k_output_data {
	ns_jp2k_output_data() :image(0), stream(0), codec(0), ex(0) {}
	opj_image_t *image;
	opj_stream_t * stream;
	opj_codec_t * codec;
	opj_cparameters_t parameters;
	std::string comment;
	ns_ex * ex;

	bool exception_thrown() { return ex != 0; }

	~ns_jp2k_output_data() {
		ns_safe_delete(ex);
		if (codec != 0) {
			opj_destroy_codec(codec);
			codec = 0;
		}
		if (stream != 0) {
			opj_stream_destroy(stream);
			stream = 0;
		}
		if (image != 0) {
			opj_image_destroy(image);
			image = 0;
		}
	}
};

void ns_ojp2k_setup_output(const ns_image_properties & properties, const std::string & filename, const unsigned long rows_per_strip, ns_jp2k_output_data * data);

template<class ns_component>
class ns_ojp2k_image_output_file : public ns_image_output_file<ns_component> {
protected:
	ns_component * output_buf;
	ns_jp2k_output_data * data;
	ns_image_properties properties;
	unsigned long lines_written;
	unsigned long lines_received;
	unsigned long output_buffer_height;
	unsigned long rows_per_strip;

public:
	ns_ojp2k_image_output_file() :lines_written(0), data(0), output_buf(0), rows_per_strip(50) {
		ns_ojp2k_initialization::init();
	}
	~ns_ojp2k_image_output_file() { ns_safe_delete(data); if (output_buf != 0) delete[] output_buf; output_buf = 0; }

	void open_file(const std::string & filename, const ns_image_properties & properties) {
		if (data != 0)
			throw ns_ex("Opening already-open file");
		data = new ns_jp2k_output_data;

		lines_written = 0;

		ns_image_output_file<ns_component>::_properties = properties;
		output_buf = (ns_component *)new unsigned char[sizeof(ns_component)*rows_per_strip*NS_TIFF_WIDTH];
		
		rows_per_strip = 512;
		if (rows_per_strip == 0)
			rows_per_strip = 1;
		if (rows_per_strip > properties.height)
			rows_per_strip = properties.height;
		ns_ojp2k_setup_output(properties, filename, rows_per_strip, data);
	}

	void close() {
		try {
			if (output_buf != 0) {
				if (output_buffer_height != 0) {

					OPJ_UINT32 l_data_size = sizeof(ns_component)*NS_OPJEG_WIDTH*output_buffer_height;

					if (!opj_write_tile(data->codec, lines_written / rows_per_strip, (OPJ_BYTE *)((void *)output_buf), l_data_size, data->stream))
						throw ns_ex("openjpeg::Could not write line");

					if (data->exception_thrown())
						std::cerr << "ns_ojpeg:close_file() wants to throw an exception, but cannot: " << data->ex->text() << "\n";

				}
				delete[] output_buf;
				ns_safe_delete(data);
				output_buf = 0;
				output_buffer_height = 0;
			}
		}
		catch (ns_ex & ex) {
			std::cerr << "ns_opjpeg::!Throwing in close(): " << ex.text() << "\n";
		}
		catch (...) {
			std::cerr << "ns_opjpeg::close()::!Throwing an unknown error\n";
		}
	}


	//write a single line
	bool write_line(const ns_component * buffer) {
		for (unsigned int i = 0; i < NS_OPJEG_WIDTH; i++)
			output_buf[output_buffer_height*NS_OPJEG_WIDTH + i] = buffer[i];
		output_buffer_height++;
		lines_received++;
		if (output_buffer_height == rows_per_strip) {

			OPJ_UINT32 l_data_size = sizeof(ns_component)*NS_OPJEG_WIDTH*output_buffer_height;
			
			if (!opj_write_tile(data->codec, lines_written / rows_per_strip, (OPJ_BYTE *)((void *)output_buf), l_data_size, data->stream))
				throw ns_ex("openjpeg::Could not write line");

			if (data->exception_thrown()){
				std::cerr << "10";
				throw data->ex;
			}
			output_buffer_height = 0;
		}
		return true;
	}
	void write_lines(const ns_component ** buffer, const unsigned int n) {
		for (unsigned int i = 0; i < n; i++)
			write_line(buffer[i]);
	}	
	void open_mem(const void *, const ns_image_properties &p) { throw ns_ex("NOT IMPLEMENTED"); }
	virtual ns_component * operator()(const unsigned long x, const unsigned int component, ns_component * buffer)const {
		return &(buffer[ns_image_output_file<ns_component>::_properties.components*x + component]);
	}
};

void ns_jp2k_out_error_callback(const char *msg, void *client_data);


#endif
