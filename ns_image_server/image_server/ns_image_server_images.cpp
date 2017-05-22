#include "ns_image_server_images.h"	
#include "ns_detected_worm_info.h"
#include "ns_image_statistics.h"

using namespace std;

#ifndef NS_MINIMAL_SERVER_BUILD

void ns_summarize_stats(const std::vector<const ns_detected_worm_info *> & worms,ns_image_object_statistics & stats){
	
	ns_detected_worm_stats w_stats;
	stats.count = worms.size();
	for (unsigned int i = 0; i < worms.size(); i++){
			w_stats = worms[i]->generate_stats();
			stats.area_mean			+=w_stats[ns_stat_pixel_area];
			stats.length_mean			+=w_stats[ns_stat_spine_length];
			stats.width_mean			+=w_stats[ns_stat_average_width];

			stats.absolute_intensity.mean		+=w_stats[ns_stat_absolute_intensity_average];	
			stats.absolute_intensity.variance	+=w_stats[ns_stat_absolute_intensity_variance]*w_stats[ns_stat_absolute_intensity_variance];
			stats.absolute_intensity.bottom_percentile_average	+=w_stats[ns_stat_absolute_intensity_dark_pixel_average];
			stats.absolute_intensity.entropy	+=w_stats[ns_stat_absolute_intensity_roughness_1];
			stats.absolute_intensity.top_percentile_average =0;

			stats.relative_intensity.mean		+=w_stats[ns_stat_relative_intensity_average];	
			stats.relative_intensity.variance	+=w_stats[ns_stat_relative_intensity_variance]*w_stats[ns_stat_relative_intensity_variance];
			stats.relative_intensity.bottom_percentile_average	+=w_stats[ns_stat_relative_intensity_dark_pixel_average];
			stats.relative_intensity.entropy	+=w_stats[ns_stat_relative_intensity_roughness_1];
			stats.relative_intensity.top_percentile_average =0;

			stats.area_variance			+=w_stats[ns_stat_pixel_area]*w_stats[ns_stat_pixel_area];
			stats.length_variance		+=w_stats[ns_stat_spine_length]*w_stats[ns_stat_spine_length];
			stats.width_variance		+=w_stats[ns_stat_average_width]*w_stats[ns_stat_average_width];
		}
	
		if (stats.count > 0){
			stats.area_variance		/=stats.count;
			stats.length_variance		/=stats.count;
			stats.width_variance		/=stats.count;
			stats.absolute_intensity.variance /=stats.count;
			stats.relative_intensity.variance /=stats.count;
		
			stats.area_mean			/=stats.count;
			stats.length_mean			/=stats.count;
			stats.width_mean			/=stats.count;
			stats.absolute_intensity.mean		/=stats.count;
			stats.relative_intensity.mean /=stats.count;

			// E[x^2]-E[x]^2
			stats.area_variance		-= stats.area_mean*stats.area_mean;
			stats.length_variance		-= stats.length_mean*stats.length_mean;
			stats.width_variance		-= stats.width_mean*stats.width_mean;
			stats.absolute_intensity.variance	-= stats.absolute_intensity.mean*stats.absolute_intensity.mean;
			stats.relative_intensity.variance	-= stats.relative_intensity.mean*stats.relative_intensity.mean;

			stats.area_variance		= sqrt(stats.area_variance);
			stats.length_variance		= sqrt(stats.length_variance);
			stats.width_variance		= sqrt(stats.width_variance);
			stats.absolute_intensity.variance = sqrt(stats.absolute_intensity.variance);
			stats.relative_intensity.variance = sqrt(stats.relative_intensity.variance);	
	}
}
void ns_image_server_captured_image_region::summarize_stats(ns_image_worm_detection_results * wi,ns_image_statistics * stats,bool calculate_non_worm){
	ns_summarize_stats(wi->actual_worm_list(),stats->worm_statistics);
	if(calculate_non_worm)
	ns_summarize_stats(wi->non_worm_list(),stats->non_worm_statistics);
}

void ns_image_server_captured_image_region::register_worm_detection(ns_image_worm_detection_results * wi, const bool interpolated,ns_sql & sql,const bool calculate_stats){
		
	ns_image_statistics stats;
	if (calculate_stats){
		//save a summary of the current image to the image_statistics table
		summarize_stats(wi,&stats);

		sql << "SELECT image_statistics_id FROM sample_region_images WHERE id= " << region_images_id;
		ns_sql_result res;
		sql.get_rows(res);
		if (res.size() == 0)
			stats.db_id = 0;
		else stats.db_id = ns_atoi64(res[0][0].c_str());
		stats.submit_to_db(stats.db_id,sql,false,true);
	}
	
	ns_sql_result res;
	//save a detailed summary of each worm to disk

	//if a results object has previously been made, overwrite it.  Otherwise, create a new record.
	sql << "SELECT worm_detection_results_id, worm_interpolation_results_id FROM sample_region_images WHERE id = " << region_images_id;

	sql.get_rows(res);
	if (res.size() != 0){
		if (!interpolated)	wi->id = atol(res[0][0].c_str());
		else				wi->id = atol(res[0][1].c_str());
	}
	else wi->id = 0;
	wi->save_to_disk(*this,interpolated,sql);
	//update the region record to reflect the new results
	sql << "UPDATE sample_region_images SET ";
	if (calculate_stats)
		sql << "image_statistics_id=" << stats.db_id <<",";
	if (!interpolated) sql << "worm_detection_results_id=";
	else			   sql << "worm_interpolation_results_id=";
	sql << wi->id << " WHERE id = " << region_images_id;
	sql.send_query();

	//DEPRECIATED
	if (0){
		//Update worm_movement results list the results have been made invalid by new changes
		sql << "SELECT id FROM worm_movement WHERE region_id_short_1 = " << region_images_id;
		ns_sql_result a;
		sql.get_rows(a);
		for (unsigned int i = 0; i < (unsigned int)a.size(); i++){
			sql << "UPDATE worm_movement SET calculated = 0 WHERE id=" << a[i][0];
			sql.send_query();
		}
		sql << "SELECT id FROM worm_movement WHERE region_id_short_2 = " << region_images_id;
		ns_sql_result b;
		sql.get_rows(b);		
		for (unsigned int i = 0; i < (unsigned int)b.size(); i++){
			sql << "UPDATE worm_movement SET calculated = 0 WHERE id=" << b[i][0];
			sql.send_query();
		}
		sql << "SELECT id FROM worm_movement WHERE region_id_long = " << region_images_id;
		ns_sql_result c;
		sql.get_rows(c);
		for (unsigned int i = 0; i < (unsigned int)c.size(); i++){
			sql << "UPDATE worm_movement SET calculated = 0 WHERE id=" << c[i][0];
			sql.send_query();
		}
	}
	return;
}
#endif
std::string ns_image_server_captured_image::experiment_directory(ns_image_server_sql * sql){
	return experiment_directory(experiment_name,experiment_id);
}
std::string ns_image_server_captured_image::experiment_directory(const std::string & experiment_name, const ns_64_bit experiment_id){

	std::string path;
	add_string_or_number(path,experiment_name,"experiment_",experiment_id);
	return path;
}

std::string ns_image_server_captured_image::captured_image_directory_d(const std::string & sample_name,const ns_64_bit sample_id,const std::string & experiment_directory, const bool small_images, const ns_processing_task & task){	
	
	string path(ns_sample_directory(sample_name,sample_id,experiment_directory));
	path += DIR_CHAR_STR;
	if (small_images)
		path+="captured_small_images";
	else path += "captured_images";
	std::string step = ns_processing_step_directory_d(task);
	if (task != ns_unprocessed && step.size() != 0)
		 return path + DIR_CHAR_STR + step;
	return path;
}

ns_image_server_image ns_image_server_captured_image::make_small_image_storage(ns_image_server_sql * sql){
	ns_image_server_image image;
	image.filename = small_image_filename(sql);
	image.path = small_image_directory(sql);
	image.partition = image_server_const.image_storage.get_partition_for_experiment(experiment_id,sql);
	return image;
}

std::string ns_image_server_captured_image::small_image_directory(ns_image_server_sql * sql){
	if (captured_images_id == 0)
		throw ns_ex("ns_image_server_captured_image::Could not create filename with unspecified sample id.");
	//if not specified, collect information needed to create filename.
	if (sample_id == 0 || experiment_id == 0 ||
		sample_name == "" || device_name == "" || experiment_name == "")
			if (!load_from_db(captured_images_id,sql))
				throw ns_ex("ns_image_server_captured_image: During path creation, experiment and sample information was not specified at runtime or in the db.");
	return captured_image_directory_d(sample_name,sample_id,experiment_directory(sql),true);
}
std::string ns_image_server_captured_image::directory(ns_image_server_sql * sql, const ns_processing_task & task,const bool allow_blank_capture_image_id){
	if (!allow_blank_capture_image_id && captured_images_id == 0)
		throw ns_ex("ns_image_server_captured_image::Could not create filename with unspecified sample id.");
	//if not specified, collect information needed to create filename.
	if (sample_id == 0 || experiment_id == 0 ||
		sample_name == "" || device_name == "" || experiment_name == "")
			if (!load_from_db(captured_images_id,sql))
				throw ns_ex("ns_image_server_captured_image: During path creation, experiment and sample information was not specified at runtime or in the db.");
	return captured_image_directory_d(sample_name,sample_id,experiment_directory(sql),false);
}


bool ns_image_server_captured_image::load_from_db(const ns_64_bit _id, ns_image_server_sql * con){
		captured_images_id				= _id;
		*con << "SELECT ci.sample_id, "
			"ci.experiment_id, "
			"ci.image_id, "
			"cs.name,"
			"exp.name, "
			"ci.capture_time, "
			"cs.device_name, "
			"ci.small_image_id, "
			"ci.never_delete_image"
			<< " FROM "<< con->table_prefix() << "captured_images as ci,  "<< con->table_prefix() <<"capture_samples as cs, "<< con->table_prefix() <<"experiments as exp WHERE ci.id = " << captured_images_id  
			<< " AND ci.experiment_id = exp.id"
			<< " AND cs.id = ci.sample_id";
	//	cerr << con->query() << "\n";
	//	std::string foo(con->query());
		ns_sql_result info;
		con->get_rows(info);
		if (info.size() == 0)
			return false;
		if (info.size() > 1)
			throw ns_ex("Captured image doesn't have a unique id!");
		sample_id						= atol(info[0][0].c_str());
		experiment_id					= atol(info[0][1].c_str());
		capture_images_image_id		 	= atol(info[0][2].c_str());
		sample_name						= info[0][3];
		experiment_name					= info[0][4];
		capture_time					= atol(info[0][5].c_str());
		device_name						= info[0][6].c_str();
		capture_images_small_image_id	= atol(info[0][7].c_str());
		never_delete_image				= (info[0][8] != "0");
		return true;
	}

std::string ns_sample_directory(const std::string & sample_name,const ns_64_bit sample_id,const std::string & experiment_directory){
	std::string path(experiment_directory);
	path += DIR_CHAR;
	add_string_or_number(path,sample_name,"sample_",sample_id);
	return path;
}

std::string ns_format_base_image_filename(const ns_64_bit experiment_id,const std::string & experiment_name,
										  const ns_64_bit sample_id, const std::string & sample_name,
										  const unsigned long capture_time, const ns_64_bit captured_images_id, 
										  const ns_64_bit capture_images_image_id){

	return ns_shorten_filename(experiment_name) + std::string("=" + ns_to_string(experiment_id) + "=" + sample_name + "=" 
			 + ns_to_string(sample_id) + "=" +ns_to_string(capture_time) + "=" + ns_format_time_string(capture_time) + "="
			 + ns_to_string(captured_images_id)  + "=" + ns_to_string(capture_images_image_id));
}
std::string ns_image_server_captured_image::get_filename(ns_image_server_sql * sql,const bool small_image, bool do_not_load_data){
	if (!do_not_load_data && captured_images_id == 0)
		throw ns_ex("ns_image_server_captured_image::Could not create filename with unspecified captured_images_id.");
	//if not specified, collect information needed to create filename.
	if (!do_not_load_data && 
		(sample_id == 0 || experiment_id == 0 || capture_time == 0 || //capture_images_image_id == 0 ||
		sample_name == "" || experiment_name == ""))
			if (!ns_image_server_captured_image::load_from_db(captured_images_id,sql))
				throw ns_ex("ns_image_server_captured_image: During filename creation, experiment and sample information was not specified at runtime or in the db.");
	std::string temp(ns_format_base_image_filename(experiment_id,experiment_name,sample_id,sample_name,capture_time,captured_images_id, capture_images_image_id));
	if (specified_16_bit)
		temp += "=64bit";
	if (small_image)
		temp += "=small";
	return temp;
}

void ns_split_time(const std::string & t, std::vector<unsigned int> & val){
	std::string cur;
	val.resize(0);
	for (unsigned int i = 0; i < t.size(); i++){
		if (t[i] == '-'){
			val.push_back(atoi(cur.c_str()));
			cur.resize(0);
		}
		else cur+=t[i];
	}
	if (cur.size() != 0)
		val.push_back(atoi(cur.c_str()));
}
#include <time.h>
bool ns_image_server_captured_image::from_filename(const std::string & fn, int & offset){
	std::string filename = fn;
	unsigned char state=0;
	std::string experiment_id_str,
		   sample_id_str,
		   captured_images_id_str,
		   captured_images_image_id_str,
		   capture_date_str,
		   capture_time_str;
	unsigned int number_of_fields = 0;
	for (offset = 0; offset < (int)filename.size(); offset++){
		if (filename[offset] == '=')
			number_of_fields++;
	}
	std::string null;
	std::vector<std::string *> strings(9,&null);
	bool old_style = false;
	//very old-style, pre version 1.0 naming system
	if (number_of_fields == 4){
		filename = ns_dir::extract_filename_without_extension(filename);
		strings[0]=&experiment_name;
		strings[1]=&sample_name;
		strings[2]=&capture_date_str;
		strings[3]=&capture_time_str;
		strings[4]=&device_name;
		old_style = true;
	}
	//current naming system
	if (!old_style){
		strings[0]=&experiment_name;
		strings[1]=&experiment_id_str;
		strings[2]=&sample_name;
		strings[3]=&sample_id_str;
		strings[4]=&capture_time_str;
		strings[5]=&null;
		strings[6]=&null;
		strings[7]=&captured_images_id_str;
		strings[8]=&captured_images_image_id_str;
	}

	for (offset = 0; offset < (int)filename.size(); offset++){
		char a = filename[offset];
		
		if (a == '='){
			//one problem that cropped up was experiment names containing
			//equals signs.  We get around this with the following hack:
			//if the field following the experiment name isn't a number
			//we decide that it is just part of the experiment name and continue
			if (state == 1 && !old_style){
				int int_val = atoi(strings[1]->c_str());
				if (int_val == 0){
					experiment_name+="=";
					experiment_name+=experiment_id_str;
					experiment_id_str.resize(0);
				}
				else state++;
			}
			else{
				state++;
				if (state == 9) break;
			}
		}
		else (*strings[state])+=a;
		
	}
	offset++;
	//old naming system
	if (old_style){
		if (state < 4)
			return false;
		//in very early versions, the date was stored only as a std::string.
		//we need to back-convert it into the UNIX time format.
		std::vector<unsigned int > tmp;
		ns_split_time(capture_date_str,tmp);
		if (tmp.size() != 3)
			throw ns_ex("ns_image_server_captured_image::Could not decode old-style date: ") << capture_date_str;
		tm t;
		t.tm_year = tmp[0]-1900;
		t.tm_mon = tmp[1]-1;
		t.tm_mday = tmp[2];
		ns_split_time(capture_time_str,tmp);
		if (tmp.size() < 2)
			throw ns_ex("ns_image_server_captured_image::Could not decode old-style  time: ") << capture_time_str;
		t.tm_hour = tmp[0];
		t.tm_min = tmp[1];
		t.tm_sec = 0;
		time_t timt = mktime(&t);
		capture_time = static_cast<unsigned long>(timt);
		return true;
	}
	else if (state < 8)
		return false;
	experiment_id = atol(experiment_id_str.c_str());
	sample_id = atol(sample_id_str.c_str());
	capture_time = atol(capture_time_str.c_str());
	captured_images_id = atol(captured_images_id_str.c_str());
	capture_images_image_id = atol(captured_images_image_id_str.c_str());
	return true;
}

ns_image_server_image ns_image_server_captured_image::create_storage_for_processed_image(const ns_processing_task & task, const ns_image_type & image_type, ns_image_server_sql * sql, const std::string & filename_suffix){
	//we want to delete the previous image if it already exists.
	//here we piggyback on ns_image_server_image's code to do this.
	//Please forgive the shortcut.
	ns_image_server_image dummy;
	dummy.processed_output_storage = &processed_output_storage;
	return dummy.create_storage_for_processed_image(task,image_type,sql);
}

std::string ns_image_server_captured_image_region::filename(ns_image_server_sql * sql){
	if (region_images_id == 0)
		throw ns_ex("ns_image_server_captured_image_region::Region id not specified when trying to acquire filename.");

	if (experiment_id == 0 || region_name == "" || experiment_name == "" || sample_name == "")// || device_name == "")
		if (!load_from_db(region_images_id, sql))
			throw ns_ex("ns_image_server_captured_image_region::Filename information for image id ") << region_images_id << " not specified and not present in database.";
	return ns_image_server_captured_image::filename(sql) + "=" + region_name + "=" + ns_to_string(region_images_id);
}


bool ns_image_server_captured_image_region::from_filename(const std::string & filename){
	int offset = 0;
	if (!ns_image_server_captured_image::from_filename(filename,offset))
		return false;
	
	unsigned char state=0;
	std::string region_images_id_str;

	for (unsigned int i = offset; i < filename.size(); i++){
		char a = filename[i];
		if (a == '='){
			state++;
			if (state == 2) break;
		}
		else{
			switch(state){
				case 0: region_name+=a; break;
				case 1: region_images_id_str+=a; break;
			}
		}
	}
	if (state < 1)
		return false;
	region_images_id = atol(region_images_id_str.c_str());
	return true;
}
bool ns_image_server_captured_image_region::load_from_db(const ns_64_bit _id, ns_image_server_sql * sql){
	*sql << "SELECT s.capture_sample_image_id, si.name, s.image_id, "
		   "s.region_info_id, s.worm_detection_results_id, s.worm_interpolation_results_id, s.worm_movement_id, "
		   "s.problem,s.currently_under_processing, s.capture_time, "
		   "s.op1_image_id, s.op2_image_id, s.op3_image_id, s.op4_image_id,"
		   "s.op5_image_id, s.op6_image_id, s.op7_image_id, s.op8_image_id,"
		   "s.op9_image_id, s.op10_image_id,s.op11_image_id,s.op12_image_id,"
		   "s.op13_image_id,s.op14_image_id,s.op15_image_id,s.op16_image_id,"
		   "s.op17_image_id,s.op18_image_id,s.op19_image_id,s.op20_image_id,"
		   "s.op21_image_id,s.op22_image_id,s.op23_image_id,s.op24_image_id,"
		   "s.op25_image_id,s.op26_image_id,s.op27_image_id,s.op28_image_id,"
		   "s.op29_image_id,s.op30_image_id "
		   "FROM "<< sql->table_prefix() << "sample_region_images as s, "<< sql->table_prefix() << "sample_region_image_info as si "
		   "WHERE s.region_info_id = si.id AND "
		   "s.id = " << _id << " LIMIT 1";
	ns_sql_result res;
	sql->get_rows(res);
	region_images_id = _id;
	if (res.size() == 0)
		return false;
	captured_images_id = atol(res[0][0].c_str());
	region_name = res[0][1];
	region_images_image_id = atol(res[0][2].c_str());
	region_info_id = atol(res[0][3].c_str());
	region_detection_results_id = atol(res[0][4].c_str());
	region_interpolation_results_id = atol(res[0][5].c_str());
	movement_characterization_id = atol(res[0][6].c_str());
	problem_id = atol(res[0][7].c_str());
	processor_id = atol(res[0][8].c_str());
	capture_time = atol(res[0][9].c_str());
	op_images_.resize(ns_process_last_task_marker);
	op_images_[0] = region_images_image_id;
	for (unsigned int i = 1; i < op_images_.size(); i++)  //op[0] is unprocessed image
		op_images_[i] = atol(res[0][9+i].c_str());

	return ns_image_server_captured_image::load_from_db(captured_images_id, sql);

}

std::string ns_image_server_captured_image_region::region_base_directory(const std::string & region_name,
																	  const std::string &captured_image_directory,
																	  const std::string &experiment_directory){
	return captured_image_directory + DIR_CHAR_STR + region_name;
}
std::string ns_image_server_captured_image_region::region_directory(const std::string & region_name, const std::string & sample_name,
																	  const std::string &captured_image_directory,
																	  const std::string &experiment_directory,
																	  const ns_processing_task & task){
	//heat maps represent one image per entire region (rather than one image per sample captured).
	//thus are stored together in the experiment's base directory
	if (task == ns_process_heat_map || task == ns_process_static_mask)
		return captured_image_directory + DIR_CHAR_STR + ns_processing_step_directory_d(task);
	
	//regions containing single detected objects are stored in subdirectories corresponding to whether they contain
	//worms, dirt (anything that isn't a worm), or an unsorted object
	if (task == ns_process_add_to_training_set){
		std::string path = experiment_directory + DIR_CHAR_STR + ns_processing_step_directory_d(task) + DIR_CHAR_STR + sample_name + DIR_CHAR_STR + region_name;
		return path;
	}
	std::string step =  ns_processing_step_directory_d(task);
	if (step.size() != 0)
		 return region_base_directory(region_name,captured_image_directory,experiment_directory) + DIR_CHAR_STR + step;
	else return region_base_directory(region_name,captured_image_directory,experiment_directory);
}


std::string ns_image_server_captured_image_region::directory(ns_image_server_sql * sql, const ns_processing_task & task){
	if (experiment_name == "" || sample_name == "")
		load_from_db(region_images_id,sql);
	if (region_name == ""){
		//if we lack a region name, try to load it from the database
		if (region_info_id != 0){
			*sql << "SELECT name FROM "<< sql->table_prefix() << "sample_region_image_info WHERE id = " << region_info_id;
			ns_sql_result res;
			sql->get_rows(res);
			if(res.size() == 0)
				throw ns_ex("ns_image_server_captured_image_region::Could not load region_info_id from directory");
			region_name = res[0][0];
		}
		else{
			if (region_images_id == 0)
				throw ns_ex("ns_image_server_captured_image_region::No region_images_id specified when asking for directory");
			load_from_db(region_images_id,sql);
		}
	}
	const std::string experiment_directory(ns_image_server_captured_image::experiment_directory(experiment_name,experiment_id));
	return region_directory(region_name,sample_name,ns_sample_directory(sample_name,sample_id,experiment_directory),experiment_directory,task);
}


const ns_image_server_image ns_image_server_captured_image_region::request_processed_image(const ns_processing_task & task, ns_sql & sql){
	ns_image_server_image im;
	
	//no database information is stored for training set images.
//	if (task == ns_process_add_to_training_set)
	//	throw ns_ex("ns_image_server_captured_image_region::Data on training set images is not stored.");

	std::string db_table = ns_processing_step_db_table_name(task);
	ns_64_bit db_row_id = region_images_id;
	if (db_table ==  "sample_region_image_info")
		db_row_id = region_info_id;

	sql << "SELECT " << ns_processing_step_db_column_name(task) << " FROM " << db_table << " WHERE id = " << db_row_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("Sample region image ") << region_info_id << " could not be found in the database.";
	im.id = atol(res[0][0].c_str());

	if (task != ns_process_static_mask && task != ns_process_heat_map && im.id == 0)
		throw ns_ex("ns_image_server_captured_image_region::Required image processing step, ") << ns_processing_task_to_string(task) << " has not yet been completed.";
	
	if (im.id != 0)
		im.load_from_db(im.id,&sql);

	return im;

}

void ns_image_server_captured_image_region::wait_for_finished_processing_and_take_ownership(ns_sql & sql){
	sql.set_autocommit(false);
	try {
		for (unsigned int i = 0; i < 20; i++) {
			sql.send_query("BEGIN");
			sql << "SELECT currently_under_processing FROM sample_region_images WHERE id = " << region_images_id << " FOR UPDATE";
			ns_sql_result res;
			sql.get_rows(res);
			if (res.size() == 0) {
				sql.send_query("COMMIT");
				sql.set_autocommit(true);
				throw ns_ex("ns_image_server_captured_image_region::Could not wait for finished processing on non-existant image");
			}
			if (res[0][0] == "0") {
				sql << "UPDATE sample_region_images SET currently_under_processing = 1 WHERE id=" << region_images_id;
				sql.send_query();
				sql.send_query("COMMIT");
				sql.set_autocommit(true);
				return;
			}
			ns_thread::sleep(10);
		}
	}
	catch (...) {
		sql.clear_query();
		sql.send_query("ROLLBACK");
		throw;
	}
	sql.send_query("COMMIT");
	sql.set_autocommit(true);
	throw ns_ex("ns_image_server_captured_image_region::Timed out on waiting for image to be finished processing.");
}
void ns_image_server_captured_image_region::create_storage_for_worm_results(ns_image_server_image & im, const bool interpolated,ns_sql & sql){
	im.host_id = image_server_const.host_id();
	im.capture_time = ns_current_time();
	if (experiment_name.size() == 0 || experiment_id == 0 || sample_name.size() == 0)
		load_from_db(region_images_id,&sql);
	const std::string experiment_dir(ns_image_server_captured_image::experiment_directory(experiment_name,experiment_id));
	const std::string region_dir(region_base_directory(region_name,ns_sample_directory(sample_name,sample_id,experiment_dir),experiment_dir));
	im.path = region_dir + DIR_CHAR + "detected_data";
	im.filename = filename(&sql);
	if (interpolated) im.filename += "_i";
	im.filename += ".wrm";
	im.partition = image_server_const.image_storage.get_partition_for_experiment(experiment_id,&sql);

	sql.send_query("BEGIN");

	if (im.id != 0){
		sql << "UPDATE images SET host_id = " << im.host_id  << ", creation_time=" << ns_current_time() << ", currently_under_processing=1, "
			<< "path = '" << sql.escape_string(im.path) << "', filename='" << sql.escape_string(im.filename) << "', `partition`='" << im.partition << "' "
			<< "WHERE id = " << im.id;
		sql.send_query();
	}
	else{
		//create a new image if it doesn't exist.
		sql << "INSERT INTO images SET host_id = " << im.host_id << ", creation_time=" << ns_current_time() << ", currently_under_processing=1, "
			<< "path = '" << sql.escape_string(im.path) << "', filename='" << sql.escape_string(im.filename) << "', `partition`='" << im.partition << "' ";
		im.id = sql.send_query_get_id();
	}
	sql.send_query("COMMIT");
}

void ns_image_server_captured_image_region::delete_processed_image(const ns_processing_task & task, const ns_file_deletion_type type, ns_image_server_sql * sql){
	ns_image_server_image im;
	std::string db_table_name;
	
	ns_64_bit db_table_row_id = region_images_id;
	{
		const std::string base_db_table_name(ns_processing_step_db_table_name(task));
		if (base_db_table_name ==  "sample_region_image_info")
			db_table_row_id = region_info_id;
		db_table_name = sql->table_prefix() + base_db_table_name;
	}
	
	//no database information is stored for training set images.
	if (task == ns_process_add_to_training_set)
		im.id = 0;
	else{
		*sql << "SELECT " << ns_processing_step_db_column_name(task) << " FROM " << db_table_name << " WHERE id = " << db_table_row_id;
		im.id = sql->get_integer_value();
	}
	//delete the old file if it exists
	if (im.id != 0){
		image_server_const.image_storage.delete_from_storage(im,type,sql);
		*sql << "DELETE FROM "<< sql->table_prefix() << "images WHERE id=" << im.id << "\n";
		sql->send_query();
	}
	
	*sql << "UPDATE " << db_table_name << " SET " << ns_processing_step_db_column_name(task) << "=0 WHERE id = " << db_table_row_id;
	sql->send_query();
}


const ns_image_server_image ns_image_server_captured_image_region::create_storage_for_aligned_path_image(const unsigned long frame_index,const unsigned long alignment_type,const ns_image_type & image_type, ns_sql & sql, const std::string & filename_suffix){
	ns_image_server_image im;
	sql << "SELECT id,image_id FROM sample_region_image_aligned_path_images WHERE region_info_id=" << region_info_id << " AND frame_index = " << frame_index;
	ns_sql_result res;
	sql.get_rows(res);
	unsigned long db_id(0);
	if(res.size() == 0)
		im.id = 0;
	else{
		db_id = atol(res[0][0].c_str());
		im.load_from_db(atol(res[0][1].c_str()),&sql);
	}
	
	//delete the old file if it exists
	if (im.id != 0)
		image_server_const.image_storage.delete_from_storage(im,ns_delete_both_volatile_and_long_term,&sql);

	im.host_id = image_server_const.host_id();
	im.capture_time = ns_current_time();
	im.path = directory(&sql,ns_process_movement_posture_aligned_visualization);
	if (experiment_id == 0 || experiment_name.size() == 0 || sample_id == 0 || sample_name.size() == 0 || region_name.size() == 0){
		if (region_info_id == 0){
			if (region_info_id == 0)
				throw ns_ex("ns_image_server_captured_image_region::create_storage_for_aligned_path_image()::No image information provided");
			else load_from_db(region_info_id,&sql);
		}
		else{
			ns_64_bit d;
			ns_region_info_lookup::get_region_info(region_info_id,&sql,region_name,sample_name,d,experiment_name,d);
		}
	}
			
	im.filename = ns_format_base_image_filename(experiment_id,experiment_name,sample_id,sample_name,0,0,0)
					+ "=" + region_name + "=" + ns_to_string(region_info_id)
					+ "=" + ns_to_string(alignment_type) + "=" + filename_suffix + "=" + ns_to_string(frame_index);

	im.partition = image_server_const.image_storage.get_partition_for_experiment(experiment_id,&sql);
	ns_add_image_suffix(im.filename,image_type);

	
	im.save_to_db(im.id,&sql,true);
	if (db_id==0)
		sql << "INSERT INTO ";
	else sql << "UPDATE ";
	sql << "sample_region_image_aligned_path_images SET image_id=" << im.id << ", frame_index=" << frame_index << ", region_info_id=" << region_info_id;

	if (db_id!=0)
		sql << " WHERE id = " << db_id;
	sql.send_query();
	return im;
}

const ns_image_server_image ns_image_server_captured_image_region::create_storage_for_processed_image(const ns_processing_task & task, const ns_image_type & image_type, ns_image_server_sql * sql, const std::string & filename_suffix){
	ns_image_server_image im;
	std::string db_table_name;

	ns_64_bit db_table_row_id = region_images_id;
	{
		const std::string base_db_table_name(ns_processing_step_db_table_name(task));
		if (base_db_table_name ==  "sample_region_image_info")
			db_table_row_id = region_info_id;
		db_table_name = sql->table_prefix() + base_db_table_name;
	}
	
	*sql << "SELECT " << ns_processing_step_db_column_name(task) << " FROM " << db_table_name << " WHERE id = " << db_table_row_id;
	ns_sql_result res;
	sql->get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_server_captured_image_region::create_storage_for_processed_image()::Could not locate ") << db_table_name << " id  " << db_table_row_id;
	im.id = atol(res[0][0].c_str());

	//delete the old file if it exists
//	if (im.id != 0)
//		image_server_const.image_storage.delete_from_storage(im,sql);


	im.host_id = image_server_const.host_id();
	im.capture_time = ns_current_time();
	im.path = directory(sql,task);
	im.filename = filename(sql) + filename_suffix;
	im.partition = image_server_const.image_storage.get_partition_for_experiment(experiment_id,sql);
	ns_add_image_suffix(im.filename,image_type);

	sql->send_query("BEGIN");

	ns_64_bit old_image_id = im.id;
	im.save_to_db(im.id,sql,true);
	if (old_image_id != im.id){
		*sql << "UPDATE " << db_table_name << " SET " << ns_processing_step_db_column_name(task) << " = " << im.id<<  " WHERE id = " << db_table_row_id;
		sql->send_query();
	}
	sql->send_query("COMMIT");
	return im;
}

void ns_image_server_captured_image_region::update_all_processed_image_records(ns_sql & sql){
	sql << "UPDATE sample_region_images SET ";
	for (unsigned long i = 0; i < op_images_.size(); i++){
		if (i == ns_unprocessed)
			continue;
		ns_processing_task task = (ns_processing_task)i;

		if (ns_processing_step_db_table_name(task) !=  "sample_region_images")
			continue;

		sql << ns_processing_step_db_column_name(task) << "=" << op_images_[i];
		if (i != op_images_.size()-1)
			sql << ",";
	}
	sql << " WHERE id = " << region_images_id;
	sql.send_query();
}

ns_image_server_captured_image_region ns_image_server_captured_image_region::get_next_long_time_point(ns_sql & sql) const{
	sql << "SELECT region_id_long FROM worm_movement WHERE id=" << movement_characterization_id;
	ns_sql_result res;
	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("ns_image_server_captured_image_region::Could not locate image's movement characterization record (id=") << movement_characterization_id;
	if (res[0][0] == "0")
		throw ns_ex("ns_image_server_captured_image_region::image's movement characterization record does not have a long time point specified!");
	ns_image_server_captured_image_region region;
	region.load_from_db(atol(res[0][0].c_str()),&sql);
	return region;
}

const ns_image_server_image & ns_image_server_image::create_storage_for_processed_image(const ns_processing_task & task, const ns_image_type & image_type, ns_image_server_sql * sql, const std::string & filename_suffix){
//	throw ns_ex("Nonsensical storage request!");
	processed_output_storage->load_from_db(processed_output_storage->id,sql);
	//delete the file if it already exists
	/*if (processed_output_storage->id != 0)
		image_server_const.image_storage.delete_from_storage(*processed_output_storage,sql);
	else{*/
		processed_output_storage->host_id = image_server_const.host_id();
		processed_output_storage->capture_time = ns_current_time();
		*sql << "UPDATE " << sql->table_prefix() << "images SET host_id = " << processed_output_storage->host_id  << ", creation_time=" << processed_output_storage->capture_time << ", currently_under_processing=1,"
			<< "`partition` = '" << processed_output_storage->partition << "' WHERE id = " << processed_output_storage->id;
		sql->send_query();
		sql->send_query("COMMIT");
	//}
	return *processed_output_storage;
}
void ns_image_server_image::mark_as_finished_processing(ns_image_server_sql * sql){
	*sql << "UPDATE "<< sql->table_prefix() << "images SET currently_under_processing=0 " << "WHERE id = " << id;
	sql->send_query();
	sql->send_query("COMMIT");
}

//returns true if the specified processing task can be used as a pre-computed image for later steps (ie it is saved in a non-lossy format)
/*bool ns_use_as_precomputed_task(const ns_processing_task & t){
	switch(t){
		case ns_unprocessed:
		case ns_process_spatial:
		case ns_process_threshold:  
		case ns_process_worm_detection:
		case ns_process_worm_detection_labels:
		case ns_process_region_vis:
		case ns_process_movement_coloring:
			return true;
		default: return false;
	}
} */

std::string ns_processing_step_directory_d(const ns_processing_task & task){
	switch(task){
		case ns_unprocessed:						return "unprocessed";
		case ns_process_apply_mask:					throw ns_ex("ns_processing_step_directory::There is no standard directory name for the result of mask applications.");
		case ns_process_spatial:					return  "spatial";
		case ns_process_lossy_stretch:				return  "dynamic";
		case ns_process_threshold:					return  "threshold";
		case ns_process_thumbnail:		return  "thumbnail";
		case ns_process_worm_detection:				return  "detected";
		case ns_process_worm_detection_labels:		return  "detected_labels";
		case ns_process_worm_detection_with_graph:	return  "detected_graph";
		case ns_process_region_vis:					return  "region_vis";
		case ns_process_region_interpolation_vis:	return  "region_interpolated_vis";
		case ns_process_accept_vis:					return  "accepted_vis";
		case ns_process_reject_vis:					return  "rejected_vis";
		case ns_process_interpolated_vis:			return  "interp_vis";
		case ns_process_add_to_training_set:		return  "training_set";
		case ns_process_compile_video:				return  "video";
		case ns_process_movement_coloring:			return  "movement";
		case ns_process_movement_mapping:			return  "movement_map";
		case ns_process_posture_vis:				return  "posture_vis";
		case ns_process_movement_coloring_with_graph:return "movement_graph";
		case ns_process_heat_map:					return "heat_map";
		case ns_process_static_mask:				return "static_mask";
		case ns_process_compress_unprocessed:		return "temporal_interpolation";
		case ns_process_movement_coloring_with_survival:return "movement_survival";
			
		case ns_process_movement_paths_visualization: return "movement_path_vis";
		case ns_process_movement_paths_visualition_with_mortality_overlay: return "movement_path_vis_with_mortality";
		case ns_process_movement_posture_visualization:return "movement_posture_vis";
		case ns_process_movement_posture_aligned_visualization: return "movement_posture_aligned_vis";
		case ns_process_unprocessed_backup: return("unprocessed_backup");
		case ns_process_analyze_mask:				throw ns_ex("ns_processing_step_directory::There is no standard directory name for the result of mask analysis");
		
		default:									throw ns_ex("ns_processing_step_directory::Cannot find default directory for task:")  << ns_processing_task_to_string(task);
	}
}


std::string ns_create_operation_summary(const std::vector<char> operations){
	std::string output = "(";
	for (unsigned int i = 0; i < operations.size(); i++){
		if (operations[i]){
			output+= ns_processing_task_to_string((ns_processing_task)i); 
			if (i != operations.size()-1)
				output+=",";
		}
	}
	return output;
}

std::string ns_processing_step_db_table_name(const ns_processing_task & t){
	if (t == ns_process_heat_map || t == ns_process_static_mask)
		return "sample_region_image_info";
	return "sample_region_images";
}

std::string ns_processing_step_db_table_name(const unsigned int & t){
	return ns_processing_step_db_table_name((ns_processing_task)t);
}

std::string ns_processing_step_db_column_name(const ns_processing_task & t){
	if (t == ns_unprocessed)						return "image_id";
//	if (t == ns_process_add_to_training_set)		return "";

	if (t == ns_process_last_task_marker)
		throw ns_ex("ns_processing_step_db_column_name::ns_process_last_task_marker is not a valid processing task.");
	
	return std::string("op") + ns_to_string((unsigned int)t) + "_image_id";
}
std::string ns_processing_step_db_column_name(const unsigned int & t){
	return ns_processing_step_db_column_name((const ns_processing_task)t);
}


std::string ns_processing_task_to_string(const ns_processing_task & t){
	switch(t){
		case ns_unprocessed:							return "Unprocessed Image";
		case ns_process_apply_mask:						return "Mask";
		case ns_process_spatial:						return "Spatial Median";
		case ns_process_lossy_stretch:					return "Dynamic Range (Lossy)";
		case ns_process_threshold:						return "Threshold";
		case ns_process_thumbnail:						return "Resized Small Image";
		case ns_process_worm_detection:					return "Detect Worms";
		case ns_process_worm_detection_labels:			return "Detect Worms (Labels)";
		case ns_process_region_vis:						return "Region Visualization";
		case ns_process_accept_vis:						return "Worm Spine Visualization";
		case ns_process_region_interpolation_vis:		return "Interpolated Region Visualization";
		case ns_process_reject_vis:						return "Reject Spine Visualization";
		case ns_process_worm_detection_with_graph:		return "Detect Worms w. Graph";
		case ns_process_add_to_training_set:			return "Add detected object to training set";
		case ns_process_analyze_mask:					return "Analyze Mask";
		case ns_process_compile_video:					return "Compile Video";
		case ns_process_movement_coloring:				return "Movement Characterization";
		case ns_process_movement_mapping:				return "Movement Inter-Frame Maping";
		case ns_process_posture_vis:					return "Characterize Posture Visualization";
		case ns_process_movement_coloring_with_graph:	return "Movement Characterization with Graph";
		case ns_process_heat_map:						return "Movement Heat Map";
		case ns_process_static_mask:					return "Static Mask";
		case ns_process_compress_unprocessed:			return "Compress Unprocessed Image";
		case ns_process_movement_coloring_with_survival:return "Movement Characterization with Survival";
		case ns_process_movement_paths_visualization:	return "Movement Path Visualization";
		case ns_process_movement_paths_visualition_with_mortality_overlay:	return "Movement Path Visualization with Mortality Overlay";
		case ns_process_movement_posture_visualization:	return "Movement Animal Posture Visualization";
		case ns_process_movement_posture_aligned_visualization: return "Aligned Movement Animal Posture Visualization";
		case ns_process_unprocessed_backup:				return "Unprocessed Image Backup";
		default:										return "Unknown Task" + ns_to_string((int)t);
	}
}
