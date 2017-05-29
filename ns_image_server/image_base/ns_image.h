#ifndef NS_IMAGE_H
#define NS_IMAGE_H

#include "ns_ex.h"
#include "xs_Float.h"
#include <string>

#include "ns_image_stream_buffers.h"

typedef enum{ns_delete_volatile,ns_delete_long_term,ns_delete_both_volatile_and_long_term} ns_file_deletion_type;

///don't allow pictures 2 gigapixel+ pictures to be
///stored in main memory
#define NS_IMAGE_WHOLE_MAXIMUM_AREA 2*1024.0*1024.0*1024.0

typedef enum{ns_jpeg, ns_tiff, ns_tiff_uncompressed,ns_tiff_lzw,ns_tiff_zip,ns_jp2k,ns_wrm, ns_csv, ns_xml,ns_unknown} ns_image_type;
void ns_add_image_suffix(std::string & str, const ns_image_type & type);

ns_image_type ns_image_type_from_filename(const std::string & filename);

#include "ns_vector.h"

#pragma warning(disable: 4355)
///
///this structure contains information that must be specified for any image passing through the system.
///The quality property is only meaningful for compressed images.
///
struct ns_image_properties{
	ns_image_properties():height(0),width(0),components(0),resolution(0){}
	ns_image_properties(unsigned long h, unsigned long w, unsigned char c,float r=-1):height(h),width(w),components(c),resolution(r){}
	bool operator==(const ns_image_properties & l) const{
		return (height == l.height) && (width == l.width) && (components == l.components);
	}
	bool operator!=(const ns_image_properties & l) const{
		return !(*this == l);
	}

	unsigned long height,
				  width;

	//resolution (in pixels per inch)
	float resolution;

	unsigned char components;
	std::string description;
};

///
///this is a simple image structure that reads data from a file.
///
template<class ns_component>
class ns_image_input_file{
public:
	//~ns_image_input_file(){close();}
	//open and close files
	virtual void open_file(const std::string & filename)=0;
	virtual void open_mem(const void *)=0;
	virtual void close()=0;
	//read in a single line
	virtual bool read_line(ns_component * buffer) =0 ;
	//read a position/component from the specified buffer.
	virtual ns_component * operator()(const unsigned long x, const  unsigned int component, ns_component * buffer)const=0 ;
	//read in multiple lines
	const unsigned int read_lines(ns_component ** buffer,const unsigned int n);

	virtual unsigned long seek_to_beginning()=0;

	const long image_line_buffer_length() const {return _properties.width*_properties.components;}

	const ns_image_properties & properties() const {return _properties;}

protected:
	ns_image_properties _properties;
};
///
///a simple structure that writes data into a file
///
template<class ns_component>
class ns_image_output_file{
public:
	//open and close files
	virtual void open_file(const std::string & filename, const ns_image_properties & properties, const float compression_ratio)=0;
	virtual void open_mem(const void *, const ns_image_properties & properties)=0;
	virtual void close()=0;

	//write a single line
	virtual bool write_line(const ns_component * buffer)=0;
	virtual void write_lines(const ns_component ** buffer,const unsigned int n)=0;

	//read a position/component from the specified buffer.
	virtual ns_component * operator()(const unsigned long x, const unsigned int component, ns_component * buffer)const=0;

	const unsigned int image_line_buffer_length() const {return _properties.width*_properties.components;}

	ns_image_properties & properties() {return _properties;}
protected:
	ns_image_properties _properties;
};


///
///ns_image_stream_reciever recieves an image stream.
///
template<class storage_buffer>
class ns_image_stream_reciever{
public:
	ns_image_stream_reciever(const long max_line_block_height, const ns_image_stream_reciever * reciever):_max_line_block_height(max_line_block_height),_properties(ns_image_properties(0,0,0)){}
	typedef storage_buffer storage_buffer_type;

	///verify that derived objects can instantiate the ability
	///to provide buffers and recieve them.
	void verify_constraints(const ns_image_stream_reciever * reciever){

		storage_buffer * (*provide_buffer)(const ns_image_stream_buffer_properties & buffer_properties) = & reciever->provide_buffer;
		void (*recieve_lines)(const storage_buffer & lines, const unsigned long height) = &reciever->recieve_lines;
	}

	///tells the stream to prepare to recieve a file of a certain width/height/etc
	virtual void prepare_to_recieve_image(const ns_image_properties & properties){_properties = properties;init(properties);}

	virtual void finish_recieving_image()=0;
	//to recieve a message, first the reciever must provide a buffer into which the data will be written.
	//virtual read_buffer * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties)=0;

	//after the data is written to the provide_buffer() buffer, recieve_lines() is called.
	//virtual void recieve_lines(const read_buffer & lines, const unsigned long height)=0;

	const ns_image_properties & properties() const {return _properties;}
	long line_block_height(){return _max_line_block_height;}

	storage_buffer * output_buffer;
protected:
	//should implement any code needed to prepare for a new file being received
	virtual bool init(const ns_image_properties & properties)=0;
	ns_image_properties _properties;
	unsigned long _max_line_block_height;

private:
	//dissalow default copy constructor (causes no end of problems if derived classes aren't particularly careful about the initialization order of _properties)
	ns_image_stream_reciever(const ns_image_stream_reciever<storage_buffer> & reciever);

};
///
///This structure represents a source of an image stream.  It sends lines to
///ns_image_stream_reciever.
///line_block_height represents the number of lines it sends at a time.
///setting line_block_height = -1 signifies that the object can send any number of lines at once
///
template <class ns_component, class sender_t, class sender_internal_state_t>
class ns_image_stream_sender{
public:


	ns_image_stream_sender(const ns_image_properties & properties, const sender_t * sender):_properties(properties){verify_constraints(sender);}
	typedef ns_component component_type;
	typedef sender_internal_state_t internal_state_t;
	///stream senders need to be able to "send" image lines, ie write
	///them to a supplied buffer.  This function checks to see that
	///derived types can instantiate this behavior for at least one buffer type.

	void verify_constraints(const sender_t * sender){
		void (sender_t::*send_lines)(ns_image_stream_static_buffer<ns_component> &, const unsigned int, internal_state_t &) = &sender_t::send_lines;
	}

	virtual internal_state_t seek_to_beginning() = 0;

	///ns_image_stream_sender needs to be fill the specified buffer upon request.
	///n is a recommendation; less lines may be returned.
	///void send_lines(write_buffer & lines, unsigned int count)=0;

	template<class reciever_t>
	void pump(reciever_t & reciever, const unsigned int block_height) const{
		internal_state_t state = init_send_const();
		pump(&reciever, block_height,state);
		finish_send_const();
	}
	template<class reciever_t>
	void pump(reciever_t & reciever, const unsigned int block_height){
		internal_state_t state = init_send();
		pump(&reciever, block_height, state);
		finish_send();
	}

	const unsigned int image_line_buffer_length() const {return _properties.width*_properties.components;}

	inline ns_image_properties & properties() {return _properties;}
	inline const ns_image_properties & properties() const {return _properties;}
	//should implement any code needed to prepare for a new file being sent
	virtual internal_state_t  init_send() = 0;
	virtual void finish_send(){}
	virtual internal_state_t init_send_const()const = 0;
	virtual void finish_send_const()const {}
protected:

	ns_image_properties _properties;
	unsigned long _max_line_block_height;
private:
	template<class reciever_t>
	void pump(reciever_t * reciever, const unsigned int block_height, sender_internal_state_t & state)const {
		reciever->prepare_to_recieve_image(_properties);
		ns_image_stream_buffer_properties buf_prop;

		buf_prop.width = _properties.width*_properties.components;

		for (unsigned long lines_sent = 0; lines_sent < _properties.height;) {
			unsigned long lines_to_send = _properties.height - lines_sent;
			if (lines_to_send > block_height)
				lines_to_send = block_height;

			buf_prop.height = lines_to_send;
			//get the reciever's buffer into which data will be written.
			reciever->output_buffer = reciever->provide_buffer(buf_prop);
			//write data to the buffer
			//downcast this class into its derived class and call the send_lines.
			//Note, this is actually safe, because we've already checked that the function
			//exists in check_constraints.
			//If a derived class lies about its type (passes an incorect type in as reciever_t,
			//bad things will happen.
			const sender_t * this_down = static_cast<const sender_t*>(this);
			this_down->send_lines(*(reciever->output_buffer), lines_to_send,state);
			//inform the reciever that the data has been written.
			reciever->recieve_lines(*reciever->output_buffer, lines_to_send);
			lines_sent += lines_to_send;
		}
		reciever->finish_recieving_image();
	}

	template<class reciever_t>
	void pump(reciever_t * reciever, const unsigned int block_height, sender_internal_state_t & state) {
		reciever->prepare_to_recieve_image(_properties);
		ns_image_stream_buffer_properties buf_prop;

		buf_prop.width = _properties.width*_properties.components;

		for (unsigned long lines_sent = 0; lines_sent < _properties.height;) {
			unsigned long lines_to_send = _properties.height - lines_sent;
			if (lines_to_send > block_height)
				lines_to_send = block_height;

			buf_prop.height = lines_to_send;
			//get the reciever's buffer into which data will be written.
			reciever->output_buffer = reciever->provide_buffer(buf_prop);
			//write data to the buffer
			//downcast this class into its derived class and call the send_lines.
			//Note, this is actually safe, because we've already checked that the function
			//exists in check_constraints.
			//If a derived class lies about its type (passes an incorect type in as reciever_t,
			//bad things will happen.
			sender_t * this_down = static_cast<sender_t*>(this);
			this_down->send_lines(*(reciever->output_buffer), lines_to_send, state);
			//inform the reciever that the data has been written.
			reciever->recieve_lines(*reciever->output_buffer, lines_to_send);
			lines_sent += lines_to_send;
		}
		reciever->finish_recieving_image();
	}

	//dissalow default copy constructor (causes no end of problems if derived classes aren't particularly careful about the initialization order of _properties)
	ns_image_stream_sender(const ns_image_stream_sender<ns_component, sender_t, internal_state_t> & sender);
};

///
///ns_image_stream_file_sourcewraps an ns_image_input_file and allows it to act as an
///image_stream_sender.
///
template<class ns_component>
  class ns_image_stream_file_source : public ns_image_stream_sender<ns_component, ns_image_stream_file_source<ns_component>, unsigned long> {
public:
	ns_image_stream_file_source(ns_image_input_file<ns_component> & input_file):/*lines_sent(0),*/ns_image_stream_sender<ns_component, ns_image_stream_file_source<ns_component>,unsigned long >(input_file.properties(),this)
		{file = &input_file;}
	typedef ns_image_stream_static_buffer<ns_component> storage_type;
	typedef ns_component component_type;

	//ns_image_stream_sender needs to be able to produce a block of lines upon request.
	//n is a recommendation; less lines may be returned.
	 template<class write_buffer>
	 void send_lines(write_buffer & lines, const unsigned int count, unsigned long & state){
		for (unsigned int i = 0; i < count; i++)
			file->read_line(lines[i]);
	}
	unsigned long init_send() { return 0; }
	unsigned long init_send_const()const{ return 0; }
	void finish_send(){
		file->close();
	}
	unsigned long seek_to_beginning(){
		return file->seek_to_beginning();
	}

protected:
//	unsigned long lines_sent;
	ns_image_input_file<ns_component> * file;
};
///
///ns_image_stream_file_sink wraps an ns_image_output_file and allows it to
///recieve data as an ns_image_stream_reciever.
///
template<class ns_component>
class ns_image_stream_file_sink : public ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >{

public:
	ns_image_stream_file_sink(const std::string & filename, ns_image_output_file<ns_component> & output_file, const long max_line_block_height, const float compression_ratio_)
		:compression_ratio(compression_ratio_),_filename(filename),file_opened(false),ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >(max_line_block_height,this),file(&output_file){}

	typedef ns_image_stream_static_buffer<ns_component> storage_type;
	typedef ns_component component_type;
	//prepare to recieve lines by opening the file.
	bool init(const ns_image_properties & properties){
		if (file_opened)
			file->close();
		//on empty images, clear buffer and return.
		if (properties.width ==0 || properties.height == 0){
			buffer.resize(ns_image_stream_buffer_properties(0,0));
			return true;
		}
		//cerr << "Opening file " << _filename << " for " << properties.height << " lines.\n";
		file->open_file(_filename,properties, compression_ratio);
		file_opened = true;

		//allocate buffer
		ns_image_stream_buffer_properties bprop;
		bprop.height = ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >::_max_line_block_height;
		bprop.width = properties.width*properties.components;
		buffer.resize(bprop);
		return false;
	}

	~ns_image_stream_file_sink(){
		try{
			if (file_opened)
				file->close();
		}
		catch(ns_ex & ex){
			std::cerr << "Could not close file properly: " << ex.text() << "\n";
		}
		catch(...){
			std::cerr << "Unknown error encountered while closing file.\n";
		}
	}
	ns_image_stream_static_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		//buffer_properties is ignored as this data has been specified by init
		return &buffer;
	}
	//called by ns_image_stream_sender to notify the reciever it should recieve the lines.
	void recieve_lines(const ns_image_stream_static_buffer<ns_component> & buffer, const unsigned long height){
		//cerr << "Writing " << height << " lines\n";
		for (unsigned int i = 0; i < height; i++)
			file->write_line(buffer[i]);
	}

	void finish_recieving_image(){
		if (file_opened){
			file->close();
			file_opened = false;
		}
	}
protected:
	float compression_ratio;
	ns_image_output_file<ns_component> * file;
	ns_image_stream_static_buffer<ns_component> buffer;
	bool file_opened;
	std::string _filename;
};

///used to enforce constraints on ns_image_stream derived classes.
template<class storage_t>
class ns_verify_image_stream_processor_type : public ns_image_stream_reciever<storage_t>{
public:
	ns_verify_image_stream_processor_type():ns_image_stream_reciever<storage_t>(0,this){}

	storage_t * provide_buffer(const ns_image_stream_buffer_properties & p){
		return 0;
	}
	void recieve_lines(const storage_t & t, const unsigned long height){
	}
};
///
///ns_image_stream_processor takes from an image stream, processes it
///and sends it out again to the reciever stream set in bind()
///
template<class processor_t>
class ns_image_stream_processor{
public:
	ns_image_stream_processor(const long max_line_block_height, const processor_t * processor):_max_line_block_height(max_line_block_height){check_constraints(processor);}

	//typedef typename processor_t::component_type component_type;
	//typedef typename processor_t::storage_type storage_type;
	typedef processor_t processor_type;

	  inline void check_constraints(const processor_t * processor){

			typename processor_t::storage_type * (processor_t::*provide_buffer)(const ns_image_stream_buffer_properties & , ns_verify_image_stream_processor_type< typename processor_t::storage_type > & ) = &processor_t::provide_buffer;
			void (processor_t::*recieve_and_send_lines)(const typename processor_t::storage_type & , const unsigned long height, ns_verify_image_stream_processor_type< typename processor_t::storage_type > & ) = &processor_t::recieve_and_send_lines;
			void (processor_t::*prepare_to_recieve_image)(const ns_image_properties & , ns_verify_image_stream_processor_type< typename processor_t::storage_type > & reciever)  = &processor_t::prepare_to_recieve_image;
			void (processor_t::*finish_recieving_image)(ns_verify_image_stream_processor_type< typename processor_t::storage_type > & ) = &processor_t::finish_recieving_image;
	  }

	//template<class storage_buffer, class reciever_t>
	//void recieve_and_send(const storage_buffer & lines, const unsigned long height, reciever_t & reciever);
	template<class reciever_t>
	inline void default_prepare_to_recieve_image(const ns_image_properties & properties, reciever_t & reciever){
		_properties = properties;
		init(properties);

		reciever.prepare_to_recieve_image(_properties);
	}
	inline ns_image_properties & properties() {return _properties;}
	inline const ns_image_properties & properties() const {return _properties;}

protected:
	ns_image_properties _properties;
	virtual bool init(const ns_image_properties & properties)=0;
	unsigned long _max_line_block_height;
};

template<class T1, class T2> struct ns_can_copy {
	static void constraints(T1 a, T2 b) { T2 c = a; b = a; }
	ns_can_copy() { void(*p)(T1,T2) = constraints; }
};

template<class processor_t, class reciever_t>
class ns_image_stream_binding : public ns_image_stream_reciever<typename processor_t::storage_type>{
public:
	typedef typename processor_t::storage_type storage_type;

	ns_image_stream_binding(processor_t & processor, reciever_t & reciever, const unsigned long max_line_block_height):_processor(&processor),_reciever(&reciever),ns_image_stream_reciever<typename processor_t::storage_type>(max_line_block_height,this){}
	ns_image_stream_binding(const unsigned long max_line_block_height):_processor(0),_reciever(0),ns_image_stream_reciever<typename processor_t::storage_type>(max_line_block_height,this){}

	void bind(processor_t * processor, reciever_t * reciever){
		_processor = processor;
		_reciever = reciever;
	}

	typename processor_t::storage_type * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		return _processor->provide_buffer(buffer_properties, *_reciever);
	}
	void recieve_lines(const typename processor_t::storage_type & lines, const unsigned long height){
		_processor->recieve_and_send_lines(lines,height,*_reciever);
	}

	void prepare_to_recieve_image(const ns_image_properties & properties){
		_processor->prepare_to_recieve_image(properties, *_reciever);
	}
	bool init(const ns_image_properties & p){return false;}

	void finish_recieving_image(){
		_processor->finish_recieving_image(*_reciever);
	}

private:
	processor_t * _processor;
	reciever_t * _reciever;
};



///
///ns_histogram storage type scales with the number of pixels in the histogram
///ie if you have 10 pixels you'll only need a char because the maximum
///number of pixels in any column of the histogram is 10.
///

template<class storage_type, class ns_component>
class ns_histogram{
public:
	ns_histogram():hist(max_pixel_depth,0),N(0){
		//s = 2^(8*sizeof(ns_component))
		unsigned int s = 1;
		for (unsigned i = 0; i < 8*sizeof(ns_component); i++)
			s*= 2;
		length = s;
		if (length >= max_pixel_depth)
			throw ns_ex("ns_histogram::Pixel depth is larger than memory allocated--increase allocation.");
		//hist.resize(s);
		//cerr << "Histogram has " << s << " elements!";
	}
	void clear(){
		N = 0;
		for (unsigned int i = 0; i < max_pixel_depth; i++)
			hist[i] = 0;
	}
	inline ns_component median(bool ignore_zero = false) const{
		storage_type c_area = N;
		if (ignore_zero) c_area-=hist[0];
		if (c_area == 0) return 0;
		storage_type half(c_area/2);

		storage_type c(0);
		for (unsigned int i = ignore_zero?1:0; i < length; i++){
			c += hist[i];
			if (c >= half)
				return i;
		}
		throw ns_ex("ns_histogram::median_from_histogram_ignore_zero::Not enough pixels!");
	}

	double mean(bool ignore_zero = false) const{
		storage_type c_area = N;
		if (ignore_zero) c_area-=hist[0];
		if (c_area == 0) return 0;

		ns_64_bit mean(0);
		for (unsigned int i = ignore_zero?1:0; i < length; i++)
			mean+=hist[i]*(storage_type)i;
		return (double)(mean/(ns_64_bit)c_area);
	}
	double variance(double mean_=-1,bool ignore_zero = false) const{
		storage_type c_area = N;
		if (ignore_zero) c_area-=hist[0];
		if (c_area == 0) return 0;
		if (mean_==-1) mean_ = mean(ignore_zero);
		double v(0);
		for (unsigned int i = ignore_zero?1:0; i < length; i++)
			v += hist[i]*((double)i-mean_)*((double)i-mean_);
		return v/c_area;
	}
	double skewness(double mean_=-1,double variance_=-1,bool ignore_zero = false) const{
		storage_type c_area = N;
		if (ignore_zero) c_area-=hist[0];
		if (c_area == 0) return 0;
		if (mean_==-1) mean_ = mean(ignore_zero);
		if (variance_==-1) variance_ = variance(mean_);
		double s(0);
		for (unsigned int i = ignore_zero?1:0; i < length; i++)
			s += hist[i]*((double)i-mean_)*((double)i-mean_)*((double)i-mean_);
		return (s/sqrt(variance_*variance_*variance_))/c_area;
	}
	double entropy(bool ignore_zero=false) const{
		storage_type c_area = N;
		if (ignore_zero) c_area-=hist[0];
		if (c_area == 0) return 0;
		double e(0);
		for (unsigned int i = ignore_zero?1:0; i < length; i++){
			const double p(hist[i]/(double)c_area);
			if (p == 0) continue;
			e-=p*log(p);
		}
		return e;
	};
	inline const double average_of_ntile(const unsigned long ntile, const unsigned long ntile_number_of_divisions, bool ignore_zero = false) const{
		if (ntile_number_of_divisions <= ntile)
			throw ns_ex("Invalid ntile specification: ") << ntile_number_of_divisions << ","<< ntile;
		//calculate apropriate area
		storage_type c_area = N;
		if (ignore_zero) c_area-=hist[0];

		//calculate the width of the ntile, and the area of the curve beneath the ntile.
		storage_type ntile_width = c_area/(storage_type)ntile_number_of_divisions;
		storage_type start=ntile_width*ntile;

		if (ntile_width == 0) return 0;


		storage_type under_count(0);
		ns_64_bit cur_ntile_sum(0);
		storage_type  cur_ntile_width(0);
		//u and o stored for debugging only
		ns_64_bit u(0),o(0);
		for (unsigned int i = (ignore_zero?1:0); i < length; i++){

			//go through the histogram until you find the start of the requested ntile.
			if (under_count < start){
				//if we find the bottom of the percentile, add whatever's left in the current histogram value to the ntile data
				if (under_count+hist[i] > start){
					ns_64_bit overflow(hist[i]+under_count-start);
					if (overflow > ntile_width)
						return (double)i;  //the entire ntile exists in the current histogram level
					cur_ntile_sum+=(overflow)*(storage_type)i;
					cur_ntile_width+=overflow;
					o+=(overflow);
					u+=start-under_count;
				}
				else u+=hist[i];
				under_count+=hist[i];
			}
			else{
				if (cur_ntile_width+hist[i] >= ntile_width){
					cur_ntile_sum+=(ntile_width-cur_ntile_width)*(storage_type)i;
					return cur_ntile_sum/(double)ntile_width;
				}
				cur_ntile_width+=hist[i];
				cur_ntile_sum+=hist[i]*(storage_type)i;
				o+=hist[i];
			}
		}
		if (cur_ntile_width >= ntile_width)
			return cur_ntile_sum/(double)ntile_width;
		throw ns_ex("Error calculating ntile");
	}
	inline storage_type & operator[](const unsigned long i){
		return hist[i];
	}
	inline const storage_type & operator[](const unsigned long i) const{
		return hist[i];
	}

	inline void increment(const unsigned long i){
		N++;
		hist[i]++;
	}
	inline void decrement(const unsigned long i){
		N--;
		hist[i]--;
	}
	inline const unsigned long & size() const{
		return length;
	}
	inline const unsigned long & number_of_pixels() const { return N;}
private:
	enum {max_pixel_depth = 256*256};
	std::vector<storage_type> hist;
	unsigned long N;
	unsigned long length;
};

#ifdef _WIN32
	template<class ns_component>
	class ns_image_whole;
	typedef ns_image_whole<ns_8_bit>  ns_image_standard;
	HBITMAP ns_create_GDI_bitmap(const ns_image_standard * image, const HDC device);
	BITMAPINFO * ns_create_GDI_bitmapinfo(const ns_image_standard * image);
#endif

///
///ns_whole_image implements in-memory image storage.  It can act as a image stream reciever
///or sender using pump() and recieve() interfaces, but also allows random access of image pixels via its [] operator
///
template<class ns_component>
class ns_image_whole: public ns_image_stream_reciever<ns_image_stream_static_offset_buffer<ns_component> >,
					  public ns_image_stream_sender<ns_component, ns_image_whole<ns_component>, unsigned long> {
public:
  typedef ns_image_stream_sender<ns_component,ns_image_whole<ns_component>,unsigned long> sender_t;
  typedef ns_image_stream_reciever<ns_image_stream_static_offset_buffer<ns_component> > reciever_t;
	typedef ns_component component_type;
	typedef ns_image_stream_static_offset_buffer<ns_component> storage_type;


	//initialize as an empty image
	ns_image_whole():avoid_memory_reallocations(false),ns_image_stream_reciever<ns_image_stream_static_offset_buffer<ns_component> >(0,this),
	  ns_image_stream_sender<ns_component,ns_image_whole<ns_component>, typename sender_t::internal_state_t >(ns_image_properties(0,0,0),this),
							lines_received(0),lines_sent(0){}

	ns_image_whole<ns_component>(const ns_image_whole<ns_component> & w):ns_image_stream_reciever<ns_image_stream_static_offset_buffer<ns_component> >(0,this),
	  ns_image_stream_sender<ns_component,ns_image_whole<ns_component>,typename sender_t::internal_state_t >(ns_image_properties(0,0,0),this),avoid_memory_reallocations(false),
							lines_received(0),lines_sent(0){
		w.pump(*this,1024);
	}

	//deconstruction of buffer handled by buffer class

	//STREAM_RECIEVER ABILITIES
	//allocate buffer for entire image and prepare to recieve it
	bool init(const ns_image_properties & properties){
		bool resized(false);
		ns_image_stream_buffer_properties prop;
		prop.height = properties.height;
		prop.width = properties.width*properties.components;
		if ((properties.width != 0 && properties.height != 0) && properties.components != 1 && properties.components != 3)
			throw ns_ex("ns_whole_image::Invalid component specification: ") << properties.components;
		if (prop.height*prop.width > NS_IMAGE_WHOLE_MAXIMUM_AREA)
			throw ns_ex("ns_whole_image::Cannot allocate an image with ") << prop.width << "x" << prop.height << " dimentions.";

		//cerr << "Resizing buffer to " << prop.width << "," << prop.height << "\n";
		if ( !(image_buffer.properties() == prop)){
			if (image_buffer.properties().width != prop.width || image_buffer.properties().height != prop.height){
				if (avoid_memory_reallocations)
					image_buffer.wasteful_resize(prop);
				else image_buffer.resize(prop);
				resized = true;
			}

		}
		///XXX Removing template specifications does not generate an error here
		//NS_SR_PROP
		reciever_t::_properties = sender_t::_properties = properties;
		lines_received = 0;
		image_buffer.set_offset(0);
		return resized;
	}

	inline bool resize(const ns_image_properties & properties){return init(properties);}

	ns_image_stream_static_offset_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		image_buffer.set_offset(lines_received);
		return &image_buffer;
	}


	//after the data is written to the provide_buffer() buffer, recieve_lines() is called.
	void recieve_lines(const ns_image_stream_static_offset_buffer<ns_component> & lines, const unsigned long height){
		lines_received+=height;
	}
	typename sender_t::internal_state_t init_send() { return 0; }
	typename sender_t::internal_state_t init_send_const() const { return 0; }

	///increases the canvas size of the image (ie doesn't change the current image but addes a margin around it)
	void inline increase_size(const ns_image_properties & prop){
		if (prop == this->properties())
			return;
		if (prop.height < this->properties().height)
			throw ns_ex("ns_whole_image::Cannot increase image size to a smaller size than extant image!");
		if (prop.width != this->properties().width)
			throw ns_ex("ns_whole_image::whole images can only be increased in height, not width");
		if (prop.components != this->properties().components)
			throw ns_ex("ns_whole_image::whole images can only be increased in height, not component number");

		reciever_t::_properties = sender_t::_properties = prop;
		ns_image_stream_buffer_properties bprop;
		bprop.height = prop.height;
		bprop.width = prop.width*prop.components;
		image_buffer.increase_size(bprop);

	}

	//STREAM_SENDER ABILITIES

	template<class write_buffer>
	  void send_lines(write_buffer & lines, const unsigned int count, typename sender_t::internal_state_t & lines_sent) {
		send_lines_internal(lines, count, lines_sent);
	}
	template<class write_buffer>
	  void send_lines(write_buffer & lines, const unsigned int count, typename sender_t::internal_state_t & lines_sent) const {
		send_lines_internal(lines, count, lines_sent);
	}

	template<class write_buffer>
	  void send_lines_internal(write_buffer & lines, const unsigned int count, typename sender_t::internal_state_t & lines_sent) const {
		image_buffer.set_offset(0);
		unsigned long to_send = count;
		if (count + lines_sent > reciever_t::_properties.height)
		  to_send = reciever_t::_properties.height - lines_sent;

		for (unsigned long y = 0; y < to_send; y++) {
			for (unsigned int x = 0; x < lines.properties().width; x++) {
				lines[y][x] = image_buffer[y + lines_sent][x];
			}
		}
		lines_sent += count;
	}
	//when entire image is loaded, there is nothing to do.
	void finish_recieving_image(){image_buffer.set_offset(0);}

	const inline ns_image_properties & properties() const{
	  return reciever_t::_properties;
	}
	void set_description(const std::string & dsc){
	  reciever_t::_properties.description = dsc;
	}
	const float finline sample_f(const float y, const float x) const{

		const int p0x((int)x),
					p0y((int)y);
		const int p1x(p0x+1),
					p1y(p0y+1);
		const float dx(x-(float)p0x),
					dy(y-(float)p0y);
		const float d1x(1.0-dx),
					d1y(1.0-dy);

		#ifdef NS_DEBUG_IMAGE_ACCESS
		if (p0y >= reciever_t::_properties.height || p1y >= reciever_t::_properties.height ||
		    p0x >= reciever_t::_properties.width || p1x >= reciever_t::_properties.width)
				throw ns_ex("Out of bound access!");
		#endif
		return	image_buffer[p0y][p0x]*(d1y)*(d1x) +
				image_buffer[p0y][p1x]*(d1y)*(dx) +
				image_buffer[p1y][p0x]*(dy)*(d1x) +
				image_buffer[p1y][p1x]*(dy)*(dx);
	}

	const double finline sample_d(const double y, const double x) const{

		const int p0x(xs_float::xs_FloorToInt(x)),
					p0y(xs_float::xs_FloorToInt(y));
		const int p1x(p0x+1),
					p1y(p0y+1);
		const double dx(x-(double)p0x),
					dy(y-(double)p0y);
		const double d1x(1.0-dx),
					d1y(1.0-dy);

		#ifdef NS_DEBUG_IMAGE_ACCESS
		if (p0y >= reciever_t::_properties.height || p1y >= reciever_t::_properties.height ||
		    p0x >= reciever_t::_properties.width || p1x >= reciever_t::_properties.width)
				throw ns_ex("Out of bound access!");
		#endif
		return	image_buffer[p0y][p0x]*(d1y)*(d1x) +
				image_buffer[p0y][p1x]*(d1y)*(dx) +
				image_buffer[p1y][p0x]*(dy)*(d1x) +
				image_buffer[p1y][p1x]*(dy)*(dx);
	}
	const double
		//finline
		sample_d_scaled(const double y, const double x, const float * scale_f) const {

		const int p0x(xs_float::xs_FloorToInt(x)),
			p0y(xs_float::xs_FloorToInt(y));
		const int p1x(p0x + 1),
			p1y(p0y + 1);
		const double dx(x - (double)p0x),
			dy(y - (double)p0y);
		const double d1x(1.0 - dx),
			d1y(1.0 - dy);

#ifdef NS_DEBUG_IMAGE_ACCESS
		if (p0y >= reciever_t::_properties.height || p1y >= reciever_t::_properties.height ||
			p0x >= reciever_t::_properties.width || p1x >= reciever_t::_properties.width ||
			p0x < 0 ||
			p0y < 0)
			throw ns_ex("Out of bound access!");
#endif
		return	scale_f[image_buffer[p0y][p0x]]* (d1y)*(d1x)+
			scale_f[image_buffer[p0y][p1x]] * (d1y)*(dx)+
			scale_f[image_buffer[p1y][p0x]] * (dy)*(d1x)+
			scale_f[image_buffer[p1y][p1x]] * (dy)*(dx);
	}

	const double finline weighted_sample(const double &y, const double &x,
										 const double &p00_weight, const double &p10_weight, const double &p01_weight, const double &p11_weight ) const{

		const int p0x((int)(x)),
					p0y((int)(y));
		const int p1x(p0x+1),
					p1y(p0y+1);
		const double d1x(x-(double)p0x),
					d1y(y-(double)p0y);
		const double dx(1.0-d1x),  //because you want  to weigh points greater the closer they are to the fractional point.
					dy(1.0-d1y);


		return	((p00_weight!=0)?((image_buffer[p0y][p0x]/p00_weight)*(dy)*(dx)):0) +
				((p10_weight!=0)?((image_buffer[p0y][p1x]/p10_weight)*(dy)*(d1x)):0) +
				((p01_weight!=0)?((image_buffer[p1y][p0x]/p01_weight)*(d1y)*(dx)):0) +
				((p11_weight!=0)?((image_buffer[p1y][p1x]/p11_weight)*(d1y)*(d1x)):0);
	}

	///random pixel access
	const inline ns_component * operator[](const unsigned long y)const{
		return image_buffer[y];
	}
	///random pixel access
	inline ns_component * operator[](const unsigned long y){
		#ifdef NS_DEBUG_IMAGE_ACCESS
		if (y >= image_buffer.properties().height)
			throw ns_ex("Out of Bounds!");
		#endif
		return image_buffer[y];
	}
	///Transfers an image to another image_whole object without recopying the buffer.
	///The current image is emptied.
	void transfer_contents_to_new_image(ns_image_whole<ns_component> & n){
		image_buffer.give_buffer_to_new_object(n.image_buffer);
		n.reciever_t::_properties = reciever_t::_properties;
		n.sender_t::_properties = sender_t::_properties;

		n.lines_sent = lines_sent;
		n.lines_received = lines_received;

		init(ns_image_properties(0,0,0));
	}

	///calculates the image's histogram, neglecting the value 0 which is used to represent
	///pixels that are not part of the image.
	ns_histogram<unsigned int, ns_component> histogram() const{
		ns_histogram<unsigned int,ns_component> hist;
		hist.clear();
		unsigned long num_zeros = 0;
		if (reciever_t::_properties.components == 3)
		  for (unsigned int y = 0; y < reciever_t::_properties.height; y++)
		    for (unsigned int x = 0; x < reciever_t::_properties.width*3; x+=3){
					hist[(image_buffer[y][x]+image_buffer[y][x+1]+image_buffer[y][x+2])/3]++;
				}
		else
		  for (unsigned int y = 0; y < reciever_t::_properties.height; y++)
		    for (unsigned int x = 0; x < reciever_t::_properties.width; x++){
					hist[image_buffer[y][x]]++;
				}
		return hist;
	}

	///Slow, Safe access to pixels.
	///returns 0 for out of range outputs.
	inline ns_component slow_safe_access(const int y, const int x){
	  if(x >= 0 && (static_cast<unsigned int>(x) < reciever_t::_properties.components*reciever_t::_properties.width) &&
	     y >= 0 && (static_cast<unsigned int>(y) < reciever_t::_properties.height))
			 return image_buffer[y][x];
		return 0;
	}
	///Slow, Safe access to pixels.
	///returns 0 for out of range outputs.
	inline const ns_component slow_safe_access(const int y, const int x) const{
	  if(x >= 0 && (static_cast<unsigned int>(x) < reciever_t::_properties.components*reciever_t::_properties.width) &&
	     y >= 0 && (static_cast<unsigned int>(y) < reciever_t::_properties.height))
			 return image_buffer[y][x];
		return 0;
	}

	///sets a given position in the bitmap to the given color
	///If this is called on a black and white image, a segfault will likely occur.  Careful!
	///(a check could be done for this but is omitted for efficiency reasons)
	template<class color_t>
	inline void set_color(const unsigned int & y, const unsigned int & x, const color_t & color){
		image_buffer[y][3*x    ] = color.x;
		image_buffer[y][3*x + 1] = color.y;
		image_buffer[y][3*x + 2] = color.z;
	}
	template<class color_t>
	inline void set_color(const unsigned int & y, const unsigned int & x, const color_t & color,float opacity){
		image_buffer[y][3*x    ] = (ns_8_bit)(opacity*color.x+(1-opacity)*image_buffer[y][3*x    ]);
		image_buffer[y][3*x + 1] = (ns_8_bit)(opacity*color.y+(1-opacity)*image_buffer[y][3*x + 1]);
		image_buffer[y][3*x + 2] = (ns_8_bit)(opacity*color.z+(1-opacity)*image_buffer[y][3*x + 2]);
	}

	///A bound-checked set_color.  Slow but safe.
	///Will segfault if called on B&W image
	template<class color_t>
	void inline safe_set_color(const int &y, const int &x, const color_t & color){
	  if (x < 0 || y < 0 || x >= reciever_t::_properties.width || y >= reciever_t::_properties.height)
			return;
		set_color((unsigned int)y,(unsigned int)x,color);
	}


	///simple Bresenham's algorithm,
	///as described at http://en.wikipedia.org/wiki/Bresenham's_line_algorithm
	///Will segfault if called on a grayscale image
	template<class color_t>
	void draw_line_color(const ns_vector_2i & _start, const ns_vector_2i & _stop, const color_t & color,const float opacity=1){
		if (_start.x < 0 || (unsigned int)_start.x >= properties().width || _start.y < 0 || (unsigned int)_start.y >= properties().height ||
			_stop.x < 0 || (unsigned int)_stop.x >= properties().width || _stop.y < 0 || (unsigned int)_stop.y >= properties().height)
			return;
		ns_vector_2i start = _start,
					 stop = _stop;
		ns_swap<int> swap;
		ns_swap<ns_vector_2i> swap_v;

		constrain_vector(start);
		constrain_vector(stop);

		const bool steep(abs(stop.y - start.y) > abs(stop.x - start.x));
		if (steep){swap(start.x, start.y); swap(stop.x, stop.y);}

		if (start.x > stop.x) swap_v(start,stop);


		int deltax = stop.x - start.x,
			deltay = abs(stop.y - start.y),
			error = 0,
			ystep,
			y = start.y;

		if (start.y < stop.y) ystep = 1; else ystep = -1;
		for (int x = start.x; x <= stop.x; x++){
			if (steep) set_color(x,y,color,opacity); else set_color(y,x,color,opacity);
			error += deltay;
			if (2*error >= deltax){
				y += ystep;
				error -= deltax;
			}
		}
	}
	///simple Bresenham's algorithm,
	///as described at http://en.wikipedia.org/wiki/Bresenham's_line_algorithm
	///Will segfault if called on a grayscale image
	template<class ns_component2>
	void draw_line_grayscale(const ns_vector_2i & _start, const ns_vector_2i & _stop, const ns_component2 & color,const float & opacity){
		if (_start.x < 0 || (unsigned int)_start.x >= properties().width || _start.y < 0 || (unsigned int)_start.y >= properties().height ||
			_stop.x < 0 || (unsigned int)_stop.x >= properties().width || _stop.y < 0 || (unsigned int)_stop.y >= properties().height)
			return;
		ns_vector_2i start = _start,
					 stop = _stop;
		ns_swap<int> swap;
		ns_swap<ns_vector_2i> swap_v;

		constrain_vector(start);
		constrain_vector(stop);

		const bool steep(abs(stop.y - start.y) > abs(stop.x - start.x));
		if (steep){swap(start.x, start.y); swap(stop.x, stop.y);}

		if (start.x > stop.x) swap_v(start,stop);


		int deltax = stop.x - start.x,
			deltay = abs(stop.y - start.y),
			error = 0,
			ystep,
			y = start.y;

		if (start.y < stop.y) ystep = 1; else ystep = -1;

		if (opacity == 1){
			for (int x = start.x; x <= stop.x; x++){
				if (steep) image_buffer[x][y] = (ns_component)color; else image_buffer[y][x] = (ns_component)color;
				error += deltay;
				if (2*error >= deltax){
					y += ystep;
					error -= deltax;
				}
			}
		}
		else{
			for (int x = start.x; x <= stop.x; x++){
				if (steep) image_buffer[x][y] = (ns_component)(color*opacity + (1-opacity)*image_buffer[x][y]);
					 else image_buffer[y][x] =  (ns_component)(color*opacity + (1-opacity)*image_buffer[y][x]);
				error += deltay;
				if (2*error >= deltax){
					y += ystep;
					error -= deltax;
				}
			}

		}
	}

	///A very simple thick-line drawing routine.
	template<class color_t>
	void draw_line_color_thick(const ns_vector_2i & _start, const ns_vector_2i & _stop, const color_t & color, const unsigned int thickness,const float opacity=1){
		const ns_vector_2i d(_stop - _start);
		const ns_vector_2i d_rot(int(cos(ns_pi/4)*d.x - sin(ns_pi/4)*d.y), int(cos(ns_pi/4)*d.x + sin(ns_pi/4)*d.y));
		const char half_t(thickness/2);
		const char offset(thickness%2);
		ns_vector_2i p_offset;
		if (d_rot.x > 0 && d_rot.y > 0 || d_rot.x < 0 && d_rot.y < 0)
			p_offset = ns_vector_2i(0,1);
		else p_offset = ns_vector_2i(1,0);
		for (int i = -half_t; i <= half_t+offset; i++)
			draw_line_color(_start+p_offset*i,_stop+p_offset*i,color,opacity);
	}
	template<class ns_component2>
	void draw_line_grayscale_thick(const ns_vector_2i & _start, const ns_vector_2i & _stop, const ns_component2 & color, const unsigned int thickness){
		const ns_vector_2i d(_stop - _start);
		const ns_vector_2i d_rot(int(cos(ns_pi/4)*d.x - sin(ns_pi/4)*d.y), int(cos(ns_pi/4)*d.x + sin(ns_pi/4)*d.y));
		const char half_t(thickness/2);
		const char offset(thickness%2);
		ns_vector_2i p_offset;
		if (d_rot.x > 0 && d_rot.y > 0 || d_rot.x < 0 && d_rot.y < 0)
			p_offset = ns_vector_2i(0,1);
		else p_offset = ns_vector_2i(1,0);
		for (int i = -half_t; i <= half_t+offset; i++)
			draw_line_grayscale(_start+p_offset*i,_stop+p_offset*i,color);
	}

	#ifdef _WIN32
	///Returns the image data as a Windows GDI device bitmap.
	///Only available under windows.
	HBITMAP create_GDI_bitmap(const HDC & device){
		return ns_create_GDI_bitmap(this,device);
	}
	BITMAPINFO * create_GDI_bitmapinfo(){
		return ns_create_GDI_bitmapinfo(this);
	}

	#endif

	///Returns the image as a single, long buffer (rather than the native array of buffers, one line per buffer)
	ns_component * to_raw_buf(const bool upside_down = true, const int &vertical_offset=0, const bool rotate_colors=false) const{
		ns_component * buf = new ns_component[3*properties().height*properties().width];
		to_raw_buf(upside_down,vertical_offset,buf,rotate_colors);
		return buf;
	}

	void to_raw_buf(const bool upside_down, const int &vertical_offset, ns_component * buf,const bool rotate_colors = false) const{
		const int h = properties().height;
		const int w = properties().width;
		const int c = properties().components;
		if (vertical_offset > h)
			throw ns_ex("ns_image::Vertical offset (") << vertical_offset << ") larger than image height (" << h << ")";

		int y_offset,sign;
		if (upside_down){
			y_offset = h - 1;
			sign = -1;
		}
		else{
			y_offset = 0;
			sign = 1;
		}

		//black out blank area at top of image that may be produced by vertical offset
		for (int y = 0; y < vertical_offset; y++){
			for (int x = 0; x < w; x++){
				buf[3*x*y + 3*x+0]=0;
				buf[3*x*y + 3*x+1]=0;
				buf[3*x*y + 3*x+2]=0;
			}
		}

		int start = 0,stop = h;
		if (vertical_offset >= 0) start = vertical_offset;
		else stop = vertical_offset + h;

		if (c == 3){
			if (!rotate_colors)
				for (int y = start; y < stop; y++)
					for (int x = 0; x < w; x++){
						buf[3*w*y + 3*x] = (*this)[y_offset + sign*(y - vertical_offset)][3*x];
						buf[3*w*y + 3*x+1] = (*this)[y_offset + sign*(y - vertical_offset)][3*x+1];
						buf[3*w*y + 3*x+2] = (*this)[y_offset + sign*(y - vertical_offset)][3*x+2];
					}
			else
				for (int y = start; y < stop; y++)
					for (int x = 0; x < w; x++){
						buf[3*w*y + 3*x] = (*this)[y_offset + sign*(y - vertical_offset)][3*x+2];
						buf[3*w*y + 3*x+1] = (*this)[y_offset + sign*(y - vertical_offset)][3*x+1];
						buf[3*w*y + 3*x+2] = (*this)[y_offset + sign*(y - vertical_offset)][3*x];
					}
		}
		else{
			for (int y = start; y < stop; y++)
				for (int x = 0; x < w; x++){
					buf[3*w*y + 3*x] =
					buf[3*w*y + 3*x+1] =
					buf[3*w*y + 3*x+2] = (*this)[y_offset + sign*(y - vertical_offset)][x];
				}
		}
		//black out blank area at bottom of image
		for (int y = h+vertical_offset; y < h; y++){
			for (int x = 0; x < w; x++){
				buf[3*w*y + 3*x+0]=0;
				buf[3*w*y + 3*x+1]=0;
				buf[3*w*y + 3*x+2]=0;
			}
		}
	}

	///Resize the image using bilinear interpolation
	template<class ns_component2>
	void resample(const ns_image_properties & new_dimentions, ns_image_whole<ns_component2> & out) const{

		if (new_dimentions == properties()){
			this->pump(out,512);
			return;
		}
		ns_image_properties d = new_dimentions;
		d.components = properties().components;
		out.init(d);

		float sx = ((float)d.width)/((float)properties().width),
				sy = ((float)d.height)/((float)properties().height);

		 if (sx >= 1 && sy >= 1 ){
			 expand(sx,sy,out);
			 return;
		}
		if (sx <= 1 && sy <= 1){
			 shrink(sx,sy,out);
			 return;
		}
		//if one dimention is expanded while the other shrunk,
		//first expand in the larger direction, then shrink in the smaller.
		if (sx < 1 && sy > 1){
			ns_image_whole<ns_component2> temp;
			ns_image_properties t = d;
			t.width = properties().width;
			temp.init(t);
			resample(t,temp);
			temp.resample(d,out);
		}
		if (sx > 1 && sy < 1){
			ns_image_whole<ns_component2> temp;
			ns_image_properties t = d;
			t.height = properties().height;
			temp.init(t);
			resample(t,temp);
			temp.resample(d,out);
		}

	}

	void clear(){
		resize(ns_image_properties(0,0,1,0));
	}
	void use_more_memory_to_avoid_reallocations(const bool r=true){
		avoid_memory_reallocations = r;
	}

	//mimic interface of ns_buffered_random_access_image.h
	inline typename sender_t::internal_state_t seek_to_beginning() { return 0; }
	inline void make_line_available(unsigned long i){}


private:
	bool avoid_memory_reallocations;
	ns_image_stream_static_offset_buffer<ns_component> image_buffer;
	unsigned long lines_received;
	unsigned long lines_sent;

	///if the specified std::vector lies outside the image,
	///it is moved to the closest edge of the image.
	void inline constrain_vector(ns_vector_2i & a){
		if (a.x < 0)
			a.x = 0;
		if ((unsigned int)a.x >= reciever_t::_properties.width)
		  a.x = reciever_t::_properties.width-1;
		if (a.y < 0)
			a.y = 0;
		if ((unsigned int)a.y >= reciever_t::_properties.height)
		  a.y = (int)reciever_t::_properties.height-1;
	}

		static inline ns_component lerp(const ns_component & c, const ns_component & d, float t){
		return (ns_component)floor((1.0 - t) * c + t * d + 0.5);
	}

	///sx and sy are both less than or equal to one, representing the percentage of the original dimentions
	///the resampled image should be.
	template<class ns_component2>
	void shrink(const float sx, const float sy, ns_image_whole<ns_component2> & out) const{

		const unsigned int h(properties().height),
							 w(properties().width);
		const unsigned int c(properties().components);

		for (unsigned int y = 0; y < out.properties().height; y++)
			for (unsigned int x = 0; x < out.properties().width; x++){
				int u0 = (int)(((float)x) / sx),
					v0 = (int)(((float)y) / sy),
					u1 = (int)((x+1.0) / sx),
					v1 = (int)((y+1.0) / sy),
					sum;
			//	if (u0 < 0) u0 = 0;
				if ((unsigned int)u1 >= w) u1 = w-1;
			//	if (v0 < 0) v0 = 0;
				if ((unsigned int)v1 >= h) v1 = h-1;
				int area = (v1 - v0+1)*(u1-u0+1);
				if (area == 0){
					for (unsigned int _c = 0; _c < c; _c++)
						out[y][c*x+_c] = 0;
					continue;
				}

				for (unsigned int _c = 0; _c < c; _c++){
					sum = 0;
					for (int v = v0; v <= v1; v++)
						for (int u = u0; u <= u1; u++)
							sum+=image_buffer[v][c*u+_c];
					out[y][c*x+_c] = sum/area;
				}
			}
	}


	inline ns_component sample_bl(const float & u, const float & v, const int _c) const{
		int uf = static_cast<int>(floor(u));  //implicit floor
		int vf = static_cast<int>(floor(v));
		float vf_offset = v-vf;
		float uf_offset = u-uf;

		if ((unsigned int)uf >= properties().width-1)
			uf = properties().width-2;
		if ((unsigned int)vf >= properties().height-1)
			vf = properties().height-2;

		const unsigned int c(properties().components);

		ns_component a = lerp(image_buffer[vf+1][c*uf+_c],image_buffer[vf+1][c*(uf+1)+_c], uf_offset),
					 b = lerp(image_buffer[vf  ][c*uf+_c],image_buffer[vf  ][c*(uf+1)+_c], uf_offset);

		return lerp(a,b,1-vf_offset);
	}

	///sx and sy are both greater or equal to 1, the image is expanded by the specified dimentions
	template<class ns_component2>
	void expand(const float sx, const float sy, ns_image_whole<ns_component2> & out) const{

		const unsigned int h(properties().height),
							 w(properties().width);

		const unsigned int c(properties().components);
		if (h == 1 || w == 1){
			if (w != 1)
				for (unsigned int y = 0; y < out.properties().height; y++)
					for (unsigned int x = 0; x < c*out.properties().width; x++)
							out[y][x] = (*this)[1][(unsigned int)(x/sx)];
			else if (h != 1)
				for (unsigned int y = 0; y < out.properties().height; y++)
					for (unsigned int x = 0; x < c*out.properties().width; x++)
							out[y][x] = (*this)[(unsigned int)(y/sy)][1];
			else
				for (unsigned int y = 0; y < out.properties().height; y++)
					for (unsigned int x = 0; x < c*out.properties().width; x++)
							out[y][x] = (*this)[0][0];
			return;
		}

		for (unsigned int y = 0; y < out.properties().height; y++){
			for (unsigned int x = 0; x < out.properties().width; x++)
				for (unsigned int _c = 0; _c < c; _c++)
					out[y][c*x+_c] = sample_bl(((float)x)/sx,((float)y)/sy,_c);
		}
	}

};


///filters that change an ns_whole_image dimentions cannot be pumped directly back into that image.
///Instead, they can be pumped via a ns_image_whole_indirect object bound to the original image.
///This class does the necissary buffering to make this happen.
template<class ns_component>
class ns_image_whole_indirect: public ns_image_stream_reciever<ns_image_stream_static_offset_buffer<ns_component> >{
public:
//initialize as an empty image
	ns_image_whole_indirect(ns_image_whole<ns_component> & output):ns_image_stream_reciever<ns_image_stream_static_offset_buffer<ns_component> >(0,this),destination(&output){}

	//STREAM_RECIEVER ABILITIES
	//allocate buffer for entire image and prepare to recieve it
	bool init(const ns_image_properties & properties){
		return temp_image.init(properties);
	}
	ns_image_stream_static_offset_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		return temp_image.provide_buffer(buffer_properties);
	}


	void recieve_lines(const ns_image_stream_static_offset_buffer<ns_component> & lines, const unsigned long height){
		temp_image.recieve_lines(lines,height);
		/*for (unsigned int y = 0; y < height; y++)
			for (unsigned int x = 0; x < lines.properties().width; x++)
				if (lines[y][x] != 0)
					std::cerr << "Value = " << lines[y][x] << "\n";*/

	}

	void finish_recieving_image(){
		temp_image.finish_recieving_image();
		temp_image.transfer_contents_to_new_image(*destination);
	}
	ns_image_whole<ns_component> temp_image;
	ns_image_whole<ns_component> * destination;
};

typedef ns_image_whole<ns_8_bit>  ns_image_standard;
typedef ns_image_whole_indirect<ns_8_bit> ns_image_standard_indirect;

typedef ns_image_whole<ns_bit> ns_image_bitmap;


typedef ns_image_whole<ns_16_bit> ns_image_standard_16_bit;
typedef ns_image_whole_indirect<ns_16_bit> ns_image_standard_indirect_16_bit;

typedef ns_image_whole<ns_32_bit> ns_image_standard_32_bit;
typedef ns_image_whole_indirect<ns_32_bit> ns_image_standard_indirect_32_bit;



class ns_image_standard_signed : public ns_image_standard_16_bit{
public:

	const inline short * operator[](const unsigned long y)const{
		return reinterpret_cast<const short *> (ns_image_standard_16_bit::operator[](y));
	}
	///random pixel access
	inline short * operator[](const unsigned long y){
		return reinterpret_cast<short *> (ns_image_standard_16_bit::operator[](y));
	}
};

#endif
