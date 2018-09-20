#pragma once

//////////////////////////////////////////////////////////////////////////
//
// ns_wcon_rich_data_record.h
//
// This class ns_wcon_rich_data_record_element implements the parser/writer for the full
// Worm tracker Commons Object Notation schema

// wcon_schema.json // as specified in August 2018.
// https://github.com/openworm/tracker-commons
//
// Implemented by Nicholas Stroustrup
// Centre for Genomic Regulation
// 2018
// https://github.com/nstroustrup
// http://lifespanmachine.crg.eu
//
// Worm images are left as MIME-endcoded strings
//
//////////////////////////////////////////////////////////////////////////


//corresponds to similarly named object in WCON schema
struct ns_wcon_pixel_walk_record {
	std::vector<double> px;
	std::vector<double> n;
	std::string four;

	bool operator==(const ns_wcon_pixel_walk_record & r) const;

	bool is_specified() const { return !px.empty(); }
	static std::string json_name() { return "walk"; }
	nlohmann::json to_json() const;
};

class ns_wcon_rich_data_record_element {
public:
	ns_wcon_rich_data_record_element() :ventral(ventral_NA), head(head_NA), ox(0), oy(0),
		json_reader_current_px_index(0), json_reader_current_py_index(0), json_reader_current_ptail_index(0), json_reader_current_walk_px_index(0), json_reader_current_walk_n_index(0) {}
	std::string id;
	double t, x, y, ox, oy;
	const std::string & get_id() const { return id; }
	const double & get_t() const { return t; }
	const double & get_x() const { return x; }
	const double & get_y() const { return y; }
	const double & get_ox() const { return ox; }
	const double & get_oy() const { return oy; }
	typedef enum { L, R, head_NA } ns_head;
	typedef enum { CW, CCW, ventral_NA } ns_ventral;
	double cx, cy;
	std::vector<double> px, py, ptail;
	ns_head head;
	ns_ventral ventral;
	ns_wcon_pixel_walk_record walk;
	std::map<std::string, std::map<std::string, std::vector<std::string> > > additional_fields;

	bool operator==(const ns_wcon_rich_data_record_element & r) const;


	static std::string to_string(const ns_head & head);
	static std::string to_string(const ns_ventral & ventral);
	static ns_head head_from_string(const std::string & str);
	static ns_ventral ventral_from_string(const std::string & str);
	nlohmann::json to_json() const;

	nlohmann::json add_subclass(int i) const;
	template<class T>
	bool set_value(const std::string & key, const T & val, const std::string & subclass_name);

	static int number_of_additional_json_fields() { return 8; }
	static std::string additional_json_field_name(const int &i);

	static bool json_field_is_a_known_subclass(const std::string & key);
	static ns_wcon_data_element_member_type additional_json_field_type(const int &i);

	double get_additional_json_field_value_double(const int &i) const;
	std::string get_additional_json_field_value_string(const int &i) const;
	const std::vector<double> * get_additional_json_field_value_vector_double(const int &i) const ;
	const std::vector < std::string > * get_additional_json_field_value_vector_string(const int &i) const;
	nlohmann::json get_additional_json_field_value_subclass(const int &i) const;

private:
	//internal state during file parsing
	std::size_t json_reader_current_px_index,
		json_reader_current_py_index,
		json_reader_current_ptail_index,
		json_reader_current_walk_px_index,
		json_reader_current_walk_n_index;
};

//////////////////////////////////////////////////////////////////////////
//
// Implementation
//
//////////////////////////////////////////////////////////////////////////
bool ns_wcon_rich_data_record_element::operator==(const ns_wcon_rich_data_record_element & r) const {

#ifdef NS_wcon_VERBOSE
	if (!(head == r.head))
		std::cerr << "difference in head\n";
	if (!(ventral == r.ventral))
		std::cerr << "difference in ventral\n";
	if (!(walk == r.walk))
		std::cerr << "difference in walk\n";
#endif

	return  ns_quick_type_conversion::compare_double(t, r.t) &&
		ns_quick_type_conversion::compare_double(x, r.x) &&
		ns_quick_type_conversion::compare_double(y, r.y) &&
		ns_quick_type_conversion::compare_double(ox, r.ox) &&
		ns_quick_type_conversion::compare_double(oy, r.oy) &&
		ns_quick_type_conversion::compare_double(cx, r.cx) &&
		ns_quick_type_conversion::compare_double(cy, r.cy) &&
		ns_quick_type_conversion::compare_double_arrays(px, r.px) &&
		ns_quick_type_conversion::compare_double_arrays(py, r.py) &&
		ns_quick_type_conversion::compare_double_arrays(ptail, r.ptail) &&
		head == r.head &&
		ventral == r.ventral &&
		walk == r.walk;
}
std::string ns_wcon_rich_data_record_element::to_string(const ns_wcon_rich_data_record_element::ns_head & head) {
	switch (head) {
	case L: return "L";
	case R: return "R";
	case head_NA: return "?";
	default:throw std::runtime_error("Unkonwn head spec");
	}
}

std::string ns_wcon_rich_data_record_element::to_string(const ns_wcon_rich_data_record_element::ns_ventral & ventral) {
	switch (ventral) {
	case CW: return "CW";
	case CCW: return "CCW";
	case ventral_NA: return "?";
	default:throw std::runtime_error("Unkonwn head spec");
	}
}
ns_wcon_rich_data_record_element::ns_head ns_wcon_rich_data_record_element::head_from_string(const std::string & str) {
	if (str == "L") return L;
	if (str == "R") return R;
	if (str == "?") return head_NA;
	throw std::runtime_error((std::string("Unknown head spec ") + str).c_str());
}
ns_wcon_rich_data_record_element::ns_ventral ns_wcon_rich_data_record_element::ventral_from_string(const std::string & str) {
	if (str == "CW") return CW;
	if (str == "CCW") return CCW;
	if (str == "?") return ventral_NA;
	throw std::runtime_error((std::string("Unknown ventral spec ") + str).c_str());
}
nlohmann::json ns_wcon_rich_data_record_element::to_json() const {
	nlohmann::json j;
	j["id"] = id;
	j["t"] = t;
	j["x"] = x;
	j["y"] = y;
	if (ox != 0 || oy != 0) {
		j["ox"] = ox;
		j["oy"] = oy;
	}
	j["cx"] = cx;
	j["cy"] = cy;
	j["px"] = px;
	j["py"] = py;
	j["ptail"] = ptail;
	j["head"] = to_string(head);
	j["ventral"] = to_string(ventral);
	if (walk.is_specified())
		j[walk.json_name()] = walk.to_json();
	return j;
}


nlohmann::json ns_wcon_rich_data_record_element::add_subclass(int i) const {
	nlohmann::json j;
	if (i == 7) {
		if (walk.is_specified())
			j = walk.to_json();
	}
	else
		throw std::runtime_error("Unknown subclass");
	return j;
}
template<class T>
bool ns_wcon_rich_data_record_element::set_value(const std::string & key, const T & val, const std::string & subclass_name) {
	if (subclass_name == "") {
		std::string tmp;
		if (key == "px") {
			if (json_reader_current_px_index >= px.size())
				px.resize(px.size() + 1);
			ns_quick_type_conversion::set(px[json_reader_current_px_index], val);
			json_reader_current_px_index++;
		}
		else if (key == "py") {
			if (json_reader_current_py_index >= py.size())
				py.resize(py.size() + 1);
			ns_quick_type_conversion::set(py[json_reader_current_py_index], val);
			json_reader_current_py_index++;
		}
		else if (key == "ptail") {
			if (json_reader_current_ptail_index >= ptail.size())
				ptail.resize(ptail.size() + 1);
			ns_quick_type_conversion::set(ptail[json_reader_current_ptail_index], val);
			json_reader_current_ptail_index++;
		}
		else if (key == "cx")
			ns_quick_type_conversion::set(cx, val);
		else if (key == "cy")
			ns_quick_type_conversion::set(cy, val);
		else if (key == "head") {
			ns_quick_type_conversion::set(tmp, val);
			head = head_from_string(tmp);
		}
		else if (key == "ventral") {
			ns_quick_type_conversion::set(tmp, val);
			ventral = ventral_from_string(tmp);
		}
		else {
			std::cerr << "Encountered unexpected data variable " << key << "\n";
			std::size_t s(additional_fields[subclass_name][key].size());
			additional_fields[subclass_name][key].resize(s + 1);
			ns_quick_type_conversion::set(additional_fields[subclass_name][key][s], val);
		}
	}
	else if (subclass_name == walk.json_name()) {
		if (key == "px") {
			if (json_reader_current_walk_px_index >= walk.px.size())
				walk.px.resize(walk.px.size() + 1);
			ns_quick_type_conversion::set(walk.px[json_reader_current_walk_px_index], val);
			json_reader_current_walk_px_index++;
		}
		else if (key == "n") {
			if (json_reader_current_walk_n_index >= walk.n.size())
				walk.n.resize(walk.n.size() + 1);
			ns_quick_type_conversion::set(walk.n[json_reader_current_walk_n_index], val);
			json_reader_current_walk_n_index++;
		}
		else if (key == "4") {
			ns_quick_type_conversion::set(walk.four,val);
		}
		else {
			std::cerr << "Encountered unexpected data variable " << key << "\n";
			std::size_t s(additional_fields[subclass_name][key].size());
			additional_fields[subclass_name][key].resize(s + 1);
			ns_quick_type_conversion::set(additional_fields[subclass_name][key][s], val);
		}
	}
	return true;
}

std::string ns_wcon_rich_data_record_element::additional_json_field_name(const int &i) {
	switch (i) {
	case 0: return "cx";
	case 1: return "cy";
	case 2: return "px";
	case 3: return "py";
	case 4: return "ptail";
	case 5: return "head";
	case 6: return "ventral";
	case 7: return ns_wcon_pixel_walk_record::json_name();
	default:
		throw std::runtime_error("Unknown field id");
	}
}

bool ns_wcon_rich_data_record_element::json_field_is_a_known_subclass(const std::string & key) {
	return key == ns_wcon_pixel_walk_record::json_name();
}
ns_wcon_data_element_member_type ns_wcon_rich_data_record_element::additional_json_field_type(const int &i) {
	switch (i) {
	case 0:
	case 1:
		return ns_wcon_double;
	case 2:
	case 3:
	case 4:
		return ns_vector_double;
	case 5:
	case 6:
		return ns_string;
	case 7: return ns_wcon_subclass;
	default: return ns_wcon_unknown;
	}
}

double ns_wcon_rich_data_record_element::get_additional_json_field_value_double(const int &i) const {
	switch (i) {
	case 0: return cx;
	case 1: return cy;
	default:
		throw std::runtime_error("Unknown field id");
	}
}
std::string ns_wcon_rich_data_record_element::get_additional_json_field_value_string(const int &i) const {
	char buf[256];
	switch (i) {
	case 5: return to_string(head);
	case 6: return to_string(ventral);
	default:
		throw std::runtime_error("Unknown field id");
	}
	return buf;
}
const std::vector<double> * ns_wcon_rich_data_record_element::get_additional_json_field_value_vector_double(const int &i) const {
	switch (i) {
	case 2: return &px;
	case 3: return &py;
	case 4: return &ptail;
	default:
		throw std::runtime_error("Unknown field id");
	}
}
const std::vector < std::string > * ns_wcon_rich_data_record_element::get_additional_json_field_value_vector_string(const int &i) const {
	throw std::runtime_error("No string vectors supported");
}

nlohmann::json ns_wcon_pixel_walk_record::to_json() const {
	nlohmann::json j;
	j["4"] = four;
	j["n"] = n;
	j["px"] = px;
	return j;
}
bool ns_wcon_pixel_walk_record::operator==(const ns_wcon_pixel_walk_record & r) const {
	return ns_quick_type_conversion::compare_double_arrays(px, r.px) &&
		ns_quick_type_conversion::compare_double_arrays(n, r.n) &&
		four == r.four;
}
