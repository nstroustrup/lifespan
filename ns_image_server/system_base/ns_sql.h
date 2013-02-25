#ifndef NS_SQL
#define NS_SQL
#include "ns_ex.h"
#ifdef WIN32
//#include <winsock2.h>
#include <sys/locking.h>
#include <winsock2.h>
#include <fcntl.h>
#include <io.h>
#include <malloc.h>
namespace ns_mysql_header{
#include "config-win.h"
#include <mysql.h>
#else
#include <stdlib.h>
namespace ns_mysql_header{
#include "mysql.h"
#endif
}

#define NS_INCLUDE_ERROR_SIMULATION 1

#undef bool
#undef sleep
#include <string>
#include <vector>
#include <iostream>

typedef std::vector<std::string> ns_sql_result_row;
typedef std::vector< ns_sql_result_row > ns_sql_result;

class ns_query{
public:
	ns_query(){}
	template<class T>
	ns_query(const T & t){append(t);}
	template<class T>
	ns_query & operator<<(const T & t){append(t);return *this;}

	ns_query & operator=(const std::string & s){
		str = s;
		return *this;
	}
	ns_query & write_data(const char *, const unsigned int length);

	const std::string & to_str() const{return str;}

	void clear(){str.clear();}

private:
	template<class T>
	void append(const T & i){
		str+=ns_to_string(i);
	}/*
	void append_double(const double i){
		str+=ns_to_string(i);
	}
	void append_int(const long int l){
		str+=ns_to_string(l);
	}*/
	std::string str;
};


//This class encapsulates mySQL behavior
class ns_sql_connection{
 public:
	 ns_sql_connection():commit(true),_retry_count(0),mysql_internal_data_allocated(false){}
  ns_sql_connection(const std::string & server_name, const std::string & user_id, const std::string & password, const unsigned int retry_count):commit(true),mysql_internal_data_allocated(false)
    { connect(server_name, user_id, password, retry_count);}

  //connects to a remote SQL server.
  void connect(const std::string & server_name, const std::string & user_id, const std::string & password, const unsigned int retry_count);

  void check_connection();

  static void load_sql_library(){ns_mysql_header::mysql_library_init(0,NULL,NULL);}
  static void unload_sql_library(){ns_mysql_header::mysql_library_end();}
  static bool thread_safe();

  static void simulate_unreachable_mysql_server(const std::string & unreachable_host,bool make_unreachable);
  static std::string unreachable_hostname();
  //disconnects from the server
  void disconnect();
  //selects the databse to which queries should be sent
  void select_db(const std::string & db_name);

  //request data from the database
  void get_rows(const std::string & query, ns_sql_result & result);
  void get_rows(ns_sql_result & result);

  void get_single_row(const std::string & query, std::vector< std::string > & result);
  void get_single_row(std::vector<std::string> &res);

  std::string get_value(const std::string & query);
  std::string get_value();

  int get_integer_value(const std::string & query);
  int get_integer_value();

  unsigned long get_ulong_value(const std::string & query);
  unsigned long get_ulong_value();

  //This function sends a query but doesn't look for any returned data.
  void send_query(const std::string & query);
  void send_query();

  ns_64_bit send_query_get_id(const std::string & query);
  ns_64_bit send_query_get_id();

  template<class T>
  ns_sql_connection & operator<<(const T & s){current_query << s;return *this;}
  ns_sql_connection & write_data(const char *, const unsigned long length);

  std::string query() const{ return current_query.to_str();}

  std::string escape_string(const std::string & str);

  void set_autocommit(const bool & commit_);
  bool autocommit_state() const {return commit;}

  void clear_query(){current_query.clear();}

  ~ns_sql_connection();

 private:
  static std::string unreachable_host;
  void simulate_errors_if_requested() const;
  bool should_simulate_as_unreachable(const std::string & host) const;
  
   ns_query current_query;
   bool commit;

   std::string _server_name,
			_user_id,
			_password;
	unsigned int _retry_count;

	std::string latest_error();
   ns_mysql_header::MYSQL mysql;
   bool mysql_internal_data_allocated;
};

struct ns_table_to_lock{
	ns_table_to_lock(){}
	ns_table_to_lock(const std::string & n,const bool w):table_name(n),write(w){}
	std::string table_name;
	bool write;
};
class ns_sql_full_table_lock{
public:
	typedef std::vector<ns_table_to_lock> ns_table_list;
	ns_sql_full_table_lock(ns_sql_connection & sql_):sql(&sql_),locked(false),final_autocommit_state(true){}
	ns_sql_full_table_lock(ns_sql_connection & sql_,const std::string & table_to_lock,const bool & write=true):sql(&sql_),locked(false),final_autocommit_state(true)
		{lock(table_to_lock,write);}

	ns_sql_full_table_lock(ns_sql_connection & sql_,const ns_table_list & tables_to_lock):sql(&sql_),locked(false),final_autocommit_state(true)
		{lock(tables_to_lock);}

	void lock(const std::string & table_to_lock,const bool & write=true);
	void lock(const ns_table_list & tables_to_lock);

	void unlock();
	~ns_sql_full_table_lock(){if (locked) unlock();}
private:
	bool locked;
	bool final_autocommit_state;
	ns_sql_connection * sql;
};



#endif
