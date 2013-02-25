#ifndef NS_PERFORMANCE_STATISTICS_H
#define NS_PERFORMANCE_STATISTICS_H
#include "ns_processing_tasks.h"
#include "ns_high_precision_timer.h"
#include "ns_sql.h"

class ns_performance_statistics_analyzer{
public:

	typedef enum{ns_idle,
				 ns_capturing_image,
				 ns_sql_read,
				 ns_sql_write,
				 ns_running_a_job,
				 ns_volatile_io_read,
				 ns_volatile_io_write,
				 ns_long_term_io_read,
				 ns_long_term_io_write,
				 ns_image_allocation,
				 ns_image_deallocation,
				 ns_last_operating_state
				} ns_operation_state;

	ns_performance_statistics_analyzer():operation_running(ns_process_last_task_marker+ns_last_operating_state,0),
										 operation_timers(ns_process_last_task_marker+ns_last_operating_state),
										 operation_duration_sum(ns_process_last_task_marker+ns_last_operating_state,0), //in microseconds
										 operation_duration_sum_squared(ns_process_last_task_marker+ns_last_operating_state,0), //in milliseconds squared
										 operation_counts(ns_process_last_task_marker+ns_last_operating_state,0){}

	void register_job_duration(const ns_processing_task action, const unsigned long long microseconds){register_job_duration((int)action,microseconds);}
	void register_job_duration(const ns_operation_state state, const unsigned long long microseconds){register_job_duration((int)ns_process_last_task_marker + (int)state,microseconds);}

	void starting_job(const ns_processing_task action)	{starting_job((int)action);}
	void starting_job(const ns_operation_state state)	{starting_job((int)ns_process_last_task_marker + (int)state);}
	
	void finished_job(const ns_processing_task action)	{finished_job((int)action);}
	void finished_job(const ns_operation_state state)	{finished_job((int)ns_process_last_task_marker + (int)state);}
	
	void cancel_outstanding_jobs(){
		for (unsigned int i = 0; i < operation_running.size(); i++)
			operation_running[i] = 0;
	}

	//in seconds
	double mean(const ns_processing_task action) const	{return mean((int)action);}
	double mean(const ns_operation_state state) const	{return mean((int)ns_process_last_task_marker + (int)state);}
	
	double variance(const ns_processing_task action) const	{return variance((int)action);}
	double variance(const ns_operation_state state) const	{return variance((int)ns_process_last_task_marker + (int)state);}

	void update_db(const unsigned long host_id, const ns_processing_task action, ns_sql & sql)const 	{update_db(host_id,(int)action,sql);}	
	void update_db(const unsigned long host_id, const ns_operation_state state, ns_sql & sql)const	{update_db(host_id,(int)ns_process_last_task_marker + (int)state,sql);}

	void update_db(const unsigned long host_id, ns_sql & sql) const {
		for (unsigned int i = 0; i < operation_counts.size(); i++){
			if (operation_counts[i] == 0) continue;
			update_db(host_id,(ns_processing_task)i,sql);
		}
	}
	void clear_db(const unsigned long host_id, ns_sql & sql) const{
		sql << "DELETE FROM performance_statistics WHERE host_id = '" << host_id << "'";
		sql.send_query();
	}

	void add(const ns_performance_statistics_analyzer & a){
		for (unsigned int i = 0; i < a.operation_counts.size(); ++i){
			operation_counts[i]+=a.operation_counts[i];
			operation_duration_sum[i]+=a.operation_duration_sum[i];
			operation_duration_sum_squared[i]+=a.operation_duration_sum_squared[i];
		}
	}
	void merge(const ns_performance_statistics_analyzer & a){
		for (unsigned int i = 0; i < a.operation_counts.size(); ++i){
			if (operation_counts[i] != 0) continue;
			operation_counts[i]=a.operation_counts[i];
			operation_duration_sum[i]=a.operation_duration_sum[i];
			operation_duration_sum_squared[i]=a.operation_duration_sum_squared[i];
		}
	}
private:
	//in seconds
	double mean(const int a) const{
		if (operation_counts[a] == 0) return 0;
		return (operation_duration_sum[a]/(double)operation_counts[a])/(1000.0*1000.0);
	}
	//in seconds
	double variance(const int a) const{
		//E(X^2) - E(X)^2
		if (operation_counts[a] == 0) return 0;
		return ((operation_duration_sum_squared[a]/(double)operation_counts[a]))/1000.0/1000.0 - (mean(a))*(mean(a));
	}

	void starting_job(const int a){
		if (operation_running[a] != 0)
			std::cerr << "\n***ns_performance_statistics_analyzer::check_out()::Checking out already started job: " << a << "***\n\n";
		operation_running[a] = 1;
		operation_timers[a].start();
	}
	void finished_job(const int i){
		const unsigned long long duration(operation_timers[i].stop());
		/*const unsigned long m((unsigned long)( duration/(60*1000*1000)));
		const double s((duration-(60*1000*1000*m)/(1000.0*1000.0)));
		if (output_duration){
			if (m != 0) std::cerr << m << "m";
			std::cerr << s << "s";
		}*/
		register_job_duration(i,duration);
	}
	void register_job_duration(const int i, const ns_64_bit duration){
		#ifdef _WIN32 
		const ns_64_bit max_i(0xffffffffffffffff);
		#else
		const ns_64_bit max_i(0xffffffffffffffffLLU);
		#endif

		if (operation_counts[i] == max_i){
			operation_duration_sum[i] = 0;
			operation_duration_sum_squared[i] = 0;
			operation_counts[i] = 0;
		}
		operation_duration_sum[i] += duration;
		operation_duration_sum_squared[i] += (duration)*(duration)/(1000*1000);
		operation_running[i] = 0;
		operation_counts[i]++;
	}
	void update_db(const unsigned long host_id, const unsigned int i, ns_sql & sql)const {
		sql << "SELECT host_id FROM performance_statistics WHERE host_id = " << host_id << " AND operation= " << i;
		ns_sql_result res;
		sql.get_rows(res);

		if (res.size() == 0)
			sql << "INSERT INTO performance_statistics SET ";
		else sql << "UPDATE performance_statistics SET ";
		sql << " operation='" << i << "', mean='" << ns_to_string(mean(i)) << "',variance='"<< ns_to_string(variance(i)) 
			<< "', count='" << operation_counts[i] << "' ";

		if (res.size() == 0)
			sql << ", host_id = '" << host_id << "'";
		else
			sql << " WHERE host_id = '" << host_id << "' AND operation = '" << (unsigned long)i << "'";
	//	cerr << sql.query() << "\n";
		sql.send_query();

	}
	std::vector<char> operation_running;
	std::vector<ns_high_precision_timer> operation_timers;
	std::vector<ns_64_bit> operation_duration_sum;
	std::vector<ns_64_bit> operation_duration_sum_squared;
	std::vector<ns_64_bit> operation_counts;
};
#endif
