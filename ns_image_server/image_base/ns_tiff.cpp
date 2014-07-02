#include "ns_tiff.h"
#include <fstream>

#include "ns_xmp_encoder.h"

void ns_throw_exception(const ns_ex & ex){
	throw ns_ex(ex.text());
}
using namespace std;
static void ns_tiff_error_handler(thandle_t client_data,const char * module, const char * fmt, va_list ap){
	ns_tiff_client_data * cd(static_cast<ns_tiff_client_data *>(client_data));
	ns_ex ex("libtiff::");
	char buf[1024];	
	if (module != NULL){
		#ifdef _WIN32 
		_snprintf_s(buf, 1024,"%s", module);
		#else
		snprintf(buf, 1024,"%s", module);
		#endif
		ex << buf << "::";
	}

	vsnprintf(buf,1024, fmt, ap);
	ex << buf << ns_file_io;
	//if no client data is provided or error storage is not requested, immediately throw the exception
	if (cd == 0 || !cd->store_errors)
		ns_throw_exception(ex);
	//if the client data requests an exception be stored rather than thrown, do so.
	cd->error_storage = new ns_ex(ex);
}

static void ns_tiff_warning_handler(const char* module, const char* fmt, va_list ap){
	ns_ex ex("libtiff::");
	char buf[1024];	
	if (module != NULL){
		#ifdef _WIN32 
		_snprintf_s(buf, 1024,"%s", module);
		#else
		snprintf(buf, 1024,"%s", module);
		#endif
		ex << buf << "::";
	}

	vsnprintf(buf,1024, fmt, ap);
	ex << buf;
	string txt(ex.text());
	//supress spurious error messages generated when opening photoshop CS3 files
	if (txt.find("wrong data type 7 for \"RichTIFFIPTC\"") != txt.npos)
		return;
	else cerr << txt << "\n";
      
}

//Initialize Libtiff error handlers once 
bool libtiff_error_init(false);
void ns_initialize_libtiff(){
	if (libtiff_error_init)
		return;

	TIFFSetErrorHandler(0);
	TIFFSetErrorHandlerExt(ns_tiff_error_handler);
	TIFFSetWarningHandler(ns_tiff_warning_handler);
	libtiff_error_init=true;
}

//we can store image description info in two places
//1)The baseline ASCII TIFF tags (ImageDescription, Make, Model, Artist, and Copyright)
//2)The Adobe XMP tag
//
//Using baseline TIFF tags is very simple and portable, as they are built into libtiff and all clients must support baseline tags.
//Unfortunately adobe photoshop appears to crop tiff tag values at 2000 characters upon re-saving images.  If we want to store
//lots of metadata, then we can't use baseline TIFF tags.
//Also, ImageJ inserts its own ImageDescription tag on save and strips other ASCII baseline tags that are used to store the metadata,
//so metadata isn't reliably propagated with that program. 
//The GIMP is better in that it can keep an ImageDescription tag, but data in other baseline tags are similarly removed.
//
//The XMP tag is an XML based format adobe developed to store image metadata. It is a specific XML schema written as a text std::string
//to the TIFFTAG_XMLPACKET tag.  Using this to store metadata has the disadvantage of having to create / parse the verbose XML gobbledook.
//Not all clients will support this field, however photoshop does and the XMP tag allows us to escape the 2000 character limit on individual baseline tiff tags.
//Downsides of using the XMP tag are that the GIMP/ImageJ and other editors that don't know about the tag strip it from files. This is
//standard-compliant behavior: http://gimp.1065349.n5.nabble.com/GIMP-throwing-away-TIFF-tags-tp5232p5233.html
//Specifically, the TIFF spec section 7 has:
// "It is unnecessary—and possibly dangerous—for an editor to copy fields 
// it does not understand because the editor might alter the file in a way 
// that is incompatible with the unknown fields."
//Another odd downside is that Photoshop, which ostensibly supports XMP,
//erroneously XML-escapes entities inside CDATA sections. (Probably because CDATA is discouraged by the XMP spec:
// http://www.adobe.com/content/dam/Adobe/en/devnet/xmp/pdfs/XMPSpecificationPart1.pdf .)
// This necessitates the un-escaping code in ns_xmp_encoder::read.
//
//So, there are basically no reliable ways of saving arbitrary image metadata in TIFFs in a way that will persist
//through various common image editors. As above, this is basically by design of the TIFF spec. 
//The best option for short metadata would be to only store 2000 characters of metadata in the ImageDescription tag and then 
//only use Photoshop or the GIMP. Sorry, ImageJ.
// If large metadata must be stored, then XMP it is. Sorry GIMP.
// And it turns out that the software can generate some very large image metadata, so 
// it seems that XMP is the way to go. No go GIMP, no go ImageJ.

//Choose just one! And don't choose NS_STORE_METATADATA_IN_TIFFTAGS.
//#define NS_STORE_METATADATA_IN_TIFFTAGS
#define NS_STORE_METADATA_IN_XMP

ns_tiff_compression_type ns_get_tiff_compression_type(const ns_image_type & type){
	switch(type){
		case ns_tiff:return ns_tiff_compression_lzw; //I pose that there is never a good reason to save uncompressed tiffs.  
		case ns_tiff_lzw: return ns_tiff_compression_lzw;
		case ns_tiff_zip: return ns_tiff_compression_zip;
		default: return ns_tiff_compression_lzw;
	}
	//we'll never get here but not returning anything produces compiler warnings
	return ns_tiff_compression_lzw;;
}

template<>
unsigned int ns_tiff_image_input_file<ns_8_bit>::readEncodedStrip(TIFF * image, const unsigned int current_strip, ns_8_bit * comp_buf,const unsigned int length){
	
	unsigned int r(TIFFReadEncodedStrip(image,current_strip,comp_buf,length));
	if (client_data.exception_thrown()){
		cerr << "12";
			ns_throw_exception(client_data.ex());
	}
	return r;
}
template<class T>
void ns_set_tiff_field(TIFF * im,const long field,T val){
	if (TIFFSetField(im,field,val) != 1)
		ns_throw_exception(ns_ex("ns_set_tiff_field()::Could not set field: ") << field << " to " << val);
}
template<class T>
void ns_set_tiff_field(TIFF * im,const long field,const unsigned long size,T val){
	if (TIFFSetField(im,field,size,val) != 1)
		ns_throw_exception(ns_ex("ns_set_tiff_field()::Could not set field: ") << field << " to " << val);
}
class ns_long_string_tifftag_encoder{
public:

	void write(const std::string & s, TIFF * image){
		if ((unsigned long)s.length() > (unsigned long)2000*tag_series_size)
			ns_throw_exception(ns_ex("ns_long_string_tiff_encoder::Image Description is too long for storage: ") << (unsigned long)s.length() << " (Max = " << 2000*tag_series_size << ")");
		std::string tmp;	
		bool halt(false);
		for (unsigned int i = 0; i < tag_series_size && !halt; i++){
			std::string::size_type stop = 2000*(i+1);
			if (stop > s.length()){
				stop = s.length();
				halt = true;
			}
			tmp = s.substr(2000*i,stop-2000*i);
			ns_set_tiff_field(image,tag_series[i],tmp.c_str());
		}
	}
	void read(std::string & s, TIFF * image){
		s.resize(0);
		char * desc;
		for (int i = 0; i < (int)tag_series_size; i++){
			if (!TIFFGetField(image,tag_series[i],&desc))
				break;
			cerr << i << "\n";
			s += desc;
		}
	}
private:
	static const int tag_series[];
	static const unsigned int tag_series_size;
};

const int ns_long_string_tifftag_encoder::tag_series[] = {
	TIFFTAG_IMAGEDESCRIPTION,
	TIFFTAG_MAKE,
	TIFFTAG_MODEL,
	TIFFTAG_ARTIST,
	TIFFTAG_COPYRIGHT
};
const unsigned int ns_long_string_tifftag_encoder::tag_series_size = 5;
class ns_long_string_xmp_encoder_tiff{
public:
	void write(const std::string & s, TIFF * image){
		std::string xmp;
		ns_long_string_xmp_encoder::write(s,xmp);
		ns_set_tiff_field(image,TIFFTAG_XMLPACKET,(unsigned long)xmp.length(),xmp.c_str());
	}
	void read(std::string & s, TIFF * image){
		char * dsc;
		long count(0);
		std::string xmp;
		//The size of TIFFTAG_XMLPACKET is incorrectly defined as BYTE
		//in the libtiff documentation, perhaps resulting from ambiguity
		//in the standard.  There's some history to this one and variation
		//between different versions of libtiff. However, it has to be a LONG
		//otherwise the maximum length of XML packets would be 2^8 which is way
		//to short.  In most cases TIFFGetField correctly returns a long
		//but it is important to set the high bits to zero in case a BYTE is returned.
		if (TIFFGetField(image,TIFFTAG_XMLPACKET,&count,&dsc)){
			//cerr << "\n";
			for (unsigned int i = 0; i < (unsigned int)count; i++)
				xmp+=dsc[i];
			ns_long_string_xmp_encoder::read(xmp,s);
			
		}
	}
};

void ns_set_default_tiff_parameters(const ns_image_properties & p,const ns_tiff_compression_type & t,const unsigned long bits_per_sample, const unsigned long rows_per_strip,TIFF * image){
	
	ns_set_tiff_field(image, TIFFTAG_IMAGEWIDTH, p.width);
	ns_set_tiff_field(image, TIFFTAG_IMAGELENGTH, p.height);
	ns_set_tiff_field(image, TIFFTAG_SAMPLESPERPIXEL, (short)p.components);
	ns_set_tiff_field(image, TIFFTAG_BITSPERSAMPLE, (short)bits_per_sample);
	if (p.resolution < 0){
		ns_set_tiff_field(image, TIFFTAG_XRESOLUTION, 1.0f);
		ns_set_tiff_field(image, TIFFTAG_YRESOLUTION, 1.0f);
	}
	else{
		ns_set_tiff_field(image, TIFFTAG_XRESOLUTION, p.resolution);
		ns_set_tiff_field(image, TIFFTAG_YRESOLUTION, p.resolution);
	}

	ns_set_tiff_field(image, TIFFTAG_RESOLUTIONUNIT, RESUNIT_INCH);


	ns_set_tiff_field(image, TIFFTAG_ROWSPERSTRIP, rows_per_strip);

	switch(t){
		case ns_tiff_compression_none:		ns_set_tiff_field(image, TIFFTAG_COMPRESSION, COMPRESSION_NONE); break;
		case ns_tiff_compression_lzw:		ns_set_tiff_field(image, TIFFTAG_COMPRESSION, COMPRESSION_LZW);  break;
		case ns_tiff_compression_zip:		ns_set_tiff_field(image, TIFFTAG_COMPRESSION, COMPRESSION_ADOBE_DEFLATE); 
											/*ns_set_tiff_field(image, TIFFTAG_ZIPQUALITY , 9);*/break;
		case ns_tiff_compression_jp2000:	ns_set_tiff_field(image, TIFFTAG_COMPRESSION, COMPRESSION_JP2000); break;
		default: ns_throw_exception(ns_ex("ns_tiff_image_output_file::Unknown compression format requested!"));
	}
	
	ns_set_tiff_field(image, TIFFTAG_PREDICTOR, PREDICTOR_HORIZONTAL);
	switch(p.components){
		case 1: ns_set_tiff_field(image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_MINISBLACK);
			break;
		case 3: ns_set_tiff_field(image, TIFFTAG_PHOTOMETRIC, PHOTOMETRIC_RGB);
			break;
		default: ns_throw_exception(ns_ex("ns_tiff_image_output_file::Could not create a tiff file with ") << ns_file_io << p.components << " components.");
	}
	//ns_set_tiff_field(image, TIFFTAG_FILLORDER, FILLORDER_MSB2LSB);
	ns_set_tiff_field(image, TIFFTAG_ORIENTATION, ORIENTATION_TOPLEFT);
	ns_set_tiff_field(image, TIFFTAG_PLANARCONFIG, PLANARCONFIG_CONTIG);
	ns_set_tiff_field(image, TIFFTAG_ORIENTATION,ORIENTATION_TOPLEFT);
	ns_set_tiff_field(image, TIFFTAG_SOFTWARE, "ns_image_server (Nicholas Stroustrup 2009)");
	
	if (p.description.size() != 0){
		#ifdef NS_STORE_METATADATA_IN_TIFFTAGS
		ns_long_string_tifftag_encoder enc;
		#endif
		#ifdef NS_STORE_METADATA_IN_XMP
			ns_long_string_xmp_encoder_tiff enc;
		#endif
		enc.write(p.description,image);
	}

	
	//ns_set_tiff_field(image, NS_METADATA_TAG, p.description.c_str());
	ns_set_tiff_field(image, TIFFTAG_DATETIME,ns_format_time_string_for_tiff(ns_current_time()).c_str());
}



void ns_get_default_tiff_parameters(const unsigned char component_size, ns_image_properties & properties,ns_tiff_info & tiff_info,TIFF * image){

	short bitsize=0;
	if (TIFFGetField(image, TIFFTAG_BITSPERSAMPLE, &bitsize) == 0)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Image did not specify its bitsize.")<< ns_file_io);
	if (bitsize == 0)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Bitsize specified as zero"));
	if (bitsize != 8*component_size)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::pixel depth mismatch: Loading a ") << ns_file_io << bitsize << " bit image into a " << 8*component_size << " ns_image object.");
		
	tiff_info.strip_pos = 0;
	tiff_info.strip_length = 0;
	tiff_info.bytes_read_from_current_strip= 0;
	tiff_info.current_strip = 0;
	properties.width=0;
	properties.height=0;
	properties.resolution=0;

	if (TIFFGetField(image, TIFFTAG_IMAGEWIDTH, &properties.width) == 0)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Image does not specify its width.")<< ns_file_io);
	if (TIFFGetField(image, TIFFTAG_IMAGELENGTH, &properties.height) == 0)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Image does not specify its height.")<< ns_file_io);

	//get resolution information
	float xres,yres;
	bool res_defined=true;
	short res_unit = 0;

	if (TIFFGetField(image, TIFFTAG_XRESOLUTION, &xres) == 0)
		res_defined=false;
	else if (TIFFGetField(image, TIFFTAG_YRESOLUTION, &yres) == 0)
			res_defined=false;

	if (TIFFGetField(image, TIFFTAG_RESOLUTIONUNIT, &res_unit) != 0)
			res_unit = RESUNIT_NONE;

	if (res_unit == RESUNIT_CENTIMETER)  //convert resolution to inches if necissary
			yres /=2.54f;
	
	if (res_defined)
		properties.resolution = yres;
	else
		properties.resolution = 0;


	short photometric=0;
	if (TIFFGetField(image, TIFFTAG_PHOTOMETRIC, &photometric) == 0)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Image does not specify it's photometric.")<< ns_file_io);

	if (TIFFGetField(image, TIFFTAG_ROWSPERSTRIP, &tiff_info.rows_per_strip) == 0)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Image does not specify the number of rows per strip.")<< ns_file_io);
	//else cerr << "Rows per strip: " << tiff_info.rows_per_strip << "\n";

	//char * desc;
	//if (TIFFGetField(image, NS_METADATA_TAG,&desc))
	//	properties.description = desc;
	#ifdef NS_STORE_METATADATA_IN_TIFFTAGS
		ns_long_string_tifftag_encoder enc;
	#endif
	#ifdef NS_STORE_METADATA_IN_XMP
		ns_long_string_xmp_encoder_tiff enc;
	#endif
	enc.read(properties.description,image);

	tiff_info.stripsize = TIFFStripSize(image);
	tiff_info.number_of_strips = TIFFNumberOfStrips(image);

	unsigned long tiles = 0;
	
    if (TIFFGetField(image, TIFFTAG_TILEWIDTH, &tiles) != 0 && tiles != 0)
		ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Image is stored as tiles.")<< ns_file_io);

	switch(photometric){
		case PHOTOMETRIC_RGB: properties.components = 3; break;
		case PHOTOMETRIC_MINISWHITE: properties.components = 1; break;
		case PHOTOMETRIC_MINISBLACK: properties.components = 1; break;
		default:ns_throw_exception(ns_ex("ns_tiff_image_input_stream::Image posesses unknown photometric.")<< ns_file_io);
	}	
	if (tiff_info.stripsize < properties.width*properties.components*component_size)
		ns_throw_exception(ns_ex("Strip size is smaller than a single line!")<< ns_file_io);
	if (tiff_info.stripsize % (properties.width*properties.components*component_size) != 0)
		ns_throw_exception(ns_ex("Strip size does not line up evently with rows.")<< ns_file_io);
	//load buffer on first read.
	tiff_info.rows_read_from_current_strip = tiff_info.rows_per_strip;
}
