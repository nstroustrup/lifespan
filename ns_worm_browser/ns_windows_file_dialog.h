#ifndef NS_WINDOWS_FILE_DIALOG
#define NS_WINDOWS_FILE_DIALOG
#include "ns_ex.h"
#include <string>
#include <vector>


struct dialog_file_type{
	dialog_file_type(){}
	dialog_file_type(const std::string & n, const std::string & e):name(n),extension(e){}
	std::string extension,
		   name;
};

bool set_filter_string(const std::vector<dialog_file_type> filters, char *& filter_text);

std::string open_file_dialog(const std::string & heading, const std::vector<dialog_file_type> filters);

void destroy_icons();

std::string save_file_dialog(const std::string & heading, const std::vector<dialog_file_type> filters,const std::string & default_extension, const std::string & default_filename="");

#endif