#ifndef NS_HP_TIMER
#define NS_HP_TIMER
#include "ns_ex.h"

#ifdef _WIN32 
#include <sys/timeb.h>
#endif
class ns_high_precision_timer{
public:
	void start(){
		#ifdef _WIN32 
		if (_ftime64_s(&start_time)!=0)
			throw ns_ex("ns_high_precision_timer::start():Could not start timer");
		#else
		if (clock_gettime(CLOCK_REALTIME,&start_time)!= 0)
			throw ns_ex("ns_high_precision_timer::start():Could not start timer");
		#endif
	}
	//returns the time between start() and stop() in microseconds
	ns_64_bit stop(){
		#ifdef _WIN32 
		__timeb64 d(start_time);
		start();
		if (d.millitm > start_time.millitm){
			d.time = start_time.time - d.time - 1;
			d.millitm = (unsigned short)((1000+(int)start_time.millitm) - (int)d.millitm);
		}
		else{
			d.time = start_time.time - d.time;
			d.millitm = start_time.millitm - d.millitm;
		}
		#else
		timespec d(start_time);
		start();
		if (d.tv_nsec > start_time.tv_nsec){
			d.tv_sec = start_time.tv_sec - d.tv_sec - 1;
			d.tv_nsec = 1000000000+start_time.tv_nsec - d.tv_nsec;
		}
		else{
			d.tv_sec = start_time.tv_sec - d.tv_sec;
			d.tv_nsec = start_time.tv_nsec - d.tv_nsec;
		}
		#endif
		return to_microsecond(d);
	}
	ns_64_bit absolute_time(){
		start();
		return to_microsecond(start_time);
	}
private:
	#ifdef _WIN32 
	static ns_64_bit to_microsecond(const __timeb64 & s){
		return 1000*1000*(ns_64_bit)s.time+1000*(ns_64_bit)s.millitm;
	}
	__timeb64 start_time;
	#else
	static ns_64_bit to_microsecond(const timespec & s){
		return  1000*1000*(ns_64_bit)s.tv_sec + (ns_64_bit)s.tv_nsec/1000;
	}
	timespec start_time;
	#endif
};

#endif
