#ifndef NS_IMAGE_STORAGE_H
#define NS_IMAGE_STORAGE_H

#include "ns_image.h"
#include "ns_image_stream_buffers.h"
#include "ns_image_socket.h"
#include "ns_jpeg.h"
#include "ns_tiff.h"
#include "ns_ojp2k.h"
#include "ns_dir.h"
#include "ns_image_server_images.h"
#include "ns_thread.h"
#include "ns_image_server_message.h"
#include "ns_performance_statistics.h"
#include "ns_managed_pointer.h"


std::string ns_shorten_filename(std::string name, const unsigned long limit=40);

#pragma warning(disable: 4355) //our use of this in constructor is valid, so we suppress the error message
//Generalized Image Recieving
template<class ns_component>
class ns_image_storage_reciever : public ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >{
public:
	typedef ns_image_stream_static_buffer<ns_component> storage_type;
	ns_image_stream_static_buffer<ns_component> * output_buffer;
	typedef ns_component component_type;
	ns_image_storage_reciever(const unsigned long max_block_height):
		ns_image_stream_reciever<ns_image_stream_static_buffer<ns_component> >(max_block_height,this){}

	virtual ns_image_stream_static_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties)=0;

	virtual void finish_recieving_image()=0;
	virtual void recieve_lines(const ns_image_stream_static_buffer<ns_component> & lines, const unsigned long height)=0;

	virtual ~ns_image_storage_reciever(){}

//protected:
	virtual bool init(const ns_image_properties & properties)=0;
};
#pragma warning(default: 4355)


ns_image_type ns_get_image_type(const std::string & filename);

template<class image_t>
image_t & ns_choose_image_source(const ns_image_type & type, image_t & jpeg, image_t & tif, image_t & jp2k){
	switch(type){
		case ns_jpeg: return jpeg;
		case ns_tiff:
		case ns_tiff_lzw:
		case ns_tiff_zip:
			return tif;
		case ns_jp2k:
			return jp2k;
		default: throw ns_ex("No image type specified!");
	}
}

template<class ns_component>
class ns_image_storage_reciever_to_disk: public ns_image_storage_reciever<ns_component>{
public:
	ns_image_storage_reciever_to_disk(const unsigned long max_block_height, const std::string & filename,const ns_image_type & type,ns_performance_statistics_analyzer * performance_analyzer_,const bool volatile_file_=false):
	  ns_image_storage_reciever<ns_component>(max_block_height),pa(performance_analyzer_),volatile_file(volatile_file_),total(0),
		  file_sink(filename,ns_choose_image_source<ns_image_output_file<ns_component> >(type,jpeg_out,tiff_out,jp2k_out),max_block_height),tiff_out(ns_get_tiff_compression_type(type))
	  {}

    ns_image_stream_static_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties)
	{return file_sink.provide_buffer(buffer_properties);}

	void recieve_lines(const ns_image_stream_static_buffer<ns_component> & lines, const unsigned long height)
	{	mark_pa_started();
		file_sink.recieve_lines(lines,height);
		mark_pa_finished();
	}

	void finish_recieving_image(){
	mark_pa_started();
	file_sink.finish_recieving_image();
	mark_pa_finished();
	if (pa==0)return;
	pa->register_job_duration(volatile_file?
					ns_performance_statistics_analyzer::ns_volatile_io_write:
					ns_performance_statistics_analyzer::ns_long_term_io_write,total);}
	//deconstruction handled by output file destructors.

	~ns_image_storage_reciever_to_disk(){
		
	}
protected:
	bool init(const ns_image_properties & properties){return file_sink.init(properties);}
private:
	ns_jpeg_image_output_file<ns_component> jpeg_out;
	ns_tiff_image_output_file<ns_component> tiff_out;
	ns_ojp2k_image_output_file<ns_component> jp2k_out;

	ns_image_stream_file_sink<ns_component> file_sink;

	inline void mark_pa_started(){ 
		if (pa==0) return; 
		tp.start();
	}	
	inline void mark_pa_finished(){ 
		if (pa==0) return; 
		total+=tp.stop();
	}
	ns_performance_statistics_analyzer * pa;
	ns_high_precision_timer tp;
	unsigned long long total;
	bool volatile_file;
};

template<class ns_component>
class ns_image_storage_reciever_to_net: public ns_image_storage_reciever<ns_component>{
public:
	ns_image_storage_reciever_to_net(const unsigned long max_block_height, ns_socket_connection & socket_connection, ns_lock & network_lock):
		ns_image_storage_reciever<ns_component>(max_block_height),
		_connection(socket_connection), image_socket(max_block_height), release_when_finished(network_lock){}

    ns_image_stream_static_buffer<ns_component> * provide_buffer(const ns_image_stream_buffer_properties & buffer_properties){
		return image_socket.provide_buffer(buffer_properties);
    }

	void recieve_lines(const ns_image_stream_static_buffer<ns_component> & lines, const unsigned long height){
		image_socket.recieve_lines(lines,height);
	}
	void finish_recieving_image(){
		image_socket.finish_recieving_image();
		_connection.close();
		release_when_finished.release();
	}
	~ns_image_storage_reciever_to_net(){
		//close();
	}
	void close(){
		_connection.close(); //closing can occur multiple times as duplicats are ignored
		release_when_finished.release(); //release can occur multiple times as duplicats are ignored
	}

protected:
	void init(const ns_image_properties & properties){
		image_socket.bind_socket(_connection);
		image_socket.init(properties);
	}
private:
	ns_image_socket_sender<ns_component> image_socket;
	ns_socket_connection _connection;
	ns_lock & release_when_finished;
};

#pragma warning(disable: 4355) //our use of this in constructor is valid, so we suppress the error message
//generalized Image Sources
template<class ns_component>
class ns_image_storage_source : public ns_image_stream_sender<ns_component,ns_image_storage_source<ns_component> >{
public:
	ns_image_storage_source(const ns_image_properties & properties):
		ns_image_stream_sender<ns_component,ns_image_storage_source<ns_component> >(properties,this){}
	virtual void send_lines(ns_image_stream_static_buffer<ns_component> & lines, unsigned int count)=0;
	virtual void send_lines(ns_image_stream_static_offset_buffer<ns_component> & lines, unsigned int count)=0;
	virtual void send_lines(ns_image_stream_sliding_offset_buffer<ns_component> & lines, unsigned int count)=0;
	virtual void send_lines(ns_image_stream_safe_sliding_offset_buffer<ns_component> & lines, unsigned int count)=0;
	virtual void seek_to_beginning()=0;
	virtual ~ns_image_storage_source(){}

private:
	virtual void init_send(){}
	virtual void finish_send(){}
};
#pragma warning(default: 4355)

template<class ns_component>
class ns_image_storage_source_from_disk : public ns_image_storage_source<ns_component>{
public:
	ns_image_storage_source_from_disk(const std::string & filename,ns_performance_statistics_analyzer * performance_analyzer_,const bool volatile_file_=false):
	  pa(performance_analyzer_),volatile_file(volatile_file_),total(0),
		_source( ns_choose_image_source<ns_image_input_file<ns_component> >(ns_get_image_type(filename),jpeg_in,tiff_in,jp2k_in) ),
		ns_image_storage_source<ns_component>(ns_image_properties(0,0,0)){

		target = &(ns_choose_image_source<ns_image_input_file<ns_component> >(ns_get_image_type(filename),jpeg_in,tiff_in,jp2k_in));
		target->open_file(filename);
		ns_image_storage_source<ns_component>::_properties = target->properties();
	}

	//closing of files handled by their destructors
	~ns_image_storage_source_from_disk(){}

	void send_lines(ns_image_stream_static_buffer<ns_component> & lines, unsigned int count){mark_pa_started();_source.send_lines(lines,count);mark_pa_finished();	}
	void send_lines(ns_image_stream_static_offset_buffer<ns_component> & lines, unsigned int count){mark_pa_started();_source.send_lines(lines,count);mark_pa_finished();	}
	void send_lines(ns_image_stream_sliding_offset_buffer<ns_component> & lines, unsigned int count){mark_pa_started();_source.send_lines(lines,count);mark_pa_finished();	}
	void send_lines(ns_image_stream_safe_sliding_offset_buffer<ns_component> & lines, unsigned int count){mark_pa_started();_source.send_lines(lines,count);mark_pa_finished();	}
	void seek_to_beginning(){target->seek_to_beginning();}
protected:
	void finish_send(){
		_source.finish_send();
		if (pa==0)return;
		pa->register_job_duration(volatile_file?
			ns_performance_statistics_analyzer::ns_volatile_io_read:
			ns_performance_statistics_analyzer::ns_long_term_io_read,total);
	}
	void init_send(){_source.init_send();}
private:
	ns_jpeg_image_input_file<ns_component> jpeg_in;
	ns_tiff_image_input_file<ns_component> tiff_in;
	ns_ojp2k_image_input_file<ns_component> jp2k_in;
	ns_image_stream_file_source<ns_component> _source;
	ns_image_input_file<ns_component> * target;
	inline void mark_pa_started(){ 
		if (pa==0) return; 
		tp.start();
	}	
	inline void mark_pa_finished(){ 
		if (pa==0) return; 
		total+=tp.stop();
	}
	ns_performance_statistics_analyzer * pa;
	ns_high_precision_timer tp;
	bool volatile_file;
	unsigned long long total;
};

template<class ns_component>
class ns_image_storage_source_from_net :  public ns_image_storage_source<ns_component>{
public:
	ns_image_storage_source_from_net(ns_socket_connection & connection):_connection(connection),ns_image_storage_source<ns_component>(ns_image_properties(0,0,0)){
		reciever.bind_socket(_connection);
	}

	void send_lines(ns_image_stream_static_buffer<ns_component> & lines, unsigned int count){reciever.send_lines(lines,count);}
	void send_lines(ns_image_stream_static_offset_buffer<ns_component> & lines, unsigned int count){reciever.send_lines(lines,count);	};
	void send_lines(ns_image_stream_sliding_offset_buffer<ns_component> & lines, unsigned int count){reciever.send_lines(lines,count);	}
	void send_lines(ns_image_stream_safe_sliding_offset_buffer<ns_component> & lines, unsigned int count){reciever.send_lines(lines,count);	}
	void seek_to_beginning(){throw ns_ex("Not implemented");}

	~ns_image_storage_source_from_net(){
		try{
			_connection.close();	//can be done multiple times as ns_connection ignores duplicates
		}
		catch(ns_ex & ex){
			std::cerr << "~ns_image_storage_source_from_net()::Exception thrown::" << ex.text() << "\n";
		}
		catch(...){
				
			std::cerr << "~ns_image_storage_source_from_net()::Unknown Exception thrown\n";
		}
	}

private:
	void init_send(){
		reciever.init_send();
		ns_image_storage_source<ns_component>::_properties = reciever.properties();
	}
	void finish_send(){
		reciever.finish_send();
		_connection.close();
	}
	ns_image_socket_reciever<ns_component> reciever;
	ns_socket_connection _connection;
};

template<class ns_component>
class ns_image_storage_reciever_handle{
	public:
		ns_image_storage_reciever_handle(ns_image_storage_reciever<ns_component> * handle):reciever(handle){pointer_manager.take(handle);}
		ns_image_storage_reciever<ns_component> & output_stream(){ return *reciever;}

		void bind(ns_image_storage_source<ns_component> * handle){
			if (handle == reciever)
				return;
			pointer_manager.release(reciever);
			reciever = handle;
			pointer_manager.take(reciever);
		}

		//destructor
		~ns_image_storage_reciever_handle(){
			try{
				pointer_manager.release(&reciever);
				}
			catch(ns_ex & ex){
				std::cerr << "~ns_image_storage_reciever_handle()::Exception thrown::" << ex.text() << "\n";
			}
			catch(...){
				
				std::cerr << "~ns_image_storage_reciever_handle()::Unknown Exception thrown\n";
			}
		}  //everything handled by reciever destructors.

		ns_image_storage_reciever_handle<ns_component> & operator=(const ns_image_storage_reciever_handle<ns_component> & r){
			if (reciever == r.reciever)
				return *this;
			pointer_manager.release(&reciever);
			reciever = r.reciever;
			pointer_manager.take(reciever);
			return *this;
		}
		ns_image_storage_reciever_handle(const ns_image_storage_reciever_handle<ns_component> & r){
			reciever = r.reciever;
			pointer_manager.take(reciever);
		}

		void clear(){
			pointer_manager.release(&reciever);
			reciever = 0;
		}
private:
	ns_image_storage_reciever<ns_component> * reciever;
	ns_managed_pointer< ns_image_storage_reciever<ns_component> > pointer_manager;
};

template<class ns_component>
class ns_image_storage_source_handle{
	public:
		ns_image_storage_source_handle(ns_image_storage_source<ns_component> * handle){
			source = handle;
			pointer_manager.take(source);
		}
		ns_image_storage_source<ns_component> & input_stream(){ return *source;}

		const ns_image_storage_source<ns_component> & input_stream() const{ return *source;}

		void bind(ns_image_storage_source<ns_component> * handle){
			if (handle == source)
				return;
			pointer_manager.release(source);
			source = handle;
			pointer_manager.take(source);
		}

		~ns_image_storage_source_handle(){
			try{
			pointer_manager.release(&source);
			}
			catch(ns_ex & ex){
				std::cerr << "~ns_image_storage_source_handle()::Exception thrown by ~ns_image_storage_source_handle()::" << ex.text() << "\n";
			}
			catch(...){
				
				std::cerr << "~ns_image_storage_source_handle()::Unknown Exception thrown by ~ns_image_storage_source_handle()\n";
			}
		}
		ns_image_storage_source_handle<ns_component> & operator=(const ns_image_storage_source_handle<ns_component> & r){
			if (source == r.source)
				return *this;
			pointer_manager.release(&source);
			source = r.source;
			pointer_manager.take(source);
			return *this;
		}
		ns_image_storage_source_handle(const ns_image_storage_source_handle<ns_component> & r){
			source = r.source;
			pointer_manager.take(source);
			//cerr << "Storage Handle Copy Constructor!";
		}
		void clear(){
			pointer_manager.release(&source);
		}

private:
	ns_image_storage_source<ns_component> * source;
	ns_managed_pointer< ns_image_storage_source<ns_component> > pointer_manager;
};


#endif
