

#include "itkImageRegionIterator.h"
#include "itkLevelSetMotionRegistrationFilter.h"
#include "itkHistogramMatchingImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkWarpImageFilter.h"


#include "ns_optical_flow.h"
#include "ns_image_easy_io.h"

class ns_external_image_t {
public:
	typedef itk::Image< float, 2 >  ns_itk_image;
	ns_itk_image::Pointer image;
	ns_image_properties properties;

};
float * ns_external_image ::buffer() {
	return image->image->GetBufferPointer();
}
const float * ns_external_image::buffer() const {
	return image->image->GetBufferPointer();
}


class ns_flow_processor_storage_t {
public:
	typedef itk::Vector< float, 2 >                VectorPixelType;
	typedef itk::Image<  VectorPixelType, 2 >      DisplacementFieldType;
	typedef itk::LevelSetMotionRegistrationFilter<
		ns_external_image_t::ns_itk_image,
		ns_external_image_t::ns_itk_image,
		DisplacementFieldType> RegistrationFilterType;
	RegistrationFilterType::Pointer filter;


	typedef itk::HistogramMatchingImageFilter<
		ns_external_image_t::ns_itk_image,
		ns_external_image_t::ns_itk_image >   MatchingFilterType;
	MatchingFilterType::Pointer matcher;
};
ns_external_image::ns_external_image() {
	image = new ns_external_image_t();
}
ns_external_image::~ns_external_image() { delete image;}



void ns_external_image::init(const ns_image_properties & p) {
	ns_external_image_t::ns_itk_image::RegionType region;
	ns_external_image_t::ns_itk_image::IndexType start;
	start[0] = 0;
	start[1] = 0;
	ns_external_image_t::ns_itk_image::SizeType size;
	image->properties = p;
	size[0] = p.width;
	size[1] = p.height;
	region.SetSize(size);
	region.SetIndex(start);
	image->image = ns_external_image_t::ns_itk_image::New();
	image->image->SetRegions(region);
	image->image->Allocate();
}

void ns_external_image::from(const ns_image_standard & i) {
	init(i.properties());
	for (unsigned int y = 0; y < image->properties.height; y++){
		for (unsigned int x = 0; x < image->properties.width; x++){
			ns_external_image_t::ns_itk_image::IndexType pixelIndex;
			pixelIndex[0] = x;
			pixelIndex[1] = y;
			image->image->SetPixel(pixelIndex, i[y][x]);
		}
	}
}


ns_optical_flow::ns_optical_flow(){
	flow_processor = new ns_flow_processor_storage_t;
	flow_processor->filter = ns_flow_processor_storage_t::RegistrationFilterType::New();
	flow_processor->matcher = ns_flow_processor_storage_t::MatchingFilterType::New();
}
ns_optical_flow::~ns_optical_flow() {
	delete flow_processor;
}

void ns_optical_flow::test(){
	const int n(8);
	std::vector<ns_image_standard> im(n);
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
	p2.width = p.width*3;
	p2.components = 3;
	ns_optical_flow flow;
	ns_image_whole<ns_16_bit> out,mag;
	ns_image_whole<float>  vx[n], vy[n];
	out.prepare_to_recieve_image(p2);
	float maxm(0);
	float minm(0);
	for (unsigned int i = 1; i < n; i++) {
		flow.Dim1.from(im[i - 1]);
		flow.Dim2.from(im[i]);
		flow.calculate(20, 2);
		flow.get_movement(vx[i - 1], vy[i - 1]);
		for (unsigned long y = 0; y < p.height; y++) {
			for (unsigned long x = 0; x < p.width; x++) {
				if (vx[i - 1][y][x] > maxm)
					maxm = vx[i - 1][y][x];
				if (vy[i - 1][y][x] > maxm)
					maxm = vy[i - 1][y][x];
				if (vx[i - 1][y][x] < minm)
					minm = vx[i - 1][y][x];
				if (vy[i - 1][y][x] < minm)
					minm = vy[i - 1][y][x];
			}
		}
	}
	for (unsigned int i = 1; i < n; i++) {
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*x] = (ns_16_bit)(im[i-1][y][x])*(ns_16_bit)(USHRT_MAX/255);
				out[y+p.height*(i-1)][3*x+1] = im[i][y][x]*(USHRT_MAX/255);
				out[y+p.height*(i-1)][3*x+2] = 0;
			}
		}
		
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*(p.width+x)] = ((vx[i - 1][y][x]-minm)/(maxm-minm)*USHRT_MAX);
				out[y+p.height*(i-1)][3*(p.width+x)+1] = ((vy[i - 1][y][x]-minm) / (maxm - minm)*USHRT_MAX);
				out[y+p.height*(i-1)][3*(p.width+x)+2] = 0;
			}
		}
		mag.prepare_to_recieve_image(p);
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				mag[y][x] = (sqrt(pow(vx[i - 1][y][x],2)+pow(vy[i - 1][y][x],2)) - minm) / (maxm - minm)*(USHRT_MAX/2);
			}
		}
		for (unsigned long y = 0; y < p.height;y++){
			for (unsigned long x = 0; x < p.width;x++){
				out[y+p.height*(i-1)][3*(2*p.width+x)] = mag[y][x];
				out[y+p.height*(i-1)][3*(2*p.width+x)+1] =mag[y][x];
				out[y+p.height*(i-1)][3*(2*p.width+x)+2] = mag[y][x];
			}
		}
	}
	ns_tiff_image_output_file<ns_16_bit> tiff_out(ns_tiff_compression_none);
	ns_image_stream_file_sink<ns_16_bit> file_sink(dir + "out2.tif",tiff_out,1024);
	out.pump(file_sink,1024);
	exit(0);
}

void ns_optical_flow::calculate(const int num_it, float gaussian_stdev) {


	flow_processor->matcher->SetInput(Dim2.image->image.GetPointer());
	flow_processor->matcher->SetReferenceImage(Dim1.image->image.GetPointer());
	flow_processor->matcher->SetNumberOfHistogramLevels(1024);
	flow_processor->matcher->SetNumberOfMatchPoints(14);
	flow_processor->matcher->ThresholdAtMeanIntensityOn();


	flow_processor->filter->SetFixedImage(Dim1.image->image.GetPointer());
	flow_processor->filter->SetMovingImage(flow_processor->matcher->GetOutput());

	flow_processor->filter->SetNumberOfIterations(num_it);
	flow_processor->filter->SetGradientSmoothingStandardDeviations(gaussian_stdev);
	flow_processor->filter->Update();
}

void ns_optical_flow::get_movement(ns_image_whole<float> & vx, ns_image_whole<float> & vy){
	//this is the displacement field!
	const ns_flow_processor_storage_t::DisplacementFieldType * displacement_field = flow_processor->filter->GetOutput();
	const ns_flow_processor_storage_t::VectorPixelType * fbuf = displacement_field->GetBufferPointer();
	vx.init(Dim1.image->properties);
	vy.init(Dim1.image->properties);

	for (unsigned long y = 0; y < Dim1.image->properties.height; y++)
		for (unsigned long x = 0; x < Dim1.image->properties.width; x++) {
			vx[y][x] = fbuf[y*Dim1.image->properties.width + x][0];
			vy[y][x] = fbuf[y*Dim1.image->properties.width + x][1];
		}
}
