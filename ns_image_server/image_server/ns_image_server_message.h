#ifndef NS_IMAGE_SERVER_MESSAGE
#define NS_IMAGE_SERVER_MESSAGE

#include "ns_ex.h"
#include "ns_socket.h"
#include "ns_image_server_images.h"

#include <vector>
#include <map>
#include <string>
#pragma warning(disable: 4355) //our use of this in constructors is valid, so we suppress the error message.

typedef enum { ns_ts_standard, ns_ts_error, ns_ts_sql_error, ns_ts_minor_event, ns_ts_debug} ns_text_stream_type;

//holds global information about the current image server host
//Also used to coordinate threads
class ns_image_server_event : public ns_text_stream<ns_image_server_event, ns_text_stream_type>{
public:
	ns_image_server_event(const bool _log=true):time(0),log(_log),ns_text_stream<ns_image_server_event, ns_text_stream_type>(*this, ns_ts_standard){clear_data();set_time();}
	ns_image_server_event(const char * str, const bool _log=true):time(0),log(_log),ns_text_stream<ns_image_server_event, ns_text_stream_type>(*this, ns_ts_standard){clear_data();append(str);set_time();}
	ns_image_server_event(const std::string & str, const bool _log=true):time(0),log(_log),ns_text_stream<ns_image_server_event, ns_text_stream_type>(*this, ns_ts_standard){clear_data();append(str);set_time();}

	bool log;  //whether the event should be logged, or just displayed.
	void print(std::ostream & out) const{
		std::string time_str = ns_format_time_string(time);
		out << time_str;
		out << std::string(": ");
		out << value;
		out << std::string("\n");
	}
	void clear_data(){
		processing_job_operation = (ns_processing_task)0;
		time = 0;
		parent_event_id = 0;
		subject_experiment_id = 0;
		subject_sample_id = 0;
		subject_captured_image_id = 0;
		subject_region_info_id = 0;
		subject_region_image_id = 0;
		subject_image_id = 0;
		subject_properties = ns_image_properties(0,0,0);
		processing_duration = 0;
	}
	//server events can represent processing tasks that are being performed.
	void specify_processing_job_operation(const ns_processing_task &operation){processing_job_operation = operation;}
	//server events can have "sub-events" that correspond to substeps of the parent job.
	void specify_parent_server_event(const unsigned long & event_id){parent_event_id = event_id;}
	void specifiy_event_subject(const ns_image_server_image & image){
		subject_image_id = image.id;
	}
	void specifiy_event_subject(const ns_image_server_captured_image & captured_image){
		subject_experiment_id = captured_image.experiment_id;
		subject_sample_id = captured_image.sample_id;
		subject_captured_image_id = captured_image.captured_images_id;
	}
	void specifiy_event_subject(const ns_image_server_captured_image_region & region_image){
		subject_experiment_id = region_image.experiment_id;
		subject_sample_id = region_image.sample_id;
		subject_region_info_id = region_image.region_info_id;
		subject_region_image_id = region_image.region_images_id;
	}
	void specify_event_subject_dimentions(const ns_image_properties & properties){
		subject_properties = properties;
	}
	void specify_processing_duration(const unsigned long duration){
		processing_duration = duration;
	}
	void set_time(const unsigned long t){
		time = t;
	}
	unsigned long event_time() const{return time;}
private:
	void set_time(){
		time = ns_current_time();
	}
	unsigned long time;
	ns_processing_task processing_job_operation;
	ns_64_bit parent_event_id;
	ns_64_bit subject_experiment_id,
				  subject_sample_id,
				  subject_captured_image_id,
				  subject_region_info_id,
				  subject_region_image_id,
				  subject_image_id,
				  processing_duration;
	ns_image_properties subject_properties;

	friend class  ns_image_server;
};
#pragma warning(default: 4355)

typedef enum {NS_NULL, NS_TIMER, NS_IMAGE_REQUEST, NS_IMAGE_SEND,NS_STATUS_REQUEST, NS_QUIT, 
			  NS_DELETE_IMAGE, NS_OK, NS_FAIL, NS_CHECK_FOR_WORK, NS_HOTPLUG_NEW_DEVICES, NS_RESET_DEVICES,
			  NS_RELOAD_MODELS, NS_LOCAL_CHECK_FOR_WORK,NS_WRAP_M4V, NS_RELAUNCH_SERVER,
			  NS_CLEAR_DB_BUF_CLEAN,NS_CLEAR_DB_BUF_DIRTY,NS_SIMULATE_CENTRL_DB_CONNECTION_ERROR} ns_message_request;

#define ns_image_server_message_length 18
#define NS_VAR_DELIMITER_CHAR ';'
#define NS_VAR_DELIMITER_STR ";"

std::string ns_message_request_to_string(const ns_message_request & req);

class ns_vars{
public:

	std::string operator()(const std::string & key){
		return get(key);
	}
	std::string get(const std::string & key){
		std::map<std::string,std::string>::iterator p = vars.find(key);
		if (p == vars.end())
			throw ns_ex("Could not retrieve variable ") << key << " from variable list.";
		return p->second;
	}

	unsigned int get_int(const std::string & key){
		std::string str = get(key);
		return atoi(str.c_str());
	}

	void set(const std::string & key, const std::string & val){
		vars[key] = val;
	}

	std::string to_string(){
		std::string str;
		for (std::map<std::string,std::string>::iterator p = vars.begin(); p != vars.end(); p++){
			add_formatted(str,p->first);
			str += "NS_VAR_DELIMITER";
			add_formatted(str,p->second);
			str += "NS_VAR_DELIMITER";
		}
		return str;
	}

	//run a state machine to parse std::string into a list of variables.
	void from_string(const std::string & str){
		std::string key, value;
		bool state = 0;
		bool escape = false;
		char cur_char;
		for (unsigned int i = 0; i < str.size(); i++){
			if (!escape){
				//if you've found an escape character, move on to see what character was escaped
				if (str[i] == '\\'){
					escape = true;
					continue;
				}
			}
			else{
				if (str[i] == NS_VAR_DELIMITER_CHAR)
					cur_char = NS_VAR_DELIMITER_CHAR;
				if (str[i] == '\\')
					cur_char = '\\';
				//all other escape std::strings vanish
			}

			//reading a key
			if (state == 0){
				if (cur_char == NS_VAR_DELIMITER_CHAR)
					state = 1;
				else key += cur_char;
			}
			//reading a value
			else{
				if (cur_char == NS_VAR_DELIMITER_CHAR){
					vars[key]=value;
					key.clear();
					value.clear();
					state=0;
				}
				else value += cur_char;
			}
		}
	}
private:
	std::map<std::string,std::string> vars;
	//comments out
	void add_formatted(std::string & reciever, const std::string & toadd){
		for (unsigned int i = 0; i<toadd.size(); i++){
			if (toadd[i]==NS_VAR_DELIMITER_CHAR){
				reciever += "\\";
				reciever += NS_VAR_DELIMITER_CHAR;
			}
			else if (toadd[i]=='\\')
				reciever += "\\\\";
			else reciever += toadd[i];
		}
	}
};


class ns_image_server_message{

public:
	ns_image_server_message(ns_socket_connection & connection);

	//parse incomming connections
	void get();
	std::string data();
	unsigned int data_length() const;
	ns_message_request request() const;
	ns_socket_connection & connection(){ return _connection;}

	//to send outgoing connections
	//send entire message in one go
	void send_message(const ns_message_request & req, const std::string & data);
	void send_message(const ns_message_request & req, const long & data);
	void send_message(const ns_message_request & req, const unsigned long & data);
	//send data in bits
	void send_message_header(const ns_message_request & req, const unsigned int data_size);
	void send_message_data(char * buf, unsigned int data_size);
	void send_message_data(const unsigned int i);



private:
	ns_socket_connection _connection;
	ns_message_request _request;
	unsigned int _data_length;
	std::string _data;
	bool message_read_from_socket,
		 data_read_from_socket;
};

#endif
