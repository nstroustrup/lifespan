#ifndef NS_PROGRESS_REPORTER
#define NS_PROGRESS_REPORTER
#include <iostream>

class ns_progress_reporter{
public:
	template<class T>
	ns_progress_reporter(const T total_number_of_items, const unsigned long number_of_intervals):count_((unsigned long)total_number_of_items),number_of_intervals_(number_of_intervals){
		absolute_step_ = count_/(unsigned long)number_of_intervals;
		if (absolute_step_ == 0)
			absolute_step_ = count_;
		last_step = 0;
		intervals_used = 0;
	}

	template<class T>
	void inline operator()(const T i){
		output_stats(i);
	}
	//to suppress type conversion problems
	void inline operator()(const size_t i){output_stats((unsigned long)i);}
private:
	unsigned long number_of_intervals_,count_,absolute_step_,last_step,display_rounding_factor,intervals_used;
	
	template<class T>
	void output_stats(const T i){
		if ((unsigned long)i >= count_){std::cout << "100%\n";return;}
		if ((unsigned long)i - last_step >=absolute_step_){
			std::cout << (100/number_of_intervals_)*intervals_used << "% ";
			last_step = (unsigned long)i;
			intervals_used++;
		}
	}
};



#endif
