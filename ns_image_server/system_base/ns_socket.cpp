#include "ns_socket.h"
#include "ns_ex.h"
#include <string.h>

using namespace std;

/*******************************************
ns_socket: a multi-platform socket library
This library provides compatible socket support
for Windows, Linux, and OSX machines.
(C) Nicholas Stroustrup 2006
*******************************************/

#ifdef _WIN32 
	#pragma comment (lib, "ws2_32.lib")
#else
	#include <sys/socket.h>
	#include <sys/types.h>
	#include <netdb.h>
	#include <dirent.h>
	#include <errno.h>
	#include <unistd.h>
	#include <ifaddrs.h>


	#include <stdio.h>
	#include <stdlib.h>
	#include <unistd.h>
	#include <arpa/inet.h>


#endif

#include <iostream>
using namespace std;

string ns_socket_connection::read(const unsigned long size){
	char * buf = new char[size+1];
	try{
		unsigned long bytes_read = 0;
		while (bytes_read < size)
			bytes_read += this->read(&buf[bytes_read],size-bytes_read);
		string tmp;
		tmp.resize(size);
		for (unsigned int i = 0; i < size; i++)
			tmp[i] = buf[i];
		delete[] buf;
		return tmp;
	}
	catch(...){
		delete buf;
		throw;
	}
}
void ns_socket_connection::operator<<(const string & str){
	this->write(str.c_str(),static_cast<unsigned long>(str.size()));
}
void ns_socket_connection::operator<<(const char * str){
	this->write(str,static_cast<unsigned long>(strlen(str)));
}

void ns_socket_connection::flush(){
        char buf[1025];
        int recieve_result = 0;
	#ifdef _WIN32 
        while(recieve_result > 0)
		recieve_result = recv(handle, buf, 1024,0);

	      
      
	#else
	while(recieve_result > 0)
	  recieve_result =  ::read(handle, buf, 1024);
	
	#endif

}
#ifdef _WIN32 
string get_windows_socket_error(){
	int err = WSAGetLastError();
	switch(err){
		case WSA_INVALID_HANDLE: return "WSA_INVALID_HANDLE";
		case WSA_NOT_ENOUGH_MEMORY: return "WSA_NOT_ENOUGH_MEMORY";
		case WSA_INVALID_PARAMETER: return "WSA_INVALID_PARAMETER";
		case WSA_OPERATION_ABORTED: return "WSA_OPERATION_ABORTED";
		case WSA_IO_INCOMPLETE: return "WSA_IO_INCOMPLETE";
		case WSA_IO_PENDING: return "WSA_IO_PENDING";
		case WSAEINTR: return "WSAEINTR";
		case WSAEBADF: return "WSAEBADF";
		case WSAEACCES: return "WSAEACCES";
		case WSAEFAULT: return "WSAEFAULT";
		case WSAEINVAL: return "WSAEINVAL";
		case WSAEMFILE: return "WSAEMFILE";
		case WSAEWOULDBLOCK: return "WSAEWOULDBLOCK";
		case WSAEINPROGRESS: return "WSAEINPROGRESS";
		case WSAEALREADY: return "WSAEALREADY";
		case WSAENOTSOCK: return "WSAENOTSOCK";
		case WSAEDESTADDRREQ: return "WSAEDESTADDRREQ";
		case WSAEMSGSIZE: return "WSAEMSGSIZE";
		case WSAEPROTOTYPE: return "WSAEPROTOTYPE";
		case WSAENOPROTOOPT: return "WSAENOPROTOOPT";
		case WSAEPROTONOSUPPORT: return "WSAEPROTONOSUPPORT";
		case WSAESOCKTNOSUPPORT: return "WSAESOCKTNOSUPPORT";
		case WSAEOPNOTSUPP: return "WSAEOPNOTSUPP";
		case WSAEPFNOSUPPORT: return "WSAEPFNOSUPPORT";
		case WSAEAFNOSUPPORT: return "WSAEAFNOSUPPORT";
		case WSAEADDRINUSE: return "WSAEADDRINUSE";
		case WSAEADDRNOTAVAIL: return "WSAEADDRNOTAVAIL";
		case WSAENETDOWN: return "WSAENETDOWN";
		case WSAENETUNREACH: return "WSAENETUNREACH";	
		case WSAENETRESET: return "WSAENETRESET";
		case WSAECONNABORTED: return "WSAECONNABORTED";
		case WSAECONNRESET: return "WSAECONNRESET";
		case WSAENOBUFS: return "WSAENOBUFS";
		case WSAEISCONN: return "WSAEISCONN";	
		case WSAENOTCONN: return "WSAENOTCONN";
		case WSAESHUTDOWN: return "WSAESHUTDOWN";
		case WSAETOOMANYREFS: return "WSAETOOMANYREFS";
		case WSAETIMEDOUT: return "WSAETIMEDOUT";
		case WSAECONNREFUSED: return "WSAECONNREFUSED";
		case WSAELOOP: return "WSAELOOP";
		case WSAENAMETOOLONG: return "WSAENAMETOOLONG";
		case WSAEHOSTDOWN: return "WSAEHOSTDOWN";
		case WSAEHOSTUNREACH: return "WSAEHOSTUNREACH";
		case WSAENOTEMPTY: return "WSAENOTEMPTY";
		case WSAEPROCLIM: return "WSAEPROCLIM";
		case WSAEUSERS: return "WSAEUSERS";
		case WSAEDQUOT: return "WSAEDQUOT";
		case WSAESTALE: return "WSAESTALE";
		case WSAEREMOTE: return "WSAEREMOTE";
		case WSASYSNOTREADY: return "WSASYSNOTREADY";
		case WSAVERNOTSUPPORTED: return "WSAVERNOTSUPPORTED";
		case WSANOTINITIALISED: return "WSANOTINITIALISED";
		case WSAEDISCON: return "WSAEDISCON";
		case WSAENOMORE: return "WSAENOMORE";
		case WSAECANCELLED: return "WSAECANCELLED";
		case WSAEINVALIDPROCTABLE: return "WSAEINVALIDPROCTABLE";
		case WSAEINVALIDPROVIDER: return "WSAEINVALIDPROVIDER";
		case WSAEPROVIDERFAILEDINIT: return "WSAEPROVIDERFAILEDINIT";
		case WSASYSCALLFAILURE: return "WSASYSCALLFAILURE";
		case WSASERVICE_NOT_FOUND: return "WSASERVICE_NOT_FOUND";
		case WSATYPE_NOT_FOUND: return "WSATYPE_NOT_FOUND";
		case WSA_E_NO_MORE: return "WSA_E_NO_MORE";
		case WSA_E_CANCELLED: return "WSA_E_CANCELLED";
		case WSAEREFUSED: return "WSAEREFUSED";
		case WSAHOST_NOT_FOUND: return "WSAHOST_NOT_FOUND";
		case WSATRY_AGAIN: return "WSATRY_AGAIN";
		case WSANO_RECOVERY: return "WSANO_RECOVERY";
		case WSANO_DATA: return "WSANO_DATA";
		case WSA_QOS_RECEIVERS: return "WSA_QOS_RECEIVERS";
		case WSA_QOS_SENDERS: return "WSA_QOS_SENDERS";
		case WSA_QOS_NO_SENDERS: return "WSA_QOS_NO_SENDERS";
		case WSA_QOS_NO_RECEIVERS: return "WSA_QOS_NO_RECEIVERS";
		case WSA_QOS_REQUEST_CONFIRMED: return "WSA_QOS_REQUEST_CONFIRMED";
		case WSA_QOS_ADMISSION_FAILURE: return "WSA_QOS_ADMISSION_FAILURE";
		case WSA_QOS_POLICY_FAILURE: return "WSA_QOS_POLICY_FAILURE";
		case WSA_QOS_BAD_STYLE: return "WSA_QOS_BAD_STYLE";
		case WSA_QOS_BAD_OBJECT: return "WSA_QOS_BAD_OBJECT";
		case WSA_QOS_TRAFFIC_CTRL_ERROR: return "WSA_QOS_TRAFFIC_CTRL_ERROR";
		case WSA_QOS_GENERIC_ERROR: return "WSA_QOS_GENERIC_ERROR";
		case WSA_QOS_ESERVICETYPE: return "WSA_QOS_ESERVICETYPE";
		case WSA_QOS_EFLOWSPEC: return "WSA_QOS_EFLOWSPEC";
		case WSA_QOS_EPROVSPECBUF: return "WSA_QOS_EPROVSPECBUF";
		case WSA_QOS_EFILTERSTYLE: return "WSA_QOS_EFILTERSTYLE";
		case WSA_QOS_EFILTERTYPE: return "WSA_QOS_EFILTERTYPE";
		case WSA_QOS_EFILTERCOUNT: return "WSA_QOS_EFILTERCOUNT";
		case WSA_QOS_EOBJLENGTH: return "WSA_QOS_EOBJLENGTH";
		case WSA_QOS_EFLOWCOUNT: return "WSA_QOS_EFLOWCOUNT";
		case WSA_QOS_EUNKOWNPSOBJ: return "WSA_QOS_EUNKOWNPSOBJ";
		case WSA_QOS_EPOLICYOBJ: return "WSA_QOS_EPOLICYOBJ";
		case WSA_QOS_EFLOWDESC: return "WSA_QOS_EFLOWDESC";
		case WSA_QOS_EPSFLOWSPEC: return "WSA_QOS_EPSFLOWSPEC";
		case WSA_QOS_EPSFILTERSPEC: return "WSA_QOS_EPSFILTERSPEC";	
		case WSA_QOS_ESDMODEOBJ: return "WSA_QOS_ESDMODEOBJ";
		case WSA_QOS_ESHAPERATEOBJ: return "WSA_QOS_ESHAPERATEOBJ";
		case WSA_QOS_RESERVED_PETYPE: return "WSA_QOS_RESERVED_PETYPE";
		default: return "Unknown Socket Error";
	}
}
#endif
unsigned int ns_socket_connection::read(char * buf, const unsigned long size){
  
	unsigned long bytes_read=0;
	int recieve_result;

	#ifdef _WIN32 
	while(bytes_read < size){
		recieve_result = recv(handle, &buf[bytes_read], size - bytes_read,0);
		if (recieve_result == SOCKET_ERROR){


				

			throw ns_ex("Error during recieve: ") << get_windows_socket_error();


		}
		if (recieve_result == 0)
				throw ns_ex("Connection gracefully closed.");
		bytes_read+=recieve_result;
	}
	#else
  
	while(bytes_read < size){
	  //cerr << "Looking for "  << size-bytes_read << " bytes...";
		recieve_result =  ::read(handle, &buf[bytes_read], size - bytes_read);
	       
		if (recieve_result == -1)
			throw ns_ex("Error during send!");
		//cerr << "Recieved " << recieve_result << "\n";
		bytes_read+=recieve_result;
	}
	#endif

    return bytes_read;
}

void ns_socket_connection::write(const char * buf, const unsigned long size){
	int bytes_written = 0;
	int send_result;

	#ifdef _WIN32 
	while((unsigned)bytes_written < size){
		send_result= ::send (handle,&buf[bytes_written],size - bytes_written,0);
		if (send_result == SOCKET_ERROR)
			throw ns_ex("ns_socket_connection::Error during send!") << get_windows_socket_error();
		//if (send_result == WSAEMSGSIZE)
		//	throw ns_ex("ns_socket_connection::Maximum packet size exceeded!");
		bytes_written+=send_result;
	}
	#else
		bytes_written = ::write(handle, buf, size);
		if (bytes_written == -1)
			throw ns_ex("Error during send!");
	#endif

}


unsigned int uint_from_str(const string & str){
	unsigned int a =	static_cast<unsigned char>((char)str[3]);
	unsigned int aa =	static_cast<unsigned char>((char)str[2]);
	unsigned int aaa =	static_cast<unsigned char>((char)str[1]);
	unsigned int aaaa = static_cast<unsigned char>((char)str[0]);
	return a | (aa << 8) | (aaa << 16) | (aaaa << 24);
}

string str_from_uint(const unsigned int i){
	string str("....");
	str[3] = static_cast<char>(i & 0x000000FF);
	str[2] = static_cast<char>((i >> 8) & 0x000000FF);
	str[1] = static_cast<char>((i >> 16) & 0x000000FF);
	str[0] = static_cast<char>((i >> 24) & 0x000000FF);
	return str;
}	


void out_char_string(const string & str){
	cerr<< "{ ";
	for (unsigned int i = 0; i < str.size(); i++){
		cerr << (int)str[i] << ",";
	}
	cerr << "}";

}
void ns_socket_connection::write(const unsigned long & i){
	this->write(reinterpret_cast<const char *>(&i),sizeof(unsigned long)/sizeof(char));
}
void ns_socket_connection::write(const long & i){
	this->write(*reinterpret_cast<const unsigned long *>(&i));
}

void ns_socket_connection::write(const ns_64_bit & i){
	this->write(reinterpret_cast<const char *>(&i),sizeof(ns_64_bit)/sizeof(char));
}
void ns_socket_connection::write(const ns_s64_bit & i){
	
	this->write(*reinterpret_cast<const ns_64_bit *>(&i));
}

void ns_socket_connection::write(const unsigned int & i){write(static_cast<const unsigned long>(i));}
void ns_socket_connection::write(const unsigned char & i){write(static_cast<const unsigned long>(i));}
void ns_socket_connection::write(const char & i){write(static_cast<const long>(i));}


unsigned long ns_socket_connection::read_uint(){
        unsigned long i=0;
	//	cerr << "Reading" << (unsigned long)(sizeof(unsigned long)/sizeof(char)) << " bytes.\n";
	this->read(reinterpret_cast<char *>(&i),(unsigned long)(sizeof(unsigned long)/sizeof(char)));
	//cerr << "recieved " << is.size() << " characters: " <<uint_from_str(is)<< "(";
	//out_char_string(is);
	//cerr << ")\n";
	return i;
}

long ns_socket_connection::read_int(){
	long i = read_uint();
	return *(static_cast<long *>(&i));
}


ns_64_bit ns_socket_connection::read_64bit_uint(){
        unsigned long i=0;
	//	cerr << "Reading" << (unsigned long)(sizeof(unsigned long)/sizeof(char)) << " bytes.\n";
	this->read(reinterpret_cast<char *>(&i),(ns_64_bit)(sizeof(ns_64_bit)/sizeof(char)));
	//cerr << "recieved " << is.size() << " characters: " <<uint_from_str(is)<< "(";
	//out_char_string(is);
	//cerr << ")\n";
	return i;
}

ns_s64_bit ns_socket_connection::read_64bit_int(){
	ns_64_bit i = read_64bit_uint();
	return *(reinterpret_cast<ns_s64_bit*>(&i));
}

void ns_socket_connection::operator<<(const unsigned long & i){
	this->write(i);
}

void ns_socket_connection::operator<<(const long & i){
	this->write(i);
}
void ns_socket_connection::operator<<(const unsigned int & i){this->write(static_cast<const unsigned long>(i));}
void ns_socket_connection::operator<<(const int & i){this->write(static_cast<const long>(i));}
void ns_socket_connection::operator<<(const unsigned char & i){this->write(static_cast<const unsigned long>(i));}
void ns_socket_connection::operator<<(const char & i){this->write(static_cast<const long>(i));}



void ns_socket_connection::close(){
	if (!is_open)
		return;
	is_open = false;
	#ifdef _WIN32 
	if (closesocket (handle) == SOCKET_ERROR){
		cerr << " ns_socket_connection::close() wanted to throw an exception, but could not: Error closing socket: ";
		cerr << get_windows_socket_error();
	}		
			
	#else
	if (::close(handle) == -1){
		cerr << " ns_socket_connection::close() wanted to throw an exception, but could not: Error closing socket!";
	}
	#endif

}


void ns_socket::close_socket(){
	#ifdef _WIN32 
	closesocket(listen_socket);
	#else
	close(listen_socket);
	#endif
}
void ns_socket::listen(const unsigned int port, const unsigned int connection_buffer_size){

	#ifdef _WIN32 

 		listen_socket =  socket (AF_INET,SOCK_STREAM,IPPROTO_TCP);
		if (listen_socket == INVALID_SOCKET)
			   throw ns_ex("Could not create socket for listening.");
		char buf[100];
		if ( gethostname (buf,100) == SOCKET_ERROR)
				throw ns_ex("Could not resolve current host name");
		hostent *host = gethostbyname(buf);
		if (host == NULL)
				throw ns_ex("Could not resolve host name entity");

		sockaddr_in saddr;
		int size = sizeof(struct sockaddr_in);
		memset(&saddr, 0, size);

		saddr.sin_family = host->h_addrtype;  //host address
		saddr.sin_port = htons(port);    //port in host byte order

		if ( bind(listen_socket, const_cast<const sockaddr *>(reinterpret_cast	<sockaddr * >(&saddr)),size)
				 == INVALID_SOCKET)
				throw ns_ex("Could not bind socket to port ") << port;
		 ::listen(listen_socket,connection_buffer_size);

	#else

		sockaddr_in addr;

		memset(&addr, 0, sizeof(addr));
		addr.sin_family = AF_INET;
		addr.sin_port = htons(port);
		addr.sin_addr.s_addr = htonl(INADDR_ANY);
	  

		listen_socket = ::socket(PF_INET, SOCK_STREAM, 0);
	  if (listen_socket < 0)
	  	throw ns_ex("Could not create socket for listening.");

	  int val = 1;
	  int ret = setsockopt(listen_socket,SOL_SOCKET,SO_REUSEADDR,&val,sizeof(val));
	  if (ret != 0)
	  	throw ns_ex("Could not set socket options");

	  ret = bind(listen_socket, (struct sockaddr *)&addr, sizeof(addr));
	  if (ret != 0)
	  	throw ns_ex("Could not bind socket to port ") << port;

	  ret = ::listen(listen_socket, connection_buffer_size);
	  if (ret != 0)
	  	throw ns_ex("Could not listen on port ") << port;

	#endif

}


ns_socket_connection ns_socket::accept(){
	#ifdef _WIN32 

		ns_socket_handle cn = ::accept(listen_socket, NULL, NULL);
		if (cn == INVALID_SOCKET)
			throw ns_ex("Error accepting remote connection.");
		return ns_socket_connection(cn);

	#else

		sockaddr_in addr;
		socklen_t len = sizeof(addr);

		int cn = ::accept(listen_socket, (struct sockaddr *)&addr, &len);
		if (cn < 0)
		throw ns_ex("Error accepting remote connection.");

		return ns_socket_connection(cn);

	#endif

}

void ns_socket::build_interface_list(std::vector<ns_interface_info> & interfaces){
	#ifdef _WIN32 
		char localhostname[1000];
	//	gethostname_lock.wait_to_acquire(__FILE__,__LINE__);
		try{
			if (gethostname(localhostname, 1000) == SOCKET_ERROR)
				throw ns_ex() << "Error getting local hostname";

			hostent *ent = gethostbyname(localhostname);
			if (ent == NULL)
					throw ns_ex() << "Could not parse local hostname: \"" << localhostname << "\"";
			//cerr << "Getting ip adress info...\n";
			for (unsigned int i = 0; ent->h_addr_list[i] != 0; i++){
					in_addr addr;
					memcpy(&addr, ent->h_addr_list[i], sizeof(in_addr));
					interfaces.push_back(ns_interface_info(ns_to_string(i),inet_ntoa(addr)));
			}
		//cerr << "Done.\n";		
		}
		catch(...){
	//		gethostname_lock.release();
			throw;
		}
		//gethostname_lock.release();
	#else
		// from http://stackoverflow.com/questions/4139405/how-to-know-ip-address-for-interfaces-in-c
		// works on linux and OS X
		struct ifaddrs *ifap, *ifa;
		struct sockaddr_in *sa;

		getifaddrs(&ifap);
		for (ifa = ifap; ifa != NULL; ifa = ifa->ifa_next) {
			if (ifa->ifa_addr->sa_family==AF_INET) {
				sa = (struct sockaddr_in *) ifa->ifa_addr;
				interfaces.push_back(ns_interface_info(ifa->ifa_name,inet_ntoa(sa->sin_addr)));

			}
		}

	    freeifaddrs(ifap);
	#endif
}

const string ns_socket::get_local_ip_address(const std::string & interface_name){

	vector<ns_interface_info> interfaces;
	build_interface_list(interfaces);
	if (interfaces.size() == 0)
		throw ns_ex("ns_socket::There are no ethernet interfaces available on this machine.");

	
	if (interface_name.size() == 0){
		//choose the default interface 
		//try to avoid localhost, as that is not informative.
		for (vector<ns_interface_info>::const_iterator p = interfaces.begin(); p != interfaces.end(); ++p){
			if (p->ip_address != "127.0.0.1" && p->interface_name != "lo")
				return p->ip_address;
		}
		//return localhost if no other is found.
		return interfaces[0].ip_address;
	}
	else{
		for (vector<ns_interface_info>::const_iterator p = interfaces.begin(); p != interfaces.end(); ++p)
			if (interface_name == p->interface_name)
				return p->ip_address;
		ns_ex ex("ns_socket::Could not find requested interface:");
		ex << interface_name << "\nCurrently Connected:\n";
		for (vector<ns_interface_info>::const_iterator p = interfaces.begin(); p != interfaces.end(); ++p)
			ex << p->interface_name << "(" << p->ip_address << ")\n";
		throw ex;
	}
}


ns_socket_connection::~ns_socket_connection(){
//	if (this->is_open)
//		close();
}

ns_socket_connection ns_socket::connect(const string & address, const unsigned int port){

	#ifdef _WIN32 

	 	hostent     *host_ent;

		host_ent = gethostbyname(address.c_str());
		if (host_ent == NULL)
			throw ns_ex() << "Could not parse address: \"" << address << "\"";
		//cerr << "Connecting to socket...\n";
		sockaddr_in saddr;
		memset(&saddr,0,sizeof(sockaddr));
		memcpy(&saddr.sin_addr, host_ent->h_addr, host_ent->h_length);
		//cerr << "Done.\n";
		saddr.sin_family = host_ent->h_addrtype;
		saddr.sin_port = htons(port);

		//create socket
		ns_socket_handle handle = socket(host_ent->h_addrtype, SOCK_STREAM, 0);
		if (handle == INVALID_SOCKET)
				throw ns_ex("Could not create socket for remote connect.");

		//connect over socket
		if (::connect(handle, const_cast<const sockaddr *>(reinterpret_cast <sockaddr * >(&saddr)), sizeof(sockaddr_in)) == SOCKET_ERROR)
				throw ns_ex("Could not open socket");

		return ns_socket_connection(handle);

	#else

		hostent * he = gethostbyname(address.c_str());
		if (he == NULL)
			throw ns_ex("Could not parse address.");

		sockaddr_in servaddr;
		memset(&servaddr, 0, sizeof(servaddr));
		servaddr.sin_family = AF_INET;
		servaddr.sin_port = htons(port);
        servaddr.sin_addr = *((struct in_addr *)he->h_addr);

		errno = 0;
		ns_socket_handle sockfd = socket(AF_INET, SOCK_STREAM, 0);
		if (sockfd == -1)
			throw ns_ex("Could not create socket for remote connect:") << strerror(errno);

		int res = ::connect(sockfd, (struct sockaddr *)&servaddr, sizeof(servaddr));
		if (res != 0)
			throw ns_ex("Could not open socket:") << strerror(errno);
		return ns_socket_connection(sockfd);


	#endif

}

#include <iostream>
void ns_socket::global_init(){
	if (global_init_performed)
		return;

	#ifdef _WIN32 
		//std::cerr << "Initializing Windows Sockets...\n";
		//Windows Sockets need to be initialized once per instance.
		//This init code was copied from the Microsoft Windows Sockets 2 Reference Guide
		WORD wVersionRequested;
		WSADATA wsaData;
		int err;
		wVersionRequested = MAKEWORD( 2, 0 );

		err = WSAStartup( wVersionRequested, &wsaData );
		if ( err != 0 )
				throw ns_ex("Could not locate WinSock Dll file.");

		if ( LOBYTE( wsaData.wVersion ) != 2 || HIBYTE( wsaData.wVersion ) != 0 ) {
				WSACleanup( );
				throw ns_ex("The Winsock Dll found is invalid.");
		}
		global_init_performed = true;
	#else


	#endif
}

void ns_socket::global_clean(){
	#ifdef _WIN32 
        WSACleanup();
	#endif
}

ns_lock ns_socket::gethostname_lock("ns_socket::gethostname");
bool ns_socket::global_init_performed = false;
