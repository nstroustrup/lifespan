#ifndef NS_IMAGE_STREAM_BUFFERS
#define NS_IMAGE_STREAM_BUFFERS

#include <vector>

//#define NS_TRACK_PERFORMANCE_STATISTICS
#ifdef NS_TRACK_PERFORMANCE_STATISTICS
#include "ns_performance_statistics.h"
extern ns_performance_statistics_analyzer ns_image_allocation_performance_stats;
#endif
//#define NS_DEBUG_IMAGE_ACCESS

///Stores the length and height of a 2D pixel buffer
class ns_image_stream_buffer_properties{
public:
	unsigned long width,
				  height;
	bool operator==(const ns_image_stream_buffer_properties & l) const{
		return (width == l.width) && (height == l.height);
	}
	ns_image_stream_buffer_properties(){}
	ns_image_stream_buffer_properties(const unsigned long w, const unsigned long h):width(w),height(h){}
};

///A simple buffer interface for storing image information
template <class ns_component>
class ns_image_stream_buffer{
public:
	virtual void resize(const ns_image_stream_buffer_properties & p)=0;
	virtual inline ns_component *  operator[](const unsigned long i)=0;
	virtual const inline ns_component *  operator[](const unsigned long i) const =0;

	inline const ns_image_stream_buffer_properties & properties() const {return _properties;}
protected:
	ns_image_stream_buffer_properties _properties;
};

///a buffer that stores pixel info on the heap
template <class ns_component>
class ns_image_stream_static_buffer {//: public ns_image_stream_buffer<ns_component>{
public:
	ns_image_stream_static_buffer(const ns_image_stream_buffer_properties & p):_properties(p),buffer(0){create_buffer(p);}
	ns_image_stream_static_buffer():buffer(0),_properties(0,0){}

	ns_image_stream_static_buffer(const ns_image_stream_static_buffer & buf){
	//	cerr << "Calling static buffer copy constructor!\n";
		buffer = 0;
		create_buffer(buf._properties);
		for (unsigned int y = 0; y < buf._properties.height; y++)
			for (unsigned int x = 0; x < buf._properties.width; x++)
				buffer[y][x] = buf[y][x];
	}
	///delete image contents and reallocate a larger buffer
	bool resize(const ns_image_stream_buffer_properties & p){
		//do nothing if no change is needed.
		if (buffer != 0 && p.height == _properties.height && p.width == _properties.width)
			return false;

		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
		if (buffer != 0){
			for (unsigned int i = 0; i < _properties.height; i++)
				delete[] buffer[i];
			delete[] buffer;
		}
		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_image_allocation_performance_stats.register_job_duration(ns_performance_statistics_analyzer::ns_image_deallocation,tp.stop());
		#endif
		create_buffer(p);
		return true;
	}
	inline ns_component *  operator[](const unsigned long i){
		return buffer[i];
	}
	const inline ns_component *  operator[](const unsigned long i) const{
		return buffer[i];
	}
	inline const ns_image_stream_buffer_properties & properties() const {return _properties;}
	~ns_image_stream_static_buffer(){
		resize(ns_image_stream_buffer_properties(0,0));
	}
private:
	ns_component ** buffer;
protected:
	void create_buffer(const ns_image_stream_buffer_properties & p){

		_properties = p;
		if (p.height == 0 && p.width == 0){
			buffer = 0;
			return;
		}

		if (p.height == 0 || p.width == 0)
			throw ns_ex("ns_image_stream_static_buffer::Cannot allocate a buffer with dimentions ") << p.width << "x" << p.height;
		if (p.height > 200000 || p.width > 200000)
			throw ns_ex("ns_image_stream_static_buffer::Cannot allocate a buffer with dimentions ") << p.width << "x" << p.height;
		
		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
		buffer = new ns_component *[p.height];
		//clear memory so we can clean up if an error occurs during allocation.
		for (unsigned int i = 0; i < p.height; i++)
			buffer[i] = 0;

		try{
			for (unsigned int i = 0; i < p.height; i++)
				buffer[i] = new ns_component[p.width];
			#ifdef NS_TRACK_PERFORMANCE_STATISTICS
			ns_image_allocation_performance_stats.register_job_duration(ns_performance_statistics_analyzer::ns_image_allocation,tp.stop());
			#endif
		}
		catch(...){
			for (unsigned int i = 0; i < p.height && buffer[i] != 0; i++)
					delete buffer[i];
			delete buffer;
			buffer = 0;
			throw;
		}
	}
	ns_image_stream_buffer_properties _properties;
};

///a buffer that stores pixel info on the heap.  To facilitate image streaming,
///an "offset" can be declared such that pixels written to image[y][x] are actually
///written to image[y+offet][x].
template <class ns_component>
class ns_image_stream_static_offset_buffer {//: public ns_image_stream_buffer<ns_component>{
public:
	ns_image_stream_static_offset_buffer(const ns_image_stream_buffer_properties & p):content_properties(p),memory_properties(p),buffer(0),offset(0){create_buffer(p);}
	ns_image_stream_static_offset_buffer():buffer(0),offset(0),content_properties(ns_image_stream_buffer_properties(0,0)),memory_properties(ns_image_stream_buffer_properties(0,0)){}

	void wasteful_resize(const ns_image_stream_buffer_properties & p){
		//reallocate everything if we need a bigger image.
		//we could do a partial resize but this might put individual lines of images on different memory pages.
		if (p.width > memory_properties.width || p.height > memory_properties.height){
			resize(p);
			return;
		}
		//if we have enough space to store a smaller image
		else content_properties = p;
	}
	void resize(const ns_image_stream_buffer_properties & p){
		//do nothing if no change is needed.
		if (buffer != 0 && p.height == content_properties.height && p.width == content_properties.width)
			return;

		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
		if (buffer != 0){
		//	cerr << "Deleting old buffer...\n";
			for (unsigned int i = 0; i < memory_properties.height; i++)
				delete[] buffer[i];
			delete[] buffer;
		}
		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_image_allocation_performance_stats.register_job_duration(ns_performance_statistics_analyzer::ns_image_deallocation,tp.stop());
		#endif
		create_buffer(p);
	}
	void increase_size(const ns_image_stream_buffer_properties & p){
		if (buffer == 0)
			throw ns_ex("ns_image_stream_static_offset_buffer::Cannot increase the size of an unallocated buffer!");
		if (memory_properties.width < p.width)
			throw ns_ex("ns_image_stream_static_offset_buffer:: Cannot in situ increase the width of a buffer!");
		ns_component ** temp = new ns_component *[p.height];
		unsigned int i;
		for (i = 0; i < memory_properties.height; i++)
			 temp[i] = buffer[i];
		for (;i < p.height; i++)
			temp[i] = new ns_component[memory_properties.width];
		delete[] buffer;
		buffer = temp;
		content_properties.height = memory_properties.height = p.height;
	}

	inline ns_component *  operator[](const unsigned long i){
		#ifdef NS_DEBUG_IMAGE_ACCESS
			if (i + offset >= this->_properties.height)
				throw ns_ex("Invalid line height: ") << i;
		#endif
		return buffer[(unsigned long)((long)i+offset)];
	}
	const inline ns_component *  operator[](const unsigned long i) const{
		return buffer[(unsigned long)((long)i+offset)];
	}

	void set_offset(const long i) const {offset = i;}
	inline const ns_image_stream_buffer_properties & properties() const {return content_properties;}

	//transfers the contents of one buffer to another, emptying the first.
	void give_buffer_to_new_object(ns_image_stream_static_offset_buffer<ns_component> & n){

		//delete the old buffer
		n.resize(ns_image_stream_buffer_properties(0,0));

		//transfer the new buffer
		n.buffer = buffer;
		n.memory_properties = memory_properties;
		n.content_properties = content_properties;

		//remove previous reference to new buffer
		buffer = 0;
		memory_properties = content_properties = ns_image_stream_buffer_properties(0,0);
	}
	~ns_image_stream_static_offset_buffer(){
		if (buffer != 0){
			#ifdef NS_TRACK_PERFORMANCE_STATISTICS
			ns_high_precision_timer tp;
			tp.start();
			#endif
			for (unsigned int i = 0; i < memory_properties.height; i++)
				delete[] buffer[i];
			delete[] buffer;
			#ifdef NS_TRACK_PERFORMANCE_STATISTICS
			ns_image_allocation_performance_stats.register_job_duration(ns_performance_statistics_analyzer::ns_image_deallocation,tp.stop());
			#endif
		}
	}

	ns_image_stream_static_offset_buffer(const ns_image_stream_static_offset_buffer & buf){
		buffer = 0;
		create_buffer(buf.content_properties);
		for (unsigned int y = 0; y < buf.content_properties.height; y++)
			for (unsigned int x = 0; x < buf.content_properties.width; x++)
				buffer[y][x] = buf[y][x];
	}


private:
	ns_component ** buffer;
	mutable long offset;
protected:
	ns_image_stream_buffer_properties content_properties,memory_properties;
	void create_buffer(const ns_image_stream_buffer_properties & p){
		if ((p.height == 0) != (p.width == 0))
			throw ns_ex("ns_image_stream_static_offset_buffer::Cannot allocate a buffer with dimentions ") << p.width << "x" << p.height;
		memory_properties =  content_properties = p;
	//	cerr << "Creating buffer...";
		if (p.height == 0 || p.width == 0){
			buffer = 0;
			return;
		}
		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
		buffer = new ns_component *[p.height];
		//clear buffer so we can clean up if an error occurs during memory allocation
		for (unsigned int i = 0; i < p.height; i++)
			buffer[i] = 0;
		try{
			for (unsigned int i = 0; i < p.height; i++){
			//	cerr << i << ".";
				buffer[i] = new ns_component[p.width];
			}
			#ifdef NS_TRACK_PERFORMANCE_STATISTICS
			ns_image_allocation_performance_stats.register_job_duration(ns_performance_statistics_analyzer::ns_image_allocation,tp.stop());
			#endif
		}
		catch(...){
			for (unsigned int i = 0; i < p.height && buffer[i] != 0; i++)
					delete buffer[i];
			delete buffer;
			buffer = 0;
			throw;
		}
	}
};


///A simple FIFO queue
///step() pops the queue.
template <class T>
class ns_sliding_buffer{
	public:
	ns_sliding_buffer(const unsigned long height):lines(height),begin(0){}

	inline T & step(){
		int new_last = begin;

		begin++;
		if (begin == lines.size())
			begin = 0;
		return lines[new_last];
	}
	inline void step(const unsigned int dist){
		begin = (begin+dist)%lines.size();
	}

	void reset(){
		begin = 0;
	}

	inline T & operator[](const unsigned int i){
		return lines[(begin+i)%lines.size()];
	}

	inline const T & operator[](const unsigned int i) const{
		return lines[(begin+i)%lines.size()];
	}

	inline void resize(const unsigned long l){
		//cerr << (unsigned int)lines.size();
		lines.resize(l);
	}
	inline void resize(const unsigned long l,const T &val){
		//cerr << (unsigned int)lines.size();
		lines.resize(l,val);
	}
	inline unsigned long size(){ return static_cast<unsigned long>(lines.size());}

	//inline const ns_image_stream_buffer_properties & properties() const {return _properties;}


protected:
	//ns_image_stream_buffer_properties _properties;
	unsigned long begin;
	std::vector<T> lines;

};

///a FIFO queue that conforms to the ns_image_stream_buffer interface.
///lines can be efficiently popped off the buffer using step()
template<class ns_component>
class ns_image_stream_sliding_buffer {

public:
	ns_image_stream_sliding_buffer(const ns_image_stream_buffer_properties & p):buffer(p.height),_properties(p){resize(p);}
	ns_image_stream_sliding_buffer():buffer(0),_properties(ns_image_stream_buffer_properties(0,0)){}

	void free_buffer(){
		
		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
		buffer.reset();
		for (unsigned long i = 0; i < buffer.size(); i++)
			delete[] buffer[i];
		buffer.resize(0);
		
		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_image_allocation_performance_stats.register_job_duration(ns_performance_statistics_analyzer::ns_image_deallocation,tp.stop());
		#endif
	}

	void resize(const ns_image_stream_buffer_properties & p){
		free_buffer();
		buffer.reset();
		if ((p.height == 0) != (p.width == 0))
			throw ns_ex("ns_image_stream_static_buffer::Cannot allocate a buffer with dimentions ") << p.width << "x" << p.height;
		
		#ifdef NS_TRACK_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif

		//initialize buffer as zero so we can clean up if error occurs during memory allocation
		buffer.resize(p.height,0);
		_properties = p;
		try{
			for (unsigned long i = 0; i < p.height; i++)
				buffer[i] = new ns_component[p.width];
			#ifdef NS_TRACK_PERFORMANCE_STATISTICS
			ns_image_allocation_performance_stats.register_job_duration(ns_performance_statistics_analyzer::ns_image_allocation,tp.stop());
			#endif
		}
		catch(...){
			for (unsigned int i = 0; i < p.height && buffer[i] != 0; i++)
					delete buffer[i];
			buffer.resize(0);
			throw;
		}
	}
	inline ns_component * operator[](const unsigned long i){
		return buffer[i];
	}

	inline const ns_component * operator[](const unsigned long i) const{
		return buffer[i];
	}

	inline ns_component *  step(){
		return buffer.step();
	}
	inline void step(const unsigned int dist){
		buffer.step(dist);
	}
	~ns_image_stream_sliding_buffer(){
		free_buffer();
	}
	inline const ns_image_stream_buffer_properties & properties() const {return _properties;}
protected:
	ns_image_stream_buffer_properties _properties;
	ns_sliding_buffer<ns_component *> buffer;
};
///a buffer that stores pixel info on the heap.  To facilitate image streaming,
///an "offset" can be declared such that pixels written to image[y][x] are actually
///written to image[y+offet][x].
///Lines cann be efficiently popped off the top of the buffer using step()
template<class ns_component>
class ns_image_stream_sliding_offset_buffer: public ns_image_stream_sliding_buffer<ns_component>{
public:
	ns_image_stream_sliding_offset_buffer(const ns_image_stream_buffer_properties & p):ns_image_stream_sliding_buffer<ns_component>(p),offset(0){}
	ns_image_stream_sliding_offset_buffer():offset(0){}

	inline void set_offset(const long i){offset = i;}

	inline ns_component * operator[](const unsigned long i){
		return ns_image_stream_sliding_buffer<ns_component>::operator []((long)i+offset);
	}

	inline const ns_component * operator[](const unsigned long i) const{
		return ns_image_stream_sliding_buffer<ns_component>::operator []((long)i+offset);
	}

protected:
	long offset;
};

template<class ns_component>
class ns_image_stream_safe_sliding_offset_buffer{
public:
	ns_image_stream_safe_sliding_offset_buffer(const ns_image_stream_buffer_properties & p,const unsigned long edge_buffer_size_, const ns_component edge_buffer_value_):ns_image_stream_sliding_offset_buffer<ns_component>(ns_image_stream_buffer_properties(p.width+2*edge_buffer_size_,p.height+2*edge_buffer_size_)),edge_buffer_size(edge_buffer_size_),edge_buffer_value(edge_buffer_value_),external_properties(p){fill_in_border();}
	ns_image_stream_safe_sliding_offset_buffer():edge_buffer_size(0){}
	ns_image_stream_safe_sliding_offset_buffer(const unsigned long edge_buffer_size_):edge_buffer_size(edge_buffer_size_){}

	void fill_in_border(){
		for (unsigned int y = 0; y < edge_buffer_size; y++)
			for (unsigned int x = 0; x < buffer.properties().width; x++)
				buffer[y][x] = edge_buffer_value;
		for (unsigned int y = edge_buffer_size; y < buffer.properties().height - edge_buffer_size; y++){
			for (unsigned int x = 0; x < edge_buffer_size; x++)
				buffer[y][x] = edge_buffer_value;
			for (unsigned int x = 0; x < edge_buffer_size; x++)
				buffer[y][buffer.properties().width-edge_buffer_size-1+x] = edge_buffer_value;
		}
		for (unsigned int y = 0; y < edge_buffer_size; y++)
			for (unsigned int x = 0; x < buffer.properties().width; x++)
				buffer[buffer.properties().height-edge_buffer_size - 1 + y][x] = edge_buffer_value;
	}
	void resize(const ns_image_stream_buffer_properties & p, const unsigned long edge_buffer_size_, ns_component edge_buffer_value_){
		edge_buffer_size = edge_buffer_size_;
		edge_buffer_value = edge_buffer_value_;
		resize(p);
	}
	void resize(const ns_image_stream_buffer_properties & p){
		buffer.resize(ns_image_stream_buffer_properties(p.width+2*edge_buffer_size,p.height+2*edge_buffer_size));
		external_properties = p;
		fill_in_border();
	}
	inline ns_component * operator[](const unsigned long i){
		return &(buffer[i+edge_buffer_size][edge_buffer_size]);
	}

	inline const ns_component * operator[](const unsigned long i) const{
		return &(buffer[i+edge_buffer_size][edge_buffer_size]);
	}

	inline void set_offset(const unsigned long i){
		buffer.set_offset(i);
	}

	inline ns_component *  step(){
		return buffer.step();
	}
	inline void step(const unsigned int dist){
		buffer.step(dist);
	}

	//the sliding buffer is allocated larger to include the margins.
	//we want to report the without-margins size.
	inline const ns_image_stream_buffer_properties & properties() const {return external_properties;}

private:
	unsigned long edge_buffer_size;
	ns_component edge_buffer_value;
	ns_image_stream_buffer_properties external_properties;
	ns_image_stream_sliding_offset_buffer<ns_component> buffer;
};
/*
//A simple FIFO queue with all of its elements accessible.
//step() pops the queue.
template <class T>
class sliding_buffer_raw{
	public:
		sliding_buffer_raw(const long _buffer_height):
		  buffer_height(_buffer_height),begin(0)
		{
			lines = new T[buffer_height];
		}
		~sliding_buffer_raw(){
			delete[] lines;
		}

	inline T & step(){
		int new_last = begin;

		begin++;
		if (begin == buffer_height)
			begin = 0;
		return lines[new_last];
	}

	inline T & operator[](const unsigned int i){
		return lines[(begin+i)%buffer_height];
	}

	inline const T & operator[](const unsigned int i) const{
		return lines[(begin+i)%buffer_height];
	}

	inline T * & block(){
		return lines;
	}

	inline T & raw_access(const unsigned long i){
		return lines[i];
	}

	inline void resize(const unsigned long l){
		delete[] lines;
		buffer_height = l;
		lines = new T[buffer_height];


	}
	inline unsigned long size(){ return buffer_height;}

	private:
	unsigned int begin;
	unsigned long buffer_height;
	T * lines;

};
*/
#endif
