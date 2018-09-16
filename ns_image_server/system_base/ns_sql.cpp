#include "ns_sql.h"
#include "ns_thread.h"
#include <string.h>
#include <stdio.h>
#include <algorithm>

#ifdef _WIN32 
#include <errmsg.h>
#else
#include "errmsg.h"
#endif

#undef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS

#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
#include "ns_image_server.h"
#endif
#include <stdlib.h>
using namespace std;

ns_query & ns_query::write_data(const char * buffer, const unsigned int length){

	unsigned int begin = (unsigned int)str.size();
	str.resize(begin+length);
	for (unsigned int i = 0; i < length; i++)
		str[begin+i] = buffer[i];
	return *this;
};

bool ns_sql_connection::thread_safe(){return ns_mysql_header::mysql_thread_safe()==1;}


ns_sql_connection::~ns_sql_connection(){disconnect();}

ns_acquire_lock_for_scope ns_sql_connection::get_lock(const char * file, unsigned long line,bool check_for_allocation) {
	if (check_for_allocation && !mysql_internal_data_allocated)
		throw ns_ex("Unallocated SQL data!");
	//local_locking_behavior = ns_global_locking;
	switch (local_locking_behavior) {
		case ns_no_locking: return ns_acquire_lock_for_scope(local_lock, file, line,false);
		case ns_global_locking: return ns_acquire_lock_for_scope(global_sql_lock, file, line);
		case ns_thread_locking: return ns_acquire_lock_for_scope(local_lock, file, line);
	}
	throw ns_ex("Unknown locking behavior!");
}
void ns_sql_connection::connect(const std::string & server_name, const std::string & user_id, const std::string & password, const unsigned int retry_count){
	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__,false));
	_server_name = server_name;
	_user_id = user_id;
	_password = password;
	_retry_count = retry_count;
	simulate_errors_if_requested();
  if (ns_mysql_header::mysql_init(&mysql)==NULL)
	  throw ns_ex("ns_sql_connection::connect():Insufficient memory to allocate new MYSQL object") << ns_sql_fatal;
  mysql_internal_data_allocated = true;

  ns_mysql_header::MYSQL * success = 0;
  ns_mysql_header::my_bool reconnect(true);
  ns_mysql_header::mysql_options(&mysql,ns_mysql_header::MYSQL_OPT_RECONNECT,&reconnect);
  ns_mysql_header::mysql_options(&mysql,ns_mysql_header::MYSQL_OPT_COMPRESS,0);

  for (unsigned int i = 0; i <= retry_count && success == 0;i++){
	  success = ns_mysql_header::mysql_real_connect( &mysql,server_name.c_str(), user_id.c_str(), password.c_str(), "", 0 , 0, CLIENT_REMEMBER_OPTIONS);
	  if (success == NULL && retry_count != i)
		ns_thread::sleep(10);
  }
  if (success == NULL){
	    disconnect();
		string blocked_password = password;  
		for (int i = 0; i < blocked_password.size(); i++)
			blocked_password[i] = '*';
		if (password.empty())
			blocked_password = "(Empty password)";
	//	std::cout << "Current password: " << password.c_str() << "\n";;
		throw ns_ex() << "ns_sql_connection::Could not connect to server with the credentials (username;password;hostname)=(" << user_id.c_str() << ";" << blocked_password << ";" << server_name.c_str() << "): " << latest_error() << ns_sql_fatal;
  }
  lock.release();
}

void ns_sql_connection::disconnect(){
  ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__,false));
  if (mysql_internal_data_allocated){
	 ns_mysql_header::mysql_close(&mysql);
	 mysql_internal_data_allocated = false;
  }
  ns_mysql_header::mysql_thread_end();
  lock.release();
}

void ns_sql_connection::select_db(const std::string & db_name){
  ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
  if (mysql_select_db(&mysql,db_name.c_str()))
    throw ns_ex() << "ns_sql_connection::Could not select database: " << db_name << ns_sql_fatal;
  lock.release();
}

void ns_sql_connection::set_autocommit(const bool & commit_){
	// TODO: why are we both calling mysql_autocommit and then sending a SET AUTOCOMMIT query??

	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	ns_mysql_header::my_bool res = ns_mysql_header::mysql_autocommit(&mysql, (ns_mysql_header::my_bool)commit_);
	
	lock.release();
	simulate_errors_if_requested();
	if (!commit_)
		this->send_query("SET AUTOCOMMIT = 0");
	else
		this->send_query("SET AUTOCOMMIT = 1");
	commit = commit_;
	if (res != 0)
		throw ns_ex("ns_sql_connection::Could not set autocommit state.") << ns_sql_fatal;
	
}


std::string ns_sql_connection::latest_error(){

	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	const char * a = ns_mysql_header::mysql_error(&mysql);
	std::string err(a);
	lock.release();
	return err;
}

void ns_mysql_real_query(ns_mysql_header::MYSQL *mysql, const char *stmt_str, unsigned long length){
	int res(ns_mysql_header::mysql_real_query(mysql,stmt_str, length));
	const char * prob(ns_mysql_header::mysql_error(mysql));
	switch(res){
		case 0: return;
		case 1:{ //undocumented error code seems to be thrown on malformed querries
				std::string err("ns_sql_connection::Error in query: ");
				err += std::string(stmt_str) + "\n(" +  prob +")";
				throw ns_ex(err) << ns_sql_fatal;
			   }
		case CR_COMMANDS_OUT_OF_SYNC: throw ns_ex("Commands were executed in an improper order:") << prob;
		case CR_SERVER_GONE_ERROR:	throw ns_ex("The MySQL server has gone away:") << prob;
		case CR_SERVER_LOST:	throw ns_ex("The connection to the server was lost during the query:") << prob;
		case CR_UNKNOWN_ERROR:	throw ns_ex("An unknown error has occurred:") << prob;
		default:	
			throw ns_ex("mysql_real_query() returned an unknown error code:") << res << ":" << prob;
	}
}


void ns_sql_connection::check_connection(){
	ns_sql_result res;
	clear_query();
	*this << "SELECT NOW()";
	get_rows(res);
	if (res.size() != 1)
		throw ns_ex("ns_sql_connection::check_connection()::received an unexpected reply size from the server: ") << res.size();
}



void ns_sql_connection::send_query(const std::string & query){

	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	try{
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
			ns_mysql_real_query(&mysql,query.data(), static_cast<unsigned long>(query.size()));
		ns_ex stored_ex[3] = {ns_ex(), ns_ex(), ns_ex()};
		unsigned int i;
		bool success = false;
		for (i=0; i < 3 && !success; i++){
		  try{
			  std::string error_str;
			  error_str = ns_mysql_header::mysql_error(&mysql);
			  if(error_str.size() != 0)
				throw ns_ex("ns_sql_connection::Error in query: ") << query<< "\n" << error_str;
			  
			  simulate_errors_if_requested();

			  ns_mysql_header::MYSQL_RES * sql_result;
			  sql_result = ns_mysql_header::mysql_store_result(&mysql);
			  if(sql_result){
				ns_mysql_header::mysql_free_result(sql_result);
				throw ns_ex() << "ns_sql_connection::send_query provided with a query that seems to request data: " << query;
			  }
			  else{
				  if (ns_mysql_header::mysql_field_count(&mysql) != 0)  
					throw ns_ex() << "ns_sql_connection::send_query provided with a query that seems to request data: " << query;
			  }
		  
			  success = true;
		  }
		  catch(ns_ex & ex){
			  stored_ex[i] = ex;	  
			//  if (i == 0)
			//	  send_query("COMMIT");	//XXX avoid deadlocks (A hack!)
			//  if (i == 1){ //on second failure to execure query, disconnect and reconnect.
			//	disconnect();
			//	connect(_server_name,_user_id,_password,_retry_count);
			//  }
		  }
	  
			current_query.clear();
		}
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		image_server.performance_statistics.register_job_duration(ns_performance_statistics_analyzer::ns_sql_write,tp.stop());
		#endif

		if (i == 3){
		  ns_ex ex("ns_sql_connection::Three consecutive attempts failed, producing the following results: ");
		  for (unsigned int j = 0; j < 3; j++)
			ex << "\n " << stored_ex[j].text();
		  ex << ns_sql_fatal;
		  this->clear_query();
		  throw ex;
		}
		else if (i > 1){
		  ns_ex ex("ns_sql_connection::The following error(s) occured but ns_sql_connection was able to recover: ");
		  for (unsigned int j = 0; j < i; j++)
			ex << "\n " << stored_ex[j].text();
		}
	}
	catch(...){
		current_query.clear();
		throw;
	}
	lock.release();
}
void ns_sql_connection::send_query(){
	if (current_query.to_str().size() == 0)
		throw ns_ex("ns_sql_connection::Attempting to send an empty cached query!");

	this->send_query(current_query.to_str());
}


//should handle binary data correctly.
void ns_sql_connection::get_rows(const std::string & query, std::vector< std::vector<std::string> > & result){

	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	try{
		result.resize(0);
		//removed semicolon
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
		ns_mysql_real_query(&mysql,query.data(), static_cast<unsigned long>(query.size()));
		simulate_errors_if_requested();
		std::string error_str;
		error_str = ns_mysql_header::mysql_error(&mysql);

		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
			image_server.performance_statistics.register_job_duration(ns_performance_statistics_analyzer::ns_sql_write,tp.stop());
		#endif

		if(error_str.size() != 0){
			std::string err("ns_sql_connection::Error in query: ");
			err += query + "\n(" + error_str +")";
			clear_query();
			throw ns_ex(err) << ns_sql_fatal;
		}
		ns_mysql_header::MYSQL_RES * sql_result;
		ns_mysql_header::MYSQL_ROW   sql_row;
			
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		tp.start();
		#endif

		sql_result = ns_mysql_header::mysql_use_result(&mysql);
		try{
				
			simulate_errors_if_requested();
		if (sql_result == 0)
			return;

			while (sql_row = ns_mysql_header::mysql_fetch_row(sql_result)){
				unsigned long num_columns(ns_mysql_header::mysql_num_fields(sql_result));
				unsigned long * column_lengths(ns_mysql_header::mysql_fetch_lengths(sql_result));
				if (num_columns == 0 || sql_row == 0 || sql_row[0] == 0)
					continue;
				unsigned int result_row = (unsigned int)result.size();
				result.resize(result_row+1,std::vector<std::string>(num_columns));

				for (unsigned int column = 0; column < num_columns; column++){
					result[result_row][column].resize(column_lengths[column]);
					for (unsigned int i = 0; i < column_lengths[column]; i++)
						result[result_row][column][i] = sql_row[column][i];
				}
			}	
		}
		catch(ns_ex & ex){	
			ns_mysql_header::mysql_free_result(sql_result); 
			#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
			image_server.performance_statistics.register_job_duration(ns_performance_statistics_analyzer::ns_sql_read,tp.stop());
			#endif
			throw ex << ns_sql_fatal;
		}
		ns_mysql_header::mysql_free_result(sql_result);
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		image_server.performance_statistics.register_job_duration(ns_performance_statistics_analyzer::ns_sql_read,tp.stop());
		#endif
	}
	catch(...){
		current_query.clear();
		throw;
	}
	lock.release();
}

void ns_sql_connection::get_rows(std::vector< std::vector<std::string> > & result){
	if (current_query.to_str().size() == 0)
		throw ns_ex("ns_sql_connection::Attempting to send an empty cached query.");
	this->get_rows(current_query.to_str(),result);
	current_query.clear();
}
ns_lock escape_string_lock("lk");
std::string ns_sql_connection::escape_string(const std::string & str){

	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	char * escaped_string = new char[str.length()*2 + 1];
	const unsigned long escaped_length(
		ns_mysql_header::mysql_real_escape_string(&mysql,escaped_string,str.c_str(),(unsigned long)str.length()));
	std::string ret;
	ret.resize(escaped_length);
	for (unsigned long i = 0; i < escaped_length; i++)
		ret[i] = escaped_string[i];
	delete[] escaped_string;
	lock.release();
	return ret;
}

ns_sql_connection & ns_sql_connection::write_data(const char * buffer, const unsigned long length){	

	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	char * escaped_string = new char[length*2 + 1];
	const unsigned long escaped_length(mysql_real_escape_string(&mysql,escaped_string,buffer,length));
	current_query.write_data(escaped_string, escaped_length);
	delete[] escaped_string;
	lock.release();
	return *this;
}

//get a single row from the db
//should be binary safe
void ns_sql_connection::get_single_row(const std::string & query,std::vector<std::string> &res){
	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	try{
		res.clear(); 
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		ns_high_precision_timer tp;
		tp.start();
		#endif
		ns_mysql_real_query(&mysql,(query+";").c_str(), static_cast<unsigned long>(query.size()));
		simulate_errors_if_requested();
		std::string error_str;

		error_str = ns_mysql_header::mysql_error(&mysql); 
		simulate_errors_if_requested();

		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		image_server.performance_statistics.register_job_duration(ns_performance_statistics_analyzer::ns_sql_write,tp.stop());
		#endif
		if(error_str.size() != 0)
			throw ns_ex("ns_sql_connection::Invalid query: ") << query << error_str;

		ns_mysql_header::MYSQL_RES * sql_result;
		ns_mysql_header::MYSQL_ROW   sql_row;
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		tp.start();
		#endif
		sql_result = ns_mysql_header::mysql_use_result(&mysql);
		simulate_errors_if_requested();
		try{
		sql_row = ns_mysql_header::mysql_fetch_row(sql_result);
		#ifdef USE_IMAGE_SERVER_PERFORMANCE_STATISTICS
		image_server.performance_statistics.register_job_duration(ns_performance_statistics_analyzer::ns_sql_read,tp.stop());
		#endif

		if (sql_row == 0)
			throw ns_ex("ns_sql_connection::No rows returned by a get_single_row().");

		unsigned long num_columns(ns_mysql_header::mysql_num_fields(sql_result));
		unsigned long * column_lengths(ns_mysql_header::mysql_fetch_lengths(sql_result));
		if (num_columns == 0 || sql_row == 0 || sql_row[0] == 0)
			return;
		res.resize(num_columns);

		for (unsigned int column = 0; column < num_columns; column++){
			res[column].resize(column_lengths[column]);
			for (unsigned int i = 0; i < column_lengths[column]; i++)
				res[column][i] = sql_row[column][i];
		}
		sql_row = mysql_fetch_row(sql_result);
		if (sql_row != 0)
			throw ns_ex("ns_sql_connection::Multiple rows returned by get_single_row().");
		}
		catch(ns_ex & ex){
			mysql_free_result(sql_result);
			throw ex;
		}
		ns_mysql_header::mysql_free_result(sql_result);
	}
	
	catch(...){
		current_query.clear();
		throw;
	}
	lock.release();
}
void ns_sql_connection::get_single_row(std::vector<std::string> &res){
	if (current_query.to_str().size() == 0)
		throw ns_ex("Attempting to send an empty cached query!");
	this->get_single_row(current_query.to_str(),res);
	current_query.clear();
}

std::string ns_sql_connection::get_value(const std::string & query){
	std::vector<std::string> res;
	get_single_row(query,res);
	if (res.size() != 1)
		throw ns_ex("ns_sql_connection::") << static_cast<unsigned long>(res.size()) << " values received by get_value()";
	current_query.clear();
	return res[0];
}
std::string ns_sql_connection::get_value(){
	if (current_query.to_str().size() == 0)
		throw ns_ex("ns_sql_connection::Attempting to send an empty cached query.");
	return this->get_value(current_query.to_str());

}
int ns_sql_connection::get_integer_value(const std::string & query){
	return atoi(get_value(query).c_str());
}

void ns_sql_connection::clear_query() {
	current_query.clear();
}


int ns_sql_connection::get_integer_value(){
	if (current_query.to_str().size() == 0)
		throw ns_ex("ns_sql_connection::Attempting to send an empty cached query.");
	return this->get_integer_value(current_query.to_str());

}

unsigned long ns_sql_connection::get_ulong_value(const std::string & query){
	return atol(this->get_value(query).c_str());
}

unsigned long ns_sql_connection::get_ulong_value(){
	if (current_query.to_str().size() == 0)
		throw ns_ex("ns_sql_connection::Attempting to send an empty cached query.");
	return this->get_ulong_value(current_query.to_str());
}

ns_64_bit ns_sql_connection::send_query_get_id(const std::string & query){
	this->send_query(query);

	ns_acquire_lock_for_scope lock(get_lock(__FILE__, __LINE__));
	const ns_64_bit id(ns_mysql_header::mysql_insert_id(&mysql));
	lock.release();
	if (id == 0)
		throw ns_ex("Requesting a column ID from an insert without an AUTO_INCREMENT column!");
	return id;

}
ns_64_bit ns_sql_connection::send_query_get_id(){
	if (current_query.to_str().size() == 0)
		throw ns_ex("ns_sql_connection::Attempting to send an empty cached query.");
	return this->send_query_get_id(current_query.to_str());
}
void ns_sql_full_table_lock::lock(const ns_table_list & tables_to_lock){
	if(sql->query().size())
		throw ns_ex("ns_sql_full_table_lock::lock()::Query not cleared before attempting Lock");
	if (tables_to_lock.size() == 0)
		throw ns_ex("ns_sql_full_table_lock::lock()::No tables specified");

	ns_acquire_lock_for_scope lock(sql->get_lock(__FILE__, __LINE__));
	final_autocommit_state = sql->autocommit_state();
	sql->set_autocommit(false);
	lock.release();
	sql->send_query("BEGIN");
	*sql << "LOCK TABLES "<< tables_to_lock[0].table_name << " ";
	*sql << (tables_to_lock[0].write?"WRITE":"READ");
	for (unsigned int i = 1; i < tables_to_lock.size(); i++){
		*sql << ", " << tables_to_lock[i].table_name << " ";
		*sql << (tables_to_lock[i].write?"WRITE":"READ");
	}
	sql->send_query();
	locked = true;
}
void ns_sql_full_table_lock::lock(const std::string & table_to_lock,const bool & write){

	ns_acquire_lock_for_scope lock(sql->get_lock(__FILE__, __LINE__));
	if(sql->query().size())
		throw ns_ex("ns_sql_full_table_lock::lock()::Query not cleared before attempting Lock");
	final_autocommit_state = sql->autocommit_state();
	sql->set_autocommit(false);
	lock.release();
	sql->send_query("BEGIN");
	*sql << "LOCK TABLES " << table_to_lock << " ";
	if (write) *sql << "WRITE";
	else *sql << "READ";
	sql->send_query();
	locked = true;
}

void ns_sql_full_table_lock::unlock(){
	sql->clear_query();
	sql->send_query("COMMIT");
	sql->send_query("UNLOCK TABLES");

	ns_acquire_lock_for_scope lock(sql->get_lock(__FILE__, __LINE__));
	sql->set_autocommit(final_autocommit_state);
	lock.release();
	locked = false;
}

 ns_lock unreachable_host_lock("ns_unreachable_host_lock");
 void ns_sql_connection::simulate_unreachable_mysql_server(const std::string & host,bool make_unreachable){
	ns_acquire_lock_for_scope lock(unreachable_host_lock,__FILE__,__LINE__);
	if (!make_unreachable)
		unreachable_host.clear();
	else unreachable_host = host;
	lock.release();
}

void ns_sql_connection::simulate_errors_if_requested() const{
	if (!NS_INCLUDE_ERROR_SIMULATION)
		return;
	if (should_simulate_as_unreachable(_server_name))
		throw ns_ex("ns_sql_connection::Simulating a lost connection to ") << _server_name;
}

bool ns_sql_connection::should_simulate_as_unreachable(const std::string & host) const{
	
	ns_acquire_lock_for_scope lock(unreachable_host_lock,__FILE__,__LINE__);
	const bool r(unreachable_host.size() > 0 && host == unreachable_host);
	lock.release();
	return r;
}
std::string ns_sql_connection::unreachable_hostname(){
	ns_acquire_lock_for_scope lock(unreachable_host_lock,__FILE__,__LINE__);
	const std::string r(unreachable_host);
	lock.release();
	return r;
}

ns_lock ns_sql_connection::global_sql_lock("gssql");

std::string ns_sql_connection::unreachable_host = "";
