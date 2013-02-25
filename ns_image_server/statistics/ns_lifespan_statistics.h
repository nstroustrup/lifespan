#ifndef NS_LIFESPAN_STATISTICS
#define NS_LIFESPAN_STATISTICS
#include "ns_xml.h"
#include <vector>
#include <map>
#include "ns_survival_curve.h"

struct ns_device_lifespan_statistics{
	typedef std::vector<ns_survival_data_summary> ns_lifespan_data_summary_set;
	ns_lifespan_data_summary_set set;
	std::string device_name;
};


class ns_device_lifespan_statistics_compiler{
public:
	void clear(){
		devices.clear();
	}
	typedef std::map<std::string,ns_device_lifespan_statistics> ns_scanner_data_list;
	ns_scanner_data_list devices;

	void add_data(const ns_survival_data_summary & data){
		ns_scanner_data_list::iterator p = devices.find(data.metadata.device);
		if (p == devices.end()){
			p = devices.insert(ns_scanner_data_list::value_type(data.metadata.device,ns_device_lifespan_statistics())).first;
			p->second.device_name = data.metadata.device;
		}
		p->second.set.push_back(data);
	}

	static void out_jmp_header(std::ostream & o){
		ns_survival_data_summary::out_jmp_header("",o);
	}
	void out_jmp_data(std::ostream & o) const{
		for (ns_scanner_data_list::const_iterator p = devices.begin(); p != devices.end(); ++p){
			for (ns_device_lifespan_statistics::ns_lifespan_data_summary_set::const_iterator q = p->second.set.begin(); q != p->second.set.end(); q++){
				q->out_jmp_data(o);
			}
		}
	}


	void save_to_disk(std::ostream & o){
		ns_xml_simple_writer xml;
		xml.add_header();
		xml.start_group("ns_device_lifespan_statistics_set");
		xml.add_tag("generation_time",ns_current_time());
		xml.end_group();
		for (ns_scanner_data_list::const_iterator p = devices.begin(); p != devices.end(); ++p)
			for (ns_device_lifespan_statistics::ns_lifespan_data_summary_set::const_iterator q = p->second.set.begin(); q != p->second.set.end(); q++)
				xml.add_raw(q->to_xml());
		xml.add_footer();
		o << xml.result();
	}
	void load_from_disk(std::istream & in){
		ns_xml_simple_object_reader xml;
		xml.from_stream(in);
		for (unsigned int i = 0; i < xml.objects.size(); i++){
			ns_survival_data_summary data;
			if (xml.objects[i].name == ns_survival_data_summary::xml_tag()){
				data.from_xml(xml.objects[i]);
				add_data(data);
			}
			else if (xml.objects[i].name == "ns_device_lifespan_statistics_set")
				continue;
			else throw ns_ex("ns_device_lifespan_statistics_compiler::load_from_disk()::Unknown tag:") << xml.objects[i].name;
		}
	}	
};


class ns_device_lifespan_strain_mean_normalizer{
public:
	void build(const ns_device_lifespan_statistics_compiler & in, ns_device_lifespan_statistics_compiler & out){
		for (ns_device_lifespan_statistics_compiler::ns_scanner_data_list::const_iterator p = in.devices.begin(); p != in.devices.end(); ++p){
			for (unsigned int i = 0; i < p->second.set.size(); i++){
				add_summary_to_mean_data(p->second.set[i]);
			}
		}
		finish_adding_mean_data();

		generate_normalized_set(in,out);
	}
	void clear(){
		strain_mean_data.clear();
	}
private:
	void generate_normalized_set(const ns_device_lifespan_statistics_compiler & in,ns_device_lifespan_statistics_compiler & out){
		out.clear();
		for (ns_device_lifespan_statistics_compiler::ns_scanner_data_list::const_iterator p = in.devices.begin(); p != in.devices.end(); ++p){
			for (unsigned int i = 0; i < p->second.set.size(); i++){
				
				ns_survival_data_summary data(p->second.set[i]);
				ns_strain_mean_data::iterator q = strain_mean_data.find(strain_hash(data));

				if (q == strain_mean_data.end())
					throw ns_ex("ns_device_lifespan_strain_mean_normalizer::build()::Encountered an unknown strain");

				if (q->second.death.mean != 0)
					data.death = data.death.scale(q->second.death.mean);	
				if (q->second.local_movement_cessation.mean != 0)
					data.local_movement_cessation = data.local_movement_cessation.scale(q->second.long_distance_movement_cessation.mean);	
				if (q->second.long_distance_movement_cessation.mean != 0)
					data.long_distance_movement_cessation = data.long_distance_movement_cessation.scale(q->second.long_distance_movement_cessation.mean);
				out.add_data(data);
			}
		}
	}
	typedef std::map<std::string,ns_survival_data_summary>  ns_strain_mean_data;
	ns_strain_mean_data strain_mean_data;

	static std::string strain_hash(const ns_survival_data_summary & s){
		return s.metadata.strain + ":" + s.metadata.strain_condition_1 + ":" + s.metadata.strain_condition_2;
	}
	void finish_adding_mean_data(){
		for (ns_strain_mean_data::iterator p = strain_mean_data.begin(); p != strain_mean_data.end(); ++p){
			if (p->second.death.count > 0){
				p->second.death.mean/=p->second.death.count;
				p->second.death.number_of_events_involving_multiple_worm_disambiguation/=p->second.death.count;
			}
			if (p->second.local_movement_cessation.count > 0){
				p->second.local_movement_cessation.mean/=p->second.local_movement_cessation.count;
				p->second.local_movement_cessation.number_of_events_involving_multiple_worm_disambiguation/=p->second.local_movement_cessation.count;
			}
			if (p->second.long_distance_movement_cessation.count > 0){
				p->second.long_distance_movement_cessation.mean/=p->second.long_distance_movement_cessation.count;
				p->second.long_distance_movement_cessation.number_of_events_involving_multiple_worm_disambiguation/=p->second.long_distance_movement_cessation.count;
			}
		}
	}
	void add_summary_to_mean_data(const ns_survival_data_summary & s){
		const std::string h(strain_hash(s));
		ns_strain_mean_data::iterator p = strain_mean_data.find(h);
		if (p == strain_mean_data.end()){
			p = strain_mean_data.insert(ns_strain_mean_data::value_type(h,ns_survival_data_summary())).first;
			p->second.death.set_as_zero();
			p->second.local_movement_cessation.set_as_zero();
			p->second.long_distance_movement_cessation.set_as_zero();
		}
		p->second.death.mean+=s.death.mean;
		p->second.local_movement_cessation.mean+=s.local_movement_cessation.mean;
		p->second.long_distance_movement_cessation.mean+=s.long_distance_movement_cessation.mean;

		p->second.death.number_of_events_involving_multiple_worm_disambiguation+=s.death.number_of_events_involving_multiple_worm_disambiguation;
		p->second.local_movement_cessation.number_of_events_involving_multiple_worm_disambiguation+=s.local_movement_cessation.number_of_events_involving_multiple_worm_disambiguation;
		p->second.long_distance_movement_cessation.number_of_events_involving_multiple_worm_disambiguation+=s.long_distance_movement_cessation.number_of_events_involving_multiple_worm_disambiguation;

		p->second.death.count++;
		p->second.local_movement_cessation.count++;
		p->second.long_distance_movement_cessation.count++;
	}
};

#endif
