#ifndef NS_EX
#define NS_EX

#ifdef _WIN32
	#ifdef INCLUDE_STDAFX
		#include "stdafx.h"
	#endif
#endif

#ifdef _DEBUG
	#ifndef NS_MINIMAL_SERVER_BUILD
//#include <vld.h>
	#endif
#endif

#ifdef _WIN32
	#include <winsock2.h>
//	#include <windows.h>
	//#include <omp.h>
#endif
//#include <crtdbg.h>
#include <string>
#include <map>
#include <vector>
#include "time.h"

typedef unsigned char ns_8_bit;
typedef unsigned short ns_16_bit;
typedef unsigned long ns_32_bit;
#ifdef _WIN32
	typedef unsigned __int64 ns_64_bit;
	typedef __int64 ns_s64_bit;
	#define ns_64_bit_abs _abs64

#else
	typedef unsigned long long ns_64_bit;
	typedef long long ns_s64_bit;
//	#define ns_64_bit_abs abs
	#define ns_64_bit_abs(a) ((a<0) ? (-a) : (a))
#endif
typedef bool ns_bit;

/*#ifdef _DEBUG
   #define DEBUG_CLIENTBLOCK   new( _CLIENT_BLOCK, __FILE__, __LINE__)
#else
   #define DEBUG_CLIENTBLOCK
#endif

#ifdef _DEBUG
#define new DEBUG_CLIENTBLOCK
#endif*/


unsigned long ns_current_time();

std::string ns_format_time_string(const tm & t);
std::string ns_format_time_string(const unsigned long);
std::string ns_format_time_string_for_human(const tm & t);
std::string ns_format_time_string_for_human(const unsigned long);
std::string ns_format_time_string_for_tiff(const unsigned long t);

unsigned long ns_time_from_format_string(const std::string & t);

ns_64_bit ns_atoi64(const char * s);

std::string ns_to_string(const unsigned int);
std::string ns_to_string(const int);
std::string ns_to_string(const unsigned long);
std::string ns_to_string(const long);
std::string ns_to_string(const ns_64_bit);
std::string ns_to_string(const ns_s64_bit);
std::string ns_to_string(const double);
std::string ns_to_string(const float);
std::string ns_to_string_short(const float,unsigned int decimal_places=1);
std::string ns_to_string_short(const double,unsigned int decimal_places=1);
std::string ns_to_string_scientific(const double k);
const std::string & ns_to_string(const std::string &);
std::string ns_to_string(const char *);

//ns_text_stream accepts a variety of variable types
//and returns objects of type derived_t.
//stream_type_t allows returned objects to be "tagged" with flags of the type stream_type_t.
template<class derived_t, class stream_type_t>
class ns_text_stream{
public:
	ns_text_stream(derived_t & derived, const stream_type_t & default_type):stream_type(default_type){}
	ns_text_stream(){}
	std::string text() const{return value;}
	void clear_text(){value.clear();}

	derived_t & operator<<(const char * str ){append(str);return *d_this();}
	derived_t & operator<<(const std::string & str){append(str);return *d_this();}
	derived_t & operator<<(const int val){append(val);return *d_this();}
	derived_t & operator<<(const unsigned int val){append(val);return *d_this();}
	derived_t & operator<<(const long int val){append(val);return *d_this();}
	derived_t & operator<<(const ns_64_bit val){append(val);return *d_this();}
	derived_t & operator<<(const ns_s64_bit val){append(val);return *d_this();}
	derived_t & operator<<(const unsigned long int val){append(val);return *d_this();}
	derived_t & operator<<(const double val){append(val);return *d_this();}

	derived_t & operator << (const stream_type_t & ex){ stream_type = ex; return *d_this();}

	//virtual ~ns_text_stream() throw (){}
	inline const stream_type_t & type() const {return stream_type;}

protected:
	template<class T> void append(const T & i){
		value += ns_to_string(i);
	}
	std::string value;
	stream_type_t stream_type;
	derived_t *d_this(){return static_cast<derived_t *>(this);}
};


typedef enum { ns_standard, ns_file_io, ns_network_io, ns_fatal, ns_sql_fatal, ns_memory_allocation, ns_cache} ns_ex_type;
std::string ns_ex_type_string(const ns_ex_type & ex);
ns_ex_type ns_ex_type_from_string(const std::string & str);
//#include <iostream>
#pragma warning(disable: 4355)  //our use of this in the constructor is completely legit, so we disable the warning.
class ns_ex : public ns_text_stream<ns_ex, ns_ex_type>, public std::exception{
public:
	~ns_ex() throw(){}
	ns_ex():ns_text_stream<ns_ex, ns_ex_type>(*this, ns_standard){}
	ns_ex(const char * str):ns_text_stream<ns_ex, ns_ex_type>(*this, ns_standard){append(str);}
	/*ns_ex(const char * _filename, const unsigned long line, const char * err):ns_text_stream<ns_ex, ns_ex_type>(*this, ns_standard){line_number=line;filename=_filename;append(err);}
	ns_ex(const char * _filename, const unsigned long line):ns_text_stream<ns_ex, ns_ex_type>(*this, ns_standard){line_number=line;filename=_filename;}
	*/
	ns_ex(const std::exception & exc):ns_text_stream<ns_ex, ns_ex_type>(*this, ns_standard){
		std::string wha(exc.what());
		std::string::size_type p= wha.find_last_of(";");
		if (p == std::string::npos){
			*this << "std::" << wha;
			if (wha.find("bad") != wha.npos && wha.find("alloc") != wha.npos)
				*this<< ns_memory_allocation;
			else *this << ns_fatal;
			return;
		}
		*this << wha.substr(0,p);
		*this << ns_ex_type_from_string(wha.substr(p+1));
	}


	ns_ex(const std::string & str):ns_text_stream<ns_ex,  ns_ex_type>(*this, ns_standard){append(str);}

	const char * what() const throw() {
		std::string wha = ns_text_stream<ns_ex,ns_ex_type>::text();
		wha+=";";
		wha+=ns_ex_type_string(this->type());
		std::string::size_type s = wha.size();

		//Not sure when the pointer returned by text.c_str() goes sour, so we copy the error to a safe location.
		if (s > 1023)
			s = 1023;
		for (std::string::size_type i = 0; i < s; i++)
			err_text[i] = wha[i];
		err_text[s] = 0;
		return err_text;
	}
	#ifdef _WIN32
	void append_windows_error();
	#endif

private:
	mutable char err_text[1024];
	/*
	unsigned long line_number;
	std::string filename;*/
};

//text streams are generally used to concatenate mysql requests; the stream_type flag type is set
//to bool simply because we don't really ever use the type data member
class ns_text_stream_t : public ns_text_stream<ns_text_stream_t, bool>{
public:
	ns_text_stream_t():ns_text_stream<ns_text_stream_t, bool>(*this,false){}
	ns_text_stream_t(const char * str):ns_text_stream<ns_text_stream_t, bool>(*this,false){append(str);}
	ns_text_stream_t(const std::string & str):ns_text_stream<ns_text_stream_t, bool>(*this,false){append(str);}
};

//#define ns_ex(...) ns_ex(__FILE__,__LINE__)
template<class T>
void ns_safe_delete(T * & pointer){
	if (pointer == 0) return;
	T * tmp(pointer);
	pointer = 0;
	delete tmp;
}

template<class T>
class ns_acquire_for_scope{
public:
	ns_acquire_for_scope(T * ptr):p(ptr){}
	ns_acquire_for_scope():p(){}

	void attach(T * ptr){p=ptr;}
	void release(){ns_safe_delete(p);p = 0;}

	T & operator()(){
		if (p==0)
			throw ns_ex("Accessing invalid ptr!");
		return *p;
	}
	inline bool is_null()const{return p==0;}

	~ns_acquire_for_scope(){ns_safe_delete(p);}
private:
	T * p;
};

#pragma warning(default: 4355)

std::string ns_to_lower(const std::string & s);


typedef void (*ns_global_debug_output_handler)(const ns_text_stream_t &);
void ns_set_global_debug_output_handler(ns_global_debug_output_handler handler);
void ns_global_debug(const ns_text_stream_t &);
#endif


