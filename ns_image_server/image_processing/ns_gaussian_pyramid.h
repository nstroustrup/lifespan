#ifndef NS_GAUSIAN_PYRAMID_H
#define NS_GAUSIAN_PYRAMID_H
#include "ns_subpixel_image_alignment.h"
#include "ipp.h"
#ifdef _MSC_VER
#define finline                     __forceinline
#else
#define finline						__attribute__((always_inline))
#endif

template<class T>
void ns_ippi_safe_delete(T * & pointer) {
	if (pointer == 0) return;
	T * tmp(pointer);
	pointer = 0;
	ippiFree(tmp);
}
template<class T>
void ns_ipps_safe_delete(T * & pointer) {
	if (pointer == 0) return;
	T * tmp(pointer);
	pointer = 0;
	ippsFree(tmp);
}


//#define NS_DEBUG_IMAGE_ACCESS
class ns_intel_image_32f {
public:
	ns_intel_image_32f() :buffer(0) {}
	~ns_intel_image_32f() {
		clear();
	}
	finline Ipp32f &  val(const int & y, const int & x) { return buffer[y*line_step_in_pixels + x]; }
	finline 
		const Ipp32f & val(const int & y, const int & x) const {
		/*if (y < 0 || x < 0 ||
			y > properties_.height ||x > properties_.width) { //OK because we allocate an extra pixel pad; see comment in init()
			std::cerr << "ns_intel_image_32f::Out of bound access: (" << x << "," << y << ") [" << properties_.width << "," << properties_.height << "]: " <<  x << ',' << y <<")\n";
			throw ns_ex("ns_intel_image_32f::Out of bound access: (") << x << "," << y << ") [" << properties_.width << "," << properties_.height << "]: " << x << ',' << y << ")";
		}*/
		return buffer[y*line_step_in_pixels + x];
	}

	const Ipp32f
		//xxx
#ifndef NS_DEBUG_IMAGE_ACCESS
		finline
#endif
		sample_d(const float y, const float x) const {

		const int p0x(xs_float::xs_FloorToInt(x)),
			p0y(xs_float::xs_FloorToInt(y));
		const int p1x(p0x + 1),
			p1y(p0y + 1);
		const float dx(x - (float)p0x),
			dy(y - (float)p0y);
		const float d1x(1.0 - dx),
			d1y(1.0 - dy);

#ifdef NS_DEBUG_IMAGE_ACCESS
		if (p0y < 0 || p1y < 0 ||
			p0x < 0 || p1x < 0 ||
			p0y >= properties_.height || p1y >= properties_.height ||
			p0x >= properties_.width || p1x >= properties_.width) {
			std::cerr << "ns_intel_image_32f::Out of bound access: (" << x << "," << y << ") [" << properties_.width << "," << properties_.height << "]: (" << p0x << "," << p0y << ")-(" << p1x << "," << p1y << ")\n";
			throw ns_ex("ns_intel_image_32f::Out of bound access: (") << x << "," << y << "): (" << p0x << "," << p0y << ")-(" << p1x << "," << p1y << ")";
		}
#endif
		//note that accessing val[height-1][width-1]  only works because we allocate an extra 1 pixel right and bottom buffer
		//(see the function init()
		const float d = (d1y)*(d1x)*val(p0y, p0x) +
			(d1y)*(dx)*val(p0y, p1x) +
			(dy)*(d1x)*val(p1y, p0x) +
			(dy)*(dx)*val(p1y, p1x);
	/*	if (std::isnan(d)) {
			std::cerr << "ns_intel_image_32f::Out of bound access: (" << x << "," << y << ") [" << properties_.width << "," << properties_.height << "]: (" << p0x << "," << p0y << ")-(" << p1x << "," << p1y << ")\n";
			std::cerr << dx << " "<< dy<< " " << d1x << " " << d1y << "\n";
		}*/
		return d;
	}


	int  init(const ns_image_properties & prop) {
		return init(prop.width, prop.height);
	}
	void clear() {
		properties_.width = 0;
		properties_.height = 0;
		line_step_in_bytes = 0;
		line_step_in_pixels = 0;
		ns_ippi_safe_delete(buffer);
	}
	int init(const unsigned long int width, const unsigned long int height) {
		if (properties_.width == width && properties_.height == height)
			return line_step_in_bytes;

		clear();
		//SUBTLE THING: We add a one pixel righthand boundary around images
		//to allow subsampling to handle boundaries correctly.
		//calling sample_d[height-1][width-1] will produce the code
		// sample_d[height-1][width-1]*1 + 0*sample_d[height-1][width]+0*sample_d[height][width-1].
		// by allocating that 1 pixel boundary, we prevent accessing unallocated memory
		// while also we never propigating the uninitialized values at sample_d[height][..] or sample_d[..][width]
		// which are multiplied by zero in every case.
		buffer = ippiMalloc_32f_C1((int)(width + 1), (int)(height + 1), &line_step_in_bytes);
		if (buffer == NULL) {
			//ippiFree(buffer);
			throw ns_ex("ns_intel_image_32f::Could not allocate buffer ") << width << "," << height;
		}
		line_step_in_pixels = line_step_in_bytes / sizeof(Ipp32f);
		properties_.width = width;
		properties_.height = height;
		properties_.components = 1;


		//zero out buffer.
		//we need to do this in case any of these pixels have a nan value
		//as 0*nan == nan 
		for (unsigned int y = 0; y < height + 1; y++) {
		//	if (std::isnan(val(y, width)))
			//	std::cerr << "Found NAN";
			val(y, width) = 0;
		}
		for (unsigned int x = 0; x < width + 1; x++) {
			//if (std::isnan(val(height, x)))
			//	std::cerr << "Found NAN"; 
			val(height, x) = 0;
		}


		return line_step_in_bytes;
	}
	void convert(ns_image_whole<float> & im) {
		im.init(properties_);
		for (unsigned int y = 0; y < properties_.height; y++)
			for (unsigned int x = 0; x < properties_.height; x++)
				im[y][x] = val(y, x);
	}
	const ns_image_properties & properties() const {
		return properties_;
	}
	Ipp32f  * buffer;
	int line_step_in_bytes;
	int line_step_in_pixels;
private:
	ns_image_properties properties_;
};


class ns_gaussian_pyramid : public ns_image_stream_sender<ns_8_bit, ns_gaussian_pyramid, unsigned long>{
public:
	ns_gaussian_pyramid() :_properties(0, 0, 0),
		pyrLStateSize(0), pyrLBufferSize(0), max_pyrLStateSize(0), max_pyrLBufferSize(0), pPyrLStateBuf(0), pPyrLBuffer(0),
		pPyrStruct(0), ns_image_stream_sender<ns_8_bit, ns_gaussian_pyramid, unsigned long>(ns_image_properties(0,0,0),this),pPyrBuffer(0), pPyrStrBuffer(0), max_pyrBufferSize(0), max_pyrStructSize(0), pyrBufferSize(0), pyrStructSize(0) {}
	void clear() {
		for (unsigned int i = 0; i < ns_calc_best_alignment_fast::ns_max_pyramid_size; i++)
			image_scaled[i].clear();
		_properties.width = _properties.height = 0;
		ns_image_stream_sender<ns_8_bit, ns_gaussian_pyramid, unsigned long>::_properties = _properties;
		ns_ipps_safe_delete(pPyrLStateBuf);
		ns_ipps_safe_delete(pPyrLBuffer);
		ns_ipps_safe_delete(pPyrStrBuffer);
		ns_ipps_safe_delete(pPyrBuffer);
	}
	~ns_gaussian_pyramid() { clear(); }

	template< class sender_t >
	void recieve_and_calculate(sender_t & source,const int min_downsample) {
		ns_image_properties prop(source.input_stream().properties());
		prop.width /= min_downsample;
		prop.height /= min_downsample;
		allocate(prop);
		ns_image_stream_buffer_properties p;
		p.height = 1; 
		p.width = source.input_stream().properties().width;

		ns_image_stream_static_buffer<ns_8_bit> buffer(p);
		unsigned long source_state = source.input_stream().init_send();
		for (unsigned int y = 0; y < _properties.height; y++) {
			//get line to use
			source.input_stream().send_lines(buffer, 1, source_state);
			for (unsigned int x = 0; x < _properties.width; x++)
				image_scaled[0].val(y, x) = buffer[0][x*min_downsample] / 255.f;
			//throw away next lines as req by downsample factor
			for (int i = 0; i < min_downsample - 1 && min_downsample*y+i < source.input_stream().properties().height; i++)
				source.input_stream().send_lines(buffer, 1, source_state);
		}
		source.input_stream().finish_send();
		calculate();
	}

	template<class T>
	void calculate(const ns_image_whole<T> & im, 
		const ns_vector_2i & subregion_position,
		const ns_vector_2i & subregion_size) {
		ns_image_properties p(im.properties());
		p.height = subregion_size.y;
		p.width = subregion_size.x;
		allocate(p);
		
		//copy over raw image to first layer of pyramid
		for (unsigned int y = 0; y < p.height; y++)
			for (unsigned int x = 0; x < p.width; x++)
				image_scaled[0].val(y, x) = im[y+subregion_position.y][x +subregion_position.x] / 255.f;

		calculate();
	}
	template<class T>
	void calculate(const T & im,
		const ns_vector_2i & subregion_position,
		const ns_vector_2i & subregion_size) {

		ns_image_properties p(im.properties());
		p.height = subregion_size.y;
		p.width = subregion_size.x;
		allocate(p);

		//copy over raw image to first layer of pyramid
		for (unsigned int y = 0; y < p.height; y++)
			for (unsigned int x = 0; x < p.width; x++)
				image_scaled[0].val(y, x) = im[y + subregion_position.y][x + subregion_position.x] / 255.f;
		calculate();
		
	}


	//contains some temporary image data buffers that are kept to avoid reallocating memory each time
	ns_intel_image_32f image_scaled[ns_calc_best_alignment_fast::ns_max_pyramid_size];
	const ns_image_properties & properties() const { return _properties; }
	int num_current_pyramid_levels;

	unsigned long seek_to_beginning() { return 0; }
	unsigned long  init_send() { return 0; }
	void finish_send() {}
	unsigned long init_send_const()const {return 0;}
	void finish_send_const()const {}
	template<class buffer_t>
	void send_lines(buffer_t & buffer, 
							const unsigned int number_of_lines_requested, 
							unsigned long & current_state) {

		for (unsigned int y = 0; y < number_of_lines_requested; y++)
			for (unsigned int x = 0; x < _properties.width; x++) {
				buffer[y][x] = (ns_8_bit)round(255.0 * image_scaled[0].val(y + current_state,x));
			}
		current_state += number_of_lines_requested;
	}



private:
	ns_image_properties _properties;
	void calculate() {
		/* Perform downsampling of the image with 5x5 Gaussian kernel */
		for (int i = 1; i < num_current_pyramid_levels; i++) {
			int status = ippiPyramidLayerDown_32f_C1R(image_scaled[i - 1].buffer, pPyrStruct->pStep[i - 1], pPyrStruct->pRoi[i - 1],
				image_scaled[i].buffer, pPyrStruct->pStep[i], pPyrStruct->pRoi[i], (IppiPyramidDownState_32f_C1R*)pPyrStruct->pState);
			if (status != ippStsNoErr)
				throw ns_ex("Could not calculate pyramid layer ") << i;
		}

	}

	void allocate(const ns_image_properties & image) {

		long min_d(image.width < image.height ? image.width : image.height);
		//if we go resample to images smaller than 16x16 pixels, our gradient calculations
		//we lose the important features we want to align.
		num_current_pyramid_levels = log2(min_d) - 4;
		
		if (num_current_pyramid_levels <= 1) {
			num_current_pyramid_levels = 1;
			//if the image is too small to reasonably build a pyramid, don't do anything.
			image_scaled[0].init(image.width, image.height);
			_properties = image;
			ns_image_stream_sender<ns_8_bit, ns_gaussian_pyramid, unsigned long>::_properties = _properties;
			return;
		}

		/* Computes the temporary work buffer size */
		IppiSize    roiSize = { image.width,image.height };
		float rate(2);
		const int kernel_size = 5;
		Ipp32f kernel[kernel_size] = { 1.f, 4.f, 6.f,4.f,1.f };

		int status = ippiPyramidGetSize(&pyrStructSize, &pyrBufferSize, num_current_pyramid_levels - 1, roiSize, rate);
		if (status != ippStsNoErr)
			throw ns_ex("Could not estimate intel pyramid size for image size") << roiSize.width << ", " << roiSize.height << " levels: " << num_current_pyramid_levels - 1 << " rate: " << rate;
		if (pyrStructSize > max_pyrStructSize) {
			max_pyrStructSize = pyrStructSize;
			ns_ipps_safe_delete(pPyrStrBuffer);
			pPyrStrBuffer = ippsMalloc_8u(pyrStructSize);
		}
		if (pyrBufferSize > max_pyrBufferSize) {
			max_pyrBufferSize = pyrBufferSize;
			ns_ipps_safe_delete(pPyrBuffer);
			pPyrBuffer = ippsMalloc_8u(pyrBufferSize);
		}
		/* Initializes Gaussian structure for pyramids */
		status = ippiPyramidInit(&pPyrStruct, num_current_pyramid_levels - 1, roiSize, rate, pPyrStrBuffer, pPyrBuffer);
		if (status != ippStsNoErr)
			throw ns_ex("Could not make intel pyramid");
		/* Correct maximum scale level */
		/* Allocate structures to calculate pyramid layers */
		status = ippiPyramidLayerDownGetSize_32f_C1R(roiSize, rate, kernel_size, &pyrLStateSize, &pyrLBufferSize);
		if (status != ippStsNoErr)
			throw ns_ex("Could not estimate pyramid layer down size");
		if (pyrLStateSize > max_pyrLStateSize) {
			max_pyrLStateSize = pyrLStateSize;
			ns_ipps_safe_delete(pPyrLStateBuf);
			pPyrLStateBuf = ippsMalloc_8u(pyrLStateSize);
		}
		if (pyrLBufferSize > max_pyrLBufferSize) {
			max_pyrLBufferSize = pyrLBufferSize;
			ns_ipps_safe_delete(pPyrLBuffer);
			pPyrLBuffer = ippsMalloc_8u(pyrLBufferSize);
		}
		/* Initialize the structure for creating a lower pyramid layer */
		status = ippiPyramidLayerDownInit_32f_C1R((IppiPyramidDownState_32f_C1R**)&pPyrStruct->pState, roiSize, rate,
			kernel, kernel_size, IPPI_INTER_LINEAR, pPyrLStateBuf, pPyrLBuffer);
		if (status != ippStsNoErr)
			throw ns_ex("Could not init pyramid layers");
		/* Allocate pyramid layers */

		_properties = image;
		ns_image_stream_sender<ns_8_bit, ns_gaussian_pyramid, unsigned long>::_properties = _properties;
		for (int i = 0; i < num_current_pyramid_levels; i++) {
			if (image_scaled[i].properties().width != pPyrStruct->pRoi[i].width ||
				image_scaled[i].properties().height != pPyrStruct->pRoi[i].height)
				pPyrStruct->pStep[i] = image_scaled[i].init(pPyrStruct->pRoi[i].width, pPyrStruct->pRoi[i].height);
			else
				pPyrStruct->pStep[i] = image_scaled[i].line_step_in_bytes;
		}
		for (int i = num_current_pyramid_levels; i < ns_calc_best_alignment_fast::ns_max_pyramid_size; i++)
			image_scaled[i].clear();
	}

	//intel specific pyramid info
	int pyrBufferSize, max_pyrBufferSize,
		pyrStructSize, max_pyrStructSize;
	IppiPyramid *pPyrStruct;
	Ipp8u       *pPyrBuffer;
	Ipp8u       *pPyrStrBuffer;
	int      pyrLStateSize, max_pyrLStateSize;
	int      pyrLBufferSize, max_pyrLBufferSize;
	Ipp8u   *pPyrLStateBuf;
	Ipp8u   *pPyrLBuffer;
};
#endif