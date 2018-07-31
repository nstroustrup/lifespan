#ifndef NS_REGION_METADATA
#define NS_REGION_METADATA

#include "ns_image_server_sql.h"
#include "ns_xml.h"

struct ns_region_metadata{
	ns_region_metadata():time_at_which_animals_had_zero_age(0),
		time_of_last_valid_sample(0),region_id(0),sample_id(0), position_of_region_in_sample(0,0),position_of_sample_on_scanner(0,0),size(0,0),movement_rebuild_timestamp(0),by_hand_annotation_timestamp(0){}

	std::string device,
		technique,
		experiment_name,
		sample_name,
		region_name,
		strain,
		strain_condition_1,
		strain_condition_2,
		strain_condition_3,
		culturing_temperature,
		experiment_temperature,
		food_source,
		environmental_conditions,
		genotype,
		details,
		analysis_type;

	typedef enum { ns_strain, ns_strain_and_condition_1, ns_strain_and_conditions_1_and_2,ns_strain_and_conditions_1_2_and_3} ns_strain_description_detail_type;
	static ns_strain_description_detail_type description_detail_type_from_string(const std::string & s){
		if (s == "s")
			return ns_strain;
		if (s == "s1")
			return ns_strain_and_condition_1;
		if (s == "s12")
			return ns_strain_and_conditions_1_and_2;
		if (s == "s123")
			return ns_strain_and_conditions_1_2_and_3;
		throw ns_ex("ns_region_metadata::description_detail_type_from_string()::Could not parse description detail type: ") << s;
	}
	bool matches(const ns_strain_description_detail_type & t, const ns_region_metadata & m) const{
		switch(t){
			case ns_strain:
				return strain == m.strain;
			case ns_strain_and_condition_1:
				return strain == m.strain && 
				strain_condition_1 == m.strain_condition_1;
			case ns_strain_and_conditions_1_and_2:
				return strain == m.strain && 
				strain_condition_1 == m.strain_condition_1 && 
				strain_condition_2 == m.strain_condition_2;
			case ns_strain_and_conditions_1_2_and_3:
				return strain == m.strain && 
				strain_condition_1 == m.strain_condition_1 && 
				strain_condition_2 == m.strain_condition_2 &&
				strain_condition_3 == m.strain_condition_3;
			default: throw ns_ex("ns_region_metadata::matches()::Unknown strain detail type: ") << (int)t;
		}
	}

	std::string incubator_name,
			    incubator_location;

	ns_vector_2d position_of_region_in_sample,
				 position_of_sample_on_scanner,
				 size;
	ns_vector_2d center_position_of_region_on_scanner() const {return position_of_region_in_sample +  position_of_sample_on_scanner + size/2;}
	
	ns_64_bit region_id,
				  sample_id,
				  experiment_id;
	unsigned long movement_rebuild_timestamp, by_hand_annotation_timestamp;

	const std::string device_regression_match_description() const{
		std::string s(strain);
	//	if (genotype.empty())
		//	s = strain;
		if (strain_condition_1.empty() && strain_condition_2.empty())
			return s;
		if (strain_condition_1.empty())
			return s + "::" + strain_condition_2;
		if (strain_condition_2.empty())
			return s + ":" + strain_condition_1;
		return s + ":" + strain_condition_1+ ":" + strain_condition_2;
	}
	const std::string plate_type_summary() const{
		std::string type;

		const std::string * details[6] = {&	strain_condition_1,
		&strain_condition_2,
		&strain_condition_3,
		&culturing_temperature,
	//	&experiment_temperature,
		&food_source,
		&environmental_conditions};
		if (genotype.size() != 0)
			type+=genotype;
		else type+= strain;
		for (unsigned int i = 0; i < 6; i++){
			if (details[i]->size() != 0){
				type+="::";
				type+=*(details[i]);
			}
		}
		return type;
	}
	char incubator_column() const{
		if (incubator_location.size() != 2) 
			throw ns_ex("Invalid incubator position: ") << incubator_location; 
		return toupper(incubator_location[1]);
	}
	unsigned long incubator_shelf() const{
		if (incubator_location.size() != 2) 
			throw ns_ex("Invalid incubator position: ") << incubator_location; 
		return incubator_location[0]-'0';
	}
	bool incubator_location_specified() const {return incubator_location.size() == 2;}

std::string to_xml() const;
	bool from_xml(const ns_xml_simple_object & o);
	static std::string by_hand_technique_label(){return "Machine-Assisted By Hand Annotation";}

	unsigned char plate_column() const{
		if (this->sample_name.size() == 0)
			return '.';
		return *sample_name.rbegin();
	}

	std::string plate_position() const{
		std::string s;
		s += plate_column();
		s += "::";
		s += this->region_name;
		return s;
	}

	std::string plate_name() const{
		return sample_name + "::" + region_name;
	}

	unsigned long time_at_which_animals_had_zero_age,
				  time_of_last_valid_sample;


	static inline void out_JMP_plate_identity_header_short(std::ostream & o){
		o << "Experiment Name,Plate Name,Device, Strain, Condition 1, Condition 2, Condition 3,"
			 "Culturing Temperature,Experiment Temperature, Food Source,Environmental Conditions";
	}

	void out_JMP_plate_identity_data_short(std::ostream & o) const{
		o << experiment_name << "," << plate_name() << "," << device << ","
		  << strain << "," << strain_condition_1 << "," << strain_condition_2 << "," << strain_condition_3
		<< "," << culturing_temperature
		<< "," << experiment_temperature
		<< "," << food_source
		<< "," << environmental_conditions;
	}
	static inline void out_JMP_plate_identity_header(std::ostream & o){
		o << "Device,Experiment,Plate Name,Animal Description,Plate Position Name,Plate Row,Plate Column,"
			"Plate Center Position X (inches),Plate Center Position Y (inches),Plate Size X (inches),Plate Size Y (inches),"
			"Strain,Genotype,Condition 1,Condition 2,Condition 3,"
			"Culturing Temperature,Experiment Temperature, Food Source,Environmental Conditions,"
			"Incubator Name,Incubator Column,Incubator Shelf";
	}

	static std::string out_proc(const std::string & s);

	void out_JMP_plate_identity_data(std::ostream & o) const;
	
	void clear(){
		device.clear();
		technique.clear();
		experiment_name.clear();
		sample_name.clear();
		region_name.clear();
		strain.clear();
		details.clear();
		analysis_type.clear(); 
		strain_condition_1.clear();
		strain_condition_2.clear();
		strain_condition_3.clear();
		culturing_temperature.clear();
		experiment_temperature.clear();
		food_source.clear();
		environmental_conditions.clear();
		genotype.clear();
		time_at_which_animals_had_zero_age = 0;
		incubator_name.clear();
		incubator_location.clear();
		region_id = 0;
		sample_id = 0;
		experiment_id = 0;
		movement_rebuild_timestamp = 0;
		 position_of_region_in_sample = 
				 position_of_sample_on_scanner = 
				 size = ns_vector_2d(0,0);
	}
	
	void load_from_db(const ns_64_bit region_info_id, const std::string &analysis_type_, ns_sql & sql);
	
	void load_only_region_info_from_db(const ns_64_bit region_info_id, const std::string &analysis_type_, ns_sql & sql);

	void load_only_sample_info_from_db(const ns_64_bit sample_id, ns_sql & sql);

	static bool is_age_zero_field(const std::string & s);
	void load_from_fields(const std::map<std::string,std::string> & m,std::map<std::string,std::string> &unknown_values);
	std::string * get_field_by_name(const std::string & s);
	static void recognized_field_names(std::vector<std::string> & names);
};
#endif
