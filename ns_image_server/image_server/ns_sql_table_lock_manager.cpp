#include "ns_sql_table_lock_manager.h"
#include "ns_image_server.h"




void ns_sql_table_lock::release(const std::string & source, unsigned long line, const ns_error_type & error_type){
	manager->lock_table_lock.wait_to_acquire(source.c_str(), line);
	typename ns_sql_table_lock_manager::ns_lock_table::iterator p;
	try {
		p = manager->inspect_table_lock(table, source, line);
	}
	catch (...) {
		manager->lock_table_lock.release();
		throw;
	}
	if (p->second.lock_point != lock_point) {
		manager->lock_table_lock.release();
		throw ns_ex("An attempt to release the lock for table ") << table << " obtained at " << lock_point << " at " << time <<
			" was made at " << source << "::" << line << " but the existing lock was obtained by someone else at " << p->second.lock_point << " at " << p->second.time;
	}
	else {
		//release all tables associated with this connection;
		ns_sql_table_lock_info lock_info(p->second);
		if (lock_info.time != 0) {
			manager->unlock_all_tables_no_lock(lock_info.sql_connection, source, line);
		}
		manager->lock_table_lock.release();

		if (error_type == ns_scream_if_released && lock_info.time == 0)
			throw ns_ex("Attempting to unlock table ") << table << " acquired at " << lock_point << " but it was already unlocked at " << lock_info.unlock_point;
		if (error_type == ns_scream_if_unreleased && lock_info.time != 0)
			throw ns_ex("Table ") << table << " lock acquired at " << lock_point << " was never unlocked!";
	}
}
ns_sql_table_lock::ns_sql_table_lock(const std::string & tab, const std::string &lock_p, const long t, ns_sql_table_lock_manager * man) {
	table = tab;
	lock_point = lock_p;
	time = t;
	manager = man;
}
ns_sql_table_lock::~ns_sql_table_lock() {
	if (manager == 0)
		return;
	bool reported(false);
	try {
		release("table lock destructor", 0, ns_scream_if_unreleased);
	}
	catch (ns_ex & ex) {
		try {
			manager->image_server->register_server_event(ns_image_server::ns_register_in_central_db_with_fallback, ex);
			reported = true;
		}
		catch(...){}
		if (!reported)
			std::cerr << "Could not report this error to the central db: " << ex.text() << "\n";
	}
	catch (...) {
		std::cerr << "Could not release a lock for unknown reasons.\n";
	}
	
}


ns_sql_table_lock_manager::ns_sql_table_lock_manager(ns_image_server * im):lock_table_lock("ltl") {
	image_server = im;
}
ns_sql_table_lock ns_sql_table_lock_manager::obtain_table_lock(const std::string & table, ns_sql_connection * sql, bool write, const std::string & source, const unsigned long line) {
	*sql << "SELECT CONNECTION_ID()";
	const std::string connection_id_string(sql->get_value());

	*sql << "LOCK TABLES " << table << " ";
	if (write) *sql << "WRITE";
	else *sql << "READ";
	sql->send_query();
	lock_table_lock.wait_to_acquire(__FILE__, __LINE__);
	ns_lock_table::iterator p(locks.find(table));
	std::string lock_point(source + "::" + ns_to_string(line));
	const unsigned long current_time(ns_current_time());
	if (p == locks.end()) {
		std::pair<std::string, ns_sql_table_lock_info> pp(table, ns_sql_table_lock_info(lock_point, current_time, sql, ns_atoi64(connection_id_string.c_str())));
		locks.insert(pp);
	}
	else {
		if (p->second.time != 0) {
			lock_table_lock.release();
			throw ns_ex("Attempted to lock a table at line ") << lock_point
				<< " that was already locked at line " << p->second.lock_point << " at time " << ns_format_time_string_for_human(p->second.time);
		}
		else {
			p->second.lock_point = lock_point;
			p->second.time = current_time;
			p->second.sql_connection = sql;
			p->second.sql_process_id = ns_atoi64(connection_id_string.c_str());
			p->second.unlock_point.resize(0);
		}
	}
	lock_table_lock.release();
	ns_sql_table_lock l(table,lock_point, current_time, this);
	return l;
}

ns_sql_table_lock_manager::ns_lock_table::iterator ns_sql_table_lock_manager::inspect_table_lock(const std::string & table, const std::string & source, const unsigned long & line) {
	ns_lock_table::iterator p(locks.find(table));

	std::string lock_point(source + "::" + ns_to_string(line));
	if (p == locks.end())
		throw ns_ex("Attempting to release a lock for table ") << table << " that was never obtained, at " << lock_point;

	else {
		if (p->second.time == 0)
			throw ns_ex("Attempting to release a lock for table ") << table << " that was previously released at " << lock_point;
		return p;
	}
}
void ns_sql_table_lock_manager::unlock_all_tables(ns_sql_connection * sql, const std::string & source, const unsigned long line) {
	lock_table_lock.wait_to_acquire(source.c_str(), line);
	unlock_all_tables_no_lock(sql, source, line);
	lock_table_lock.release();
}
void ns_sql_table_lock_manager::unlock_all_tables_no_lock(ns_sql_connection * sql, const std::string & source, const unsigned long line) {
	*sql << "UNLOCK TABLES";
	sql->send_query();
	for (ns_lock_table::iterator p = locks.begin(); p != locks.end(); p++) {
		if (p->second.sql_connection == sql) {
			p->second.time = 0;
			p->second.sql_connection = 0;
			p->second.unlock_point = source + "::" + ns_to_string(line);
		}
	}
}