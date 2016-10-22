#include "ns_simple_cache.h"
#include "ns_thread_pool.h"
#include "ns_high_precision_timer.h"
#include <fstream>

class ns_cache_tester_data : public ns_simple_cache_data<int, int,ns_64_bit> {
public:
	ns_64_bit size;
	char * data;
	ns_64_bit id_;
	ns_cache_tester_data() :data(0) {};
	template<class a,class b, bool c>
	friend class ns_simple_cache;

	~ns_cache_tester_data() {
		if (data != 0) { delete[] data; data = 0; }
	}
private:
	ns_64_bit size_in_memory_in_kbytes() const {
		return size;
	}
	void load_from_external_source(const int & id, int & source) {
		data = new char[source];
		id_ = id;
		size = source;
	}
	void clean_up(int & source) {}
	ns_64_bit to_id(const int & id) const {
		return id;
	}
	const ns_64_bit & id() const {
		return id_;
	}
};
struct ns_pool_test_persistant_data {
	ns_pool_test_persistant_data() :thread_id(0) {}
	int thread_id;
};

struct ns_pool_test_external_data {
	ns_simple_cache<ns_cache_tester_data, ns_64_bit,true> * cache;
	std::ostream * debug_output;
	ns_lock * output_lock;
	unsigned long  * max_thread_id;
	ns_lock * thread_id_lock;
};

class ns_thread_pool_cache_tester_job {
public:
	ns_thread_pool_cache_tester_job(const int aw, ns_pool_test_external_data & c) :
		external_data(&c), average_wait_in_milliseconds(aw) {}

	int average_wait_in_milliseconds;
	ns_pool_test_external_data * external_data;
	void operator()(ns_pool_test_persistant_data & persistant_data) {
		if (persistant_data.thread_id == 0) {
			external_data->thread_id_lock->wait_to_acquire(__FILE__, __LINE__);
			(*external_data->max_thread_id)++;
			external_data->thread_id_lock->release();
			persistant_data.thread_id = *external_data->max_thread_id;
		}
		int id = rand() % 100;
		int size = rand() % 1000;


		const bool output(true);
		bool clear_cache = rand() % 100 < 5;
		if (clear_cache) {
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "d" << id << "? ";
				external_data->output_lock->release();
			}

			external_data->cache->remove_old_images(0, size);
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "d" << id << "! ";
				external_data->output_lock->release();
			}
		}
		bool write = rand() % 20 < 5;
		int wait = (int)(average_wait_in_milliseconds * (500 + rand() % 200 - 100) / 500.0);
		if (wait < 0)
			wait = 0;
		if (write) {
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "w" << id << "? ";
				external_data->output_lock->release();
			}
			ns_high_precision_timer t;
			t.start();
			ns_simple_cache<ns_cache_tester_data, ns_64_bit, true>::handle_t h;
			external_data->cache->get_for_write(id, h, size);

			ns_64_bit res(t.stop());
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "w" << id << "( ";
				external_data->output_lock->release();
			}

			ns_thread::sleep_microseconds(wait / 2);
			int n = h().size + (long)(rand() % 200) - 100;
			if (n < 0) n = 200;
			h().size = n;
			delete[](h().data);
			h().data = new char[h().size];
			for (unsigned int i = 0; i < h().size; i++) {
				h().data[i] = 0;
			}
			ns_thread::sleep_microseconds(wait / 2);
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "w" << id << ") ";
				external_data->output_lock->release();
			}
			h.release();
		}
		else {
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "r" << id << "? ";
				external_data->output_lock->release();
			}
			ns_high_precision_timer t;
			t.start();

			ns_simple_cache<ns_cache_tester_data, ns_64_bit,true>::const_handle_t h;

			external_data->cache->get_for_read(id, h, size);

			ns_64_bit res(t.stop());
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "r" << id << "( ";
				external_data->output_lock->release();
			}

			int sum(0);
			for (unsigned int i = 0; i < h().size; i++)
				sum += h().data[i];

			ns_thread::sleep_microseconds(wait);
			if (output) {
				external_data->output_lock->wait_to_acquire(__FILE__, __LINE__);
				std::cerr << persistant_data.thread_id << "r" << id << ") ";
				external_data->output_lock->release();
			}
			h.release();
		}

	}
};


void ns_test_simple_cache(char * debug_output){
	ns_simple_cache<ns_cache_tester_data,ns_64_bit, true> cache(10000);
	ns_lock output_lock("OL");
	ns_lock thread_id_lock("TL");
	ns_pool_test_external_data data;
	unsigned long max_thread_id = 0;
	data.cache = &cache;
	data.output_lock = &output_lock;
	data.max_thread_id = &max_thread_id;
	data.thread_id_lock = &thread_id_lock;
	std::ofstream debug_out(debug_output);
	debug_out << "thread_id,action,wait_time\n";
	data.debug_output = &debug_out;
	const int average_wait_time_in_milliseconds(5);

	ns_thread_pool<ns_thread_pool_cache_tester_job, ns_pool_test_persistant_data> pool;
	//pool.debug.open("c:\\server\\thread_debug.txt");
	pool.set_number_of_threads(100);
	pool.prepare_pool_to_run();
	for (unsigned int j = 0; j < 100; j++) {
		int num(rand() % 1000);
		std::cerr << "Running a round with " << num << " jobs\n";
		for (unsigned int i = 0; i < num; i++)
			pool.add_job_while_pool_is_not_running(ns_thread_pool_cache_tester_job(average_wait_time_in_milliseconds, data));
		pool.run_pool();
		pool.wait_for_all_threads_to_become_idle();
		cache.clear_cache(num);
		std::cerr << "Done\n";
	}
}