#include "ns_image.h"

#include "ns_dir.h"
#include "ns_jpeg.h"
#include <stack>

using namespace std;

ns_image_type ns_image_type_from_filename(const string & filename){
	string ext = ns_dir::extract_extension(filename);
	for (unsigned int i = 0; i < (unsigned int)ext.size(); i++)
		ext[i] = tolower(ext[i]);
	if (ext == "jpeg" || ext == "jpg")
		return ns_jpeg;
	if (ext == "tif" || ext == "tiff")
		return ns_tiff;
	if (ext == "jp2" | ext == "jpk")
		return ns_jp2k;
	if (ext == "wrm")
		return ns_wrm;
	if (ext == "csv")
		return ns_csv;
	if (ext == "xml")
		return ns_xml;
	return ns_unknown;

}

unsigned int _c = 0;
void debug_bitmap_output(ns_image_bitmap & bitmap,unsigned int _l){
	ns_jpeg_image_output_file<ns_8_bit> j;
	string filename = "c:\\debug\\debug_l" + ns_to_string(_l) + "_" + ns_to_string(_c) + ".jpg";
	ns_image_stream_file_sink<ns_8_bit> output(filename,j, .8,512);
	bitmap.pump(output,512);
	_c++;
};

void ns_add_image_suffix(string & str, const ns_image_type & type){
	switch(type){
		case ns_jpeg: str+=".jpg"; return;
		case ns_tiff: str+=".tif"; return;
		case ns_tiff_lzw: str+=".tif"; return;
		case ns_tiff_zip: str+=".tif"; return;
		case ns_tiff_uncompressed: str += ".tif"; return;
		case ns_jp2k: str+=".jp2"; return;
		case ns_wrm: str += ".wrm"; return;
		case ns_csv: str += ".csv"; return;
		case ns_xml: str += ".xml"; return;
		default: throw ns_ex("ns_add_image_suffix: Unknown image type: ") << (unsigned int)type;
	}
}

#ifdef _WIN32 
HBITMAP ns_create_GDI_bitmap(const ns_image_standard * image, const HDC device){
		BITMAPINFO bmpinfo;  
		bmpinfo.bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpinfo.bmiHeader.biWidth = image->properties().width; 
		bmpinfo.bmiHeader.biHeight = image->properties().height;
		bmpinfo.bmiHeader.biPlanes = 1; 
		bmpinfo.bmiHeader.biBitCount = 24;
		bmpinfo.bmiHeader.biCompression = BI_RGB;
		bmpinfo.bmiHeader.biSizeImage = 0; 
		bmpinfo.bmiHeader.biXPelsPerMeter = 0;
		bmpinfo.bmiHeader.biYPelsPerMeter = 0;
		bmpinfo.bmiHeader.biClrUsed = 0;
		bmpinfo.bmiHeader.biClrImportant = 0;

		unsigned int h = image->properties().height;
		unsigned int w = image->properties().width;
		ns_8_bit * buf = image->to_raw_buf();

		HBITMAP res = CreateDIBitmap(device,&bmpinfo.bmiHeader,CBM_INIT,buf,&bmpinfo,DIB_RGB_COLORS);
		delete buf;
		return res;
	}

BITMAPINFO * ns_create_GDI_bitmapinfo(const ns_image_standard * image){  
		long len =  3*sizeof(ns_8_bit)*image->properties().width*image->properties().height,
			 header_size = sizeof(BITMAPINFOHEADER) ;//+ 256*sizeof(RGBQUAD);
		HANDLE hand = GlobalAlloc(GHND,header_size + len);
		if (!hand){
			ns_ex ex("Could not allocate clipboard memory: ");
			ex.append_windows_error();
			throw ex;
		}

		BITMAPINFO * bmpinfo = (BITMAPINFO *)GlobalLock(hand);
		bmpinfo->bmiHeader.biSize = sizeof(BITMAPINFOHEADER);
		bmpinfo->bmiHeader.biWidth = image->properties().width; 
		bmpinfo->bmiHeader.biHeight = image->properties().height;
		bmpinfo->bmiHeader.biPlanes = 1; 
		bmpinfo->bmiHeader.biBitCount = 24;
		bmpinfo->bmiHeader.biCompression = BI_RGB;
		bmpinfo->bmiHeader.biSizeImage = len;
		bmpinfo->bmiHeader.biXPelsPerMeter = 0;
		bmpinfo->bmiHeader.biYPelsPerMeter = 0;
		bmpinfo->bmiHeader.biClrUsed = 0;
		bmpinfo->bmiHeader.biClrImportant = 0;
		/*for(unsigned int i = 0; i< 256; i++)
                bmpinfo->bmiColors[i].rgbBlue  = 
                        bmpinfo->bmiColors[i].rgbGreen =
                        bmpinfo->bmiColors[i].rgbRed   = i;*/
                    
		unsigned int h = image->properties().height;
		unsigned int w = image->properties().width;

		image->to_raw_buf(true,0,&((ns_8_bit *)bmpinfo)[header_size],true);

        GlobalUnlock(hand);
		return bmpinfo;
	}


#ifdef NS_TRACK_PERFORMANCE_STATISTICS
ns_performance_statistics_analyzer ns_image_allocation_performance_stats;
#endif

#endif
