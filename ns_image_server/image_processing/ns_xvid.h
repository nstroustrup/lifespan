/*
 *  All the important parts of the encoding process are drawn verbatim from the source code
 *  of the xvid_encraw utility as detailed below.
 *
 *  XVID MPEG-4 VIDEO CODEC
 *  - Console based test application  -
 *
 *  Copyright(C) 2002-2003 Christoph Lampert <gruel@web.de>
 *               2002-2003 Edouard Gomez <ed.gomez@free.fr>
 *               2003      Peter Ross <pross@xvid.org>
 */

#ifndef NS_XVID
#define NS_XVID

#include <vector>
#include <string>
#include "xvid.h"
#include "ns_vector.h"
#define MAX_ZONES   64

///Parameters used to fine-tune xvid compression of videos
struct ns_xvid_parameters{

	xvid_enc_zone_t ZONES[MAX_ZONES];
	int NUM_ZONES;
	int ARG_STATS;
	int ARG_DUMP;
	int ARG_LUMIMASKING;
	int ARG_BITRATE;
	int ARG_SINGLE;
	char *ARG_PASS1;
	char *ARG_PASS2;
	int ARG_QUALITY;
	float ARG_FRAMERATE;
	int ARG_MAXFRAMENR;
	int ARG_MAXKEYINTERVAL;
	int ARG_SAVEMPEGSTREAM;
	int ARG_SAVEINDIVIDUAL;
	int XDIM;
	int YDIM;
	int max_height;
	int max_width;
	int ARG_BQRATIO;
	int ARG_BQOFFSET;
	int ARG_MAXBFRAMES;
	int ARG_PACKED;
	int ARG_DEBUG;
	int ARG_VOPDEBUG;
	int ARG_GREYSCALE;
	int ARG_QTYPE;
	int ARG_QMATRIX;
	int ARG_GMC;
	int ARG_INTERLACING;
	int ARG_QPEL;
	int ARG_TURBO;
	int ARG_VHQMODE;
	int ARG_BVHQ;
	int ARG_CLOSED_GOP;

	unsigned char qmatrix_intra[64];
	unsigned char qmatrix_inter[64];
	bool use_assembler;

	void choose_bitrate_to_match_resolution(const long width, const long height, const bool hd);
};

///Results from a single round of xvid encoding
struct ns_xvid_encoder_results{
	int key;
	int stats_type;
	int stats_quant;
	int stats_length;
	int sse[3];
	int m4v_size;
};

struct ns_video_region_specification{
	typedef enum{ns_no_timestamp,ns_date_timestamp,ns_age_timestamp} ns_timestamp_type;
	ns_video_region_specification():position_x(0),position_y(0),width(0),height(0),time_at_which_population_had_zero_age(0),timestamp_type(ns_no_timestamp), output_at_high_definition(false){}
	ns_video_region_specification(const long px,const long py,const long w,const long h, const unsigned long t_start, const unsigned long t_stop, const ns_timestamp_type & timestamp_t, const unsigned long zero_time, const bool output_at_high_definition_):
		position_x(px),position_y(py),width(w),height(h),start_time(t_start),stop_time(t_stop),label_position_x(0),label_position_y(0),label_size(0),timestamp_type(timestamp_t),time_at_which_population_had_zero_age(zero_time), output_at_high_definition(output_at_high_definition_){}
	long position_x,
		position_y,
		width,
		height,
		start_time,
		stop_time;
	ns_timestamp_type timestamp_type;
	unsigned long time_at_which_population_had_zero_age;
	long label_position_x,
		 label_position_y,
		 label_size;
	bool output_at_high_definition;
	bool is_specified() const{return position_x>0 || position_y>0 || width>0 || height>0;}
	bool label_info_is_specified() const{return label_size > 0;}
};


#define ME_ELEMENTS (sizeof(ns_xvid_encoder::motion_presets)/sizeof(ns_xvid_encoder::motion_presets[0]))
#define VOP_ELEMENTS (sizeof(ns_xvid_encoder::vop_presets)/sizeof(ns_xvid_encoder::vop_presets[0]))

///Takes a series of still images and compiles them into an xvid-encoded video
class ns_xvid_encoder{
public:
	ns_xvid_encoder():enc_handle(0),in_buffer(0),mp4_buffer(0){}

	static ns_xvid_parameters default_parameters();

	///Compiles all images in the specified directory into a video, in alphabetical order
	void run_from_directory(const std::string & input_directory, ns_xvid_parameters & p, const std::string & output_file, const ns_video_region_specification region_spec = ns_video_region_specification());

	///Compiles all specified images into a video
	void run(const std::vector<std::string> & input_files, ns_xvid_parameters & p, const std::string & output_file, const ns_video_region_specification region_spec = ns_video_region_specification(),const std::vector<std::string> & labels=std::vector<std::string>(),const std::vector<ns_vector_2i> & image_offsets=std::vector<ns_vector_2i>());

	///various encoding constants
	static const int motion_presets[7];
	///various encoding constants
	static const int vop_presets[7];

private:
	
	///handle to the global xvid encoding engine
	void * enc_handle;
	int rawenc_debug(void *handle,int opt, void *param1, void *param2);
	///initializes the global xvid encoding engine
	int enc_init(const ns_xvid_parameters & p,ns_xvid_encoder_results & r, const bool use_assembler=true);

	///deallocates the global xvid encoding engine
	int enc_stop();

	///starts a round of encoding
	int enc_main(unsigned char *image, const ns_xvid_parameters & p, unsigned char *bitstream, ns_xvid_encoder_results & r);

	///input buffer for the current encoding round
	unsigned char *in_buffer;
	unsigned char * mp4_buffer; 
	unsigned char current_qmatrix_intra[64];
	unsigned char current_qmatrix_inter[64];
	
};
#endif
