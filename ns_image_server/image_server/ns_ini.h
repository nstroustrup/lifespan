#ifndef NS_INI_H
#define NS_INI_H

#include "ns_ex.h"
#include <string>
#include <vector>
#include <map>
#include <iostream>
#include <fstream>

struct ns_ini_entry{
	ns_ini_entry(const std::string & val = "", const bool ld = false):value(val),loaded(ld){}
	std::string value;
	std::string comment;
	bool loaded;
};
struct ns_ini_specification_group{
	ns_ini_specification_group(const std::string & name, const std::string & description_):
		group_name(name),description(description_){}
	std::string group_name;
	std::string description;
	std::vector<std::string> names_in_order;
};

class ns_ini{
public:
	void add_field(const std::string & field, const std::string & default_value="", const std::string & comment="");
	void start_specification_group(const ns_ini_specification_group & group);
	void load(const std::string & fname);
	void save(const std::string & fname);

	const std::string & operator[](const std::string & field);
	const std::string & get_value(const std::string & field);
	int get_integer_value(const std::string & field);
	void reject_incorrect_fields(const bool & reject){reject_incorrect_fields_ = reject;}
	const bool field_specified(const std::string & field) const;
private:
	std::vector<ns_ini_specification_group> specification_groups;
	bool get_field(std::istream & in);
	bool reject_incorrect_fields_;
	std::map<std::string,ns_ini_entry> data;
	std::string filename;
};
#endif
