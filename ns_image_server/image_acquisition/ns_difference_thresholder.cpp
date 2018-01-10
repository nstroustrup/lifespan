#include "ns_difference_thresholder.h"


using namespace std;
ns_two_stage_difference_parameters::ns_two_stage_difference_parameters(){
	  strict_height=80;
	  strict_absolute_threshold=20;
	  strict_radius=0;
	  permissive_height=0; 
	  permissive_absolute_threshold=10;
	  permissive_radius=0;
}


void ns_difference_thresholder::run(const ns_image_standard & im, ns_image_standard & output, const unsigned int difference_threshold, const unsigned int kernel_half_width, const unsigned int absolute_threshold){
		ns_image_whole<ns_32_bit> diff;
		//cerr << "(" <<  difference_threshold << "," << kernel_half_width << "," << absolute_threshold << ")\n";
			
		const long w(im.properties().width);
		const long h(im.properties().height);

		//cerr << "Allocating image " << w << "x" << h << "."<< input.properties().components << "\n";
		output.prepare_to_recieve_image(im.properties());

		if (kernel_half_width == 0){
			for (long y = 0; y < h; y++)
				for (long x = 0; x < w; x++)
					output[y][x] = 255*(im[y][x] >= absolute_threshold);
			return;
		}

		long khr(kernel_half_width);
		long k_area((2*khr+1)*(2*khr+1));
		ns_image_whole<ns_32_bit> val;
		val.prepare_to_recieve_image(im.properties());
		unsigned long max_val(0);
		for (long y = 0; y < khr; y++)
			for (long x = 0; x < w; x++)
				val[y][x] = 0;

		for (long y = khr; y < h-khr; y++){
			//empty left margin
			for (long y_ = -khr; y_<= khr; y_++) 
				for (long x = 0; x < khr; x++)
					val[y+y_][x] = 0;

			//fill up first row
			long lp(0);
			for (long y_ = -khr; y_ <= khr; y_++) 
				for (long x = -khr; x <= khr; x++)
					lp-=im[y+y_][khr+x];
			if (im[y][khr] < absolute_threshold)
				val[y][khr] = 0;
			else{
				long lp2((k_area+1)*im[y][khr] + lp);
				val[y][khr] = (lp2*(lp2>0));
				if (val[y][khr] > max_val)max_val = val[y][khr];
			}


			for (long  x = khr+1; x < w-khr; x++){
				for (long y_ = -khr; y_ <= khr; y_++){
						lp+=im[y+y_][x-khr-1];
						lp-=im[y+y_][x+khr];
				}
				if (im[y][x] < absolute_threshold)
					val[y][x] = 0;
				else{
					long lp2((k_area+1)*im[y][x] + lp);
					val[y][x] = (lp2*(lp2>0));
					if (val[y][x] > max_val && val[y][x] > absolute_threshold)max_val = val[y][x];
				}
			}

			//fill right margin
			for (long y_ = -khr; y_ <= khr; y_++) 
				for (long x = w-khr; x < w; x++)
					val[y+y_][x] = 0;
		}
		for (long y = h-khr; y < h; y++)
			for (long x = 0; x < w; x++)
				val[y][x] = 0;

		for (long y = 0; y < h; y++)
			for (long x = 0; x < w; x++)
				output[y][x] = (1000*val[y][x])/max_val > difference_threshold;
		
		
		//calculate difference map
		//center
/*		ns_image_standard diff;

		diff.prepare_to_recieve_image(im.properties());	
		const unsigned int w(input.properties().width);
		const unsigned int h(input.properties().height);

		//cerr << "Allocating image " << w << "x" << h << "."<< input.properties().components << "\n";
		diff.prepare_to_recieve_image(input.properties());	
		output.prepare_to_recieve_image(input.properties());

		const int diff_kernel_half_width(kernel_half_width);

		const int tot((2*diff_kernel_half_width+1)*(2*diff_kernel_half_width+1)-2);
		const int offset = difference_threshold;

		
		
#ifdef NS_THRESH_FAST
		for (int y = diff_kernel_half_width; y < (int)(h-diff_kernel_half_width); y++){
			//load in val
			int val = 0;
			for (int _y = -diff_kernel_half_width; _y <= diff_kernel_half_width; _y++)
				for (int _x = -diff_kernel_half_width; _x <= diff_kernel_half_width; _x++)
					val -=input[y+_y][diff_kernel_half_width+_x];

			diff[y][diff_kernel_half_width] = 255*(tot*input[y][diff_kernel_half_width] + val > offset);

			for (unsigned int x = diff_kernel_half_width+1; x < w-diff_kernel_half_width; x++){

				
				for (int _y = -diff_kernel_half_width; _y <= diff_kernel_half_width; _y++){
					val+=input[y+_y][x-diff_kernel_half_width-1];
					val-=input[y+_y][x+diff_kernel_half_width];
				}
					//for (int _x = -diff_kernel_half_width; _x <= diff_kernel_half_width; _x++)
					//	val -=input[y+_y][x+_x];

				diff[y][x] = 255*(tot*input[y][x] + val > offset);
			}
		}
#else
		for (int y = diff_kernel_half_width; y < (int)(h-diff_kernel_half_width); y++){
			for (unsigned int x = diff_kernel_half_width; x < w-diff_kernel_half_width; x++){

				int val = 0;
				
				for (int _y = -diff_kernel_half_width; _y <= diff_kernel_half_width; _y++)
					for (int _x = -diff_kernel_half_width; _x <= diff_kernel_half_width; _x++)
						val -=input[y+_y][x+_x];

				diff[y][x] = 255*(tot*input[y][x] + val > offset);
			}
		}
#endif
		//left edge
		
		for (int y = 0; y < (int)h; y++){				
			for (int x = 0; x < (int)diff_kernel_half_width; x++){
				int val = 0;
				for (int _y = -diff_kernel_half_width; _y <= diff_kernel_half_width; _y++)
					for (int _x = -diff_kernel_half_width; _x <= diff_kernel_half_width; _x++)
						val -=ns_safe_get(input,w,h,x+_x,y+_y);

				diff[y][x] = 255*(tot*ns_safe_get(input,w,h,x,y) + val > offset);
			}
		}
		//right edge
		
		for (int y = 0; y < (int)h; y++){
			for (int x = (int)((int)w-(int)diff_kernel_half_width); x < (int)w; x++){
				int val = 0;
				for (int _y = -diff_kernel_half_width; _y <= diff_kernel_half_width; _y++)
					for (int _x = -diff_kernel_half_width; _x <= diff_kernel_half_width; _x++)
						val -=ns_safe_get(input,w,h,x+_x,y+_y);

				diff[y][x] = 255*(tot*ns_safe_get(input,w,h,x,y) + val > offset);
			}
		}		
		//top edge
		
		for (int y = 0; y < diff_kernel_half_width; y++){
			for (int x = 0; x < (int)w; x++){
				int val = 0;
				for (int _y = -diff_kernel_half_width; _y <= diff_kernel_half_width; _y++)
					for (int _x = -diff_kernel_half_width; _x <= diff_kernel_half_width; _x++)
						val -=ns_safe_get(input,w,h,x+_x,y+_y);

				diff[y][x] = 255*(tot*ns_safe_get(input,w,h,x,y) + val > offset);
			}
		}
		//bottom edge
		
		for (int y = (int)(h-diff_kernel_half_width); y < (int)h; y++){
			for (int x = 0; x < (int)w; x++){
				int val = 0;
				for (int _y = -diff_kernel_half_width; _y <= diff_kernel_half_width; _y++)
					for (int _x = -diff_kernel_half_width; _x <= diff_kernel_half_width; _x++)
						val -=ns_safe_get(input,w,h,x+_x,y+_y);

				diff[y][x] = 255*(tot*ns_safe_get(input,w,h,x,y) + val > offset);
			}
		}

		diff.pump(output,128);*/
		return;
	}

