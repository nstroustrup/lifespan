#ifndef NS_FL_MODAL_DIALOGS
#define NS_FL_MODAL_DIALOGS

#include "ns_worm_browser.h"
#include "FL/Fl_Output.H"
#include "FL/Fl_Button.H"
#include "FL/Fl_Pack.H"
#include "FL/Fl_Message.H"

class ns_input_dialog{
public:
	string title,default_value, result;
	bool canceled;
	void act(){
		Fl_Widget* f(fl_message_icon());
		f->labelcolor(0x00000000);
		const char * a = fl_input(title.c_str(),default_value.c_str());
		canceled = a == 0;
		if (!canceled)
			result = a;
		else result.resize(0);
	}

};
#include <FL/Fl_Native_File_Chooser.H>

struct ns_file_chooser_file_type{
	ns_file_chooser_file_type(){}
	ns_file_chooser_file_type(const std::string & n, const std::string & e):name(n),extension(e){}
	std::string extension,
		   name;
};

class ns_file_chooser{
	public:
	ns_file_chooser():dialog_type(Fl_Native_File_Chooser::BROWSE_FILE){}
	
	string title,default_directory,default_filename,result;
	Fl_Native_File_Chooser::Type dialog_type;
	std::vector<ns_file_chooser_file_type> filters;
	
	bool chosen;
	string error;
	void act(){
		
		 Fl_Native_File_Chooser fnfc;
		 fnfc.title(title.c_str());
		 fnfc.type(dialog_type);
		 char * filter_string;
		 bool delete_filter_string(set_filter_string(filter_string));
		 fnfc.filter(filter_string);
		 fnfc.preset_file(default_filename.c_str());

		 fnfc.directory(default_directory.c_str());           // default directory to use
		 chosen = false;
		 // Show native chooser
		 switch ( fnfc.show() ) {
		   case -1: error = fnfc.errmsg();    break;  // ERROR
		   case  1: break;                      break;  // CANCEL
		   default: result =  fnfc.filename(); chosen = true; break;  // FILE CHOSEN
		 }
		 if (delete_filter_string)
			 delete[] filter_string;
	}

	bool set_filter_string(char *& filter_text){
		bool should_delete(false);
		//char * filter_text;
		unsigned long size = 0;
		if (filters.size() == 0){
			filter_text = "All\0*.*\0";
			size = 8;
		}
		else{
			for (unsigned int i = 0; i < filters.size(); i++){
				size += static_cast<unsigned long>(filters[i].name.size());
				size += static_cast<unsigned long>(filters[i].extension.size());
				size+= 4;
			}
			filter_text = new char[size+1];
			should_delete = true;
			unsigned int cur_pos=0;
			for (unsigned int i = 0; i < filters.size(); i++){
				for (unsigned int j = 0; j < filters[i].name.size(); j++)
					filter_text[cur_pos+j] = filters[i].name[j];

				cur_pos+=static_cast<unsigned long>(filters[i].name.size());
				filter_text[cur_pos] = '\0';
				filter_text[cur_pos+1] = '*';
				filter_text[cur_pos+2] = '.';
				cur_pos+=3;	

				for (unsigned int j = 0; j < filters[i].extension.size(); j++)
					filter_text[cur_pos+j] = filters[i].extension[j];
				cur_pos+=static_cast<unsigned long>(filters[i].extension.size());
				filter_text[cur_pos] = '\0';
				cur_pos++;
			}
			filter_text[cur_pos] = '\0';
		}
		return should_delete;
	}
};

class ns_choice_dialog{
public:
	string title, option_1,option_2,option_3;
	int result;
	void act(){
		Fl_Widget* f(fl_message_icon());
		f->labelcolor(0x00000000);
		result = 3-fl_choice(title.c_str(),option_3.c_str(),option_2.c_str(),option_1.c_str());
	}
};





#include <FL\Fl_Text_Display.H>
#include "FL\Fl_Button.h"
class ns_text_display_window;
class ns_close_button : public Fl_Button{
public:
	ns_text_display_window * main_window;
	ns_close_button(int x, int y, int w, int h, const char * l = 0):Fl_Button(x,y,w,h,l){}
};
class ns_text_display_window : public Fl_Window {
	bool have_focus;
public:
	Fl_Text_Display *text;
	bool * wait_for_it;
	ns_close_button *button;
	Fl_Text_Buffer buff;
	enum{button_width=100,button_height=35,buffer=10};
    ns_text_display_window(int W,int H,const char*L=0) : Fl_Window(50,50,W,H,L),text(0),button(0){
		text = new Fl_Text_Display(buffer,buffer,W-2*buffer,H-button_height-3*buffer),
		button = new ns_close_button(W-button_width-buffer,H-button_height-buffer,button_width,button_height,"&OK");
		button->callback(on_click);
		button->main_window = this;
    }
	~ns_text_display_window(){
		ns_safe_delete(text);
		ns_safe_delete(button);
	}
	void set_text(const std::vector<std::string> & t){

		for (unsigned int i = 0; i < t.size(); i++){
			buff.append(t[i].c_str());
			buff.append("\n");
			text->buffer(buff);
		}
	}
	void set_text(const std::string & t){
		buff.append(t.c_str());
		text->buffer(buff);
	}

	void resize(int x, int y, int W, int H){
		text->resize(buffer,buffer,W-2*buffer,H-button_height-2*buffer);
		button->resize(W-button_width-buffer,H-button_height-buffer,button_width,button_height);
	}
	static void on_click(Fl_Widget * w){
		ns_close_button * b = (ns_close_button *)(w);
		bool * wait_for_it = b->main_window->wait_for_it;
		b->main_window->hide();
		*wait_for_it = false;
	}
};
class ns_text_dialog{
public:
	std::string text,title;
	std::vector<std::string> grid_text;
	bool wait_for_it;
	void act(){
		ns_text_display_window * win = new ns_text_display_window(600,400,title.c_str());
		win->wait_for_it = &wait_for_it;
		if(text.size() != 0)
		win->set_text(text);
		if (grid_text.size() != 0)
			win->set_text(grid_text);
		win->set_modal();
		win->show();
	}

};

template<class T>
class ns_run_in_main_thread{
public:
	ns_run_in_main_thread(T * t){
		data = t;
		wait_for_it = true;
		Fl::awake(ns_run_in_main_thread<typename T>::main_thread_call,(void *)(this));
		while(wait_for_it)ns_thread::sleep(1);
	}
	static void main_thread_call(void * t){
		ns_run_in_main_thread<typename T> * tt = (ns_run_in_main_thread<typename T> *)(t);
		tt->data->act();
		tt->wait_for_it = false;
	}
private:
	T * data;
	bool wait_for_it;
};

template<class T>
class ns_run_in_main_thread_custom_wait{
public:
	ns_run_in_main_thread_custom_wait(T * t){
		data = t;
		t->wait_for_it = true;
		Fl::awake(ns_run_in_main_thread<typename T>::main_thread_call,(void *)(this));
		while(t->wait_for_it)
			ns_thread::sleep(1);
		cout << "WHA";
	}
	static void main_thread_call(void * t){
		ns_run_in_main_thread<typename T> * tt = (ns_run_in_main_thread<typename T> *)(t);
		tt->data->act();
	}
private:
	T * data;
};


#endif