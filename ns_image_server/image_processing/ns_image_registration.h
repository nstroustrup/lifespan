#ifndef NS_IMAGE_REGISTRATION
#define NS_IMAGE_REGISTRATION
#include "ns_image.h"

typedef std::vector<unsigned long> ns_image_registation_profile_dimension;
struct ns_image_registration_profile{
	 ns_image_registation_profile_dimension horizontal_profile,
											vertical_profile;
};

template<int thresh, class ns_component>
class ns_image_registration{
public:
	/*
	template<class sender_t>
	static void generate_vertical_profile(const ns_image_stream_sender<ns_component,sender_t> * r, ns_vertical_image_registration_profile & profile){
		profile.resize(r->properties().height);
		const unsigned long h(r->properties().height),
							w(r->properties().width);

		ns_image_stream_sender::
		light_count[i].resize(h,0);
		for (unsigned int y = 0; y < h; y++){
			for (unsigned int x = 0; x < w; x++)
				light_count[i][y]+=((*im[i])[y][x] >= thresh);
		}
	}
	*/

	static void generate_profiles(const ns_image_whole<ns_component> & r, ns_image_registration_profile & profile){
		profile.horizontal_profile.resize(r.properties().width,0);
		profile.vertical_profile.resize(r.properties().height,0);
		const unsigned long h(r.properties().height),
							w(r.properties().width);
		for (unsigned int y = 0; y < h; y++){
			for (unsigned int x = 0; x < w; x++){
				profile.vertical_profile[y]+=(r[y][x] >= thresh);
				profile.horizontal_profile[x]+=(r[y][x] >= thresh);
			}
		}
	}
	

	static ns_vector_2i register_images(const ns_image_whole<ns_component> & r, const ns_image_whole<ns_component> & a, const ns_vector_2i max_offset = ns_vector_2i(0,0)){
		ns_image_registration_profile profiles[2];

		generate_profiles(r,profiles[0]);
		generate_profiles(a,profiles[1]);
		
		return register_profiles(profiles[0],profiles[1],max_offset);
	}
	static ns_vector_2i register_profiles(const ns_image_registration_profile & r , const ns_image_registration_profile & a,  const ns_vector_2i max_offset = ns_vector_2i(0,0)){
		return ns_vector_2i(register_profile(r.horizontal_profile, a.horizontal_profile, max_offset.x),
						    register_profile(r.vertical_profile, a.vertical_profile,   max_offset.y));
	}
	static int register_profile(const ns_image_registation_profile_dimension & r , const ns_image_registation_profile_dimension & a, const unsigned int max_offset = 0){
	
		//first, we get the vertical "profile" of the image by counting
		//the number of >= threshold pixels in each row.

	
		//now we minimize the distance between the rows.
		unsigned long h(r.size());
		if (h > a.size())
			h = a.size();

		unsigned long minimum_offset_difference = (unsigned long)(-1),
					  minimum_offset = 0;

		int d = (int)max_offset;
		if (d == 0){
			if (h > 10) d = h/10;
			else d = h;
		}

		//test positive offset
		for (int i = 0; i < d; i++){
			unsigned long diff = 0;
			for (unsigned int y = 0; y < h-d; y++)
				diff += (unsigned long)abs((long)r[y]-(long)a[y+i]);
			{
				if (minimum_offset_difference > diff){
					minimum_offset_difference = diff;
					minimum_offset = i;
				}
			}
		}

		//test negative offset
		for (int i = 0; i < d; i++){
			unsigned long diff = 0;
			for (unsigned int y = d; y < h; y++)
				diff += (unsigned long)abs((long)r[y]-(long)a[y-i]);
			if (minimum_offset_difference > diff){
				minimum_offset_difference = diff;
				minimum_offset = -i;
			}
		}
		return minimum_offset;
	}
};

class ns_image_registration_profile_cache{
public:
	ns_image_registration_profile_cache(const unsigned long max_size_in_mb ):size(0),max_size(max_size_in_mb){}
	ns_image_registration_profile * get(unsigned long id){
		cache_type::iterator p(cache.find(id));
		if (p != cache.end())
			return &p->second;
		return 0;
	}
	void insert(unsigned long id,const ns_image_registration_profile & profile){
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
	typedef std::map<unsigned long,ns_image_registration_profile> cache_type;
	std::map<unsigned long,ns_image_registration_profile> cache;
	unsigned long size,
		max_size;
};



#endif
