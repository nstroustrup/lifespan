#ifndef NS_HP_TIMER
#define NS_HP_TIMER
#include "ns_ex.h"

#ifdef _WIN32 
#include <sys/timeb.h>
#elif defined __MACH__
#include <mach/mach.h>
#include <mach/mach_time.h>
#include <unistd.h>
#endif
class ns_high_precision_timer{
public:
	void start(){
		#ifdef _WIN32 
		// TODO: Should really use QueryPerformanceFrequency instead of 
		// system time which is not guaranteed to be monotonic.
		// See. e.g. https://github.com/awreece/monotonic_timer/blob/master/monotonic_timer.c
		if (_ftime64_s(&start_time)!=0)
			throw ns_ex("ns_high_precision_timer::start():Could not start timer");
		#elif defined __MACH__
		// OS X: https://developer.apple.com/library/mac/qa/qa1398/_index.html
		start_time = mach_absolute_time();
		#else
		// should use monotonic clock for calculating time diffs.
		// CLOCK_REALTIME can jump back and forth with NTP updates.
		if (clock_gettime(CLOCK_MONOTONIC,&start_time)!= 0)
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
		#elif defined __MACH__
		uint64_t d(start_time);
		start();
		d -= start_time;
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
	#elif defined __MACH__
	static ns_64_bit to_microsecond(const uint64_t & s){
		static double numer = 0;
		static double denom = 0;
		// If this is the first time we've run, get the timebase.
	    if ( numer == 0 ) {
			mach_timebase_info_data_t sTimebaseInfo;
			mach_timebase_info(&sTimebaseInfo);
			numer = (double) sTimebaseInfo.numer;
			denom = (double) sTimebaseInfo.denom;
	    }
	    double elapsedNano =  (double) s * numer / denom;
		return (ns_64_bit) elapsedNano / 1000;
	}
	uint64_t start_time;
	#else
	static ns_64_bit to_microsecond(const timespec & s){
		return  1000*1000*(ns_64_bit)s.tv_sec + (ns_64_bit)s.tv_nsec/1000;
	}
	timespec start_time;
	#endif
};

#endif
