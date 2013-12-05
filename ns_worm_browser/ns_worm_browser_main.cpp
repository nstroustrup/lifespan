#include <sys/timeb.h>
#include "ns_worm_browser.h"
#include "ns_time_path_image_analyzer.h"
#ifndef WIN32
#include <sys/time.h>
#endif
#include <Intrin.h>
#include "resource.h"
#include "ns_high_precision_timer.h"
#include "ns_experiment_storyboard.h"
#include "ns_fl_modal_dialogs.h"


bool output_debug_messages = false;
bool output_debug_file_opened = false;
std::ofstream debug_output;

void ns_worm_browser_output_debug(const unsigned long line_number,const std::string & source, const std::string & message){
	if (!output_debug_messages)
		return;
	if (!output_debug_file_opened){
		debug_output.open("verbose_debug_output.txt");
		output_debug_file_opened = true;
		if (debug_output.fail()){
			cerr << "Could not open verbose debug output file";
			return;
		}
	}
	cerr << source << "::" << line_number << ": " << message << "\n";
	debug_output << source << "::" << line_number << ": " << message << "\n";
	debug_output.flush();
}

ns_lock menu_bar_processing_lock("ns_menu_bar_lock");
ns_worm_learner worm_learner;

void ns_set_menu_bar_activity(bool a);
void refresh_main_window();


double GetTime(void){
#ifdef _WIN32 
	struct _timeb t;
	_ftime(&t);
	return t.time+t.millitm/1000.0;
#else
	struct timeval t;
	gettimeofday(&t,NULL);
	return t.tv_sec+(double)t.tv_usec/1000000;
#endif
}

ns_lock dndlock;

bool show_worm_window = false;
bool hide_worm_window = false;

ns_thread_return_type ns_handle_drag_and_drop_asynch(void * request_text){
	ns_acquire_for_scope<string> v(static_cast<string *>(request_text));
	try{
	//	dndlock.wait_to_acquire(__FILE__,__LINE__);//)
		//	return;
		string cur;
		for (unsigned int i = 0; i < v().size(); i++){
			if (v()[i]==0 || v()[i]=='\n'){
				worm_learner.handle_file_request(cur);
				cur.resize(0);
			}
			if (v()[i]==0) break;
			cur+=v()[i];
		}
		if (cur.size() != 0)
				worm_learner.handle_file_request(cur);
	//	dndlock.release();
	}
	catch(ns_ex & ex){
		cerr << "Error: " << ex.text() << "\n";
	}
	ns_set_menu_bar_activity(true);
	return 0;
}

void ns_handle_drag_and_drop(){
	ns_set_menu_bar_activity(false);
	ns_thread handle_file_thread(ns_handle_drag_and_drop_asynch,new string(Fl::event_text()));
	handle_file_thread.detach();
}


void idle_main_window_update_callback(void *);
void idle_worm_window_update_callback(void *);

//structure of class lifted from example at 
// http://seriss.com/people/erco/fltk/#OpenGlSimpleWidgets


class ns_worm_terminal_main_window;
class ns_worm_terminal_worm_window;

ns_worm_terminal_main_window * current_window;
ns_worm_terminal_worm_window * worm_window;

// OPENGL WINDOW CLASS
class ns_worm_terminal_gl_window : public Fl_Gl_Window {
	bool mouse_is_down;
	ns_vector_2i mouse_click_location;
	bool have_focus;

    void fix_viewport(unsigned long x, unsigned long y, int width,int height) {
        glLoadIdentity();
     
    	glShadeModel (GL_FLAT);
		glMatrixMode (GL_PROJECTION);    /* prepare for and then */ 
	    glLoadIdentity ();               /* define the projection */
	    glFrustum (-1.0, 1.0, -1.0, 1.0, /* transformation */
	                  5, 20.0); 
	    glMatrixMode (GL_MODELVIEW);  /* back to modelview matrix */
	    glViewport (0,0, width,height);      /* define the viewport */

	  //glViewport(0, 0, width, height); // set viewport
	  //ProjectionMatrix();              // set projection matrix
		//remember the current window values
		//set the current window values as ideal.  The algorithm may disagree later.
	//	cerr << "GL: Setting ideal width and height to " << width << "x" << height << "\n";
//		worm_learner.main_window.ideal_image_size.y= height;
	//	worm_learner.main_window.ideal_image_size.x = width;
    }
    // DRAW METHOD
    void draw() {
        if (!valid()) { 
			valid(1); 
			fix_viewport(x(),y(),w(), h()); 
			Fl::add_idle(idle_main_window_update_callback); 
			glClearColor(1, 1, 1, 1); 
			 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear buffer
			 //glLoadIdentity ();             /* clear the matrix */
			 glTranslatef (0.0, 0.0, -5); /* viewing transformation */
			 glScalef (1.0, 1.0, 1.0);      /* modeling transformation */
			 //glColor3f(0,0,0);
		}      

		  try{
				worm_learner.update_main_window_display(worm_learner.main_window.specified_gl_image_size.x, worm_learner.main_window.specified_gl_image_size.y);	 
		  }
		  catch(std::exception & exception){
			ns_ex ex(exception);
				cerr << ex.text() << "\n";
				exit(1);
			}
    }    
	int handle(int state){
		
		switch(state){
			case FL_FOCUS:
				have_focus = true;
				return Fl_Gl_Window::handle(state);
			case FL_UNFOCUS:
				have_focus = false;
				return Fl_Gl_Window::handle(state);

			case FL_DND_ENTER:
            case FL_DND_RELEASE:
            case FL_DND_LEAVE:
            case FL_DND_DRAG:
                return 1;
			case FL_PASTE: 
				ns_handle_drag_and_drop();
				 return 1;
		}

		try{
			int button(Fl::event_button());
			if(button == FL_LEFT_MOUSE || button == FL_RIGHT_MOUSE){
			
			
			ns_button_press press;
			press.right_button = false;
			if (button == FL_RIGHT_MOUSE)
				press.right_button = true;

			press.shift_key_held = Fl::event_key(FL_Shift_L) || Fl::event_key(FL_Shift_R);
			press.control_key_held = Fl::event_key(FL_Control_L) || Fl::event_key(FL_Control_R);
			press.screen_position = ns_vector_2i(Fl::event_x(),Fl::event_y());

				switch(state){
			
					case FL_PUSH:{
						press.click_type = ns_button_press::ns_down;
						mouse_click_location = press.screen_position;
						mouse_is_down = true;
						worm_learner.touch_main_window_pixel(press);		
						
						return 1;
					}
					case FL_RELEASE:{
						press.click_type = ns_button_press::ns_up;
						mouse_is_down = false;
						worm_learner.touch_main_window_pixel(press);
						
						return 1;
					}
					case FL_DRAG:{
						press.click_type = ns_button_press::ns_drag;
						if (mouse_is_down && (abs(mouse_click_location.x-press.screen_position.x) > 4 || abs(mouse_click_location.y - press.screen_position.y) > 4))
								worm_learner.touch_main_window_pixel(press);
						return 1;
					} 

				}
			}
		}
		catch(std::exception & exception){
			ns_ex ex(exception);
			cerr << ex.text() << "\n";
			return 1;
		}
		//cerr << "Could not handle " << state << "\n";
		return Fl_Gl_Window::handle(state);
	}
	
public:
	// HANDLE WINDOW RESIZING
    void resize(int X,int Y,int W,int H) {
        Fl_Gl_Window::resize(X,Y,W,H);
		if (W != w() || H != h()){
	//		cerr << W << "x" << H << " from " << w() << "x" << h() << "\n";
       		fix_viewport(X,Y,W,H);
		}
        redraw();
    }

    // OPENGL WINDOW CONSTRUCTOR
    ns_worm_terminal_gl_window(int X,int Y,int W,int H,const char*L=0) : Fl_Gl_Window(X,Y,W,H,L),mouse_is_down(false),mouse_click_location(0,0),have_focus(false) {
        end();
    }
};

void ns_start_death_time_annotation(ns_worm_learner::ns_behavior_mode m, const ns_experiment_storyboard_spec::ns_storyboard_flavor & f){
	if (worm_learner.start_death_time_annotation(m,f))
		ns_set_main_window_annotation_controls_activity(true);
}


// OPENGL WINDOW CLASS
class ns_worm_gl_window : public Fl_Gl_Window {
	bool mouse_is_down;
	ns_vector_2i mouse_click_location;
	bool have_focus;

    void fix_viewport(int width,int height) {
        glLoadIdentity();
     
    	glShadeModel (GL_FLAT);
		glMatrixMode (GL_PROJECTION);    /* prepare for and then */ 
	    glLoadIdentity ();               /* define the projection */
	    glFrustum (-1.0, 1.0, -1.0, 1.0, /* transformation */
	                  5, 20.0); 
	    glMatrixMode (GL_MODELVIEW);  /* back to modelview matrix */
	    glViewport (0, 0, width,height);      /* define the viewport */

	  //glViewport(0, 0, width, height); // set viewport
	  //ProjectionMatrix();              // set projection matrix
		//remember the current window values
		//set the current window values as ideal.  The algorithm may disagree later.
	//	cerr << "GL: Setting ideal width and height to " << width << "x" << height << "\n";
		//worm_learner.worm_window.ideal_window_size.y = height ;
		//worm_learner.worm_window.ideal_window_size.x = width;
		//worm_learner.worm_window.ideal_window_size = worm_learner.worm_window.ideal_image_size-worm_image_window_size_difference();
    }
    // DRAW METHOD
    void draw() {
        if (!valid()) { 
			valid(1); 
			fix_viewport(w(), h()); 
			Fl::add_idle(idle_worm_window_update_callback); 
			glClearColor(1, 1, 1, 1); 
			 glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // clear buffer
			 glLoadIdentity ();             /* clear the matrix */
			 glTranslatef (0.0, 0.0, -5.0); /* viewing transformation */
			 glScalef (1.0, 1.0, 1.0);      /* modeling transformation */
			 glColor3f(0,0,0);
		}      

		  try{
				worm_learner.update_worm_window_display(w(), h());	 
		  }
		  catch(std::exception & exception){
			ns_ex ex(exception);
				cerr << ex.text() << "\n";
				exit(1);
			}
    }    
	int handle(int state){
		
		switch(state){
			case FL_FOCUS:
				have_focus = true;
				return Fl_Gl_Window::handle(state);
			case FL_UNFOCUS:
				have_focus = false;
				return Fl_Gl_Window::handle(state);

	/*		case FL_DND_ENTER:
            case FL_DND_RELEASE:
            case FL_DND_LEAVE:
            case FL_DND_DRAG:
                return 1;
			case FL_PASTE: 
				ns_handle_drag_and_drop();
				 return 1;*/
		}

		try{
			int button(Fl::event_button());
			if(button == FL_LEFT_MOUSE || button == FL_RIGHT_MOUSE){
			
			
			ns_button_press press;
			press.right_button = false;
			if (button == FL_RIGHT_MOUSE)
				press.right_button = true;

			press.shift_key_held = Fl::event_key(FL_Shift_L) || Fl::event_key(FL_Shift_R);
			press.control_key_held = Fl::event_key(FL_Control_L) || Fl::event_key(FL_Control_R);
			press.screen_position = ns_vector_2i(Fl::event_x(),Fl::event_y());

				switch(state){
			
					case FL_PUSH:{
						press.click_type = ns_button_press::ns_down;
						mouse_click_location = press.screen_position;
						mouse_is_down = true;
						worm_learner.touch_worm_window_pixel(press);		
						
						return 1;
					}
					case FL_RELEASE:{
						press.click_type = ns_button_press::ns_up;
						mouse_is_down = false;
						worm_learner.touch_worm_window_pixel(press);
						
						return 1;
					}
					case FL_DRAG:{
						press.click_type = ns_button_press::ns_drag;
						if (mouse_is_down && (abs(mouse_click_location.x-press.screen_position.x) > 4 || abs(mouse_click_location.y - press.screen_position.y) > 4))
								worm_learner.touch_worm_window_pixel(press);
						return 1;
					} 

				}
			}
		}
		catch(std::exception & exception){
			ns_ex ex(exception);
			cerr << ex.text() << "\n";
			return 1;
		}
		//cerr << "Could not handle " << state << "\n";
		return Fl_Gl_Window::handle(state);
	}
	
public:
	// HANDLE WINDOW RESIZING
    void resize(int X,int Y,int W,int H) {
        Fl_Gl_Window::resize(X,Y,W,H);
		if (W != w() || H != h()){
			cerr << W << "x" << H << " from " << w() << "x" << h() << "\n";
       		fix_viewport(W,H);
		}
        redraw();
    }

    // OPENGL WINDOW CONSTRUCTOR
    ns_worm_gl_window (int X,int Y,int W,int H,const char*L=0) : Fl_Gl_Window(X,Y,W,H,L),mouse_is_down(false),mouse_click_location(0,0),have_focus(false) {
       //xxxx
		end();
    }
};

struct ns_menu_item_options{
	ns_menu_item_options(const std::string & t, bool inactive_):text(t),inactive(inactive_){}
	ns_menu_item_options(const std::string & t):text(t),inactive(0){}
	ns_menu_item_options():inactive(0){}
	std::string text;
	bool inactive;
};
typedef void (*ns_menu_action)(const std::string & data);
struct ns_menu_item_spec{
	ns_menu_item_spec(){}
	ns_menu_item_spec(const ns_menu_action a,const std::string  t,const int s=0,const int f=0):
		action(a),title(t),shortcut(s),flags(f){}

	ns_menu_action action;
	const std::string title;
	std::vector<ns_menu_item_options> options;
	int shortcut;
	int flags;
};

Fl_Menu_Bar * get_menu_bar();
class ns_worm_terminal_main_menu_organizer;
ns_worm_terminal_main_menu_organizer * get_menu_handler();

void all_menu_callback(Fl_Widget*w, void*data);

class ns_menu_organizer;

struct ns_asynch_menu_request{
	ns_asynch_menu_request(ns_menu_organizer * o, const std::string & r):organizer(o),request(r){}
	ns_menu_organizer * organizer;
	std::string request;

};

struct ns_menu_organizer_callback_data{
	Fl_Menu_Bar * bar;
	ns_menu_organizer * organizer;
};

class ns_menu_organizer{
protected:
	void add(const ns_menu_item_spec & item){
			menu_ordering.push_back(&(menu_actions.insert(std::pair<string,ns_menu_item_spec>(item.title,item)).first->second));
		}
	void clear(){
		menu_ordering.resize(0);
		menu_actions.clear();
	}
	
	typedef std::map<string,ns_menu_item_spec> ns_menu_actions;
	ns_menu_actions menu_actions;
	std::vector<ns_menu_item_spec *> menu_ordering;	

	static ns_thread_return_type handle_dispatch_request_asynch(void * request){

		ns_asynch_menu_request * r(static_cast<ns_asynch_menu_request *>(request));
		try{
			if(!r->organizer->asynch_lock.try_to_acquire(__FILE__,__LINE__)){
				cerr << "Too many simultaneous requests\n";
				delete r;
				return 0;
			}
			ns_set_menu_bar_activity(false);
			r->organizer->dispatch_request(r->request);
			r->organizer->asynch_lock.release();
			ns_set_menu_bar_activity(true);
			delete r;
			 
		}
		catch(ns_ex & ex){
			delete r;
			cerr << "Error in asynchronous thread: " << ex.text();
			return 1;
		}
		return 0;
	}

	ns_lock asynch_lock;
	ns_menu_organizer_callback_data callback_data;
public:
	ns_menu_organizer():asynch_lock("menu_organizer_asynch_lock"){}

	void build_menus(Fl_Menu_Bar & bar){
		callback_data.organizer = this;
		callback_data.bar = &bar;

		for (unsigned int i = 0; i < menu_ordering.size(); i++){
			if (menu_ordering[i]->options.size() == 0)
				bar.add(menu_ordering[i]->title.c_str(),menu_ordering[i]->shortcut,all_menu_callback,&callback_data,menu_ordering[i]->flags);
			for (unsigned int j = 0; j < menu_ordering[i]->options.size(); j++){
				bar.add((menu_ordering[i]->title + "/" + menu_ordering[i]->options[j].text).c_str(),0,all_menu_callback,&callback_data,menu_ordering[i]->options[j].inactive?FL_MENU_INACTIVE:0);
			}
		}
	}

	static std::string remove_menu_formatting(const string & s){
		string ret;
		for (unsigned int i = 0; i < s.size(); ++i){
			if (s[i]!='_' || i > 0 && (s[i-1] != '/')) ret+=s[i];
		}
		return ret;
	}
	void dispatch_request_asynch(const std::string & request){
	//		cerr << "To Many things at once.\n";
	//		return;
	//	}
		ns_asynch_menu_request * req(new ns_asynch_menu_request(this,request));
		ns_thread thread;
		thread.run(handle_dispatch_request_asynch,req);
	}

	void dispatch_request(const std::string & request) const{
		

		try{
			for (ns_menu_actions::const_iterator p = menu_actions.begin(); p != menu_actions.end(); ++p){
				if (remove_menu_formatting(p->second.title) == request){
					cerr << "Action: " << request << "\n";
					p->second.action("");
					return;
				}
			}
			//look to see if an item has been chosen from a menu with multiple options
			for (ns_menu_actions::const_iterator p = menu_actions.begin(); p != menu_actions.end(); ++p){
				const string suffix(remove_menu_formatting(p->second.title));
				string::size_type l(request.find(suffix));
				if (l != string::npos){
					string::size_type d(request.find_last_of("/"));
					cerr << "Action: " << request << "\n";
					p->second.action(request.substr(d+1,string::npos));
					return;
				}
			}
			throw ns_ex("Could not handle menu request:: ") << request;
		}
		catch(ns_ex & ex){
			cerr << "Error: " << ex.text();
			MessageBox(
				0,
				ex.text().c_str(),
				"Worm Browser",
				MB_TASKMODAL | MB_ICONEXCLAMATION| MB_DEFBUTTON1 | MB_TOPMOST);
			}
	} 

};


HANDLE ns_main_thread_id(0);


std::string ns_get_input_string(const std::string title,const std::string default_value){
	ns_text_dialog td;
	td.grid_text.push_back("WHA");
	td.grid_text.push_back("BAH");
	td.title = "Warning";
	ns_run_in_main_thread_custom_wait<ns_text_dialog> n(&td);

	
	ns_choice_dialog c;
	c.title = "What do you want to do with this experiment schedule?";
	c.option_1 = "Generate a Summary File";
	c.option_2 = "Run it!";
	c.option_3 = "Cancel";
	ns_run_in_main_thread<ns_choice_dialog> b(&c);

	ns_file_chooser d;
	d.dialog_type = Fl_Native_File_Chooser::BROWSE_DIRECTORY;
	d.title = "Choose the desired experiment's backup directory";
	d.default_filename = "";
	ns_run_in_main_thread<ns_file_chooser> g(&d);
	
	ns_input_dialog is;
	is.title = "GAR";
	is.default_value = "ROR";
	ns_run_in_main_thread<ns_input_dialog> a(&is);
	return is.result;
}

class ns_worm_terminal_main_menu_organizer : public ns_menu_organizer{

	/*static void run_animation_trial(const std::string & value){
		ns_dir dir;
		string d("Y:\\image_server_storage\\partition_000\\2010_01_13_daf16_uno\\rise_c\\1\\movement_posture_vis");
		dir.load(d);
		sort(dir.files.begin(),dir.files.end());
		ns_image_standard im;
		for (unsigned int i = 0; i < dir.files.size(); i++){
			string filename(d + DIR_CHAR_STR + dir.files[i]);
			cerr << "Loading " << filename << "\n";
			worm_learner.load_file(filename);
			worm_learner.draw();
		}
	}*/

	/*****************************
	Worm Detection Tasks
	*****************************/
	static void calculate_erosion_gradient(const std::string & value){worm_learner.calculate_erosion_gradient();}
	static void show_objects(const std::string & value){worm_learner.detect_and_output_objects();}
	static void detect_worms(const std::string & value){worm_learner.process_contiguous_regions();}
	static void show_region_edges(const std::string & value){worm_learner.show_edges();}
	static void view_region_collage(const std::string & value){worm_learner.make_object_collage();}
	static void view_spine_collage(const std::string & value){worm_learner.make_spine_collage(worm_learner.output_svg_spines);}
	static void view_spine_collage_stats(const std::string & value){worm_learner.make_spine_collage_with_stats(worm_learner.output_svg_spines);}
	static void view_reject_spine_collage(const std::string & value){worm_learner.make_reject_spine_collage(worm_learner.output_svg_spines);}
	static void view_reject_spine_collage_stats(const std::string & value){worm_learner.make_reject_spine_collage_with_stats(worm_learner.output_svg_spines,worm_learner.worm_visualization_directory());}
	static void output_feature_distributions(const std::string & value){worm_learner.output_distributions_of_detected_objects();}
	static void calculate_slow_movement(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		foo.push_back(dialog_file_type("JPEG2000 (*.jp2)","jp2"));
		std::string filename = open_file_dialog("Load Image",foo);
		if (filename != ""){
			worm_learner.characterize_movement(filename);
		}
	}
	/*****************************
	Machine Learning Tasks
	*****************************/

	static void start_death_aligned_posture_annotation(const std::string & value){
		ns_start_death_time_annotation(ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture,ns_experiment_storyboard_spec::ns_number_of_flavors);
	}		
	static void start_time_aligned_posture_annotation(const std::string & value){
		ns_start_death_time_annotation(ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture,ns_experiment_storyboard_spec::ns_number_of_flavors);
	}	
	static void start_storyboard_annotation(const string & flavor_str, const std::string & subject){
		ns_experiment_storyboard_spec::ns_storyboard_flavor flavor;
		if (flavor_str.find("All") != flavor_str.npos)
			flavor = ns_experiment_storyboard_spec::ns_inspect_for_multiworm_clumps;
		else if (flavor_str.find("Immediately") != flavor_str.npos)
			flavor = ns_experiment_storyboard_spec::ns_inspect_for_non_worms;
		else throw ns_ex("Unkown Storyboard flavor");
		if (subject.find("Single Plate") != subject.npos)
			ns_start_death_time_annotation(ns_worm_learner::ns_annotate_storyboard_region,flavor);
		else if (subject.find("Experiment") != subject.npos)
			ns_start_death_time_annotation(ns_worm_learner::ns_annotate_storyboard_experiment,flavor);
		else throw ns_ex("Unknown Storyboard Subject type:") << subject;
	}
	static void start_storyboard_annotation_whole_experiment(const std::string & value){
		start_storyboard_annotation(value,"Experiment");
	}
	static void start_storyboard_annotation_plate(const std::string & value){
		start_storyboard_annotation(value,"Single Plate");
	}

	static void start_death_time_region_annotation(const std::string & value){
		cerr << "Nothing here yet.\n";
	}
	static void stop_posture_annotation(const std::string & value){
		worm_learner.stop_death_time_annotation();
		refresh_main_window();
	}

	static void compare_machine_and_by_hand_annotations(const std::string & value){
		worm_learner.compare_machine_and_by_hand_annotations();
	}
	static void generate_survival_curve_from_hand_annotations(const std::string & value){
		worm_learner.generate_survival_curve_from_hand_annotations();
	}
	static void generate_detailed_animal_data_file(const std::string & value){
		worm_learner.generate_detailed_animal_data_file();
	}

	static void output_learning_set(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.*)","*"));
		std::string filename = open_file_dialog("Load Image",foo);
		if (filename != "") worm_learner.output_learning_set(filename);		
	}
	static void auto_output_learning_set(const std::string & value){worm_learner.output_learning_set("c:\\worm_detection\\training_set\\a",true);}
	static void rethreshold_image_set(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.*)","*"));
		std::string filename = open_file_dialog("Load File",foo);
		if (filename != "") worm_learner.training_file_generator.re_threshold_training_set(filename,worm_learner.get_svm_model_specification());
	}
	static void generate_training_set(const std::string & value){worm_learner.generate_training_set_image();}
	static void process_training_set(const std::string & value){worm_learner.process_training_set_image();}
	static void generate_SVM_training_data(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.*)","*"));
		std::string * filename = new std::string(open_file_dialog("Load File",foo));
		worm_learner.train_from_data(*filename);
	}
	static void split_results(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.tif)","*"));
		std::string filename = open_file_dialog("Load Directory",foo);
		worm_learner.training_file_generator.split_training_set_into_different_regions(filename);
	}
	static void fix_headers_for_svm_training_set_images(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.*)","*"));
		std::string  problem_directory = open_file_dialog("Directory to Fix",foo);
		if (problem_directory == "")
			return;
		std::string  reference_directory = open_file_dialog("Reference Directory",foo);
		if (reference_directory == "")
			return;
		problem_directory = ns_dir::extract_path(problem_directory);
		reference_directory = ns_dir::extract_path(reference_directory);
		string output_directory = problem_directory + DIR_CHAR_STR + "fixed_images";
		ns_dir::create_directory_recursive(output_directory);
		ns_training_file_generator gen;
		gen.repair_xml_metadata(problem_directory,reference_directory,output_directory);

	}
	static void analyze_svm_results(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.*)","*"));
		std::string * filename = new std::string(open_file_dialog("Load Results File",foo));
		ns_thread t;
		if (*filename != "") worm_learner.training_file_generator.plot_errors_on_freq(*filename);
	}
//	static void run_temporal_inference(const std::string & value){worm_learner.run_temporal_inference();}
	static void remove_duplicates_from_training_set(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.tif)","*"));
		std::string filename = open_file_dialog("Load Directory",foo);
		worm_learner.training_file_generator.mark_duplicates_in_training_set(filename);
	}
	static void generate_region_subset_time_series(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.txt)","*"));
		std::string filename = open_file_dialog("Load Directory",foo);
		worm_learner.output_subregion_as_test_set(filename);
	}
	static void load_region_as_new_experiment(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.txt)","*"));
		std::string filename = open_file_dialog("Load Directory",foo);
		worm_learner.input_subregion_as_new_experiment(filename);
	}
	static void create_decimated_subset(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.tif)","*"));
		std::string filename = open_file_dialog("Load Directory",foo);
		worm_learner.decimate_folder(filename);
	}
	static void translate_fscore(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("(*.tif)","*"));
		std::string filename = open_file_dialog("Load Directory",foo);
		worm_learner.translate_f_score_file(filename);
	}
	/*****************************
	Copy and Paste
	*****************************/
	static void clipboard_copy(const std::string & value){

		worm_learner.copy_to_clipboard();
	}
	static void clipboard_paste(const std::string & value){worm_learner.paste_from_clipboard();}
	/*****************************
	Mask Management
	*****************************/
	static void masks_generate_composite(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		std::string filename = save_file_dialog("Save Image",foo,"*.tif",worm_learner.data_selector.current_experiment_name() + "_mask.tif");
		if (filename != "")
			worm_learner.produce_experiment_mask_file(filename);
	}
	static void masks_process_composite(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		std::string filename = open_file_dialog("Save Image",foo);
		if (filename != "") worm_learner.decode_experiment_mask_file(filename);
	}
	static void masks_submit_composite(const std::string & value){worm_learner.submit_experiment_mask_file_to_cluster();}

	static void open_individual_mask(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		std::string filename = open_file_dialog("Load Mask",foo);
		if (filename != "") worm_learner.load_mask(filename);
	}
	static void view_current_mask(const std::string & value){worm_learner.view_current_mask();}

	static void apply_mask_on_current(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		std::string filename = open_file_dialog("Load Image",foo);
		if (filename != "") worm_learner.apply_mask_on_current_image();
	}

	static void submit_individual_mask_to_server(const std::string & value){
		string ip_address;
		unsigned long port;
		worm_learner.get_ip_and_port_for_mask_upload(ip_address,port);
		worm_learner.send_mask_to_server(ip_address,port);
	
	}
	/*****************************
	Samle Region Selection
	*****************************/
	static void save_current_areas(const std::string & value){
		worm_learner.save_current_area_selections();
	}
	static void clear_current_areas(const std::string & value){worm_learner.clear_areas();}
	/*****************************
	Movement Detection Tasks
	*****************************/

	static std::string movement_type_menu_label(const ns_movement_data_source_type::type t){
		switch(t){
			case ns_movement_data_source_type::ns_time_path_image_analysis_data:	return "Time Path Image Analysis";
			case ns_movement_data_source_type::ns_time_path_analysis_data:			return "Time Path Spline Analysis";
			case ns_movement_data_source_type::ns_triplet_data:						return "Old-Style Triplet Analysis";
			case ns_movement_data_source_type::ns_triplet_interpolated_data:		return "Old-Style Triplet Interpolated Analysis";
		}
		return "";
	}
	static void generate_area_movement(const std::string & value){
		ns_movement_data_source_type::type t;
		//if (value == movement_type_menu_label(ns_movement_data_source_type::ns_time_path_image_analysis_data))
			t = ns_movement_data_source_type::ns_time_path_image_analysis_data;
		/*else if (value == movement_type_menu_label(ns_movement_data_source_type::ns_time_path_analysis_data))
			t = ns_movement_data_source_type::ns_time_path_analysis_data;
		else if (value == movement_type_menu_label(ns_movement_data_source_type::ns_triplet_data))
			t = ns_movement_data_source_type::ns_triplet_data;
		else if (value == movement_type_menu_label(ns_movement_data_source_type::ns_triplet_interpolated_data))
			t = ns_movement_data_source_type::ns_triplet_interpolated_data;
		else throw ns_ex("Unknown movement option: ") << value;*/
		
			worm_learner.compile_experiment_survival_and_movement_data(true,ns_movement_area_plot,t);
		
	}
	static void generate_survival_curves(const std::string & value){
		worm_learner.compile_experiment_survival_and_movement_data(true,ns_survival_curve,ns_movement_data_source_type::ns_all_data);
	}	
	static void generate_survival_curves_for_experiment_group(const std::string & value){
		ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
		ns_experiment_region_selector_experiment_info info;
		worm_learner.data_selector.get_experiment_info(worm_learner.data_selector.current_experiment_id(),info);
		for (unsigned int i = 0; i < worm_learner.data_selector.experiment_groups.size(); i++){
			if (worm_learner.data_selector.experiment_groups.size() == 0)
				continue;
			else if (worm_learner.data_selector.experiment_groups[i][0].experiment_group_id != info.experiment_group_id)
				continue;
			for (unsigned int j = 0; j < worm_learner.data_selector.experiment_groups[i].size(); j++){
				try{
					worm_learner.data_selector.set_current_experiment(worm_learner.data_selector.experiment_groups[i][j].experiment_id,sql());
					cerr << "Loading information for experiment " << worm_learner.data_selector.current_experiment_name() << " (" << (j+1) << "/" << worm_learner.data_selector.experiment_groups[i].size() << ")\n";
					worm_learner.compile_experiment_survival_and_movement_data(true,ns_survival_curve,ns_movement_data_source_type::ns_all_data);
				}
				catch(ns_ex & ex){
					cerr << "Error: " << ex.text() << "\n";
				}
			}
			return;
		}
		throw ns_ex("Could not find experiment group in cache");
	}
	
	static void generate_location_plot(const std::string & value){worm_learner.compile_experiment_survival_and_movement_data(true,ns_movement_3d_path_plot,ns_movement_data_source_type::ns_time_path_analysis_data);}
	static void generate_posture_plot(const std::string & value){worm_learner.compile_experiment_survival_and_movement_data(true,ns_movement_3d_movement_plot,ns_movement_data_source_type::ns_time_path_analysis_data);}
	static void generate_timing_data(const std::string & value){
		worm_learner.output_device_timing_data(worm_learner.data_selector.current_experiment_id(),0);
	}
	static void simulate_multiple_worm_clusters(const std::string & value){
		if (value == slow_moving_name())
		worm_learner.simulate_multiple_worm_clumps(true,true);
		else
		worm_learner.simulate_multiple_worm_clumps(false,false);
	}
	static void generate_timing_data_all_exp(const std::string & value){
		ns_experiment_region_selector_experiment_info info;
		worm_learner.data_selector.get_experiment_info(worm_learner.data_selector.current_experiment_id(),info);
		worm_learner.output_device_timing_data(0,info.experiment_group_id);
	}
	static void generate_region_stats(const std::string & value){
		worm_learner.output_region_statistics(worm_learner.data_selector.current_experiment_id(),0);
	}
	static void export_experiment_data(const std::string & value){
		worm_learner.export_experiment_data(worm_learner.data_selector.current_experiment_id());
	}
	static void import_experiment_data(const std::string & value){
		ns_file_chooser d;
		d.dialog_type = Fl_Native_File_Chooser::BROWSE_DIRECTORY;
		d.title = "Locate the experiment's backup directory";
		d.default_filename = "";
		ns_run_in_main_thread<ns_file_chooser> g(&d);
		if (!d.chosen)
			return;
		ns_input_dialog is;
		is.title = "Enter in the new name of the new database to hold the experiment";
		is.default_value = "import_db";
		ns_run_in_main_thread<ns_input_dialog> a(&is);
		if (is.result.empty())
			return;
		if (!worm_learner.import_experiment_data(is.result,d.result,false)){
			string message("The database ");
			message += is.result;
			message += " already exists on the cluster.  Using it could currupt existing data, and probably won't work.  Are you sure you want to do this?";
			int ret = MessageBox(
			0,message.c_str(),"Database exists",
			MB_TASKMODAL | MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST);
			if (ret == IDCANCEL)
				return;
			if (ret == IDYES){
				worm_learner.import_experiment_data(is.result,d.result,true);
			}
		}
	}
	static void generate_region_stats_for_all_regions_in_group(const std::string & value){
		ns_experiment_region_selector_experiment_info info;
		worm_learner.data_selector.get_experiment_info(worm_learner.data_selector.current_experiment_id(),info);
		worm_learner.output_region_statistics(0,info.experiment_group_id);
	}
	static void generate_experiment_detailed_w_by_hand_movement_image_quantification_analysis_data(const std::string & value){worm_learner.generate_experiment_movement_image_quantification_analysis_data(ns_worm_learner::ns_quantification_detailed_with_by_hand);	}
	static void generate_experiment_abbreviated_movement_image_quantification_analysis_data(const std::string & value){worm_learner.generate_experiment_movement_image_quantification_analysis_data(ns_worm_learner::ns_quantification_abbreviated_detailed);	}
	
	static void generate_single_frame_posture_image_pixel_data(const std::string & value){
		worm_learner.generate_single_frame_posture_image_pixel_data((value.find("Plate") != std::string::npos));
	}
	static void generate_worm_markov_posture_model_from_by_hand_annotations(const std::string & value){worm_learner.generate_experiment_movement_image_quantification_analysis_data(ns_worm_learner::ns_build_worm_markov_posture_model_from_by_hand_annotations);	}
	
	static void generate_experiment_detailed_movement_image_quantification_analysis_data(const std::string & value){worm_learner.generate_experiment_movement_image_quantification_analysis_data(ns_worm_learner::ns_quantification_detailed);	}
	
	static void generate_experiment_summary_movement_image_quantification_analysis_data(const std::string & value){worm_learner.generate_experiment_movement_image_quantification_analysis_data(ns_worm_learner::ns_quantification_summary);	}
	
	static void generate_movement_image_analysis_optimization_data(const std::string & value){
	
		if (value.find("Quiecent") != value.npos)
			worm_learner.output_movement_analysis_optimization_data(ns_worm_learner::ns_quiecent);
		else if (value.find("Lifespan") != value.npos)
			worm_learner.output_movement_analysis_optimization_data(ns_worm_learner::ns_lifespan);
		else 
			worm_learner.output_movement_analysis_optimization_data(ns_worm_learner::ns_thermotolerance);
		
	}

	static void generate_training_set_from_by_hand_annotations(const std::string & value){worm_learner.generate_training_set_from_by_hand_annotation();}
	
	
	/*****************************
	Configuration Tasks
	*****************************/
	static void do_not_overwrite_schedules(const std::string & value){worm_learner.overwrite_submitted_specification(false);}
	static void overwrite_schedules(const std::string & value){worm_learner.overwrite_submitted_specification(true);}
	static void do_not_overwrite_existing_masks(const std::string & value){worm_learner.overwrite_existing_masks(false);}
	static void overwrite_existing_masks(const std::string & value){worm_learner.overwrite_existing_masks(true);}
	static void generate_mp4(const std::string & value){worm_learner.generate_mp4(true);}
	static void generate_wmv(const std::string & value){worm_learner.generate_mp4(false);}

	/*****************************
	Image Processing Tasks
	*****************************/
	static void spatial_median(const std::string & value){worm_learner.apply_spatial_average();}
	static void difference_threshold(const std::string & value){worm_learner.difference_threshold();}
	static void adaptive_threshold(const std::string & value){worm_learner.apply_threshold();}
	static void movement_threshold(const std::string & value){
		std::vector<dialog_file_type> foo;
			foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
			foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
			std::string filename = open_file_dialog("Load Image",foo);
			if (filename != "") worm_learner.calculate_movement_threshold(filename);
	}
	static void movement_threshold_vis(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		std::string filename = open_file_dialog("Load Image",foo);
		if (filename != "")worm_learner.calculate_movement_threshold(filename,true);
	}
	static void two_stage_threshold(const std::string & value){worm_learner.two_stage_threshold();}
	static void two_stage_threshold_vis(const std::string & value){worm_learner.two_stage_threshold(true);}
	static void remove_large_objects(const std::string & value){worm_learner.remove_large_objects();}
	static void morph_manip(const std::string & value){worm_learner.run_binary_morpholgical_manipulations();}
	static void zhang_thinning(const std::string & value){worm_learner.zhang_thinning();}
	static void stretch_lossless(const std::string & value){worm_learner.stretch_levels();}
	static void stretch_lossy(const std::string & value){worm_learner.stretch_levels_approx();}
	static void compress_dark(const std::string & value){worm_learner.compress_dark_noise();}
	static void grayscale_from_blue(const std::string & value){worm_learner.grayscale_from_blue();}
	static void vertical_offset(const std::string & value){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		std::string filename = open_file_dialog("Load Image",foo);
		if (filename != "")worm_learner.calculate_vertical_offset(filename);
	}
	static void heat_map_overlay(const std::string & value){worm_learner.calculate_heatmap_overlay();}
	static void test_resample(const std::string & value){worm_learner.resize_image();}
	static void sharpen(const std::string & value){worm_learner.sharpen();}
	/*****************************
	Experiment and Model Selection
	*****************************/
	static void select_experiment(const std::string & value){
		ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
		
		worm_learner.data_selector.set_current_experiment(value,sql());
		sql.release();
		update_region_choice_menu();
		update_strain_choice_menu();
		ns_update_information_bar();
	}
	
	static void specifiy_model(const std::string & value){
		for (unsigned int i = 0; i < worm_learner.model_specifications.size(); ++i){
			if (worm_learner.model_specifications[i]->model_name == value){
				worm_learner.set_svm_model_specification(*worm_learner.model_specifications[i]);
				return;
			}
		}
		throw ns_ex("Could not recognize model name:") << value;
	}
	/*****************************
	File Tasks
	*****************************/
	
	static void set_database(const std::string & data){
		image_server.set_sql_database(data,false);
		cerr << "Switching to database " << data << "\n";
		get_menu_handler()->update_experiment_choice(*get_menu_bar());
	}
	static void file_open(const std::string & data){
		cout << ns_get_input_string("TITLE","GOBER");
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		std::string filename = open_file_dialog("Load Image",foo);
		if (filename != ""){
			worm_learner.load_file(filename);
			worm_learner.draw();
		}
	}
	static void file_save(const std::string & data){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		std::string filename = save_file_dialog("Save Image",foo,"tif");		
		if (filename != "")
			worm_learner.save_current_image(filename);
	}
	static void file_open_16_bit_dark(const std::string & data){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		std::string filename = open_file_dialog("Load 16-bit Image",foo);
		if (filename != "")
			worm_learner.load_16_bit<ns_features_are_dark>(filename);
	}
	static void file_open_16_bit_light(const std::string & data){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("TIF (*.tif)","tif"));
		foo.push_back(dialog_file_type("JPEG (*.jpg)","jpg"));
		std::string filename = open_file_dialog("Load 16-bit Image",foo);
		if (filename != "")
			worm_learner.load_16_bit<ns_features_are_light>(filename);
	}
	static void file_quit(const std::string & data){
		exit(0);
	}

	static void upload_strain_metadata(const std::string & data){
		std::vector<dialog_file_type> foo;
		foo.push_back(dialog_file_type("comma-separated value file (*.csv)","csv"));
		std::string filename = open_file_dialog("Load Strain Metadata File",foo);
		worm_learner.load_strain_metadata_into_database(filename);
	}

public:

	/*****************************
	Menu Specification
	*****************************/

	void update_experiment_choice(Fl_Menu_Bar & bar){		
		bar.menu(NULL);
		clear();
		ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
		worm_learner.data_selector.load_experiment_names(sql());
		add_menus();
		build_menus(bar);
		bar.redraw();
	}

	ns_worm_terminal_main_menu_organizer(){
		add_menus();
	}
	
	static std::string slow_moving_name(){return "Require Animals to be slow moving before entering clump";}

	void add_menus(){

		ns_menu_item_spec exp_spec(select_experiment,"&File/_Select Current Experiment");

		for (unsigned int i = 0; i < worm_learner.data_selector.experiment_groups.size(); i++){
			for (unsigned int j = 0; j < worm_learner.data_selector.experiment_groups[i].size(); j++){
				const bool last_in_group( j +1 ==  worm_learner.data_selector.experiment_groups[i].size());
				const bool last_group_in_list(i+1 == worm_learner.data_selector.experiment_groups.size());
				const std::string div((last_in_group&&!last_group_in_list)?"_":"");
				exp_spec.options.push_back(worm_learner.data_selector.experiment_groups[i][j].experiment_group_name + "/" + div + worm_learner.data_selector.experiment_groups[i][j].name);
			}
		}
		
		add(exp_spec);

		
	//	add(ns_menu_item_spec(run_animation_trial,"File/_Run Animation Trial"));
		add(ns_menu_item_spec(file_open,"File/Open Image",FL_CTRL+'o'));
		//add(ns_menu_item_spec(file_open_16_bit_dark,"File/Open 16-bit Image (dark)"));
		//add(ns_menu_item_spec(file_open_16_bit_light,"File/_Open 16-bit Image (light)"));
		add(ns_menu_item_spec(file_save,"File/_Save Image",FL_CTRL+'s'));
		add(ns_menu_item_spec(file_quit,"File/Quit",FL_CTRL+'q'));
		
		add(ns_menu_item_spec(save_current_areas,"&Plate Locations/Define Scan Areas/(Open Preview Capture Image and Draw Scan Areas)",0,FL_MENU_INACTIVE));
		add(ns_menu_item_spec(save_current_areas,"Plate Locations/Define Scan Areas/_Save Selected Scan Areas to Disk"));
		add(ns_menu_item_spec(clear_current_areas,"Plate Locations/Define Scan Areas/Clear Selected Scan Areas"));

		//add(ns_menu_item_spec(clipboard_copy,"Clipboard/Copy",FL_CTRL+'c'));
		//add(ns_menu_item_spec(clipboard_paste,"Clipboard/Paste",FL_CTRL+'v'));

		add(ns_menu_item_spec(masks_generate_composite,"Plate Locations/Define Sample Masks/Generate Experiment Mask Composite"));
		add(ns_menu_item_spec(masks_generate_composite,"Plate Locations/Define Sample Masks/(Draw Plate Locations on Mask using Photoshop or GIMP)",0,FL_MENU_INACTIVE));
		add(ns_menu_item_spec(masks_process_composite,"Plate Locations/Define Sample Masks/Analyze Plate Locations Drawn on Experiment Mask Composite"));
		add(ns_menu_item_spec(masks_submit_composite,"Plate Locations/Define Sample Masks/_Submit Analyzed Experiment Mask Composite to Cluster"));
		add(ns_menu_item_spec(open_individual_mask,"Plate Locations/Define Sample Masks/Individual Sample Masks/Open Mask"));
		add(ns_menu_item_spec(view_current_mask,"Plate Locations/Define Sample Masks/Individual Sample Masks/View Current Mask"));
	//	add(ns_menu_item_spec(apply_mask_on_current,"Masks/Mask Analysis/Individual Masks/Apply Mask on Current Image"));
		add(ns_menu_item_spec(submit_individual_mask_to_server,"Plate Locations/Define Sample Masks/Individual Sample Masks/Submit Analyzed Mask to Cluster"));
		
		add(ns_menu_item_spec(start_storyboard_annotation_whole_experiment,"&Annotation/(Generate Storyboards Prior to Annotation)",0,FL_MENU_INACTIVE));
		ns_menu_item_spec st_an(start_storyboard_annotation_whole_experiment,"Annotation/Browse Entire Experiment");
		st_an.options.push_back(ns_menu_item_options("Immediately After Each Worm's Death"));
		st_an.options.push_back(ns_menu_item_options("After All Worms Have Died"));
		add(st_an);
		ns_menu_item_spec st_an2(start_storyboard_annotation_plate,"Annotation/_Browse Single Plate");
		st_an2.options.push_back(ns_menu_item_options("Immediately After Each Worm's Death"));
		st_an2.options.push_back(ns_menu_item_options("After All Worms Have Died"));
		add(st_an2);
		add(ns_menu_item_spec(stop_posture_annotation,"Annotation/Stop Annotation"));

		add(ns_menu_item_spec(generate_survival_curves,"&Data Files/_Death Times/Generate Death Times for Current Experiment"));
		add(ns_menu_item_spec(generate_survival_curves_for_experiment_group,"Data Files/Death Times/Generate Death Times for all Experiment in Experiment Group"));			 

		add(ns_menu_item_spec(generate_area_movement,"Data Files/Movement Data/_Generate Movement State Time Series"));
		//add(ns_menu_item_spec(generate_experiment_summary_movement_image_quantification_analysis_data,"Data/Movement/Generate Summary Time Path Image Analysis Quantification Data"));
		add(ns_menu_item_spec(generate_experiment_detailed_movement_image_quantification_analysis_data,"Data Files/Movement Data/Posture Analysis Data/Machine Event Times"));
		add(ns_menu_item_spec(generate_experiment_detailed_w_by_hand_movement_image_quantification_analysis_data,"Data Files/Movement Data/Posture Analysis Data/_By Hand Event Times"));
		add(ns_menu_item_spec(generate_experiment_abbreviated_movement_image_quantification_analysis_data,"Data Files/Movement Data/Posture Analysis Data/Abbreviated"));
	
		ns_menu_item_spec st2(generate_single_frame_posture_image_pixel_data,"Data Files/Movement Data/Generate Single Frame Posture Image Data");
		st2.options.push_back(ns_menu_item_options("Experiment"));
		st2.options.push_back(ns_menu_item_options("Single Plate"));
		add(st2);
		add(ns_menu_item_spec(generate_timing_data,"Data Files/_Other Statistics/Generate Scanner Timing Data for Current Experiment"));
		add(ns_menu_item_spec(generate_timing_data_all_exp,"Data Files/Other Statistics/_Generate Scanner Timing Data for All Experiments in Group"));
		add(ns_menu_item_spec(generate_region_stats,"Data Files/Other Statistics/Generate Image Statistics for all regions in current Experiment"));
		add(ns_menu_item_spec(generate_region_stats_for_all_regions_in_group,"Data Files/Other Statistics/_Generate Image Statistics for all Regions in current Experiment Group"));
	//	add(ns_menu_item_spec(generate_detailed_animal_data_file,"Data/Statistics/_Generate Detailed Animal Statistics for current experiment"));
		add(ns_menu_item_spec(export_experiment_data,"Data Files/Transfer and Backup/Export Database Contents for Current Experiment"));
		add(ns_menu_item_spec(import_experiment_data,"Data Files/Transfer and Backup/Import Experiment from Backup"));
	
		//add(ns_menu_item_spec(generate_survival_curve_from_hand_annotations,"&Calibration/Generate Survival Curves from by hand annotations"));
		add(ns_menu_item_spec(compare_machine_and_by_hand_annotations,"&Calibration/Compare by-hand annotations to Machine"));
		ns_menu_item_spec st4(generate_movement_image_analysis_optimization_data,"Calibration/Generate Threshold Posture Model Parameter Optimization File from By Hand Annotations");
		
		st4.options.push_back(ns_menu_item_options("Using Thermotolerance Parameter Range"));
		st4.options.push_back(ns_menu_item_options("Using Lifespan Parameter Range"));
		st4.options.push_back(ns_menu_item_options("Using Quiecent Lifespan Parameter Range"));
		add(st4);
		
		add(ns_menu_item_spec(generate_worm_markov_posture_model_from_by_hand_annotations,"Calibration/Build Hidden Markov Posture Model From By Hand Annotations"));
		
	

		
		ns_menu_item_spec model_spec(specifiy_model,"&Testing/Worm Detection/Specify SVM Model");
		for (unsigned int i = 0; i < worm_learner.model_specifications.size(); i++)
			model_spec.options.push_back(ns_menu_item_options(worm_learner.model_specifications[i]->model_name));
	
		add(model_spec);
		add(ns_menu_item_spec(spatial_median,"Testing/Image Processing/Spatial Median Filter"));
		add(ns_menu_item_spec(difference_threshold,"Testing/Image Processing/Difference Threshold"));
		add(ns_menu_item_spec(adaptive_threshold,"Testing/Image Processing/Adaptive Threshold"));
		add(ns_menu_item_spec(movement_threshold,"Testing/Image Processing/Movement Threshold"));
		add(ns_menu_item_spec(movement_threshold_vis,"Testing/Image Processing/Movement Threshold (vis)"));
		add(ns_menu_item_spec(two_stage_threshold,"Testing/Image Processing/Two Stage Threshold"));
		add(ns_menu_item_spec(two_stage_threshold_vis,"Testing/Image Processing/_Two Stage Threshold (vis)"));
		add(ns_menu_item_spec(remove_large_objects,"Testing/Image Processing/Remove Large Objects"));
		add(ns_menu_item_spec(morph_manip,"Testing/Image Processing/Morphological Manipulations"));
		add(ns_menu_item_spec(zhang_thinning,"Testing/Image Processing/_Zhang Thinning"));
		add(ns_menu_item_spec(stretch_lossless,"Testing/Image Processing/Stretch Dynamic Range (Lossless)"));
		add(ns_menu_item_spec(stretch_lossy,"Testing/Image Processing/_Stretch Dynamic Range (Lossy)" ));
		add(ns_menu_item_spec(compress_dark,"Testing/Image Processing/Compress Dark Noise"));
		add(ns_menu_item_spec(grayscale_from_blue,"Testing/Image Processing/Grayscale from Blue Chanel"));
		add(ns_menu_item_spec(vertical_offset,"Testing/Image Processing/Calculate Vertical offset"));
		add(ns_menu_item_spec(heat_map_overlay,"Testing/Image Processing/_Calculate heat map overlay"));
		add(ns_menu_item_spec(test_resample,"Testing/Image Processing/Test Resample"));
		add(ns_menu_item_spec(sharpen,"Testing/Image Processing/Sharpen"));
		add(ns_menu_item_spec(calculate_erosion_gradient,"Testing/Image Processing/Calculate Erosion Gradient"));

		add(ns_menu_item_spec(show_objects,"Testing/Worm Detection/Show objects in image"));
		add(ns_menu_item_spec(detect_worms,"Testing/Worm Detection/Detect Worms" ));
		add(ns_menu_item_spec(show_region_edges,"Testing/Worm Detection/Show Region Edges" ));
		add(ns_menu_item_spec(view_region_collage,"Testing/Worm Detection/View Region Collage"));
		add(ns_menu_item_spec(view_spine_collage,"Testing/Worm Detection/View Spine Collage"));
		add(ns_menu_item_spec(view_spine_collage_stats,"Testing/Worm Detection/View Spine Collage with stats"));
		add(ns_menu_item_spec(view_reject_spine_collage,"Testing/Worm Detection/View Reject Spine Collage"));
		add(ns_menu_item_spec(view_reject_spine_collage_stats,"Testing/Worm Detection/View Reject Spine Collage with stats"));
		add(ns_menu_item_spec(output_feature_distributions,"Testing/Worm Detection/Output feature distributions"));
		add(ns_menu_item_spec(calculate_slow_movement,"Testing/Worm Detection/Calculate Slow Movement" ));

		add(ns_menu_item_spec(output_learning_set,"Testing/Machine Learning/Output Learning Image Set"));
		add(ns_menu_item_spec(auto_output_learning_set,"Testing/Machine Learning/Auto Output Learning Image Set"));
		add(ns_menu_item_spec(rethreshold_image_set,"Testing/Machine Learning/_Rethreshold Learning Image Set "));
		add(ns_menu_item_spec(generate_training_set,"Testing/Machine Learning/Generate worm training set image"));
		add(ns_menu_item_spec(process_training_set,"Testing/Machine Learning/_Process worm training set image"));
		add(ns_menu_item_spec(generate_SVM_training_data,"Testing/Machine Learning/Generate SVM Training Data"));
		add(ns_menu_item_spec(split_results,"Testing/Machine Learning/Split results into multiple regions"));
		
		add(ns_menu_item_spec(fix_headers_for_svm_training_set_images, "Testing/Machine Learning/Fix Scrambled Training Set Metadata"));
		add(ns_menu_item_spec(analyze_svm_results,"Testing/Machine Learning/_Analyze SVM Results"));
	//	add(ns_menu_item_spec(run_temporal_inference,"Testing/Machine Learning/Run temporal inference"));
		add(ns_menu_item_spec(remove_duplicates_from_training_set,"Testing/Machine Learning/_Remove duplicates from training set"));
		add(ns_menu_item_spec(generate_region_subset_time_series,"Testing/Machine Learning/Generate Region Subset time series"));
		add(ns_menu_item_spec(load_region_as_new_experiment,"Testing/Machine Learning/_Load Region Subset time series as new experiment"));
		add(ns_menu_item_spec(create_decimated_subset,"Testing/Machine Learning/Create decimated subset of directory"));
		add(ns_menu_item_spec(translate_fscore,"Testing/Machine Learning/_translate f-score file"));
		add(ns_menu_item_spec(generate_training_set_from_by_hand_annotations,"Testing/Machine Learning/Generate Training Set from By Hand Movement Annotations"));
		

	//	add(ns_menu_item_spec(compile_schedule_to_disk,"Config/Compile Submitted Capture Schedules to Disk"));
	//	add(ns_menu_item_spec(submit_schedule_to_db,"Config/_Submit Capture Schedules to Database"));	
		string version("Worm Browser v");
		version = version + ns_to_string(image_server.software_version_major()) + "." + ns_to_string(image_server.software_version_minor()) + "." + ns_to_string(image_server.software_version_compile()) + " (2013)";
		add(ns_menu_item_spec(set_database,string("&Config/") + version,0,FL_MENU_INACTIVE));
		add(ns_menu_item_spec(set_database,string("Config/_Nicholas Stroustrup, Harvard Systems Biology"),0,FL_MENU_INACTIVE));
		//add(ns_menu_item_spec(set_database,string("Config/_ "),0,FL_MENU_INACTIVE));
		ns_menu_item_spec db_spec(set_database,"&Config/_Set Database");
		for (unsigned int i = 0; i < worm_learner.databases_available.size(); i++)
			db_spec.options.push_back(worm_learner.databases_available[i]);
		add(db_spec);

		add(ns_menu_item_spec(do_not_overwrite_schedules,"Config/Set Behavior/Do not overwrite existing Experiment Specifications"));
		add(ns_menu_item_spec(overwrite_schedules,"Config/Set Behavior/_Overwrite existing Experiment Specifications"));
		add(ns_menu_item_spec(do_not_overwrite_existing_masks,"Config/Set Behavior/Do not overwrite existing sample masks"));
		add(ns_menu_item_spec(overwrite_existing_masks,"Config/Set Behavior/_Overwrite existing sample masks"));
		
		add(ns_menu_item_spec(generate_mp4,"Config/Set Behavior/Generate Mp4 Videos"));
		add(ns_menu_item_spec(generate_wmv,"Config/Set Behavior/_Generate WMV Videos"));
		add(ns_menu_item_spec(upload_strain_metadata,"Config/Set Behavior/Upload Strain Metadata to the Database"));
	}

	

};


class ns_worm_terminal_exclusion_menu_organizer : public ns_menu_organizer{
	static void pick_exclusion(const std::string & value){

		worm_learner.data_selector.set_censor_masking(ns_experiment_region_selector::censor_masking_from_string(value));
		::update_exclusion_choice_menu();
	}
public:
	std::string strain_menu_name(){return "File/_Select Current Strain";}
	void update_exclusion_choice(Fl_Menu_Bar & bar){
		bar.menu(NULL);
		clear();
		if (!worm_learner.data_selector.experiment_selected()){
			bar.deactivate();
			return;
		}
		if (worm_learner.data_selector.samples.size() == 0){
			bar.deactivate();
			return;
		}
		if (!worm_learner.data_selector.region_selected()){
			if (!worm_learner.data_selector.select_default_sample_and_region()){
				bar.deactivate();
				return;
			}
		}
		bar.activate();
		string title("");
		ns_experiment_region_selector::ns_censor_masking cur_masking(worm_learner.data_selector.censor_masking());
		ns_menu_item_spec spec(pick_exclusion,ns_experiment_region_selector::censor_masking_string(cur_masking));
		string sep;
		ns_experiment_region_selector::ns_censor_masking op[3] = {ns_experiment_region_selector::ns_show_all,
			ns_experiment_region_selector::ns_hide_censored,
			ns_experiment_region_selector::ns_hide_uncensored};

		for (unsigned int i = 0; i < 3; i++){
			if (op[i] == cur_masking)
				continue;
			else spec.options.push_back(ns_menu_item_options(ns_experiment_region_selector::censor_masking_string(op[i])));
		}
		add(spec);
		build_menus(bar);
		bar.redraw();
	}
};
class ns_worm_terminal_strain_menu_organizer : public ns_menu_organizer{
	static void pick_strain(const std::string & value){
		worm_learner.data_selector.select_strain(value);
		::update_strain_choice_menu();
		::update_region_choice_menu();
	}
public:
	std::string strain_menu_name(){return "File/_Select Current Strain";}
	void update_strain_choice(Fl_Menu_Bar & bar){
		bar.menu(NULL);
		clear();
		if (!worm_learner.data_selector.experiment_selected()){
			bar.deactivate();
			return;
		}
		if (worm_learner.data_selector.samples.size() == 0){
			bar.deactivate();
			return;
		}
		if (!worm_learner.data_selector.region_selected()){
			if (!worm_learner.data_selector.select_default_sample_and_region()){
				bar.deactivate();
				return;
			}
		}
		bar.activate();
		string title("");
		if (worm_learner.data_selector.strain_selected())
			title = worm_learner.data_selector.current_strain().device_regression_match_description();
		else title = "All Strains";
		ns_menu_item_spec spec(pick_strain,title);
		string sep;
		if (worm_learner.data_selector.strain_selected())
			spec.options.push_back(ns_menu_item_options("All Strains"));
		for (ns_experiment_region_selector::ns_experiment_strain_list::iterator p = worm_learner.data_selector.experiment_strains.begin(); p != worm_learner.data_selector.experiment_strains.end(); p++){
			if (p->first == title)
				continue;
			else spec.options.push_back(ns_menu_item_options(p->first));
		}
		add(spec);
		build_menus(bar);
		bar.redraw();
		/*bar.resize(experiment_bar_width(),
					h_-info_bar_height(),
					region_name_bar_width(),
					info_bar_height());*/
		/*if (worm_learner.current_behavior_mode() == ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture)
				ns_start_death_time_annotation(ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture);
		else if (worm_learner.current_behavior_mode() == ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture)
				ns_start_death_time_annotation(ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture);
		else if (worm_learner.current_behavior_mode() == ns_worm_learner::ns_annotate_death_times_in_region)
			/*do nothing 0;*/
	}
};


struct ns_asynch_region_picker{
	static ns_thread_return_type run_asynch(void * l){
		ns_asynch_region_picker launcher;
		std::string * region_name(static_cast<std::string *>(l));
		launcher.launch(*region_name);
		delete region_name;
		return true;
	}
	void launch(const std::string & region_name){
		try{
			if (worm_learner.current_behavior_mode()==ns_worm_learner::ns_annotate_storyboard_region){
				if (!worm_learner.prompt_to_save_death_time_annotations())
					return;
				worm_learner.stop_death_time_annotation();
				worm_learner.data_selector.select_region(region_name);
				::update_region_choice_menu();
				worm_learner.start_death_time_annotation(ns_worm_learner::ns_annotate_storyboard_region,worm_learner.current_storyboard_flavor);
				return;
			}
			else{
				worm_learner.data_selector.select_region(region_name);
				::update_region_choice_menu();
			}
		}
		catch(ns_ex & ex){
			cerr << "Error loading worm info:" << ex.text();
			worm_learner.death_time_solo_annotater.close_worm();
			show_worm_window = false;
			ns_set_menu_bar_activity(true);
		}
	}
};


class ns_worm_terminal_region_menu_organizer : public ns_menu_organizer{
	static void pick_region(const std::string & value){
		std::string *n(new std::string(value));
		ns_thread t(ns_asynch_region_picker::run_asynch,n);
		t.detach();
	}
public:
	std::string region_menu_name(){return "File/_Select Current Region";}
	void update_region_choice(Fl_Menu_Bar & bar){
		bar.menu(NULL);
		clear();
		if (!worm_learner.data_selector.experiment_selected()){
			bar.deactivate();
			return;
		}
		if (worm_learner.data_selector.samples.size() == 0){
			bar.deactivate();
			return;
		}
		if (!worm_learner.data_selector.region_selected())
			if (!worm_learner.data_selector.select_default_sample_and_region()){
				bar.deactivate();
				return;
			}
		bar.activate();
		ns_menu_item_spec spec(pick_region,worm_learner.data_selector.current_region().display_name);
		string sep;
		for (unsigned int i = 0; i < worm_learner.data_selector.samples.size(); i++){
			for (unsigned int j = 0; j < worm_learner.data_selector.samples[i].regions.size(); j++){
			//	if (j == worm_learner.data_selector.samples[i].regions.size() -1) sep = "_";
			//	else sep = "";
				if (worm_learner.data_selector.samples[i].regions[j].region_id == worm_learner.data_selector.current_region().region_id)
					continue;
				const bool unselected_strain(worm_learner.data_selector.strain_selected() && worm_learner.data_selector.samples[i].regions[j].region_metadata!=&worm_learner.data_selector.current_strain());
				
				spec.options.push_back(ns_menu_item_options(worm_learner.data_selector.samples[i].device + "/" + worm_learner.data_selector.samples[i].sample_name + "/" + worm_learner.data_selector.samples[i].regions[j].display_name,unselected_strain));
			
			}
		}
		add(spec);
		build_menus(bar);
		bar.redraw();
		/*bar.resize(experiment_bar_width(),
					h_-info_bar_height(),
					region_name_bar_width(),
					info_bar_height());*/
		if (worm_learner.current_behavior_mode() == ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture)
				ns_start_death_time_annotation(ns_worm_learner::ns_annotate_death_times_in_time_aligned_posture,worm_learner.current_storyboard_flavor);
		else if (worm_learner.current_behavior_mode() == ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture)
				ns_start_death_time_annotation(ns_worm_learner::ns_annotate_death_times_in_death_aligned_posture,worm_learner.current_storyboard_flavor);
		else if (worm_learner.current_behavior_mode() == ns_worm_learner::ns_annotate_death_times_in_region)
			/*do nothing*/0;
	}
};

void ns_handle_death_time_annotation_button(Fl_Widget * w, void * data);
void ns_handle_death_time_solo_annotation_button(Fl_Widget * w, void * data);

class ns_death_event_annotation_group : public Fl_Pack {
public:
	Fl_Button * forward_button,
			  * back_button,
			  * play_forward_button,
			  * play_reverse_button,
			  * stop_button,
			  * save_button;
	enum{button_width=24,button_height=24,window_width=168,window_height=24};
	int handle(int e){
		//don't claim keystrokes--we want them to be passed on to the glwindow for navigation
		switch(e){
			case FL_KEYDOWN:
				return 0;
			case FL_KEYUP:
				return 0;
		}
		return Fl_Pack::handle(e);
	}
	ns_death_event_annotation_group(int x,int y, int w, int h, bool add_save_button) : Fl_Pack(x,y,w,h) {
		type(Fl_Pack::HORIZONTAL);
		spacing(0);
		back_button 		= new Fl_Button(0*button_width,0,button_width,button_height,"@-2<");
		back_button->callback(ns_handle_death_time_annotation_button,
			new ns_image_series_annotater::ns_image_series_annotater_action(ns_death_time_posture_annotater::ns_back));
		play_reverse_button = new Fl_Button(1*button_width,0,button_width,button_height,"@-2<<");
		play_reverse_button->callback(ns_handle_death_time_annotation_button,
			new ns_image_series_annotater::ns_image_series_annotater_action(ns_death_time_posture_annotater::ns_fast_back));
		stop_button 		= new Fl_Button(2*button_width,0,button_width,button_height,"@-9square");
		stop_button->callback(ns_handle_death_time_annotation_button,
			new ns_image_series_annotater::ns_image_series_annotater_action(ns_death_time_posture_annotater::ns_stop));
		play_forward_button = new Fl_Button(3*button_width,0,button_width,button_height,"@-2>>");
		play_forward_button->callback(ns_handle_death_time_annotation_button,
			new ns_image_series_annotater::ns_image_series_annotater_action(ns_death_time_posture_annotater::ns_fast_forward));
		forward_button 		= new Fl_Button(4*button_width,0,button_width,button_height,"@-2>");
		forward_button->callback(ns_handle_death_time_annotation_button,
			new ns_image_series_annotater::ns_image_series_annotater_action(ns_death_time_posture_annotater::ns_forward));
		if (add_save_button){
			save_button 		= new Fl_Button(5*button_width,0,2*button_width,button_height,"Save");
			save_button->callback(ns_handle_death_time_annotation_button,
				new ns_image_series_annotater::ns_image_series_annotater_action(ns_death_time_posture_annotater::ns_save));
		}
		else{
			save_button=0;
		}
		end();
	//	clear_visible_focus();

	}
};
					

class ns_death_event_solo_annotation_group : public Fl_Pack {
public:
	Fl_Button * forward_button,
			  * back_button,
			  * play_forward_button,
			  * play_reverse_button,
			  * stop_button,
			  * goto_death_time_button,
			  * save_button;
	enum{button_width=18,button_height=18,all_buttons_width=108};
	int handle(int e){
		//don't claim keystrokes--we want them to be passed on to the glwindow for navigation
		switch(e){
			case FL_KEYDOWN:
				return 0;
			case FL_KEYUP:
				return 0;
		}
		return Fl_Pack::handle(e);
	}
	ns_death_event_solo_annotation_group(int x,int y, int w, int h, bool add_save_button) : Fl_Pack(x,y,w,h) {
		type(Fl_Pack::HORIZONTAL);
		spacing(0);
		back_button 		= new Fl_Button(0*button_width,0,button_width,button_height,"@-2<");
		back_button->callback(ns_handle_death_time_solo_annotation_button,
			new ns_death_time_solo_posture_annotater::ns_image_series_annotater_action(ns_death_time_solo_posture_annotater::ns_back));
		play_reverse_button = new Fl_Button(1*button_width,0,button_width,button_height,"@-2<<");
		play_reverse_button->callback(ns_handle_death_time_solo_annotation_button,
			new ns_death_time_solo_posture_annotater::ns_image_series_annotater_action(ns_death_time_solo_posture_annotater::ns_fast_back));
		stop_button 		= new Fl_Button(2*button_width,0,button_width,button_height,"@-9square");
		stop_button->callback(ns_handle_death_time_solo_annotation_button,
			new ns_death_time_solo_posture_annotater::ns_image_series_annotater_action(ns_death_time_solo_posture_annotater::ns_stop));

		goto_death_time_button = new Fl_Button(3*button_width,0,button_width,button_height,"@-9square"); 
		goto_death_time_button->callback(ns_handle_death_time_solo_annotation_button,
			new ns_death_time_solo_posture_annotater::ns_image_series_annotater_action(ns_death_time_solo_posture_annotater::ns_rewind_to_zero));

		play_forward_button = new Fl_Button(4*button_width,0,button_width,button_height,"@-2>>");
		play_forward_button->callback(ns_handle_death_time_solo_annotation_button,
			new ns_death_time_solo_posture_annotater::ns_image_series_annotater_action(ns_death_time_solo_posture_annotater::ns_fast_forward));
		forward_button 		= new Fl_Button(5*button_width,0,button_width,button_height,".");
		forward_button->callback(ns_handle_death_time_solo_annotation_button,
			new ns_death_time_solo_posture_annotater::ns_image_series_annotater_action(ns_death_time_solo_posture_annotater::ns_forward));
		save_button = new Fl_Button(8*button_width,0,3*button_width,button_height,"Save");
		save_button->  callback(ns_handle_death_time_solo_annotation_button,
			new ns_death_time_solo_posture_annotater::ns_image_series_annotater_action(ns_death_time_solo_posture_annotater::ns_write_quantification_to_disk));

		end();
	//	clear_visible_focus();

	}
};
			

// APP WINDOW CLASS
class ns_dialog_window : public Fl_Window {
public:
	Fl_Button* a,*b,*c;
    ns_dialog_window (int W,int H,const char*L=0) : Fl_Window(W,H,L){
		
		a = new Fl_Button(W/5,H/4,W/5,H/4,"Hello!");
		b = new Fl_Button(2*W/5,H/4,W/5,H/4,"There!");
		c = new Fl_Button(3*W/5,H/4,W/5,H/4,"Beautiful!");
  
	    end();
    }
	/*int handle(int state) {
		redraw();
		return Fl_Window::handle(state);
	};*/
};

/*
ns_thread_return_type asynch_redisplay(void *);*/

// APP WINDOW CLASS
class ns_worm_terminal_main_window : public Fl_Window {
	bool have_focus;
public:
    ns_worm_terminal_gl_window * gl_window;
	ns_death_event_annotation_group * annotation_group;
	Fl_Menu_Bar *main_menu;         
	Fl_Menu_Bar *region_menu;       
	Fl_Menu_Bar *exclusion_menu;  
	Fl_Menu_Bar *strain_menu;  
	Fl_Output * experiment_name_bar;
	//Fl_Output * region_name_bar;
	Fl_Output * info_bar;
	//ns_dialog_window * dialog_window;
//	Fl_Output * spacer_bar;
	ns_worm_terminal_main_menu_organizer * main_menu_handler;
	ns_worm_terminal_region_menu_organizer * region_menu_handler;
	ns_worm_terminal_strain_menu_organizer * strain_menu_handler;
	ns_worm_terminal_exclusion_menu_organizer * exclusion_menu_handler;

	static unsigned long menu_height() {return 23;}
	static unsigned long info_bar_height(){return 24;}

	static unsigned long experiment_bar_width(){return 145;}
	static unsigned long region_name_bar_width(){return 75;}
	static unsigned long exclusion_bar_width(){return 30;}
	static unsigned long strain_bar_width(){return 140;}

	static unsigned long border_height() {return 0;}
	static unsigned long border_width() {return 0;}
	~ns_worm_terminal_main_window(){
		ns_safe_delete(gl_window);
		ns_safe_delete(main_menu);
		ns_safe_delete(region_menu);
		ns_safe_delete(main_menu_handler);
		ns_safe_delete(region_menu_handler);
		ns_safe_delete(strain_menu_handler);
		ns_safe_delete(exclusion_menu_handler);
		ns_safe_delete(experiment_name_bar);
		ns_safe_delete(info_bar);
		ns_safe_delete(exclusion_menu);
	}

	void update_information_bar(const std::string & status="-1"){
		experiment_name_bar->value(worm_learner.data_selector.current_experiment_name().c_str());
		cerr << "Selecting experiment " << worm_learner.data_selector.current_experiment_name().c_str() << "\n";
	/*	if (worm_learner.data_selector.region_selected() && worm_learner.data_selector.sample_selected()){
			region_name_bar->value((worm_learner.data_selector.current_sample().sample_name + "::" + 
								   worm_learner.data_selector.current_region().region_name).c_str());
		}*/
		if (status != "-1")
			info_bar->value(status.c_str());
	}

    ns_worm_terminal_main_window(int W,int H,const char*L=0) : Fl_Window(50,50,W,H,L), draw_animation(false),last_draw_animation(false),have_focus(false){
        // OpenGL window
        gl_window = new ns_worm_terminal_gl_window(0, menu_height(), w()-border_width(), h()-menu_height()-info_bar_height());

    	main_menu = new Fl_Menu_Bar(0,0,W,menu_height());
		region_menu = new Fl_Menu_Bar(experiment_bar_width(),H-info_bar_height(),region_name_bar_width(),info_bar_height());
		strain_menu = new Fl_Menu_Bar(experiment_bar_width()+region_name_bar_width(),H-info_bar_height(),strain_bar_width(),info_bar_height());
		exclusion_menu = new Fl_Menu_Bar(experiment_bar_width() + region_name_bar_width() + strain_bar_width(),H-info_bar_height(),exclusion_bar_width(),info_bar_height());
		
		main_menu->textsize(main_menu->textsize()-2);
		main_menu->textfont(FL_HELVETICA);
		region_menu->textsize(region_menu->textsize()-4);
		region_menu->textfont(FL_HELVETICA);
		strain_menu->textsize(region_menu->textsize());
		strain_menu->textfont(FL_HELVETICA);
		exclusion_menu->textsize(region_menu->textsize());
		exclusion_menu->textfont(FL_HELVETICA);
		
		main_menu_handler = new ns_worm_terminal_main_menu_organizer();
		main_menu_handler->build_menus(*main_menu);

		//dialog_window = new ns_dialog_window(300,150,"Dialog");

		region_menu_handler = new ns_worm_terminal_region_menu_organizer();
		region_menu_handler->update_region_choice(*region_menu);
		
		strain_menu_handler = new ns_worm_terminal_strain_menu_organizer();
		strain_menu_handler->update_strain_choice(*strain_menu);

		
		exclusion_menu_handler = new ns_worm_terminal_exclusion_menu_organizer();
		exclusion_menu_handler->update_exclusion_choice(*exclusion_menu);

		experiment_name_bar = new Fl_Output(0,
											h()-info_bar_height(),
											experiment_bar_width(),
											info_bar_height());
		experiment_name_bar->textsize(experiment_name_bar->textsize()-4);
		experiment_name_bar->textfont(FL_HELVETICA);

		info_bar = new Fl_Output(			experiment_bar_width()+region_name_bar_width() + strain_bar_width() + exclusion_bar_width(),
											h()-info_bar_height(),
											W-experiment_bar_width()-region_name_bar_width()-strain_bar_width() - ns_death_event_annotation_group::window_width,
											info_bar_height());
		
		info_bar->textsize(info_bar->textsize()-4);
		info_bar->textfont(FL_HELVETICA);
	

		annotation_group = new ns_death_event_annotation_group(W-ns_death_event_annotation_group::window_width,
															  H - ns_death_event_annotation_group::window_height,
															  ns_death_event_annotation_group::window_width,
															  ns_death_event_annotation_group::window_height,true);

		annotation_group->deactivate();
		experiment_name_bar->box(FL_EMBOSSED_BOX);
		info_bar->box(FL_EMBOSSED_BOX);

		info_bar->deactivate();
		//region_name_bar->deactivate();
		experiment_name_bar->deactivate();
		update_information_bar("Welcome!");

	

		experiment_name_bar->value(worm_learner.data_selector.current_experiment_name().c_str());

		info_bar->value("Welcome!");
	    end();
		
    }

	static ns_vector_2i image_window_size_difference(){
		return ns_vector_2i(-(int)border_width(),-(int)menu_height()-(int)info_bar_height());
	}
	void resize(int x, int y, int w_, int h_){
		ns_acquire_lock_for_scope lock(worm_learner.main_window.display_lock,__FILE__,__LINE__);
		worm_learner.main_window.ideal_window_size = ns_vector_2d(w_,h_);
		worm_learner.main_window.specified_gl_image_size = worm_learner.main_window.ideal_window_size+image_window_size_difference();
		lock.release();
		Fl_Window::resize(x,y,w_,h_);
	
		gl_window->resize(0,menu_height(),worm_learner.main_window.specified_gl_image_size.x,
			worm_learner.main_window.specified_gl_image_size.y);
	
		main_menu->resize(0,0,w_,menu_height());

		experiment_name_bar->resize(		0,
											h_-info_bar_height(),
											experiment_bar_width(),
											info_bar_height());
		region_menu->resize(experiment_bar_width(),
											h_-info_bar_height(),
											region_name_bar_width(),
										
	info_bar_height());
		strain_menu->resize(experiment_bar_width()+region_name_bar_width(),
											h_-info_bar_height(),
											strain_bar_width(),
											info_bar_height());
		exclusion_menu->resize(experiment_bar_width()+region_name_bar_width()+strain_bar_width(),
											h_-info_bar_height(),
											exclusion_bar_width(),
											info_bar_height());
		info_bar->resize(experiment_bar_width()+region_name_bar_width()+strain_bar_width()+
						 exclusion_bar_width(),
											h_-info_bar_height(),
											w_-experiment_bar_width()-region_name_bar_width()-strain_bar_width()-ns_death_event_annotation_group::window_width,
											info_bar_height());

		annotation_group->resize(w_-ns_death_event_annotation_group::window_width,
								 h_ - ns_death_event_annotation_group::window_height,
								 ns_death_event_annotation_group::window_width,
								 ns_death_event_annotation_group::window_height);
		
	}
	 
	void update_region_choice_menu(){
		region_menu_handler->update_region_choice(*region_menu);
		update_information_bar();
	
	}
	void update_exclusion_choice_menu(){
		exclusion_menu_handler->update_exclusion_choice(*exclusion_menu);
		update_information_bar();
	}
	void update_strain_choice_menu(){
		strain_menu_handler->update_strain_choice(*strain_menu);
		update_information_bar();
	}
	bool draw_animation;
	bool last_draw_animation;

	private:
		
		
	int handle(int state) {


		switch(state){ 
			case FL_FOCUS:
				have_focus = true;
				break;
			case FL_UNFOCUS:
				have_focus = false;
				break;
			case FL_KEYDOWN:{
				//if (have_focus){
					Fl::lock();
					int c(Fl::event_key());
					if (c!=0){
						if (worm_learner.register_main_window_key_press(c,
														Fl::event_key(FL_Shift_L) || Fl::event_key(FL_Shift_R),
														Fl::event_key(FL_Control_L) || Fl::event_key(FL_Control_R),
														Fl::event_key(FL_Alt_L) || Fl::event_key(FL_Alt_R)
														))
														return 1;
					}
					Fl::release();
				//}
				break;
			}
		
		}
	//	dialog_window->handle(state);
		//cerr << "Could not handle " << state << "\n"; 
		return Fl_Window::handle(state);
	}
};


// APP WINDOW CLASS
class ns_worm_terminal_worm_window : public Fl_Window {
	bool have_focus;
public:
    ns_worm_gl_window * gl_window;
	ns_death_event_solo_annotation_group * annotation_group;
	
	static unsigned long info_bar_height(){return 18;}
	/*
	static unsigned long experiment_bar_width(){return 145;}
	static unsigned long region_name_bar_width(){return 50;}
	static unsigned long strain_bar_width(){return 140;}
	*/
	static unsigned long border_height() {return 0;}
	static unsigned long border_width() {return 0;}
	~ns_worm_terminal_worm_window (){
		delete gl_window;
		delete annotation_group;
	}

	/*void update_information_bar(const std::string & status="-1"){
		experiment_name_bar->value(worm_learner.data_selector.current_experiment_name().c_str());
		cerr << "Selecting experiment " << worm_learner.data_selector.current_experiment_name().c_str() << "\n";
		if (status != "-1")
			info_bar->value(status.c_str());
	}*/
    ns_worm_terminal_worm_window (int W,int H,const char*L=0) : Fl_Window(W,H,L), have_focus(false){
        // OpenGL window
		begin();
        gl_window = new  ns_worm_gl_window(30, 0, w()+image_window_size_difference().x, h()+image_window_size_difference().y);

		
		annotation_group = new ns_death_event_solo_annotation_group(0,
															  H - ns_death_event_solo_annotation_group::button_height,
															  ns_death_event_solo_annotation_group::all_buttons_width,
															  ns_death_event_solo_annotation_group::button_height,
															  false);

		//annotation_group->deactivate();
		//annotation_group->hide();
		
	    end();
		

	//	ask_modal_question("Hi","1","2","3");
    }
	
	static ns_vector_2i image_window_size_difference(){
		return ns_vector_2i(-(long)border_width(),-(long)ns_death_event_solo_annotation_group::button_height);
	}
	void resize(int x, int y, int w_, int h_){
		Fl_Window::resize(x,y,w_,h_);
//		cerr << "worm window responding to resize request of " << w_ << "x" << h_ << "\n";
	//	cerr << "Resizing to " << w_+image_window_size_difference().x << "x" << h_+image_window_size_difference().y << "\n";

		gl_window->resize(0,0,w_+image_window_size_difference().x,h_+image_window_size_difference().y);
		//xxxx
	//	gl_window->redraw();
		
		annotation_group->resize(0,
								 h_ - ns_death_event_solo_annotation_group::button_height,
								 ns_death_event_solo_annotation_group::all_buttons_width,
								 ns_death_event_solo_annotation_group::button_height);
		redraw();
	}
	private:
		
		
	int handle(int state) {


		switch(state){ 
			case FL_FOCUS:
				have_focus = true;
				break;
			case FL_UNFOCUS:
				have_focus = false;
				break;
			case FL_KEYDOWN:{
				//if (have_focus){
					Fl::lock();
					int c(Fl::event_key());
					if (c!=0){
						if (worm_learner.register_worm_window_key_press(c,
														Fl::event_key(FL_Shift_L) || Fl::event_key(FL_Shift_R),
														Fl::event_key(FL_Control_L) || Fl::event_key(FL_Control_R),
														Fl::event_key(FL_Alt_L) || Fl::event_key(FL_Alt_R)
														))
														return 1;
					}
					Fl::release();
				//}
				break;
			}
		
		}
	//	dialog_window->handle(state);
		//cerr << "Could not handle " << state << "\n"; 
		return Fl_Window::handle(state);
	}
};


void ns_specifiy_worm_details(const unsigned long region_id,const ns_stationary_path_id & worm, const ns_death_time_annotation & sticky_properties, std::vector<ns_death_time_annotation> & event_times){
	worm_learner.storyboard_annotater.specifiy_worm_details(region_id,worm,sticky_properties,event_times);
	worm_learner.storyboard_annotater.redraw_current_metadata();
	worm_learner.storyboard_annotater.request_refresh();
}


struct ns_asynch_annotation_saver{
	static ns_thread_return_type run_asynch(void * l){
		ns_asynch_annotation_saver launcher;
		launcher.launch();
		return true;
	}
	void launch(){
		try{
			ns_set_menu_bar_activity(false);
			worm_learner.navigate_death_time_annotation(ns_image_series_annotater::ns_save);
			ns_set_menu_bar_activity(true);
		}
		catch(ns_ex & ex){
			cerr << "Error loading worm info:" << ex.text();
			worm_learner.death_time_solo_annotater.close_worm();
			show_worm_window = false;
			ns_set_menu_bar_activity(true);
		}
	}
};
void ns_handle_death_time_annotation_button(Fl_Widget * w, void * data){
	ns_image_series_annotater::ns_image_series_annotater_action action(*static_cast<ns_image_series_annotater::ns_image_series_annotater_action *>(data));
	if (action == ns_image_series_annotater::ns_save){
		ns_thread t(ns_asynch_annotation_saver::run_asynch,0);
		t.detach();
		return;
	}
	worm_learner.navigate_death_time_annotation(action);
}
void ns_handle_death_time_solo_annotation_button(Fl_Widget * w, void * data){
	ns_death_time_solo_posture_annotater::ns_image_series_annotater_action action(*static_cast<ns_death_time_solo_posture_annotater::ns_image_series_annotater_action *>(data));
	worm_learner.navigate_solo_worm_annotation(action);
	//Fl::focus(current_window->gl_window);
}

void all_menu_callback(Fl_Widget*w, void*data) {
 	ns_menu_organizer_callback_data  ww (*static_cast<ns_menu_organizer_callback_data *>(data));
	char pn[512];
	
	int ret(ww.bar->item_pathname(pn,512));
	if (ret == 0) ww.organizer->dispatch_request_asynch(pn);
	else if (ret == -1) throw ns_ex("Could not identify requested menu item");
	else if (ret == -2) throw ns_ex("Menu item is larger than buffer provided");
	else throw ns_ex("FLTK returned a cryptic error while processing menu request");
}
/*
void redraw_screen(){
	redraw_screen(0,0,false);
}*/

Fl_Menu_Bar * get_menu_bar(){
	return current_window->main_menu;
}
ns_worm_terminal_main_menu_organizer * get_menu_handler(){
	return current_window->main_menu_handler;
}
void update_region_choice_menu(){
	current_window->update_region_choice_menu();
}
void update_strain_choice_menu(){
	current_window->update_strain_choice_menu();
}

void update_exclusion_choice_menu(){
	current_window->update_exclusion_choice_menu();
}

void ns_update_information_bar(const std::string & status){
	cerr << status;
	current_window->update_information_bar(status);
}

void redraw_main_window(const unsigned long w, const unsigned long h,const bool resize){
	worm_learner.main_window.redraw_requested = false;
	Fl::awake();
	Fl::lock();
	if (resize){
		current_window->size(w,h);	
	}
	current_window->gl_window->damage(1);
	current_window->gl_window->redraw();
	Fl::check();
	Fl::unlock();
}


void redraw_worm_window(const unsigned long w, const unsigned long h,const bool resize){
	worm_learner.worm_window.redraw_requested = false;
	//ns_vector_2i n(ns_worm_terminal_main_window::border_width(),ns_worm_terminal_worm_window::info_bar_height());
	Fl::awake();
	Fl::lock();
	if (resize){
		worm_window->size(w,h);	
	}
	worm_window->gl_window->damage(1);
	worm_window->gl_window->redraw();
	Fl::check();
	Fl::unlock();
}


double init_time;
void ns_show_worm_display_error(){
	cerr << "Error! Could not show image.\n";
}

void ns_handle_menu_bar_activity_request();
void idle_main_window_update_callback(void *){
	//double last_time = c_time;
	//c_time =  GetTime() - init_time;
	//cerr << "FPS = " << 1.0/(time-last_time) << "\n";
	Fl::lock();
	try{
		ns_vector_2d cur_size(current_window->w(),current_window->h());
	//	worm_learner.main_window.ideal_window_size = worm_learner.main_window.ideal_image_size;
		//worm_learner.main_window.ideal_window_size.y+=ns_worm_terminal_main_window::menu_height()+ns_worm_terminal_main_window::info_bar_height();
		//worm_learner.main_window.ideal_window_size.x+= ns_worm_terminal_main_window::border_width();
		if ( abs((int)(worm_learner.main_window.ideal_window_size.x - cur_size.x)) > 3 || abs((int)(worm_learner.main_window.ideal_window_size.y- cur_size.y)) > 3){
			//current_window->size(current_window->size(worm_learner.main_window.ideal_window_size.x, worm_learner.ideal_current_window_height);
			current_window->size(worm_learner.main_window.ideal_window_size.x, worm_learner.main_window.ideal_window_size.y);
		//	current_window->gl_window->size(worm_learner.main_window.ideal_image_size.x, worm_learner.main_window.ideal_image_size.y);
		//	cerr << "Redrawing screen from " << cur_size << " to ideal " << worm_learner.main_window.ideal_window_size << "\n";
		}
		ns_handle_menu_bar_activity_request();
		if (worm_learner.current_annotater->refresh_requested()){
			worm_learner.current_annotater->display_current_frame();
		}
		if (worm_learner.main_window.redraw_requested){
			redraw_main_window(worm_learner.main_window.ideal_window_size.x,worm_learner.main_window.ideal_window_size.y,true);
		}
		ns_image_series_annotater::ns_image_series_annotater_action a(worm_learner.current_annotater->fast_movement_requested());
		if (a == ns_image_series_annotater::ns_fast_forward){
			worm_learner.current_annotater->step_forward(ns_show_worm_display_error);
			worm_learner.current_annotater->display_current_frame();
		}
		else if (a==ns_image_series_annotater::ns_fast_back){
			worm_learner.current_annotater->step_back(ns_show_worm_display_error);
			worm_learner.current_annotater->display_current_frame();
		}
		//draw busy animation if requested
		if (current_window->draw_animation){
			worm_learner.draw_animation(GetTime() - init_time);
		//	cerr << GetTime() << "\n";
			current_window->last_draw_animation = true;
		}
		//clear animation when finished
		if (!current_window->draw_animation && current_window->last_draw_animation){
			current_window->last_draw_animation = false;
			worm_learner.draw();
	//		redraw_screen();
		}
		if (show_worm_window){
			show_worm_window = false;
			worm_window->size(worm_learner.worm_window.ideal_window_size.x,worm_learner.worm_window.ideal_window_size.y);
			worm_window->show();
			ns_set_menu_bar_activity(true);	
		}
		if (hide_worm_window){
			hide_worm_window = false;
			worm_window->hide();
		}
		Fl::unlock();
	}
	catch(...){
		Fl::unlock();
	}
}
void ns_hide_worm_window(){
	hide_worm_window = true;
}

void idle_worm_window_update_callback(void *){
	//double last_time = c_time;
	//c_time =  GetTime() - init_time;
	//cerr << "FPS = " << 1.0/(time-last_time) << "\n";
	Fl::lock();
	try{
		ns_vector_2d cur_size(worm_window->w(),worm_window->h());
		/*worm_learner.worm_window.ideal_window_size = worm_learner.worm_window.ideal_window_size;
		worm_learner.worm_window.ideal_window_size.x+=ns_worm_terminal_worm_window::border_width();
		worm_learner.worm_window.ideal_window_size.y+=ns_worm_terminal_worm_window::info_bar_height();*/
		if ( abs((int)(worm_learner.worm_window.ideal_window_size.x - cur_size.x)) > 3 || abs((int)(worm_learner.worm_window.ideal_window_size.y - cur_size.y)) > 3){
			worm_window->size(worm_learner.worm_window.ideal_window_size.x, worm_learner.worm_window.ideal_window_size.y);
			//xxxx
		}
		if (worm_learner.death_time_solo_annotater.refresh_requested()){
			worm_learner.death_time_solo_annotater.display_current_frame();
		}
		if (worm_learner.worm_window.redraw_requested){
			redraw_worm_window(worm_learner.worm_window.ideal_window_size.x,worm_learner.worm_window.ideal_window_size.y,true);
		}
		ns_image_series_annotater::ns_image_series_annotater_action a(worm_learner.death_time_solo_annotater.fast_movement_requested());
		if (a == ns_image_series_annotater::ns_fast_forward){
			worm_learner.death_time_solo_annotater.step_forward(ns_hide_worm_window);
			worm_learner.death_time_solo_annotater.display_current_frame();
		}
		else if (a==ns_image_series_annotater::ns_fast_back){
			worm_learner.death_time_solo_annotater.step_back(ns_hide_worm_window);
			worm_learner.death_time_solo_annotater.display_current_frame();
		}
		Fl::unlock();
	}
	catch(...){
		Fl::lock();
	}
}



void ns_transfer_annotations(const std::string & annotation_source_filename, const std::string & annotation_destination_filename, const std::string & output_filename){
	
	ns_image_standard annotation_source, destination_image;

	ns_load_image(annotation_destination_filename,destination_image);
	ns_load_image(annotation_source_filename,annotation_source);
	//if (metadata_source_filename == "Y:\\image_server_storage\\partition_000\\2010_04_17_daf16_split_3\\training_set\\2010_04_17_daf16_split_3=163=ben_d=2506=1271547790=2010-04-17=19-43=250429=8031647=2=648488.tif"){
	//	cerr << "ok";
	//}
	ns_image_standard output;
	cerr << "Processing " << output_filename << "\n";
	try{
		ns_transfer_annotations(annotation_source, destination_image,output);
		ns_save_image(output_filename,output);
	}
	catch(ns_ex & ex){
		cerr << "Error: " << ex.text() << "\n";
	}

	/*
	if (file_source.properties().width != destination.properties().width ||
		file_source.properties().height != destination.properties().height)
		throw ns_ex("Image Size Mismatch: Source::") << file_source.properties().width << "x" << file_source.properties().height <<
		" vs " << destination.properties().width << "x" << destination.properties().height
		<< "\nMetadata: " << metadata_source_filename << "\n Image data " << destination_filename << "\n";
	*/

	//ns_save_image(destination_filename,annotated_image);
}

void ns_transfer_annotations_directory(const std::string & annotation_source_directory, const std::string & annotation_destination_directory, const std::string & output_destination_directory){
	ns_dir source_dir,
		   destination_dir;
	vector<std::string> source_files,
						destination_files;
	source_dir.load_masked(annotation_source_directory,"tif",source_files);
	destination_dir.load_masked(annotation_destination_directory,"tif",destination_files);
	set<string> destinations;

	for (unsigned int i = 0; i < destination_files.size(); i++)
		destinations.insert(destination_files[i]);
	
	for (unsigned int i = 0; i < source_files.size(); i++){
		set<string>::iterator p = destinations.find(source_files[i]);
		if (p == destinations.end())
			throw ns_ex("Could not find metadata source for ") << destination_files[i];
		cerr << i << "/" << destination_files.size() << "\n";
		ns_dir::create_directory_recursive(output_destination_directory);
		ns_transfer_annotations(annotation_source_directory + DIR_CHAR_STR + source_files[i],annotation_destination_directory + DIR_CHAR_STR + *p, output_destination_directory + DIR_CHAR_STR + *p);
	}
}


void ns_align_to_reference(const unsigned long region_info_id,ns_sql & sql){
	ns_sql_result res;
	sql << "SELECT " << ns_processing_step_db_column_name(ns_unprocessed) << ", " 
		<< ns_processing_step_db_column_name(ns_process_spatial) << ", "
		<< ns_processing_step_db_column_name(ns_process_lossy_stretch) << ", "
		<< ns_processing_step_db_column_name(ns_process_threshold)
		<< " FROM sample_region_images WHERE region_info_id = " << region_info_id
		<< " ORDER BY capture_time ASC";

	sql.get_rows(res);
	if (res.size() == 0)
		throw ns_ex("Region has no images!");
	
	ns_image_registration_profile reference_profile;
	{
		ns_image_standard reference_image;
		ns_image_server_image im;
		im.id = atol(res[0][0].c_str());
		try{
			image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(reference_image,1024);
		}
		catch(ns_ex & ex){
			throw ns_ex("Could not load reference image: ") << ex.text();
		}
		ns_image_registration<127,ns_8_bit>::generate_profiles(reference_image,reference_profile);
	}
	
	ns_image_standard current_image;
	ofstream alignment_out("c:\\alignment_test\\alignments.csv");
	if (alignment_out.fail())
		throw ns_ex("Could not open alignment output file");
	for (unsigned int i = 255; i < res.size(); i++){
		try{
			ns_image_server_image im;
			im.id = atol(res[i][0].c_str());
			image_server.image_storage.request_from_storage(im,&sql).input_stream().pump(current_image,1024);
			ns_image_registration_profile current_image_profile;
			ns_image_registration<127,ns_8_bit>::generate_profiles(current_image,current_image_profile);
			ns_vector_2i offset(ns_image_registration<127,ns_8_bit>::register_profiles(reference_profile,current_image_profile));
			cerr << "Offset: " << offset << "\n";
			alignment_out << i << "," << im.filename << "," << offset.x << "," << offset.y << "\n";
			alignment_out.flush();
			ns_image_standard registered_current;
			registered_current.init(current_image.properties());
			ns_vector_2i source_offset(offset*-1),
				dest_offset(0,0);
			if (offset.y < 0){
				source_offset.y = 0;
				dest_offset.y = offset.y;
			}
			else{
				for (unsigned int y = 0; y < offset.y; y++)
					for (unsigned int x = 0; x < current_image.properties().width; x++)
						registered_current[y][x] = 0;
			}
			if (offset.x < 0){
				source_offset.x = 0;
				dest_offset.x = offset.x;
			}
			for (unsigned int y = abs(offset.y); y < current_image.properties().height; y++){
				for (long x = 0; x < dest_offset.x; x++)
					registered_current[y+dest_offset.y][x] = 0;
				for (long x = abs(offset.x); x < (long)current_image.properties().width; x++)
					registered_current[y+dest_offset.y][x+dest_offset.x] = current_image[y+source_offset.y+offset.y][x+source_offset.y+offset.y];
				for (long x = current_image.properties().width+dest_offset.x; x < current_image.properties().width; x++)
					registered_current[y+dest_offset.y][x] = 0;
			}
			for (unsigned int y = current_image.properties().height+offset.y; y < current_image.properties().height; y++)
					for (unsigned int x = 0; x < current_image.properties().width; x++)
						registered_current[y][x] = 0;

			string filename("c:\\alignment_test\\");
			filename+=im.filename;
			ns_save_image(filename,registered_current);
		}
		catch(ns_ex & ex){
			cerr << ex.text() << "\n";
		}
	}
}

void ns_update_sample_info(ns_sql & sql){
	sql << "SELECT id, device_name, experiment_id FROM capture_samples WHERE device_capture_period_in_seconds = 0 || number_of_consecutive_captures_per_sample > 10";
	ns_sql_result res;
	sql.get_rows(res);	
	ns_sql_result res2;
	for (unsigned int i = 0; i < res.size(); i++){	
		sql << "SELECT count(*) FROM capture_samples WHERE device_name = '" << res[i][1] << "' AND experiment_id = " << res[i][2];
		sql.get_rows(res2);
		const unsigned long number_of_samples(atol(res2[0][0].c_str()));
	
		sql << "SELECT scheduled_time FROM capture_schedule WHERE sample_id = " << res[i][0] << " ORDER BY scheduled_time ASC LIMIT 20";
		sql.get_rows(res2);
		unsigned long short_i(10000000);
		unsigned long long_i(0);
		for (unsigned int j = 1; j < res2.size(); j++){
			unsigned long d(atol(res2[j][0].c_str())-atol(res2[j-1][0].c_str()));
			if (d < short_i)
				short_i = d;
			if (d > long_i)
				long_i = d;
		}
		cerr << "short: " << short_i/60 << ",long: " << long_i/60;
		double r;
		unsigned long p;
		if (long_i == short_i){
			p = short_i/number_of_samples;
			r = 1;
		}
		else{
			p = short_i;
			r = (long_i-short_i)/(double)(short_i*(number_of_samples-1));
			if (floor(r) != r){
				cerr << "Weird intervals!\n";
				continue;
			}
		}

		cerr << ", repeats: " << r << "\n";
		sql << "UPDATE capture_samples SET device_capture_period_in_seconds = "
			<< p
			<<", number_of_consecutive_captures_per_sample = "
			<< r 
			<< " WHERE id = " << res[i][0];
		sql.send_query();

	}

};

void refresh_main_window(){
	current_window->redraw();
}
#include <FL/Fl_File_Icon.H>
// MAIN
int main() {
	ns_worm_browser_output_debug(__LINE__,__FILE__,"Launching worm browser");
	init_time = GetTime();
	Fl::lock();
	Fl_File_Icon::load_system_icons();
	Fl::scheme("none");
	
	ns_main_thread_id = GetCurrentThread();
	try{
	
		ns_multiprocess_control_options mp_options;
		mp_options.total_number_of_processes = 1;
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Loading constants");
		image_server.load_constants(ns_image_server::ns_worm_terminal_type,mp_options); 
		if (image_server.verbose_debug_output())
			output_debug_messages = true;
		

		worm_learner.maximum_window_size = image_server.max_terminal_window_size;
		worm_learner.death_time_annotater.set_resize_factor(image_server.terminal_hand_annotation_resize_factor);
		
		//ns_update_sample_info(sql());
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Checking for new release");
		if (image_server.new_software_release_available()){
			MessageBox(
				0,
				"This version of the Worm Browser is outdated.  Please update it.",
				"Worm Browser",
				MB_TASKMODAL | MB_ICONEXCLAMATION| MB_DEFBUTTON1 | MB_TOPMOST);
		}
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Loading detection models");
		image_server.load_all_worm_detection_models(worm_learner.model_specifications);
		{
		//	image_server.set_sql_database("image_server_archive");
			
			ns_worm_browser_output_debug(__LINE__,__FILE__,"Getting flags from db");
			ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
			ns_death_time_annotation_flag::get_flags_from_db(sql());

	
			ns_worm_browser_output_debug(__LINE__,__FILE__,"Refreshing partition cache");
			image_server.image_storage.refresh_experiment_partition_cache(&sql());
			
			ns_worm_browser_output_debug(__LINE__,__FILE__,"Getting flags from db again");
			ns_death_time_annotation_flag::get_flags_from_db(sql());

			
			ns_worm_browser_output_debug(__LINE__,__FILE__,"Loading experient names");
			worm_learner.data_selector.load_experiment_names(sql());

			worm_learner.load_databases(sql());
			try{
					
				ns_worm_browser_output_debug(__LINE__,__FILE__,"Setting current experiment");
				worm_learner.data_selector.set_current_experiment(-1,sql());
			}
			catch(ns_ex & ex){
				ns_worm_browser_output_debug(__LINE__,__FILE__,std::string("Error setting experiment: ") + ex.text());
		
				cerr << ex.text() << "\n";
			}
	//		worm_learner.data_selector.select_region("ben_d::1");
	//		worm_learner.generate_training_set_from_by_hand_annotation();
			sql.release();
		}
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Loading for models");
		if (worm_learner.model_specifications.size() == 0){
			cerr << "No model specifications were found in the default model directory.  Worm detection will probably not work.";
			worm_learner.set_svm_model_specification(worm_learner.default_model);
		}
		else {
			
			ns_worm_browser_output_debug(__LINE__,__FILE__,"Setting svm model specification");
			worm_learner.set_svm_model_specification(*worm_learner.model_specifications[0]);
		}
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Displaying splash image");
		ns_image_standard im;
		worm_learner.display_splash_image();
		
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Creating new window");
		current_window = new ns_worm_terminal_main_window(worm_learner.current_image_properties().width,worm_learner.current_image_properties().height, "Worm Browser");
		
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Loading occupied animation");
		std::string tmp_filename = "occupied_animation.tif";
		ns_load_image_from_resource(IDR_BIN2,tmp_filename);
		ns_load_image(tmp_filename,worm_learner.animation);
		ns_dir::delete_file(tmp_filename);
		//win.draw_animation = true;
		
		//worm_learner.compare_machine_and_by_hand_annotations();

		//current_window->resizable(win);
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Showing window");
		current_window->show();
		worm_window = new ns_worm_terminal_worm_window(100,100,"Inspect Worm");
		worm_window->hide();

		worm_learner.draw();
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Setting current experiment");
		//ns_acquire_for_scope<ns_sql> sql(image_server.new_sql_connection(__FILE__,__LINE__));
		//worm_learner.data_selector.set_current_experiment(341,sql());
		//worm_learner.data_selector.set_current_experiment(-1,sql());
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Setting default sample and region");
		worm_learner.data_selector.select_default_sample_and_region();
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Updating information bar");
		current_window->update_information_bar();
	
		//worm_learner.data_selector.select_region("hare_a::0");
	//	ns_start_death_time_annotation(ns_worm_learner::ns_annotate_storyboard_experiment);
		
		ns_worm_browser_output_debug(__LINE__,__FILE__,"Entering idle loop");
		return(Fl::run());
	/*	while( Fl::wait() > 0)
			SleepEx(0,TRUE);
			*/
	}
	catch(ns_ex & ex){
		MessageBox(
		0,
		ex.text().c_str(),
		"Worm Browser",
		MB_TASKMODAL | MB_ICONEXCLAMATION| MB_DEFBUTTON1 | MB_TOPMOST);
	/*
	  	cerr << ex.text() << "\n";
		for (unsigned int i = 0; i < 5; i ++){
			cerr << (5-i) << "...";
			ns_thread::sleep(2);
		}*/

	}
	catch(std::exception & exception){
		ns_ex ex(exception);
		cerr << ex.text() << "\n";
		for (unsigned int i = 0; i < 5; i ++){
			cerr << (5-i) << "...";
			ns_thread::sleep(2);
		}
	}
	
}


//ns_sql * worm_window_sql = 0;
struct ns_asynch_worm_launcher{

	unsigned long region_id;
	ns_stationary_path_id worm;
	const ns_experiment_storyboard * storyboard;
	unsigned long current_time;
	static ns_thread_return_type run_asynch(void * l){
		ns_asynch_worm_launcher * launcher(static_cast<ns_asynch_worm_launcher *>(l));
		launcher->launch();
		delete launcher;
		return true;
	}
	void launch(){
		try{
			//if (worm_window_sql == 0)
			ns_sql *	worm_window_sql = image_server.new_sql_connection(__FILE__,__LINE__);
			worm_learner.death_time_solo_annotater.load_worm(region_id,worm,current_time,storyboard,&worm_learner,*worm_window_sql);
			worm_learner.death_time_solo_annotater.display_current_frame();
			show_worm_window = true;
			/*Fl::awake();
			Fl::lock();
			worm_window->show();
			worm_window->resize(0,0,worm_learner.worm_window.ideal_window_size.x,worm_learner.worm_window.ideal_window_size.y);
			worm_window->redraw();
			Fl::unlock();*/
		}
		catch(ns_ex & ex){
			cerr << "Error loading worm info:" << ex.text();
			worm_learner.death_time_solo_annotater.close_worm();
			show_worm_window = false;
			ns_set_menu_bar_activity(true);
		}
	}
};


void ns_launch_worm_window_for_worm(const unsigned long region_id, const ns_stationary_path_id & worm, const unsigned long current_time){
	
	ns_set_menu_bar_activity(false);
	ns_asynch_worm_launcher * launcher(new ns_asynch_worm_launcher);
	launcher->region_id = region_id;
	launcher->worm = worm;
	launcher->current_time = current_time;
	launcher->storyboard = &worm_learner.storyboard_annotater.get_storyboard();
	ns_thread worm_launcher(ns_asynch_worm_launcher::run_asynch,launcher);
	worm_launcher.detach();
}
 
void ns_set_main_window_annotation_controls_activity(const bool active){
	if (active)
		current_window->annotation_group->activate();
	else current_window->annotation_group->deactivate();
}

/*ns_thread_return_type asynch_redisplay(void *){
	//cerr << "x";
	
	worm_learner.redraw_screen();
	return 0;
}*/

bool ns_set_animation_state(bool new_state){
	bool old_state(current_window->draw_animation);
	current_window->draw_animation = new_state;
	return old_state;
}
typedef enum{ns_none,ns_activate,ns_deactivate} ns_menu_bar_request;

ns_menu_bar_request set_menu_bar_request;
void ns_set_menu_bar_activity_internal(bool activate){
	if (activate)
		current_window->draw_animation = false;
//	Fl::awake();
	Fl::lock();
	if (activate){
		for (unsigned int i = 0; i < current_window->main_menu->size(); i++){
			current_window->main_menu->mode(i,current_window->main_menu->mode(i) &  ~FL_MENU_INACTIVE);
	//		current_window->draw_animation = false;
		}
		current_window->main_menu->activate();
		current_window->gl_window->activate();
		current_window->region_menu->activate();
		current_window->strain_menu->activate();
		current_window->annotation_group->activate();
		worm_window->activate();
		worm_window->gl_window->activate();
		worm_window->annotation_group->activate();
		current_window->main_menu->redraw();
		//worm_window->redraw();
	}
	else {
		
		for (unsigned int i = 0; i < current_window->main_menu->size(); i++){
			current_window->main_menu->mode(i,current_window->main_menu->mode(i) |  FL_MENU_INACTIVE);
			current_window->draw_animation = true;
		}
		current_window->main_menu->deactivate();
		current_window->gl_window->deactivate();
		current_window->region_menu->deactivate();
		current_window->strain_menu->deactivate();
		worm_window->deactivate();
		worm_window->gl_window->deactivate();
		worm_window->annotation_group->deactivate();
		current_window->annotation_group->deactivate();
	//	worm_window->redraw();
		current_window->main_menu->redraw();
	}
	Fl::unlock();
}
void ns_handle_menu_bar_activity_request(){
	menu_bar_processing_lock.wait_to_acquire(__FILE__,__LINE__);
	if (set_menu_bar_request == ns_none){
		menu_bar_processing_lock.release();
		return;
	}
	ns_set_menu_bar_activity_internal(set_menu_bar_request==ns_activate);
	set_menu_bar_request = ns_none;
	menu_bar_processing_lock.release();
}
void ns_set_menu_bar_activity(bool activate){
	menu_bar_processing_lock.wait_to_acquire(__FILE__,__LINE__);
	set_menu_bar_request = activate?ns_activate:ns_deactivate;
	menu_bar_processing_lock.release();
}

void ask_if_schedule_should_be_submitted_to_db(bool & write_to_disk, bool & write_to_db){
	ns_choice_dialog c;
	c.title = "What do you want to do with this experiment schedule?";
	c.option_1 = "Generate a Summary File";
	c.option_2 = "Run it!";
	c.option_3 = "Cancel";
	ns_run_in_main_thread<ns_choice_dialog> b(&c);
	write_to_disk = c.result == 1;
	write_to_db = c.result == 2;
	return;

	//old behavior
	write_to_disk = false;
	write_to_db = false;

	int ret;
	ret = MessageBox(
	0,
	"Do you want to output a summary of this schedule to disk?",
	"Schedule Submission",
	MB_TASKMODAL | MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST);
	if (ret == IDCANCEL)
		return;
	if (ret == IDYES){
		write_to_disk = true;
		return;
	}
	ret = MessageBox(
	0,
	"Do you want to submit this schedule to run on the database?",
	"Schedule Submission",
	MB_TASKMODAL | MB_YESNOCANCEL | MB_ICONQUESTION | MB_DEFBUTTON1 | MB_TOPMOST);
	
	if (ret == IDCANCEL || ret == IDNO)
		return;

	ret = MessageBox(
	0,
	"Do you really want to submit this schedule to run on the database?",
	"Schedule Submission",
	MB_TASKMODAL | MB_YESNO | MB_ICONEXCLAMATION| MB_DEFBUTTON1 | MB_TOPMOST);
	if (ret == IDYES){
		write_to_db = true;
		return;
	}
	return;
}


 void ns_experiment_storyboard_annotater_timepoint::load_image(const unsigned long bottom_height,ns_annotater_image_buffer_entry & im,ns_sql & sql,ns_image_standard & temp_buffer,const unsigned long resize_factor_){
	this->blacked_out_non_subject_animals = false;
	if (division != 0){
		for (unsigned int i = 0; i < this->division->events.size(); i++){
			this->division->events[i].drawn_worm_bottom_right_overlay = false;
			this->division->events[i].drawn_worm_top_right_overlay = false;
		}
	}
	experiment_annotater->populate_division_images_from_composit(division_id,sql);
	ns_image_properties prop(division_image.properties());
	prop.components = 3;
	im.im->prepare_to_recieve_image(prop);
	if(division_image.properties().components == 1){
	for (unsigned int y = 0; y < prop.height; y++)
		for (unsigned int x = 0; x < prop.width; x++){
			(*im.im)[y][3*x+0] = division_image[y][x];
			(*im.im)[y][3*x+1] = division_image[y][x];
			(*im.im)[y][3*x+2] = division_image[y][x];
		}
	}
	else{
		for (unsigned int y = 0; y < prop.height; y++)
			for (unsigned int x = 0; x < 3*prop.width; x++){
				(*im.im)[y][x] = division_image[y][x];
			}
	}
	//division_image.pump(*im.im,1024);
	//ns_annotater_timepoint::load_image(im,sql,temp_buffer,resize_factor_);
		
//		vis_info.from_xml(temp_buffer.properties().description);
//	check_metadata();
	im.loaded = true;
}



 ns_vector_2i main_image_window_size_difference(){
	 return ns_worm_terminal_main_window::image_window_size_difference ();
 }
 
 ns_vector_2i worm_image_window_size_difference(){
	 return ns_worm_terminal_worm_window::image_window_size_difference ();
 }


ns_death_time_posture_solo_annotater_data_cache ns_death_time_solo_posture_annotater::data_cache;