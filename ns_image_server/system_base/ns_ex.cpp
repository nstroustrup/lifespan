#ifndef NS_SINGLE_THREAD
#include "ns_thread.h"
#endif
#include "ns_ex.h"
#include <time.h>

using namespace std;

#include <time.h>
#include <string>
#include <stdio.h>
#include <stdlib.h>

#define NS_SNPRINTF_BUFFER_SIZE 256

#ifdef _WIN32 
#define snprintf _snprintf_s
#endif

#ifndef NS_SINGLE_THREAD
ns_lock current_time_lock("ns_fts::current_time");
#endif

string ns_format_time_string(const tm & t){
	char buf[40];
	snprintf(buf,40,"%i-%02i-%02i=%02i-%02i",t.tm_year+1900,t.tm_mon+1,t.tm_mday,t.tm_hour,t.tm_min);
	return string(buf);
}
long ns_parse_triplet(const std::string & s,const char delim1, const char delim2,const unsigned long start_p,std::string & a, std::string & b, std::string & c){
	
	unsigned int state(0);
	unsigned int i;
	for (i= start_p; i < s.size(); i++){
		if (state==0 && isspace(s[i]))
			continue;
		if (s[i]==delim1 || s[i] == delim2){
			state++;
			continue;
		}
		if (isspace(s[i])) return i+1;
		switch(state){
			case 0: a+=s[i]; break;
			case 1: b+=s[i]; break;
			case 2: c+=s[i]; break;
			default: return -1;
		}
	}
	return i;
}
long ns_parse_date(const string & s,const unsigned long start_p,struct tm &tm){
	std::string as,bs,cs;
	long p(ns_parse_triplet(s,'\\','/',start_p,as,bs,cs));
	if (p == -1)
		throw ns_ex("Could not parse date (1): \"") << s << "\"[" << start_p << "," << s.size() << "]";
	if (as.empty() || bs.empty() || cs.empty())
		throw ns_ex("Could not parse date: \"") << s << "\"[" << start_p << "," << s.size() << "]";
	
	unsigned long a (atol(as.c_str())),
				  b (atol(bs.c_str())),
				  c (atol(cs.c_str()));

	if (a > 31){ //year month day
		tm.tm_year = a-1900;
		tm.tm_mon = b-1;
		tm.tm_mday = c;
	}
	else{  //month day year
		tm.tm_mon = a-1;
		tm.tm_mday = b;
		tm.tm_year = c-1900;
	}
	return p;
}

unsigned long ns_parse_time(const string & s, const unsigned long start_p, struct tm & tm){
	std::string as,bs,cs;
	long p(ns_parse_triplet(s,':',0,start_p,as,bs,cs));
	if (p == -1)
		throw ns_ex("Could not parse time: \"") << s << "\"[" << start_p << "," << s.size() << "]";
	if (as.empty() || bs.empty())
		throw ns_ex("Could not parse time: \"") << s << "\"[" << start_p << "," << s.size() << "]";
	
	tm.tm_hour = atol(as.c_str());
	tm.tm_min = atol(bs.c_str());
	if (!cs.empty())
		tm.tm_sec = atol(bs.c_str());
	else tm.tm_sec = 0;

	return p;

}
unsigned long ns_time_from_format_string(const std::string & t){
	const std::string format("Format: MM/DD/YYYY HH:MM:SS OR HH:MM:SS MM/DD/YYYY");

	std::string::size_type date(t.find("/"));
	std::string::size_type tt(t.find(":"));

	if (date == t.npos)
		throw ns_ex("Could not parse time (No Date Found) ") << t << " " << format;
	struct tm tm;
	tm.tm_wday = 0;
	tm.tm_yday = 0;
	tm.tm_isdst = -1;

	if (tt == t.npos){
		//MM/DD/YYY OR YYYY/MM/DD
		ns_parse_date(t,0,tm);
		tm.tm_hour = 0;
		tm.tm_min = 0;
		tm.tm_sec = 0;
	}
	else{
		unsigned long c(0);
		std::string time_str;
		for (unsigned int i = tt;  i < t.size(); i++)
			if (t[i] == ':')
				c++;
		switch(c){
			case 1: time_str = "HH:MM"; break;
			case 2: time_str = "HH:MM:SS"; break;
			default: 
				throw ns_ex("Could not parse time (Could not parse time) ") << t << " " << format;
		}
		if (tt > date){
			//MM/DD/YYYY HH:MM:SS
			unsigned long p(ns_parse_date(t,0,tm));
			ns_parse_time(t,p,tm);
		}
		else{
			//HH:MM:SS MM/DD/YYYY
			unsigned long p(ns_parse_time(t,0,tm));
			ns_parse_date(t,p,tm);
		}
	}
	time_t ttt(mktime(&tm));
	if (ttt==-1)
		throw ns_ex("Could not parse time (tm) ") << t << " " << format;
	return (unsigned long)ttt;

}


string ns_format_time_string(const unsigned long t){
	time_t current_time = static_cast<time_t>(t);
	//localtime is not thread safe.
#ifndef NS_SINGLE_THREAD
	current_time_lock.wait_to_acquire(__FILE__,__LINE__);
#endif
	tm *tm = localtime(&current_time);
	string str(ns_format_time_string(*tm));
	
#ifndef NS_SINGLE_THREAD
	current_time_lock.release();
#endif
	return str;
}

string ns_format_time_string_for_human(const tm & t){
	char buf[40];
	snprintf(buf,40,"%02i:%02i %02i/%02i/%i",t.tm_hour,t.tm_min,t.tm_mon+1,t.tm_mday,t.tm_year+1900);
	return string(buf);
}
string ns_format_time_string_for_tiff(const tm & t){
	char buf[40];
	snprintf(buf,40,"%04i:%02i:%02i %02i:%02i:%02i",t.tm_year+1900,t.tm_mon+1,t.tm_mday, t.tm_hour,t.tm_min,t.tm_sec);
	return string(buf);
}

string ns_format_time_string_for_human(const unsigned long t){
	time_t current_time = static_cast<time_t>(t);
	//localtime is not thread safe.
	
#ifndef NS_SINGLE_THREAD
	current_time_lock.wait_to_acquire(__FILE__,__LINE__);
#endif
	tm *tm = localtime(&current_time);
	string str(ns_format_time_string_for_human(*tm));
	
#ifndef NS_SINGLE_THREAD
	current_time_lock.release();
#endif
	return str;
}

string ns_format_time_string_for_tiff(const unsigned long t){
	time_t current_time = static_cast<time_t>(t);
	//localtime is not thread safe.
	
#ifndef NS_SINGLE_THREAD
	current_time_lock.wait_to_acquire(__FILE__,__LINE__);
#endif
	tm *tm = localtime(&current_time);
	string str(ns_format_time_string_for_tiff(*tm));
	
#ifndef NS_SINGLE_THREAD
	current_time_lock.release();
#endif
	return str;
}


///Returns the current time (seconds since 1/1/1970)
unsigned long ns_current_time(){
	time_t t;
	t = time(&t);
	return static_cast<unsigned long>(t);
}

ns_64_bit ns_atoi64(const char * s){
	#ifdef WIN32
	  return _atoi64(s);
	#else
	  return atoll(s);
	#endif
}

string ns_to_string(const unsigned int k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%u",k);
	return string()+buf;
}
string ns_to_string(const int k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%i",k);
	return string()+buf;
}

string ns_to_string(const unsigned long k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%lu",k);
	return string()+buf;
}
string ns_to_string(const long k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%li",k);
	return string()+buf;
}


std::string ns_to_string(const ns_64_bit k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%llu",k);
	return string()+buf;
}

std::string ns_to_string(const ns_s64_bit k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%lli",k);
	return string()+buf;
}




string ns_to_string(const float k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%f",k);
	return string()+buf;
}
string ns_to_string(const double k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%f",k);
	return string()+buf;
}
string ns_to_string_scientific(const double k){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,"%.3e",k);
	return string()+buf;
}
string ns_to_string_short(const float k,const unsigned int decimal_places){
	char buf[NS_SNPRINTF_BUFFER_SIZE];
	string a = "%.";
	a += ns_to_string(decimal_places);
	a += "f";
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,a.c_str(),k);
	return string()+buf;
}

string ns_to_string_short(const double k,unsigned int decimal_places){
char buf[NS_SNPRINTF_BUFFER_SIZE];
	string a = "%.";
	a += ns_to_string(decimal_places);
	a += "f";
	snprintf(buf,NS_SNPRINTF_BUFFER_SIZE,a.c_str(),k);
	return string()+buf;
}
string ns_ex_type_string(const ns_ex_type & ex){
	switch(ex){
		case ns_standard: return "ns_standard";
		case ns_file_io: return "ns_file_io";
		case ns_network_io: return "ns_network_io";
		case ns_fatal: return "ns_fatal";
		case ns_sql_fatal: return "ns_sql_fatal";
		case ns_memory_allocation: return "ns_memory_allocation";
		default: return "ns_ex_type_string::Unknown exception type!";
	}
}

ns_ex_type ns_ex_type_from_string(const string & str){
	if (str == "ns_standard")
		return ns_standard;
	if (str == "ns_file_io")
		return ns_file_io;
	if (str == "ns_network_io")
		return ns_network_io;
	if (str == "ns_fatal")
		return ns_fatal;
	if (str == "ns_sql_fatal")
		return ns_sql_fatal;
	if(str == "ns_memory_allocation")
		return ns_memory_allocation;
	return ns_standard;
}
const string & ns_to_string(const string & a){
	return a;
}
string ns_to_string(const char * a){
	return a;
}
#ifdef _WIN32 
void ns_ex::append_windows_error(){
	char * a = new char[200];
	FormatMessage(FORMAT_MESSAGE_FROM_SYSTEM,0,GetLastError(),0,a,200,0);
	value+= a;
	delete[] a;
}
#endif


std::string ns_to_lower(const std::string & s){
	std::string ret;
	ret.resize(s.size());
	for (unsigned int i = 0; i < s.size(); i++)
		ret[i] = tolower(s[i]);
	return ret;
}

ns_global_debug_output_handler global_debug_output_handler = 0;
void ns_global_debug(const ns_text_stream_t & t){
	if (global_debug_output_handler == 0){
		cerr << "Global Debug Handler Unset: " << t.text();
		return;
	}
	global_debug_output_handler(t);
}
void ns_set_global_debug_output_handler(ns_global_debug_output_handler handler){
	global_debug_output_handler = handler;
}

