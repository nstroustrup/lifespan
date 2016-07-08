#ifndef NS_OPTICAL_FLOW
#define NS_OPTICAL_FLOW
#include "ns_image.h"
class ns_external_image_t;
class ns_flow_processor_storage_t;
class ns_external_image {
public:
	ns_external_image();
	~ns_external_image();
	float * buffer();
	const float * buffer() const;
	void from(const ns_image_standard & i);
	void init(const ns_image_properties & p);
	ns_external_image_t * image;
};

class ns_optical_flow {
public:
	ns_optical_flow();
	~ns_optical_flow();

	static void test();
	void calculate(const int num_it=20, float gaussian_stdev=3);
	void get_movement(ns_image_whole<float> & vx, ns_image_whole<float> & vy);
	void get_movement(ns_image_whole<ns_16_bit> & vx, ns_image_whole<ns_16_bit> & vy);
	ns_external_image Dim1,Dim2;
	ns_flow_processor_storage_t * flow_processor;
};
#endif
