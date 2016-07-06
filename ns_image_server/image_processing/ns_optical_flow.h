#ifndef NS_OPTICAL_FLOW
#define NS_OPTICAL_FLOW
#include "ns_image.h"
class ns_external_image;

class ns_optical_flow {
public:
	ns_optical_flow();
	~ns_optical_flow();
	void set_Dim_properties(const ns_image_properties & p,ns_external_image * dim);
	void image_to_DImage(const ns_image_standard & im, ns_external_image* dim);

	void image_from_DImage(const ns_external_image * dim, ns_image_standard & im);
	void image_from_DImage(const ns_external_image * dim, ns_image_whole<ns_16_bit> & im);

	void image_from_DImage(const ns_external_image * dim, ns_image_whole<double> & im);

	void calculate(const ns_image_standard & im1, 
					const ns_image_standard & im2,
					double alpha= 1,
					double ratio=0.5,
					int minWidth= 40,
					int nOuterFPIterations = 3,
					int nInnerFPIterations = 1,
					int nSORIterations= 20);
	void calculate(double alpha= 1,
					double ratio=0.5,
					int minWidth= 40,
					int nOuterFPIterations = 3,
					int nInnerFPIterations = 1,
					int nSORIterations= 20);

	ns_external_image *vx,*vy,*warpI2;
	ns_external_image *Dim1,*Dim2;
	void test();
};
#endif
