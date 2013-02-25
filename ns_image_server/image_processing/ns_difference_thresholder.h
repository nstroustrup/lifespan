#ifndef NS_DIFFERENCE_THRESHOLDER_H
#define NS_DIFFERENCE_THRESHOLDER_H
#include "ns_image.h"
#include "ns_image_tools.h"

typedef enum{ns_threshold_one_stage, ns_threshold_two_stage} ns_threshold_type;

class ns_difference_thresholder{
public:
	static void run(const ns_image_standard & input, ns_image_standard & output, const unsigned int difference_threshold, const unsigned int kernel_half_width, const unsigned int absolute_threshold);
};

struct ns_two_stage_difference_parameters{
	ns_two_stage_difference_parameters();
	unsigned long strict_height,
				  strict_absolute_threshold,
				  strict_radius,
				  permissive_height,
				  permissive_absolute_threshold,
				  permissive_radius;
};

class ns_two_stage_difference_thresholder{
public:
	template<class ns_component>
	static void run(const ns_image_whole<ns_component> & im, ns_image_whole<ns_component> & out, const ns_two_stage_difference_parameters & dp=ns_two_stage_difference_parameters(), const bool make_color_vis=false){

		//unsigned long start_time = ns_current_time();

		ns_image_whole<ns_component> skeleton_thresh;
		std::cerr << "0% ";
		ns_difference_thresholder::run(im,skeleton_thresh,dp.strict_height,dp.strict_radius,dp.strict_absolute_threshold);
		std::cerr << "25% ";
		ns_remove_small_objects<ns_component,12>(skeleton_thresh);
		std::cerr << "37.5% ";

		ns_image_whole<ns_component> skeleton_dialated;
		ns_dilate<7>(skeleton_thresh,skeleton_dialated);

		std::cerr << "50% ";
		ns_image_whole<ns_component> body_thresh;
		ns_difference_thresholder::run(im,body_thresh,dp.permissive_height,dp.permissive_radius,dp.permissive_absolute_threshold);
		ns_remove_small_objects<ns_component,5>(body_thresh);

		ns_image_properties prop(im.properties());

		std::cerr << "75% ";
		const unsigned int h( im.properties().height),
							w( im.properties().width);
		if (make_color_vis){
			prop.components = 3;
			out.prepare_to_recieve_image(prop);
			for (unsigned int y = 0; y < h; y++)
				for (unsigned int x = 0; x < w; x++){
					if (skeleton_thresh[y][x]){
						out[y][3*x] = 175;
						out[y][3*x+1] = 0;
						out[y][3*x+2] = 0;
					}
					else if (skeleton_dialated[y][x] && body_thresh[y][x]){
						out[y][3*x] = 255;
						out[y][3*x+1] = 0;
						out[y][3*x+2] = 0;
					}
					else{
						out[y][3*x] = 0;
						out[y][3*x+1] = 255*(body_thresh[y][x]!=0);
						out[y][3*x+2] = 255*(skeleton_dialated[y][x]!=0);
					}
				}
		}
		else{
			ns_image_whole<ns_component> tmp;
			out.prepare_to_recieve_image(prop);
			for (unsigned int y = 0; y < h; y++)
				for (unsigned int x = 0; x < w; x++)
					out[y][x] = 255*(ns_component)((skeleton_dialated[y][x] && body_thresh[y][x]));
			ns_close<1>(out,tmp);
			ns_erode<1>(tmp,out);
			for (unsigned int y = 0; y < h; y++)
				for (unsigned int x = 0; x < w; x++)
					out[y][x]*=255;
		}

		std::cerr << "100%";
		//cerr << ns_current_time() - start_time << "sec\n";
	}
};




template <ns_threshold_type threshold_type, class read_image, class write_image>
void ns_movement_threshold(const read_image & image, const read_image & time_lapse_image,write_image & output_image, const bool & produce_color_visualization=false){
	if (time_lapse_image.properties() != image.properties())
		throw ns_ex("ns_movement_threshold::Source image and time lapse image are different sizes.");
	if (time_lapse_image.properties().components != 1)
		throw ns_ex("ns_movement_threshold::Cannot calculate movement threshold on color images");
	read_image temp;
	temp.prepare_to_recieve_image(image.properties());
	for (unsigned int y = 0; y < image.properties().height; y++){
		for (unsigned int x = 0; x < image.properties().width; x++){
			const int diff = abs((int)time_lapse_image[y][x]-(int)image[y][x]);
			if (diff > 10)
				 temp[y][x] =image[y][x];
			else
				temp[y][x] = 0;
		}
	}

	read_image movement_threshold;

	if (threshold_type == ns_threshold_one_stage){
		//find foreground regions in movement image
		ns_difference_thresholder::run(temp, movement_threshold,900,5,0);
		//find foreground regions in image image
		ns_difference_thresholder::run(image,temp,900,5,0);
	}
	else if (threshold_type == ns_threshold_two_stage){
		ns_two_stage_difference_thresholder::run(temp,movement_threshold);
		ns_two_stage_difference_thresholder::run(image,temp);
	}

	//combine both images
	if (produce_color_visualization){
		ns_image_properties prop = image.properties();
		prop.components = 3;
		output_image.prepare_to_recieve_image(prop);
		for (unsigned int y = 0; y < image.properties().height; y++){
			for (unsigned int x = 0; x < image.properties().width; x++){
				output_image[y][3*x] = 255*(movement_threshold[y][x] > 0);
				output_image[y][3*x+1] = temp[y][x];
				output_image[y][3*x+2] = temp[y][x];
			}
		}
	}
	else{
		output_image.prepare_to_recieve_image(image.properties());
		for (unsigned int y = 0; y < image.properties().height; y++){
			for (unsigned int x = 0; x < image.properties().width; x++)
				output_image[y][x] = 255*((movement_threshold[y][x] > 0) || (temp[y][x] > 0));

		}
	}
}

#endif
