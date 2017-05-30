#ifndef NS_IMAGE_STATISTICS
#define NS_IMAGE_STATISTICS


#ifndef NS_NO_SQL
#include "ns_image_server_images.h"
#include "ns_sql.h"
#endif

class ns_histogram_sql_converter{
public:
	//stores histograms in the database.
	//histograms are binned by 2 values and the sum stored
	//in 32 bits, for a total of 4096 bytes, stored in a 512 byte blob
	typedef std::vector<unsigned long> ns_histogram_type;
	
#ifndef NS_NO_SQL
	static void insert_histogram(const ns_histogram_type & histogram,ns_sql & sql);
#endif
	static void extract_histogram(const std::string & data, std::vector<unsigned long> & histogram);
};

struct ns_image_intensity_statistics{
	ns_image_intensity_statistics():mean(0),
		variance(0),
		entropy(0),
		bottom_percentile_average(0),
		top_percentile_average(0){}

	double mean,
		variance,
		entropy,
		bottom_percentile_average,
		top_percentile_average;

	void set_as_zero(){
		mean=0;
		variance=0;
		entropy=0;
		bottom_percentile_average=0;
		top_percentile_average=0;
	}
	void operator +=(const ns_image_intensity_statistics & r){
		mean+=r.mean;
		variance+=r.variance;
		entropy+=r.entropy;
		bottom_percentile_average+=r.bottom_percentile_average;
		top_percentile_average+=r.top_percentile_average;
	}
	void operator/=(const double r){
		mean/=r;
		variance/=r;
		entropy/=r;
		bottom_percentile_average/=r;
		top_percentile_average/=r;
	}
};

struct ns_image_object_statistics{
	ns_image_object_statistics():area_mean(0),
		   area_variance(0),
		   length_mean(0),
		   length_variance(0),
		   width_mean(0),
		   width_variance(0),count(0){}
	unsigned long count;
	double area_mean,
		   area_variance,
		   length_mean,
		   length_variance,
		   width_mean,
		   width_variance;
	ns_image_intensity_statistics relative_intensity,
								  absolute_intensity;

	void set_as_zero(){
		count = 0;
		area_mean = 0;
		area_variance = 0;
		length_mean = 0;
		length_variance = 0;
		width_mean = 0;
		width_variance = 0;
		relative_intensity.set_as_zero();
		absolute_intensity.set_as_zero();
	}

	void operator+=(const ns_image_object_statistics & r){
		area_mean+=         r.area_mean;
		area_variance+=	    r.area_variance;
		length_mean+=	    r.length_mean;
		length_variance+=   r.length_variance;
		width_mean+=		r.width_mean;
		width_variance+=	r.width_variance;
		count += r.count;
		relative_intensity+=r.relative_intensity;
		absolute_intensity+=r.absolute_intensity;
	}
	void operator/=(const double & r){
		area_mean/=         r;
		area_variance/=	  r;  
		length_mean/=	    r;
		length_variance/=   r;
		width_mean/=		r;
		width_variance/=	r;
		count = (unsigned long)(count/r);
		relative_intensity/=r;
		absolute_intensity/=r;
	}
};

class ns_image_statistics{
public:
	ns_64_bit db_id;
	ns_image_statistics():
		histogram(256,0),
		size(0,0){}

	ns_vector_2i size;
	ns_image_object_statistics worm_statistics,
							   non_worm_statistics;

	ns_image_intensity_statistics image_statistics;

	void set_as_zero(){
		size = ns_vector_2i(0,0);
		db_id = 0;
		histogram.resize(0);
		histogram.resize(256,0);
		worm_statistics.set_as_zero();
		non_worm_statistics.set_as_zero();
		image_statistics.set_as_zero();
	}

	ns_histogram_sql_converter::ns_histogram_type histogram;
	void calculate_statistics_from_histogram();
	
#ifndef NS_NO_SQL
	bool submit_to_db(ns_64_bit & id,ns_sql & sql,bool include_image_stats=true,bool include_worm_stats=true);
	void calculate_statistics_from_image(ns_image_server_image & im,ns_sql & sql);
	void from_sql_result(const ns_sql_result_row & res);
#endif

	static std::string produce_sql_query_stub();
	static unsigned long sql_query_stub_field_count();
	void operator +=(const ns_image_statistics & r){
		size+=r.size;
		worm_statistics+=r.worm_statistics;
		non_worm_statistics+=r.non_worm_statistics;
		image_statistics+=r.image_statistics;
		for (unsigned int i = 0; i < histogram.size(); i++)
			histogram[i]+=r.histogram[i];
	}
	void operator/=(const double & r){
		size = ns_vector_2i((int)(size.x/r),(int)(size.y/r));
		worm_statistics/=r;
		non_worm_statistics/=r;
		image_statistics/=r;
		for (unsigned int i = 0; i < histogram.size(); i++)
			histogram[i] = (unsigned long)(histogram[i]/r);
	}
};
class ns_detected_worm_info;
void ns_summarize_stats(const std::vector<const ns_detected_worm_info *> & worms, ns_image_object_statistics & stats);

#endif
