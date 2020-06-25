#ifndef NS_SQL_TABLE_LOCK_MANAGER
#define NS_SQL_TABLE_LOCK_MANAGER
#include "ns_lock.h"
#include "ns_sql.h"
#include <ostream>
#include <map>

struct ns_sql_table_lock_info {
	ns_sql_table_lock_info(const std::string & lock_p, const unsigned long &t, ns_sql_connection * sql,ns_64_bit proc_id) :lock_point(lock_p), time(t),sql_connection(sql),sql_process_id(proc_id){}
	std::string lock_point;
	std::string unlock_point;
	unsigned long time;
	ns_sql_connection * sql_connection;
	ns_64_bit sql_process_id;
};
class ns_sql_table_lock_manager;

struct ns_sql_table_lock {
	typedef enum { ns_scream_if_released, ns_scream_if_unreleased } ns_error_type;
	ns_sql_table_lock(const std::string & tab, const std::string &lock_p, const long t, ns_sql_table_lock_manager * man);
	ns_sql_table_lock() :time(0), manager(0) {}

	ns_sql_table_lock& operator=(ns_sql_table_lock&& c) {
		manager = c.manager;
		table = c.table;
		time = c.time;
		lock_point = c.lock_point;
		c.time = 0;
		c.manager = 0;
		c.lock_point.erase();
		c.table.erase();
		return *this;
	}
	//c++11 move constructor!
	ns_sql_table_lock(ns_sql_table_lock && c){
		manager = c.manager;
		table = c.table;
		time = c.time;
		lock_point = c.lock_point;
		c.time = 0; 
		c.manager = 0; 
		c.lock_point.erase();
		c.table.erase();
	}
	void release(const std::string & source, const unsigned long line, const ns_error_type & error_type= ns_scream_if_released);
	~ns_sql_table_lock();
private:
	std::string lock_point;
	std::string table;
	unsigned long time;
	ns_sql_table_lock_manager * manager;

};

class ns_image_server;
class ns_sql_table_lock_manager {
	friend struct ns_sql_table_lock;
	ns_lock lock_table_lock;
	typedef std::map<std::string, ns_sql_table_lock_info> ns_lock_table;
	ns_lock_table locks;
	void unlock_all_tables_no_lock(ns_sql_connection * sql, const std::string & source, const unsigned long line);
	ns_image_server * image_server;
public:
	ns_sql_table_lock_manager(ns_image_server * image_server);
	ns_sql_table_lock obtain_table_lock(const std::string & table, ns_sql_connection * sql, bool write, const std::string & source, const unsigned long line);

	ns_lock_table::iterator inspect_table_lock(const std::string & table, const std::string & source, const unsigned long & line);
	void output_current_locks(std::ostream & o);	
	void output_current_locks(std::string & o);

	void unlock_all_tables(ns_sql_connection * sql, const std::string & source, const unsigned long line);
	
};
#endif
