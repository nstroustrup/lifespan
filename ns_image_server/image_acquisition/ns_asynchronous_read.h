#ifndef NS_ASYNCHRONOUS_READ
#define NS_ASYNCHRONOUS_READ
#include "ns_ex.h"

//allows the stderr output of a child process to be read at the same time as its stdout

class ns_asynchronous_read;
class ns_capture_device;

template<class reciever_t>
struct ns_asynchronous_read_info{
	ns_asynchronous_read * asynch;
	reciever_t * reciever;
};

class ns_asynchronous_read{
public:
	ns_asynchronous_read(ns_external_execute * ex):exec(ex){}

	void read(){
	//	cerr << "Reading from stderr\n";
		char buf[1024*10+1];
		int i;
		while (true){
			//cerr << "Reading from stderr";
			i = exec->read_stderr(buf,1024*10);
			//cerr << "Read " << i << " bytes from stderr\n";
			buf[i] = 0;
			_result+=buf;
			if (i == 0)
				break;
		}
		//we can't call finished_reading_from_stderr here because of a potential race condition.
		//exec->finished_reading_from_stderr();
		//it will be called in the primary capture thread.
	}
	const std::string & result()const {return _result;}
private:
	std::string _result;
	ns_external_execute * exec;
};
template<class reciever_t>
ns_thread_return_type asynchronous_read_start(void * read_info){
	ns_asynchronous_read_info<reciever_t> * ar = static_cast<ns_asynchronous_read_info<reciever_t> *>(read_info);
	try{
		ar->asynch->read();
		return 0;
	}
	catch(std::exception & exception){
		ns_ex ex(exception);

		//ns_thread self = ns_thread::get_current_thread();
		//cerr << ex.text() << "\n";
		//self.detach();
		ar->reciever->throw_delayed_exception(ns_ex("Asynchronous Read::") << ex.text());
		return 0;
	}
}
#endif
