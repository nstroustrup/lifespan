#ifndef NS_MORTALITY_FILE_LOADER
#define NS_MORTALITY_FILE_LOADER
#include "ns_ex.h"
#include "ns_xml.h"
#include <fstream>
#include <string>
#include <iostream>
#include "ns_movement_state.h"
#ifndef NS_NO_SQL
#include "ns_image_server_sql.h"
#endif
#include "ns_death_time_annotation.h"
#include <set>
#include "ns_region_metadata.h"
#include "ns_image_pool.h"

#define NS_OUTPUT_MULTIWORM_STATS


struct ns_survival_timepoint_event_count{
	ns_survival_timepoint_event_count():from_multiple_worm_disambiguation(false),
		number_of_worms_in_by_hand_worm_annotation(0),
		number_of_worms_in_machine_worm_cluster(0){properties.excluded = ns_death_time_annotation::ns_not_excluded;}
	bool from_multiple_worm_disambiguation;
	ns_death_time_annotation properties;
	std::vector<ns_death_time_annotation> events;

	bool empty() const{return events.empty();}

	//we have already sorted annotations according to the number of worms in each annotation
	//this happened when the annotation was added to the appropriate ns_survival_timepoint_event_count structure.
	//thus we need not check the actual annotation to see how many events it records.
	//The properties label is more reliable than the annotation because it has been expliclty calculated elsewhere.
	unsigned long number_of_worms_in_annotation(const unsigned long i) const{
		//we can choose between the machine or the by hand annotation count.
		//we choose the by hand where possible.
		if (number_of_worms_in_by_hand_worm_annotation != 0)
			return number_of_worms_in_by_hand_worm_annotation;
		if (ns_trust_machine_multiworm_data || properties.is_censored()) //always trust machine mutliples for censoring counts.  only trust machine death multiples
																		//if explicitly requested
			return number_of_worms_in_machine_worm_cluster;
		else return 1;  //assume one worm per machine cluster
	}

	unsigned long number_of_worms_in_machine_worm_cluster,
				  number_of_worms_in_by_hand_worm_annotation;
	//unsigned long number_of_clusters_identified_by_machine,
	//			  number_of_clusters_identified_by_hand;
	/*unsigned long total_number_of_worms() const{
		if (total_number_of_by_hand_worms()!= 0)
			return total_number_of_by_hand_worms();
		if (ns_trust_machine_multiworm_data || properties.is_censored()) //always trust machine mutliples for censoring counts.  only trust machine death multiples
																		//if explicitly requested
			return total_number_of_machine_worms();
		else return number_of_clusters_identified_by_machine;  //assume one worm per machine cluster
	}*/

	inline bool is_an_unexcluded_singleton() const{
		return number_of_worms_in_by_hand_worm_annotation <= 1 &&
				(!ns_censor_machine_multiple_worms || number_of_worms_in_machine_worm_cluster <= 1 ) &&
				!properties.is_excluded() && !properties.is_censored();
	}

	
	inline bool is_an_unexcluded_multiple() const{
		return number_of_worms_in_by_hand_worm_annotation > 1 &&
				(!ns_censor_machine_multiple_worms || number_of_worms_in_machine_worm_cluster > 1) &&
				!properties.is_excluded();
	}
	inline bool is_appropriate_in_default_censoring_scheme(const ns_death_time_annotation::ns_exclusion_type & censor_type= ns_death_time_annotation::ns_censored) const{
		return !properties.is_excluded() 
				&& properties.is_censored() 
				&& properties.excluded != ns_death_time_annotation::ns_censored_at_end_of_experiment
				&& (
				properties.missing_worm_return_strategy == ns_death_time_annotation::default_missing_return_strategy()
				 && properties.multiworm_censoring_strategy == ns_death_time_annotation::default_censoring_strategy()
				 || properties.multiworm_censoring_strategy == ns_death_time_annotation::ns_by_hand_censoring )
				 &&
				(properties.excluded == censor_type 
				|| properties.excluded == ns_death_time_annotation::ns_censored
				|| censor_type== ns_death_time_annotation::ns_censored);
	}
	inline bool matches_exclusion_type(const ns_death_time_annotation::ns_exclusion_type & t) const{
		return  properties.excluded == t
				||
				ns_death_time_annotation::is_excluded(t) &&
				properties.is_excluded()
				||
				ns_death_time_annotation::is_censored(t) &&
				properties.is_censored();
	}
	
/*inline unsigned long mulitple_worm_disambiguation_total(bool a)const {
		unsigned long sum(0);
		for (unsigned int i = 0; i < events.size(); i++){
			if ( events[i].from_multiple_worm_disambiguation == a)
				sum += ((events[i].number_of_clusters_identified_by_hand*events[i].number_of_worms_in_by_hand_worm_annotation >
				events[i].number_of_clusters_identified_by_machine*events[i].number_of_worms_in_machine_worm_cluster)
				?events[i].number_of_clusters_identified_by_hand*events[i].number_of_worms_in_by_hand_worm_annotation:
				events[i].number_of_clusters_identified_by_machine*events[i].number_of_worms_in_machine_worm_cluster);
		}
		return sum;
	}*/
//private:
//	inline unsigned long total_number_of_machine_worms() const { return number_of_worms_in_machine_worm_cluster * number_of_clusters_identified_by_machine;}
//	inline unsigned long total_number_of_by_hand_worms() const { return number_of_worms_in_by_hand_worm_annotation * number_of_clusters_identified_by_hand;}
};
	
struct ns_metadata_worm_properties{
	ns_metadata_worm_properties():events(0),properties_override_set(false),control_group(-1), event_period_end_time(0), event_type(ns_number_of_event_types){}
	typedef enum{ns_long_distance_movement_cessation,ns_local_movement_cessation,ns_movement_based_death,ns_death_associated_expansion, ns_typeless_censoring_events,ns_best_guess_death,ns_number_of_event_types} ns_survival_event_type;
	long control_group;
	unsigned long event_period_end_time;
	ns_survival_event_type event_type;
	ns_death_time_annotation_flag flag;
	ns_death_time_annotation properties_override;
	bool properties_override_set;
	const ns_survival_timepoint_event_count * events;
	
	inline static std::string  event_type_to_string(const ns_survival_event_type & e){
			switch(e){
				case ns_long_distance_movement_cessation: return "Long Distance Movement Cessation";
				case ns_local_movement_cessation: return "Local Movement Cessation";
				case ns_movement_based_death:	return "Movement Cessation";
				case ns_best_guess_death:	return "Best Guess Death Time";
				case ns_death_associated_expansion:	return "Death-Associated Expansion";
				case ns_typeless_censoring_events:	return "Censoring Event";
				default: throw ns_ex("ns_region_metadata::event_type_to_string()::Unknown Event Type");
			}
	}

};

struct ns_genotype_db_info{
	std::string strain,
				genotype;
};
struct ns_genotype_db_internal_info{
	ns_genotype_db_internal_info(const long i, const std::string & g):id(i),genotype(g){}
	ns_genotype_db_internal_info():id(0){}
	long id;
	std::string	genotype;
};
#ifndef NS_NO_SQL
class ns_genotype_fetcher{

public:
	void load_from_db(ns_image_server_sql * sql,const bool load_all=false);
	void add_information_to_database(const std::vector<ns_genotype_db_info> & info,ns_image_server_sql * sql_);
	const std::string & genotype_from_strain(const std::string & strain,ns_image_server_sql * sql_) const;

private:
	typedef std::map<std::string,ns_genotype_db_internal_info> ns_genotype_list;
	mutable ns_genotype_list genotypes;
	std::string empty_string;
};
#endif
struct ns_survival_timepoint_event{
	ns_survival_timepoint_event() { events.reserve(50); }
	std::vector<ns_survival_timepoint_event_count> events;
	void add(const ns_survival_timepoint_event_count & );
	void add(const ns_survival_timepoint_event & e);
	void remove_purely_non_machine_events();
};

//ns_survival_timepoint_event operator+(const ns_survival_timepoint_event & a, const ns_survival_timepoint_event & b);

struct ns_survival_timepoint{
	ns_survival_timepoint():absolute_time(0), best_guess_deaths_disambiguated(false){}
	
	unsigned long absolute_time;
	ns_survival_timepoint_event movement_based_deaths,
		long_distance_movement_cessations,
		local_movement_cessations,
		death_associated_expansions,
		best_guess_deaths,	//note that immediately after loading, this structure will contain /both/ death movement based and death_associated death times.  This is then sorted out later.
		typeless_censoring_events;
	void add(const ns_survival_timepoint & t);
	bool best_guess_deaths_disambiguated;
};

bool operator<(const ns_survival_timepoint & a, const ns_survival_timepoint & b);

//ns_survival_timepoint operator+(const ns_survival_timepoint & a, const ns_survival_timepoint & b);

struct ns_survival_data_quantities{
	void set_as_zero(){count = by_hand_excluded_count = machine_excluded_count = censored_count= number_of_events_involving_multiple_worm_disambiguation = 0; mean = variance = percentile_90th = percentile_50th = percentile_10th = maximum = minimum = 0.0;}
	unsigned long count,
				  machine_excluded_count,
				  by_hand_excluded_count,
				  censored_count,
				  number_of_events_involving_multiple_worm_disambiguation;
	double mean,
		   variance,
		   percentile_90th,
		   percentile_50th,
		   percentile_10th,
		   maximum,
		   minimum;

	ns_survival_data_quantities scale(const double & d) const{
		ns_survival_data_quantities n(*this);
		n.mean/=d;
		n.maximum/=d;
		n.minimum/=d;
		n.percentile_10th/=d;
		n.percentile_50th/=d;
		n.percentile_90th/=d;
		n.variance/=(d*d);
		return n;
	}
	void add(const ns_survival_data_quantities & s);
	static void out_jmp_header(const std::string & type, std::ostream & o, const std::string & terminator = "\n");
	void out_jmp_data(const ns_region_metadata & metadata, std::ostream & o, const std::string & terminator = "\n") const;
	static void out_blank_jmp_data(std::ostream & o, const std::string & terminator = "\n");
};


struct ns_survival_data_summary{
	ns_region_metadata metadata;
	ns_survival_data_quantities long_distance_movement_cessation,
								local_movement_cessation,
								death;
	std::string to_xml() const;
	void from_xml(const ns_xml_simple_object & o);
	static const char * xml_tag(){return "suvival_data_summary";}

	static void out_jmp_header(const std::string & label,std::ostream & o, const std::string & terminator = "\n");
	void out_jmp_data(std::ostream & o, const std::string & terminator="\n") const;
	static void out_blank_jmp_data(std::ostream & o, const std::string & terminator="\n") ;
	void add(const ns_survival_data_summary & s);
	void set_stats_as_zero(){
		long_distance_movement_cessation.set_as_zero();
		local_movement_cessation.set_as_zero();
		death.set_as_zero();
	}
	ns_survival_data_summary divide_stats_by_count() const{
		ns_survival_data_summary t(*this);
		if (t.long_distance_movement_cessation.count!= 0)
			t.long_distance_movement_cessation = t.long_distance_movement_cessation.scale(1.0/t.long_distance_movement_cessation.count);
		if (t.local_movement_cessation.count!= 0)
			t.local_movement_cessation = t.local_movement_cessation.scale(1.0/t.local_movement_cessation.count);
		if (t.death.count!= 0)
			t.death = t.death.scale(1.0/t.death.count);
		return t;
	}
	ns_survival_data_summary multiply_stats_by_count() const{
		ns_survival_data_summary t(*this);
		t.long_distance_movement_cessation = t.long_distance_movement_cessation.scale(t.long_distance_movement_cessation.count);
		t.local_movement_cessation = t.local_movement_cessation.scale(t.local_movement_cessation.count);
		t.death = t.death.scale(t.death.count);
		return t;
	}
};


struct ns_mean_statistics{
	ns_mean_statistics():weighted_sum_of_death_times(0),number_of_deaths(0),mean_survival_including_censoring(0){}

	unsigned long long weighted_sum_of_death_times;
	unsigned long number_of_deaths,
				  number_censored;
	double mean_survival_including_censoring,
		   mean_survival_including_censoring_computed_with_log;

	double mean_excluding_censoring() const{
		//be a little fancy to avoid overflows going from unsigned long long to double2
		if (number_of_deaths == 0)
			return 0;
		return ((double)(weighted_sum_of_death_times/number_of_deaths)) + 
			(weighted_sum_of_death_times%number_of_deaths)/(double)number_of_deaths;
	}
	void clear(){
		weighted_sum_of_death_times = 0;
		number_of_deaths = 0;
		number_censored = 0;
		mean_survival_including_censoring = 0;
		mean_survival_including_censoring_computed_with_log = 0;
	}
};
struct ns_survival_statistics{
	ns_survival_statistics():number_of_plates(0){}
	ns_mean_statistics mean;
	ns_mean_statistics mean_excluding_tails;
	unsigned long number_of_plates;
	void clear(){
		number_of_plates = 0;
		mean.clear();
		mean_excluding_tails.clear();
	}
};
struct ns_multi_event_survival_statistics{
	ns_survival_statistics movement_based_death,
		long_distance_movement_cessation,
		local_movement_cessations,
		death_associated_expansion_start,
		best_guess_death;
};
struct ns_survival_data_with_censoring_timeseries{
	void resize(unsigned long i, const double v);
	void resize(unsigned long i);
	std::vector<unsigned long> cumlative_number_of_censoring_events,
							   cumulative_number_of_deaths,
							   number_of_animals_at_risk,
							   number_of_events,
							   number_of_censoring_events;
	unsigned long total_number_of_deaths,
				  total_number_of_censoring_events;
	unsigned long number_surviving_excluding_censoring(unsigned long i) const{
		return total_number_of_deaths - cumulative_number_of_deaths[i];
	}

	std::vector<double> risk_of_death_in_interval,
					    probability_of_surviving_up_to_interval,
						log_probability_of_surviving_up_to_interval;
	void calculate_risks_and_cumulatives();
};
struct ns_survival_data_with_censoring{
	ns_survival_data_with_censoring_timeseries data,
											   data_excluding_tails;
	void calculate_risks_and_cumulatives(){
		data.calculate_risks_and_cumulatives();
		data_excluding_tails.calculate_risks_and_cumulatives();
	}
};	
struct ns_multi_event_survival_data_with_censoring{
	ns_survival_data_with_censoring movement_based_death,
		long_distance_movement_cessation,
		local_movement_cessations,
		death_associated_expansion_start,
		best_guess_death;
};

class ns_survival_data{
public:
	void clear(){
		metadata.clear();
		timepoints.clear();
	}

	ns_region_metadata metadata;
	std::vector<ns_survival_timepoint> timepoints;
	

	ns_multi_event_survival_data_with_censoring risk_timeseries;
	ns_multi_event_survival_statistics survival_statistics;  //UNITS OF AGE ( time at age zero subtracted from event times)
	//void calculate_survival();
	
	void genenerate_survival_statistics();
	void detetermine_best_guess_death_times();
	void convert_absolute_times_to_ages();
	ns_survival_data(int a) {}	//required by memory pool
	ns_survival_data() {}	
	//ns_survival_data_summary produce_summary() const;
private:
	void generate_risk_timeseries(){
		generate_risk_timeseries(ns_movement_cessation,risk_timeseries.movement_based_death);
		generate_risk_timeseries(ns_translation_cessation,risk_timeseries.local_movement_cessations);
		generate_risk_timeseries(ns_fast_movement_cessation,risk_timeseries.long_distance_movement_cessation);
		generate_risk_timeseries(ns_death_associated_expansion_start, risk_timeseries.death_associated_expansion_start);
		generate_risk_timeseries(ns_death_associated_expansion_start, risk_timeseries.death_associated_expansion_start);
		//special case using ns_additional_worm_entry as a special flag to indicate choosing the best guess death time
		generate_risk_timeseries(ns_additional_worm_entry, risk_timeseries.best_guess_death);
	}
	void generate_risk_timeseries(const ns_movement_event & event_type,ns_survival_data_with_censoring & survival) const;
	
	void generate_survival_statistics(const ns_survival_data_with_censoring & survival_data,ns_survival_statistics & stats) const;
};


struct ns_lifespan_device_normalization_statistics{
	ns_lifespan_device_normalization_statistics():control_mean_fix_point(-1),
		additive_device_regression_coefficient(0),
		multiplicative_additive_device_regression_additive_coefficient(0),
		multiplicative_additive_device_regression_multiplicative_coefficient(0),
		multiplicative_device_regression_coefficient(0),external_control_mean_fix_point_specified(false),
		device_censoring_count_used(0),device_death_count_used(0), grand_strain_mean_used(0), device_strain_mean_used(0), is_a_control_plate(false){}

	ns_region_metadata strain_info;
	ns_survival_statistics grand_strain_mean,
							  device_strain_mean;
	double grand_strain_mean_used,
			device_strain_mean_used,
		   device_death_count_used,
		   device_censoring_count_used;
	
	bool external_control_mean_fix_point_specified;
	double  control_mean_fix_point;  //time in seconds
	double  additive_device_regression_coefficient,  //y(i,D) = ybar + betaD + e;
			multiplicative_additive_device_regression_additive_coefficient,//y(i,D) = ybar + betaD*ei
			multiplicative_additive_device_regression_multiplicative_coefficient,//y(i,D) = ybar + betaD*ei
			multiplicative_device_regression_coefficient; //y(i,D) = ybar*betaD*ei

	double calculate_additive_device_regression_residual(const double measurement) const;
	double calculate_multiplicative_device_regression_residual(const double measurement) const;
	double calculate_multiplicative_device_regression_residual_additive_offset(const double measurement) const;

	ns_death_time_annotation_time_interval calculate_additive_device_regression_residual(const ns_death_time_annotation_time_interval & e) const;
	ns_death_time_annotation_time_interval calculate_multiplicative_device_regression_residual(const ns_death_time_annotation_time_interval& e) const;
	ns_death_time_annotation_time_interval calculate_multiplicative_device_regression_residual_additive_offset(const ns_death_time_annotation_time_interval& e) const;

	bool is_a_control_plate;
	//bool external_fix_point_specified()const{return control_mean_external_fix_point >=0;}
};
struct ns_lifespan_device_normalization_statistics_for_device{
	ns_lifespan_device_normalization_statistics_for_device():device_had_control_plates(false) {}
//	std::vector<ns_lifespan_device_normalization_statistics *> control_strains;
	ns_survival_statistics device_control_plate_statistics;
	ns_lifespan_device_normalization_statistics regression_statistics;
	typedef  std::map<std::string,ns_lifespan_device_normalization_statistics> ns_strain_stat_list;
	ns_strain_stat_list strains;
	bool device_had_control_plates;

private:
	//bool control_specified_;
	//bool control_specified() const{
	//	return control_specified_;
	//}
};
struct ns_control_group_strain_assignment{
	std::string strain,
				device;
	std::set<unsigned long> control_group_memberships;
};

struct ns_control_group_plate_assignment{
	ns_64_bit plate_id;
	typedef std::set<unsigned long> ns_control_group_membership_list;
	ns_control_group_membership_list control_group_memberships;
};

struct ns_lifespan_device_normalization_statistics_set{
	//pair <device name, list of all statistics for all strains on the device>
	typedef std::map<std::string,ns_lifespan_device_normalization_statistics_for_device > ns_device_stats_list;
	ns_survival_statistics grand_control_mean;
	std::map<std::string,ns_survival_statistics> grand_strain_mean;
	ns_device_stats_list devices;
	ns_lifespan_device_normalization_statistics_set(const ns_movement_event & e):normalization_event_type(e), produce_identity(false){}
	ns_movement_event normalization_event_type;
	bool produce_identity;

	//each plate may be used as a control or a mutant strain.
	typedef std::vector<ns_control_group_strain_assignment> ns_control_group_strain_list;
	ns_control_group_strain_list control_group_strain_list;

	typedef std::map<ns_64_bit, ns_control_group_plate_assignment> ns_control_group_plate_list;
	ns_control_group_plate_list control_group_plate_list;


	/*void add(const ns_lifespan_device_normalization_statistics & s, const std::string & control_strain){
		for (ns_device_strain_stats::const_iterator p(s.device_strain_stats.begin()); p != s.device_strain_stats.end(); p++){
			ns_device_strain_stats::iterator q(device_strain_stats.find(p->first));
			if (q == device_strain_stats.end())
				q = device_strain_stats.insert(ns_device_strain_stats::value_type(p->first,ns_device_strain_stat_list())).first;
			q->second.insert(q->second.end(),p->second.begin(),p->second.end());
		}
	}*/
	static void output_JMP_header(std::ostream & o){
		//0
		o << "Device Name,Animal Details,Is Control Strain,Normalization Event,"
			//1
			"Total Deaths(All Animals), Total Censored(All Animals),"
			"Total Deaths (Excluding Tails), Total Censored (Excluding Tails),"
			//2
			"Mean Lifespan (All Animals; Including Censored),All Animals; Mean Lifespan (All Animals; Including Censored;Calculated via logarithm),Mean Lifespan (All Animals; Ignoring Censoring),"
			//3	
			"Mean Lifespan (Excluding Tails; Including Censored),Tail-excluded Mean Lifespan (Excluding Tails; Calculated via logarithm),Mean Lifespan (Excluding Tails; Ignoring Censoring),"
			" Mean Lifespan (Used), Total Deaths (Used),Total Censored(Used), "
			//4
			"Grand Strain Mean,Device Strain Mean,Control Mean Fix Point,Control Mean Fix Point Specified Externally,"
			//5
			"Additive Regression Constant,Multiplicative Regression Coefficient,"
			"Multiplicative-Additive Regression Additive Coefficient,Multiplicative-Additive Regression Multiplicative Coefficient\n";

	}
	void output_JMP_file(std::ostream & o, const double time_scale_factor){
		for (ns_device_stats_list::const_iterator device = devices.begin(); device != devices.end(); device++){
			for (ns_lifespan_device_normalization_statistics_for_device::ns_strain_stat_list::const_iterator strain =  device->second.strains.begin(); strain != device->second.strains.end(); strain++){
				//0
				o << device->first << "," 
					<< strain->second.strain_info.plate_type_summary() << ",";
				if (device->second.device_had_control_plates){
					if(strain->second.is_a_control_plate)
						o << "Control,";
					else o << " Not Control,";
				}
				else o << "No Control Plates on Device,";
				o << ns_movement_event_to_string(normalization_event_type) << ","
				//1
				<< strain->second.device_strain_mean.mean.number_of_deaths << ","
				<< strain->second.device_strain_mean.mean.number_censored << ","
				<< strain->second.device_strain_mean.mean_excluding_tails.number_of_deaths << ","
				<< strain->second.device_strain_mean.mean_excluding_tails.number_censored << ",";
				//2
				if (strain->second.device_strain_mean.mean.number_of_deaths == 0)
					o << ",,,";
				else {o
					<< strain->second.device_strain_mean.mean.mean_survival_including_censoring/time_scale_factor << ","
					<< strain->second.device_strain_mean.mean.mean_survival_including_censoring_computed_with_log/time_scale_factor << ","
					<< strain->second.device_strain_mean.mean.mean_excluding_censoring()/time_scale_factor << ",";
				}
				//3
				
				if (strain->second.device_strain_mean.mean_excluding_tails.number_of_deaths == 0)
					o << ",,,";
				else {
					o
					<< strain->second.device_strain_mean.mean_excluding_tails.mean_survival_including_censoring/time_scale_factor << ","
					<< strain->second.device_strain_mean.mean_excluding_tails.mean_survival_including_censoring_computed_with_log/time_scale_factor << ","
					<< strain->second.device_strain_mean.mean_excluding_tails.mean_excluding_censoring()/time_scale_factor << ",";
				}
				//4
				o << strain->second.device_strain_mean_used<< "," 
				<< strain->second.device_death_count_used << ","
				<< strain->second.device_censoring_count_used << ","
				//4
				<< strain->second.grand_strain_mean_used/time_scale_factor << ","
				<< strain->second.device_strain_mean_used/time_scale_factor << ",";
				o << strain->second.control_mean_fix_point/time_scale_factor << ",";
				o << (strain->second.external_control_mean_fix_point_specified?"Yes":"No") << ",";
				//5
				o << strain->second.additive_device_regression_coefficient/time_scale_factor << ","
					<< strain->second.multiplicative_device_regression_coefficient<< ","
					<< strain->second.multiplicative_additive_device_regression_additive_coefficient/time_scale_factor<< "," 
					<< strain->second.multiplicative_additive_device_regression_multiplicative_coefficient<< "\n" ;

				
			}
		}
	}
};
struct ns_device_temperature_normalization_data{
	ns_device_temperature_normalization_data():produce_identity_(false), external_fixed_control_mean_lifespan(0), fix_control_mean_lifespan(false){}
	void produce_identity(){produce_identity_ = true;}
	std::vector<ns_region_metadata> control_strains;
	ns_region_metadata::ns_strain_description_detail_type strain_description_detail_type;
	double external_fixed_control_mean_lifespan;
	bool fix_control_mean_lifespan;
#ifndef NS_NO_SQL
	void load_data_for_experiment(const unsigned long experiment_id,ns_sql & sql);
#endif
	bool identity() const{return produce_identity_;}

	private:
	bool produce_identity_;
};
struct ns_survival_data_resizer {
public:
	int parse_initialization(int dummy) { return 0; }
	template<class T>
	void resize_after_initialization(const int & t, T & d) { ; }
};

class ns_lifespan_experiment_set{
public:
	ns_lifespan_experiment_set():curves_on_constant_time_interval(false),
		normalization_stats_for_death(ns_movement_cessation),
		normalization_stats_for_translation_cessation(ns_translation_cessation),
		normalization_stats_for_fast_movement_cessation(ns_fast_movement_cessation){}
	void load_from_JMP_file(std::ifstream & in);
	void load_from_by_hand_lifespan_file(std::ifstream & in);

	~ns_lifespan_experiment_set() { clear(); }
		
	typedef enum {ns_seconds,ns_minutes,ns_hours,ns_days} ns_time_units;
	
	typedef enum {ns_include_tails,ns_exclude_tails} ns_tail_strategy;
	typedef enum {ns_include_censoring_data,ns_ignore_censoring_data} ns_censoring_strategy;

	typedef enum {ns_simple,ns_simple_with_control_groups,ns_detailed_compact,ns_detailed_with_censoring_repeats,ns_multiple_events} ns_output_file_type;

	typedef enum {ns_output_single_event_times,ns_output_event_intervals} ns_time_handing_behavior;

	typedef enum {ns_do_not_include_control_groups,ns_include_control_groups} ns_control_group_behavior;

	void generate_aggregate_risk_timeseries(const ns_region_metadata & m, bool filter_by_strain, const std::string& specific_device, const ns_64_bit& specific_region_id, ns_survival_data_with_censoring& best_guess_survival, ns_survival_data_with_censoring& movement_based_survival, ns_survival_data_with_censoring& death_associated_expansion_survival, std::vector<unsigned long> & t, bool use_external_time) const;

	static void out_detailed_JMP_header(const ns_time_handing_behavior & time_handling_behavior, std::ostream & o, const std::string & time_units,const std::string & terminator="\n");
	static void out_detailed_JMP_event_data(const ns_time_handing_behavior & time_handling_behavior,std::ostream & o, const ns_lifespan_device_normalization_statistics * regression_stats,const ns_region_metadata & metadata,const ns_metadata_worm_properties & prop,const double time_scaling_factor,const std::string & terminator="\n", const bool output_raw_data_as_regression=false,const bool output_full_censoring_detail=false);
	static void out_simple_JMP_header(const ns_time_handing_behavior & time_handling_behavior, const ns_control_group_behavior & control_group_behavior,std::ostream & o, const std::string & time_units, bool multiple_events,const std::string & terminator="\n");
	static void out_simple_JMP_event_data(const ns_time_handing_behavior & time_handling_behavior,const ns_control_group_behavior & control_group_behavior,const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_strategy,const ns_death_time_annotation::ns_multiworm_censoring_strategy & multiworm_censoring_strategy, const ns_death_time_annotation::ns_missing_worm_return_strategy & missing_worm_censoring_strategy, std::ostream & o, const ns_lifespan_device_normalization_statistics * regression_stats,const ns_region_metadata & metadata,const ns_metadata_worm_properties & prop,const double time_scaling_factor,const bool output_mulitple_events,const std::string & terminator="\n", const bool output_raw_data_as_regression=false);
	
	void output_JMP_file(const ns_death_time_annotation::ns_by_hand_annotation_integration_strategy & by_hand_strategy,const ns_time_handing_behavior & e, const ns_time_units & time_units,std::ostream & o,const ns_output_file_type& detail,const bool output_header=true) const ;

	//void output_xml_summary_file(std::ostream & o) const;
	//void output_JMP_summary_file(std::ostream & o) const ;
	void output_matlab_file(std::ostream & o) const ;
	void output_R_file(std::ostream & o)const ;


	void clear();

	void include_only_events_detected_by_machine();
	void generate_survival_statistics();
	
	void generate_aggregate_for_strain(const ns_region_metadata & m, ns_lifespan_experiment_set & r) const;
	#ifndef NS_NO_SQL
	void load_genotypes(ns_sql & sql);
	#endif
	void convert_absolute_times_to_ages();

	void generate_common_time_set(ns_lifespan_experiment_set & new_set) const;

	void force_common_time_set_to_constant_time_interval(const unsigned long interval_time_in_seconds,ns_lifespan_experiment_set & new_set) const;
	
	void compute_device_normalization_regression(const ns_device_temperature_normalization_data & data, const ns_censoring_strategy & s, const ns_tail_strategy & t);
	
	void group_strains(ns_lifespan_experiment_set & new_set) const;

	const std::vector<unsigned long> &common_time() const {if (common_time_.empty()) throw ns_ex("Not on common time!"); return common_time_;}
	bool set_is_on_common_time() const {return !common_time_.empty();}
	
	ns_lifespan_device_normalization_statistics_set normalization_stats_for_death,
													normalization_stats_for_translation_cessation,
													normalization_stats_for_fast_movement_cessation;
	ns_survival_data& curve(const std::size_t& i) { return *curves[i]; }
	const ns_survival_data& curve(const std::size_t& i) const { return *curves[i]; }
	const std::size_t size() const { return curves.size(); }
	void resize(const std::size_t& i) {
		if (i < curves.size()) {
			for (std::size_t j = i; i < curves.size(); j++)
				memory_pool.release(curves[i]);
			curves.resize(i);
		}
		else {
			std::size_t old_size = curves.size();
			curves.resize(i);
			for (std::size_t j = old_size; j < curves.size(); j++)
				curves[j] = memory_pool.get(0);
		}
	}
private:

	std::vector<ns_survival_data*> curves;

	void compute_device_normalization_regression(const ns_movement_event & event_type, const ns_device_temperature_normalization_data & data, ns_lifespan_device_normalization_statistics_set & set, const ns_censoring_strategy & s, const ns_tail_strategy & t);
	bool curves_on_constant_time_interval;
	std::vector<unsigned long> common_time_;
	std::vector<unsigned long> &common_time() {if (common_time_.empty()) throw ns_ex("Not on common time!"); return common_time_;}
	static ns_image_pool<ns_survival_data, ns_survival_data_resizer> memory_pool;
};

/*
class ns_survival_data_summary_aggregator{
public:
	ns_survival_data_summary_aggregator();

	typedef std::map<ns_movement_event,ns_survival_data_summary> ns_plate_normalization_list ;
	//sorted by normalization type
	typedef std::map<std::string, ns_plate_normalization_list > ns_plate_list;

	ns_plate_list plate_list;

	void add(const ns_movement_event normalization_type, const ns_survival_data_summary & summary);

	void add(const ns_movement_event normalization_type,const ns_lifespan_experiment_set & set);
	
	static void out_JMP_summary_data_header(std::ostream & o, const std::string & terminator);
	void out_JMP_summary_data(const ns_plate_list::const_iterator & region,std::ostream & o) const;
	static void out_JMP_empty_summary_data(std::ostream & o);

	void out_JMP_summary_file(std::ostream & o) const;
	static std::vector<ns_movement_event> events_to_output;
};
*/
#endif
