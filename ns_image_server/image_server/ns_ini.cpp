#include "ns_ini.h"
#include <stdlib.h>
#include <algorithm>


using namespace std;

const string & ns_ini::operator[](const string & field){
	return get_value(field);
}

const string & ns_ini::get_value(const string & field){
	map<string,ns_ini_entry>::iterator p = data.find(field);
	if (p == data.end() || !(p->second.loaded))
		throw ns_ex() << "The required variable \"" << field << "\" is not defined  the ini file. (" << filename << ")";
	else return p->second.value;
}
const bool ns_ini::field_specified(const std::string & field) const{
	const map<string,ns_ini_entry>::const_iterator p = data.find(field);
	if (p == data.end() || !(p->second.loaded))
		return false;
	return true;
}

void ns_ini::start_specification_group(const ns_ini_specification_group & group){
	specification_groups.push_back(group);
}
int ns_ini::get_integer_value(const string & field) {
	return atoi(get_value(field).c_str());
}

void ns_ini::add_field(const string & field, const string & default_value, const string & comment){
	data[field] = default_value;
	data[field].comment = comment;
	if (specification_groups.size() > 0)
		specification_groups.rbegin()->names_in_order.push_back(field);
}

inline void skip_to_end_of_line(istream & in){
	char a;
	while (true){
		in.get(a);
		//stop at end of line
		if (in.fail() || a == '\n')
			break;
		}
}
bool ns_ini::get_field(istream & in) {
	string field, value;
	in >> field;
	
	if (in.fail())
		return false;

	//remove leading whitespace
	while (field.size() > 0 && isspace(field[0]))
		field = field.substr(1,field.size()-1);

	//ignore comments
	if (field[0] == '#'){
		skip_to_end_of_line(in);
		return true;
	}
		

	in >> value; //this gets the "="
	if (in.fail() || value != "=")
		throw ns_ex("Invalid syntax found in the ini file: ") << value;

	//check to see if a valid field is specified
	map<string,ns_ini_entry>::iterator p = data.find(field);
	if (p == data.end()){
		if (reject_incorrect_fields_)
			throw ns_ex() << "Invalid variable found in the ini file:\"" << field << "\"";
		else{
			//ignore the error; discard the rest of the line
			cerr << "Discarding unknown variable in ini file: \"" << field << "\".";
			char a;
			while (true){
				in.get(a);
				//stop at end of line
				if (in.fail() || a == '\n')
					break;
			}
			return !in.fail();
		}
	}

	bool return_value = true;

	char a = '\0';
	p->second.value.clear();
	while (true){
		in.get(a);
		//stop at end of line
		if (in.fail() || a == '\n')
			break;
		//look for the first non-space character and ignore carriage returns.
		if ((p->second.value.size() == 0 && isspace(a)) || a == '\r')
			continue;
		p->second.value+=a;
	}
	if (in.fail())
		return_value = false;

	//remove comments
	std::string::size_type cp = p->second.value.find('#');
	if (cp != p->second.value.npos)
		p->second.value.resize(cp);

	//remove trailing whitespace
	if(p->second.value.size() > 0)
		while (isspace(p->second.value[p->second.value.size()-1]))
			p->second.value.resize(p->second.value.size()-1);
	p->second.loaded = true;
	

	return return_value;
}

void ns_ini::load(const string & fname){
	ns_ini::filename = fname;
	ifstream in(filename.c_str());
	//if the ini file doesn't exist, make it
	if (in.fail()){
		in.close();
		save(filename);
		in.open(filename.c_str());
		if (in.fail()){
			in.close();
			throw ns_ex() << "Could not read from the ini file " << filename << " even after writing to it.";
		}
		throw ns_ex("Could not find the ") << fname << " configuration file.  A blank template has been created to fill in.";
	}
	try{
		while (get_field(in));
		in.close();
	}
	catch(...){
		in.close();
		throw;
	}
}
void output_comment_wrapped(const std::string & txt,std::ostream & o,const std::string & sep = "# ",const int width=75){
	long cur_pos = 0;
	//go through the comment
	while (cur_pos < txt.size()){
		//if the comment will end on the next line, write it out.
		if (cur_pos+width >= txt.size()){
			o << sep;
			for (long i = cur_pos; i < txt.size(); i++)
				o << txt[i];
			o << "\n";
			break;
		}
		//if the comment is longer than the next line,
		//find the first whitespace from the end to insert the break.
		for (long i = cur_pos+width; i > cur_pos; i--){
			if (isspace(txt[i])){
				o << sep << txt.substr(cur_pos,i-cur_pos) << "\n";
				cur_pos = i+1;
				break;
			}
		}
	}
}

void ns_ini::save(const string & file_name){
	ofstream out(file_name.c_str());
	if (out.fail()){
		out.close();
		throw ns_ex() << "Could not save to the ini file " << file_name;
	}
	if (specification_groups.size() == 0){
		for(map<string,ns_ini_entry>::iterator p = data.begin();  p != data.end(); p++){
			if (!p->second.comment.empty())
				out << "# " << p->second.comment << "\n";
			out << p->first << " = " << p->second.value << "\n";
		}
	}
	else{
		for (unsigned int i = 0; i < specification_groups.size(); i++){
			const ns_ini_specification_group & s(specification_groups[i]);
			if (!s.group_name.empty())
				out << "#### " << s.group_name << " ####\n";
			if (!s.description.empty())
				output_comment_wrapped(s.description,out);
			if (!s.group_name.empty() || !s.description.empty())
				out << "\n";
			
			for (unsigned int j = 0; j < s.names_in_order.size(); j++){
				map<string,ns_ini_entry>::iterator p(data.find(s.names_in_order[j]));
				if (p == data.end())
					throw ns_ex("Could not find ") << s.names_in_order[j] << " in names in order list!";
				output_comment_wrapped( p->first + ": " + p->second.comment,out);
				out << "\n" << p->first << " = " << p->second.value << "\n\n";
			}
			out << "\n";
		}
	}

	out.close();
}
