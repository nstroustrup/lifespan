
#include "ns_ex.h"
#include "ns_dir.h"
#include "ns_thread.h"
#include <fstream>

#ifdef _WIN32 
#include "shellapi.h"

#include <filesystem>
namespace ns_fs = std::filesystem;

#endif
using namespace std;
#ifndef _WIN32
	#if defined(__APPLE__) || defined(__FreeBSD__)
		#include <sys/statvfs.h>
	#else
		#include <sys/statfs.h>
		#include <stdlib.h>
                #include <unistd.h>

	#endif
#endif

//change all incorrect path specifiers to their correct ones.
string ns_dir::format_path(const string & str){
	string ret = str;
	if (ret.size() == 0)
		return ret;
	string::size_type i = 0;
	while (true){
		i = ret.find_first_of(WRONG_DIR_CHAR_STR,i);
		if (i == ret.npos)
			break;
		
		ret.replace(i,1,DIR_CHAR_STR);

		if (i == ret.size()-1)
			break;
		i++;
	}
	return ret;
}
#ifdef _WIN32 
wchar_t * str_to_wchar(const string & s){
	if (s.size() > 499)
		throw ns_ex("ns_dir: String too long to convert to a w_char!");
	wchar_t *t = new wchar_t[500];
	for (unsigned int i = 0; i < s.size(); i++)
		t[i] = s[i];
	t[s.size()]='\0';
	return t;
}

string wchar_to_str(wchar_t * c){
	string t;
	for (unsigned int i = 0; c[i]!='\0'; i++)
		t+=static_cast<char>(c[i]);
	return t;
}
#endif

const bool ns_dir::file_exists(const string & f){
	#ifdef _WIN32 
   char * ff = new char[f.size() + 1];
   for (unsigned int i = 0; i < f.size(); i++)
     ff[i] = f[i];
   //com._Copy_s(cmd,com.size(),com.size());
	ff[f.size()] = 0;
	bool ret = (GetFileAttributes(f.c_str()) != INVALID_FILE_ATTRIBUTES);
	delete[] ff;
	return ret;
	#else
	return ::access(f.c_str(),0) == 0;

	#endif
}

const bool ns_dir::copy_file(const string & source, const string & destination){
#ifdef _WIN32
	ifstream in(source.c_str(), std::ios::binary);
	ofstream out(destination.c_str(), std::ios::binary);
#else
	ifstream in(source.c_str());
	ofstream out(destination.c_str()); 
#endif
	if (in.fail() || out.fail())
		return false;
	out << in.rdbuf();
	return true;
}

const bool ns_dir::move_file(const string & source, const string & destination){
	if (source == destination)
		return true;
#ifdef _WIN32 
	return MoveFile(source.c_str(),destination.c_str()) != 0;
#else
	return rename(source.c_str(),destination.c_str()) == 0;
#endif	
}

const bool ns_dir::rename_to_unique_filename(string & f){

	if (!file_exists(f))
		return false;
	string base_name = extract_filename_without_extension(f),
		   extension = extract_extension(f);
	unsigned long i = 1;
	string new_filename,
		   suffix;
	while(true){
		suffix = ns_to_string(i);
		new_filename = base_name + "="+suffix + "." + extension;
		if (!file_exists(new_filename))
			break;
		i++;
	}
	f = new_filename;
	return true;

}

const bool ns_dir::delete_file(const string &str){
#ifdef _WIN32 
	return DeleteFile(str.c_str())!=0;
#else
	return ::remove(str.c_str()) == 0;
#endif
}

string to_lower(const string & a){
        string o;
        for (unsigned int i=0;i<a.size();i++)
                o+= tolower(a[i]);
        return o;
}

class ns_directory_size_calculator{
public:
	ns_64_bit run(const std::string & path,const bool recursive=true){
		ns_64_bit s(0);
		ns_dir dir;
		dir.load(path);
		dir.files.resize(0);
		for (unsigned int i = 0; i < dir.file_sizes.size(); i++)
			s += dir.file_sizes[i];
		if (recursive)
			for (unsigned int i = 0; i < dir.dirs.size(); i++){
				if (dir.dirs[i] == "." || dir.dirs[i] == ".." || dir.dir_is_simlink[i])
					continue;
				s+=run(path + DIR_CHAR_STR + dir.dirs[i],true);
			}
		return s;
	}
};
#ifdef _WIN32
LONGLONG FileTime_to_POSIX(FILETIME ft)
{
	//from https://www.frenk.com/2009/12/convert-filetime-to-unix-timestamp/
	// takes the last modified date
	LARGE_INTEGER date, adjust;
	date.HighPart = ft.dwHighDateTime;
	date.LowPart = ft.dwLowDateTime;

	// 100-nanoseconds = milliseconds * 10000
	adjust.QuadPart = 11644473600000 * 10000;

	// removes the diff between 1970 and 1601
	date.QuadPart -= adjust.QuadPart;

	// converts back from 100-nanoseconds to seconds
	return date.QuadPart / 10000000;
}
#endif
unsigned long ns_dir::get_file_timestamp(const std::string & filename) {
#ifdef _WIN32
	HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		throw ns_ex("ns_dir::get_file_timestamp()::File does not exist: ") << filename; 
	FILETIME CreationTime,
		LastAccessTime,
		LastWriteTime;

	if (!GetFileTime(hFile,&CreationTime,&LastAccessTime,&LastWriteTime)) {
		CloseHandle(hFile);
			throw ns_ex("ns_dir::get_file_timestamp()::Could not obtain file timestamp: ") << filename;
	}
	CloseHandle(hFile);
	LONGLONG c(FileTime_to_POSIX(CreationTime)), w(FileTime_to_POSIX(LastWriteTime));
	//return 0;
	return (unsigned long)(c > w ? c : w);

#else
	struct stat buffer;
	int rc = stat(filename.c_str(), &buffer);
	if (rc != 0)
		throw ns_ex("ns_dir::get_file_timestamp()::Could not obtain file timestamp: ") << filename;
	return buffer.st_mtime > buffer.st_ctime ? buffer.st_mtime : buffer.st_ctime;
#endif

}
ns_64_bit ns_dir::get_file_size(const std::string & filename) {
#ifdef _WIN32
	//from https://stackoverflow.com/questions/8991192/check-filesize-without-opening-file-in-c
	HANDLE hFile = CreateFile(filename.c_str(), GENERIC_READ,
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, OPEN_EXISTING,
		FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE)
		throw ns_ex("ns_dir::get_file_timestamp()::File does not exist: ") << filename;

	LARGE_INTEGER size;
	if (!GetFileSizeEx(hFile, &size))
	{
		CloseHandle(hFile);
		throw ns_ex("ns_dir::get_file_timestamp()::Could not obtain file size: ") << filename;
	}

	CloseHandle(hFile);
	return size.QuadPart;
#else
	struct stat stat_buf;
	int rc = stat(filename.c_str(), &stat_buf);
	if (rc != 0)
		throw ns_ex("ns_dir::get_file_timestamp()::Could not obtain file size: ") << filename;
	return stat_buf.st_size;
#endif
}

ns_64_bit ns_dir::get_directory_size(const std::string & path, const std::string & du_path, const bool recurse){
	if (!ns_dir::file_exists(path))
		return 0;
	ns_directory_size_calculator size_calculator;
	ns_64_bit val(size_calculator.run(path,recurse));
//	cerr << "Running " << du_path << " " << path << "\n";
/*	ns_external_execute exec;
	ns_external_execute_options opt;
	opt.take_stdout_handle = true;
//	exec.start_timeout(60*10);  //ten minutes
	#ifdef _WIN32 
		string arg("-q ");
		if (!recurse) 
			arg += "-n ";
		arg+= path;
		
		exec.run(du_path,arg,opt);  //the result in bytes
	#else	
		string arg("--apparent-size -s -B1024 ");
		if (!recurse) 
			arg+="-S ";
		exec.run(du_path,arg,opt);  //the result in bytes
	#endif

	exec.finished_reading_from_stderr();
	exec.finished_writing_to_stdin();
	char buf[100];
	string result;
	while(true){
		unsigned long bytes_read(exec.read_stdout(buf,99));
	//	cerr << "Read " << bytes_read << "...";
		if (bytes_read == 0)
			break;
		
		buf[bytes_read] = 0;
	//	cout << "[" << buf << "]";
		result+=buf;
	}
#ifdef _WIN32 
	const string::size_type err(result.find("No matching files were found")); 
	if (err != result.npos)
		return 0;

	const string::size_type end(result.rfind(" bytes")); 
	if (end == result.npos)
		throw ns_ex("ns_dir::get_directory_size()::Could not parse du output");
	const string::size_type beg(result.rfind("Size on disk: ",end));
	if(beg == result.npos)
		throw ns_ex("ns_dir::get_directory_size()::Could not parse du output");
	const string raw_size(result.substr(beg+14,end-(beg+14)));
	string proc_size;
	proc_size.reserve(raw_size.size());
	for (unsigned int i = 0; i < raw_size.size(); i++){
		if (raw_size[i]!=',')
			proc_size+=raw_size[i];
	}
	const __int64 val(_atoi64(proc_size.c_str()));
	if (val < (unsigned long)(-1))
		return ((double)(val))/1024.0/1024.0;
	return (double)(val/1024)/1024.0;
#else
	const string::size_type err(result.find("No such file")); 
	if (err != result.npos)
		return 0;

	string proc_size;
	proc_size.reserve(result.size());
	for (unsigned int i = 0; i < result.size(); i++){
		if (isspace(result[i]))
			break;
		proc_size+=result[i];
	}
	return ((double)atol(proc_size.c_str()))/1024.0;
#endif
	*/
	if (val < (unsigned long)(-1))
		return ((double)(val))/1024.0/1024.0;
	return (double)(val/1024)/1024.0;
}

void ns_dir::load(const string & dir){
	dirs.resize(0);
	files.resize(0);
	file_sizes.resize(0);
#ifdef _WIN32 
	string dirt = dir + DIR_CHAR_STR + "*";
	WIN32_FIND_DATA a;
	//wchar_t * wdir =  str_to_wchar(format_path(dir));
	HANDLE h = FindFirstFile(dirt.c_str(), &a); 
	if (h == INVALID_HANDLE_VALUE){
		//delete wdir;
		ns_ex ex("ns_dir::Could not load requested directory: ");
		ex << format_path(dir) << " (";
		ex.append_windows_error();
		ex << ")";
		throw ex;
	}
	add_s(&a);
	while (FindNextFile(h, &a)){
		add_s(&a);
	}
	FindClose(h);
//	delete wdir;
	return;
#else
	DIR * a = opendir(dir.c_str());
	if (a == 0)
		throw ns_ex("ns_dir::Could not load requested directory: ") << format_path(dir);
	dirent dir_entry;
	struct dirent * dir_result;
	struct stat f_stat;
	int error;
	string filename;
	while(true){
		error = readdir_r(a,&dir_entry,&dir_result);
		if (error != 0)
			throw ns_ex("ns_dir::An error occured while loading directory: ") << format_path(dir);
		if (dir_result == 0)
			break;
		filename = dir.c_str();
		filename += DIR_CHAR_STR;
		filename += dir_result->d_name;
		error = stat(filename.c_str(),&f_stat);
		if (error != 0)
			throw ns_ex("ns_dir::An error occured while getting file stats for : ") << filename;
		switch(f_stat.st_mode&S_IFMT){
			case S_IFDIR:
			  //	  cerr << dir_result->d_name << " is a dir\n";
				dirs.push_back(dir_result->d_name);
				dir_is_simlink.push_back(0);
				break;
			case S_IFLNK:
			  //cerr << dir_result->d_name << " is a link\n";
				dirs.push_back(dir_result->d_name);
				dir_is_simlink.push_back(1);
				break;
			default:
			  //	  cerr << dir_result->d_name << " is " << f_stat.st_mode << "\n";
				files.push_back(dir_result->d_name);
				file_sizes.push_back(f_stat.st_size);
		}
	}
	error = closedir(a);
	if (error != 0)
		throw ns_ex("ns_dir::An error occured while closing directory: ") << format_path(dir);
		
#endif
}
void ns_dir::load_masked(const string & dir, const string & mask, vector<string> & m_files){

	load(dir);
	vector<string> f;
	for (unsigned int i = 0; i < files.size(); i++){
		if (to_lower(files[i]).find(mask) != files[i].npos)
			f.push_back(files[i]);
	}
	m_files.resize(0);
	m_files.insert(m_files.begin(),f.begin(),f.end());

}
#ifdef _WIN32 
void ns_dir::add_s(WIN32_FIND_DATA * a){
	if ((a->dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) == FILE_ATTRIBUTE_DIRECTORY){
		dirs.push_back(a->cFileName);
		
		dir_is_simlink.push_back( a->dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT );
	}
	else{
		files.push_back(a->cFileName);
		file_sizes.push_back((((ns_64_bit)a->nFileSizeHigh)*(MAXDWORD+1)) + a->nFileSizeLow);
	}
	return;
}
#endif



const bool ns_dir::file_is_writeable(const string & f){
	ofstream outf(f.c_str());
	if (outf.fail())
		return false;
	outf.close();
	delete_file(f);
	return true;
}
#ifdef _WIN32 
inline unsigned long size_from_64_bit(const ULONGLONG & i){
	ULONGLONG j = i/(ULONGLONG)(1024*1024);
	unsigned long max = (unsigned long)((unsigned long)0 - 1);
	if (j > (ULONGLONG)max)
		return max;
	return (unsigned long)j;
}
#endif

unsigned long ns_dir::get_free_disk_space(const string & path){
	#ifdef _WIN32 
		ULARGE_INTEGER bytes_available,
					   total_number_of_bytes,
					   total_number_of_free_bytes;
		if (!GetDiskFreeSpaceEx(path.c_str(),&bytes_available,&total_number_of_bytes,&total_number_of_free_bytes))
			throw ns_ex("ns_dir::get_free_disk_space::Could not call GetDiskFreeSpaceEx");
		return size_from_64_bit(total_number_of_free_bytes.QuadPart);
	#else
		#if defined(__APPLE__) || defined(__FreeBSD__)
			struct statvfs fs;
			if (statvfs(path.c_str(),&fs) != 0)
				throw ns_ex("ns_dir::get_free_disk_space::Could not call fstatvfs()");
		#else
			struct statfs fs;
			if (statfs(path.c_str(),&fs) != 0)
				throw ns_ex("ns_dir::get_free_disk_space::Could not call fstatfs()");
		#endif

		unsigned long max = (unsigned long)((unsigned long)0 - 1);
		if (fs.f_bavail >= max/fs.f_bsize)
			return max;
		return (fs.f_bavail*fs.f_bsize)/(1024*1024);
	#endif
}


void ns_dir::create_directory(const string & path,const bool _format_path){
#ifdef _WIN32 
	wchar_t * t;
	if (_format_path)
		t= str_to_wchar(format_path(path));
	else 
		t= str_to_wchar(path);
	unsigned int b = CreateDirectoryW(t, 0);
	if (b == 0){
		
		delete[] t;
		DWORD err = GetLastError();
		if (err == ERROR_ALREADY_EXISTS){
			return;
		}
		else{
			ns_ex ex("ns_dir: Error Creating Directory \"");
			ex << path << "\": ";
			ex.append_windows_error();
			throw ex;
		}
	}
	delete[] t;
	return;
#else 
	//	cerr << "Making directory " << path.c_str() << "\n";
	int res = mkdir(path.c_str(),0777);
	if (res == -1 && errno != EEXIST)
		ns_ex ex("ns_dir: Error Creating Directory \"");
#endif
}

struct ns_split_string{
	string first,
		   second;
};

ns_split_string extract_base_directory(const string & a){
	ns_split_string ret;
	string::size_type i = a.find_first_of(DIR_CHAR_STR);
	if (i==a.npos){ //there is no directory specified.
		ret.first = ".";
		ret.second = a;
		return ret;
	}
	
	if (i == 0){  //image path starts at root
		string::size_type j;
		//identify remote paths like //mywindowsfileserver/
		const bool windows_remote_path = (a.size() >= 2 && a[1] == DIR_CHAR);

		if (windows_remote_path)
			j = a.find_first_of(DIR_CHAR_STR, 2);
		else j = a.find_first_of(DIR_CHAR_STR, 1);

	  if(j==a.npos){
	    ret.first = a;
	    ret.second = "";
	    return ret;
	  }
	  else i = j;
	}
	ret.first = a.substr(0,i);
	ret.second = a.substr(i+1,a.size()-i-1);
	return ret;
}

void ns_dir::create_directory_recursive(const string & path){
	if (path.empty())
		return;
  //cerr << "Making recursive: " << path << "\n";
	string cur_dir = format_path(path);
	if (cur_dir[cur_dir.size()-1] != DIR_CHAR)
		cur_dir+=DIR_CHAR;
	string base;
	while(true){
		ns_split_string s = extract_base_directory(cur_dir);
		//	cerr << s.first << " |  " << s.second << "\n";
		if (s.first == "." || s.first == "")
			break;
		//ignore drive names
		if (s.first.find(":") == s.first.npos)
			ns_dir::create_directory(base + s.first,false);
		cur_dir = s.second;
		base += s.first + DIR_CHAR_STR;
	}
}

void ns_dir::convert_to_unix_slashes(string & str){
	for (unsigned int i = 0; i < str.size(); i++)
		if (str[i] == '\\')
			str[i] = '/';
}
void ns_dir::convert_slashes(string & str){
  for (unsigned int i = 0; i < str.size(); i++)
    if (str[i] == WRONG_DIR_CHAR)
      str[i] = DIR_CHAR;
  
  //remove double slashes
  bool last_was_slash = false;
  for (string::iterator p = str.begin(); p != str.end();){
	  if (*p == DIR_CHAR && last_was_slash)
			p = str.erase(p);
	  else{
		last_was_slash = (*p == DIR_CHAR);
		p++;
	  }		
  }
}

string ns_tolower(const string & a){
        string o;
        for (unsigned int i=0;i<a.size();i++)
                o+= tolower(a[i]);
        return o;
}

string ns_dir::extract_filename(const string & str){
	string a = format_path(str);
	string::size_type i = a.find_last_of(DIR_CHAR_STR);
	if (i==a.npos)
		return a;
	return a.substr(i+1,a.size()-i-1);
}

string ns_dir::extract_path(const string & str){
	string a = format_path(str);
	string::size_type i =  a.find_last_of(DIR_CHAR_STR);
	if (i==a.npos)
		return "";
	return a.substr(0,i);
}

string ns_dir::extract_extension(const string & filename){
	string::size_type dot_pos = filename.find_last_of(".");
	if (dot_pos == filename.npos)
		return "";
	const std::string ex(filename.substr(dot_pos + 1));
	if (dot_pos > 1 && ex == "gz") {
		string::size_type dot_pos2 = filename.find_last_of(".", dot_pos - 1);
		if (dot_pos2 != filename.npos)
			return filename.substr(dot_pos2 + 1);
	}
	return ex;
}
string ns_dir::extract_filename_without_extension(const string & filename){
	string::size_type dot_pos= filename.find_last_of(".");
	if (dot_pos == filename.npos)
		return filename;
	const std::string ex(filename.substr(dot_pos + 1));
	if (dot_pos > 1 && ex == "gz") {
		string::size_type dot_pos2 = filename.find_last_of(".", dot_pos - 1);
		if (dot_pos2 != filename.npos)
			return filename.substr(0, dot_pos2);
	}
	return filename.substr(0,dot_pos);
}
/*
void recurse_dirs(const string & str, const string & mask, vector<string> & v){
	string base = ns_dir::format_path(str);
	ns_dir dir(base + DIR_CHAR_STR + "*.*");
	if (mask == ALL_FILES){
		for (unsigned int i = 0; i< dir.files.size(); i++)
				v.push_back(base+ DIR_CHAR +dir.files[i]);
		for (unsigned int i = 0; i < dir.dirs.size(); i++)
			if (dir.dirs[i]!= "." && dir.dirs[i] != ".."){
				recurse_dirs(base + DIR_CHAR + dir.dirs[i], mask, v);
		}
	}
	else if (mask == DIRECTORIES){
		for (unsigned int i = 0; i< dir.dirs.size(); i++){
			if (dir.dirs[i]!= "." && dir.dirs[i] != ".."){
				//debug_out << "found " << dir.dirs[i] << endl; 
				v.push_back(base+DIR_CHAR+dir.dirs[i]);
				recurse_dirs(base + DIR_CHAR + dir.dirs[i], mask, v);
			}
		}
	}
	else{
		for (unsigned int i = 0; i< dir.files.size(); i++)
			if (ns_tolower(dir.files[i]).find(mask)!= dir.files[i].npos)
				v.push_back(base+DIR_CHAR+dir.files[i]);
		for (unsigned int i = 0; i < dir.dirs.size(); i++)
			if (dir.dirs[i]!= "." && dir.dirs[i] != ".."){
				recurse_dirs(base + DIR_CHAR + dir.dirs[i], mask, v);
		}
	}
	return;
}
*/

const bool ns_dir::delete_folder_recursive(const string & d){
	if (d.size() < 5)
		throw ns_ex("delete_folder_recursive()::Failsafe triggered; decided not to recursively delete from ") << d;
	#ifdef _WIN32 
		char * foo = new char[d.size() + 2];
		for (unsigned int i = 0; i < d.size(); i++)
			foo[i] = d[i];
		foo[d.size()] = foo[d.size()+1] = 0;
	//cerr << "deleting " << d <<"\n";
		SHFILEOPSTRUCT shfo = {0};
		shfo.wFunc = FO_DELETE;
		shfo.pFrom = foo;
		shfo.fFlags = FOF_NOCONFIRMATION | FOF_SILENT | FOF_NOERRORUI | FOF_NOCONFIRMMKDIR;
		shfo.fAnyOperationsAborted = FALSE;
		int ret = SHFileOperation( &shfo );
		delete[] foo;
		return ret == 0;
	#else
		string r("rm -rf ");
		r += d;
		if (0 != system(r.c_str()))
			throw ns_ex("delete_folder_recursive()::Command failed: ") << r;
		return true;

	#endif
}

void ns_dir::set_permissions(const std::string & path, ns_output_file_permissions permision_setting) {
#ifdef _WIN32
if (permision_setting == ns_output_file_permissions::ns_group_read)
	ns_fs::permissions(path.c_str(), ns_fs::perms::owner_all | ns_fs::perms::group_read | ns_fs::perms::others_read);
else
	ns_fs::permissions(path.c_str(), ns_fs::perms::owner_all);
#else
	if (permision_setting == ns_output_file_permissions::ns_group_read)
		chmod(path.c_str(), S_IRWXU | S_IRGRP | S_IROTH);
	else
		chmod(path.c_str(), S_IRWXU);
#endif
}

bool ns_dir::try_to_set_permissions(const std::string & path, ns_output_file_permissions permision_setting) {
	try { 
		set_permissions(path, permision_setting);
	}
	catch (...) {
		return false;
	}
	return true;
}
