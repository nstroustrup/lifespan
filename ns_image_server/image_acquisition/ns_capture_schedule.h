#ifndef NS_CAPTURE_SCHEDULE_H
#define NS_CAPTURE_SCHEDULE_H
#include "ns_ex.h"
#include "ns_image_server_sql.h"
#include "ns_vector.h"
#include <algorithm>


struct ns_sample_capture_specification{
	unsigned long sample_id;
	std::string sample_name,
				capture_configuration_parameters,
				device;
				
	long			resolution;
	long desired_minimum_capture_duration;
	double x_position,  //in inches
			y_position,
			width,
			height;
	ns_sample_capture_specification():internal_schedule_id(-1){}

	std::string capture_parameters()const {
		return capture_configuration_parameters + " " + scan_area_string() + " --resolution=" + ns_to_string(resolution);
	}
	
	std::string scan_area_string() const{
		return std::string("-l ") + ns_to_string(x_position) + "in -t " + ns_to_string(y_position)
				      + "in -x "  + ns_to_string(width) + 	  "in -y " + ns_to_string(height) + "in";
	}
	
	void from_scan_area_string(const std::string & scan_area);

	std::string generate_scan_area_string(const int decimal_points=3) const{
		return ns_to_string_short(x_position,decimal_points) + "in," + ns_to_string_short(y_position,decimal_points) + "in," + 
			   ns_to_string_short(width,decimal_points) + "in," + ns_to_string_short(height,decimal_points) + "in";
	}
	static double get_inches(const std::string & s);
	long internal_schedule_id;
};

class ns_sample_capture_specification_sorter{
	public:
	bool operator()(const ns_sample_capture_specification * l, const ns_sample_capture_specification * r) const{
		if (l->x_position != r->x_position)
			return (l->x_position < r->x_position);
		if (l->y_position != r->y_position)
			return (l->x_position < r->x_position);
		if (l->width != r->width)
			return (l->width < r->width);
		if (l->height != r->height)
			return (l->height < r->height);
		return false;
	}
};

class ns_capture_schedule;

class ns_device_capture_schedule_sample_group{
public:
	std::vector<ns_sample_capture_specification *> samples;
	ns_capture_schedule * schedule;
};

class ns_device_capture_schedule{
public:
	ns_device_capture_schedule(){}
	std::string device_name;
	//all the samples for the current device, grouped (and labeled) by their common capture schedule
	typedef std::map<ns_capture_schedule *,ns_device_capture_schedule_sample_group> ns_sample_group_list;
	ns_sample_group_list sample_groups;
	
	unsigned long effective_device_period;
	unsigned long number_of_consecutive_captures_per_sample;
	
};

typedef std::map<std::string,ns_device_capture_schedule> ns_device_schedule_list;
	typedef std::map<std::string,unsigned long> ns_device_start_offset_list;

class ns_capture_schedule{
	public:
	ns_capture_schedule(const unsigned long internal_id_):internal_id(internal_id_),start_time(0),duration(0),device_capture_period(0){}
	bool use_all_samples;
	std::vector<ns_sample_capture_specification *> samples;
	unsigned long start_time;
	unsigned long stop_time;
	unsigned long duration;
	unsigned long device_capture_period;
	unsigned long number_of_consecutive_captures_per_sample;

	void populate_sample_names_from_list(const std::string & slist,std::vector<ns_sample_capture_specification> & samples_);
	static unsigned long decode_time_string(const std::string & s);
	static std::string time_string(unsigned long s);

	std::string sample_name_list;
	unsigned long effective_start_time;
	unsigned long effective_stop_time;
	
	ns_device_schedule_list device_schedules;

	ns_device_start_offset_list device_start_offsets;
	unsigned long internal_id;
};



class ns_experiment_capture_specification{
public:
	typedef enum{ns_none,ns_by_device} ns_default_sample_naming;

	ns_experiment_capture_specification():experiment_id(0),default_sample_naming(ns_none),device_schedule_produced(false),image_resolution(-1){}
	ns_64_bit experiment_id;
	std::string name,
		 default_capture_configuration_parameters;
	long image_resolution;
	
	unsigned long default_desired_minimum_capture_duration;
	ns_default_sample_naming default_sample_naming;
	std::vector<ns_sample_capture_specification> samples;
	std::vector<ns_capture_schedule> capture_schedules;
	
	
	static void confirm_valid_name(const std::string & name);
	
	std::string submit_schedule_to_db(ns_sql & sql,bool actually_write=false,bool overwrite_previous=false);
	void produce_device_schedule();
	void check_samples_and_fill_in_defaults();

	void clear();
	void save_to_xml(std::string & o)const;
	void save_to_xml_file(const std::string & filename) const;

	void load_from_xml(const std::string & o);
	void load_from_xml_file(const std::string & o);
	private:
		bool device_schedule_produced;
};



#endif
