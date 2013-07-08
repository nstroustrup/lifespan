#include "ns_barcode.h"
#include "ns_image_easy_io.h"
#include "dmtx.h"
using namespace std;
int main(int argc, char ** argv){
	if (argc < 2){
		cerr << "(To read a barcode) Usage: " << argv[0] << "[input image filename]\n";
		cerr << "(To write barcodes) Usage: " << argv[0] << " -c [output image filename] [label 1] [label 2] ...\n";
		return 1;
	}
	
	try{
		//create a barcode, 
		if (string(argv[1]) == "-c"){
			vector<string> labels;
			for (unsigned int i = 3; i < argc; i++)
				labels.push_back(argv[i]);
			if (labels.empty())
				throw ns_ex("No labels were specified.  Please specify at least one label name at command prompt.");
			cout << "Attempting to write barcodes to the file " << argv[2] << ".\n";
			ns_barcode_encoder encoder(60);
			encoder.encode(argv[2],labels,true);

			return 0;
		}
		else{
			//read a barcode
			cout << "Attempting to interpret the barcode located in the file " <<  argv[1] << ".\n";
			ns_image_standard bar_image;
			ns_load_image(argv[1],bar_image);
			cout <<"Decoded Message: " << ns_barcode_decode(bar_image, argv[1]) << "\n";
		}
		char a;
		cin >> a;
		return 0;
	}
	catch(ns_ex & ex){
		cerr << ex.text() << "\n";			
		char a;
		cin >> a;
		return 1;
	}	
}