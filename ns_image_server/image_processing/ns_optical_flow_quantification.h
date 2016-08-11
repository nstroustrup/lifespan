#ifndef NS_OPTICAL_FLOW_QUANTIFICATION
#define NS_OPTICAL_FLOW_QUANTIFICATION
#include "ns_image.h"

template<class ns_component>
class ns_optical_flow_accessor_val {
public:
	ns_optical_flow_accessor_val(const ns_image_whole<ns_component>  &d) :im(&d) {}
	const ns_component & val(unsigned long x, unsigned long y)const { return (*im)[y][x]; }
	const ns_image_whole<ns_component> * im;
};
template<class ns_component>
class ns_optical_flow_accessor_scaled_val {
public:
	ns_optical_flow_accessor_scaled_val(const ns_image_whole<ns_component>  &d_,
		const ns_image_whole<ns_8_bit>  &im1_, const ns_image_whole<ns_8_bit>  &im2_) :d(&d_), im1(&im1_), im2(&im2_) {}
	const ns_component val(unsigned long x, unsigned long y)const { return (((*im1)[y][x] > ((*im2)[y][x]) ? (*im1)[y][x] : (*im2)[y][x]))*(*d)[y][x]; }
	const ns_image_whole<ns_component> * d;
	const ns_image_whole<ns_8_bit> *im1, *im2;
};
template<class ns_component>
class ns_optical_flow_accessor_mag {
public:
	ns_optical_flow_accessor_mag(const ns_image_whole<ns_component>  &dx_, const ns_image_whole<ns_component>  &dy_) :dx(&dx_), dy(&dy_) {}
	const ns_component val(unsigned long x, unsigned long y)const { return sqrt(pow((*dx)[y][x], 2) + pow((*dy)[y][x], 2)); }
	const ns_image_whole<ns_component> * dx, *dy;
};
template<class ns_component>
class ns_optical_flow_accessor_diff {
public:
	ns_optical_flow_accessor_diff(const ns_image_whole<ns_component>  &t1_, const ns_image_whole<ns_component>  &t2_) :t1(&t1_), t2(&t2_) {}
	const float val(unsigned long x, unsigned long y)const { return (float)(*t2)[y][x] - (float)(*t1)[y][x]; }
	const ns_image_whole<ns_component> * t1, *t2;
};

class ns_optical_flow_accessor_scaled_movement {
public:
	class ns_optical_flow_accessor_scaled_movement(const ns_image_whole<float>  &dx_, const ns_image_whole<float>  &dy_,
		const ns_image_whole<ns_8_bit>  &im1_, const ns_image_whole<ns_8_bit>  &im2_) :dx(&dx_), dy(&dy_), im1(&im1_), im2(&im2_) {}
	const float val(unsigned long x, unsigned long y)const {
		return (((*im1)[y][x]>((*im2)[y][x]) ? (*im1)[y][x] : (*im2)[y][x]))
			*sqrt(pow((*dx)[y][x], 2) + pow((*dy)[y][x], 2));
	}
	const ns_image_whole<float> *dx, *dy;
	const ns_image_whole<ns_8_bit> *im1, *im2;
};

struct ns_optical_flow_quantification {
	template<class T, class ns_mask_source_t>
	void calculate(const ns_image_properties & p, const T &d, const long border, const ns_mask_source_t & mask) {
		d_max = -FLT_MIN;
		d_min = FLT_MAX;
		d_sum = 0;
		std::vector<float> vals;
		vals.reserve(p.height*p.width);
		unsigned long pixel_num(0);
		for (unsigned long y = border; y < p.height - border; y++) {
			for (unsigned long x = border; x < p.width - border; x++) {
				if (!mask.mask(y, x))
					continue;
				double v(fabs(d.val(x, y)));
				if (d_max < v)
					d_max = v;
				if (d_min > v)
					d_min = v;
				d_sum += v;
				vals.push_back(v);
				pixel_num++;
			}
		}
		if (pixel_num != 0) {
			d_avg = d_sum / (pixel_num);
			std::sort(vals.begin(), vals.end());
			d_median = vals[vals.size() / 2];
			d_95th_percentile = vals[floor(vals.size() *.95)];
			d_75th_percentile = vals[floor(vals.size() *.75)];
		}
		else {
			d_avg = 0;
			d_median = 0;
			d_95th_percentile = 0;
			d_75th_percentile = 0;
		}
	}
	double d_max, d_min, d_avg, d_sum, d_median, d_75th_percentile, d_95th_percentile;
	static void write_header(const std::string & name, std::ostream & out) {
		out << name << " max,"
			<< name << " min,"
			<< name << " avg,"
			<< name << " median,"
			<< name << " 75th_percentile,"
			<< name << " 95th_percentile";
	}
	void write(std::ostream & out) const;
	void read(std::istream & in) ;
	ns_optical_flow_quantification square_root();
	ns_optical_flow_quantification square();
	void zero();
};

ns_optical_flow_quantification operator+(const ns_optical_flow_quantification & a, const ns_optical_flow_quantification & b);
ns_optical_flow_quantification operator/(const ns_optical_flow_quantification & a, const float & d);

#endif
