#include "ns_difference_thresholder.h"
#include "ns_image_tools.h"
#include "ns_tiff.h"

#define NS_THRESH_FAST

template<class ns_component>
ns_component ns_safe_get(const ns_image_whole<ns_component> & input,const int w, const int h, int x, int y){
	if (x < 0) 
		x = 0;
	if (y < 0)
		y = 0;
	if (x >= w)
		x = w-1;
	if (y >= h)
		y = h-1;
	return input[y][x];
}



void ns_process_dynamic_stretch(ns_image_standard & im){
	unsigned char new_top(250),
				  new_bottom(155);
	unsigned char up(255-new_top);
	unsigned char diff(new_top - new_bottom);

	for (unsigned int y = 0; y < im.properties().height; y++){
		for (unsigned int x = 0; x < im.properties().width; x++){
			unsigned char c(255-im[y][x]);
			if (c>new_top)c=new_top;
			else if (c<new_bottom)c=new_bottom;
			im[y][x] = (ns_8_bit)((255*(long)(c-new_bottom))/diff);
		}
	}
}
