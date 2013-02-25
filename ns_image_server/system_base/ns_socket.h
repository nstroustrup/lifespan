#ifndef NS_SOCKET
#define NS_SOCKET

/*******************************************
ns_socket: a multi-platform socket library
This library provides socket support
for Windows, Linux, and OSX machines.
(C) Nicholas Stroustrup 2006
*******************************************/

#include <string>
#include <vector>
#include "ns_thread.h"

#ifdef _WIN32 
	#include <winsock2.h>
	typedef SOCKET ns_socket_handle;
#else
	typedef int ns_socket_handle;
#endif


class ns_socket_connection{
	public:
	ns_socket_connection(const ns_socket_handle & h):handle(h),is_open(true){}
	ns_socket_connection():handle(0),is_open(false){}
	~ns_socket_connection();

	unsigned int read(char * buf, const unsigned long size);
	std::string read(const unsigned long size);
	unsigned long read_uint();
	long read_int();
	
	ns_64_bit read_64bit_uint();
	ns_s64_bit read_64bit_int();

	void flush();

	void write(const char * buf, const unsigned long size);
	void write(const std::string & data) {this->write(data.c_str(),static_cast<unsigned long>(data.size()));}
	void write(const unsigned long & i);
	void write(const long & i);
	void write(const unsigned int & i);
	void write(const ns_64_bit & i);
	void write(const ns_s64_bit & i);
	void write(const int & i);
	void write(const unsigned char & i);
	void write(const char & i);

	void operator<<(const std::string & str);
	void operator<<(const char * str);
	void operator<<(const long & i);
	void operator<<(const unsigned long & i);
	void operator<<(const int & i);
	void operator<<(const unsigned int& i);
	void operator<<(const char & i);
	void operator<<(const unsigned char & i);

	void close();

	private:
	ns_socket_handle handle;
	bool is_open;
};

struct ns_interface_info{
  ns_interface_info(){}
  ns_interface_info(const std::string & i,const std::string & ip):interface_name(i),ip_address(ip){}
  std::string interface_name,
    		  ip_address;
};

class ns_socket{
	public:

	void listen(const unsigned int port, const unsigned int connection_buffer_size);
	void close_socket();
	ns_socket_connection accept();

	ns_socket_connection connect(const std::string & address, const unsigned int port);

	void static global_init();
	void static global_clean();
	bool static global_init_performed;

	const std::string get_local_ip_address(const std::string & interface_name="");

	private:
	void build_interface_list(std::vector<ns_interface_info> &);
	ns_socket_handle listen_socket;
	static ns_lock gethostname_lock;
};

#endif
