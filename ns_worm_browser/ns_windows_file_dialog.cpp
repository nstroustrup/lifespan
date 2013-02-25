#include "ns_windows_file_dialog.h"

bool set_filter_string(const std::vector<dialog_file_type> filters, char *& filter_text){
	bool should_delete;
	//char * filter_text;
	unsigned long size = 0;
	if (filters.size() == 0){
		filter_text = "All\0*.*\0";
		size = 8;
	}
	else{
		for (unsigned int i = 0; i < filters.size(); i++){
			size += static_cast<unsigned long>(filters[i].name.size());
			size += static_cast<unsigned long>(filters[i].extension.size());
			size+= 4;
		}
		filter_text = new char[size+1];
		should_delete = true;
		unsigned int cur_pos=0;
		for (unsigned int i = 0; i < filters.size(); i++){
			for (unsigned int j = 0; j < filters[i].name.size(); j++)
				filter_text[cur_pos+j] = filters[i].name[j];

			cur_pos+=static_cast<unsigned long>(filters[i].name.size());
			filter_text[cur_pos] = '\0';
			filter_text[cur_pos+1] = '*';
			filter_text[cur_pos+2] = '.';
			cur_pos+=3;	

			for (unsigned int j = 0; j < filters[i].extension.size(); j++)
				filter_text[cur_pos+j] = filters[i].extension[j];
			cur_pos+=static_cast<unsigned long>(filters[i].extension.size());
			filter_text[cur_pos] = '\0';
			cur_pos++;
		}
		filter_text[cur_pos] = '\0';
	}
	return should_delete;
}
std::string open_file_dialog(const std::string & heading, const std::vector<dialog_file_type> filters){

	OPENFILENAME ofn;       // common dialog box structure
	char szFile[260];       // buffer for file name
	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = 0;
	ofn.lpstrFile = szFile;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the contents of szFile to initialize itself.
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);

	char * filter;
	bool should_delete = set_filter_string(filters, filter);
	ofn.lpstrFilter = filter;
	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST |OFN_HIDEREADONLY;

	// Display the Open dialog box. 
	if (GetOpenFileName(&ofn)==TRUE){
		if (should_delete)
			delete ofn.lpstrFilter;
		return ofn.lpstrFile;
	}
	if (should_delete)
		delete ofn.lpstrFilter;
	return "";
}

void destroy_icons(){}

std::string save_file_dialog(const std::string & heading, const std::vector<dialog_file_type> filters,const std::string & default_extension, const std::string & default_filename){

	OPENFILENAME ofn;       // common dialog box structure
	char szFile[260];       // buffer for file name
	// Initialize OPENFILENAME
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = 0;
	ofn.lpstrFile = szFile;
	// Set lpstrFile[0] to '\0' so that GetOpenFileName does not use the contents of szFile to initialize itself.
	strcpy(szFile,default_filename.c_str());

	ofn.nMaxFile = sizeof(szFile);

	char * filter;
	bool should_delete = set_filter_string(filters, filter);
	ofn.lpstrFilter = filter;

	ofn.nFilterIndex = 1;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
//	ofn.lpstrDefExt = default_extension.c_str();
	ofn.Flags = OFN_EXPLORER | OFN_PATHMUSTEXIST | OFN_OVERWRITEPROMPT |OFN_HIDEREADONLY;

	// Display the Open dialog box. 
	if (GetOpenFileName(&ofn)==TRUE){
		if (should_delete)
			delete ofn.lpstrFilter;
		return ofn.lpstrFile;
	}
	if (should_delete)
		delete ofn.lpstrFilter;
	return "";
}
