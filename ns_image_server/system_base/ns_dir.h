#ifndef _NES_DIR_
#define _NES_DIR_

#include "ns_ex.h"
#include <iostream>
#include <vector>
#include <string>

//set up directory separators
#ifdef _WIN32 

	//#include <winsock2.h>
	//#include <windows.h>
	#define DIR_CHAR '\\'
	#define DIR_CHAR_STR "\\"
	#define WRONG_DIR_CHAR '/'
	#define WRONG_DIR_CHAR_STR "/"
	#include <experimental/filesystem>
	#include <filesystem>
	namespace ns_fs = std::experimental::filesystem::v1;
#else
	#include <sys/types.h>
	#include <dirent.h>
	#include <sys/stat.h>
	#include <errno.h>

	#define DIR_CHAR '/'
	#define DIR_CHAR_STR "/"
	#define WRONG_DIR_CHAR '\\'
	#define WRONG_DIR_CHAR_STR "\\"
	#include <filesystem>
	namespace ns_fs = std::filesystem;
#endif

const std::string ALL_FILES("\n\n\n\n\n\t");
const std::string DIRECTORIES("\n\n\n\n\t\t");



//----This class should be used to generate a listing of the contents of a win32 directory----------------//
class ns_dir{
    public:

	//the constructor, which creates the dir_info object for the specified directory
	ns_dir(const std::string & dir){load(dir);}
	ns_dir(){};

	//Loads the directory info for the specified directory
	void load(const std::string & dir);

	//Loads the directory info and fills the std::vector with all the files with the mask std::string
	void load_masked(const std::string & dir, const std::string & mask, std::vector<std::string> & m_files);

	//the name of the directory
	std::string name;

	//Vectors holding the names of the files and subdirectories present in the folder
	std::vector<std::string> files;
	std::vector<std::string> dirs;
	std::vector<ns_64_bit> file_sizes;
	std::vector<char> dir_is_simlink;

	//format path specifies whether the path should be checked for invalid characters.
	//can be turned off for efficiency's sake
	static void create_directory(const std::string &,const bool format_path = true);
	static void create_directory_recursive(const std::string &);
	static std::string extract_filename(const std::string &);
	static std::string extract_path(const std::string &);
	static std::string extract_extension(const std::string & filename);
	static std::string extract_filename_without_extension(const std::string & filename);

	///returns free disk space in MB
	static unsigned long get_free_disk_space(const std::string & path);

	static double get_directory_size(const std::string & path, const std::string & du_path="du", const bool recurse=true);
	static double get_file_size(const std::string file);

	static const bool file_exists(const std::string & f);
	static const bool file_is_writeable(const std::string & f);
	static const bool rename_to_unique_filename(std::string & f);
	static void set_permissions(const std::string & path, ns_fs::perms permisions);
	static bool try_to_set_permissions(const std::string & path, ns_fs::perms permisions);
	static std::string format_path(const std::string & str);

	static void convert_to_unix_slashes(std::string & str);
	static void convert_slashes(std::string & str);

	static const bool delete_file(const std::string &);

	//delete_folder_recursive is really quite dangerous!
	static const bool delete_folder_recursive(const std::string &);

	static const bool move_file(const std::string & source, const std::string & destination);
	static const bool copy_file(const std::string & source, const std::string & destination);

    private:
	#ifdef _WIN32 
	//Decides whether specified entity is a file or a directory, and places it in the appropriate std::vector.
		void add_s(WIN32_FIND_DATA * a);
	#endif
};

//Returns v, a std::vector with all files containing the substd::std::string mask in directory base or its subdirectories.
//void recurse_dirs(const std::string & base, const std::string & mask, std::vector<std::string> & v);
//converts a std::string to lowercase.
std::string ns_tolower(const std::string & a);

#endif
