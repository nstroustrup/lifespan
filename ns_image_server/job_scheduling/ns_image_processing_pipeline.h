#ifndef NS_IMAGE_PROCESSING_PIPELINE
#define NS_IMAGE_PROCESSING_PIPELINE
#include "ns_image.h"
#include "ns_image_server_images.h"
#include "ns_process_mask_regions.h"
#include "ns_worm_detector.h"
#include "ns_spatial_avg.h"
#include "ns_image_tools.h"
#include "ns_image_server.h"
#include "ns_worm_detection_constants.h"
#include "ns_worm_detector.h"
#include "ns_difference_thresholder.h"
#ifndef NS_NO_XVID
#include "ns_xvid.h"
#endif
#include "ns_image_cache.h"
#include "ns_image_registration_cache.h"
#include "ns_processing_job_push_scheduler.h"
#include "ns_worm_training_set_image.h"

void ns_handle_file_delete_request(ns_processing_job & job,ns_sql & sql);
ns_processing_job ns_handle_file_delete_action(ns_processing_job & job,ns_sql & sql);
void ns_handle_image_metadata_delete_action(ns_processing_job & job,ns_sql & sql);
void ns_check_for_file_errors(ns_processing_job & job, ns_sql & sql);

///After an initial round of processing is completed, problems often crop up that necissiate later processing steps be recomputed.
///Early steps precomputed steps do not need to be recomputed for later steps to be calculated; they can be more efficiently loaded from disk.
///ns_precomputed_processing_step_images stores information detailing pre-computed pipeline steps, and simplifies decisions as to whether
///early pipeline steps are loaded or re-computed
struct ns_precomputed_processing_step_images{
	ns_precomputed_processing_step_images():
		loaded(false),
		images((unsigned int)ns_process_last_task_marker),
		exists((unsigned int)ns_process_last_task_marker,false),
		worm_detection_needs_to_be_performed(false){}

	///specifies the database entry id for the precomputed image corresponding to the specified processing step
	bool specify_image_id(const ns_processing_task & i, const unsigned int id,ns_sql & sql);

	///loads all precomputed images based on the ids specified using specifiy_image_id()
	void load_from_db(ns_sql & sql);
	///returns the precomputed image reference object for the specified processing task
	ns_image_server_image & image(const ns_processing_task & i){
		if (!loaded) throw ns_ex("ns_precomputed_pipeline_step_images::Attempting to get image before they have been loaded!");
		return images[(unsigned int)i];
	}
	///returns true if a precomputed image is available and can be used
	bool is_provided(const ns_processing_task & i) const {return images[(unsigned int)i].id != 0;}
	//returns true if an image processing step has already been calculated (but it may not be available
	//for use as a precomputed source because, for example, it is stored in a lossy format
	bool has_been_calculated(const ns_processing_task & i) const{
		return exists[i];
	}
	void remove_preprocessed_image(const ns_processing_task & i){
		exists[i] = 0;
		images[i].id = 0;
	}

	///loads from disk the precomputed image for the specified processing task
	template<class ns_image_t, class ns_component>
	void load_image(const ns_processing_task & i, ns_image_t & image,ns_sql & sql){
		ns_image_storage_source_handle<ns_component> precomp = image_server.image_storage.request_from_storage(images[i],&sql);
		precomp.input_stream().pump(image,512);
	}
	bool worm_detection_needs_to_be_performed;
private:
	std::vector<ns_image_server_image> images;
	std::vector<bool> exists;
	bool loaded;
};
#define ns_image_processing_pipeline_spatial_average_kernal_width 24

///for most image types, we just calculate a simple difference threshold
///this template is specialized for region images, where multiple time points are integrated
///to produce better thresholds

template<class ns_component,class ns_image_server_image_t>
class ns_threshold_manager{
public:
	void run(ns_image_server_image_t & image, const ns_image_whole<ns_component> & in, ns_image_whole<ns_component> & out,ns_sql & sql, const long int _image_chunk_size){
		//low-resolution images use a single-stage thresholding
		if (in.properties().resolution < 1201){
			 ns_difference_thresholder::run(in, out,900,5,0);
		}
		else{
			//high res images use a more sophisticated double-threshold.
			ns_two_stage_difference_thresholder::run(in, out, ns_two_stage_difference_parameters(),false);
		}
	}
};
template<class ns_component>
class ns_threshold_manager<ns_component,ns_image_server_captured_image_region>{
public:
	void run(ns_image_server_captured_image_region & image, const ns_image_whole<ns_component> & in, ns_image_whole<ns_component> & out,ns_sql & sql, const long int _image_chunk_size){

		//for high res photos we use the two-stage threshold technique.
		if (1 || in.properties().resolution > 1201){
			ns_two_stage_difference_thresholder::run(in, out, ns_two_stage_difference_parameters(),false);
			return;
		}
		//for low-res photos we compare the current frame and the next frame to place moving pixels in the foreground.
		else{
			ns_image_server_captured_image_region long_time_point = image.get_next_long_time_point(sql);

			//if the long term
			ns_image_whole<ns_component> long_spatial;

			//don't try to load/calculate the long spatial average if somebody else is already working on it.
			long_time_point.wait_for_finished_processing_and_take_ownership(sql);

			try{
				//if we can load the long time point's spatial average, do so.
				if (long_time_point.op_images_[ns_process_spatial] != 0){
					ns_image_storage_source_handle<ns_component> starting_image(0);
					starting_image = image_server.image_storage.request_from_storage(long_time_point,ns_process_spatial,&sql);
					starting_image.input_stream().pump(long_spatial,_image_chunk_size);
				}
				//otherwise calculate it.
				else{
					ns_spatial_median_calculator<ns_component,true> spatial_averager(_image_chunk_size,ns_image_processing_pipeline_spatial_average_kernal_width);
					ns_image_stream_binding<
							ns_spatial_median_calculator<ns_component,true>,
							ns_image_whole<ns_component> >
							spatial_binding(spatial_averager,long_spatial,_image_chunk_size);
					ns_image_storage_source_handle<ns_component> starting_image(image_server.image_storage.request_from_storage(image,&sql));

					starting_image.input_stream().pump(spatial_binding,_image_chunk_size);
					ns_crop_lower_intensity<ns_component>(long_spatial,(ns_component)ns_worm_detection_constants::get(ns_worm_detection_constant::tiff_compression_intensity_crop_value,long_spatial.properties().resolution));


					//might as well save the long's spatial average so it doesn't have to be re-calculated
					ns_image_server_image output_image = long_time_point.create_storage_for_processed_image(ns_process_spatial,ns_tiff,&sql);
					bool had_to_use_volatile_storage;
					ns_image_storage_reciever_handle<ns_component> r = image_server.image_storage.request_storage(
																output_image,
																ns_tiff, _image_chunk_size,&sql,had_to_use_volatile_storage,false,false);
					long_spatial.pump(r.output_stream(),_image_chunk_size);
				}

				long_time_point.mark_as_finished_processing(&sql);

				if (in.properties().resolution < 1201)
					ns_movement_threshold<ns_threshold_one_stage>(in, long_spatial,out);
				else
					ns_movement_threshold<ns_threshold_two_stage>(in, long_spatial,out);

			}
			catch(...){
				long_time_point.mark_as_finished_processing(&sql);
				throw;
			}
		}
	}
};

struct ns_experiment_video_frame_sample{
	ns_experiment_video_frame_sample():capture_time(0),valid(false){}
	unsigned long capture_time;
	std::string path,
		   filename;
	ns_image_properties prop;
	bool valid;
};

struct ns_experiment_video_frame{
	ns_experiment_video_frame(unsigned int number_of_samples):samples(number_of_samples){}
	std::vector<ns_experiment_video_frame_sample> samples;

	void generate_image(const std::string & filename){

	}
};

class ns_experiment_video_manager{

	void calculate_panel_sizes(){
		if (frames.size() == 0)
			throw ns_ex("No Frames loaded.");
		panel_sizes.resize(frames[0].samples.size(),ns_image_properties(0,0,0,0));
		for (unsigned int i = 0; i < frames.size(); i++){


		}

	}
	std::vector<ns_image_properties> panel_sizes;
	std::vector<ns_experiment_video_frame> frames;
};

struct ns_lifespan_curve_region_timestamp_cache_entry{
	unsigned long latest_movement_rebuild_timestamp;
	unsigned long latest_by_hand_annotation_timestamp;
};
struct ns_lifespan_curve_cache_entry_data{
	unsigned long region_compilation_timestamp;
	ns_death_time_annotation_compiler compiler;
};
class ns_lifespan_curve_cache_entry{
public:
	mutable ns_survival_data_with_censoring cached_plate_risk_timeseries;
	mutable ns_survival_data_with_censoring cached_strain_risk_timeseries;
	mutable std::vector<unsigned long> cached_risk_timeseries_time;
	mutable std::vector<unsigned long> cached_strain_risk_timeseries_time;
	mutable ns_region_metadata cached_risk_timeseries_metadata;
	mutable ns_region_metadata cached_strain_risk_timeseries_metadata;

	const ns_death_time_annotation_compiler & get_region_data(const ns_death_time_annotation_set::ns_annotation_type_to_load & a,const unsigned long id,ns_sql & sql) const;
private:
	typedef std::map<unsigned long,ns_lifespan_curve_cache_entry_data> ns_region_raw_cache;
	void clean() const;
	mutable ns_region_raw_cache region_raw_data_cache;
};

class ns_lifespan_curve_cache{
	typedef std::map<ns_64_bit,ns_lifespan_curve_cache_entry> ns_lifespan_curve_cache_storage;
	typedef std::map<ns_64_bit,ns_lifespan_curve_region_timestamp_cache_entry> ns_lifespan_region_timestamp_cache;

	ns_lifespan_curve_cache_storage data_cache;
	ns_lifespan_region_timestamp_cache timestamp_cache;
	public:
	const ns_lifespan_curve_cache_entry & get_experiment_data(const ns_64_bit id, ns_sql & sql);
};


///ns_image_processing_pipline takes images and performs a series of image-processing steps on them.
///Actual processing steps are implemented elsewhere, so ns_image_processing_pipeline's job is mainly
///to manage database records of tasks performed and coordinate correct ordering and storage of steps.
class ns_image_processing_pipeline{

public:
	typedef ns_8_bit ns_component;

	ns_image_processing_pipeline(const unsigned int image_chunk_size):
		_image_chunk_size(image_chunk_size),
		spatial_averager(image_chunk_size,ns_image_processing_pipeline_spatial_average_kernal_width),
		mask_splitter(image_chunk_size),
		threshold_applier(image_chunk_size),
		mask_analyzer(image_chunk_size){}

	///given the source image, run() runs the processing steps specified in operations.  operations is a bit map with each entry corresponding to the step
	///referred to by its ns_processing_task enum value.  Operations flagged as "1" are performed, operations flagged as "0" are skipped.
	void process_region(const ns_image_server_captured_image_region & region_image, const std::vector<char> operations, ns_sql & sql, const ns_svm_model_specification & model, const ns_lifespan_curve_cache_entry & death_annotations);
	
	//returns the resolution of the sample.
	float process_mask(ns_image_server_image & source_image, const ns_64_bit mask_id, ns_sql & sql);

	///analyze_mask assumes the specified image is a mask containing region information.  Each region is represented by a different
	///color.  analyze_mask calculates the number of regions specified in the mask, calculates their statistics (center of mass, etc)
	///and makes a visualzation of the regions to allow easy verification of mask correctness
	//returns the mask resolution
	float analyze_mask(ns_image_server_image & image, const unsigned int resize_factor, const ns_64_bit mask_id, ns_sql & sql);
	
	
	static void generate_sample_regions_from_mask(ns_64_bit sample_id, const float capture_sample_image_resolution_in_dpi,ns_sql & sql);
	
	///used for debugging; frees all memory stored on the heap.
	void clear_heap();
	void calculate_static_mask_and_heat_map(const std::vector<char> operations, ns_image_server_captured_image_region & region_image, ns_sql & sql);

	//void calculate_temporal_interpolation(const std::vector<char> operations, ns_image_server_captured_image_region & region_image, ns_sql & sql);

	static bool task_contains_timestamp(const ns_processing_task & task) {
		return (task == ns_process_worm_detection ||
				task == ns_process_worm_detection_labels ||
				task == ns_process_worm_detection_with_graph ||
				task == ns_process_movement_coloring ||
				task == ns_process_movement_mapping ||
				task == ns_process_movement_coloring_with_graph ||
				task == ns_process_movement_coloring_with_survival);
	}
	///Creates a time-lapse video of the specified region.  A video is made for each of the specified processing steps
#ifndef NS_NO_XVID
	void compile_video(ns_image_server_captured_image_region & region_image, const std::vector<char> operations,const ns_video_region_specification & region_spec,ns_sql & sql);

	///Creates a time-lapse video of the specified sample.  A video is made for each of the specified processing steps.
	void compile_video(ns_image_server_captured_image & sample_image, const std::vector<char> operations, const ns_video_region_specification & region_spec,ns_sql & sql);
	///Creates a time-lapse video of the specified sample.  A video is made for each of the specified processing steps.
	void compile_video_experiment(ns_image_server_captured_image & sample_image, const std::vector<char> operations, ns_sql & sql);
	static void make_video(const unsigned long experiment_id, const std::vector< std::vector<std::string> > path_and_filenames, const ns_video_region_specification & region_spec, const std::vector<ns_vector_2i> registration_offsets, const std::string &output_basename, ns_sql & sql);
	#endif
	static void wrap_m4v_stream(const std::string & m4v_filename, const std::string & output_basename, const long number_of_frames, const bool for_ppt,ns_sql & sql);
	
	static bool preprocessed_step_required(const ns_processing_task & might_be_needed, const ns_processing_task & s);

	//void characterize_movement(ns_worm_movement_measurement_set & record, const std::vector<char> & operations, ns_sql & sql);

	///Several processing steps require as a precondition that worm detection to be performed.  Returns true if the
	///specified processing step has this requirement, and has not already been calculated
	static bool detection_calculation_required(const ns_processing_task & s);


	///Takes the image and applies the appropriate mask to make a series of region images.  The resulting images are saved to disk
	///and annotated in the database.
	void apply_mask(ns_image_server_captured_image & captured_image,std::vector<ns_image_server_captured_image_region> & output_regions, ns_sql & sql);
	void resize_sample_image(ns_image_server_captured_image & captured_image, ns_sql & sql);
	void resize_region_image(ns_image_server_captured_image_region & region,ns_sql & sql);
	static void register_event(const ns_processing_task & task, const ns_image_properties & properties, const ns_image_server_event & source_event,const bool precomputed,ns_sql & sql);
	static void register_event(const ns_processing_task & task, const ns_image_server_event & source_event,const bool precomputed,ns_sql & sql);

	

	ns_lifespan_curve_cache lifespan_curve_cache;
private:
	///Each time the pipeline is run, the first and last operations of the requested operation set are calculated and stored here.
	ns_processing_task first_task,last_task;

	///Several processing steps require as a precondition that worm detection to be performed.  Returns true if the
	///specified processing step has this requirement.
	static inline bool worm_detection_required_for_specified_operations(const std::vector<char> & operations){
		return (operations[ns_process_worm_detection] ||
					operations[ns_process_worm_detection_labels] ||
					operations[ns_process_region_vis] ||
					operations[ns_process_accept_vis] ||
					operations[ns_process_reject_vis] ||
					operations[ns_process_add_to_training_set]);
	}

	ns_image_stream_mask_splitter<ns_component, ns_image_storage_reciever<ns_component> > mask_splitter;

	ns_spatial_median_calculator<ns_component,true> spatial_averager;

	///temporary internal pipeline storage
	ns_image_whole<ns_component> temporary_image;
	///temporary internal pipeline storage
	ns_image_whole<ns_component> unprocessed;
	///temporary internal pipeline storage
	ns_image_whole<ns_component> spatial_average;
	///temporary internal pipeline storage
	ns_image_whole<ns_component> dynamic_stretch;
	//temporary internal pipeline storage
	ns_image_whole<ns_component> thresholded;

	ns_image_whole<ns_component> mask_image;

	ns_image_stream_apply_threshold<ns_component> threshold_applier;

	ns_image_mask_analyzer<ns_component > mask_analyzer;

	///various pipeline steps involve streaming images from one processing step to the next
	///_image_chunk_size specifies the number of image lines that are sent in a single step.
	unsigned long _image_chunk_size;

	void overlay_graph(const ns_64_bit region_id,ns_image_whole<ns_component> & image, unsigned long start_time, const ns_region_metadata & m, const ns_lifespan_curve_cache_entry & lifespan_data,ns_sql & sql);

	///Confirms that the requetsed operations are self consistant.
	void analyze_operations(const std::vector<char> & operations);

	//Confirmst that the specified operations are possible to calculate, and locates any steps that can be loaded from disk rather than re-computed.
	void analyze_operations(const ns_image_server_captured_image_region & region_image, std::vector<char> & operations, ns_precomputed_processing_step_images & precomputed_images, ns_sql & sql);

	ns_vector_2i get_vertical_registration(const ns_image_server_captured_image & captured_image, ns_image_server_image & source, ns_sql & sql, const ns_disk_buffered_image_registration_profile ** requested_image,bool & delete_profile_after_use);
	//ns_vector_2i get_vertical_registration(const ns_image_server_captured_image & captured_image, const ns_image_whole<ns_component> & image, ns_sql & sql);
	bool check_for_precalculated_registration(const ns_image_server_captured_image & captured_image, ns_vector_2i & registration_offset, ns_sql & sql);
	//ns_vector_2i run_vertical_registration(const ns_image_server_captured_image & captured_image, const ns_image_whole<ns_component> & image, ns_sql & sql);


	ns_image_properties get_small_dimensions(const ns_image_properties & prop);

	static void reason_through_precomputed_dependencies(std::vector<char> & operations,ns_precomputed_processing_step_images & precomputed_images);

};
void ns_rerun_image_registration(const ns_64_bit region_id, ns_sql & sql);

#endif

