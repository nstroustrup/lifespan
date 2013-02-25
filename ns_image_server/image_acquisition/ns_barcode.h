#ifndef NS_WORM_BARCODE_H
#define NS_WORM_BARCODE_H
#include "ns_ex.h"
#include "ns_image_tools.h"
#include "ns_tiff.h"
#include "ns_font.h"

//#define NS_USE_2D_SCANNER_BARCODES
#define NS_USE_2D_SCANNER_BARCODES

#ifdef NS_USE_2D_SCANNER_BARCODES
#include "dmtx.h"
#endif

class ns_barcode_fitter{
public:
	ns_barcode_fitter(unsigned int s, unsigned int m, unsigned int l)
		{widths[0] = s; widths[1] = m, widths[2] = l;}

	//given a bar width, returns the value 1,2, or 3 to which its closest
	unsigned char fit(unsigned int width){
		unsigned int min_dist= width,
					 index = 0;
		for (unsigned int i = 0; i < 3; i++){
			unsigned int d = abs((int)width - (int)widths[i]);
			if (d < min_dist){
				min_dist = d;
				index = i;
			}
		}
		return index;
	}
private:
	unsigned int widths[3];
};

std::string ns_barcode_decode(const ns_image_standard & image, const std::string & debug_image_filename="");
std::string ns_barcode_decode(const ns_image_bitmap & image, const std::string & debug_image_filename="");

class ns_barcode_encoder{
public:
	ns_barcode_encoder(const unsigned int h):height(h){}

	void encode(const std::string & filename, const std::vector<std::string> & labels, bool tight_pack = false);

	void encode(const std::string & str, ns_image_standard & image, const unsigned int margin_size = 0);

private:
	const unsigned height;
};


#endif
