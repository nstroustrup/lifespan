#ifndef NS_BSPLINE_H
#define NS_BSPLINE_H
#include "ns_ex.h"
#include "ns_vector.h"

class ns_bspline{
public:
	typedef enum {ns_very_low,ns_low,ns_medium,ns_high} ns_smoothness_level;
	void calculate_with_standard_params(const std::vector<ns_vector_2d> & data,unsigned int output_size,const ns_smoothness_level smoother);
	void calculate_with_standard_params(const std::vector<double> & x, const std::vector<double> & y, unsigned int output_size, const ns_smoothness_level smoother);
	void calculate_intersecting_all_points(const unsigned int degree, const bool open_spline, const bool connect_spline_endpoints, const std::vector<ns_vector_2d> & data,unsigned int output_size);
	void calculate_intersecting_all_points(const unsigned int degree, const bool open_spline, const bool connect_spline_endpoints, const std::vector<double> & x, const std::vector<double> & y, unsigned int output_size);
	void calculate_intersecting_all_points(const unsigned int degree, const bool open_spline, const bool connect_spline_endpoints, const unsigned long data_count, const void * data, unsigned int output_size);
	
	//returns the number of nodes that were cropped off the front of the positions array.
	//the number of nodes cropped off the end can be deduced from the size of the positions array.
	unsigned long  crop_ends(const double crop_fraction);

	std::vector<ns_vector_2d> positions;
	std::vector<ns_vector_2d> tangents;
	std::vector<double> curvature;
	double length;
	const unsigned int degree_used(){return degree_used_;}
private:

	void calculate_with_standard_params(const float * dat, const unsigned long data_size, unsigned int output_size, const ns_smoothness_level smoother);
	unsigned int degree_used_;
};

#endif
