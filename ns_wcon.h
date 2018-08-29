#pragma once

//////////////////////////////////////////////////////////////////////////
//
// ns_wcon.h
//
// The class ns_worm_tracker_commons_object implements an extendable parser/writer for the
// Worm tracker Commons Object Notation schema
// wcon_schema.json
// as specified in August 2018.
// https://github.com/openworm/tracker-commons
//
// Implemented by Nicholas Stroustrup
// Centre for Genomic Regulation
// 2018
// https://github.com/nstroustrup
// http://lifespanmachine.crg.eu
//
//
// Only a single header include is required.  There are no object files.
// ns_wcon is based on the JSON for Modern C++ library by Niels Lohmann ( https://github.com/nlohmann/json )
//
// Worm data is specified by a template argument timepoint_data_t
// By default, the parser/writer uses the minimal specification described in the WCON schema
//
// The extendability of this parser/writer is demonstrated in ns_wcon_rich_data_record
// which implements the full schema.
//
// Tested using gcc 4.8.x and Microsoft Visual Studio 2017
//
// Known limitations:
// 1.  Support for user defined variables is uneven
// 2.  No support for units defined in terms of algebric combinations of other units
// 3.  Specifications for unit outside of the standard single "units" object are ignored
// 4.  Unrecognized fields trigger output to cerr, which may be unattended
// 5.  No integrated DEFLATE support--library users must zip files themselves.
//
//////////////////////////////////////////////////////////////////////////


#include <string>
#include <vector>
#include <map>
#include <sstream>
#include <iostream>
#include <algorithm>
//The json library does not officially support gcc 4.8.x
//but in this application there seems to be no problems.
//
#define JSON_SKIP_UNSUPPORTED_COMPILER_CHECK 0
#include <nlohmann/json.hpp>

#undef NS_wcon_VERBOSE
//#define NS_wcon_VERBOSE

//convert between various types
class ns_quick_type_conversion {
public:
	static void set(std::string & d, const std::string & s) { d = s; }
	static void set(std::string & d, const double & s) { std::stringstream ss;  ss << s; d = ss.str(); }
	static void set(std::string & d, const uint32_t & s) { std::stringstream ss;  ss << s; d = ss.str(); }
	static void set(std::string & d, const int32_t & s) { std::stringstream ss;  ss << s; d = ss.str(); }
	static void set(std::string & d, const uint64_t & s) { std::stringstream ss;  ss << s; d = ss.str(); }
	static void set(std::string & d, const int64_t & s) { std::stringstream ss;  ss << s; d = ss.str(); }

	static void set(double & d, const double & s) { d = s; }
	static void set(double & d, const uint32_t & s) { d = s; }
	static void set(double & d, const int32_t & s) { d = s; }
	static void set(double & d, const uint64_t & s) { d = static_cast<double>(s); }
	static void set(double & d, const int64_t & s) { d = static_cast<double>(s); }
	static void set(double & d, const std::string & s) { d = atof(s.c_str()); }
	static void set(uint32_t & d, const uint32_t & s) { d = s; }
	static void set(uint32_t & d, uint32_t & s) { d = s; }
	static void set(uint32_t & d, const double & s) { d = static_cast<uint32_t>(s); }
	static void set(uint32_t & d, const uint64_t & s) { d = static_cast<uint32_t>(s); }
	static void set(uint32_t & d, const int64_t & s) { d = static_cast<uint32_t>(s); }
	static void set(uint32_t & d, const std::string & s) { d = atol(s.c_str()); }
	static void set(int32_t & d, const int32_t & s) { d = s; }
	static void set(int32_t & d, const uint32_t & s) { d = s; }
	static void set(int32_t & d, const double & s) { d = static_cast<uint32_t>(s); }
	static void set(int32_t & d, const uint64_t & s) { d = static_cast<uint32_t>(s); }
	static void set(int32_t & d, const int64_t & s) { d = static_cast<uint32_t>(s); }
	static void set(int32_t & d, const std::string & s) { d = atol(s.c_str()); }

	static void set(std::vector<double> & d, const std::vector<double> & s) { d = s; }
	static void set(std::vector<double> & d, const std::vector<std::string> & s) {
		d.resize(s.size());
		for (std::size_t i= 0; i < s.size(); i++)
			d[i] = atof(s[i].c_str());
	}
	static void set(std::vector<double> & d, const double & s) { d.resize(0); d.push_back(s); }
	static void set(std::vector<double> & d, const uint32_t & s) { d.resize(0); d.push_back(s); }
	static void set(std::vector<double> & d, const int64_t & s) { d.resize(0); d.push_back(static_cast<double>(s)); }
	static void set(std::vector<double> & d, const uint64_t & s) { d.resize(0); d.push_back(static_cast<double>(s)); }
	static void set(std::vector<double> & d, const std::string & s) { d.resize(0); d.push_back(atof(s.c_str())); }

	static bool compare_double(const double & a, const double & b, const double epsillon = .0000001) {
		#ifdef NS_wcon_VERBOSE
		if (fabs(a - b) >= epsillon)
			std::cerr << "Difference detected\n";
		#endif
		return fabs(a - b) < epsillon;
	}
	static bool compare_double_arrays(const std::vector<double> & a, const std::vector<double> & b, const double epsillon=.0000001) {
		if (a.size() != b.size())
			return false;
		for (std::size_t i = 0; i < a.size(); i++)
			if (!compare_double(a[i],b[i]))
				return false;
		return true;
	}
};

//corresponds to similarly named object in WCON schema
struct ns_wcon_arena {
	ns_wcon_arena():style("petri"),size(2,-1){}
	std::string style, orientation;
	std::vector<double> size;

	bool specified() const { return size.size() >= 2 && size[0] != -1 && size[1] != -1; }
	bool operator ==(const ns_wcon_arena & r) const {
		return style == r.style &&
			r.orientation == orientation &&
			ns_quick_type_conversion::compare_double_arrays(size, r.size);
	}
	nlohmann::json to_json() const;
};

//corresponds to similarly named object in WCON schema
struct ns_wcon_interpolation {
	std::string method;
	std::vector<std::string > values;

	bool specified() const { return !method.empty(); }
	bool operator==(const ns_wcon_interpolation & i) const {return method == i.method && values == i.values;}
	nlohmann::json to_json() const;
};

//corresponds to similarly named object in WCON schema
struct ns_wcon_tracker {
	std::string name, version;

	bool specified() const { return !name.empty(); }
	bool operator==(const ns_wcon_tracker & i) const { return name == i.name && version == i.version; }
	nlohmann::json to_json() const;
};

//corresponds to similarly named object in WCON schema
struct ns_wcon_software_metadata {
	ns_wcon_tracker tracker;
	std::string feature_id;

	bool specified() const { return tracker.specified(); }
	bool operator==(const ns_wcon_software_metadata & r) const { return tracker  == r.tracker && feature_id == r.feature_id; }
	nlohmann::json to_json() const;
};
struct ns_wcon_pattern_properties{
	std::string title, description;
	bool operator==(const ns_wcon_pattern_properties & r) const { return title == r.title && description == r.description; }
	nlohmann::json to_json() const;
};

//corresponds to similarly named object in WCON schema
struct ns_wcon_metadata {
	ns_wcon_metadata() :temperature(-1), humidity(-1),stage(ns_stage_na),sex(ns_sex_na),age(-1){}
	typedef enum {ns_stage_na,L1,L2,L3,L4,adult, dauer} ns_stage;
	typedef enum {ns_sex_na,hermaphrodite, male } ns_sex;
	static std::string to_string(const ns_stage & stage);
	static ns_stage stage_from_string(std::string & stage);
	static std::string to_string(const ns_sex & sex);
	static ns_sex sex_from_string(std::string & sex);

	std::string id;
	std::map<std::string,std::string> lab;
	std::vector<std::string> who;
	std::vector<std::string> protocol;
	std::string timestamp;
	double temperature, humidity;
	ns_wcon_arena arena;
	ns_stage stage;
	ns_sex sex;
	std::string food,
		media,
		strain;
	double age;
	std::vector< ns_wcon_interpolation> interpolate;
	std::vector<ns_wcon_software_metadata> software;
	std::map<std::string, std::string> additional_properties;

	bool operator==(const ns_wcon_metadata & r) const;
	bool is_specified() const;
	nlohmann::json to_json() const;
};

//corresponds to similarly named object in WCON schema
struct ns_wcon_units {
	typedef enum {
		ns_wcon_nanoseconds, ns_wcon_microseconds,ns_wcon_milliseconds, ns_wcon_seconds,
		ns_wcon_minutes, ns_wcon_hours, ns_wcon_days, ns_wcon_years,
		ns_wcon_nm, ns_wcon_um,ns_wcon_mm, ns_wcon_cm, ns_wcon_m, ns_wcon_km,
		ns_wcon_inches, ns_wcon_feet, ns_wcon_yards, ns_wcon_miles,
		ns_wcon_mm_per_second, ns_wcon_m_per_second, ns_wcon_inverse_meters,ns_wcon_inverse_millimeters,
		ns_wcon_degrees_F,ns_wcon_degrees_C, ns_wcon_degrees_K,
		ns_radians,ns_degrees,ns_percent,ns_unitless
	} ns_units;

	ns_wcon_units() :ox(ns_unitless), oy(ns_unitless) {}
	ns_units time;
	ns_units x, y, ox, oy;
	std::map<std::string, ns_units> additional_units;
	std::string additional_properties;


	bool operator==(const ns_wcon_units & r) const;

	static std::string to_string(const ns_units & t);
	static bool matches_singular(const std::string & subject, const char * test);
	static ns_units from_string(const std::string & str);
	nlohmann::json to_json() const;
};

//corresponds to similarly named object in WCON schema
struct ns_wcon_file_list_entry {
	std::vector<std::string> prev, next;
	std::string current;
	bool operator==(const ns_wcon_file_list_entry & r) const { return prev == r.prev && current == r.current &&  next == r.next; }
	bool is_specified() const;
	nlohmann::json to_json() const;
};

//supported data types that can be stored in data fields of a WCON record element
typedef enum { ns_unknown,ns_double, ns_string, ns_vector_double, ns_vector_string, ns_subclass} ns_wcon_data_element_member_type;

//implements the data handled by a minimal WCON writer/parser
class ns_wcon_default_data_record_element {
public:
	ns_wcon_default_data_record_element() :ox(0), oy(0) {}
	//required elements
	std::string id;
	double t, x, y, ox, oy;
	bool using_origin_coordinates;

	bool operator==(const ns_wcon_default_data_record_element & a) const;


	//fields required by extendible data type implementation
	nlohmann::json to_json() const;

	nlohmann::json add_subclass(int i) const {throw std::runtime_error("no subclasses!"); return nlohmann::json();}
	const double & get_t() const { return t; }
	const double & get_x() const { return x; }
	const double & get_y() const { return y; }
	const double & get_ox() const { return ox; }
	const double & get_oy() const { return oy; }
	const std::string & get_id() const { return id; }

	static int number_of_additional_json_fields() { return 0; }
	static std::string additional_json_field_name(const int &i) {return "";}
	static ns_wcon_data_element_member_type additional_json_field_type(const int &i) {return ns_unknown;}

	std::string get_additional_json_field_value_string(const int &i) const { return ""; }
	const double get_additional_json_field_value_double(const int &i) const { return 0; }
	const std::vector<double> * get_additional_json_field_value_vector_double(const int &i) const { return 0; }
	const std::vector<std::string> *get_additional_json_field_value_vector_string(const int &i) const { return 0; }
	nlohmann::json get_additional_json_field_value_subclass(const int &i) const {
		throw std::runtime_error("no subclasses!"); return nlohmann::json();
	}

	static bool json_field_is_a_known_subclass(const std::string & key) { return false; }

	template<class T>
	bool set_value(const std::string & key, const T & val,const std::string & subclass = "") {
		std::cerr << "Encountered unexpected data variable " << key << "\n";;
		return true;
	}
};




//storage class to hold WCON data records.  Can be written to JSON
//either split by timepoints (an array of objects, one for each timepoint, each object containing multiple variables)
//or split by variables (an object containing arrays, one array containing all measurements of each variable)
template<class timepoint_data_t>
class ns_wcon_data_record {
public:
	std::vector<timepoint_data_t> data;

	void sort_by_time_and_id();
	bool operator ==(const ns_wcon_data_record<timepoint_data_t> & d) const {
		if (data.size() != d.data.size())
			return false;
		for (std::size_t i = 0; i < data.size(); i++) {
			if (!(data[i] == d.data[i]))
				return false;
		}
		return true;
	}

	nlohmann::json to_json(bool split_timepoints = true) const;
private:
	nlohmann::json to_json_split_timepoints() const;
	nlohmann::json to_json_split_measurements() const;
};

// The class ns_worm_tracker_commons_object implements an extendable parser/writer for the
// Worm tracker Commons Object Notation schema
// wcon_schema.json
template<class timepoint_data_t = ns_wcon_default_data_record_element>
class ns_worm_tracker_commons_object {
public:
	ns_worm_tracker_commons_object() :write_split_timepoint(true) {}
	ns_wcon_file_list_entry files;
	ns_wcon_units units;
	ns_wcon_metadata metadata;
	std::string comment;

	//the structured timeseries of data points
	ns_wcon_data_record<timepoint_data_t> data;

	//any key/value pairs in the base commons object that were not recognized
	std::map<std::string, std::vector<std::string> > additional_unrecognized_fields;

	nlohmann::json to_json(bool split_timepoints = true) const;
	void from_stream(std::istream & in);
	void to_stream(std::ostream & out) const;

	bool write_split_timepoint;

	bool operator ==(const ns_worm_tracker_commons_object<timepoint_data_t> & w) const{
		#ifdef NS_wcon_VERBOSE
				if (!(files == w.files))
					std::cerr << "Difference detected in files\n";
				if (!(units == w.units))
					std::cerr << "Difference detected in units\n";
				if (!(metadata == w.metadata))
					std::cerr << "Difference detected in metadata\n";
				if (!(data == w.data))
					std::cerr << "Difference detected in data\n";
		#endif

		return files == w.files &&
			units == w.units &&
			metadata == w.metadata &&
			data == w.data;
	}

	friend std::ostream& operator<<(std::ostream& o, const ns_worm_tracker_commons_object<timepoint_data_t> & j) {
		j.to_stream(o);
		return o;
	}
	friend std::istream& operator>>(std::istream& i, ns_worm_tracker_commons_object<timepoint_data_t> & j) {
		j.from_stream(i);
		return i;
	}
};

// The class ns_wcon_sax_parser implements a SAX-style JSON parser to efficiently
// load large WCON objects using minimal memory
// This SAX parser is passed to the json library which handles all the lexer / tokenizer work
template<class timepoint_data_t = ns_wcon_default_data_record_element>
class ns_wcon_sax_parser : public nlohmann::json_sax<nlohmann::json> {
public:
	typedef nlohmann::detail::exception exception;

	ns_wcon_sax_parser(ns_worm_tracker_commons_object<timepoint_data_t> & data_storage_) :
		state(ns_reading_base_class), object_depth(0),array_depth(0),ignore_next_value(false), next_unsigned_integer(0), next_integer(0), next_string_vector(0), next_double_vector(0), next_enum(ns_no_enum), data(&data_storage_), next_string(0), next_double(0) {}


	bool key(string_t& val) {
		#ifdef NS_wcon_VERBOSE
			if (next_double != 0)
				std::cerr << "nzD:";
			if (next_unsigned_integer != 0)
				std::cerr << "nzI:";
			if (next_string != 0)
				std::cerr << "nzs:";
			if (next_string_vector != 0)
				std::cerr << "nzsv:";
			if (next_double_vector != 0)
				std::cerr << "nzdv:";
			std::cout << "level " << object_depth << " key:" << val << "\n";
		#endif
		if (object_depth == 2)
			substate = ns_none;
		if (object_depth == 1) {
			if (val == "units")
				state = ns_reading_units;
			else if (val == "data") {
				state = ns_reading_data;
				next_data_index = 0;
				current_data_element_subclass.resize(0);
			}
			else if (val == "files")
				state = ns_reading_files;
			else if (val == "metadata")
				state = ns_reading_metadata;
			else  if (val == "comment") {
				state = ns_reading_comment;
				next_string = &data->comment;
			}
			else {
				state = ns_reading_unrecognized;
				std::cerr << "unrecognized key: " << val << "\n";
				next_string_vector = &(data->additional_unrecognized_fields[val]);
			}
			return true;
		}

		switch (state) {
		case ns_reading_base_class: throw std::runtime_error("Entered unexpected state");
		case ns_reading_files: {
			if (val == "prev")
				next_string_vector = &data->files.prev;
			else if (val == "next")
				next_string_vector = &data->files.next;
			else if (val == "current")
				next_string = &data->files.current;
			else std::cerr << "unrecognized key: " << val << "\n";
		}
		case ns_reading_units: {
			if (val == "t")
				next_enum = ns_unit_t;
			else if (val == "x")
				next_enum = ns_unit_x;
			else if (val == "y")
				next_enum = ns_unit_y;
			else if (val == "ox")
				next_enum = ns_unit_ox;
			else if (val == "oy")
				next_enum = ns_unit_oy;
			else if (val == "additionalProperties")
				next_string = &data->units.additional_properties;
			else {
				next_enum = ns_unit_n;
				next_unit = val;
			}
			break;
		}
		case ns_reading_metadata: {
			switch (substate) {
			case ns_none: {
				if (val == "id")
					next_string = &data->metadata.id;
				else if (val == "lab")
					substate = ns_metadata_lab;
				else if (val == "who")
					next_string_vector = &data->metadata.who;
				else if (val == "timestamp")
					next_string = &data->metadata.timestamp;
				else if (val == "temperature")
					next_double = &data->metadata.temperature;
				else if (val == "humidity")
					next_double = &data->metadata.humidity;
				else if (val == "arena")
					substate = ns_metadata_arena;
				else if (val == "food")
					next_string = &data->metadata.food;
				else if (val == "media")
					next_string = &data->metadata.media;
				else if (val == "sex")
					next_enum = ns_sex;
				else if (val == "stage")
					next_enum = ns_stage;
				else if (val == "age")
					next_double = &data->metadata.age;
				else if (val == "strain")
					next_string = &data->metadata.strain;
				else if (val == "protocol")
					next_string_vector = &data->metadata.protocol;
				else if (val == "interpolate")
					substate = ns_metadata_interpolate;
				else if (val == "software")
					substate = ns_metadata_software;
				else {
					std::cerr << "Unrecognized metadata: " << val << "\n";
					next_string = &(data->metadata.additional_properties[val]);
				}
				break;
			}
			case ns_metadata_arena: {
				if (val == "style")
					next_string = &data->metadata.arena.style;
				else if (val == "size") {
					data->metadata.arena.size.resize(0);
					next_double_vector = &data->metadata.arena.size;
				}
				else if (val == "orientation")
					next_string = &data->metadata.arena.orientation;
				else {
					std::cerr << "Unrecognized arena property : " << val << "\n";
					ignore_next_value = true;
				}
				break;
			}
			case ns_metadata_lab: {
				next_string = &data->metadata.lab[val];
				break;
			}
			case ns_metadata_interpolate: {
				if (data->metadata.interpolate.empty())
					throw std::runtime_error("Adding to empty interpolate object");
				if (val == "method")
					next_string = &data->metadata.interpolate.rbegin()->method;
				else if (val == "values")
					next_string_vector = &data->metadata.interpolate.rbegin()->values;
				else {
					std::cerr << "Unrecognized interpolate property : " << val << "\n";
					ignore_next_value = true;
				}
				break;
			}
			case ns_metadata_software: {
				if (data->metadata.software.empty())
					throw std::runtime_error("Adding to empty software object");
				if (object_depth == 3 || object_depth == 4) {
					if (val == "name")
						next_string = &data->metadata.software.rbegin()->tracker.name;
					else if (val == "version")
						next_string = &data->metadata.software.rbegin()->tracker.version;
					else if (val == "featureID")
						next_string = &data->metadata.software.rbegin()->feature_id;
					else if (val == "tracker"); //do nothing
					else {
						std::cerr << "Unrecognized software metadata property: " << val << "\n";
						ignore_next_value = true;
					}
				}
				break;
			}

			default: throw std::runtime_error("Entered unexpected substate");

			}
			break;
		}
		case ns_reading_data: {
			if (val == "t")
				substate = ns_data_t;
			else if (val == "x")
				substate = ns_data_x;
			else if (val == "y")
				substate = ns_data_y;
			else if (val == "id")
				substate = ns_data_id;
			else if (val == "ox")
				substate = ns_data_ox;
			else if (val == "oy")
				substate = ns_data_oy;
			else if (timepoint_data_t::json_field_is_a_known_subclass(val)) {
				current_data_element_subclass = val;
			}
			else {
				substate = ns_data_n;
				next_data_variable = val;
			}
			break;
		}
		default:throw std::runtime_error("Entered unexpected state");
		}
		return true;
	}


	bool start_array(std::size_t elements) {
		array_depth++;
		if (substate == ns_metadata_interpolate) return true;
		if (substate == ns_metadata_software) return true;
		if (state == ns_reading_data) {
			if (data_split != ns_split_by_timepoint && elements != std::size_t(-1))
				data->data.data.reserve(elements);

			switch (object_depth) {
			case 1: if (data_split == ns_unknown_split) data_split = ns_split_by_timepoint;
				break;

			case 2:
				if (data_split != ns_split_by_timepoint && array_depth==1) {
				data_split = ns_split_by_variable;
				next_data_index = 0;
			}
				break;

			default: break;
			}
			return true;
		}

		if (next_double_vector == 0 && next_string_vector == 0)
			throw std::runtime_error("Unanticipated array");
		if (elements == std::size_t(-1))
			return true;
		if (next_double_vector != 0)
			next_double_vector->reserve(elements);
		if (next_string_vector != 0)
			next_string_vector->reserve(elements);
		return true;
	}
	bool end_array() {

		array_depth--;
		next_double_vector = 0;
		next_string_vector = 0;
		if (array_depth == 1 && data_split == ns_split_by_variable && current_data_element_subclass.empty())
			next_data_index++;

		if (state == ns_reading_data && object_depth == 2 && !current_data_element_subclass.empty() && data_split == ns_split_by_variable)
			current_data_element_subclass.resize(0);
		return true;
	}

	bool null() { return true; }

	bool boolean(bool val) { return number_float(val, ""); }

	template<class T>
	bool add_data_point(const T &val, std::string & data_element_subclass) {
		if (next_data_index >= data->data.data.size())
			data->data.data.resize(next_data_index + 1);

		switch (substate) {
		case ns_data_t:
			ns_quick_type_conversion::set(data->data.data[next_data_index].t, val); break;
		case ns_data_id:
			ns_quick_type_conversion::set(data->data.data[next_data_index].id, val); break;
		case ns_data_x:
			ns_quick_type_conversion::set(data->data.data[next_data_index].x, val); break;
		case ns_data_y:
			ns_quick_type_conversion::set(data->data.data[next_data_index].y, val); break;
		case ns_data_ox:
			ns_quick_type_conversion::set(data->data.data[next_data_index].ox, val); break;
		case ns_data_oy:
			ns_quick_type_conversion::set(data->data.data[next_data_index].oy, val); break;
		case ns_data_n:
			data->data.data[next_data_index].set_value(next_data_variable, val, current_data_element_subclass); break;
		default: throw std::runtime_error("Reached an unanticipated reading data substate.");
		}
		if (array_depth == 1 && data_split == ns_split_by_variable && current_data_element_subclass.empty()) //if we are reading each variable separately, we need to increment through each array
			next_data_index++;
		//else
		//	std::cerr << "WHA";
		return true;
	}
	bool number_integer(number_integer_t val) {
		if (ignore_next_value) {
			ignore_next_value = false;
			return true;
		}
		if (state == ns_reading_data)
			return add_data_point(val, current_data_element_subclass);
		if (next_integer != 0) {
			*next_integer = val;
			next_integer = 0;
		}
		else if (next_double != 0) {
			ns_quick_type_conversion::set(*next_double,val);
			next_double = 0;
		}
		else if (next_double_vector != 0) {
			next_double_vector->resize(1);
			ns_quick_type_conversion::set((*next_double_vector)[0],val);
			if (array_depth == 0)
				next_double_vector = 0;
		}
		else if (next_string_vector != 0) {
			next_string_vector->resize(1);
			ns_quick_type_conversion::set((*next_string_vector)[0],val);
			if (array_depth == 0)
				next_string_vector = 0;
		}
		else throw std::runtime_error("Recieving an unanticipated integer");
		return true;
	}
	bool number_unsigned(number_unsigned_t val) {
		if (ignore_next_value) {
			ignore_next_value = false;
			return true;
		}
		if (state == ns_reading_data)
			return add_data_point(val, current_data_element_subclass);

		if (next_unsigned_integer != 0) {
			*next_unsigned_integer = val;
			next_unsigned_integer = 0;
		}
		else if (next_double != 0) {
			ns_quick_type_conversion::set(*next_double,val);
			next_double = 0;
		}
		else if (next_double_vector != 0) {
			next_double_vector->resize(1);
			ns_quick_type_conversion::set((*next_double_vector)[0],val);
			if (array_depth == 0)
				next_double_vector = 0;
		}
		else if (next_string_vector != 0) {
			next_string_vector->resize(1);
			ns_quick_type_conversion::set((*next_string_vector)[0], val);
			if (array_depth == 0)
				next_string_vector = 0;
		}
		else
			throw std::runtime_error("Recieving an unanticipated unsigned integer");
		return true;
	}

	bool number_float(number_float_t val, const string_t& s) {
		if (ignore_next_value) {
			ignore_next_value = false;
			return true;
		}
		if (state == ns_reading_data)
			return add_data_point(val, current_data_element_subclass);

		if (next_double_vector != 0) {
			next_double_vector->push_back(val);
			if (array_depth == 0)
				next_double_vector = 0;
		}
		else if (next_double != 0){
			*next_double = val;
			next_double = 0;
		}
		else if (next_string != 0) {
			ns_quick_type_conversion::set(*next_string, val);
		}
		else if (next_string_vector != 0) {
			next_string_vector->resize(1);
			ns_quick_type_conversion::set((*next_string_vector)[0], val);
			if (array_depth == 0)
				next_string_vector = 0;
		}
		else
			throw std::runtime_error("Recieving an unanticipated double");
		return true;
	}
	bool string(string_t& val) {
		if (ignore_next_value) {
			ignore_next_value = false;
			return true;
		}
		if (state == ns_reading_data)
			return add_data_point(val, current_data_element_subclass);
		if (next_string_vector == 0 && next_string == 0 && next_enum == ns_no_enum)
			throw std::runtime_error("Recieving an unanticipated string");
		if (next_string != 0) {
			*next_string = val;
			next_string = 0;
		}
		else if (next_string_vector != 0) {
			next_string_vector->push_back(val);
			if (array_depth==0)
				next_string_vector = 0;
		}
		else if (next_enum != ns_no_enum) {
			switch (next_enum) {
			case ns_unit_t: data->units.time = ns_wcon_units::from_string(val); break;
			case ns_unit_x: data->units.x = ns_wcon_units::from_string(val); break;
			case ns_unit_y: data->units.y = ns_wcon_units::from_string(val); break;
			case ns_unit_ox: data->units.ox = ns_wcon_units::from_string(val); break;
			case ns_unit_oy: data->units.oy = ns_wcon_units::from_string(val); break;
			case ns_unit_n:
				if (next_unit.empty())
					throw std::runtime_error("No unit name found");
				data->units.additional_units[next_unit] = ns_wcon_units::from_string(val);
				next_unit.resize(0);
				break;
			case ns_sex:
				data->metadata.sex = ns_wcon_metadata::sex_from_string(val);
				break;
			case ns_stage:
				data->metadata.stage = ns_wcon_metadata::stage_from_string(val);
				break;
			default: throw std::runtime_error((std::string("Unknown next enum spec:") + val).c_str());
			}
			next_enum = ns_no_enum;
		}
		return true;
	}
	bool start_object(std::size_t elements) {
		object_depth++;
		switch (object_depth) {
		case 0:
		case 1:
		case 2:
			return true;
		case 3:
			if (state == ns_reading_metadata) {
				switch (substate) {
				case ns_metadata_software:
					data->metadata.software.resize(data->metadata.software.size() + 1);
					break;
				case ns_metadata_interpolate:
					data->metadata.interpolate.resize(data->metadata.interpolate.size() + 1);
					break;
				}
			}
			break;
		default: break;
		}
		return true;
	}

	bool end_object() {
		if (object_depth == 2 && state == ns_reading_data && data_split == ns_split_by_timepoint)
			next_data_index++;
		if (state == ns_reading_data && !current_data_element_subclass.empty() && object_depth == 3 && data_split != ns_split_by_variable)
			current_data_element_subclass.resize(0);
		if (data_split == ns_split_by_variable && !current_data_element_subclass.empty()) //if we are reading each variable separately, we need to increment through each array
			next_data_index++;
		object_depth--;
		return true;
	}

	bool parse_error(std::size_t position,
		const std::string& last_token,
		const  nlohmann::detail::exception & ex) {
		throw ex;
		return false;
	}

	private:

		typedef enum { ns_reading_base_class, ns_reading_data, ns_reading_metadata, ns_reading_files, ns_reading_units,ns_reading_comment,ns_reading_unrecognized } ns_state;
		typedef enum { ns_none, ns_metadata_arena, ns_metadata_lab, ns_metadata_interpolate, ns_metadata_software, ns_data_t, ns_data_id, ns_data_x, ns_data_y, ns_data_ox, ns_data_oy, ns_data_n } ns_substate;
		typedef enum { ns_no_enum, ns_unit_t, ns_unit_x, ns_unit_y, ns_unit_ox, ns_unit_oy, ns_unit_n, ns_ventral, ns_sex, ns_stage } ns_next_enum;
		typedef enum { ns_unknown_split, ns_split_by_variable, ns_split_by_timepoint } ns_data_split_type;
		ns_state state;
		std::string next_unit;
		ns_substate substate;
		unsigned long object_depth, array_depth;
		ns_next_enum next_enum;
		double * next_double;
		int64_t * next_integer;
		bool ignore_next_value;
		uint64_t * next_unsigned_integer;
		std::string * next_string;
		std::string next_data_variable;
		std::size_t next_data_index;
		std::string current_data_element_subclass;
		ns_data_split_type data_split;
		std::vector<std::string> * next_string_vector;
		std::vector<double> * next_double_vector;

		ns_worm_tracker_commons_object<timepoint_data_t> * data;
};


nlohmann::json ns_wcon_arena::to_json() const {
	nlohmann::json j;
	j["style"] = style;
	j["orientation"] = orientation;
	j["size"] = size;
	return j;
}

nlohmann::json ns_wcon_interpolation::to_json() const {
	nlohmann::json j;
	j["method"] = method;
	j["values"] = values;
	return j;
}
nlohmann::json ns_wcon_tracker::to_json() const {
	nlohmann::json j;
	j["name"] = name;
	j["version"] = version;
	return j;
}
nlohmann::json ns_wcon_software_metadata::to_json() const {
	nlohmann::json j;
	j["tracker"] = tracker.to_json();
	j["featureID"] = feature_id;
	return j;
}
nlohmann::json ns_wcon_pattern_properties::to_json()const {
	nlohmann::json j;
	j["title"] = title;
	j["description"] = description;
	return j;
}

bool ns_wcon_metadata::operator==(const ns_wcon_metadata & r) const {
	return id == r.id && lab == r.lab && who == r.who && protocol == r.protocol && timestamp == r.timestamp && temperature == r.temperature &&
		humidity == r.humidity && arena == r.arena && stage == r.stage && sex == r.sex && food == r.food &&
		media == r.media && strain == r.strain && age == r.age && interpolate == r.interpolate && r.software == r.software && additional_properties == r.additional_properties;
}
std::string ns_wcon_metadata::to_string(const ns_wcon_metadata::ns_stage & stage) {
	char buf[256];
	switch (stage) {
	case L1: return "L1";
	case L2: return "L2";
	case L3: return "L3";
	case L4: return "L4";
	case adult: return "adult";
	case dauer: return "dauer";
	case ns_stage_na: return "?";
	default:
		snprintf(buf, 256, "%d", (int)stage);
		throw std::runtime_error((std::string("Unknown stage type") + buf).c_str());
	}
}
ns_wcon_metadata::ns_stage ns_wcon_metadata::stage_from_string(std::string & stage) {
	if (stage == "L1") return L1;
	if (stage == "L2") return L2;
	if (stage == "L3") return L3;
	if (stage == "L4") return L4;
	if (stage == "adult") return adult;
	if (stage == "dauer") return dauer;
	if (stage == "?") return ns_stage_na;
	throw std::runtime_error((std::string("Unknown stage") + stage).c_str());
}
 std::string ns_wcon_metadata::to_string(const ns_wcon_metadata::ns_sex & sex) {
	char buf[256];
	switch (sex) {
	case hermaphrodite: return "hermaphrodite";
	case male: return "male";
	case ns_sex_na: return "?";
	default:
		snprintf(buf, 256, "%d", (int)sex);
		throw std::runtime_error((std::string("Unknown sex type") + buf).c_str());
	}
}
 ns_wcon_metadata::ns_sex ns_wcon_metadata::sex_from_string(std::string & sex) {
	if (sex == "hermaphrodite") return hermaphrodite;
	if (sex == "male") return male;
	if (sex == "?") return ns_sex_na;
	std::cerr << "Unknown sex specifier" << sex;
	return ns_sex_na;
}
 bool ns_wcon_metadata::is_specified() const {
	 return to_json().size() > 0;
 }
 nlohmann::json ns_wcon_metadata::to_json() const {
	 nlohmann::json j;
	 if (!id.empty()) j["id"] = id;
	 if (!lab.empty()) j["lab"] = lab;
	 if (!who.empty()) j["who"] = who;
	 if (!timestamp.empty()) j["timestamp"] = timestamp;
	 if (temperature != -1) j["temperature"] = temperature;
	 if (humidity != -1) j["humidity"] = humidity;
	 if (arena.specified()) j["arena"] = arena.to_json();
	 if (stage != ns_stage_na) j["stage"] = to_string(stage);
	 if (sex != ns_sex_na) j["sex"] = to_string(sex);
	 if (!food.empty()) j["food"] = food;
	 if (!media.empty()) j["media"] = media;
	 if (!strain.empty()) j["strain"] = strain;
	 if (age != -1) j["age"] = age;
	 {
		 nlohmann::json tmp = nlohmann::json::array();
		 for (std::vector<ns_wcon_interpolation>::const_iterator p = interpolate.begin(); p != interpolate.end(); p++)
			 if (p->specified()) tmp.push_back(p->to_json());
		 if (!tmp.empty())j["interpolate"] = tmp;
	 }
	 {
		 nlohmann::json tmp = nlohmann::json::array();
		 for (std::vector<ns_wcon_software_metadata>::const_iterator p = software.begin(); p != software.end(); p++)
			 if (p->specified()) tmp.push_back(p->to_json());
		 if (!tmp.empty())
			 j["software"] = tmp;
	 }
	 if (!protocol.empty()) j["protocol"] = protocol;
	 if (!additional_properties.empty())j["additionalProperties"] = additional_properties;
	 return j;
 }
 bool ns_wcon_units::operator==(const ns_wcon_units & r) const {
	 return time == r.time && x == r.x &&  y == r.y && ox == r.ox && oy == r.oy &&
		 additional_units == r.additional_units && additional_properties == r.additional_properties;
 }
std::string ns_wcon_units::to_string(const ns_wcon_units::ns_units & t) {
	 switch (t) {
	 case ns_wcon_nanoseconds: return "ns";
	 case ns_wcon_microseconds: return "us";
	 case ns_wcon_milliseconds: return "ms";
	 case ns_wcon_seconds: return "s";
	 case ns_wcon_minutes: return "min";
	 case ns_wcon_hours: return "h";
	 case ns_wcon_days: return "d";
	 case ns_wcon_years: return "y";
	 case ns_wcon_um: return "um";
	 case ns_wcon_mm: return "mm";
	 case ns_wcon_cm: return "cm";
	 case ns_wcon_nm: return "nm";
	 case ns_wcon_m:return "m";
	 case ns_wcon_km:return "km";
	 case ns_wcon_inches:return "in";
	 case ns_wcon_feet:return "ft";
	 case ns_wcon_yards:return "yd";
	 case ns_wcon_miles:return "mi";
	 case ns_wcon_mm_per_second: return "mm/s";
	 case ns_wcon_m_per_second: return "m/s";
	 case ns_wcon_inverse_meters: return "1/m";
	 case ns_wcon_inverse_millimeters: return "1/mm";
	 case ns_wcon_degrees_F:return "F";
	 case ns_wcon_degrees_C:return "C";
	 case ns_wcon_degrees_K:return "K";
	 case ns_radians:return "rad";
	 case ns_degrees:return "degrees";
	 case ns_percent:return "%";
	 case ns_unitless:return "1";
	 default:
		 char buf[256];
		 snprintf(buf, 256, "%d", (int)t);
		 throw std::runtime_error((std::string("Unknown unit:") + buf).c_str());
	 }
 }
bool ns_wcon_units::matches_singular(const std::string & subject, const char * test) {
	 for (unsigned int i = 0; i < subject.size(); i++) {
		 if (test[i] == 0) {
			 if (subject[i] == 's') return true;
			 return false;
		 }
		 if (tolower(subject[i]) != test[i])
			 return false;
	 }
	 return true;
 }
ns_wcon_units::ns_units ns_wcon_units::from_string(const std::string & str) {
	 if (str == "ns" || matches_singular(str, "nanosecond")) return ns_wcon_nanoseconds;
	 if (str == "us" || matches_singular(str, "microsecond")) return ns_wcon_microseconds;
	 if (str == "ms" || matches_singular(str, "millisecond")) return ns_wcon_milliseconds;
	 if (str == "s" || matches_singular(str, "second")) return ns_wcon_seconds;
	 if (str == "min" || matches_singular(str, "minute")) return ns_wcon_minutes;
	 if (str == "h" || matches_singular(str, "hour")) return ns_wcon_hours;
	 if (str == "d" || matches_singular(str, "day")) return ns_wcon_days;
	 if (str == "y" || matches_singular(str, "year")) return ns_wcon_years;
	 if (str == "nm" || matches_singular(str, "nanometer")) return ns_wcon_nm;
	 if (str == "um" || matches_singular(str, "micron")) return ns_wcon_um;
	 if (str == "mm" || matches_singular(str, "millimeter")) return ns_wcon_mm;
	 if (str == "cm" || matches_singular(str, "centimeter")) return ns_wcon_cm;
	 if (str == "m" || matches_singular(str, "meter")) return ns_wcon_m;
	 if (str == "km" || matches_singular(str, "kilometer")) return ns_wcon_km;
	 if (str == "in" || matches_singular(str, "inche")) return ns_wcon_inches;
	 if (str == "ft" || str == "foot" || str == "feet") return ns_wcon_feet;
	 if (str == "yd" || matches_singular(str, "yard")) return ns_wcon_yards;
	 if (str == "mi" || matches_singular(str, "mile")) return ns_wcon_miles;
	 if (str == "mm/s") return ns_wcon_mm_per_second;
	 if (str == "m/s") return ns_wcon_m_per_second;
	 if (str == "1/m") return ns_wcon_inverse_meters;
	 if (str == "1/mm") return ns_wcon_inverse_millimeters;
	 if (str == "F" || matches_singular(str, "fahrenheit")) return ns_wcon_degrees_F;
	 if (str == "C" || matches_singular(str, "centigrade")) return ns_wcon_degrees_C;
	 if (str == "K" || matches_singular(str, "kelvin")) return ns_wcon_degrees_K;
	 if (str == "rad" || matches_singular(str, "radian")) return ns_radians;
	 if (str == "%" || matches_singular(str, "percent")) return ns_percent;
	 if (str == "degrees") return ns_degrees;
	 if (str == "" || str == "1") return ns_unitless;
	 throw std::runtime_error((std::string("Unknown unit:") + str).c_str());

 }

 nlohmann::json ns_wcon_units::to_json() const {
	 nlohmann::json j;
	 j["t"] = to_string(time);
	 j["x"] = to_string(x);
	 j["y"] = to_string(y);
	 if (ox != ns_unitless || oy != ns_unitless) {
		 j["ox"] = to_string(ox);
		 j["oy"] = to_string(oy);
	 }

	 if (!additional_properties.empty())
		 j["additionalProperties"] = additional_properties;
	 for (std::map<std::string, ns_units>::const_iterator p = additional_units.begin(); p != additional_units.end(); p++)
		 j[p->first] = to_string(p->second);
	 return j;
 }

 bool ns_wcon_file_list_entry::is_specified() const { return !prev.empty() || !next.empty(); }

 nlohmann::json ns_wcon_file_list_entry::to_json() const {
	 nlohmann::json j;
	 if (!prev.empty()) j["prev"] = prev;
	 if (!prev.empty()) j["current"] = current;
	 if (!next.empty()) j["next"] = next;
	 return j;
 }

 bool ns_wcon_default_data_record_element::operator==(const ns_wcon_default_data_record_element & a) const {
	 return ns_quick_type_conversion::compare_double(t, a.t) &&
		 ns_quick_type_conversion::compare_double(x, a.x) &&
		 ns_quick_type_conversion::compare_double(y, a.y) &&
		 ns_quick_type_conversion::compare_double(ox, a.ox) &&
		 ns_quick_type_conversion::compare_double(oy, a.oy) &&
		 id == a.id;
 }
 nlohmann::json ns_wcon_default_data_record_element::to_json() const {
	 nlohmann::json j;
	 j["id"] = id;
	 j["t"] = t;
	 j["x"] = x;
	 j["y"] = y;
	 if (ox != 0 || oy != 0) {
		 j["ox"] = ox;
		 j["oy"] = oy;
	 }
	 return j;
 }

 template<class timepoint_data_t>
 nlohmann::json  ns_wcon_data_record<timepoint_data_t>::to_json(bool split_timepoints) const {
	 return (split_timepoints) ? to_json_split_timepoints() : to_json_split_measurements();
 }

 template<class timepoint_data_t>
 nlohmann::json ns_wcon_data_record<timepoint_data_t>::to_json_split_timepoints() const {
	 if (data.size() == 0)
		 throw std::runtime_error(" ns_wcon_data_record::to_json()::No data was provided");
	 nlohmann::json j;
	 std::vector<nlohmann::json> tmp;
	 tmp.reserve(data.size());
	 for (std::vector<double>::size_type i = 0; i < data.size(); i++)
		 tmp.push_back(data[i].to_json());
	 return nlohmann::json(tmp);
 }

 template<class timepoint_data_t>
 nlohmann::json ns_wcon_data_record<timepoint_data_t>::to_json_split_measurements() const {
	 if (data.size() == 0)
		 throw std::runtime_error(" ns_wcon_data_record::to_json()::No data was provided");
	 nlohmann::json j;

	 std::vector<double> tmp;
	 std::vector<std::string> tmp_s;
	 tmp.resize(data.size());
	 tmp_s.resize(data.size());

	 //add required t, x, and y
	 for (std::vector<double>::size_type i = 0; i < data.size(); i++)	tmp_s[i] = data[i].get_id();
	 j["id"] = tmp_s;
	 for (std::vector<double>::size_type i = 0; i < data.size(); i++)	tmp[i] = data[i].get_t();
	 j["t"] = tmp;
	 for (std::vector<double>::size_type i = 0; i < data.size(); i++)	tmp[i] = data[i].get_x();
	 j["x"] = tmp;
	 for (std::vector<double>::size_type i = 0; i < data.size(); i++)	tmp[i] = data[i].get_y();
	 j["y"] = tmp;

	 bool oxy_specified = false;
	 for (std::vector<double>::size_type i = 0; i < data.size(); i++)
		 if (data[i].get_ox() != 0 || data[i].get_oy() != 0) {
			 oxy_specified = true;
			 break;
		 }
	 if (oxy_specified) {
		 for (std::vector<double>::size_type i = 0; i < data.size(); i++)	tmp[i] = data[i].get_ox();
		 j["ox"] = tmp;
		 for (std::vector<double>::size_type i = 0; i < data.size(); i++)	tmp[i] = data[i].get_oy();
		 j["oy"] = tmp;
	 }

	 //add any additional time-varying fields
	 for (int k = 0; k < timepoint_data_t::number_of_additional_json_fields(); k++) {
		 ns_wcon_data_element_member_type type = timepoint_data_t::additional_json_field_type(k);
		 switch (type) {
		 case ns_unknown:
			 std::cerr << "Encountered an unknown field type for spec" << k << "\n"; break;
		 case ns_string:
			 for (std::vector<std::string>::size_type i = 0; i < data.size(); i++)
				 tmp_s[i] = data[i].get_additional_json_field_value_string(k);
			 j[data[0].additional_json_field_name(k)] = tmp_s;
			 break;
		 case ns_double:
			 for (std::vector<double>::size_type i = 0; i < data.size(); i++)
				 tmp[i] = data[i].get_additional_json_field_value_double(k);
			 j[data[0].additional_json_field_name(k)] = tmp;
			 break;
		 case ns_vector_double: {
			 std::vector<nlohmann::json> j2;
			 j2.reserve(data.size());
			 for (std::vector<double>::size_type i = 0; i < data.size(); i++)
				 j2.push_back(*data[i].get_additional_json_field_value_vector_double(k));
			 j[data[0].additional_json_field_name(k)] = j2;
			 break;
		 }
		 case ns_vector_string: {
			 std::vector<nlohmann::json> j2;
			 j2.reserve(data.size());
			 for (std::vector<double>::size_type i = 0; i < data.size(); i++)
				 j2.push_back(*data[i].get_additional_json_field_value_vector_string(k));
			 j[data[0].additional_json_field_name(k)] = j2;
			 break;
		 }
		 case ns_subclass: {
			 std::vector<nlohmann::json> j2;
			 j2.reserve(data.size());
			 for (std::vector<double>::size_type i = 0; i < data.size(); i++)
				 j2.push_back(data[i].add_subclass(k));
			 j[data[0].additional_json_field_name(k)] = j2;
			 break;
		 }
		 default:
			 throw std::runtime_error("Unknown data member type variable");
		 }

	 }
	 return j;
 }

 class ns_data_record_sorter {
 public:
	 template<class timepoint_data_t>
	 bool operator ()(const timepoint_data_t & a, const timepoint_data_t & b) {
		 if (a.get_t() < b.get_t()) return true;
		 if (a.get_t() > b.get_t()) return false;
		 if (a.get_id() < b.get_id()) return true;
		 if (a.get_id() > b.get_id()) return false;
		 return false;
	 }
	 template<class timepoint_data_t>
	 bool equiv(const timepoint_data_t & a, const timepoint_data_t & b) {
		 return a.get_t() == b.get_t() && a.get_id() == b.get_it();
	 }

 };
 template<class timepoint_data_t>
 void ns_wcon_data_record<timepoint_data_t>::sort_by_time_and_id() {
	 std::sort(data.begin(), data.end(), ns_data_record_sorter());
 }



 template<class timepoint_data_t>
 nlohmann::json ns_worm_tracker_commons_object<timepoint_data_t>::to_json(bool split_timepoints) const {
	 nlohmann::json j;

	 j["data"] = data.to_json(split_timepoints);

	 if (files.is_specified()) j["files"] = files.to_json();

	 j["units"] = units.to_json();

	 if (metadata.is_specified()) j["metadata"] = metadata.to_json();

	 return j;
 }

template<class timepoint_data_t>
void ns_worm_tracker_commons_object<timepoint_data_t>::from_stream(std::istream & in) {

	ns_wcon_sax_parser<timepoint_data_t> sax_parser(*this);
	nlohmann::detail::input_adapter adaptor(in);
	nlohmann::detail::parser<nlohmann::json> parser(adaptor, nullptr);
	parser.sax_parse(&sax_parser);
 }
template<class timepoint_data_t>
void ns_worm_tracker_commons_object<timepoint_data_t>::to_stream(std::ostream & out) const{
	 out << to_json(write_split_timepoint).dump(2, ' ', true);
 }
template<class timepoint_data_t>
 std::ostream& operator<<(std::ostream& o, const ns_worm_tracker_commons_object<timepoint_data_t> & j) {
	 j.to_file(o);
	 return o;
 }
 template<class timepoint_data_t>
 std::istream& operator>>(std::istream& i, ns_worm_tracker_commons_object<timepoint_data_t> & j) {
	 return j.from_file(i);
 }
