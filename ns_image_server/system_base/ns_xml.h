#ifndef NS_XML
#define NS_XML

#include "ns_ex.h"
#include <stdlib.h>
#include <map>

class ns_xml_simple_writer{
	std::string o;
	std::vector<std::string> groups;
	std::string eolc,tabc;
public:
	void generate_whitespace(const bool & b=true);
	void add_header();
	void start_group(const std::string & name="group");
	void end_group();
	void add_tag(const std::string & k, const std::string & v);
	void add_raw(const std::string & k);
	template<class T>
	static std::string format_pair(T&r,T&l){
		return std::string(ns_to_string(r) + "," + ns_to_string(l));
	}
	template<class T>
	void add_tag(const std::string & k, const T & v){
		add_tag(k,ns_to_string(v));
	}
	void add_footer();
	const std::string result();
private:
};
struct ns_xml_simple_object{

	void clear(){name.clear();value.clear();tags.clear();}
	std::string name,
				value;
	typedef std::map<std::string,std::string> ns_tag_list;
	ns_tag_list tags;
	bool tag_specified(const std::string & key) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		return (p != tags.end());
	}
	const std::string & tag(const std::string & key) const;

	bool assign_if_present(const std::string & key, std::string & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = p->second;
		return true;
	}
	bool assign_if_present(const std::string & key, long & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = atol(p->second.c_str());
		return true;
	}
	bool assign_if_present(const std::string & key, unsigned long & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = atol(p->second.c_str());
		return true;
	}
	bool assign_if_present(const std::string & key, ns_64_bit & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = ns_atoi64(p->second.c_str());
		return true;

	}
	bool assign_if_present(const std::string & key, double & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = atof(p->second.c_str());
		return true;
	}
};

class ns_xml_simple_object_reader{
public:
	//uses a fast, brittle, hand-coded parser
	void from_stream(std::istream & i);
	//uses a slow, standards-obeying parser
	void from_string(const std::string & s);
	std::vector<ns_xml_simple_object> objects;
};

#ifndef NS_NO_TINYXML

struct ns_xml_object{

	void clear(){name.clear();value.clear();tags.clear();}
	std::string name,
				value;
	typedef std::map<std::string,std::string> ns_tag_list;
	ns_tag_list tags;
	std::vector<ns_xml_object> children;
	bool tag_specified(const std::string & key) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		return (p != tags.end());
	}
	const std::string & tag(const std::string & key) const;
	bool assign_if_present(const std::string & key, std::string & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = p->second;
		return true;
	}
	bool assign_if_present(const std::string & key, long & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = atol(p->second.c_str());
		return true;
	}
	bool assign_if_present(const std::string & key, int & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = atoi(p->second.c_str());
		return true;
	}
	bool assign_if_present(const std::string & key, unsigned long & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = atol(p->second.c_str());
		return true;
	}
	bool assign_if_present(const std::string & key, double & destination) const{
		std::map<std::string,std::string>::const_iterator p = tags.find(key);
		if(p == tags.end()) return false;
		destination = atof(p->second.c_str());
		return true;
	}
	void to_string(std::string & o);
};
class ns_xml_object_reader{
public:
	//uses a slow, standards-obeying parser
	void from_string(const std::string & s);
	void from_filename(const std::string & filename);
	void to_string(std::string & s);
	ns_xml_object objects;
};
#endif
#endif
