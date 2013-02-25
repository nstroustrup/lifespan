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
	bool loaded;
};

class ns_ini{
public:
	void add_field(const std::string & field, const std::string & default_value="");
	void load(const std::string & fname);
	void save(const std::string & fname);

	const std::string & operator[](const std::string & field);
	const std::string & get_value(const std::string & field);
	int get_integer_value(const std::string & field);
	void reject_incorrect_fields(const bool & reject){reject_incorrect_fields_ = reject;}
private:
	bool get_field(std::istream & in);
	bool reject_incorrect_fields_;
	std::map<std::string,ns_ini_entry> data;
	std::string filename;
};
#endif
