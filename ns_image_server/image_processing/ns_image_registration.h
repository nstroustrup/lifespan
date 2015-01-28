#ifndef NS_IMAGE_REGISTRATION
#define NS_IMAGE_REGISTRATION
#include "ns_image.h"
#include "ns_image_registration_cache.h"


template<int thresh, class ns_component>
class ns_image_registration{
public:
	
	static void generate_profiles(const ns_image_whole<ns_component> & r, ns_image_registration_profile & profile,const ns_registration_method & method,const unsigned long downsampling_factor=0){
		profile.registration_method = method;
		if (method == ns_full_registration){
			r.pump(profile.whole_image,1024*1024);
			if (downsampling_factor == 0){
				profile.downsampling_factor= sqrt((double)((r.properties().width*(ns_64_bit)r.properties().height)/(500*500)));
				if (profile.downsampling_factor > 8)
					profile.downsampling_factor = 8;
			}
			else profile.downsampling_factor=downsampling_factor;
			ns_image_properties prop(r.properties());
			prop.width=prop.width/profile.downsampling_factor;
			prop.height=prop.height/profile.downsampling_factor;
			r.resample(prop,profile.downsampled_image);
			if (profile.downsampling_factor > 4){
				prop.width=r.properties().width/4;
				prop.height=r.properties().height/4;
				r.resample(prop,profile.downsampled_image_2);
			}
			return;
		}
		profile.horizontal_profile.resize(0);
		profile.vertical_profile.resize(0);
		
		profile.horizontal_profile.resize(r.properties().width,0);
		profile.vertical_profile.resize(r.properties().height,0);
		profile.average = 0;

		const unsigned long h(r.properties().height),
							w(r.properties().width);
		if (method == ns_threshold_registration){
			for (unsigned int y = 0; y < h; y++){
				for (unsigned int x = 0; x < w; x++){
					profile.vertical_profile[y]+=(r[y][x] >= thresh);
					profile.horizontal_profile[x]+=(r[y][x] >= thresh);
				}
			}
		}
		else if (method == ns_sum_registration || method == ns_compound_registration){
			ns_64_bit average(0);
			for (unsigned int y = 0; y < h; y++){
				for (unsigned int x = 0; x < w; x++){
					profile.vertical_profile[y]+=(r[y][x]);
					profile.horizontal_profile[x]+=(r[y][x]);
					profile.average+=r[y][x];
				}
			}
			profile.average = profile.average/(ns_64_bit)(w*h);
			if (method == ns_compound_registration)
				r.pump(profile.whole_image,1024*1024);
		}
		else throw ns_ex("Unknown registration method");
	}


	static ns_vector_2i register_images(const ns_image_whole<ns_component> & r, const ns_image_whole<ns_component> & a, const ns_registration_method & registration_method,const ns_vector_2i max_offset = ns_vector_2i(0,0)){
		
		
		ns_image_registration_profile profiles[2];

		generate_profiles(r,profiles[0],registration_method);
		generate_profiles(a,profiles[1],registration_method,profiles[0].downsampling_factor);

		if(registration_method == ns_full_registration)
			return register_full_images(profiles[0],profiles[1],max_offset);

		//ofstream out1(std::string("c:\\server\\horizontal_profiles_") + ns_registration_method_string(registration_method) + "_.csv");
		//out1 << "name,direction,position,value,value_normalized\n";
		//write_profile("r",profiles[0],out1);
		//write_profile("a",profiles[1],out1);
		//out1.close();

		if (registration_method == ns_threshold_registration || registration_method == ns_sum_registration)
			return register_profiles(profiles[0],profiles[1],max_offset,ns_registration_method_string(registration_method));

		if (registration_method == ns_compound_registration){
			ns_vector_2i approximate_registration(register_profiles(profiles[0],profiles[1],max_offset,ns_registration_method_string(registration_method)+"_first_step"));
			ns_vector_2i local_registration(7,7);
			return register_whole_images(profiles[0].whole_image,profiles[1].whole_image,
					approximate_registration-local_registration,
					approximate_registration+local_registration);
		}
		throw ns_ex("Unknown registration method");
	}

	static ns_vector_2i register_full_images(const ns_image_registration_profile & r , const ns_image_registration_profile & a,  ns_vector_2i max_offset = ns_vector_2i(0,0), const std::string & debug_name=""){
		if (r.downsampling_factor != a.downsampling_factor)
			throw ns_ex("Downsampling factor mismatch");
		if (max_offset == ns_vector_2i(0,0))
			max_offset = ns_vector_2i(r.whole_image.properties().width/5,r.whole_image.properties().height/5);
		ns_vector_2i downsampled_max_offset(max_offset/r.downsampling_factor);

		ns_vector_2i downsampled_shift(register_whole_images(r.downsampled_image,a.downsampled_image,downsampled_max_offset*-1,downsampled_max_offset));
		
		downsampled_shift = downsampled_shift*r.downsampling_factor;
		ns_vector_2i downsample_v(r.downsampling_factor,r.downsampling_factor);

		if (r.downsampling_factor > 4){
			ns_vector_2i downsampled_shift_2(
					register_whole_images(r.downsampled_image_2,a.downsampled_image_2,
						(downsampled_shift-downsample_v)/4,(downsampled_shift+downsample_v)/4));

			downsampled_shift_2 = downsampled_shift*4;
			ns_vector_2i downsample_v(4,4);
			return register_whole_images(r.whole_image,a.whole_image,downsampled_shift_2-downsample_v,downsampled_shift_2+downsample_v);
		}
		return register_whole_images(r.whole_image,a.whole_image,downsampled_shift-downsample_v,downsampled_shift+downsample_v);

	}
	static ns_vector_2i register_whole_images(const ns_image_whole<ns_component> & r, const ns_image_whole<ns_component> & a, const ns_vector_2i offset_minimums,const ns_vector_2i offset_maximums){
		
		unsigned long h(r.properties().height);
		if (h > a.properties().height)
			h = a.properties().height;
		unsigned long w(r.properties().width);
		if (w > a.properties().width)
			w = a.properties().width;

		ns_vector_2i minimum_offset(0,0);
		ns_64_bit minimum_offset_difference((ns_64_bit)-1);

		int x_distance_from_edge(max(abs(offset_minimums.x),abs(offset_maximums.x)));
		int y_distance_from_edge(max(abs(offset_minimums.y),abs(offset_maximums.y)));

		for (int dy = offset_minimums.y; dy < offset_maximums.y; dy++){
			for (int dx = offset_minimums.x; dx < offset_maximums.x; dx++){
				ns_64_bit diff = 0;
				for (unsigned int y = y_distance_from_edge; y < h-y_distance_from_edge; y++){
					for (unsigned int x = x_distance_from_edge; x < w-x_distance_from_edge; x++){
						diff+=(ns_64_bit)abs((int)r[y][x]-(int)a[y+dy][x+dx]);
					}
				}
				if (minimum_offset_difference > diff){
					minimum_offset_difference = diff;
					minimum_offset = ns_vector_2i(dx,dy);
				}
			}
		}
		return minimum_offset;
	}
	
	static ns_vector_2i register_profiles(const ns_image_registration_profile & r , const ns_image_registration_profile & a,  const ns_vector_2i max_offset = ns_vector_2i(0,0), const std::string & debug_name=""){
		if (r.registration_method == ns_full_registration){
			return register_full_images(r,a,max_offset);
		}
		return ns_vector_2i(register_profile(r.horizontal_profile, a.horizontal_profile,r.registration_method,max_offset.x,r.average,a.average,debug_name+"_horiz"),
						    register_profile(r.vertical_profile, a.vertical_profile,r.registration_method,   max_offset.y,r.average,a.average,debug_name+"_vert"));
	
	}
	static int register_profile(const ns_image_registation_profile_dimension & r , const ns_image_registation_profile_dimension & a, const ns_registration_method & registration_method,const unsigned int max_offset = 0,const ns_64_bit &r_avg=1,const ns_64_bit & a_avg=1,const std::string & debug_filename=""){
		
		//now we minimize the distance between the rows.
		unsigned long h(r.size());
		if (h > a.size())
			h = a.size();

		int minimum_offset = 0;

		int d = (int)max_offset;
		if (d == 0){
			if (h > 10) d = h/10;
			else d = h;
		}
		//ofstream o(std::string("c:\\server\\alignments_")+debug_filename + ".csv");
		//o << "offset,diff\n";
		//ofstream o2(std::string("c:\\server\\distance_")+debug_filename + ".csv");
		//o2 << "registration_shift,x_position,diff,cumulative_diff,r,a\n";
		if (registration_method == ns_sum_registration || registration_method == ns_compound_registration){
			
			double minimum_offset_difference = DBL_MAX;
			const double aavg(a_avg),
				   ravg(r_avg);
			//test positive offset
			for (int i = 0; i < d; i++){
				double diff = 0;
				for (unsigned int y = d; y < h-d; y++){
					diff += fabs(r[y]/ravg-a[y+i]/aavg);
			//		o2 << i << "," << y << "," <<  fabs(r[y]/ravg-a[y+i]/aavg) << ","  << diff <<","<< r[y]/ravg <<","<< a[y+i]/aavg << "\n";
				}
			//	o << i << "," << diff << "\n";
				if (minimum_offset_difference > diff){
					minimum_offset_difference = diff;
					minimum_offset = i;
				}
			}
			//test negative offset
			for (int i = 1; i < d; i++){
				double diff = 0;
				for (unsigned int y = d; y < h-d; y++){
					diff +=fabs(r[y]/ravg-a[y-i]/aavg);
			//		o2 << -i << "," << y << "," <<  fabs(r[y]/ravg-a[y-i]/aavg) << ","  << diff <<  ","<<r[y]/ravg <<","<< a[y-i]/aavg << "\n";
				}
			//	o << -i << "," << diff << "\n";
				if (minimum_offset_difference > diff){
					minimum_offset_difference = diff;
					minimum_offset = -i;
				}
			}
		//	o.close();
		//	o2.close();
		}
		else{
			ns_64_bit minimum_offset_difference = (ns_64_bit)(-1);

			//test positive offset
			for (int i = 0; i < d; i++){
				ns_64_bit diff = 0;
				for (unsigned int y = d; y < h-d; y++)
					diff += (ns_64_bit)ns_64_bit_abs((ns_s64_bit)r[y]-(ns_s64_bit)a[y+i]);
				
			//	o << i << "," << diff << "\n";
				if (minimum_offset_difference > diff){
					minimum_offset_difference = diff;
					minimum_offset = i;
				}
			}
			//test negative offset
			for (int i = 0; i < d; i++){
				ns_64_bit diff = 0;
				for (unsigned int y = d; y < h-d; y++)
					diff += (ns_64_bit)ns_64_bit_abs((ns_s64_bit)r[y]-(ns_s64_bit)a[y-i]);
				
			//	o << -i << "," << diff << "\n";
				if (minimum_offset_difference > diff){
					minimum_offset_difference = diff;
					minimum_offset = -i;
				}
			}


		}
		return minimum_offset;
	}


	static void write_profile(const std::string & name, const ns_image_registration_profile & profile, std::ostream & out){
		for (unsigned long i = 0; i < profile.horizontal_profile.size();i++)
			out << name << ",horiz," << i << "," << profile.horizontal_profile[i] << "," << profile.horizontal_profile[i]/(double)profile.average << "\n";
		for (unsigned long i = 0; i < profile.vertical_profile.size();i++)
			out << name << ",vertical," << i << "," << profile.vertical_profile[i] << "," << profile.vertical_profile[i]/(double)profile.average << "\n";
	}
	
	static std::string ns_registration_method_string(const ns_registration_method & reg){
		switch(reg){
		case ns_no_registration: return "none";
		case ns_threshold_registration: return "threshold";
		case ns_sum_registration:	return "sum";
		case ns_full_registration: return "full";
		case ns_compound_registration: return "compound";
		default: throw ns_ex("Unknown");
		}
	}
};

#endif
