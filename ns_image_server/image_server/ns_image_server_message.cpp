#include "ns_image_server_message.h"

using namespace std;

ns_message_request  ns_message_request_from_string(const string & str){
	if (str == "NS_NULL...........") return NS_NULL;
	if (str == "NS_TIMER..........") return NS_TIMER;	
	if (str == "NS_IMAGE_REQUEST..") return NS_IMAGE_REQUEST;	
	if (str == "NS_IMAGE_SEND.....") return NS_IMAGE_SEND;
	if (str == "NS_STATUS_REQUEST.") return NS_STATUS_REQUEST;
	if (str == "NS_QUIT...........") return NS_QUIT;
	if (str == "NS_DELETE_IMAGE...") return NS_DELETE_IMAGE;
	if (str == "NS_OK.............") return NS_OK;
	if (str == "NS_FAIL...........") return NS_FAIL;
	if (str == "NS_CHECK_FOR_WORK.") return NS_CHECK_FOR_WORK;
	if (str == "NS_HOTPLUG........") return NS_HOTPLUG_NEW_DEVICES;
	if (str == "NS_RESET_DEVICES..") return NS_RESET_DEVICES;
	if (str == "NS_RELOAD_MODELS..") return NS_RELOAD_MODELS;
	if (str == "NS_LCHK_FOR_WORK..") return NS_LOCAL_CHECK_FOR_WORK;
	if (str == "NS_WRAP_M4V.......") return NS_WRAP_M4V;
	if (str == "NS_RELAUNCH_SERVER") return NS_RELAUNCH_SERVER;
	if (str == "NS_CLEAR_BUF_CLEAN") return NS_CLEAR_DB_BUF_CLEAN;
	if (str == "NS_CLEAR_BUF_DIRTY") return NS_CLEAR_DB_BUF_DIRTY;
	if (str == "NS_SIM_DB_ERR.....") return NS_SIMULATE_CENTRL_DB_CONNECTION_ERROR;
	if (str == "NS_IM_BUF_INFO....") return NS_OUTPUT_IMAGE_BUFFER_INFO;
	if (str == "NS_NO_CENTRAL_DB..") return NS_STOP_CHECKING_CENTRAL_DB;
	if (str == "NS_USE_CENTRAL_DB.") return NS_START_CHECKING_CENTRAL_DB;
	if (str == "NS_OUTPUT_SQL_LOCK") return NS_OUTPUT_SQL_LOCK_INFORMATION;
	throw ns_ex("Unknown message request: ") << str;
}

string  ns_message_request_to_string(const ns_message_request & req){
	switch(req){
		case NS_TIMER:								return "NS_TIMER..........";
		case NS_IMAGE_REQUEST:						return "NS_IMAGE_REQUEST..";
		case NS_IMAGE_SEND:							return "NS_IMAGE_SEND.....";
		case NS_STATUS_REQUEST:						return "NS_STATUS_REQUEST.";
		case NS_QUIT:								return "NS_QUIT..........."; 
		case NS_DELETE_IMAGE:						return "NS_DELETE_IMAGE...";
		case NS_OK:									return "NS_OK.............";
		case NS_FAIL:								return "NS_FAIL...........";
		case NS_CHECK_FOR_WORK:						return "NS_CHECK_FOR_WORK.";
		case NS_HOTPLUG_NEW_DEVICES:				return "NS_HOTPLUG........";
		case NS_RESET_DEVICES:						return "NS_RESET_DEVICES..";
		case NS_RELOAD_MODELS:						return "NS_RELOAD_MODELS..";
		case NS_LOCAL_CHECK_FOR_WORK:				return "NS_LCHK_FOR_WORK..";
		case NS_WRAP_M4V:							return "NS_WRAP_M4V.......";
		case NS_RELAUNCH_SERVER:					return "NS_RELAUNCH_SERVER";
		case NS_CLEAR_DB_BUF_CLEAN:					return "NS_CLEAR_BUF_CLEAN";
		case NS_CLEAR_DB_BUF_DIRTY:					return "NS_CLEAR_BUF_DIRTY";
		case NS_SIMULATE_CENTRL_DB_CONNECTION_ERROR:return "NS_SIM_DB_ERR.....";
		case NS_OUTPUT_IMAGE_BUFFER_INFO:			return "NS_IM_BUF_INFO....";
		case NS_STOP_CHECKING_CENTRAL_DB:			return "NS_NO_CENTRAL_DB..";
		case NS_START_CHECKING_CENTRAL_DB:			return "NS_USE_CENTRAL_DB.";
		case NS_OUTPUT_SQL_LOCK_INFORMATION:		return "NS_OUTPUT_SQL_LOCK";
		case NS_NULL:			
		default:						return "NS_NULL...........";
	}
}

#include <iostream>
ns_image_server_message::ns_image_server_message(ns_socket_connection &connection):_connection(connection),
	message_read_from_socket(false),data_read_from_socket(false){}

void ns_image_server_message::get(){
	if (message_read_from_socket)
		throw ns_ex("ns_image_server_message::get()::Repeated call to get()");
	string message_header = _connection.read(18);
	_request = ns_message_request_from_string(message_header);
	_data_length = _connection.read_uint();
	message_read_from_socket = true;
	//std::cerr << "data length = " << _data_length << "\n";
}
string ns_image_server_message::data(){
	if (data_read_from_socket)
		throw ns_ex("ns_image_server_message::data()::Repeated call to data()");
	if (_data_length !=0 && _data.size() == 0)
		_data = _connection.read(_data_length);
	data_read_from_socket = true;
	return _data;
}
unsigned int ns_image_server_message::data_length() const{
	return _data_length;
}
ns_message_request ns_image_server_message::request() const{
	return _request;
}

void ns_image_server_message::send_message(const ns_message_request & req, const string & data){
	_connection.write( ns_message_request_to_string(req));
	_connection.write(static_cast<unsigned long>(data.length()));
	_connection.write(data);
}
void ns_image_server_message::send_message(const ns_message_request & req, const long & data){
	string a = ns_to_string(data);
	send_message(req, a);
}
void ns_image_server_message::send_message(const ns_message_request & req, const unsigned long & data){
	string a = ns_to_string(data);
	send_message(req, a);
}

	//send data in bits
void ns_image_server_message::send_message_header(const ns_message_request & req, const unsigned int data_size){
	_connection.write( ns_message_request_to_string(req));
	_connection.write(data_size);
}

void ns_image_server_message::send_message_data(char * buf, unsigned int data_size){
	_connection.write(buf,data_size);
}

void ns_image_server_message::send_message_data(const unsigned int i){
	string a = ns_to_string(i);
	_connection.write(a);
}
