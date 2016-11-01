#ifndef NS_THREAD_POOL
#define NS_THREAD_POOL

#include <vector>
#include <queue>
#include "ns_thread.h"
#include "stdlib.h"

#undef NS_THREAD_POOL_DEBUG

#ifdef NS_THREAD_POOL_DEBUG
#include <fstream>
#define NS_TPDBG( x ) x
#else
#define NS_TPDBG( x )
#endif

template<class job_specification_t, class thread_persistant_data_t>
class ns_thread_pool;

template<class job_specification_t, class thread_persistant_data_t>
struct ns_thread_pool_thread_init_data {
	ns_thread_pool<job_specification_t, thread_persistant_data_t> * pool;
	unsigned long thread_id;

	#ifdef NS_THREAD_POOL_DEBUG
	std::ofstream * debug;
	#endif
};

template<class job_specification_t>
struct ns_thread_pool_exception{
	ns_thread_pool_exception() {};
	ns_thread_pool_exception(const job_specification_t & j, const ns_ex & e) :job(j), ex(e) {}
	job_specification_t job;
	ns_ex ex;
};

template<class job_specification_t,class thread_persistant_data_t>
class ns_thread_pool {
public:
	ns_thread_pool() :state(ns_uninitialized),job_access_lock("ns_tpiatp"), wait_after_jobs_finish("ns_tpw"), thread_init_lock("ns_tic"), error_access_lock("ns_tpierr"), shutdown_(false) {}

	~ns_thread_pool() { shutdown(); }
	typedef enum { ns_uninitialized, ns_paused, ns_polling_for_idle_threads,ns_holding_idle_threads,ns_running } ns_thread_pool_state;
	ns_thread_pool_state state;
	//gets the oldest exception thrown by any thread.
	//returns the number errors in queue (including the one returned); 0 if no errors exist
	long get_next_error(job_specification_t & job, ns_ex & ex) {
		error_access_lock.wait_to_acquire(__FILE__, __LINE__);
		if (errors.empty()) {
			error_access_lock.release();
			return 0;
		}
		unsigned long number_of_errors = (unsigned long)errors.size();
		job = errors.front().job;
		ex = errors.front().ex;
		errors.pop();
		error_access_lock.release();
		return number_of_errors;
	}

	void set_number_of_threads(unsigned long i) {
		if (state != ns_uninitialized)
			throw ns_ex("Cannot change number of threads on an active thread pool");
		threads.clear();
		threads.resize(i);
		thread_idle_locks.clear();
		thread_idle_locks.reserve(i);
		for (unsigned int j = 0; j < i; j++)
			thread_idle_locks.push_back(ns_lock(std::string("ns-tpi-") + ns_to_string(j)));
	}
	unsigned long number_of_threads() const {
		return thread_idle_locks.size();
	}

	//waits until the the job queue becomes empty
	//important!  this holds onto the wait_after_jobs_finish lock
	//this does not require all threads to be finished
	void poll_until_jobs_queue_is_empty_and_hold_idle_threads() {
		if (state != ns_running)
			throw ns_ex("Attempting to poll on non_running thread pool");
		while (true) {
			job_access_lock.wait_to_acquire(__FILE__, __LINE__);
			if (jobs.empty())
				break;
			job_access_lock.release();
			ns_thread::sleep_milliseconds(1);
		}
		job_access_lock.release();
		wait_after_jobs_finish.wait_to_acquire(__FILE__, __LINE__);
		state = ns_holding_idle_threads;
	}
	//call this after calling poll_until_jobs_queue_is_empty_and_hold_idle_threads()
	//to let the idle threads get back to work.
	void release_hold_on_idle_threads() {
		wait_after_jobs_finish.release();
		state = ns_running;
	}
	bool any_thread_is_idle() {
		return number_of_idle_threads(true) > 0;
	}
	unsigned long number_of_idle_threads(bool any=false) {
		unsigned long n(0);
		for (int i = 0; i < thread_idle_locks.size(); i++) {
			if (thread_idle_locks[i].try_to_acquire(__FILE__, __LINE__)) {
				n++;
				thread_idle_locks[i].release();
				//short circuit
				if (any) return 1;
			}
		}
		return n;
	}

	//this function waits for all jobs in the queue to be finished processing,
	//and all threads to become idle
	//important!  This /keeps/ the job_access_lock for the current thread.
	void wait_for_all_threads_to_become_idle() {
		if (state != ns_running)
			throw ns_ex("Waiting for all threads to be idle on non-running thread pool!");
		state = ns_polling_for_idle_threads;
		//wait until whichever thread has the running lock gives it up
		//now grab control of everything
		while (true) {

			wait_after_jobs_finish.wait_to_acquire(__FILE__, __LINE__);
			bool give_up_and_try_again(false);
			unsigned int i;
			for (i = 0; i < thread_idle_locks.size(); i++) {
				NS_TPDBG(debug << "(main waits for thread lock " << i << ")");
				thread_idle_locks[i].wait_to_acquire(__FILE__, __LINE__);
				NS_TPDBG(debug << "(main gets t" << i << " idle lock)");

				NS_TPDBG(debug << "(main waits for job lock)");
				job_access_lock.wait_to_acquire(__FILE__, __LINE__);
				if (!jobs.empty()) {
					job_access_lock.release();
					NS_TPDBG(debug << "(main finds jobs and releases job and idle threads)");
					//oops! someone has added a job.  we must give up waiting for idle threads until someone handles it.
					give_up_and_try_again = true;
					break;
				}
				else	job_access_lock.release();
				//we retain the job lock
			}

			if (give_up_and_try_again) {
				for (unsigned int j = 0; j < i; j++)
					thread_idle_locks[j].release();
				wait_after_jobs_finish.release();
				ns_thread::sleep_milliseconds(1);
				continue;
			}

			NS_TPDBG(debug << "(main gets all idle locks!)");

			//NS_TPDBG(debug << "(main waits for job lock)");
			//job_access_lock.wait_to_acquire(__FILE__, __LINE__);
			NS_TPDBG(cerr << "mw ");
			if (!jobs.empty()) {
				NS_TPDBG(debug << "(main gets job lock but finds jobs)");
				job_access_lock.release();
				wait_after_jobs_finish.release();
				ns_thread::sleep_milliseconds(1);
				continue;
			}

			//ok! we have all the idle locks (all threads are idle)
			//and there are no jobs left in the queue

			//set this up for loading more jobs
			job_access_lock.wait_to_acquire(__FILE__, __LINE__);
			//and release all the locks
		/*	for (unsigned int i = 0; i < thread_idle_locks.size(); i++)
				thread_idle_locks[i].release();
				*/
			state = ns_paused;
			return;
		}
	}
	void shutdown () {
		//wait to get out of temporary states
		if (state == ns_holding_idle_threads) {
			wait_after_jobs_finish.wait_to_acquire(__FILE__, __LINE__);
			wait_after_jobs_finish.release();
		}
		if (state == ns_polling_for_idle_threads) {
			while(true)
				if (job_access_lock.try_to_acquire(__FILE__, __LINE__)) {
					job_access_lock.release();
				}
				else {
					if (state != ns_polling_for_idle_threads)
						break;
				}
				ns_thread::sleep_milliseconds(1);
		}
		
		switch (state) {
		case ns_uninitialized: break;
		case ns_paused: 
			run_pool();
			//deliberate run-through
		case ns_running:
			shutdown_ = true;
			for (unsigned int i = 0; i < threads.size(); i++) {
				threads[i].block_on_finish();
			}
			break;
		default: throw ns_ex("Unknown state encountered during thread pool shutdown");
		}
		threads.resize(0);
		thread_idle_locks.resize(0);
		shutdown_ = false;
	}

	void prepare_pool_to_run() {
		if (state != ns_uninitialized)
			throw ns_ex("Prepping initialized pool!");
		if (threads.size() == 0)
			throw ns_ex("No threads!");

		//we grab job_access so that we can add jobs after prepare_pool_to_run() is called
		job_access_lock.wait_to_acquire(__FILE__, __LINE__);

		//we grab wait_after_jobs_finish as a precodition to running run_pool() which releases them
		wait_after_jobs_finish.wait_to_acquire(__FILE__, __LINE__);
		for (unsigned int i = 0; i < threads.size(); i++) {
			//we grab these locks as a precodition to running run_pool() which releases them
			thread_idle_locks[i].wait_to_acquire(__FILE__, __LINE__);
			//put this on the heap so the thread can access it whenever it pleases.
			ns_thread_pool_thread_init_data<job_specification_t,thread_persistant_data_t> * data = new ns_thread_pool_thread_init_data<job_specification_t, thread_persistant_data_t>;
			data->pool = this;
			data->thread_id = i;
#ifdef NS_THREAD_POOL_DEBUG
			data->debug = &debug;
#endif
			threads[i].run(this->run, data);
		}
		state = ns_paused;
	}
	void run_pool() {
		if (state != ns_paused)
			throw ns_ex("Attempting to run a non-paused pool!");
		NS_TPDBG(debug << "(main waiting for thread init lock");
		thread_init_lock.wait_to_acquire(__FILE__, __LINE__);
		//we'll need to wait until all threads are initialized
		thread_init_count = 0;
		thread_init_lock.release();

		NS_TPDBG(debug << "(main releasing all idle locks");
		for (unsigned int i = 0; i < threads.size(); i++)
			thread_idle_locks[i].release();

		NS_TPDBG(debug << "(main releasing wait locks");
		wait_after_jobs_finish.release();

		//wait until all threads are initialized
		while (true) {

			NS_TPDBG(debug << "(main waiting for thread init count...)");
			thread_init_lock.wait_to_acquire(__FILE__, __LINE__);
			const unsigned long c(thread_init_count);
			thread_init_lock.release();
			if (c == threads.size())
				break;
			ns_thread::sleep_milliseconds(1);
		}
		job_access_lock.release();
		state = ns_running;
	}

	void add_job_while_pool_is_not_running(const job_specification_t & job) {
		jobs.push(job);
	}
	unsigned long number_of_jobs_pending() {
		job_access_lock.wait_to_acquire(__FILE__, __LINE__);
		const unsigned long n(jobs.size());
		job_access_lock.release();
		return n;
	}
	void add_job_while_pool_is_running(const job_specification_t & job) {
		job_access_lock.wait_to_acquire(__FILE__, __LINE__);
		add_job_while_pool_is_not_running(job);
		job_access_lock.release();
	}
	static ns_thread_return_type run(void * pool_data) {
		//copy this from the heap to the stack, and free up the heap.
		ns_thread_pool_thread_init_data<job_specification_t,thread_persistant_data_t> * pp = (ns_thread_pool_thread_init_data<job_specification_t, thread_persistant_data_t> *)pool_data;
		ns_thread_pool_thread_init_data<job_specification_t, thread_persistant_data_t> p = *pp;
		delete pp;


		thread_persistant_data_t persistant_data;


		bool thread_has_been_idle = true;
		ns_acquire_lock_for_scope thread_idle_lock(p.pool->thread_idle_locks[p.thread_id], __FILE__, __LINE__, false);
		while (true) {
			//register thread as not idle 
			if (thread_has_been_idle) {
				NS_TPDBG(*p.debug << "(t" << p.thread_id << " waits for its idle lock)");
				thread_idle_lock.get(__FILE__, __LINE__);
				NS_TPDBG(*p.debug << "(t" << p.thread_id << " gets its idle lock)");

				//mark as running
				p.pool->thread_init_lock.wait_to_acquire(__FILE__, __LINE__);
				p.pool->thread_init_count++;
				p.pool->thread_init_lock.release();
				thread_has_been_idle = false;
			}

			//a shutdown is signalled!  Exit.
			if (p.pool->shutdown_) {
				NS_TPDBG((*p.debug) << "(t" << p.thread_id << " shutsdown)");
				thread_idle_lock.release();
				return 0;
			}
			NS_TPDBG(*p.debug << "(t" << p.thread_id << " requests job lock)");
			ns_acquire_lock_for_scope job_access_lock(p.pool->job_access_lock, __FILE__, __LINE__);

			NS_TPDBG(*p.debug << "(t" << p.thread_id << " gets job lock)");
			//a shutdown is signalled!  Exit.
			if (p.pool->shutdown_) {
				NS_TPDBG(*p.debug << "(t" << p.thread_id << " shutsdown)");
				thread_idle_lock.release();
				job_access_lock.release();
				return 0;
			}
			//if the pool of jobs is empty, the thread goes idle and wait on the job access lock until something happens.
			//if some other thread is waiting for all jobs to be done, it will grab this job access lock,
			//so this thread will not poll the jobs queue unneccisarily
			if (p.pool->jobs.empty()) {
				//go idle
				NS_TPDBG(*p.debug << "(t" << p.thread_id << " goes idle)");
				thread_idle_lock.release();

				NS_TPDBG(*p.debug << "(t" << p.thread_id << " finds no jobs and releases lock)");
				job_access_lock.release();

				//wait for more work to arrive someday
				NS_TPDBG(*p.debug << "(t" << p.thread_id << " waits for wait lock)");
				p.pool->wait_after_jobs_finish.wait_to_acquire(__FILE__, __LINE__);
				p.pool->wait_after_jobs_finish.release();

				//more work may have arrived!  the thread is released to look for more jobs
				thread_has_been_idle = true;
				ns_thread::sleep_milliseconds(1);
				continue;
			}
			//grab the job
			NS_TPDBG(*p.debug << "(t" << p.thread_id << " finds a job and releases lock)");

			job_specification_t job = p.pool->jobs.front();
			p.pool->jobs.pop();
			job_access_lock.release();

			//run the job!
			try {
				job(persistant_data);
			}
			catch (ns_ex & ex) {
				NS_TPDBG(*p.debug << "(t" << p.thread_id << " encounters an error " << ex.text() << ")");
				p.pool->error_access_lock.wait_to_acquire(__FILE__, __LINE__);
				p.pool->errors.push((ns_thread_pool_exception<job_specification_t>(job,ex)));
				p.pool->error_access_lock.release();
			}
			catch (std::exception & e) {
				p.pool->error_access_lock.wait_to_acquire(__FILE__, __LINE__);
				p.pool->errors.push(ns_thread_pool_exception<job_specification_t>(job, ns_ex(std::string("std::exception::") + e.what())));
				p.pool->error_access_lock.release();
			}
			catch (...) {
				p.pool->error_access_lock.wait_to_acquire(__FILE__, __LINE__);
				p.pool->errors.push(ns_thread_pool_exception<job_specification_t>(job,ns_ex("Unknown error")));
				p.pool->error_access_lock.release();
			}
		}
		return 0;
	}

#ifdef NS_THREAD_POOL_DEBUG
	std::ofstream debug;
#endif

private:


	std::queue<job_specification_t> jobs;
	ns_lock job_access_lock;


	std::queue<ns_thread_pool_exception<job_specification_t> > errors;
	ns_lock error_access_lock;

	ns_lock wait_after_jobs_finish;

	std::vector<ns_lock> thread_idle_locks;
	bool shutdown_;
	std::vector<ns_thread> threads;

	ns_lock thread_init_lock;
	unsigned long thread_init_count;
};

#endif

