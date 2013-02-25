#include "ns_jpeg.h"
#include <vector>
#include <algorithm>
#define MEDIAN
using namespace std;

void usage(char * exename){
	cerr << "Usage: " << exename << " [kernal size] [inputfile] [outputfile]\n";
	exit(1);
}
#if 0

jpeg_error_mgr global_jerr;

image_stats global_stats;

template <class T>
inline T linear_median(vector<T> &v){
	  typename vector<T>::iterator med = v.begin() + v.size()/2;
	  nth_element(v.begin(),med,v.end());
      return *med;
}

//produces a 1 high, width wide array that represents
//the spatial average.  The kernal size is thus kernal_width by height 
template <class T, class ns_component>
inline void area_median(const T & in_buffer, const unsigned int & height, const unsigned int &width, const unsigned int &kernal_width, ns_component * out_buf){
	int k2_width = kernal_width/2;
	//left edge
	#ifdef MEDIAN
	unsigned int sum;
	#endif
	cerr <<"1";
	for (unsigned int x = 0; x < k2_width; x++){

		#ifdef MEDIAN
		vector<ns_component> vals( (k2_width + x)*height);
		#else
		sum = 0;
		#endif

		for (unsigned int rx = 0; rx < k2_width + x; rx++)
			for (unsigned int y = 0; y < height; y++){
				#ifdef MEDIAN
				vals[rx + (k2_width + x)*y] = in_buffer[y][rx];
				#else
				sum += in_buffer[y][rx];
				#endif
		}
		#ifdef MEDIAN
		out_buf[x] = linear_median(vals);
		#else
		out_buf[x] = sum/((k2_width + x)*height);
		#endif
	}

	cerr <<"2";
	//center
	for (unsigned int x = k2_width; x < width-k2_width; x++){
		#ifdef MEDIAN
		vector<ns_component> vals(height*kernal_width);
		#else
		sum = 0;
		#endif

		for (unsigned int rx = x - k2_width; rx <= x + k2_width; rx++){
			for (unsigned int y = 0; y < height; y++){
				#ifdef MEDIAN
				vals[(rx - (x - k2_width)) + (kernal_width)*y] = in_buffer[y][rx];
				#else
				sum += in_buffer[y][rx];
				#endif
			}
		}
		#ifdef MEDIAN
		out_buf[x] = linear_median(vals);
		#else
		out_buf[x] = sum/(height*kernal_width);
		#endif
	}


	cerr <<"3";
	//right edge
	for (unsigned int x = width-k2_width; x < width; x++){
			#ifdef MEDIAN
			vector<ns_component> vals( (k2_width + (width -  x + k2_width)*height));
			#else
			sum = 0;
			#endif

			for (unsigned int rx = x-k2_width; rx < width; rx++){
				for (unsigned int y = 0; y < height; y++)
					#ifdef MEDIAN
					vals[(rx - (x - k2_width)) + (width -  x + k2_width)*y] = in_buffer[y][rx];
					#else
					sum += in_buffer[y][rx];
					#endif
			}
			#ifdef MEDIAN
			out_buf[x] = linear_median(vals);
			#else
			out_buf[x] = sum/(height*kernal_width);
			#endif
		}

	cerr <<"4";
}



class report_status{
	public:
	inline report_status(const unsigned int & _max):last(0),max(_max){}

	inline void update(const unsigned int & cur){
		//cerr <<".";
		if ((100*cur)/max- last >= 1){
			last = (100*cur)/max;
			cerr<< last << "%...";
		}
	}
	unsigned int last, max;

};


template<class ns_component>
class ns_spatial_median_calculator : public ns_image_stream_processor<class ns_component>{
public:				
	ns_spatial_median_calculator(const long max_line_block_height, const long _kernal_height, const ns_image_properties & properties):
	  kernal_height(_kernal_height),
	  kernal_half_height(_kernal_height/2),
	  in_buf(max_line_block_height + _kernal_height -1),  //the largest possible buffer size occurs when reading the first line,
													   //when _kernal_height - 1 lines have previously been recieved (necissitating their buffering
													   //and then max_line_block_height is recieved.
	  ns_image_stream_processor(properties, max_line_block_height){
		  if (kernal_height % 2 != 1) throw ns_ex("only odd Kernal sizes may be specified.");

		  //allocate memory.
		  for (unsigned int i = 0; i < max_line_block_height + _kernal_height -1; i++)
			  in_buf.raw_access(i) = new ns_component[_properties.width*_properties.components];
	  }
	~ns_spatial_median_calculator(){
		//deallocate memory.
		  for (unsigned int i = 0; i < _max_line_block_height + kernal_height -1; i++)
			  delete[] in_buf.raw_access(i);
	}

	void provide_buffer(ns_image_stream_lines<ns_component> & lines){
		lines.lines = &(sliding_buffer[input_buffer_height]);

	}
	void recieve_lines(ns_image_stream_lines<ns_component> & lines){
		lines_recieved += lines.height;
		//we're reading in the first few lines
		if (lines_processed < kernal_height){
			//if we don't have enough lines to start processing, do nothing.
			//Otherwise, process and send the top edge.
			if (lines_recieved >= kernal_height){				
				//get output buffer from reciever
				ns_image_stream_lines<ns_component> output_lines;
				output_lines.height = kernal_height/2;
				output_lines.width = lines.width;
				output_reciever->provide_buffer(output_lines);
				//process input buffer
				for (unsigned int y = 0; y < kernal_half_height; y++){
					area_median(in_buf, kernal_half_height + 1 +y, lines.width, kernal_size, *output_lines.lines);
					rep.update(y);
				}
				//we have now processed kernal_size/2+1 lines.

				output_reciever->recieve_lines(output_lines);
				lines_processed+=kernal_height/2+1;

				//we don't need the first line of the image any more!
				sliding_buffer.step();
			}
		}

		//we're reading in the last few lines
		if (_properties.height - lines_processed < kernal_height){

		}
		//we're reading in the middle.
		else{
			//We need to keep kernal_height/2 lines above the current line in our buffer at all times
			//these lines are necissary to fill the kernal for the current line!
			long lines_to_process =  recieved_lines - processed_lines - kernal_half_height;

			//detect if we're hitting the end of the file
			if (_properties.height - kernal_height < lines_to_process + lines_processed)
				lines_to_process = _properties.height - kernal_height - lines_processed;

			ns_image_stream_lines<ns_component> output_lines;
			output_lines.height = lines_to_process;
			output_lines.width = lines.width;
			output_reciever->provide_buffer(output_lines);

			for (long i = 0; i < lines_to_process; lines_to_process++){
					area_median(in_buf, kernal_half_height + 1 +y, lines.width, kernal_size, &(*output_lines.lines[i]));
					sliding_buffer.step();
					rep.update(y);
			}
			//we have now processed kernal_size/2+1 lines.

			output_reciever->recieve_lines(output_lines);
			lines_processed+=lines_to_process;

			//we don't need the first line of the image any more!

		}
	}
	void init(const ns_image_properties & properties){
		lines_recieved = 0;
		lines_processed = 0;
	}
    
private:
	const unsigned long kernal_height,
					    kernal_half_height;
	unsigned long input_buffer_height;
	unsigned long lines_recieved,
			 long lines_processed;

	//used to store initial lines as kernal_height lines must be read in before processing can start.
	sliding_buffer<ns_component *> in_buf;
	report_status rep;
};


int main(int argc, char ** argv){
	if (argc != 4)
		usage(argv[0]);
	cout << "Input file: " << argv[2] << "; Output file: " << argv[3] << endl;

	ns_image_stream imstream(&global_jerr,argv[2],argv[3]);
	int kernal_size = atoi(argv[1]);
	int kernal_half_size = kernal_size/2;

	image_stats stat = imstream.stats();
	cout <<"Image size = " << stat.image_width << "x" << stat.image_height << "; Kernal Size = " << kernal_size << endl;

	//allocate output buffer
	JSAMPLE * out_buf = new JSAMPLE[stat.image_components*stat.image_width];

	if (kernal_size > stat.image_height){
		cerr << "Your kernal is higher than the image.";
		exit(1);
	}
	if (kernal_size > stat.image_width){
		cerr << "Your kernal is wider than the image.";
		exit(1);
	}
	if (kernal_size <= 0){
		cerr << "You must specify a positive kernal size";
		exit(1);
	}

	//allocate input buffer
	cout << "Allocating Memory...";
	sliding_buffer<JSAMPLE *> in_buf(kernal_size);
	for (unsigned int i = 0; i < kernal_size; i++)
		in_buf[i] = new JSAMPLE[stat.image_components*stat.image_width];
	cout << "Processing Image..." << endl;

	report_status rep(stat.image_height);

	//read in top edge and first normal row
	for (unsigned int y = 0; y < kernal_size; y++)
		imstream.read_line(in_buf[y]);


	//process the top edge
	for (unsigned int y = 0; y < kernal_half_size; y++){
		area_median(in_buf, kernal_half_size+y, stat.image_width, kernal_size, out_buf);
		imstream.write_line(out_buf);
		rep.update(y);
	}

	//process the first line
	//buf height width kernal_width out
	area_median(in_buf, kernal_size, stat.image_width, kernal_size, out_buf);
	imstream.write_line(out_buf);

	rep.update(kernal_half_size);


	int last = 0;
	for (unsigned int lines_read = kernal_size; lines_read < stat.image_height; lines_read++){
		int pos = (100*lines_read)/stat.image_height;


		//discard first line in buffer and read in new next line.
		imstream.read_line(in_buf.step());
		area_median(in_buf, kernal_size, stat.image_width, kernal_size, out_buf);
		imstream.write_line(out_buf);
		rep.update(lines_read - kernal_size);
	}

	//process bottom edge
	for (int y = 1; y <= kernal_half_size; y++){
		in_buf.step();
		area_median(in_buf, kernal_size-y, stat.image_width, kernal_size, out_buf);
		imstream.write_line(out_buf);
		rep.update(stat.image_height - kernal_size + y);
	}
	imstream.close();
	cerr << "\n";
	return 0;

}
#endif

