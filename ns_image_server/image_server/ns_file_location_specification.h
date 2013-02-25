#ifndef NS_FILE_LOCATION_SPECIFICATION
#define NS_FILE_LOCATION_SPECIFICATION
#include <string>
#include "ns_dir.h"


class ns_file_location_specification{
private:
	std::string relative_directory,
			volatile_directory,
			long_term_directory,
			partition,
			filename;

	inline std::string absolute_volatile_directory() const{
		return volatile_directory + DIR_CHAR_STR + par() + relative_directory;
	}
	inline std::string absolute_long_term_directory() const{
		return long_term_directory + DIR_CHAR_STR + par() + relative_directory;
	}

	inline std::string absolute_volatile_filename() const{
		return absolute_volatile_directory() + DIR_CHAR_STR + filename;
	}
	inline std::string absolute_long_term_filename() const{
		return absolute_long_term_directory() + DIR_CHAR_STR + filename;
	}

	std::string par() const{
		if (partition.size() == 0) return "";
		else return partition + DIR_CHAR_STR;
	}
	friend class ns_image_storage_handler;
};


class ns_region_info_lookup{
	public:
	static void get_region_info(const ns_64_bit region_id,ns_image_server_sql * sql, std::string & region_name, std::string & sample_name,ns_64_bit & sample_id, std::string & experiment_name, ns_64_bit & experiment_id);
	static void get_sample_info(const ns_64_bit sample_id,ns_image_server_sql * sql, std::string & sample_name, std::string & experiment_name, ns_64_bit & experiment_id);
	static void get_experiment_info(const ns_64_bit experiment_id,ns_image_server_sql * sql, std::string & experiment_name);
};


#endif
