#include "ns_barcode.h"
#include "ns_vector_bitmap_interface.h"
#include "ns_image_tools.h"

#ifdef NS_USE_2D_SCANNER_BARCODES
#include "ns_difference_thresholder.h"
using namespace std;

string run_dmtx_decode(DmtxImage *image){
	DmtxDecode * decode;

	//SetOptionDefaults(&options);
	decode = dmtxDecodeCreate(image,1);

    DmtxRegion * region(dmtxRegionFindNext(decode, NULL));
    if(region == NULL)
		return "";//throw ns_ex("No barcode found");

	DmtxMessage *message(dmtxDecodeMatrixRegion(decode, region, 1));
	if (message == 0)
		return "";//throw ns_ex("Barcode did not encode any information");
	string res;
	for (unsigned int i = 0; message->output[i] != 0; i++)
		res.push_back(message->output[i]);

	dmtxMessageDestroy(&message);
	dmtxRegionDestroy(&region);  
	dmtxDecodeDestroy(&decode);
	return res;
}
string ns_barcode_decode_done(const ns_image_standard & im, const string & debug_image_filename){
	//ns_save_image("c:\\bar_debug.tif",im);
	unsigned long c(im.properties().components);
	unsigned long w(im.properties().width);
	unsigned long h(im.properties().height);

	unsigned char * pxl = new unsigned char[im.properties().width * im.properties().height * c];

	
	try{
	

		if (c == 3)
		for (unsigned int y = 0; y < h; y++){
			for (unsigned int x = 0; x < w; x++){
				pxl[3*(y*w+x)+0] = im[y][3*x];
				pxl[3*(y*w+x)+1] = im[y][3*x+1];
				pxl[3*(y*w+x)+2] = im[y][3*x+2];
			}
		}
		if (c == 1)
			for (unsigned int y = 0; y < h; y++){
				for (unsigned int x = 0; x < w; x++){
					pxl[y*w+x] = im[y][x];
				}
			}
		
		int pack(DmtxPack24bppRGB);
		if (c == 1)	
			pack = DmtxPack8bppK;
		DmtxImage *image(dmtxImageCreate(pxl,im.properties().width, im.properties().height,pack));	

		if (image == NULL)
			throw ns_ex("Could not persuade dmtxlib to allocate an image of dimentions ") << im.properties().width <<"x" << im.properties().height;
		if (image->height != im.properties().height || image->width != im.properties().width)
			throw ns_ex("Requested an image of dimentions ") << im.properties().width <<"x" << im.properties().height << "; dmtx produced an image of dimentions "
				<< image->width  << "x" << image->height;

		string ret =  run_dmtx_decode(image);
		dmtxImageDestroy(&image);
		delete[] pxl;

		//if (ret.size() != 0)
			return ret;

		//if we can't find a barcode, check to see if there has been a mirror-image reflection.
		/*if (c == 3)
		for (unsigned int y = 0; y < h; y++)
			for (unsigned int x = 0; x < w; x++){
				image->pxl[y*w+x][0] = im[y][c*(w-x-1)];
				image->pxl[y*w+x][1] = im[y][c*(w-x-1)];
				image->pxl[y*w+x][2] = im[y][c*(w-x-1)];
			}
		string name(run_dmtx_decode(image));
		*/

		//delete image;
		//return name;
	}
	catch(...){
		delete[] pxl;
		throw;
	}
}
string ns_barcode_decode(const ns_image_bitmap & image, const string & debug_image_filename){
	ns_image_standard im;
	im.init(im.properties());
	for (unsigned int y = 0; y < image.properties().height; y++)
	for (unsigned int x = 0; x < image.properties().width; x++)
		im[y][x] = 255*(ns_8_bit)image[y][x];
	return ns_barcode_decode(im,debug_image_filename);
}

string ns_barcode_decode(const ns_image_standard & image, const string & debug_image_filename){
/*	ns_image_standard im;
	ns_two_stage_difference_thresholder::run(image,im,false);
	long w(im.properties().width);
	int x_buffer = 50;
	w-=2*x_buffer;
	if (w<=0) return "";

	vector<unsigned long> x_profile(w,0);
	long max_x(0), max_val(-1);

	for (unsigned long x = 0; x < w; x++){
		for (long _x = -buffer; _x <= buffer; _x++)
			for (unsigned long y = 0; y < im.properties().height; y++)
				x_profile[x] += im[y][x+buffer];
		if (x_profile[x] > max_val){
			max_val = x_profile[x];
			max_x = x;
		}
	}
	unsigned long start_x,end_x;
	for (start_x = max_x; start_x > 0; start_x--)
		if (x_profile[start_x] <= max_val/10)
			break;
	for (stop_x = max_x; stop_x < w; stop_x++)
		if (x_profile[start_x] <= max_val/10)
			break;
	ns_image_standard im_crop;
*/
	return ns_barcode_decode_done(image,debug_image_filename);
}
#endif


#ifndef NS_USE_2D_SCANNER_BARCODES
string ns_barcode_decode(const ns_image_standard & image, const string & debug_image_filename){
	unsigned char  c = image.properties().components;	
	ns_image_properties p(image.properties());

	/*ns_histogram<unsigned long,ns_8_bit> hist;
	for (unsigned int y = 0; y < p.height;  y++){
		for (unsigned int x = 0; x < p.width; x+=c){
			hist[ image[y][x] ]++;
		}
	}
	
	unsigned int max,min;
	for (min = 0; min < hist.size(); min++)
		if (hist[min] != 0)break;

	for (max = hist.size(); max > 1; max--)
		if (hist[max] != 0)break;*/
	unsigned char thresh = 125;

	//build the vertical intensity profile of the image
	//we're looking for the top and bottom of the barcode strip.
	vector<unsigned int> profile(p.height,0);
	for (unsigned int y = 0; y < p.height; y++)
		for (unsigned int x = 0; x < c*p.width; x+=c)
			profile[y]+=image[y][x] >= thresh;
	
	vector<unsigned int> profile_smoothed(p.height,0);
	ns_smooth_series<unsigned int, 48>(profile,profile_smoothed,0);
	
	int bottom(0), 
		top((int)profile_smoothed.size()-1);

	for (int i = (int)profile_smoothed.size()/2; i >=0; i--){
		if (profile[i] >= profile[bottom])
			bottom = i;
	}
	for (int i = (int)profile_smoothed.size()/2; i < (int)profile_smoothed.size(); i++){
		if (profile[i] >= profile[top])
			top = i;
	}
	unsigned int b_old(bottom),
				 t_old(top);
	if (b_old >= t_old)
		throw ns_ex("ns_barcode_decode::Could not register barcode (vertical)");

	bottom = b_old;//bottom + (t_old-b_old)/4;
	top = t_old;//top - (t_old-b_old)/4;

	unsigned int b_marge = bottom + (t_old-b_old)/4,
				 t_marge = top    - (t_old-b_old)/4;

	//calculate median intensity of bright area
	ns_histogram<unsigned long,ns_8_bit> hist;
	for (int y= bottom; y < top; y++)
		for (unsigned int x = 0; x < p.width; x++)
			hist[image[y][c*x]]++;
	hist.set_number_of_pixels((top-bottom)*p.width);
	ns_8_bit bright = (ns_8_bit)((3*(unsigned int)hist.median_from_histogram())/5);

	//calculate 10th percentile
	unsigned int tenth(0);
	unsigned int tenth_count(0);
	for (tenth = 0; tenth< hist.size(); tenth++){
		tenth_count+= hist[tenth] > 50;
		if (tenth_count > 10)
			break;
	}

	//bright = tenth;

	//find start of bright region at left
	unsigned int l_old(0),r_old(p.width);
	//find l_old edge
	for (unsigned int x = 0; x < p.width/2; x++){
		unsigned int sum = 0;
		for (unsigned int y = b_marge; y < t_marge; y++)
			sum+= image[y][c*x];
		if (sum >= bright*( t_marge-b_marge)){
			l_old = x;
			break;
		}
		if (l_old != 0) 
			break;
	}

	//find the end of the dark region (the barcode) at right
	unsigned int dark_cut = tenth*3*( t_marge-b_marge);
	for (int x = l_old; x < (int)p.width ; x++){
		unsigned int sum = 0;
		for (unsigned int y =  b_marge; y < t_marge; y++)
			sum+= image[y][c*x];
		if (sum <= dark_cut){
			r_old = x;
		}
	}

	//find r_old edge
	/*for (int x = (int)p.width-1; x > (int)p.width/2 ; x--){
		unsigned int sum = 0;
		for (unsigned int y =  b_marge; y < t_marge; y++)
			sum+= image[y][c*x];
		if (sum >= bright*( t_marge-b_marge)){
			r_old = x;
			break;
		}
		if (r_old != p.width)
			break;
	}*/

	/*for (unsignj
	unsigned long window = p.width/12;
	int start = l_old - window;
	int stop =  r_old + window;
	if (start < 0)
		start = window;
	if (stop > p.width)
		stop = p.width - window;

	vector<unsigned int> scores(stop-start);
	for (int x = start; x < stop; x++){
		unsigned int vertical_mean(0);
		unsigned int vertical_varience(0);
		
			for (unsigned int y = bottom; y < top; y++)
				vertical_mean+=image[y][x+window];
		vertical_mean/=((2*window+1)*(top-bottom));
	

	}*/

	
	unsigned int left = (11*l_old)/10,
	//			 right = r_old - ((11*(p.width-r_old))/10);
		right = r_old + (p.width-r_old)/25;
	if (left >= right)
		throw ns_ex("ns_barcode_decode::Could not register barcode (horizontal)");
	
	string dbg2_filename;
	if (debug_image_filename.size() != 0){
		ns_image_standard im;
		image.pump(im,1024);
		for (int y = bottom; y < top;  y++){
			im[y][left] = 255*(y%2);
			im[y][right] = 255*(y%2);
		}
		for (unsigned int x = left; x < right; x++){
			im[bottom][x] = 255*(x%2);
			im[top][x] = 255*(x%2);
		}
		/*
		ns_image_storage_reciever_handle<ns_8_bit> processing_out(image_server.image_storage.request_volatile_storage(debug_image_filename,1024,false));
		im.pump(processing_out.output_stream(),1024);	
		dbg2_filename = ns_dir::extract_filename_without_extension(debug_image_filename) + "2." + ns_dir::extract_extension(debug_image_filename);*/
	}
	


	ns_image_bitmap bmp;
	ns_image_properties bprop(p);
	bprop.components = 1;
	bprop.width = right-left;
	bprop.height = top-bottom;
	bmp.prepare_to_recieve_image(bprop);

	const unsigned int read_height(4);

	for (unsigned int y = 0; y < read_height;  y++)
		for (unsigned int x = 0; x < bprop.width; x++)
		bmp[y][x] = 0;

	for (unsigned int y = read_height; y < bprop.height-read_height;  y++){
		for (unsigned int x = 0; x < bprop.width; x++){
			const int b(y - read_height);
			const int t(y + read_height);
			int sum(0);
			for (int i = b; i < t; i++)
				sum+= image[i+bottom][c*left+c*x];
			bmp[y][x] = (sum <= (t-b)*thresh);

		}
	}
	for (unsigned int y = 0; y < read_height; y++)
		for (unsigned int x = 0; x < bprop.width; x++)
		bmp[bprop.height-1-y][x] = 0;


	return ns_barcode_decode(bmp,dbg2_filename);
}

string ns_barcode_decode(const ns_image_bitmap & image, const string & debug_image_filename){
	
	vector<unsigned int> bar_widths;
	char state = 0;
	unsigned int bar_start_pos = 0;
	
	int max_dark = 0;
	int h = 0;
	for (unsigned int y = 0; y < image.properties().height; y++){
		int cur(0);
		for(unsigned int x = 0; x < image.properties().width; x++)
			cur+=image[y][x];
		if (cur > max_dark){
			max_dark = cur;
			h = y;
		}
	}

	/*unsigned int max_black = 0;
	//find the height at which the barcode starts
	for (unsigned int y = 0; y < image.properties().height;  y++){
		unsigned int line_black = 0;
		for (unsigned int x = 0; x < image.properties().width; x++)
			line_black+=image[y][x];
		if (line_black > max_black){
			h = y;
			max_black = line_black;
		}
	}*/
	//get the width of the bars


	for (unsigned int x = 0; x < image.properties().width; x++){
		if (state == 0){
			if (image[h][x]){
				bar_start_pos = x;
				state = 1;
			}
		}
		else{
			if (!image[h][x]){
				bar_widths.push_back(x - bar_start_pos);
				state = 0;
			}
		}
	}
	unsigned int max_width = 0,
				 min_width = image.properties().width;
	for (unsigned int i = 0; i < bar_widths.size(); i++){
		if (bar_widths[i] > max_width)
			max_width = bar_widths[i];
		if (bar_widths[i] < min_width)
			min_width = bar_widths[i];
	}
	ns_barcode_fitter fitter(min_width, (min_width*2 + max_width/2)/2, max_width);
	string val;
	val.resize(bar_widths.size()/3);
	for (unsigned int i = 0; (int)i < (int)bar_widths.size()-2; i+=3){
		char v = 	9*fitter.fit(bar_widths[i  ]) +
					3*fitter.fit(bar_widths[i+1]) +
					  fitter.fit(bar_widths[i+2]);
		if (v == 26)
			 val[i/3] = '_';
		else val[i/3] = 'a' + v;
	}


	if (debug_image_filename.size() != 0){
		ns_image_standard im;
		image.pump(im,1024);
		for (unsigned int y = 0; y < im.properties().height; y++)
			for (unsigned int x = 0; x < im.properties().width; x++)
				im[y][x] = 255-128*im[y][x];
		for (unsigned int x = 0; x < im.properties().width; x++)
			im[h][x] = 0;		
/*		ns_image_storage_reciever_handle<ns_8_bit> processing_out(image_server.image_storage.request_volatile_storage(debug_image_filename,1024,false));
		im.pump(processing_out.output_stream(),1024);*/
	}
	return val;
}
#endif


void ns_barcode_encoder::encode(const string & filename, const vector<string> & labels, bool tight_pack){
	ns_tiff_image_output_file<ns_8_bit> tiff_out;
	vector<ns_image_standard> images(labels.size());
	vector<const ns_image_standard *> images_p(labels.size());
	for (unsigned int i = 0; i < images.size(); i++){
		encode(labels[i],images[i],20);
		images_p[i] = &images[i];
	}
	ns_image_stream_file_sink<ns_8_bit> sink(filename,tiff_out,512,1.0);

	if (tight_pack)
		ns_compile_collage(images_p, sink, 512, 1, 255);
	else ns_compile_collage(images_p, sink, 512, 1, 255,false,(float)labels.size());
}

void ns_barcode_encoder::encode(const string & str, ns_image_standard & image, const unsigned int margin_size){
	#ifdef NS_USE_2D_SCANNER_BARCODES
		DmtxEncode * encode;
		encode = dmtxEncodeCreate();
		unsigned char * a(new unsigned char[str.size()+1]);
		try{
			for (unsigned int i = 0; i < str.size(); i++)
				a[i] = str[i];
			a[str.size()] = 0;
			cerr << "Encoding " << a << "\n";
			dmtxEncodeDataMatrix(encode, (int)str.size(), a);
			int width(dmtxImageGetProp(encode->image, DmtxPropWidth)),
				height(dmtxImageGetProp(encode->image, DmtxPropHeight)),
				c = dmtxImageGetProp(encode->image, DmtxPropBytesPerPixel);

			image.prepare_to_recieve_image(ns_image_properties(height+2*margin_size,
															 width+2*margin_size,
															 c,150));
			if (c == 1){
				for (unsigned int y = 0; y < image.properties().height; y++)
					for (unsigned int x = 0; x < image.properties().width; x++)
						image[y][x] = 255;

				for (unsigned int y = margin_size; y < image.properties().height-margin_size; y++)
					for (unsigned int x = margin_size; x < image.properties().width-margin_size; x++)
						image[y][x] = encode->image->pxl[encode->image->width*(y-margin_size) + (x-margin_size)];
			}
			if (c == 3){
				for (unsigned int y = 0; y < image.properties().height; y++)
					for (unsigned int x = 0; x < image.properties().width; x++)
						for (unsigned int c = 0; c < 3; c++)
						image[y][3*x+c] = 255;

				for (unsigned int y = margin_size; y < image.properties().height-margin_size; y++)
					for (unsigned int x = margin_size; x < image.properties().width-margin_size; x++)
						for (unsigned int c = 0; c < 3; c++)
							image[y][3*x+c] = encode->image->pxl[3*encode->image->width*(y-margin_size) + 3*(x-margin_size)+c];
						
			}
			ns_acquire_lock_for_scope font_lock(font_server.default_font_lock, __FILE__, __LINE__);
			ns_font & font(font_server.get_default_font());
			font.set_height(16);
			font.draw(margin_size+10,margin_size + encode->image->height + 10,ns_color_8(125,125,125),str,image);
			font_lock.release();
			
			dmtxEncodeDestroy(&encode);
			delete[] a;
		}
		catch(...){
			
			dmtxEncodeDestroy(&encode);
			delete[] a;
			throw;
		}
	#else
	const unsigned w[3] = {3,6,12};
	const unsigned int spacing = w[1];

	vector <unsigned int> bar_widths;

	for (unsigned int i = 0; i < str.size(); i++){
		unsigned int v;
		if (str[i] == '_')
			v = 26;
		else v = str[i]-'a';
		bar_widths.push_back( w[(v/9)%3]);
		bar_widths.push_back( w[(v/3)%3]);
		bar_widths.push_back( w[(v  )%3]);
	}
	unsigned int total_width = 0;
	for(unsigned int i = 0; i < bar_widths.size(); i++)
		total_width += bar_widths[i] + spacing;
	ns_image_properties prop;
	prop.width = total_width + 2*margin_size;
	prop.height = height*2 + 2*margin_size;
	prop.components = 1;
	prop.resolution = 150;
	image.prepare_to_recieve_image(prop);
	for (unsigned int y = 0; y < prop.height; y++)
		for (unsigned int x = 0; x < prop.width; x++)
			image[y][x] = 255;
	unsigned int x = 0;
	for (unsigned int i = 0; i < bar_widths.size(); i++){
		for (unsigned int y = 0; y < height; y++)
			for (unsigned int dx = 0; dx < bar_widths[i]; dx++)
				image[y+margin_size][x+margin_size+dx] = 0;
		x += bar_widths[i];
		for (unsigned int y = 0; y < height; y++)
			for (unsigned int dx = 0; dx < spacing; dx++)
				image[y+margin_size][x+margin_size+dx] = 255;
		x += spacing;
	}
	font.draw(margin_size,height*3/2+margin_size,ns_color_8(0,0,0),str,image);
	#endif

}
