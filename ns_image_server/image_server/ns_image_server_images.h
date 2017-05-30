#ifndef NS_IMAGE_SERVER_IMAGES
#define NS_IMAGE_SERVER_IMAGES

#include "ns_sql.h"
#include "ns_image.h"
#include "ns_dir.h"
#include "ns_processing_tasks.h"
#include "ns_image_server_sql.h"

///returns true if the specified processing task can be used as a pre-computed image for later steps (ie it is saved in a non-lossy format)
//bool ns_use_as_precomputed_task(const ns_processing_task & t);
///returns a human-readable std::string describing the specified processing task
std::string ns_processing_task_to_string(const ns_processing_task & t);
///returns a human-readable summary of the processing tasks specified in operations[]
std::string ns_create_operation_summary(const std::vector<char> operations);
///returns the name of the subdirectory in which images produced by the specified processing task are stored
std::string ns_processing_step_directory_d(const ns_processing_task &);
///returns the name of the column of the sample_region_images record in which the operation is stored.
std::string ns_processing_step_db_column_name(const ns_processing_task &);
std::string ns_processing_step_db_column_name(const unsigned int & t);
//returns the table in which the operation record is stored
std::string ns_processing_step_db_table_name(const ns_processing_task & t);
std::string ns_processing_step_db_table_name(const unsigned int & t);

std::string ns_sample_directory(const std::string & sample_name,const ns_64_bit sample_id,const std::string & experiment_directory);


typedef enum{ ns_detected_worm_unsorted, ns_detected_worm_actual_worm, ns_detected_worm_not_a_worm} ns_detected_worm_state;

///contains information about a stored image.
struct ns_image_server_image{
	ns_image_server_image():id(0),host_id(0),capture_time(0),processed_output_storage(0){}
	ns_64_bit id,
		host_id;
	unsigned long capture_time;
	std::string filename,
			path,
			partition;

	//if a processing operation is being applied to the image,
	//the output will be written to this record in the images sql table.
	ns_image_server_image * processed_output_storage;

	bool load_from_db(const ns_64_bit _id, ns_image_server_sql * con){
		*con << "SELECT host_id, creation_time, filename, path, `partition` FROM " << con->table_prefix() << "images WHERE id=" << _id;
		ns_sql_result info;
		con->get_rows(info);
		id = _id;
		if (info.size() == 0)
			return false;
		host_id = ns_atoi64(info[0][0].c_str());
		capture_time = atol(info[0][1].c_str());
		filename = info[0][2];
		path = info[0][3];
		partition = info[0][4];
		return true;
	}

	void save_to_db(const ns_64_bit id_, ns_image_server_sql * sql,const bool currently_under_processing=false){
		if (id_ == 0)
			*sql << "INSERT INTO " << sql->table_prefix() << "images ";
		else *sql << "UPDATE " << sql->table_prefix() << "images ";
		*sql << "SET host_id=" << host_id 
			<< ",creation_time=" << ns_current_time() 
			<< ", currently_under_processing= " << (currently_under_processing?"1":"0") 
			<< ",filename='"<< sql->escape_string(filename) 
			<< "',path='" << sql->escape_string(path)
			<< "',`partition`='" << sql->escape_string(partition) <<"' ";
		if (id_ == 0)
			id = sql->send_query_get_id();
		else{
			id = id_;
			*sql <<" WHERE id = " << id_;
			sql->send_query();
		}
	}
	void mark_as_problem(ns_image_server_sql * sql,ns_64_bit problem_db_id){
		*sql << "UPDATE " << sql->table_prefix() << "images SET problem=" << problem_db_id << " WHERE id=" << id;
		sql->send_query();
		sql->send_query("COMMIT");
	}

	//returns the index in the images table for a new image representing the specified processing step of the current image
	//The location will just be that specified in processed_output_storage.
	//But the function is useful because it deletes the specified image if it already exists
	const ns_image_server_image & create_storage_for_processed_image(const ns_processing_task & task, const ns_image_type & image_type, ns_image_server_sql * sql, const std::string & filename_suffix="");
	void mark_as_finished_processing(ns_image_server_sql * sql);

	void mark_as_added_to_training_set(ns_sql & sql){
		//do nothing
	};
	bool inline store_intermediates(){return false;}

	std::string display_label(){
		return path + DIR_CHAR_STR + filename;
	}


	//sometimes, images are used to house a single object.  In this case, the object can either be a worm, not a worm, or unsorted.
	ns_detected_worm_state detected_worm_state;

	//needed for template completeness but not used
	const ns_image_server_image request_processed_image(const ns_processing_task & task, ns_sql & sql){ return ns_image_server_image();}
};


//contains information about a raw image representing a sample captured from an imaging device
struct ns_image_server_captured_image{
	ns_image_server_captured_image():experiment_id(0),
		captured_images_id(0),
		sample_id(0), 
		capture_images_image_id(0),
		capture_images_small_image_id(0),
		capture_time(0),
		specified_16_bit(false),
		never_delete_image(false){}

	bool never_delete_image;

	ns_64_bit captured_images_id,  //the id of the current row in the captured_images table
		 sample_id,
		 experiment_id,
		 capture_images_image_id,  //the id of the actual photo taken at this time point, indexed in the images table
		 capture_images_small_image_id; //a small version of the entire sample
	unsigned long capture_time;
	std::string sample_name,
		   device_name,
		   experiment_name;
	
	bool specified_16_bit;

	static ns_vector_2i small_image_maximum_dimensions(){return ns_vector_2i(1000,10000);}

	virtual std::string filename(ns_image_server_sql * sql){return get_filename(sql,false);}

	virtual std::string filename_no_load_from_db(ns_image_server_sql * sql){return get_filename(sql,false,true);}

	std::string small_image_filename(ns_image_server_sql * sql){return get_filename(sql,true);}
	typedef enum {ns_mark_as_not_busy,ns_mark_as_busy} ns_busy_specification;
	void save(ns_image_server_sql * sql,const ns_busy_specification busy_specification=ns_mark_as_not_busy){
		if (captured_images_id <= 0){
			*sql << "INSERT INTO " << sql->table_prefix() << "captured_images SET sample_id=" << sample_id << ", capture_time = " << capture_time 
				<< ", experiment_id=" << experiment_id << ", image_id=" << capture_images_image_id
				<< ", small_image_id=" << capture_images_small_image_id << ", never_delete_image=" << (never_delete_image?"1":"0");
			if (busy_specification==ns_mark_as_busy)
				*sql << ", currently_being_processed=1";
			else *sql << ", currently_being_processed=0";
			captured_images_id = sql->send_query_get_id();
		}
		else{
			*sql << "UPDATE " << sql->table_prefix() << "captured_images SET sample_id=" << sample_id << ", capture_time = " << capture_time 
				<< ", experiment_id=" << experiment_id << ", image_id=" << capture_images_image_id
				<< ", small_image_id=" << capture_images_small_image_id << ", never_delete_image=" << (never_delete_image?"1":"0");
			if (busy_specification == ns_mark_as_busy)
				 *sql << ", currently_being_processed=1";
			else *sql << ", currently_being_processed=0";
			*sql << " WHERE id=" << captured_images_id;
			sql->send_query();
		}

		if (capture_images_image_id != 0){
			if (busy_specification == ns_mark_as_busy){
					*sql << "UPDATE " << sql->table_prefix() << "images SET currently_under_processing=1 WHERE id=" << capture_images_image_id;
					sql->send_query();
			}
			else{
				*sql << "UPDATE " << sql->table_prefix() << "images SET currently_under_processing=0 WHERE id=" << capture_images_image_id;
				sql->send_query();
			}
		}
	}

	void update_captured_image_image_info(const std::string & partition, const ns_image_type & image_type, ns_image_server_sql * sql){
		std::string fname = filename(sql);
		ns_add_image_suffix(fname,image_type);
		*sql << "UPDATE " << sql->table_prefix() << "images SET filename = '" << sql->escape_string(fname) << "', path = '" << sql->escape_string(directory(sql)) << "', `partition`='" << sql->escape_string(partition) << "' ";
		*sql << "WHERE id=" << this->capture_images_image_id;
		sql->send_query();	
	}

	void mark_as_problem(ns_image_server_sql * sql, ns_64_bit problem_db_id){
		*sql << "UPDATE " << sql->table_prefix() << "captured_images SET problem=" << problem_db_id << " WHERE id=" << captured_images_id;
		sql->send_query();
		if (capture_images_image_id != 0){
			*sql << "UPDATE " << sql->table_prefix() << "images SET problem=" << problem_db_id << " WHERE id=" << capture_images_image_id;
			sql->send_query();
		}
		sql->send_query("COMMIT");
	}

	virtual bool load_from_db(const ns_64_bit _id, ns_image_server_sql * con);
	
	virtual std::string directory(ns_image_server_sql * sql, const ns_processing_task & task = ns_unprocessed,const bool allow_blank_capture_image_id=false);  //returns the storage path for the image, given its information.
	std::string small_image_directory(ns_image_server_sql * sql);
	std::string experiment_directory(ns_image_server_sql * sql); //returns the directory of the experiment
	static std::string experiment_directory(const std::string & experiment_name, const ns_64_bit experiment_id);
	static std::string captured_image_directory_d(const std::string & sample_name,const ns_64_bit sample_id,const std::string & experiment_directory, const bool small_images, const ns_processing_task & task=ns_unprocessed);

	ns_image_server_image make_small_image_storage(ns_image_server_sql * sql);

	//if a processing operation is being applied to the image,
	//the output will be written to this record in the images sql table.
	ns_image_server_image processed_output_storage;
	void mark_as_finished_processing(ns_image_server_sql * sql){
		
	}

	//returns the index in the images table for a new image representing the specified processing step of the current image
	//The location will just be that specified in processed_output_storage.
	//But the function is necissary because it deletes the specified image if it already exists.  Also
	//the image is marked as "currently being processed" so other processing pipelines won't grab it.
	ns_image_server_image create_storage_for_processed_image(const ns_processing_task & task, const ns_image_type & image_type, ns_image_server_sql * sql, const std::string & filename_suffix="");
	bool inline store_intermediates(){return false;}

	std::string display_label(){
		return experiment_name + "::" + sample_name + "::" +ns_format_time_string(capture_time);
	}

	bool from_filename(const std::string & filename, int & offset);
	
	//needed for template completeness but not used
	const ns_image_server_image request_processed_image(const ns_processing_task & task, ns_sql & sql){ return ns_image_server_image();}

	private:
		std::string get_filename(ns_image_server_sql * sql,const bool small_image, bool do_not_load_data=false);
};


class ns_image_worm_detection_results;
class ns_image_statistics;

//contains information about a single region (extracted by masking) of image captured from an imaging device
struct ns_image_server_captured_image_region : public ns_image_server_captured_image{
	ns_image_server_captured_image_region():region_images_id(0),
		region_images_image_id(0),
		op_images_(ns_process_last_task_marker,0),
		region_info_id(0),
		region_detection_results_id(0),
		region_interpolation_results_id(0),
		movement_characterization_id(0),
		problem_id(0),
		processor_id(0){}
	ns_64_bit region_images_id,
			 region_images_image_id,
			 region_info_id,
			 region_detection_results_id,
			 region_interpolation_results_id,
			 movement_characterization_id;

	ns_64_bit problem_id,processor_id;
	//used during the creation of a region via masking a sample.  Not stored in the database.
	ns_8_bit mask_color;


	std::string region_name;
	std::vector<ns_64_bit> op_images_;  //the id of transformed images, indexed by their (ns_processing_task) type

	std::string filename(ns_image_server_sql * sql);
	bool load_from_db(const ns_64_bit _id, ns_image_server_sql * sql);
	
	std::string directory(ns_image_server_sql * sql, const ns_processing_task & task = ns_unprocessed);

	static std::string region_directory(const std::string & region_name,const std::string & sample_name,
														  const std::string &captured_image_directory,
														  const std::string &experiment_directory,
														  const ns_processing_task & task);
	static std::string region_base_directory(const std::string & region_name,
														  const std::string &captured_image_directory,
														  const std::string &experiment_directory);

	static ns_vector_2i small_image_maximum_dimensions(){return ns_vector_2i(300,300);}

	void mark_as_problem(ns_image_server_sql * sql, ns_64_bit problem_db_id=1){
		*sql << "UPDATE " << sql->table_prefix() << "sample_region_images SET problem="<<problem_db_id<<" WHERE id=" << region_images_id;
		sql->send_query();
		*sql << "UPDATE " << sql->table_prefix() << "images SET problem="<<problem_db_id<<" WHERE id=" << region_images_image_id;
		sql->send_query();
		sql->send_query("COMMIT");
	}

	//returns the region image representing the next time point (either an hour or 55 minutes later than the current image
	ns_image_server_captured_image_region get_next_long_time_point(ns_sql & sql) const;

	//returns the index in the images table for a new image representing the specified processing step of the current image
	const ns_image_server_image create_storage_for_processed_image(const ns_processing_task & task, const ns_image_type & image_type, ns_image_server_sql * sql, const std::string & filename_suffix="");
	
	//create storage for a frame of an animation showing worm movement
	const ns_image_server_image create_storage_for_aligned_path_image(const unsigned long frame_index,const unsigned long alignment_type,const ns_image_type & image_type, ns_sql & sql, const std::string & filename_suffix);

	void delete_processed_image(const ns_processing_task & task, const ns_file_deletion_type type,ns_image_server_sql * sql);

	//generates a record and path for worm results; results are stored in the worm_results object 
	void create_storage_for_worm_results(ns_image_server_image & worm_results, const bool interpolated, ns_sql & sql);
	
	bool inline store_intermediates(){return true;}	
	
	void update_all_processed_image_records(ns_sql & sql);
	//returns the index in the images table for existing processed image
	const ns_image_server_image request_processed_image(const ns_processing_task & task, ns_sql & sql);


	bool from_filename(const std::string & filename);

	void mark_as_finished_processing(ns_image_server_sql * sql){
		*sql << "UPDATE " << sql->table_prefix() << "sample_region_images SET currently_under_processing = 0 WHERE id=" << region_images_id;
		sql->send_query();
	}

	void mark_as_under_processing(const ns_64_bit host_id,ns_image_server_sql * sql){
		*sql << "UPDATE " << sql->table_prefix() << "sample_region_images SET currently_under_processing = " << host_id << " WHERE id=" << region_images_id;
		sql->send_query();
	}

	void wait_for_finished_processing_and_take_ownership(ns_sql & sql);

	/*void mark_as_added_to_training_set(ns_sql & sql){
		throw ns_ex("mark_as_added_to_training_set()::Function is obsolete!");
		//sql << "UPDATE sample_region_images SET " << ns_processing_step_db_column_name(ns_process_add_to_training_set) << "=1 WHERE id = " << region_images_id;
		//sql.send_query();
	}*/

	std::string display_label(){
		std::string ret;
		ret += sample_name;
		ret += "::";
		ret += region_name;
		ret += "  ";
		ret += ns_format_time_string_for_human(capture_time);
		return ret;
	}

	//sometimes, regions are used to house a single object.  In this case, the object can either be a worm, not a worm, or unsorted.
	ns_detected_worm_state detected_worm_state;

	void save(ns_image_server_sql * sql){
		if (region_images_id <= 0){
			*sql << "INSERT INTO " << sql->table_prefix() << "sample_region_images SET region_info_id=" << region_info_id << ", capture_time = " << capture_time << ", capture_sample_image_id = " << captured_images_id << 
				 ",vertical_image_registration_applied=1,image_id=" << region_images_image_id << ", last_modified = " << ns_current_time();
		
			region_images_id = sql->send_query_get_id();
		}
		else{
			*sql << "UPDATE " << sql->table_prefix() << "sample_region_images SET region_info_id=" << region_info_id << ", capture_time = " << capture_time << ", capture_sample_image_id = " << captured_images_id << 
				 ",vertical_image_registration_applied=1,image_id=" << region_images_image_id << ", last_modified = " << ns_current_time();
	
			*sql << " WHERE id=" << region_images_id;
			sql->send_query();
		}
	}
};


#endif
