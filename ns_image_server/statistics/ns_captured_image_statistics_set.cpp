#include "ns_captured_image_statistics_set.h"
#include <set>

void ns_capture_scan_statistics::set_as_zero(){
	date_of_first_sample_scan = 0;		
	scheduled_time		=	0;						
	start_time			=	0;						
	data_start_time		=	0;						
	stop_time			=	0;	
	scheduled_time_date	=	0;						
	start_time_date		=	0;						
	data_start_time_date=	0;						
	stop_time_date		=	0;							
	smoothed_scanning_duration	=	0;				
	scanning_duration_variation	=	0;				
	image_stats.set_as_zero();			
	scan_position				=	ns_vector_2i(0,0);				
	scan_size					=	ns_vector_2i(0,0);				
	time_spent_reading_from_device	=	0;			
	time_spent_writing_to_disk		=	0;			
	total_time_during_read			=	0;			
	time_during_transfer_to_long_term_storage	=	0;
	time_during_deletion_from_local_storage		=	0;
	total_time_spent_during_programmed_delay = 0;
	registration_offset = ns_vector_2i(0,0);
}
void ns_capture_scan_statistics::operator +=(const ns_capture_scan_statistics & r){
	date_of_first_sample_scan +=							r.date_of_first_sample_scan;
	scheduled_time		+=									r.scheduled_time;
	start_time			+=									r.start_time;
	data_start_time		+=									r.data_start_time;
	stop_time			+=									r.stop_time_date;
	scheduled_time_date		+=								r.scheduled_time_date;
	start_time_date			+=								r.start_time_date;
	data_start_time_date		+=							r.data_start_time_date;
	stop_time_date			+=								r.stop_time_date;
	smoothed_scanning_duration	+=							r.smoothed_scanning_duration;
	scanning_duration_variation	+=							r.scanning_duration_variation;
	image_stats					+=							r.image_stats;
	scan_position				+=							r.scan_position;
	scan_size					+=							r.scan_size;
	time_spent_reading_from_device	+=						r.time_spent_reading_from_device;
	time_spent_writing_to_disk		+=						r.time_spent_writing_to_disk;
	total_time_during_read			+=						r.total_time_during_read;
	time_during_transfer_to_long_term_storage	+=			r.time_during_transfer_to_long_term_storage;
	time_during_deletion_from_local_storage		+=			r.time_during_deletion_from_local_storage;
	total_time_spent_during_programmed_delay +=		r.total_time_spent_during_programmed_delay;
	registration_offset += r.registration_offset;
}
void ns_capture_scan_statistics::operator /=(const double & r){
	date_of_first_sample_scan = (long)date_of_first_sample_scan/r;	
	scheduled_time = (long)		(scheduled_time	/r);				
	start_time = (long)			(start_time	/r);				
	data_start_time = (long)	(data_start_time/r);							
	stop_time = (long)			(stop_time	/r);		
	scheduled_time_date = (long)		(scheduled_time_date	/r);				
	start_time_date = (long)			(start_time_date	/r);				
	data_start_time_date = (long)	(data_start_time_date/r);							
	stop_time_date = (long)			(stop_time_date	/r);					
	smoothed_scanning_duration = (long)(smoothed_scanning_duration/r);		
	scanning_duration_variation = (long)(scanning_duration_variation/r);			
	image_stats/=r;									
	scan_position/=r;								
	scan_size/=r;									
	time_spent_reading_from_device = (time_spent_reading_from_device/r);				
	time_spent_writing_to_disk = (time_spent_writing_to_disk/r);					
	total_time_during_read = (total_time_during_read/r);						
	time_during_transfer_to_long_term_storage = (time_during_transfer_to_long_term_storage/r);	
	time_during_deletion_from_local_storage =(time_during_deletion_from_local_storage/r);	
	total_time_spent_during_programmed_delay = (total_time_spent_during_programmed_delay/r);
	registration_offset = (registration_offset/r);
}

float ns_capture_scan_statistics::scan_rate_inches_per_second() const{if (scanning_duration() == 0.0) return 0; return (float)(scan_size.y/scanning_duration());}
float ns_capture_scan_statistics::smoothed_scan_rate_inches_per_second() const{if (smoothed_scanning_duration == 0.0) return 0; return (float)(scan_size.y/smoothed_scanning_duration);}


long ns_capture_scan_statistics::scanning_duration() const{
	if (stop_time == 0) return 0;
	return (stop_time-start_time);
}	
long ns_capture_scan_statistics::starting_delay() const{
	if (start_time == 0)
		return 0;
	return start_time-scheduled_time;
}
long ns_capture_scan_statistics::warm_up_duration() const{
	if (data_start_time == 0 || start_time == 0)
		return 0;
	return data_start_time - start_time;
}

float ns_capture_scan_statistics::transfer_efficiency() const{
	if (time_spent_writing_to_disk +time_spent_reading_from_device == 0)
		return 0;
	return time_spent_reading_from_device/(float)(time_spent_writing_to_disk + time_spent_reading_from_device);
}

void ns_capture_scan_statistics::out_jmp_header(std::ostream & o, const std::string & delimeter){
	o << "Date of First Sample Scan (Date), Scheduled Start of Scan (Date),Actual Start of Scan (Date),"
			"Scheduled Start of Scan (Days after Experiment Start),Actual Start of Scan (Days after Experiment Start),"  
			"Did the image server skip this scan?,"
			"Did an error occur during image capture?,"
			"Did the scan start and complete normally?,"
			"Delay in Actual Start from Schedule (min),"
			"Time Spent Off Prior to Scan (min),"
			"Time Spent Waiting for the Scanner to Warm up (s),"
			"Total Scan Duration (min),"
			"Time Spent Off After Scan (min),"
			"Smoothed Total Scan Duration (min),Standard Deviation in Total Scanning Duration (min),"
			"Scanner Bar Speed (inches/min),Smoothed Scanner Bar Speed (inches/min),"
			"TEST," //13
			"Time during scan spent reading data from device (min),"
			"Time during scan spent waiting for local disk write to complete (s),"
			"Delay deliberately injected during scan (min),"
			"Total Time Spent In Communication with the scanner (min),"
			"TEST2,"
		"Time after transfer spent writing data to Long Term Storage (s),"
		"Time after transfer spent deleting data from local disk (s),"
		"Transfer Efficiency, Sample Image Position X (inches), Sample Image Position Y (inches), Sample Image Width (inches), Sample Image Height (inches),"
		"Sample Image Intensity Average,Sample Image Intensity Standard Deviation,Sample Image Intensity Entropy,Sample Image Intensity Bottom Percentile,Sample Image Intensity Top Percentile,"
		"Image Registration X offset,Image Registration Y offset"
		<< delimeter;
}
	
void ns_capture_scan_statistics::output_jmp_format(std::ostream & o, const ns_vector_2d & position, const ns_vector_2d & size,const std::string & delimeter) const{
	o 	<< date_of_first_sample_scan << ","
		<< oz(scheduled_time_date) << ","
		<< oz(start_time_date) << ","
		<< oz(scheduled_time/(60*60*24.0),scheduled_time_date) << ","
		<< oz(start_time/(60.0*60*24),start_time_date) << ","
		<< (missed?"Skpped":"Not Skipped") << ","
		<< (problem?"Error":"No Error") << ","
		<< ((missed || problem)?("Skipped or Error"):"OK") << ",";
	if (stop_time_date == 0){
		for (unsigned int i = 0; i < 24; i++)
			o << ",";
		o << delimeter;
	}
	else {
	o	<< starting_delay()/60.0 << ","
		<< oz(time_spent_off_immediately_before_starting_scan/60.0,time_spent_off_immediately_before_starting_scan+1) << "," //-1 if not specified
		<< warm_up_duration() << "," 
		<< scanning_duration()/(60.0) << "," 
		<< oz(time_spent_off_after_finishing_scan/60.0,time_spent_off_after_finishing_scan+1) << "," //-1 if not specified
		<< smoothed_scanning_duration/60.0 << ","
		<< sqrt(scanning_duration_variation)/60.0 << ","
		<< scan_rate_inches_per_second()*60.0<< ","
		<< smoothed_scan_rate_inches_per_second()*60.0 << "," 
		<< "TEST,"
		<< time_spent_reading_from_device << ","
		<< time_spent_writing_to_disk << ","
		<< total_time_spent_during_programmed_delay << ","
		<< total_time_during_read << ","
		<< "TEST2,"
		<< time_during_transfer_to_long_term_storage<< ","
		<< time_during_deletion_from_local_storage << ","
		<< transfer_efficiency() <<","
		<< position.x << "," << position.y << ","
		<< size.x << "," << size.y << ","
		<< image_stats.image_statistics.mean<< ","
		<< image_stats.image_statistics.variance<< ","
		<< image_stats.image_statistics.entropy<< ","
		<< image_stats.image_statistics.bottom_percentile_average<< "," 
		<< image_stats.image_statistics.top_percentile_average << ","
		<< registration_offset.x << ","
		<< registration_offset.y
		<< delimeter;
	}
}
/*
ns_64_bit ns_atoi64(const char * s){
  #ifdef WIN32
  return _atoi64(s);
#else
  return atoll(s);
#endif
}*/
void ns_capture_sample_image_statistics::load_from_db(unsigned long id,ns_sql & sql){
		
	sample_id = id;
	sql << "SELECT name,device_name, position_x,position_y,size_x,size_y, experiment_id FROM capture_samples WHERE id="<<id;
	ns_sql_result res;
	sql.get_rows(res);
	if(res.size()==0)
		throw ns_ex("Could not find sample id ") << sample_id << " in the db.";
	sample_name = res[0][0];
	device_name = res[0][1];
	position.x = atof(res[0][2].c_str());
	position.y = atof(res[0][3].c_str());
	size.x = atof(res[0][4].c_str());
	size.y = atof(res[0][5].c_str());
	sql << "SELECT name FROM experiments WHERE id = " << res[0][6];
	experiment_name = sql.get_value();
	sql << "SELECT s.scheduled_time, s.time_at_start, s.time_at_finish, s.missed, s.problem,s.time_at_imaging_start,"
		 "s.time_spent_reading_from_device,s.time_spent_writing_to_disk,"
		 "s.total_time_during_read,s.time_during_transfer_to_long_term_storage,"
		 "s.time_during_deletion_from_local_storage, "
		 "s.total_time_spent_during_programmed_delay,"
		 "t.intensity_average,t.intensity_std,t.intensity_entropy, t.intensity_top_percentile,t.intensity_bottom_percentile, "
		 "i.registration_vertical_offset,i.registration_horizontal_offset "
		 "FROM (capture_schedule as s LEFT OUTER JOIN captured_images as i ON i.id = s.captured_image_id) "
		 "LEFT OUTER JOIN image_statistics as t ON  i.image_statistics_id = t.id "
		 "WHERE s.sample_id=" << sample_id << " AND s.scheduled_time < UNIX_TIMESTAMP(NOW()) ORDER BY s.scheduled_time ASC";
	sql.get_rows(res);
	scans.resize(res.size());
	for (unsigned int i = 0; i < scans.size(); i++){
		scans[i].scheduled_time_date = atol(res[i][0].c_str());
		scans[i].start_time_date = atol(res[i][1].c_str());
		scans[i].stop_time_date = atol(res[i][2].c_str());
		scans[i].data_start_time_date = atol(res[i][5].c_str());
		scans[i].scan_position = position;
		scans[i].scan_size = size;

		scans[i].missed = (res[i][3]!="0");
		scans[i].problem = (res[i][4]!="0");
	
		scans[i].time_spent_reading_from_device = ns_atoi64(res[i][6].c_str())/1000.0/1000.0/60;
		scans[i].time_spent_writing_to_disk = ns_atoi64(res[i][7].c_str())/1000.0/1000.0;
		scans[i].total_time_during_read = ns_atoi64(res[i][8].c_str())/1000.0/1000.0/60.0;
		scans[i].total_time_spent_during_programmed_delay = ns_atoi64(res[i][11].c_str())/1000.0/60.0;

		scans[i].time_during_transfer_to_long_term_storage = ns_atoi64(res[i][9].c_str())/1000.0/1000.0;
		scans[i].time_during_deletion_from_local_storage = ns_atoi64(res[i][10].c_str())/1000.0/1000.0;
		scans[i].registration_offset.y = atol(res[i][17].c_str());
		scans[i].registration_offset.x = atol(res[i][18].c_str());
		if (res[i][10] != "NULL"){
			scans[i].image_stats.image_statistics.mean = atof(res[i][12].c_str());
			scans[i].image_stats.image_statistics.variance = atof(res[i][13].c_str());
			scans[i].image_stats.image_statistics.entropy = atof(res[i][14].c_str());
			scans[i].image_stats.image_statistics.top_percentile_average= atof(res[i][15].c_str());
			scans[i].image_stats.image_statistics.bottom_percentile_average= atof(res[i][16].c_str());
		}
		else{		
			scans[i].image_stats.image_statistics.mean = 0;
			scans[i].image_stats.image_statistics.variance = 0;
			scans[i].image_stats.image_statistics.entropy = 0;
			scans[i].image_stats.image_statistics.top_percentile_average = 0;
			scans[i].image_stats.image_statistics.bottom_percentile_average = 0;
		}
	//	if (scans[i].error != 0) 
	//		cerr << "ERROR FOUND";

	}

	if (scans.size() == 0)
		return;
	//normalize times to start of experiment
	date_of_first_sample_scan = scans[0].scheduled_time_date;

	for (unsigned int i = 0; i < scans.size(); i++){
		scans[i].date_of_first_sample_scan = date_of_first_sample_scan;
		if (scans[i].scheduled_time_date != 0)
			scans[i].scheduled_time=scans[i].scheduled_time_date-date_of_first_sample_scan;
		else scans[i].scheduled_time = 0;
		if (scans[i].start_time_date != 0)
			scans[i].start_time=scans[i].start_time_date-date_of_first_sample_scan;
		else scans[i].start_time = 0;
		if (scans[i].stop_time_date != 0)
			scans[i].stop_time=scans[i].stop_time_date-date_of_first_sample_scan;
		else scans[i].stop_time = 0;
		if (scans[i].data_start_time_date != 0)
			scans[i].data_start_time=scans[i].data_start_time_date - date_of_first_sample_scan;
		else scans[i].data_start_time = 0;
	}
		
	calculate_running_statistics();
}
void ns_capture_sample_image_statistics::calculate_running_statistics(){
	if (scans.size() == 0) return;
	long k(60*60*ns_capture_scan_statistics::running_kernel_width_hours);
	for (unsigned int i = 0; i < scans.size(); i++){
		unsigned start_t = scans[i].start_time;
		unsigned long stop_i;
		for (stop_i = i;stop_i < scans.size() && 
			(long)scans[stop_i].start_time - (long)start_t < k; 
			stop_i++);
		unsigned long avg(0),avg_sq(0);
		unsigned long cnt(0);
		for (unsigned int j = i; j < stop_i; j++){
			bool valid_data(scans[j].scanning_duration() > 60);
			if (!valid_data) continue;
			avg+=scans[j].scanning_duration();
			avg_sq+= scans[j].scanning_duration()*scans[j].scanning_duration();
			cnt++; 
		}
		if (cnt == 0){
			scans[i].smoothed_scanning_duration = 0;
			scans[i].scanning_duration_variation = 0;
		}
		else{
			scans[i].smoothed_scanning_duration = avg/cnt;
			scans[i].scanning_duration_variation = (avg_sq-avg*avg)/cnt;
		}
	}

};

void ns_capture_sample_image_statistics::output_jmp_header(std::ostream & o, const std::string & delimeter){
	o << "Experiment Name,Sample Name,Device Name,";
	ns_capture_scan_statistics::out_jmp_header(o,delimeter);
}
void ns_capture_sample_image_statistics::output_jmp_format(std::ostream & o, const std::string & delimeter ){
	if (scans.size() < 1) return;
	for (unsigned int j = 0; j < scans.size(); j++){
		o	<< experiment_name << ","
			<< sample_name << "," 
			<< device_name << ",";
		scans[j].output_jmp_format(o,position,size,delimeter);
	}
}


void ns_capture_sample_region_data_timepoint::set_as_zero(){
	time = 0;
	statistics.set_as_zero();
	sample_statistics.set_as_zero();
	timepoint_is_censored = false;
	timepoint_has_a_problem = false;
}
void ns_capture_sample_region_data_timepoint::operator+=(const ns_capture_sample_region_data_timepoint & r){
	time+=r.time;
	statistics+=r.statistics;
	sample_statistics+=r.sample_statistics;
	timepoint_is_censored=timepoint_is_censored || r.timepoint_is_censored;
	timepoint_has_a_problem=timepoint_has_a_problem || r.timepoint_has_a_problem;
}
void ns_capture_sample_region_data_timepoint::operator/=(const double & r){
	time=(unsigned long)(time/r);
	statistics/=r;
	sample_statistics/=r;
}

void ns_capture_sample_region_data_timepoint::output_jmp_header(const std::string & suffix,std::ostream & o, const std::string & delimeter){
	o << suffix<<" Capture Time (date),"<<suffix<<" Capture Time(days since first measurement),"
		<< suffix<<" Time point Censored,"<<suffix<<" Time Point Has a Problem,"
		<< suffix<<" Width (Inches),"<<suffix<<" Height (Inches),"
		<<suffix<<" Region Image Intensity Average,"<<suffix<<" Region Image Intensity Variance,"<<suffix<<" Region Image Intensity Entropy,"<<suffix<<" Region Image Intensity Bottom Percentile,"<<suffix<<" Region Image Intensity Top Percentile,"
		<< suffix<<" Worm Count,"<<suffix<<" Worm Area Mean,"<<suffix<<" Worm Area Variance,"<<suffix<<" Worm Length Mean,"<<suffix<<" Worm Length Variance,"<<suffix<<" Worm Width Mean,"<<suffix<<" Worm Width Variance,"<<suffix<<" Worm Intensity Mean,"<<suffix<<" Worm Intensity Variance,"
		<< suffix<<" Non-Worm Count,"<<suffix<<" Non-Worm Area Mean,"<<suffix<<" Non-Worm Area Variance,"<<suffix<<" Non-Worm Intensity Mean,"<<suffix<<" Non-Worm Intensity Variance" << delimeter;
}
void ns_capture_sample_region_data_timepoint::output_blank_jmp_data(std::ostream & o, const std::string & delimeter){
	o 		<< "," <<  ","
			<<  "," << ","
			<<  "," << ","
			<<  ","
			<< ","
			<<   ","
			<<  ","
			<< ","
			<<  ","
			<< ","
			<< ","
			<< ","
			<< ","
			<< ","
			<< ","
			<< ","
			<<  ","
			<< ","
			<< ","
			<< ","
			<< ","
			<< delimeter;

}
void ns_capture_sample_region_data_timepoint::output_jmp_data(std::ostream & o, const unsigned long start_time,const bool & censored, const bool & timepoint_has_a_problem, const std::string & delimeter) const{
	o <<	   ns_format_time_string_for_tiff(time) << "," << (time- start_time)/(60.0*60.0*24.0) << ","
			<< (censored?"1":"0") << "," << (timepoint_has_a_problem?"1":"0")<<","
			<< statistics.size.x << "," << statistics.size.y << ","
			<< statistics.image_statistics.mean << ","
			<< statistics.image_statistics.variance << ","
			<< statistics.image_statistics.entropy << ","
			<< statistics.image_statistics.bottom_percentile_average << ","
			<< statistics.image_statistics.top_percentile_average << ","

			<< statistics.worm_statistics.count << ","
			<< statistics.worm_statistics.area_mean << ","
			<< statistics.worm_statistics.area_variance << ","
			<< statistics.worm_statistics.length_mean << ","
			<< statistics.worm_statistics.length_variance << ","
			<< statistics.worm_statistics.width_mean << ","
			<< statistics.worm_statistics.width_variance << ","
			<< statistics.worm_statistics.absolute_intensity.mean << ","
			<< statistics.worm_statistics.absolute_intensity.variance << ","

			<< statistics.non_worm_statistics.count << ","
			<< statistics.non_worm_statistics.area_mean << ","
			<< statistics.non_worm_statistics.area_variance << ","
			<< statistics.non_worm_statistics.absolute_intensity.mean << ","
			<< statistics.non_worm_statistics.absolute_intensity.variance 
			<< delimeter;
}

void ns_capture_sample_region_data::generate_timepoints_sorted_by_time(){
	timepoints_sorted_by_time.clear();
	for (unsigned int i = 0; i < timepoints.size(); i++){
		timepoints_sorted_by_time[timepoints[i].time] = &timepoints[i];
	}
}
	
void ns_capture_sample_region_data::generate_summary_info(ns_capture_sample_region_data_timepoint & mean_timepoint,ns_capture_sample_region_data_timepoint & first_timepoint,ns_capture_sample_region_data_timepoint & last_timepoint){
	mean_timepoint.set_as_zero();
	first_timepoint.set_as_zero();
	last_timepoint.set_as_zero();

	if (timepoints.size() == 0)
		throw ns_ex("ns_capture_sample_region_data::generate_summary_info()::No timepoints present!");

	unsigned long decile_size = timepoints.size()/10;
	if (decile_size == 0)
		decile_size = 1;

	for (unsigned int i = 0; i < decile_size; i++){
		first_timepoint+=timepoints[i];
		mean_timepoint+=timepoints[i];
	}
	for (unsigned int i = decile_size; i < timepoints.size()-decile_size; i++)
		mean_timepoint+=timepoints[i];
	for (unsigned int i = timepoints.size()-decile_size; i < timepoints.size(); i++){
		last_timepoint+=timepoints[i];
		mean_timepoint+=timepoints[i];
	}
	first_timepoint/=(double)decile_size;
	last_timepoint/=(double)decile_size;
	mean_timepoint/=timepoints.size();
}

void ns_capture_sample_region_data::load_from_db(const unsigned long region_id_, 
					const ns_region_metadata & metadata_,
					const bool region_is_censored,
					const bool region_is_excluded,
					ns_sql & sql){
	metadata = metadata_;
	metadata.region_id = region_id_;
	censored = region_is_censored;
	excluded = region_is_excluded;

	sql << "SELECT " << ns_image_statistics::produce_sql_query_stub() 
						<< ", sample_region_images.censored, sample_region_images.problem, sample_region_images.capture_time FROM image_statistics,sample_region_images WHERE sample_region_images.region_info_id=" << metadata.region_id
						<< " AND sample_region_images.image_statistics_id!=0 AND sample_region_images.image_statistics_id = image_statistics.id ORDER BY sample_region_images.capture_time ASC";
	ns_sql_result res;
	sql.get_rows(res);
	timepoints.resize(res.size());
	for (unsigned int i = 0; i < res.size(); i++){
		timepoints[i].statistics.from_sql_result(res[i]);
		timepoints[i].timepoint_is_censored = atol(res[i][ns_image_statistics::sql_query_stub_field_count()].c_str())!=0;
		timepoints[i].timepoint_has_a_problem = atol(res[i][ns_image_statistics::sql_query_stub_field_count()+1].c_str())!=0;
		timepoints[i].time = atol(res[i][ns_image_statistics::sql_query_stub_field_count()+2].c_str());
	}
}
	
void ns_capture_sample_region_data::set_sample_info(const ns_capture_sample_image_statistics & sample_stats){

	unsigned long s(0);
	unsigned long last_rt(0),last_st(0);
	for (unsigned int i = 0; i < timepoints.size(); i++){
		while(true){
			if (timepoints[i].time == sample_stats.scans[s].scheduled_time_date)
				break;
			s++;
			if (s >= sample_stats.scans.size())
				throw ns_ex("Could not find time ") << timepoints[i].time;
		}
		if (timepoints[i].time < last_rt)
			throw ns_ex("Region Timepoints is not sorted!");
		if ((unsigned long)sample_stats.scans[s].start_time < last_st)
			throw ns_ex("Sample Timepoints is not sorted!");
		last_rt = timepoints[i].time;
		last_st = sample_stats.scans[s].start_time;

		timepoints[i].sample_statistics = sample_stats.scans[s];

	}
}
void ns_capture_sample_region_data::output_region_data_in_jmp_format_header(const std::string & suffix,std::ostream & o){
	ns_region_metadata::out_JMP_plate_identity_header(o);
	o << ",Plate Censored,Plate Excluded,";
//	ns_capture_scan_statistics::out_jmp_header(o,",");
	ns_capture_sample_region_data_timepoint::output_jmp_header(suffix,o,"\n");
}

void ns_capture_sample_region_data::output_region_data_in_jmp_format(std::ostream & o, const std::string & delimeter){
	for (unsigned int i = 0; i < timepoints.size(); ++i){
		metadata.out_JMP_plate_identity_data(o);
		o << ",";
		o << (censored?"1":"0");
		o << ",";
		o << (excluded?"1":"0");
		o << ",";
//		timepoints[i].sample_statistics.output_jmp_format(o,timepoints[0].time,ns_vector_2d(0,0),ns_vector_2d(0,0),",");
		timepoints[i].output_jmp_data(o,timepoints[0].time,timepoints[i].timepoint_is_censored,timepoints[i].timepoint_has_a_problem,delimeter);
	}
}


void ns_capture_sample_statistics_set::load_whole_experiment(const unsigned long experiment_id, ns_sql & sql){
	ns_sql_result res;
	sql << "SELECT id FROM capture_samples WHERE censored=0 AND excluded_from_analysis=0 AND experiment_id=" << experiment_id;
	sql.get_rows(res);
	samples.resize(res.size());
	samples_sorted_by_id.clear();
	for (unsigned int j = 0; j < res.size(); j++){
		samples[j].load_from_db(atol(res[j][0].c_str()),sql);
		samples_sorted_by_id[samples[j].sample_id] = &samples[j];
	}
	calculate_scanner_behavior();
}
void ns_capture_sample_statistics_set::output_scanner_activity_plot(std::ostream & o){
	const unsigned long time_interval(60); // one minute intervals
	unsigned long current_time(0);
	for (ns_whole_device_activity_aggregator_list::iterator device_scan_list = device_list.begin(); device_scan_list!=device_list.end(); device_scan_list++){
		std::set<std::string> sample_list;
		if (device_scan_list->second.size() == 0)
			continue;
		for (ns_whole_device_activity_aggregator::iterator q = device_scan_list->second.begin(); q != device_scan_list->second.end(); q++){
			sample_list.insert(q->second.sample->sample_name);
		}
		/*current_time = q-
		for (ns_whole_device_activity_aggregator::iterator q = device_scan_list->second.begin(); q != device_scan_list->second.end(); q++){
			o << device_scan_list->first << ","
		}*/
	}
}

//build a list of all activity on each scanner
void ns_capture_sample_statistics_set::calculate_scanner_behavior(){
	//order all scans on each device by time.
	for (unsigned int i = 0; i < samples.size(); i++){
		ns_whole_device_activity_aggregator_list::iterator p(device_list.find(samples[i].device_name));
		if (p == device_list.end())
			p = device_list.insert(ns_whole_device_activity_aggregator_list::value_type(samples[i].device_name,ns_whole_device_activity_aggregator())).first;
		for (unsigned int j = 0; j < samples[i].scans.size(); j++)
			(p->second)[samples[i].scans[j].scheduled_time_date] = ns_whole_device_activity_timepoint(&samples[i],&samples[i].scans[j]);
	}
	for (ns_whole_device_activity_aggregator_list::iterator device_scan_list = device_list.begin(); device_scan_list!=device_list.end(); device_scan_list++){
			
	//check for pathalogical behavior
		for (ns_whole_device_activity_aggregator::iterator q = device_scan_list->second.begin(); ; q++){
			ns_whole_device_activity_aggregator::iterator r(q);
			r++;
			if (r == device_scan_list->second.end())
				break;
			if (q->second.scan->start_time_date != 0 && 
				q->second.scan->start_time_date >=
				r->second.scan->scheduled_time_date)
				std::cerr << "Yikes!  At " << ns_format_time_string_for_human(q->second.scan->start_time) << ", a scan was started after its successor was scheduled to run. "
				"The first scan should have been canceled, but was not.  Is the server timeout time set to high?\n";
		}
		for (ns_whole_device_activity_aggregator::iterator q = device_scan_list->second.begin(); q != device_scan_list->second.end(); q++){
			q->second.scan->time_spent_off_immediately_before_starting_scan = -1;
			q->second.scan->time_spent_off_after_finishing_scan = -1;
			if (q->second.scan->start_time_date == 0) //don't search for info on scans that never started
				continue;
				
			if (q == device_scan_list->second.begin())
				continue;

			//start one back from current position in list
			ns_whole_device_activity_aggregator::iterator r(q);
			r--;
			while(true){
				if (r->second.scan->stop_time_date != 0){
					q->second.scan->time_spent_off_immediately_before_starting_scan
						= r->second.scan->time_spent_off_after_finishing_scan 
						= q->second.scan->start_time_date - r->second.scan->stop_time_date;
					break;
				}
				if (r == device_scan_list->second.begin())
					break;
				r--;
			}
		}
	}
}

void ns_capture_sample_region_statistics_set::build_id_mapping(){
	regions_sorted_by_id.clear();
	for (unsigned int i = 0; i < regions.size(); i++)
		regions_sorted_by_id[regions[i].metadata.region_id] = &regions[i];
}

void ns_capture_sample_region_statistics_set::set_sample_data(const ns_capture_sample_statistics_set & samples){
	for (unsigned int i = 0; i < regions.size(); i++){
		std::map<unsigned long,ns_capture_sample_image_statistics *>::const_iterator p = samples.samples_sorted_by_id.find(regions[i].metadata.sample_id);
		if (p==samples.samples_sorted_by_id.end())
			throw ns_ex("Could not find sample statistics for sample id ") << regions[i].metadata.sample_id << "(" << regions[i].metadata.sample_name << ")" << "\n";
		regions[i].set_sample_info(*p->second);
	}
}

void ns_capture_sample_region_statistics_set::output_plate_statistics_with_mortality_data_header(std::ostream & o){
	ns_region_metadata::out_JMP_plate_identity_header(o);
	o << ",Plate Censored,Plate Excluded,";
//	ns_survival_data_summary_aggregator::out_JMP_summary_data_header(o,",");
	ns_capture_sample_region_data_timepoint::output_jmp_header("Mean ",o,",");
	ns_capture_sample_region_data_timepoint::output_jmp_header("Initial ",o,",");
	ns_capture_sample_region_data_timepoint::output_jmp_header("Final ",o,"\n");
}
/*
void ns_capture_sample_region_statistics_set::output_plate_statistics_with_mortality_data(const ns_survival_data_summary_aggregator & survival_data, std::ostream & o){
		
	ns_capture_sample_region_data_timepoint mean,first,last;
	for (unsigned int i = 0; i < regions.size(); i++){
			
		regions[i].metadata.out_JMP_plate_identity_data(o);
		o << ",";
		o << regions[i].censored?"1":"0";
		o << ",";
		o << regions[i].excluded?"1":"0";
		o << ",";

		ns_survival_data_summary_aggregator::ns_plate_list::const_iterator region_mortality_data(survival_data.plate_list.find(regions[i].metadata.plate_name()));
		if (region_mortality_data == survival_data.plate_list.end()){
			survival_data.out_JMP_empty_summary_data(o);
		}
		else{
			survival_data.out_JMP_summary_data(region_mortality_data,o);
		}
		o << ",";

		regions[i].generate_summary_info(mean,first,last);
		mean.output_jmp_data(o,mean.time,false,false,",");
		first.output_jmp_data(o,mean.time,false,false,",");
		last.output_jmp_data(o,mean.time,false,false,"\n");
	}

}
	*/
void ns_capture_sample_region_statistics_set::load_whole_experiment(const unsigned long experiment_id,ns_sql & sql){
		
	std::string experiment_name;
	sql << "SELECT name FROM experiments WHERE id = " << experiment_id;
	ns_sql_result res1;
	sql.get_rows(res1);
	if (res1.size() == 0)
		throw ns_ex("Could not find experiment id ") << experiment_id;
	experiment_name = res1[0][0];

	ns_sql_result res;
	sql << "SELECT id,name,device_name,censored,description,excluded_from_analysis,incubator_name, incubator_location "
			    "FROM capture_samples WHERE experiment_id=" << experiment_id;
	sql.get_rows(res);
		
	ns_genotype_fetcher genotypes;
	genotypes.load_from_db(&sql);

	for (unsigned int j = 0; j < res.size(); j++){
		
		ns_region_metadata sample_metadata;
		sample_metadata.sample_id = atol(res[j][0].c_str());
		sample_metadata.experiment_name = experiment_name;
		sample_metadata.sample_name = res[j][1];
		sample_metadata.device = res[j][2];
		sample_metadata.incubator_name = res[j][6];
		sample_metadata.incubator_location = res[j][7];

		bool sample_censored=(res[j][3]!="0"),
				sample_excluded=(res[j][5]!="0");
		std::string sample_details = res[j][4];
			


			
		sql << "SELECT id,censored,excluded_from_analysis FROM sample_region_image_info WHERE sample_id=" << sample_metadata.sample_id;
		ns_sql_result res2;
		sql.get_rows(res2);

		unsigned long plate_index(regions.size());
		regions.resize(plate_index+res2.size());

		for (unsigned long k = 0; k < res2.size(); ++k){
			ns_region_metadata metadata(sample_metadata);
			unsigned long region_id(atol(res2[k][0].c_str()));
			metadata.load_from_db(region_id,"",sql);
			
			bool region_censored(res2[k][1]!="0"),
					region_excluded(res2[k][2]!="0");
//				if (region_censored ||  region_excluded){
//					char a;
//					a++;
//				}
				//std::cerr << "EX";
				
			regions[plate_index+k].load_from_db(region_id,metadata,sample_censored || region_censored,sample_excluded || region_excluded,sql);
			if (sample_details.size() > 0) 
				regions[plate_index+k].metadata.details += sample_details;
		}
	}
	build_id_mapping();
}
