#ifndef NS_IMAGE_SERVER_SQL
#define NS_IMAGE_SERVER_SQL
#include "ns_sql.h"

class ns_image_server_sql: public ns_sql_connection{
public:
	ns_image_server_sql(const bool use_local_database):local_database(use_local_database){
		table_prefix_=local_database?"buffered_":"";
	}
	ns_image_server_sql(const bool use_local_database, const std::string & server_name, const std::string & user_id, const std::string & password, const unsigned int retry_count=10):
	local_database(use_local_database),ns_sql_connection(server_name,user_id,password,retry_count){
		table_prefix_=local_database?"buffered_":"";
	}
	const bool connected_to_local_database() const{return local_database;}
	const bool connected_to_central_database() const{return !local_database;}
	const std::string & table_prefix() const{return table_prefix_;}
private:
	bool local_database;
	std::string table_prefix_;

};
class ns_sql : public ns_image_server_sql{
public:
	ns_sql():ns_image_server_sql(false){}
	
	ns_sql(const std::string & server_name, const std::string & user_id, const std::string & password, const unsigned int retry_count=10):
		ns_image_server_sql(false,server_name,user_id,password,retry_count){}
};

class ns_local_buffer_connection : public ns_image_server_sql{
public:
	ns_local_buffer_connection():ns_image_server_sql(true){}
	
	ns_local_buffer_connection(const std::string & server_name, const std::string & user_id, const std::string & password, const unsigned int retry_count=10):
		ns_image_server_sql(true,server_name,user_id,password,retry_count){}
};

#endif
