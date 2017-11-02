/*****************************************************************************
 *  All the important parts of the encoding process are drawn verbatim from the source code
 *  of the xvid_encraw utility as detailed below.
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Console based test application  -
 *
 *  Copyright(C) 2002-2003 Christoph Lampert <gruel@web.de>
 *               2002-2003 Edouard Gomez <ed.gomez@free.fr>
 *               2003      Peter Ross <pross@xvid.org>
 *******************************************************************************/
#include "ns_ex.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#ifndef _WIN32
#include <sys/time.h>
#else
//#include <winsock2.h>
//#include <windows.h>
#include <vfw.h>
#include <time.h>
#define XVID_AVI_INPUT
#endif

#include "xvid.h"
#include "ns_xvid.h"

#include "ns_image.h"
#include "ns_dir.h"
#include "ns_thread.h"
#include "ns_jpeg.h"
#include "ns_tiff.h"
#include "ns_ojp2k.h"
#include <algorithm>
#include "ns_image_easy_io.h"
#include "ns_font.h"


#define MAX_ZONES   64
#define ABS_MAXFRAMENR 9999


static void usage();

using namespace std;
	
#define IMAGE_SIZE(x,y) ((x)*(y)*3)

#define MAX(A,B) ( ((A)>(B)) ? (A) : (B) )
#define SMALL_EPS (1e-10)
#define SWAP(a) ( (((a)&0x000000ff)<<24) | (((a)&0x0000ff00)<<8) | \
				  (((a)&0x00ff0000)>>8)  | (((a)&0xff000000)>>24) )


ns_xvid_parameters ns_xvid_encoder::default_parameters(){
	ns_xvid_parameters 	params;
	params.NUM_ZONES = 0;
	params.ARG_STATS = 0;
	params.ARG_DUMP = 0;
	params.ARG_LUMIMASKING = 0;
	params.ARG_BITRATE = 0;
	params.ARG_SINGLE = 1;
	params.ARG_PASS1 = 0;
	params.ARG_PASS2 = 0;
	params.ARG_QUALITY = ME_ELEMENTS - 1;
	params.ARG_FRAMERATE = 25.00f;
	params.ARG_MAXFRAMENR = ABS_MAXFRAMENR;
	params.ARG_MAXKEYINTERVAL = 0;
	params.ARG_SAVEMPEGSTREAM = 1;
	params.ARG_SAVEINDIVIDUAL = 0;
	params.XDIM = 0;
	params.YDIM = 0;
	params.max_dimention = 0;
	params.ARG_BQRATIO = 150;
	params.ARG_BQOFFSET = 100;
	params.ARG_MAXBFRAMES = 0;
	params.ARG_PACKED = 0;
	params.ARG_DEBUG = 0;
	params.ARG_VOPDEBUG = 0;
	params.ARG_GREYSCALE = 0;
	params.ARG_QTYPE = 0;
	params.ARG_QMATRIX = 0;
	params.ARG_GMC = 0;
	params.ARG_INTERLACING = 0;
	params.ARG_QPEL = 0;
	params.ARG_TURBO = 1;
	params.ARG_VHQMODE = 1;
	params.ARG_BVHQ = 0;
	params.ARG_CLOSED_GOP = 0;
	return params;
}


template< class whole_image>
void load_file(const string & filename, whole_image & image){	
	//cerr << "Opening " << filename << "\n";
	string extension = ns_dir::extract_extension(filename);
	//open jpeg
	if (extension == "jpg"){
		ns_jpeg_image_input_file<ns_8_bit> jpeg_in;
		jpeg_in.open_file(filename);
		ns_image_stream_file_source<ns_8_bit> file_source(jpeg_in);
		file_source.pump(image,128);
	}
	//open tiff
	else if (extension == "tif" || extension == "tiff"){
		ns_tiff_image_input_file<ns_8_bit> tiff_in;
		tiff_in.open_file(filename);
		ns_image_stream_file_source<ns_8_bit > file_source(tiff_in);
		file_source.pump(image,128);
	}
	else if (extension == "jp2" || extension == "jpk"){
		ns_ojp2k_image_input_file<ns_8_bit> jp2k_in;
		jp2k_in.open_file(filename);
		ns_image_stream_file_source<ns_8_bit > file_source(jp2k_in);
		file_source.pump(image,128);
	}

	else throw ns_ex("Unknown file type");
}		

/* Return time elapsed time in miliseconds since the program started */
static double  msecond() {
#ifndef _WIN32
	struct timeval tv;

	gettimeofday(&tv, 0);
	return (tv.tv_sec * 1.0e3 + tv.tv_usec * 1.0e-3);
#else
	clock_t clk;

	clk = clock();
	return (clk * 1000.0 / CLOCKS_PER_SEC);
#endif
}


void ns_xvid_encoder::run_from_directory(const string & input_directory, ns_xvid_parameters & p, const string & output_file, const ns_video_region_specification region_spec){
	string inp = ns_dir::extract_path(input_directory);
	cerr << "Loading files from " << inp<< "...";
	ns_dir dir;
	dir.load(inp);
	cerr << "Loaded " << dir.files.size() << " files.  Sorting...";
	std::sort(dir.files.begin(),dir.files.end());
	cerr << "Done.\n";
	for (unsigned int i = 0; i < dir.files.size(); i++)
		dir.files[i] = inp  + DIR_CHAR + dir.files[i];
	run(dir.files,p,output_file,region_spec);
}

ns_thread_return_type ns_file_loader_load_next(void * arg);

///Implements multithreaded file loading--the next image in the movie
///is loaded while the current one is being encoded.
class ns_file_loader{
public:
	ns_file_loader(){}

	void done_with_current_file(const string & next_image_filename){
		next_filename = next_image_filename;
		next_is_ready = false;
		ns_thread thread;
		thread.run(&ns_file_loader_load_next,this);
		thread.detach();
	}

	ns_image_standard & get_current_file(){
		while(!next_is_ready) //wait for file to be loaded
			ns_thread::sleep(1);
		
		//if an exception was thrown by the loading thread it will be stored here
		if (loading_error.text().size() != 0) 
			throw loading_error;

		return buf;
	}	
	
private:
	friend ns_thread_return_type ns_file_loader_load_next(void * arg);

	void load_next(){
		next_is_ready = false;
		try{
			load_file(next_filename,buf);
			loading_error.clear_text(); //clear any previous loading errors that occurred last cycle.
		}
		catch(std::exception & e){
			loading_error = ns_ex(e);
		}
		catch(...){
			loading_error << "Unspecified Error";
		}
		next_is_ready = true;
	}

	ns_image_standard buf;
	bool next_is_ready;
	ns_ex loading_error;
	string next_filename;
};

ns_thread_return_type ns_file_loader_load_next(void * arg){
	ns_file_loader * file(static_cast<ns_file_loader *>(arg));
	
		file->load_next();

	//ns_thread thread;
	//thread.get_current_thread();
	//thread.detach();
	return 0;
}

void ns_xvid_encoder::run(const vector<string> & input_files,  ns_xvid_parameters & p, const string & output_file, const ns_video_region_specification region_spec, const vector<std::string> & labels, const vector<ns_vector_2i> & image_offsets){

	ns_video_region_specification spec = region_spec;
	ns_file_loader file_loader;

	if (p.ARG_SAVEMPEGSTREAM) {
		if (p.ARG_MAXFRAMENR == 0 || p.ARG_MAXFRAMENR > (int)input_files.size())
			p.ARG_MAXFRAMENR = (int)input_files.size();
	
		if (p.ARG_FRAMERATE == 0)
			p.ARG_FRAMERATE = 5;
		if (input_files.size() == 0)
			throw ns_ex("No files found in directory.");
	}
//	cerr << "FRAMERATE: " << p.ARG_FRAMERATE << "\n";

	if (p.ARG_QUALITY < 0 ) {
		p.ARG_QUALITY = 0;
	} else if (p.ARG_QUALITY >= ME_ELEMENTS) {
		p.ARG_QUALITY = ME_ELEMENTS - 1;
	}

	if (p.ARG_FRAMERATE <= 0) 
		throw ns_ex("Invalid Framerate: ") << p.ARG_FRAMERATE;
	

	if (p.ARG_MAXFRAMENR <= 0) 
		throw ns_ex("Invalid number of frames: ") << p.ARG_MAXFRAMENR;

	string filename;

	ns_xvid_encoder_results r;

	

	file_loader.done_with_current_file(input_files[0]);
	ns_image_standard & image = file_loader.get_current_file();
	filename = input_files[0];

	if (!spec.is_specified()){
		spec.position_x = spec.position_y = 0;
		spec.width = image.properties().width;
		spec.height = image.properties().height;
	}
	else{
		if (spec.position_x < 0 ||  spec.width < 0 ||
			spec.position_y < 0 ||  spec.height < 0)
			throw ns_ex("ns_xvid::Invalid subregion specification: ");
		//if both height or width are left blank, fill in the entire region
		if (spec.width == 0)
			spec.width = (long)image.properties().width - spec.position_x;
		if (spec.height == 0)
			spec.height = (long)image.properties().height - spec.position_y;

		if (spec.position_x + spec.width > (long)image.properties().width ||
			spec.position_y + spec.height > (long)image.properties().height){
			cerr << "ns_xvid::Out of bounds subregion specification, cropping to max dimensions.\n";
			spec.height = (long)image.properties().height - spec.position_y;
			spec.width = (long)image.properties().width - spec.position_x;
		}
	}

	
	

	if (p.XDIM == 0 && p.YDIM == 0 && p.max_dimention == 0){
		p.XDIM = spec.width;
		p.YDIM = spec.height;
	}
	if (p.max_dimention != 0){
		if (spec.width < p.max_dimention && spec.height < p.max_dimention){
			p.XDIM = spec.width;
			p.YDIM = spec.height;
		}
		else{
		
			if (spec.width > spec.height)
				p.XDIM = p.max_dimention;
			else p.YDIM = p.max_dimention;

			//if only one dimention is specified, resize the other one to maintain aspect ration
			if (p.XDIM != 0 && p.YDIM == 0)
				p.YDIM = (unsigned int)(spec.height*(((float)p.XDIM)/spec.width));
			else if (p.XDIM == 0 && p.YDIM != 0)
				p.XDIM = (unsigned int)(spec.width*(((float)p.YDIM)/spec.height));
		}
	}

	

	int result;
	FILE * out_file;

	p.XDIM -= p.XDIM%2; //Round to even dimentions, otherwise the encoder chokes.
	p.YDIM -= p.YDIM%2;

	ns_vector_2d text_resample_factor(spec.width/(double)p.XDIM,spec.height/(double)p.YDIM);

	{
		ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
		ns_font * font(0);
		if (labels.size() > 0)
			font = &font_server.get_default_font();


		if (!spec.label_info_is_specified() && labels.size() > 0) {
			spec.label_size = spec.height / 50;
			font->set_height(spec.label_size);
			unsigned long w(0);
			for (unsigned int i = 0; i < labels.size(); i++) {
				unsigned long ww(font->text_size(labels[i]).w);
				if (w < ww)
					w = ww;
			}
			spec.label_position_x = spec.width - (3 * w) / 2;
			spec.label_position_y = spec.height - 3 * spec.label_size / 2;
		}
		font_lock.release();
	}

	cerr << input_files.size() << " frames: ";
	if (spec.width != p.XDIM || spec.height != p.YDIM)
		cerr << "resampling " << spec.width << "x" << spec.height << " -> " << "" << p.XDIM << "x" << p.YDIM << "\n";
	else cerr << "retaining size " << p.XDIM << "x" << p.YDIM <<"\n";
	//exit(1);
	in_buffer = 0;
	mp4_buffer = 0;
	if (4*IMAGE_SIZE(p.XDIM, p.YDIM) == 0)
		throw ns_ex("ns_xvid::First frame of movie had (0,0) dimensions.");

	try{

		/* this should really be enough memory (4x times as large as original image!) */
		try{
			mp4_buffer = new unsigned char[4*IMAGE_SIZE(p.XDIM, p.YDIM)];
		}
		catch(...){
			throw ns_ex("ns_xvid::Could not allocate mp4 buffer") << ns_memory_allocation;
		}
	//cerr << "Done allocating buffers.\nInitializing encoder.";
	/*****************************************************************************
	 *                            XviD PART  Start
	 ****************************************************************************/


		result = enc_init(p,r,p.use_assembler);
	//	cerr << "Done initializing encoder.\n";
		if (result) 
			throw ns_ex("Encore INIT problem, return value ") <<  result;
		//cerr << "Done initializing encoder.\n";

	/*****************************************************************************
	 *                            Main loop
	 ****************************************************************************/


		if (p.ARG_SAVEMPEGSTREAM ) {

			if ((out_file = fopen(output_file.c_str(), "w+b")) == NULL) 
				throw ns_ex("Error opening output file ") << output_file;

		}
		else 
			out_file = NULL;
		

	/*****************************************************************************
	 *                       Encoding loop
	 ****************************************************************************/

		int totalsize = 0;

		int result = 0;

		int input_num = 0;				/* input frame counter */
		int output_num = 0;				/* output frame counter */
		in_buffer = 0;
		string filename;	
		
		double enctime;
		double totalenctime = 0.;

		float totalPSNR[3] = {0., 0., 0.};

		unsigned int last_frame = 0;

		ns_image_standard sub_region_temp;

		ns_image_standard	  resampled_temp;
		do {
		//	cerr << "Running Loop\n";
			const char *type;

			if (input_num >= p.ARG_MAXFRAMENR) {
				result = 1;
			}

			if (!result) {
				if (p.ARG_SAVEMPEGSTREAM){
					bool loaded = false;
					
					try{
						file_loader.get_current_file();
						last_frame = input_num;
					}
					catch(ns_ex & ex){
						cerr << ex.text() << "\n";
						file_loader.done_with_current_file(input_files[last_frame]);
						file_loader.get_current_file();
					}
					filename = input_files[input_num];
				}
				if (in_buffer != 0)
					delete[] in_buffer;
				ns_image_standard * subregion;
				//if we're using the entire image
				bool using_entire_image = (spec.width == file_loader.get_current_file().properties().width && 
										   spec.height == file_loader.get_current_file().properties().height);
					
				if (using_entire_image)
					subregion = &file_loader.get_current_file();
				else{
					ns_image_standard & im(file_loader.get_current_file());
					ns_image_properties prop(im.properties());
					if (spec.position_x >= (long)prop.width)  throw ns_ex("Frame has incorrect dimensions:");
					if (spec.position_y >= (long)
						prop.height) throw ns_ex("Frame has incorrect dimensions:");

					prop.width = spec.width; 
					prop.height = spec.height;

					if (spec.height + spec.position_y >= (long)im.properties().height)
						prop.height = im.properties().height - spec.position_y;
					if (spec.width + spec.position_x >= (long)im.properties().width)
						prop.width = im.properties().width - spec.position_x;

					sub_region_temp.init(prop);
					for (unsigned int y = 0; y < prop.height; y++)
						for (unsigned int x = 0; x < prop.components*prop.width; x++)
							sub_region_temp[y][x] = im[y+spec.position_y][x+prop.components*spec.position_x];
					subregion = &sub_region_temp;
				}
			
				{
					ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
					ns_font * font(0);
					if (labels.size() > 0) {
						font = &font_server.get_default_font();
						font->set_height(spec.label_size);
					}

					if (subregion->properties().width == p.XDIM && subregion->properties().height == p.YDIM) {
						if (font != 0) {
							font->draw(spec.label_position_x, spec.label_position_y, ns_color_8(255, 255, 255), labels[input_num], *subregion);
						}
						in_buffer = subregion->to_raw_buf(false);

					}
					else {
						subregion->resample(ns_image_properties(p.YDIM, p.XDIM, subregion->properties().components), resampled_temp);
						if (font != 0) {
							font->set_height(spec.label_size / text_resample_factor.y);
							font->draw(spec.label_position_x / text_resample_factor.x,
								spec.label_position_y / text_resample_factor.y,
								ns_color_8(255, 255, 255),
								labels[input_num],
								resampled_temp);
						}
						in_buffer = resampled_temp.to_raw_buf(false, 0, true);
					}
					font_lock.release();
				}
				result = 0;
				//start loading next frame simultaneously
				if (input_num+1 < p.ARG_MAXFRAMENR )
					file_loader.done_with_current_file(input_files[input_num+1]);
			}

	/*****************************************************************************
	 *                       Encode and decode this frame
	 ****************************************************************************/
			
			enctime = msecond();
			r.m4v_size =
				enc_main(!result ? in_buffer : 0, p,mp4_buffer,r);
			enctime = msecond() - enctime;

			/* Write the Frame statistics */

			printf("%5d: key=%i, time= %6.0f, len= %7d", !result ? input_num : -1,
				   r.key, (float) enctime, (int) r.m4v_size);

			if (r.stats_type > 0) {	/* !XVID_TYPE_NOTHING */

				switch (r.stats_type) {
				case XVID_TYPE_IVOP:
					type = "I";
					break;
				case XVID_TYPE_PVOP:
					type = "P";
					break;
				case XVID_TYPE_BVOP:
					type = "B";
					break;
				case XVID_TYPE_SVOP:
					type = "S";
					break;
				default:
					type = "U";
					break;
				}

				printf(" | type=%s, quant= %2d, len= %7d", type, r.stats_quant,
					   r.stats_length);

	#define SSE2PSNR(sse, width, height) ((!(sse))?0.0f : 48.131f - 10*(float)log10((float)(sse)/((float)((width)*(height)))))

				if (p.ARG_STATS) {
					printf(", psnr y = %2.2f, psnr u = %2.2f, psnr v = %2.2f",
						   SSE2PSNR(r.sse[0], p.XDIM, p.YDIM), SSE2PSNR(r.sse[1], p.XDIM / 2,
																  p.YDIM / 2),
						   SSE2PSNR(r.sse[2], p.XDIM / 2, p.YDIM / 2));

					totalPSNR[0] += SSE2PSNR(r.sse[0], p.XDIM, p.YDIM);
					totalPSNR[1] += SSE2PSNR(r.sse[1], p.XDIM/2, p.YDIM/2);
					totalPSNR[2] += SSE2PSNR(r.sse[2], p.XDIM/2, p.YDIM/2);
				}

			}
	#undef SSE2PSNR

			printf("\n");

			if (r.m4v_size < 0) {
				break;
			}

			/* Update encoding time stats */
			totalenctime += enctime;
			totalsize += r.m4v_size;

	/*****************************************************************************
	 *                       Save stream to file
	 ****************************************************************************/

			if (r.m4v_size > 0 && p.ARG_SAVEMPEGSTREAM) {

				/* Save single files */
				if (p.ARG_SAVEINDIVIDUAL) {
					FILE *out;
					string fname = output_file + ns_to_string(output_num) + ".m4v";
					out = fopen(fname.c_str(), "w+b");
					fwrite(mp4_buffer, r.m4v_size, 1, out);
					fclose(out);
					output_num++;
				}

				/* Save ES stream */
				if (out_file)
					fwrite(mp4_buffer, 1, r.m4v_size, out_file);
			}

			input_num++;

		} while (1);



	/*****************************************************************************
	 *         Calculate totals and averages for output, print results
	 ****************************************************************************/

		printf("Tot: enctime(ms) =%7.2f,               length(bytes) = %7d\n",
			   totalenctime, (int) totalsize);

		if (input_num > 0) {
			totalsize /= input_num;
			totalenctime /= input_num;
			totalPSNR[0] /= input_num;
			totalPSNR[1] /= input_num;
			totalPSNR[2] /= input_num;
		} else {
			totalsize = -1;
			totalenctime = -1;
		}

		printf("Avg: enctime(ms) =%7.2f, fps =%7.2f, length(bytes) = %7d",
			   totalenctime, 1000 / totalenctime, (int) totalsize);
	   if (p.ARG_STATS) {
		   printf(", psnr y = %2.2f, psnr u = %2.2f, psnr v = %2.2f",
				  totalPSNR[0],totalPSNR[1],totalPSNR[2]);
		}
		printf("\n");
		if (enc_handle) {
			result = enc_stop();
			if (result)
				throw ns_ex("Encore RELEASE problem return value ") << result;
		}

		if (out_file)
			fclose(out_file);
		if (mp4_buffer != 0)
			delete[] mp4_buffer;
		if (in_buffer != 0)
			delete[] in_buffer;
	}
	catch(std::exception & exc){
		ns_ex ex(exc);
		if (mp4_buffer)
			delete[] mp4_buffer;
		if (in_buffer)
			delete[] in_buffer;

		throw ex;
	}
}





/*****************************************************************************
 *     Routines for encoding: init encoder, frame step, release encoder
 ****************************************************************************/

/* sample plugin */

int
rawenc_debug(void *handle,
			 int opt,
			 void *param1,
			 void *param2)
{
	switch (opt) {
	case XVID_PLG_INFO:
		{
			xvid_plg_info_t *info = (xvid_plg_info_t *) param1;

			info->flags = XVID_REQDQUANTS;
			return 0;
		}

	case XVID_PLG_CREATE:
	case XVID_PLG_DESTROY:
	case XVID_PLG_BEFORE:
		return 0;

	case XVID_PLG_AFTER:
		{
			xvid_plg_data_t *data = (xvid_plg_data_t *) param1;
			int i, j;

			printf("---[ frame: %5i   quant: %2i   length: %6i ]---\n",
				   data->frame_num, data->quant, data->length);
			for (j = 0; j < data->mb_height; j++) {
				for (i = 0; i < data->mb_width; i++)
					printf("%2i ", data->dquant[j * data->dquant_stride + i]);
				printf("\n");
			}

			return 0;
		}
	}

	return XVID_ERR_FAIL;
}


#define FRAMERATE_INCR 1001


/* Initialize encoder for first use, pass all needed parameters to the codec */
int
ns_xvid_encoder::enc_init(const ns_xvid_parameters & p,ns_xvid_encoder_results & r, const bool use_assembler)
{
	int xerr;
	//xvid_plugin_cbr_t cbr;
	xvid_plugin_single_t single;
	xvid_plugin_2pass1_t rc2pass1;
	xvid_plugin_2pass2_t rc2pass2;
	//xvid_plugin_fixed_t rcfixed;
	xvid_enc_plugin_t plugins[7];
	xvid_gbl_init_t xvid_gbl_init;
	xvid_enc_create_t xvid_enc_create;

	/*------------------------------------------------------------------------
	 * XviD core initialization
	 *----------------------------------------------------------------------*/

	/* Set version -- version checking will done by xvidcore */
	memset(&xvid_gbl_init, 0, sizeof(xvid_gbl_init));
	xvid_gbl_init.version = XVID_VERSION;
	xvid_gbl_init.debug = p.ARG_DEBUG;


	/* Do we have to enable ASM optimizations ? */
	if (use_assembler) {

#ifdef ARCH_IS_IA64
		xvid_gbl_init.cpu_flags = XVID_CPU_FORCE | XVID_CPU_ASM;
#else
		xvid_gbl_init.cpu_flags = 0;
#endif
	} else {
		xvid_gbl_init.cpu_flags = XVID_CPU_FORCE;
	}
	//cout << "A";
	/* Initialize XviD core -- Should be done once per __process__ */
	xvid_global(NULL, XVID_GBL_INIT, &xvid_gbl_init, NULL);

	/*------------------------------------------------------------------------
	 * XviD encoder initialization
	 *----------------------------------------------------------------------*/

	/* Version again */
	memset(&xvid_enc_create, 0, sizeof(xvid_enc_create));
	xvid_enc_create.version = XVID_VERSION;

	/* Width and Height of input frames */
	xvid_enc_create.width = p.XDIM;
	xvid_enc_create.height = p.YDIM;
	xvid_enc_create.profile = XVID_PROFILE_AS_L4;

	/* init plugins  */
	xvid_enc_create.zones = (xvid_enc_zone_t *)p.ZONES;
	xvid_enc_create.num_zones = p.NUM_ZONES;

	//cout << "B";
	xvid_enc_create.plugins = plugins;
	xvid_enc_create.num_plugins = 0;

	if (p.ARG_SINGLE) {
		memset(&single, 0, sizeof(xvid_plugin_single_t));
		single.version = XVID_VERSION;
		single.bitrate = p.ARG_BITRATE;

		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_single;
		plugins[xvid_enc_create.num_plugins].param = &single;
		xvid_enc_create.num_plugins++;
	}

	if (p.ARG_PASS2) {
		memset(&rc2pass2, 0, sizeof(xvid_plugin_2pass2_t));
		rc2pass2.version = XVID_VERSION;
		rc2pass2.filename = p.ARG_PASS2;
		rc2pass2.bitrate = p.ARG_BITRATE;

/*		An example of activating VBV could look like this 
		rc2pass2.vbv_size     =  3145728;
		rc2pass2.vbv_initial  =  2359296;
		rc2pass2.vbv_maxrate  =  4000000;
		rc2pass2.vbv_peakrate = 10000000;
*/

		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_2pass2;
		plugins[xvid_enc_create.num_plugins].param = &rc2pass2;
		xvid_enc_create.num_plugins++;
	}
//	cout << "C";

	if (p.ARG_PASS1) {
		memset(&rc2pass1, 0, sizeof(xvid_plugin_2pass1_t));
		rc2pass1.version = XVID_VERSION;
		rc2pass1.filename = p.ARG_PASS1;

		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_2pass1;
		plugins[xvid_enc_create.num_plugins].param = &rc2pass1;
		xvid_enc_create.num_plugins++;
	}

	if (p.ARG_LUMIMASKING) {
		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_lumimasking;
		plugins[xvid_enc_create.num_plugins].param = NULL;
		xvid_enc_create.num_plugins++;
	}

	if (p.ARG_DUMP) {
		plugins[xvid_enc_create.num_plugins].func = xvid_plugin_dump;
		plugins[xvid_enc_create.num_plugins].param = NULL;
		xvid_enc_create.num_plugins++;
	}

#if 0
	if (ARG_DEBUG) {
		plugins[xvid_enc_create.num_plugins].func = rawenc_debug;
		plugins[xvid_enc_create.num_plugins].param = NULL;
		xvid_enc_create.num_plugins++;
	}
#endif

	/* No fancy thread tests */
	xvid_enc_create.num_threads = 0;

	/* Frame rate - Do some quick float fps = fincr/fbase hack */
	if ((p.ARG_FRAMERATE - (int) p.ARG_FRAMERATE) < SMALL_EPS) {
		xvid_enc_create.fincr = 1;
		xvid_enc_create.fbase = (int) p.ARG_FRAMERATE;
	} else {
		xvid_enc_create.fincr = FRAMERATE_INCR;
		xvid_enc_create.fbase = (int) (FRAMERATE_INCR * p.ARG_FRAMERATE);
	}

//	cout << "D";
	/* Maximum key frame interval */
	if (p.ARG_MAXKEYINTERVAL > 0) {
		xvid_enc_create.max_key_interval = p.ARG_MAXKEYINTERVAL;
	}else {
		xvid_enc_create.max_key_interval = (int) p.ARG_FRAMERATE *10;
	}

	/* Bframes settings */
	xvid_enc_create.max_bframes = p.ARG_MAXBFRAMES;
	xvid_enc_create.bquant_ratio = p.ARG_BQRATIO;
	xvid_enc_create.bquant_offset = p.ARG_BQOFFSET;

	/* Dropping ratio frame -- we don't need that */
	xvid_enc_create.frame_drop_ratio = 0;

	/* Global encoder options */
	xvid_enc_create.global = 0;

	if (p.ARG_PACKED)
		xvid_enc_create.global |= XVID_GLOBAL_PACKED;

	if (p.ARG_CLOSED_GOP)
		xvid_enc_create.global |= XVID_GLOBAL_CLOSED_GOP;

	if (p.ARG_STATS)
		xvid_enc_create.global |= XVID_GLOBAL_EXTRASTATS_ENABLE;

//	cout << "E";
	/* I use a small value here, since will not encode whole movies, but short clips */
	xerr = xvid_encore(NULL, XVID_ENC_CREATE, &xvid_enc_create, NULL);
//	cout << "F";
	/* Retrieve the encoder instance from the structure */
	enc_handle = xvid_enc_create.handle;

	return (xerr);
}

int
ns_xvid_encoder::enc_stop()
{
	int xerr;

	/* Destroy the encoder instance */
	xerr = xvid_encore(enc_handle, XVID_ENC_DESTROY, NULL, NULL);

	return (xerr);
}

int
ns_xvid_encoder::enc_main(unsigned char *image, const ns_xvid_parameters & p, unsigned char *bitstream, ns_xvid_encoder_results & r){
	int ret;

	xvid_enc_frame_t xvid_enc_frame;
	xvid_enc_stats_t xvid_enc_stats;
	
	/* Version for the frame and the stats */
	memset(&xvid_enc_frame, 0, sizeof(xvid_enc_frame));
	xvid_enc_frame.version = XVID_VERSION;

	memset(&xvid_enc_stats, 0, sizeof(xvid_enc_stats));
	xvid_enc_stats.version = XVID_VERSION;

	/* Bind output buffer */
	xvid_enc_frame.bitstream = bitstream;
	xvid_enc_frame.length = -1;

	/* Initialize input image fields */
	if (image) {
		xvid_enc_frame.input.plane[0] = image;
		xvid_enc_frame.input.csp = XVID_CSP_BGR;
		xvid_enc_frame.input.stride[0] = p.XDIM*3;
	} else {
		xvid_enc_frame.input.csp = XVID_CSP_NULL;
	}

	/* Set up core's general features */
	xvid_enc_frame.vol_flags = 0;
	if (p.ARG_STATS)
		xvid_enc_frame.vol_flags |= XVID_VOL_EXTRASTATS;
	if (p.ARG_QTYPE)
		xvid_enc_frame.vol_flags |= XVID_VOL_MPEGQUANT;
	if (p.ARG_QPEL)
		xvid_enc_frame.vol_flags |= XVID_VOL_QUARTERPEL;
	if (p.ARG_GMC)
		xvid_enc_frame.vol_flags |= XVID_VOL_GMC;
	if (p.ARG_INTERLACING)
		xvid_enc_frame.vol_flags |= XVID_VOL_INTERLACING;

	/* Set up core's general features */
	xvid_enc_frame.vop_flags = vop_presets[p.ARG_QUALITY];

	if (p.ARG_VOPDEBUG) {
		xvid_enc_frame.vop_flags |= XVID_VOP_DEBUG;
	}

	if (p.ARG_GREYSCALE) {
		xvid_enc_frame.vop_flags |= XVID_VOP_GREYSCALE;
	}

	/* Frame type -- let core decide for us */
	xvid_enc_frame.type = XVID_TYPE_AUTO;

	/* Force the right quantizer -- It is internally managed by RC plugins */
	xvid_enc_frame.quant = 0;

	/* Set up motion estimation flags */
	xvid_enc_frame.motion = motion_presets[p.ARG_QUALITY];

	if (p.ARG_GMC)
		xvid_enc_frame.motion |= XVID_ME_GME_REFINE;

	if (p.ARG_QPEL)
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16;
	if (p.ARG_QPEL && (xvid_enc_frame.vop_flags & XVID_VOP_INTER4V))
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE8;

	if (p.ARG_TURBO)
		xvid_enc_frame.motion |= XVID_ME_FASTREFINE16 | XVID_ME_FASTREFINE8 | 
								 XVID_ME_SKIP_DELTASEARCH | XVID_ME_FAST_MODEINTERPOLATE | 
								 XVID_ME_BFRAME_EARLYSTOP;

	if (p.ARG_BVHQ) 
		xvid_enc_frame.vop_flags |= XVID_VOP_RD_BVOP;

	switch (p.ARG_VHQMODE) /* this is the same code as for vfw */
	{
	case 1: /* VHQ_MODE_DECISION */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		break;

	case 2: /* VHQ_LIMITED_SEARCH */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16_RD;
		break;

	case 3: /* VHQ_MEDIUM_SEARCH */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_CHECKPREDICTION_RD;
		break;

	case 4: /* VHQ_WIDE_SEARCH */
		xvid_enc_frame.vop_flags |= XVID_VOP_MODEDECISION_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_HALFPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE16_RD;
		xvid_enc_frame.motion |= XVID_ME_QUARTERPELREFINE8_RD;
		xvid_enc_frame.motion |= XVID_ME_CHECKPREDICTION_RD;
		xvid_enc_frame.motion |= XVID_ME_EXTSEARCH_RD;
		break;

	default :
		break;
	}


	if (p.ARG_QMATRIX) {

		/* We don't use special matrices */
		xvid_enc_frame.quant_intra_matrix = (unsigned char *)current_qmatrix_intra;
		xvid_enc_frame.quant_inter_matrix = (unsigned char *)current_qmatrix_inter;
	}
	else {
		/* We don't use special matrices */
		xvid_enc_frame.quant_intra_matrix = NULL;
		xvid_enc_frame.quant_inter_matrix = NULL;
	}

	/* Encode the frame */
	ret = xvid_encore(enc_handle, XVID_ENC_ENCODE, &xvid_enc_frame,
					  &xvid_enc_stats);

	r.key = (xvid_enc_frame.out_flags & XVID_KEYFRAME);
	r.stats_type = xvid_enc_stats.type;
	r.stats_quant = xvid_enc_stats.quant;
	r.stats_length = xvid_enc_stats.length;
	r.sse[0] = xvid_enc_stats.sse_y;
	r.sse[1] = xvid_enc_stats.sse_u;
	r.sse[2] = xvid_enc_stats.sse_v;

	return (ret);
}
   

const int ns_xvid_encoder::motion_presets[7] = {
	/* quality 0 */
	0,

	/* quality 1 */
	XVID_ME_ADVANCEDDIAMOND16,

	/* quality 2 */
	XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16,

	/* quality 3 */
	XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 |
	XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8,

	/* quality 4 */
	XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 |
	XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8 |
	XVID_ME_CHROMA_PVOP | XVID_ME_CHROMA_BVOP,

	/* quality 5 */
	XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 |
	XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8 |
	XVID_ME_CHROMA_PVOP | XVID_ME_CHROMA_BVOP,

	/* quality 6 */
	XVID_ME_ADVANCEDDIAMOND16 | XVID_ME_HALFPELREFINE16 | XVID_ME_EXTSEARCH16 |
	XVID_ME_ADVANCEDDIAMOND8 | XVID_ME_HALFPELREFINE8 | XVID_ME_EXTSEARCH8 |
	XVID_ME_CHROMA_PVOP | XVID_ME_CHROMA_BVOP
};

const int ns_xvid_encoder::vop_presets[7] = {
	/* quality 0 */
	0,

	/* quality 1 */
	0,

	/* quality 2 */
	XVID_VOP_HALFPEL,

	/* quality 3 */
	XVID_VOP_HALFPEL | XVID_VOP_INTER4V,

	/* quality 4 */
	XVID_VOP_HALFPEL | XVID_VOP_INTER4V,

	/* quality 5 */
	XVID_VOP_HALFPEL | XVID_VOP_INTER4V |
	XVID_VOP_TRELLISQUANT,

	/* quality 6 */
	XVID_VOP_HALFPEL | XVID_VOP_INTER4V |
	XVID_VOP_TRELLISQUANT | XVID_VOP_HQACPRED,

};

