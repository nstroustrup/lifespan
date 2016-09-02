
#define ITK_USE_FFTWD

//set somewhere  if you want posture analysis to run partially on the GPU
//#define NS_USE_ITK_GPU
//#undef NS_USE_ITK_GPU

#undef NS_USE_LEVEL_SET_REGISTRATION
#undef NS_USE_ITK_GPU
#include "itkHistogramMatchingImageFilter.h"

#ifndef NS_USE_ITK_GPU
	#ifndef  NS_USE_LEVEL_SET_REGISTRATION
		#include <itkDemonsRegistrationFilter.h>
	#else
		#include "itkLevelSetMotionRegistrationFilter.h"
	#endif
#else
	#include "itkGPUDemonsRegistrationFilter.h"
	#include "itkGPUImage.h"
	#include "itkGPUKernelManager.h"
	#include "itkGPUContextManager.h"
#endif

#include "ns_spatial_avg.h"
#include "ns_get_double.h"


#include "ns_optical_flow.h"
#include "ns_optical_flow_quantification.h"
#include "ns_image_easy_io.h"
#include <fstream>

class ns_external_image_t {
public:
	#ifndef NS_USE_ITK_GPU
	typedef itk::Image< float, 2 >  ns_itk_image;
	#else
	typedef itk::GPUImage<float, 2> ns_itk_image;
	#endif
	ns_itk_image::Pointer image;
	ns_image_properties properties;

};
/*
template<class T>
class ns_itk_image_wrapper : itk::Image<T, 2 > {

	//imagebase class functions
	virtual void Allocate(bool initialize = false) {
		const OffsetValueType* tb(this->GetOffsetTable()[2]);
		ns_image_properties p;
		p.width = tb[1];
		p.height = tb[2] / tb[1];
		p.components = 1;
		image->resize(p);
	}
	virtual void Initialize() {
		Superclass::Initialize();
		image->clear();
	}
	virtual void Graft(const DataObject *data) {
		Superclass::Graft(data);
		if (data){
			// Attempt to cast data to an Image
			const ns_itk_image_wrapper<T> * const imgData = dynamic_cast< const ns_itk_image_wrapper<T> * >(data);

			if (imgData != ITK_NULLPTR)
				image = imgData->image;
			else{
				// pointer could not be cast back down
				itkExceptionMacro(<< "itk::Image::Graft() cannot cast "
					<< typeid(data).name() << " to "
					<< typeid(const Self *).name());
			}
		}
	}
	//image functions
	void FillBuffer(const TPixel & value);
	void SetPixel(const IndexType & index, const TPixel & value);
	const TPixel & GetPixel(const IndexType & index) const;
	TPixel & GetPixel(const IndexType & index);
	virtual TPixel * GetBufferPointer()
	{
		return m_Buffer ? m_Buffer->GetBufferPointer() : ITK_NULLPTR;
	}
	virtual const TPixel * GetBufferPointer() const
	{
		return m_Buffer ? m_Buffer->GetBufferPointer() : ITK_NULLPTR;
	}

	virtual unsigned int GetNumberOfComponentsPerPixel() const ITK_OVERRIDE;
private:
	ns_image_whole<T> *image;
};*/

float * ns_external_image ::buffer() {
	return image->image->GetBufferPointer();
}
const float * ns_external_image::buffer() const {
	return image->image->GetBufferPointer();
}


class ns_flow_processor_storage_t {
public:
	typedef itk::Vector< float, 2 >                VectorPixelType;
#ifndef NS_USE_ITK_GPU
	typedef itk::Image<  VectorPixelType, 2 >      DisplacementFieldType;

	#ifndef NS_USE_LEVEL_SET_REGISTRATION

	typedef itk::DemonsRegistrationFilter<
		ns_external_image_t::ns_itk_image,
		ns_external_image_t::ns_itk_image,
		DisplacementFieldType> RegistrationFilterType;
		RegistrationFilterType::Pointer filter;
	#else
	typedef itk::LevelSetMotionRegistrationFilter<
		ns_external_image_t::ns_itk_image,
		ns_external_image_t::ns_itk_image,
		DisplacementFieldType> RegistrationFilterType;
	RegistrationFilterType::Pointer filter;
	#endif
#else
	typedef itk::GPUImage<VectorPixelType, 2> DisplacementFieldType;
	typedef itk::GPUDemonsRegistrationFilter<
		ns_external_image_t::ns_itk_image,
		ns_external_image_t::ns_itk_image,
		DisplacementFieldType> RegistrationFilterType;
	RegistrationFilterType::Pointer filter;
#endif

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
			image->image->SetPixel(pixelIndex, i[y][x]/255.0f);
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

#include "ns_high_precision_timer.h"
void ns_optical_flow::test(){
	try {
		const int n(36);
		std::string dir("C:\\server\\debug\\");
		ns_dir dir_list;
		dir_list.load(dir);
		int specific_worm = -1;// 13;

		std::vector<int> number_of_images(n, 0);
		if (specific_worm != -1) {
			number_of_images.resize(1);
			std::string val("im_");
			val += ns_to_string(specific_worm + 1);
			val += "_";
			for (unsigned int j = 0; j < dir_list.files.size(); j++) {
				if (dir_list.files[j].substr(0, val.size()) == val)
					//		std::cerr << dir_list.files[j] << " contains " << dir_list.files[j].substr(0, val.size()) << "\n";
					number_of_images[0]++;
			}
		}
		else {
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
		}
		ns_image_standard tmp;
		std::vector<std::vector<ns_image_standard> > images(number_of_images.size());
		for (unsigned int i = 0; i < images.size(); i++) {
			images[i].resize(number_of_images[i]);
			std::cout << "Image " << i << " has " << number_of_images[i] << " images\n";
			std::cout.flush();
			for (unsigned int j = 0; j < number_of_images[i]; j++) {
				if (specific_worm != -1)
					ns_load_image(dir + "im_" + ns_to_string(specific_worm + 1) + "_" + ns_to_string(j) + "df.tif", tmp);
				else ns_load_image(dir + "im_" + ns_to_string(i + 1) + "_" + ns_to_string(j) + "df.tif", tmp);
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
		ns_image_standard im;

		ns_64_bit sum(0), N(0);
		double mk(0), sk(0), NN(0);

		for (unsigned int k = 0; k < images.size(); k++) {
			std::cerr << "processing " << k << " of " << n << "\n";
			if (number_of_images[k] <2)
				continue;

			//

			const long border(10);
			ns_image_properties p(images[k][0].properties());
			ns_image_properties p2(p);
			p2.height = (p.height-2*border)*(number_of_images[k]-1);
			p2.width = (p.width - 2 * border) * 3;
			p2.components = 3;
			ns_optical_flow flow;
			ns_image_whole<float> out;
		
			std::vector<ns_image_whole<float> >  vx(number_of_images[k] - 1), vy(number_of_images[k] - 1);
			out.prepare_to_recieve_image(p2);
			float maxm(FLT_MIN);
			float minm(FLT_MAX);
			float abs_max(0);
			for (unsigned int i = 2; i < number_of_images[k]; i++) {
				
				std::cerr << (i - 2) << "...";
				flow.Dim1.from(images[k][i - 1]);
				flow.Dim2.from(images[k][i]);
				ns_high_precision_timer timer;
				timer.start();
				flow.calculate(30, 0,3/255.0f);
				const ns_64_bit v(timer.stop());
				sum += v;
				//running variance
				if (NN > 0) {
					const double mk_1(mk);
					mk = mk + (v - mk) / NN;
					sk = sk + (v - mk_1)*(v - mk);
				}
				else mk = v;
				NN++;
				std::cout << round(sum / NN / pow(10.0,4))/100 << "+-" << round(sqrt(sk / (NN - 1))/pow(10.0, 4))/100 << "\n";

				flow.get_movement(vx[i - 1], vy[i - 1]);

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



void ns_optical_flow::calculate(const int num_it, const float gaussian_stdev, const float min_intensity_difference ) {




	flow_processor->matcher->Modified();
	flow_processor->matcher->SetInput(Dim2.image->image.GetPointer());
	flow_processor->matcher->SetReferenceImage(Dim1.image->image.GetPointer());
	flow_processor->matcher->SetNumberOfHistogramLevels(256);
	flow_processor->matcher->SetNumberOfMatchPoints(64);
	flow_processor->matcher->ThresholdAtMeanIntensityOn();


	flow_processor->filter->SetFixedImage(Dim1.image->image.GetPointer());
	//flow_processor->filter->SetMovingImage(Dim2.image->image.GetPointer());
	flow_processor->filter->SetMovingImage(flow_processor->matcher->GetOutput());

	flow_processor->filter->SetNumberOfIterations(num_it);
	flow_processor->filter->SetIntensityDifferenceThreshold(min_intensity_difference);

#if !defined NS_USE_ITK_GPU && defined NS_USE_LEVEL_SET_REGISTRATION
	double g(gaussian_stdev);
	if (g == 0)
		g = .1;
		flow_processor->filter->SetGradientSmoothingStandardDeviations(g);
		flow_processor->filter->SetAlpha(1 / 255.0f / 20.0f);
	#else
	#ifndef NS_USE_ITK_GPU
		flow_processor->filter->SetNumberOfThreads(1);
	#endif
	flow_processor->filter->SetStandardDeviations(gaussian_stdev);
	flow_processor->filter->SetUpdateFieldStandardDeviations(.1);
	flow_processor->filter->SetSmoothUpdateField(0);
#endif

	flow_processor->filter->Update();
}

void median_fill(const ns_image_properties & p,const ns_flow_processor_storage_t::VectorPixelType * fbuf, const int channel,ns_image_whole<float> & v) {
	
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

	/*for (unsigned int y = 0; y < Dim1.image->properties.height; y++)
		for (unsigned int x = 0; x < Dim1.image->properties.width; x++) {
			if (fbuf[Dim1.image->properties.width*(y)+x][0] != 0)
				std::cerr << ".";
			if (fbuf[Dim1.image->properties.width*(y)+x][1] != 0)
				std::cerr << ".";
		}*/
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
