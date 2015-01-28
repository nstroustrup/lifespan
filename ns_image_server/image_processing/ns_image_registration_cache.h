#ifndef NS_IMAGE_REGISTRATION_CACHE
#define NS_IMAGE_REGISTRATION_CACHE
#include "ns_image.h"

typedef std::vector<ns_64_bit> ns_image_registation_profile_dimension;
typedef enum {ns_no_registration,ns_threshold_registration,ns_sum_registration,ns_full_registration,ns_compound_registration} ns_registration_method;
struct ns_image_registration_profile{
	 ns_image_registration_profile():registration_method(ns_no_registration),average(0),downsampling_factor(0){}
	 ns_image_registation_profile_dimension horizontal_profile,
											vertical_profile;
	 ns_64_bit average;
	 ns_image_standard whole_image,
					downsampled_image,
					downsampled_image_2;
	 unsigned long downsampling_factor;
	 ns_registration_method registration_method;
};



class ns_image_registration_profile_cache{
public:
	ns_image_registration_profile_cache(const ns_64_bit max_size_in_mb ):size(0),max_size(max_size_in_mb){}
	ns_image_registration_profile * get(const ns_64_bit & id){
		cache_type::iterator p(cache.find(id));
		if (p != cache.end())
			return &p->second;
		return 0;
	}
	void insert(ns_64_bit id,const ns_image_registration_profile & profile){
		if (size+profile.vertical_profile.size() > max_size*1024*1024){
			cache.clear();
			size = 0;
		}
		size+=profile.vertical_profile.size();
		cache[id] = profile;
	}
	void clear(){
		cache.clear();
	}
private:
	typedef std::map<ns_64_bit,ns_image_registration_profile> cache_type;
	std::map<ns_64_bit,ns_image_registration_profile> cache;
	ns_64_bit size,
		max_size;
};


#endif
