#ifndef NS_TIME_SERIES_ANNOTATER_H
#define NS_TIME_SERIES_ANNOTATER_H
#include "ns_image.h"



void ns_update_main_information_bar(const std::string & status);
void ns_update_worm_information_bar(const std::string & status);


extern bool debug_handlers;
struct ns_annotater_image_buffer_entry{
	ns_annotater_image_buffer_entry():loaded(false),im(0){}
	bool loaded;
	ns_image_standard * im;
	void clear(){loaded = false; im = 0;}
};

ns_color_8 ns_annotation_flag_color(ns_death_time_annotation & a);
class ns_annotater_timepoint{
public:
	unsigned long resize_factor;
	virtual ns_image_storage_source_handle<ns_8_bit> get_image(ns_sql & sql)=0;
	virtual void load_image(const unsigned long bottom_border,ns_annotater_image_buffer_entry & im,ns_sql & sql,ns_image_standard & temp_buffer, ns_simple_local_image_cache &image_cache, const unsigned long resize_factor_=1){
		resize_factor = resize_factor_;
		get_image(sql).input_stream().pump(temp_buffer,1024);
		ns_image_properties prop(temp_buffer.properties());
		prop.width/=resize_factor;
		prop.height/=resize_factor;
		prop.height+=bottom_border;
		prop.resolution/=resize_factor;
	//	cerr << resize_factor << "\n";
		//temp_buffer.resample(prop,*im.im);
		im.im->init(prop);
		for (unsigned int y = 0; y < prop.height-bottom_border; y++){
			for (unsigned int x = 0; x < prop.width; x++){
				for (unsigned int c = 0; c < prop.components; c++)
				((*im.im)[y][prop.components*x +c   ]) = (temp_buffer)[resize_factor*y][prop.components*resize_factor*x+c];
			}
		}
		for (unsigned int y = prop.height-bottom_border; y < prop.height; y++){
			for (unsigned int x = 0; x < prop.components*prop.width; x++){
				(*im.im)[y][x] = 0;
		
			}
		}
		im.loaded = true;
	}
};



class ns_image_series_annotater;

typedef void (*ns_handle_error_handler)();
struct ns_annotater_asynch_load_specification{
	ns_acquire_for_scope<ns_sql> sql_;
public:
	ns_annotater_asynch_load_specification():launch_lock("ns_dt_alnh"),fatal_error_handler(0),image(0),swap_1(0),swap_2(0),timepoint(0),sql_(0),swap_lock(0),annotater(0),bottom_border_size(0), external_rescale_factor(1){}
  ~ns_annotater_asynch_load_specification(){sql_.release();}
	
	ns_lock launch_lock;
	ns_lock * swap_lock;
	ns_annotater_image_buffer_entry * image;
	ns_annotater_image_buffer_entry * swap_1,
					  				 * swap_2;
	ns_image_series_annotater * annotater;
	ns_image_standard temp_buffer;
	unsigned long bottom_border_size;
	ns_annotater_timepoint * timepoint;
	double external_rescale_factor;
	ns_sql & sql(){
	  if (sql_.is_null())
	    sql_.attach(image_server.new_sql_connection(__FILE__,__LINE__));
	  return sql_();
	}
	
	ns_handle_error_handler fatal_error_handler;
};

void report_changes_made_to_screen();

class ns_image_series_annotater{
protected:


    virtual inline ns_annotater_timepoint * timepoint(const unsigned long i){throw ns_ex("should be redefined!");}
	virtual inline unsigned long number_of_timepoints(){throw ns_ex("should be redefined!");}
	unsigned long resize_factor;
	ns_lock image_buffer_access_lock;
	ns_annotater_image_buffer_entry current_image;
	std::vector<ns_annotater_image_buffer_entry> previous_images;
	std::vector<ns_annotater_image_buffer_entry> next_images;
	ns_image_series_annotater(const unsigned long resize_factor_, const unsigned long bottom_border_size):local_image_cache(1024*1024*8),image_buffer_access_lock("ns_da_ib"),resize_factor(resize_factor_),image_bottom_border_size(bottom_border_size){}
	
	virtual void draw_metadata(ns_annotater_timepoint * tp,ns_image_standard & im, double external_rescale_factor)=0;
	
	bool refresh_requested_;
	void output_buffer_state(){
		for (unsigned int i = 0; i < previous_images.size(); i++)
			std::cerr << previous_images[previous_images.size()-1-i].im << ( previous_images[previous_images.size()-1-i].loaded?"+":"-") << " ";
		std::cerr << " | " << current_image.im << (current_image.loaded?"+":"-") << " | ";
		for (unsigned int i = 0; i < next_images.size(); i++)
			std::cerr << next_images[i].im << (next_images[i].loaded?"+":"-") << " ";
		std::cerr << "\n";
	}
	
	ns_annotater_asynch_load_specification asynch_load_specification;
	
	static ns_thread_return_type run_asynch_load(void * d){
		ns_annotater_asynch_load_specification & spec(*static_cast<ns_annotater_asynch_load_specification *>(d));
		ns_acquire_lock_for_scope lock1(*spec.swap_lock,__FILE__,__LINE__);
		ns_acquire_lock_for_scope lock2(spec.launch_lock,__FILE__,__LINE__);
	
		try{
		spec.timepoint->load_image(spec.bottom_border_size,*spec.image,spec.sql(),spec.temp_buffer,spec.annotater->local_image_cache,spec.annotater->resize_factor);
		}
		catch(ns_ex & ex){
			std::cerr << "Error: " << ex.text() << "\n";
			if (spec.fatal_error_handler != 0)
				(*spec.fatal_error_handler)();
#ifdef _WIN32
			// return type is integer-like on windows
			return 1; 
#else
			// return type is void*; no simple/clean way to signal error 
			return 0;
#endif
		}
		spec.annotater->draw_metadata(spec.timepoint,*spec.image->im, spec.external_rescale_factor);
		if (spec.swap_1 != 0 && spec.swap_2 != 0){
			ns_swap<ns_annotater_image_buffer_entry > s;
			s(*spec.swap_1,*spec.swap_2);
		}
		spec.annotater->request_refresh(); 
		report_changes_made_to_screen();
		lock1.release();
		lock2.release();
		return 0;
	}	


	void load_image_asynch(ns_annotater_timepoint * source_timepoint,ns_annotater_image_buffer_entry & destination_image, ns_handle_error_handler error_handler,double window_rescale_factor,ns_annotater_image_buffer_entry * swap_2=0,ns_annotater_image_buffer_entry * swap_1=0){

		ns_acquire_lock_for_scope lock(asynch_load_specification.launch_lock,__FILE__,__LINE__);
		asynch_load_specification.image = &destination_image;
		asynch_load_specification.swap_1 = swap_1;
		asynch_load_specification.swap_2 = swap_2;
		asynch_load_specification.swap_lock = &image_buffer_access_lock;
		asynch_load_specification.timepoint = source_timepoint;
		asynch_load_specification.annotater = this;
		asynch_load_specification.bottom_border_size = image_bottom_border_size;
		asynch_load_specification.fatal_error_handler =  error_handler;
		asynch_load_specification.external_rescale_factor = window_rescale_factor;
		ns_thread thread(run_asynch_load,&asynch_load_specification);
		thread.detach();
		lock.release();
	}

	bool fast_forward_requested,
		 fast_back_requested;
	unsigned long current_timepoint_id;

	ns_acquire_for_scope<ns_sql> sql;
public:

	ns_simple_local_image_cache local_image_cache;
	void clear_sql() { if (!sql.is_null())sql.release(); }

	typedef enum {ns_none,ns_forward, ns_back, ns_fast_forward, ns_fast_back,ns_stop,ns_save,ns_rewind_to_zero,ns_number_of_annotater_actions} ns_image_series_annotater_action;

	ns_image_series_annotater_action fast_movement_requested()const{
		if(fast_forward_requested)
			return ns_fast_forward; 
		if (fast_back_requested)
			return ns_fast_back;
		return ns_none;
	}

	~ns_image_series_annotater(){clear_base();sql.release();}
	const unsigned long image_bottom_border_size;

	virtual void clear() {}

	void clear_base(){

		image_buffer_access_lock.wait_to_acquire(__FILE__, __LINE__);
		for (unsigned int i = 0; i < previous_images.size(); i++){
			ns_safe_delete(previous_images[i].im);
			previous_images[i].loaded = false;
		}
		for (unsigned int i = 0; i < next_images.size(); i++){
			ns_safe_delete(next_images[i].im);
			next_images[i].loaded = false;
		}
		previous_images.resize(0);
		next_images.resize(0);
		ns_safe_delete(current_image.im);
		current_image.loaded = false;
		image_buffer_access_lock.release();
	}
	virtual void save_annotations(const ns_death_time_annotation_set & extra_annotations)const=0 ;
	bool step_forward(ns_handle_error_handler error_handler, double external_rescale_factor, bool asynch=false){
		ns_acquire_lock_for_scope lock(image_buffer_access_lock,__FILE__, __LINE__);
	
		if (sql.is_null())
			sql.attach(image_server.new_sql_connection(__FILE__,__LINE__));
//		output_buffer_state();
		if(number_of_timepoints() == 0 || current_timepoint_id == number_of_timepoints()-1){
			stop_fast_movement();
			lock.release();
			return false;
		}
		//grab the last image in previous buffer (we'll reuse the memory for the empty space in the next buffer)
		ns_annotater_image_buffer_entry temp(previous_images[previous_images.size()-1]);
		temp.loaded = false;

		//shift the previous buffer backwards
		for (unsigned int i = 0; i < previous_images.size()-1; i++)
			previous_images[previous_images.size()-1-i] = previous_images[previous_images.size()-2-i];

		previous_images[0] = next_images[0];

		//and shift the next buffer forwards.
		for (int i = 0; i < (int)next_images.size()-1; i++)
			next_images[i] = next_images[i+1];
		next_images[next_images.size()-1] = temp;

		current_timepoint_id++;
			
		//now, everything is set except for old current image is still in current_image
		//and the blank space for the desired new current image is in previous_images[0].
			
	//	output_buffer_state();
		//if the next image is already loaded, we run the swap now and are done!
		if (previous_images[0].loaded){
			if (debug_handlers) std::cerr << "L";
			draw_metadata(timepoint(current_timepoint_id),*previous_images[0].im,external_rescale_factor);
			ns_swap<ns_annotater_image_buffer_entry> s;
			s(previous_images[0],current_image);
		}
		else{
	//		cerr << "Loading " << current_timepoint_id << "\n";
			if (asynch) {
				if (debug_handlers) std::cerr << "A";
				load_image_asynch(timepoint(current_timepoint_id), previous_images[0], error_handler,external_rescale_factor, &current_image, &previous_images[0]);
			}
			else{
				if (debug_handlers) std::cerr << "Q";
				timepoint(current_timepoint_id)->load_image(asynch_load_specification.bottom_border_size,previous_images[0],sql(),asynch_load_specification.temp_buffer,local_image_cache,resize_factor);
				draw_metadata(timepoint(current_timepoint_id),*previous_images[0].im,external_rescale_factor);
				ns_swap<ns_annotater_image_buffer_entry> s;
				s(previous_images[0],current_image);
			}
		}
//		output_buffer_state();
//		cerr << "---\n";
		image_buffer_access_lock.release();
		return true;
	}
	bool step_back(ns_handle_error_handler error_handler, double external_rescale_factor, bool asynch=false){
		ns_acquire_lock_for_scope lock(image_buffer_access_lock, __FILE__, __LINE__);
	
		if (sql.is_null())
			sql.attach(image_server.new_sql_connection(__FILE__,__LINE__));
//		output_buffer_state();
		if(current_timepoint_id == 0){
			stop_fast_movement();
			lock.release();
			return false;
		}
		//grab the last image in next buffer (we'll reuse the memory for the empty space in the previous buffer)
		ns_annotater_image_buffer_entry temp(next_images[next_images.size()-1]);
		temp.loaded = false;

		//shift the previous buffer backwards
		for (unsigned int i = 0; i <next_images.size()-1; i++)
			next_images[next_images.size()-1-i] = next_images[next_images.size()-2-i];

		next_images[0] = previous_images[0];

		//and shift the next buffer forwards.
		for (int i = 0; i < (int)previous_images.size()-1; i++)
			previous_images[i] = previous_images[i+1];
		previous_images[previous_images.size()-1] = temp;

		current_timepoint_id--;
			
		//now, everything is set except for old current image is still in current_image
		//and the blank space for the desired new current image is in next_images[0].
			
	//	output_buffer_state();
		//if the next image is already loaded, we run the swap now and are done!
		if (next_images[0].loaded){
			if (debug_handlers) std::cerr << "L";
			draw_metadata(timepoint(current_timepoint_id),*next_images[0].im,external_rescale_factor);
			ns_swap<ns_annotater_image_buffer_entry> s;
			s(next_images[0],current_image);
		}
		else{
			//cerr << "Loading " << current_timepoint_id << "\n";
			if (asynch) {
				load_image_asynch(timepoint(current_timepoint_id), next_images[0], error_handler,external_rescale_factor, &current_image, &next_images[0]);
				if (debug_handlers) std::cerr << "A";
			}
			else{

				if (debug_handlers) std::cerr << "Q";
				timepoint(current_timepoint_id)->load_image(asynch_load_specification.bottom_border_size,next_images[0],sql(),asynch_load_specification.temp_buffer,local_image_cache,resize_factor);
				draw_metadata(timepoint(current_timepoint_id),*next_images[0].im,external_rescale_factor);
				ns_swap<ns_annotater_image_buffer_entry> s;
				s(next_images[0],current_image);
			}
		}
//		output_buffer_state();
//		cerr << "---\n";
		image_buffer_access_lock.release();
		return true;
	}
	void clear_cached_images(bool lock=true){
	  if(lock)
		image_buffer_access_lock.wait_to_acquire(__FILE__,__LINE__);
		try{
			for (unsigned int i = 0; i < next_images.size(); i++){
				next_images[i].loaded = false;
				next_images[i].im->clear();
			}
			for (unsigned int i = 0; i < previous_images.size(); i++){
				previous_images[i].loaded = false;
				previous_images[i].im->clear();
			}
			if (lock)
			image_buffer_access_lock.release();
		}
		catch(...){
		  if (lock)
			image_buffer_access_lock.release();
		}
	}
	virtual void display_current_frame()=0;
	
	typedef enum {ns_cycle_state, ns_cycle_state_alt_key_held,ns_censor,ns_annotate_extra_worm, ns_censor_all,ns_load_worm_details, ns_cycle_flags,ns_output_images,ns_increase_contrast,ns_decrease_contrast,ns_time_zoom_in,ns_time_zoom_out} ns_click_request;
	virtual void register_click(const ns_vector_2i & image_position,const ns_click_request & action, double external_rescale_factor)=0;

	virtual bool data_saved() const=0;
	
	void request_refresh(){refresh_requested_ = true;}
	bool refresh_requested(){return refresh_requested_;}

	bool fast_forward(){fast_back_requested = false; fast_forward_requested = true; return true;}
	bool fast_back(){fast_forward_requested = false; fast_back_requested = true; return true;}
	void stop_fast_movement(){fast_forward_requested = fast_back_requested = false;}
};

#endif
