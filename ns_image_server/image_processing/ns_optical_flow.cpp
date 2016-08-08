
#define ITK_USE_FFTWD
//#include "itkCurvatureRegistrationFilter.h"

//#include "itkFastSymmetricForcesDemonsRegistrationFunction.h"
//#include "itkNearestNeighborInterpolateImageFunction.h"

#include "itkImageRegionIterator.h"
#include "itkLevelSetMotionRegistrationFilter.h"
#include "itkHistogramMatchingImageFilter.h"
#include "itkDiscreteGaussianImageFilter.h"
#include "itkCastImageFilter.h"
#include "itkWarpImageFilter.h"

#include "itkCurvatureRegistrationFilter.h"

#include "ns_spatial_avg.h"
#include "ns_get_double.h"


#include "ns_optical_flow.h"
#include "ns_optical_flow_quantification.h"
#include "ns_image_easy_io.h"
#include <fstream>

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

	//for curvature registration
	//typedef itk::FastSymmetricForcesDemonsRegistrationFunction<ns_external_image_t::ns_itk_image, 
		//														ns_external_image_t::ns_itk_image,
			//													DisplacementFieldType> ForcesType;
	//typedef itk::CurvatureRegistrationFilter<ns_external_image_t::ns_itk_image, ns_external_image_t::ns_itk_image, DisplacementFieldType, ForcesType>
		//RegistrationType;
	//typedef itk::WarpImageFilter<ns_external_image_t::ns_itk_image, ns_external_image_t::ns_itk_image, DisplacementFieldType> WarperType;
	//RegistrationType::Pointer curvature_registrator;
};
ns_external_image::ns_external_image() {
	image = new ns_external_image_t();
}
ns_external_image::~ns_external_image() { delete image;}



void ns_external_image::init(const ns_image_properties & p) {
	if (image->properties == p)
		return;
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
class ns_dummy_mask {
public:
	bool mask(const int x, const int y) const { return true; }
};
struct ns_optical_flow_quantifications {
	void calculate(const ns_image_standard & t1_, const ns_image_standard & t2_,const ns_image_whole<float>  &dx_, const ns_image_whole<float> & dy_,const long border=10) {
		ns_image_properties p(t1_.properties());
		ns_dummy_mask mask;
		dx.calculate(p,ns_optical_flow_accessor_val<float>(dx_),border, mask);
		dy.calculate(p, ns_optical_flow_accessor_val<float>(dy_), border, mask);
		dmag.calculate(p, ns_optical_flow_accessor_mag<float>(dx_, dy_), border, mask);
		t1.calculate(p, ns_optical_flow_accessor_val<ns_8_bit>(t1_), border, mask);
		t2.calculate(p,ns_optical_flow_accessor_val<ns_8_bit>(t2_), border, mask);
		diff.calculate(p,ns_optical_flow_accessor_diff<ns_8_bit>(t1_, t2_), border, mask);
		sc_mag.calculate(p, ns_optical_flow_accessor_scaled_movement(dx_, dy_, t1_,t2_), border, mask);
	}
	static void write_header(std::ostream & out) {
		ns_optical_flow_quantification::write_header("displacement x", out);
		out << ",";
		ns_optical_flow_quantification::write_header("displacement y", out);
		out << ",";
		ns_optical_flow_quantification::write_header("magnitude", out);
		out << ",";
		ns_optical_flow_quantification::write_header("image 1", out);
		out << ",";
		ns_optical_flow_quantification::write_header("image 2", out);
		out << ",";
		ns_optical_flow_quantification::write_header("raw diff", out);
		out << ",";
		ns_optical_flow_quantification::write_header("scaled mag", out);
	}
	void write(std::ostream & out) const {
		dx.write(out);
		out << ",";
		dy.write(out);
		out << ",";
		dmag.write(out);
		out << ",";
		t1.write(out);
		out << ",";
		t2.write(out);
		out << ",";
		diff.write(out);
		out << ",";
		sc_mag.write(out);
		out << ",";
	}
	ns_optical_flow_quantification dx, dy, dmag, t1, t2, diff, sc_mag;
};
void ns_optical_flow::test(){
	try {
		const int n(36);
		std::string dir("C:\\server\\debug\\");
		ns_dir dir_list;
		dir_list.load(dir);
		std::vector<int> number_of_images(n, 0);
		for (unsigned int i = 0; i < n; i++) {
			std::string val("im_");
			val += ns_to_string(i + 1);
			val += "_";
			for (unsigned int j = 0; j < dir_list.files.size(); j++) {
				if (dir_list.files[j].substr(0, val.size()) == val) 
			//		std::cerr << dir_list.files[j] << " contains " << dir_list.files[j].substr(0, val.size()) << "\n";
					number_of_images[i]++;
			}
		}
		ns_image_standard tmp;
		std::vector<std::vector<ns_image_standard> > images(n);
		for (unsigned int i = 0; i < n; i++) {
			images[i].resize(number_of_images[i]);
			std::cout << "Image " << i << " has " << number_of_images[i] << " images\n";
			std::cout.flush();
			for (unsigned int j = 0; j < number_of_images[i]; j++) {
				ns_load_image(dir + "im_" + ns_to_string(i + 1) + "_" + ns_to_string(j) + "df.tif", tmp);
				ns_image_properties p(tmp.properties());
				p.components = 1;
				images[i][j].init(p);
				for (unsigned int y = 0; y < p.height; y++)
					for (unsigned int x = 0; x < p.width; x++) {
						images[i][j][y][x] = tmp[y][3 * x + 1];
					}
			}
		}
		std::vector<ns_optical_flow_quantifications> quantifications(n - 1);
		std::ofstream out_quant((dir + "out\\quantification.csv").c_str());
		out_quant << "worm_id,timepoint,";
		ns_optical_flow_quantifications::write_header(out_quant);
		out_quant << "\n";
		for (unsigned int k = 0; k < n; k++) {
			std::cerr << "processing " << k << " of " << n << "\n";
			if (number_of_images[k] <2)
				continue;
			const long border(10);
			ns_image_properties p(images[k][0].properties());
			ns_image_properties p2(p);
			p2.height = (p.height-2*border)*(number_of_images[k]-1);
			p2.width = (p.width - 2 * border) * 3;
			p2.components = 3;
			ns_optical_flow flow;
			ns_image_whole<ns_16_bit> out;
		
			std::vector<ns_image_whole<float> >  vx(number_of_images[k] - 1), vy(number_of_images[k] - 1);
			out.prepare_to_recieve_image(p2);
			float maxm(FLT_MIN);
			float minm(FLT_MAX);
			float abs_max(0);
			for (unsigned int i = 2; i < number_of_images[k]; i++) {
				
				std::cerr << (i - 2) << "...";
				flow.Dim1.from(images[k][i - 1]);
				flow.Dim2.from(images[k][i]);
				flow.calculate(15, .001);

				flow.get_movement(vx[i - 1], vy[i - 1]);


				if (i == 3) {
					ns_image_whole<float> im;
					ns_image_properties p3(p);
					p3.components = 3;
					im.init(p3);
					for (unsigned long y = 0; y < p3.height; y++) {
						for (unsigned long x = 0; x < p3.width; x++) {
							im[y][3 * x] = vx[i - 1][y][x];
							im[y][3 * x+1] = vy[i - 1][y][x];
							im[y][3 * x + 2] = 0;
						}
					}
					ns_tiff_image_output_file<float> tiff_out(ns_tiff_compression_none);
					ns_image_stream_file_sink<float> file_sink(dir + "out\\fworm_" + ns_to_string(k) + ".tif", tiff_out, 1024);
					im.pump(file_sink, 1024);
					file_sink.finish_recieving_image();
					tiff_out.close();
					ns_tiff_image_input_file<float> tiff_in;
					tiff_in.open_file(dir + "out\\fworm_" + ns_to_string(k) + ".tif");
					ns_image_stream_file_source<float> file_source(tiff_in);
					ns_image_whole<float> im2;
					file_source.pump(im2, 1024);
					for (unsigned long y = 0; y < p3.height; y++) {
						for (unsigned long x = 0; x < p3.width; x++) {
							if (im[y][x] != im2[y][x])
								std::cerr << "Yikes!";
						}
					}

				}



				quantifications[i - 1].calculate(images[k][i - 1], images[k][i], vx[i - 1], vy[i - 1]);
				out_quant << k << "," << i - 1 << ",";
				quantifications[i - 1].write(out_quant);
				out_quant << "\n";
				for (unsigned long y = border; y < p.height- border; y++) {
					for (unsigned long x = border; x < p.width- border; x++) {
						if (vx[i - 1][y][x] > maxm)
							maxm = vx[i - 1][y][x];
						if (vy[i - 1][y][x] > maxm)
							maxm = vy[i - 1][y][x];
						if (vx[i - 1][y][x] < minm)
							minm = vx[i - 1][y][x];
						if (vy[i - 1][y][x] < minm)
							minm = vy[i - 1][y][x];
						if (fabs(vx[i - 1][y][x]) > abs_max)
							abs_max = fabs(vx[i - 1][y][x]);
						if (fabs(vy[i - 1][y][x]) > abs_max)
							abs_max = fabs(vy[i - 1][y][x]);
					}
				}
			
				for (unsigned long y = 0; y < p.height- 2*border; y++) {
					for (unsigned long x =0; x < p.width - 2 * border; x++) {
						out[y + (p.height - 2 * border)*(i - 2)][3 * x] =     (ns_16_bit)(images[k][i - 1][y + border][x + border])*(ns_16_bit)(USHRT_MAX / 255);
						out[y + (p.height - 2 * border)*(i - 2)][3 * x + 1] = (ns_16_bit)(images[k][i]    [y + border][x + border] * (USHRT_MAX / 255));
						out[y + (p.height - 2 * border)*(i - 2) ][3 * x  + 2] = 0;
					}
				}
				

				for (unsigned long y = 0; y < p.height - 2 * border; y++) {
					for (unsigned long x = 0; x < p.width - 2 * border; x++) {
						out[y + (p.height - 2 * border)*(i - 2) ][3 * (p.width- 2 * border + x )] =     ((vx[i - 1][y + border][x + border] - minm) / (maxm - minm)*USHRT_MAX);
						out[y + (p.height - 2 * border)*(i - 2) ][3 * (p.width- 2 * border + x ) + 1] = ((vy[i - 1][y + border][x + border] - minm) / (maxm - minm)*USHRT_MAX);
						out[y + (p.height - 2 * border)*(i - 2) ][3 * (p.width - 2 * border + x ) + 2] = 0;
					}
				}
			
				for (unsigned long y = 0; y < p.height - 2 * border; y++) {
					for (unsigned long x = 0; x < p.width - 2 * border; x++) {
						
						ns_vector_2d v(vx[i - 1][y+border][x + border], vy[i - 1][y + border][x + border]);
						double mm(v.mag() / abs_max*USHRT_MAX);
						double mang(v.angle()/ns_pi+1); //goes from 0 to 2
						double r((mang < 1) ? (1-mang ) : 0),
											  //1->0	  0
							g((mang < 1) ? 0 : (mang - 1)),
										   //0  0->1
							b((mang < 1) ? (mang) : (2 - mang));
											//0->1	  1->0

						out[y + (p.height - 2 * border)*(i - 2)][3 * (2 * (p.width - 2 * border) + x)] = r*mm;
						out[y + (p.height - 2 * border)*(i - 2)][3 * (2 * (p.width - 2 * border) + x) + 1] = g*mm;
						out[y + (p.height - 2 * border)*(i - 2)][3 * (2 * (p.width - 2 * border) + x) + 2] = b*mm;
					}
				}
			}
			

			ns_tiff_image_output_file<ns_16_bit> tiff_out(ns_tiff_compression_none);
			ns_image_stream_file_sink<ns_16_bit> file_sink(dir + "out\\worm_" + ns_to_string(k) + ".tif", tiff_out, 1024);
			out.pump(file_sink, 1024);
		}
	}
	catch (ns_ex & ex) {
		std::cerr << ex.text() << "\n";
		char a;
		std::cin >> a;
	}
	catch (std::exception & e) {
		std::cerr << e.what() << "\n";
		char a;
		std::cin >> a;
	}
	exit(0);
}

void ns_optical_flow::calculate(const int num_it, float gaussian_stdev) {


	//flow_processor->curvature_registrator = ns_flow_processor_storage_t::RegistrationType::New();
	
	//registrator->SetInitialDisplacementField(initField);

	/*typedef itk::DiscreteGaussianImageFilter<ns_external_image_t::ns_itk_image, ns_external_image_t::ns_itk_image> gfilter;

	gfilter::Pointer g1(gfilter::New()),g2(gfilter::New());
	g1->SetInput(Dim1.image->image.GetPointer());
	g2->SetInput(Dim2.image->image.GetPointer());
	g1->SetFilterDimensionality(2);
	g2->SetFilterDimensionality(2);
	g1->SetVariance(1);
	g2->SetVariance(1);
	
	flow_processor->curvature_registrator->SetMovingImage(g2->GetOutput());
	flow_processor->curvature_registrator->SetFixedImage(g1->GetOutput());
	flow_processor->curvature_registrator->SetNumberOfIterations(50);
	flow_processor->curvature_registrator->SetTimeStep(1);
	flow_processor->curvature_registrator->SetConstraintWeight(.001);
	flow_processor->curvature_registrator->Update();
	*/
	
	flow_processor->matcher->SetInput(Dim2.image->image.GetPointer());
	flow_processor->matcher->SetReferenceImage(Dim1.image->image.GetPointer());
	flow_processor->matcher->SetNumberOfHistogramLevels(1024);
	flow_processor->matcher->SetNumberOfMatchPoints(14);
	flow_processor->matcher->ThresholdAtMeanIntensityOn();


	flow_processor->filter->SetFixedImage(Dim1.image->image.GetPointer());
	//flow_processor->filter->SetMovingImage(Dim2.image->image.GetPointer());
	flow_processor->filter->SetMovingImage(flow_processor->matcher->GetOutput());

	flow_processor->filter->SetNumberOfIterations(num_it);
	flow_processor->filter->SetGradientSmoothingStandardDeviations(gaussian_stdev);
	flow_processor->filter->Update();
	
}

void median_fill(const ns_image_properties & p,const const ns_flow_processor_storage_t::VectorPixelType * fbuf, const int channel,ns_image_whole<float> & v) {
	
	const long bsize(5);
	const long bsize2((bsize-1)*2+1);
	std::vector<float>contents(bsize2*bsize2, 0);
	unsigned long size(0);
	for (unsigned long y = 0; y < bsize-1; y++)
		for (unsigned long x = 0; x < p.width; x++) {
			v[y][x] = 0;
		}

	for ( long y = bsize-1; y < p.height - bsize; y++) {

		for (unsigned long x = 0; x < bsize - 1; x++)
			v[y][x] = 0;
		for ( long x = bsize-1; x < p.width - bsize; x++) {
			//load median filter
			for (long y_ = -bsize+1; y_ < bsize; y_++) {
				for (long x_ = -bsize+1; x_ < bsize; x_++) {
					contents[(y_+bsize-1)*bsize + x_+bsize-1] = fbuf[p.width*(y + y_) + x + x_][channel];
					//if (contents[y_*bsize + x_] != 0)
				//	std::cerr << contents[y_*bsize + x_] << " ";
				}
			}
			std::nth_element(contents.begin(), contents.begin() + contents.size() / 2, contents.end());
			v[y][x] = *(contents.begin() + contents.size() / 2);
		//	if (v[y][x] != 0)
			//	std::cerr << "\n*" << v[y][x] << "\n";
		
		}

		for (unsigned long x = p.width - bsize; x < p.width; x++)
			v[y][x] = 0;
	}
	for (unsigned long y = p.height-bsize; y < p.height; y++)
		for (unsigned long x = 0; x < p.width; x++) {
			v[y][x] = 0;
		}

}
void ns_optical_flow::get_movement(ns_image_whole<float> & vx, ns_image_whole<float> & vy) {
	//this is the displacement field!
	vx.init(Dim1.image->properties);
	vy.init(Dim1.image->properties);
	
	const ns_flow_processor_storage_t::VectorPixelType * fbuf(flow_processor->filter->GetOutput()->GetBufferPointer());
	//const ns_flow_processor_storage_t::VectorPixelType * fbuf(this->flow_processor->curvature_registrator->GetOutput()->GetBufferPointer());


	median_fill(Dim1.image->properties, fbuf, 0, vx);
	median_fill(Dim1.image->properties, fbuf, 1, vy);
	
	/*
	for (unsigned long y = 0; y < Dim1.image->properties.height; y++)
		for (unsigned long x = 0; x < Dim1.image->properties.width; x++) {
			vx[y][x] = fbuf[y*Dim1.image->properties.width + x][0];
			vy[y][x] = fbuf[y*Dim1.image->properties.width + x][1];
		}
		*/

	
}


void ns_optical_flow_quantification::write(std::ostream & out) const {
	out << d_max << "," << d_min << "," << d_avg << "," << d_median << "," << d_75th_percentile << "," << d_95th_percentile;
}
void ns_optical_flow_quantification::read(std::istream & in)  {
	ns_get_double d;
	d(in,d_max);
	if (in.fail()) throw ns_ex("Invalid Specification");
	d(in, d_min);
	if (in.fail()) throw ns_ex("Invalid Specification");
	d(in, d_avg);
	if (in.fail()) throw ns_ex("Invalid Specification");
	d(in, d_median);
	if (in.fail()) throw ns_ex("Invalid Specification");
	d(in, d_75th_percentile);
	if (in.fail()) throw ns_ex("Invalid Specification");
	d(in, d_95th_percentile);
	if (in.fail()) throw ns_ex("Invalid Specification");
}

ns_optical_flow_quantification ns_optical_flow_quantification::square_root() {
	ns_optical_flow_quantification ret(*this);
	ret.d_max = sqrt(ret.d_max);
	ret.d_min = sqrt(ret.d_min);
	ret.d_avg = sqrt(ret.d_avg);
	ret.d_sum = sqrt(ret.d_sum);
	ret.d_median = sqrt(ret.d_median);
	ret.d_75th_percentile = sqrt(ret.d_75th_percentile);
	ret.d_95th_percentile = sqrt(ret.d_95th_percentile);
	return ret;
}
ns_optical_flow_quantification ns_optical_flow_quantification::square() {
	ns_optical_flow_quantification ret(*this);
	ret.d_max = ret.d_max*ret.d_max;
	ret.d_min = ret.d_min*ret.d_min;
	ret.d_avg = ret.d_avg*ret.d_avg;
	ret.d_sum = ret.d_sum*ret.d_sum;
	ret.d_median = ret.d_median*ret.d_median;
	ret.d_75th_percentile = ret.d_75th_percentile*ret.d_75th_percentile;
	ret.d_95th_percentile = ret.d_95th_percentile*ret.d_95th_percentile;
	return ret;
}
void ns_optical_flow_quantification::zero() {
	d_max = 0;
	d_min = 0;
	d_avg = 0;
	d_sum = 0;
	d_median = 0;
	d_75th_percentile = 0;
	d_95th_percentile = 0;
}

ns_optical_flow_quantification operator+(const ns_optical_flow_quantification & a, const ns_optical_flow_quantification & b) {
	ns_optical_flow_quantification ret(a);
	ret.d_75th_percentile += b.d_75th_percentile;
	ret.d_95th_percentile += b.d_95th_percentile;
	ret.d_avg += b.d_avg;
	ret.d_max += b.d_max;
	ret.d_median += b.d_median;
	ret.d_min += b.d_min;
	ret.d_sum += b.d_sum;
	return ret;
}
ns_optical_flow_quantification operator/(const ns_optical_flow_quantification & a, const float & d) {
	ns_optical_flow_quantification ret(a);
	ret.d_75th_percentile /= d;
	ret.d_95th_percentile /= d;
	ret.d_avg /= d;
	ret.d_max /= d;
	ret.d_median /= d;
	ret.d_min /= d;
	ret.d_sum /= d;
	return ret;
}