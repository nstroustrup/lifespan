#ifndef NS_CAPTURED_IMAGE_STATISTICS_SET
#define NS_CAPTURED_IMAGE_STATISTICS_SET
#ifndef NS_NO_SQL
#include "ns_image_server_sql.h"
#endif
#include "ns_vector.h"
#include "ns_image_statistics.h"
#include "ns_survival_curve.h"

struct ns_capture_scan_statistics{
	enum {running_kernel_width_hours = 6};
	long scheduled_time_date,
		start_time_date,
		data_start_time_date,
		stop_time_date;

	long time_spent_off_immediately_before_starting_scan,
		 time_spent_off_after_finishing_scan;

	bool missed,
		 problem;

	long scheduled_time,  //time in seconds after first scan of that sample
		 start_time,
		 data_start_time,
		 stop_time;

	long date_of_first_sample_scan;

	ns_image_statistics image_stats;
	ns_vector_2d scan_position,
				 scan_size;
	
	double time_spent_reading_from_device,
		time_spent_writing_to_disk,
		total_time_during_read,
		time_during_transfer_to_long_term_storage,
		time_during_deletion_from_local_storage,
		total_time_spent_during_programmed_delay,
		smoothed_scanning_duration,
		scanning_duration_variation;

	void set_as_zero();

	void operator +=(const ns_capture_scan_statistics & r);

	void operator /=(const double & r);

	float scan_rate_inches_per_second() const;
	float smoothed_scan_rate_inches_per_second() const;


	long scanning_duration() const;
	long starting_delay() const;
	long warm_up_duration() const;

	float transfer_efficiency() const;

	static void out_jmp_header(std::ostream & o, const std::string & delimeter="\n");

	template<class T>
	static std::string oz(const T & v){
		if (v==0)
			return "";
		else return ns_to_string(v);
	}
	template<class T, class T2>
	static std::string oz(const T & val,const T2 & condition){
		if (condition==0)
			return "";
		else return ns_to_string(val);
	}

	void output_jmp_format(std::ostream & o, const ns_vector_2d & position, const ns_vector_2d & size,const std::string & delimeter="\n") const;

};

class ns_capture_sample_image_statistics{
public:
	unsigned long sample_id;
	std::string experiment_name,
		sample_name,
		device_name;
	ns_vector_2d position,size;
	unsigned long date_of_first_sample_scan;
	
#ifndef NS_NO_SQL
	void load_from_db(unsigned long id,ns_sql & sql);
#endif
	void calculate_running_statistics();

	static void output_jmp_header(std::ostream & o, const std::string & delimeter="\n");

	void output_jmp_format(std::ostream & o, const std::string & delimeter="\n" );

	//std::vector<long> movement_intervals;
	//std::vector<long> movement_times;

	std::vector<ns_capture_scan_statistics> scans;
};


struct ns_capture_sample_region_data_timepoint{
	unsigned long time;
	ns_image_statistics statistics;
	ns_capture_scan_statistics sample_statistics;
	bool timepoint_is_censored,
		 timepoint_has_a_problem;

	void set_as_zero();
	void operator+=(const ns_capture_sample_region_data_timepoint & r);
	void operator/=(const double & r);

	static void output_jmp_header(const std::string & suffix,std::ostream & o, const std::string & delimeter="\n");
	static void output_blank_jmp_data(std::ostream & o, const std::string & delimeter="\n");
	void output_jmp_data(std::ostream & o, const unsigned long start_time,const bool & censored, const bool & timepoint_has_a_problem, const std::string & delimeter="\n") const;
};

class ns_capture_sample_region_data{
public:

	ns_region_metadata metadata;

	unsigned long start_time;

	bool censored,
		 excluded;

	std::map<unsigned long,ns_capture_sample_region_data_timepoint *> timepoints_sorted_by_time;

	std::vector<ns_capture_sample_region_data_timepoint> timepoints;

	void generate_timepoints_sorted_by_time();
	
	void generate_summary_info(ns_capture_sample_region_data_timepoint & mean_timepoint,ns_capture_sample_region_data_timepoint & first_timepoint,ns_capture_sample_region_data_timepoint & last_timepoint);
	
#ifndef NS_NO_SQL
	void load_from_db(const unsigned long region_id_, 
					  const ns_region_metadata & metadata_,
					  const bool region_is_censored,
					  const bool region_is_excluded,
					  ns_sql & sql);
#endif
	
	void set_sample_info(const ns_capture_sample_image_statistics & sample_stats);

	static void output_region_data_in_jmp_format_header(const std::string & suffix,std::ostream & o);

	void output_region_data_in_jmp_format(std::ostream & o, const std::string & delimeter="\n");
};

struct ns_whole_device_activity_timepoint{
	ns_whole_device_activity_timepoint(){}
	ns_whole_device_activity_timepoint(ns_capture_sample_image_statistics * sample_,ns_capture_scan_statistics * scan_):sample(sample_),scan(scan_){}
	ns_capture_sample_image_statistics * sample;
	ns_capture_scan_statistics * scan;
};

typedef std::map<unsigned long,ns_whole_device_activity_timepoint> ns_whole_device_activity_aggregator;
typedef std::map<std::string,ns_whole_device_activity_aggregator> ns_whole_device_activity_aggregator_list;

class ns_capture_sample_statistics_set{
public:
	std::vector<ns_capture_sample_image_statistics> samples;
	
	std::map<unsigned long,ns_capture_sample_image_statistics *> samples_sorted_by_id;

	ns_whole_device_activity_aggregator_list device_list;
	
#ifndef NS_NO_SQL
	void load_whole_experiment(const unsigned long experiment_id, ns_sql & sql);
#endif
	void output_scanner_activity_plot(std::ostream & o);

	//build a list of all activity on each scanner
	void calculate_scanner_behavior();
};

class ns_capture_sample_region_statistics_set{
public:
	std::vector<ns_capture_sample_region_data> regions;
	std::map<unsigned long,ns_capture_sample_region_data *> regions_sorted_by_id;

	void build_id_mapping();

	void set_sample_data(const ns_capture_sample_statistics_set & samples);

	static void output_plate_statistics_with_mortality_data_header(std::ostream & o);

//	void output_plate_statistics_with_mortality_data(const ns_survival_data_summary_aggregator & survival_data, std::ostream & o);
	
#ifndef NS_NO_SQL
	void load_whole_experiment(const unsigned long experiment_id,ns_sql & sql);
#endif

};
#endif
