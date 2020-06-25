#ifndef NS_OJP2K
#define NS_OJP2K
#include "ns_image.h"
#ifdef _WIN32
#define OPJ_STATIC
#include "openjpeg.h"
#else
#include "openjpeg-2.3/openjpeg.h"
#endif
struct ns_ojp2k_initialization{
	static void init();
	static void cleanup();
	static bool jp2k_initialized;
	static bool verbose_output;
	static int warning_output_counter;
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
	ns_jp2k_data() :codec(0), image(0), ex(0), cstr_index(0),stream(0) {}

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

void ns_load_xml_information_from_ojp2k_xmp(const std::string filename, ns_image_properties & prop);

template<class ns_component, bool low_memory_single_line_reads=false>
class ns_ojp2k_image_input_file : public ns_image_input_file<ns_component, low_memory_single_line_reads>{
protected:
	ns_jp2k_data * data;
	unsigned long lines_read, lines_sent, input_buffer_size, total_lines_sent;
	ns_image_properties properties;
	std::string opened_filename;
	ns_component * input_buffer;
public:
	ns_ojp2k_image_input_file() :lines_read(0), data(0), lines_sent(0), total_lines_sent(0),input_buffer(0), input_buffer_size(0)
	{ ns_ojp2k_initialization::init(); }

	void open_file(const std::string & filename) {
		if (input_buffer != 0) {
			delete[] input_buffer;
			input_buffer = 0;
			input_buffer_size = 0;
		}
		lines_read = 0;
		lines_sent = 0;
		total_lines_sent = 0;
		if (data != 0)
			throw ns_ex("Opening unclosed file!");
		data = new ns_jp2k_data();
		opened_filename = filename;
		data->stream = opj_stream_create_default_file_stream(filename.c_str(), 1);
		if (!data->stream)
			throw ns_ex("openjpeg::Could not open file ") << filename;

		ns_load_xml_information_from_ojp2k_xmp(ns_oj2k_xmp_filename(filename), properties);
	

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
		ns_image_input_file<ns_component,low_memory_single_line_reads>::_properties = this->properties;

		if (data->image->comps[0].prec / 8 != sizeof(ns_component))
			throw ns_ex("Attempting to load an ") << data->image->comps[0].prec << " bit image into a " << sizeof(ns_component) * 8 << " bit data structure!";

		if (!opj_set_decode_area(data->codec, data->image, data->image->x0, data->image->y0, data->image->x1, data->image->y1))
			throw ns_ex("Cannot open decode area");

	}
	void close_file() { close(); }
	void close() {
		try {
			if (data != 0) {
				ns_ex ex;
				if (!opj_end_decompress(this->data->codec, this->data->stream))
					ex = ns_ex("Could not successfully end decompression!");
				ns_safe_delete(data);
				data = 0;
				if (input_buffer != 0)
					delete[] input_buffer;
				input_buffer = 0;
				input_buffer_size = 0;
				if (!ex.text().empty())
					throw ex;
				if (total_lines_sent != properties.height)
					throw ns_ex("Not all lines were read from file!");

				total_lines_sent = 0;
			}
		}
		catch (ns_ex & ex) {
			if (ns_ojp2k_initialization::verbose_output) std::cerr << "Supressing exception in ns_ojp2k_image_input_file::close(): " << ex.text() << "\n";
		}

	}
	unsigned long seek_to_beginning() {
		close_file();
		open_file(opened_filename);
		return 0;
	}
	ns_component * operator()(const unsigned long x, const  unsigned int component, ns_component * buffer)const {
		return &(buffer[this->properties.components*x + component]);
	}
	bool read_line(ns_component * buffer) {
		if (lines_read <= lines_sent) {
			OPJ_UINT32 tile_index, data_size, num_components;
			OPJ_INT32 current_tile_x0, current_tile_y0, current_tile_x1, current_tile_y1;
			OPJ_BOOL more_tiles_left;
			if (!opj_read_tile_header(this->data->codec,
				this->data->stream,
				&tile_index,
				&data_size,
				&current_tile_x0,
				&current_tile_y0,
				&current_tile_x1,
				&current_tile_y1,
				&num_components,
				&more_tiles_left))
				throw ns_ex("Encountered an invalid tile!");
			unsigned long tile_height = (current_tile_y1 - current_tile_y0);
			unsigned long tile_width = (current_tile_x1 - current_tile_x0);
			if (tile_width != properties.width)
				throw ns_ex("The tile width is not equal to the image width!  This is valid in the jpg2000, but is not how the lifespan machine uses jpg2000 images.");
			if (!more_tiles_left)
				return false;
			if (input_buffer != 0 && input_buffer_size != tile_width*tile_height) {
				delete[] input_buffer;
				input_buffer = 0;
			}
			if (input_buffer == 0){
				input_buffer = new ns_component[tile_width*tile_height];
				input_buffer_size = tile_width*tile_height;
			}
			if (!opj_decode_tile_data(data->codec, tile_index, (OPJ_BYTE *)((void *)input_buffer), data_size, data->stream))
				throw ns_ex("Could not decode tile data!");
			lines_sent = 0;
			lines_read = tile_height;
		}
		for (unsigned int i = 0; i < this->properties.width; i++)
			buffer[i] = input_buffer[lines_sent*this->properties.width + i];
		lines_sent++;
		total_lines_sent++;
		return true;
	}
	const unsigned int read_lines(ns_component ** buffer, const unsigned int n) {
		for (unsigned int i = 0; i < n; i++) {
			read_line(buffer[n]);
		}
		return n;
	}
	void open_mem(const void *){ throw ns_ex("NOT IMPLEMENTED"); }
	~ns_ojp2k_image_input_file() { close(); }
};



//ns_8_bit_jp2k_image_output_file contains all the code to interact
//with the openjpeg library
#define NS_OPJEG_WIDTH ns_image_output_file<ns_component,low_memory_single_line_reads>::_properties.width*ns_image_output_file<ns_component>::_properties.components


struct ns_jp2k_output_data {
	ns_jp2k_output_data() :image(0), stream(0), codec(0), ex(0), using_tiles(false){}
	opj_image_t *image;
	opj_stream_t * stream;
	opj_codec_t * codec;
	opj_cparameters_t parameters;
	std::string comment;
	bool using_tiles;
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
//tcp ratio (equivalent to setting the -r value in opj_compress) is set as 1/properties.compression.
//early testing suggested that a 20x ratio works well, e.g properties.compression = .05
void ns_ojp2k_setup_output(const ns_image_properties & properties, const float compression_ratio,const std::string & filename, const unsigned long rows_per_strip, ns_jp2k_output_data * data, const char bit_depth);

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
	ns_ojp2k_image_output_file() :lines_written(0), lines_received(0),data(0), output_buf(0),output_buffer_height(0), rows_per_strip(50) {

		ns_ojp2k_initialization::init();
	}
	~ns_ojp2k_image_output_file() { ns_safe_delete(data); if (output_buf != 0) delete[] output_buf; output_buf = 0; }

	//tcp ratio (equivalent to setting the -r value in opj_compress) is set as 1/compression_ratio
	//early testing suggested that a 20x ratio works well, e.g compression_ratio = .05
	void open_file(const std::string & filename, const ns_image_properties & properties, const float compression_ratio) {
		if (data != 0)
			throw ns_ex("Opening already-open file");
		data = new ns_jp2k_output_data;

		lines_written = 0;

		ns_image_output_file<ns_component>::_properties = properties;
		lines_received = 0;
		output_buffer_height = 0;
		rows_per_strip = 512;
		if (rows_per_strip == 0)
			rows_per_strip = 1;
		if (rows_per_strip > properties.height)
			rows_per_strip = properties.height;
		output_buf = new ns_component[rows_per_strip*ns_image_output_file<ns_component>::_properties.width];

		ns_ojp2k_setup_output(properties, compression_ratio,filename, rows_per_strip, data,sizeof(ns_component));
	}

	void close() {
		try {
			if (output_buf != 0) {

				if (output_buffer_height != 0) {
					OPJ_UINT32 l_data_size = sizeof(ns_component)*ns_image_output_file<ns_component>::_properties.width*output_buffer_height;


					if (!opj_write_tile(data->codec, lines_written / rows_per_strip, (OPJ_BYTE *)((void *)output_buf), l_data_size, data->stream))
						throw ns_ex("openjpeg::Could not write line");

					if (data->exception_thrown())
						std::cerr << "ns_ojpeg:close_file() wants to throw an exception, but cannot: " << data->ex->text() << "\n";

				}

				if (!opj_end_compress(data->codec, data->stream)) {
					std::cerr << "Could not compress openjpeg file.\n";
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
		for (unsigned int i = 0; i < ns_image_output_file<ns_component>::_properties.width; i++)
			output_buf[output_buffer_height*ns_image_output_file<ns_component>::_properties.width + i] = buffer[i];
		output_buffer_height++;
		lines_received++;
		if (output_buffer_height == rows_per_strip) {

			OPJ_UINT32 l_data_size = sizeof(ns_component)*ns_image_output_file<ns_component>::_properties.width*output_buffer_height;

			if (!opj_write_tile(data->codec, lines_written / rows_per_strip, (OPJ_BYTE *)((void *)output_buf), l_data_size, data->stream))
				throw ns_ex("openjpeg::Could not write line");
			lines_written += output_buffer_height;
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
//run a diagnostic comparing different compression levels
void ns_openjpeg_test(const std::string & input_filename, const std::string & output_basename);

#endif
