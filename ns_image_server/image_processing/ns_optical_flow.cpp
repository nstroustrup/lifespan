
#include "OpticalFlow.h"
#include "ns_optical_flow.h"
#include "ns_image_easy_io.h"

class ns_external_image{ public: Image<double> im; };


ns_optical_flow::ns_optical_flow(){
	vx = new ns_external_image;
	vy = new ns_external_image;
	warpI2 = new ns_external_image;
	Dim1 = new ns_external_image;
	Dim2 = new ns_external_image;
	
}
ns_optical_flow::~ns_optical_flow(){
	delete vx;
	delete vy;
	delete warpI2;
	delete Dim1;
	delete Dim2;
}
void ns_optical_flow::test(){
	int n(8);
	vector<ns_image_standard> im(n);
	std::string dir("y:\\debug\\select\\");
	std::string fn("im_7_");
	std::string fn2("df.tif");
	ns_image_standard tmp;
	ns_image_properties p;
	for (unsigned int i = 0; i < n; i++){
		ns_load_image(dir+fn+ns_to_string(i)+fn2,im[i]);
		p = im[i].properties();
		p.components = 1;
		im[i].pump(tmp,1024);
		im[i].prepare_to_recieve_image(p);
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				im[i][y][x] = tmp[y][3*x+1];
			}
		}
		ns_save_image(dir+"foo_"+ns_to_string(i)+".tif",im[i]);
	}




	ns_image_properties p2(p);
	p2.height = p.height*(n-1);
	p2.width = p.width*4;
	p2.components = 3;
	ns_optical_flow flow;
	ns_image_whole<ns_16_bit> out,dx,dy,mag,warp;
	out.prepare_to_recieve_image(p2);
	for (unsigned int i = 1; i < n; i++){
		//copy image diff
		/*for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*x] = 0;
				out[y+p.height*(i-1)][3*x+1] = 0;//im[i][y][x]*(USHRT_MAX/255);
				out[y+p.height*(i-1)][3*x+2] = 0;
			}
		}
		for (unsigned long y = 0; y < p.height;y++){
				out[y+p.height*(i-1)][3*y] = USHRT_MAX;
				out[y+p.height*(i-1)][3*y+1] = 0;//im[i][y][x]*(USHRT_MAX/255);
				out[y+p.height*(i-1)][3*y+2] = 0;
			}
		*/
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*x] = (ns_16_bit)(im[i-1][y][x])*(ns_16_bit)(USHRT_MAX/255);
				out[y+p.height*(i-1)][3*x+1] = im[i][y][x]*(USHRT_MAX/255);
				out[y+p.height*(i-1)][3*x+2] = 0;
			}
		}
		
		flow.image_to_DImage(im[i-1],flow.Dim1);
		flow.image_to_DImage(im[i],flow.Dim2);
		flow.calculate(1,.6,8,5,2,10);
		flow.image_from_DImage(flow.vx,dx);
		flow.image_from_DImage(flow.vy,dy);
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*(p.width+x)] = fabs(10*flow.vx->im.data()[p.width*y+x]*USHRT_MAX);
				out[y+p.height*(i-1)][3*(p.width+x)+1] = fabs(10*flow.vy->im.data()[p.width*y+x]*USHRT_MAX);
				out[y+p.height*(i-1)][3*(p.width+x)+2] = 0;
			}
		}
		mag.prepare_to_recieve_image(p);
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				mag[y][x] = floor(10*sqrt(pow(flow.vx->im.data()[p.width*y+x],2)+pow(flow.vy->im.data()[p.width*y+x],2))*USHRT_MAX);
			}
		}
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*(2*p.width+x)] = mag[y][x];
				out[y+p.height*(i-1)][3*(2*p.width+x)+1] =mag[y][x];
				out[y+p.height*(i-1)][3*(2*p.width+x)+2] = mag[y][x];
			}
		}
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*(3*p.width+x)] = flow.warpI2->im.data()[1*(p.width*y+x)]*USHRT_MAX;
				out[y+p.height*(i-1)][3*(3*p.width+x)+1] =flow.warpI2->im.data()[1*(p.width*y+x)+1]*USHRT_MAX;
				out[y+p.height*(i-1)][3*(3*p.width+x)+2] = flow.warpI2->im.data()[1*(p.width*y+x)+2]*USHRT_MAX;
			}
		}
	}
	ns_tiff_image_output_file<ns_16_bit> tiff_out(ns_tiff_compression_none);
	ns_image_stream_file_sink<ns_16_bit> file_sink(dir + "out2.tif",tiff_out,1024);
	out.pump(file_sink,1024);
	exit(0);
}


void ns_optical_flow::set_Dim_properties(const ns_image_properties & p,ns_external_image * dim){
	if (dim->im.width() != p.width || dim->im.height() != p.height || dim->im.nchannels() != p.components)
		dim->im.allocate(p.width,p.height,p.components);
}
void ns_optical_flow::image_to_DImage(const ns_image_standard & im, ns_external_image * dim){
	const ns_image_properties p(im.properties());
	set_Dim_properties(p,dim);
	if (p.components == 1){
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < p.width; x++){
				dim->im.data()[x+p.width*y] = im[y][x]/(double)255;
			}
	}else{
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < 3*p.width; x++){
				dim->im.data()[x+3*p.width*y] = im[y][x]/(double)255;
			}
	}
};

void ns_optical_flow::image_from_DImage(const ns_external_image * dim, ns_image_standard & im){
	ns_image_properties p;
	p.width = dim->im.width();
	p.height = dim->im.height();
	p.components = dim->im.nchannels();
	im.init(p);
	if (p.components == 1){
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < p.width; x++){
				im[y][x] = (ns_8_bit)(255*dim->im.data()[x+p.width*y]);
			}
	}else{
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < 3*p.width; x++){
				im[y][x] = (ns_8_bit)(255*dim->im.data()[x+3*p.width*y]);
			}
	}
};
void ns_optical_flow::image_from_DImage(const ns_external_image * dim, ns_image_whole<ns_16_bit> & im){
	ns_image_properties p;
	p.width = dim->im.width();
	p.height = dim->im.height();
	p.components = dim->im.nchannels();
	im.init(p);
	if (p.components == 1){
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < p.width; x++){
				im[y][x] = (ns_16_bit)floor(dim->im.data()[x+p.width*y]*USHRT_MAX);
			}
	}else{
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < 3*p.width; x++){
				im[y][x] = (ns_16_bit)floor(dim->im.data()[x+3*p.width*y]*USHRT_MAX);
			}
	}
};

void ns_optical_flow::image_from_DImage(const ns_external_image * dim, ns_image_whole<double> & im){
	ns_image_properties p;
	p.width = dim->im.width();
	p.height = dim->im.height();
	p.components = dim->im.nchannels();
	im.init(p);
	if (p.components == 1){
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < p.width; x++){
				im[y][x] = dim->im.data()[x+p.width*y];
			}
	}else{
		for (unsigned long y = 0; y < p.height; y++)
			for (unsigned long x = 0; x < p.width; x++){
				im[y][x] = dim->im.data()[x+3*p.width*y];
				im[y][x+1] = dim->im.data()[x+1+3*p.width*y] ;
				im[y][x+2] = dim->im.data()[x+2+3*p.width*y];
			}
	}
};

void ns_optical_flow::calculate(const ns_image_standard & im1, 
				const ns_image_standard & im2,
				double alpha,
				double ratio,
				int minWidth,
				int nOuterFPIterations,
				int nInnerFPIterations,
				int nSORIterations){
	OpticalFlow::IsDisplay = false;
	image_to_DImage(im1,Dim1);
	image_to_DImage(im2,Dim2);
	OpticalFlow::Coarse2FineFlow(vx->im,vy->im,warpI2->im,Dim1->im,Dim2->im,alpha,ratio,minWidth,nOuterFPIterations,nInnerFPIterations,nSORIterations);
}
void ns_optical_flow::calculate(double alpha,
				double ratio,
				int minWidth,
				int nOuterFPIterations,
				int nInnerFPIterations,
				int nSORIterations){
	OpticalFlow::IsDisplay = false;
	OpticalFlow::Coarse2FineFlow(vx->im,vy->im,warpI2->im,Dim1->im,Dim2->im,alpha,ratio,minWidth,nOuterFPIterations,nInnerFPIterations,nSORIterations);
}
