#ifndef NS_OPTICAL_FLOW
#define NS_OPTICAL_FLOW
#include "ns_image.h"
#include "OpticalFlow.h"

class ns_optical_flow {
public:

	void set_Dim_properties(const ns_image_properties & p,DImage & dim){
		if (dim.width() != p.width || dim.height() != p.height || dim.nchannels() != p.components)
			dim.allocate(p.width,p.height,p.components);
	}
	void image_to_DImage(const ns_image_standard & im, DImage & dim){
		const ns_image_properties p(im.properties());
		set_Dim_properties(p,dim);
		if (p.components == 1){
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					dim.data()[x+p.width*y] = im[y][x]/(double)255;
				}
		}else{
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					dim.data()[x+3*p.width*y] = im[y][x]/(double)255;
					dim.data()[x+1+3*p.width*y] = im[y][x+1]/(double)255;
					dim.data()[x+2+3*p.width*y] = im[y][x+2]/(double)255;
				}
		}
	};

	void image_from_DImage(const DImage & dim, ns_image_standard & im){
		ns_image_properties p;
		p.width = dim.width();
		p.height = dim.height();
		p.components = dim.nchannels();
		im.init(p);
		if (p.components == 1){
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					im[y][x] = (ns_8_bit)(255*dim.data()[x+p.width*y]);
				}
		}else{
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					im[y][x] = (ns_8_bit)(255*dim.data()[x+3*p.width*y]);
					im[y][x+1] = (ns_8_bit)(255*dim.data()[x+1+3*p.width*y]);
					im[y][x+2] = (ns_8_bit)(255*dim.data()[x+2+3*p.width*y]);
				}
		}
	};
	void image_from_DImage(const DImage & dim, ns_image_whole<ns_16_bit> & im){
		ns_image_properties p;
		p.width = dim.width();
		p.height = dim.height();
		p.components = dim.nchannels();
		im.init(p);
		if (p.components == 1){
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					im[y][x] = floor(dim.data()[x+p.width*y]*USHRT_MAX);
				}
		}else{
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					im[y][x] = floor(dim.data()[x+3*p.width*y]*USHRT_MAX);
					im[y][x+1] = floor(dim.data()[x+1+3*p.width*y]*USHRT_MAX);
					im[y][x+2] = floor(dim.data()[x+2+3*p.width*y]*USHRT_MAX);
				}
		}
	};

	void image_from_DImage(const DImage & dim, ns_image_whole<double> & im){
		ns_image_properties p;
		p.width = dim.width();
		p.height = dim.height();
		p.components = dim.nchannels();
		im.init(p);
		if (p.components == 1){
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					im[y][x] = dim.data()[x+p.width*y];
				}
		}else{
			for (unsigned long y = 0; y < p.height; y++)
				for (unsigned long x = 0; x < p.width; x++){
					im[y][x] = dim.data()[x+3*p.width*y];
					im[y][x+1] = dim.data()[x+1+3*p.width*y] ;
					im[y][x+2] = dim.data()[x+2+3*p.width*y];
				}
		}
	};

	void calculate(const ns_image_standard & im1, 
					const ns_image_standard & im2,
					double alpha= 1,
					double ratio=0.5,
					int minWidth= 40,
					int nOuterFPIterations = 3,
					int nInnerFPIterations = 1,
					int nSORIterations= 20){
		image_to_DImage(im1,Dim1);
		image_to_DImage(im2,Dim2);
		OpticalFlow::Coarse2FineFlow(vx,vy,warpI2,Dim1,Dim2,alpha,ratio,minWidth,nOuterFPIterations,nInnerFPIterations,nSORIterations);
	}
	void calculate(double alpha= 1,
					double ratio=0.5,
					int minWidth= 40,
					int nOuterFPIterations = 3,
					int nInnerFPIterations = 1,
					int nSORIterations= 20){
		OpticalFlow::Coarse2FineFlow(vx,vy,warpI2,Dim1,Dim2,alpha,ratio,minWidth,nOuterFPIterations,nInnerFPIterations,nSORIterations);
	}

	DImage vx,vy,warpI2;
	DImage Dim1,Dim2;
};
#endif
