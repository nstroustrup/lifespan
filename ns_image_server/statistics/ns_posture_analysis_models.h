#ifndef NS_POSTURE_ANALYSIS_MODELS
#define NS_POSTURE_ANALYSIS_MODELS
#include "ns_ex.h"
#include "ns_get_double.h"
#include <algorithm>
#include <iostream>
#include "ns_analyzed_image_time_path_element_measurements.h"
template<class T>
class ns_emperical_cdf_generator{
public:
	template<class accessor_t>
	void generate_ntiles_from_samples(unsigned long Number_of_Ntiles, std::vector<T> & values, accessor_t accessor) {
		if (values.size() == 0)
			throw ns_ex("ns_emperical_cdf_generator::generate_ntiles()::No samples provided!");
		typedef std::result_of<accessor()> TT;

		std::vector<TT> sorted_values(values.size());
		for (unsigned long i = 0; i < values.size(); i++) 
			sorted_values[i] = accessor(values[i]);
		std::sort(samples.begin(),samples.end());
		if (samples.size() <= Number_of_Ntiles)
			Number_of_Ntiles = samples.size();
		
		ntiles.resize(N+1);
		float step(samples.size()/(float)Number_of_Ntiles);
		for (unsigned int i = 0; i < Number_of_Ntiles; i++){
			const float id(step*i);
			const long idi((long)id);
			//linear interpolation to find better estimate of ntile for sparse data
			if (idi == samples.size()-1)
				ntiles[i] = samples[idi];
			else{
				const float fid(id-idi);
				ntiles[i] = static_cast<T>(samples[idi]*(1.0-fid)+samples[idi+1]*fid);
			}
		}
		ntiles[N] = samples[samples.size()-1];
	}
	inline double fraction_values_lower(const double & d) const{
			//ntiles can have the same value, e.g deciles of [0,0,0,0,5,5,5,8,8,8]
		//if d = 0, then the greater ntile will be 5, but we shuld return the 
		//the first instance of 0--index 0--which is stored in the lesser equality map
		//lower bound finds the first value in the set >= d, which is the first one, which is what we want.
		//note we use end()-1 because that is the last ntile.  end() itself is the maximum, 
		typename std::vector<T>::const_iterator p(std::lower_bound(ntiles.begin(),ntiles.end()-1,d));
		if (p == ntiles.end()-1) //value is larger than the largest_quantile
			return 1.0;

		const ns_64_bit greater_ntile(p-ntiles.begin());
		if (greater_ntile == 0)
			return 0.0;
		const ns_64_bit lesser_ntile(greater_ntile-1);//lesser_equality_map[greater_ntile-1]);

		//calculate the fraction of the distance between neighboring ntiles
		const double f((d-ntiles[lesser_ntile])/(ntiles[greater_ntile]-ntiles[lesser_ntile]));
		return (lesser_ntile+f)/(double)(ntiles.size()-1);
	}
	void write(std::ostream & o) const{
		if (ntiles.size() == 0)
			throw ns_ex("ns_emperical_cdf_generator()::Attempting to write an empty cdf.");
		o << "ns_cdf: " << ntiles.size() << " ntiles generated from " << number_of_samples << " samples\n";
		for (unsigned int i = 0; i < ntiles.size(); i++)
			o << i<<","<<i/ntiles.size() << "," << ntiles[i] << "\n";
	}
	void read(std::istream & in){
		std::string tmp;
		unsigned int N;
		in >> tmp;
		if (tmp != "ns_cdf:")
			throw ns_ex("ns_emperical_cdf_generator()::read()::Invalid cdf file (1)");
		in >> N;
		if (N > 10000)
			throw ns_ex("ns_emperical_cdf_generator()::read()::Very large cdf specified");
		if (N == 0)
			throw ns_ex("ns_emperical_cdf_generator()::read()::Invalid cdf file (2)");
		ntiles.resize(N);
		getline(in,tmp);
		ns_get_double get_double;
		ns_get_int get_int;
		int i(0),ti(0);
		T d;
		while (!in.fail()){
			get_int(in,ti);
			if (in.fail())
				break;
			if (ti != i)
				throw ns_ex("ns_emperical_cdf_generator()::read()::Invalid cdf file(3)");
			if (i >= N)
				throw ns_ex("ns_emperical_cdf_generator()::read()::Too many levels in CDF file: ") << i;
			get_double(in,d); //discard this value
			get_double(in,d);
			ntiles[i] = d;
			i++;
		}
		if (i != N)
			throw ns_ex("ns_emperical_cdf_generator()::read()::CDF file contained only ") << i << " out of " << N << " levels";
	//	calculate_equality_map();
	}
	const std::vector<T> & ntile_values()const{return ntiles;}

private:
	std::vector<T> ntiles;
	std::vector<double> ntile_numbers;
	unsigned long number_of_samples;
};
class ns_analyzed_image_time_path;
struct ns_hmm_emission {
	ns_analyzed_image_time_path_element_measurements measurement;
	ns_stationary_path_id path_id;
};
struct ns_hmm_emission_normalization_stats {
	ns_analyzed_image_time_path_element_measurements path_mean, path_variance;
	ns_death_time_annotation source;
};
struct ns_hmm_emission_probability_estimator {
	//ns_emperical_cdf_generator ecdf;
};
struct ns_emperical_posture_quantification_value_estimator{
	std::map<ns_hmm_movement_state, std::vector<ns_hmm_emission> > observed_values;
	std::map<ns_stationary_path_id, ns_hmm_emission_normalization_stats > normalization_stats;


	void read_observation_data(std::istream & in);
	void write_observation_data(std::ostream & out,const std::string & experiment_name = "") const;
	bool add_by_hand_data_to_sample_set(const std::string &software_version, const ns_death_time_annotation & properties,const ns_analyzed_image_time_path * path);
	void read(std::istream & i);
	void write(std::ostream & o)const;
	void generate_estimators_from_samples();
	static ns_emperical_posture_quantification_value_estimator dummy();
private:
	void write_visualization(std::ostream & o,const std::string & experiment_name="") const;
};

struct ns_threshold_movement_posture_analyzer_parameters{
	double stationary_cutoff,
		posture_cutoff,
		death_time_expansion_cutoff;
	unsigned long permanance_time_required_in_seconds;

	unsigned long death_time_expansion_time_kernel_in_seconds;
	bool use_v1_movement_score;
	std::string version_flag;
	static ns_threshold_movement_posture_analyzer_parameters default_parameters(const unsigned long experiment_duration_in_seconds);
	void read(std::istream & i);
	void write(std::ostream & o)const;
};

struct ns_posture_analysis_model{
	typedef enum{ns_not_specified,ns_threshold,ns_hidden_markov,ns_unknown} ns_posture_analysis_method;
	static ns_posture_analysis_method method_from_string(const std::string & s){
		if (s.size() == 0)
			return ns_not_specified;
		if (s == "thresh")
			return ns_threshold;
		if (s == "hm" || s == "hmm")
			return ns_hidden_markov;
		return ns_unknown;
	}
	ns_emperical_posture_quantification_value_estimator hmm_posture_estimator;
	
	ns_threshold_movement_posture_analyzer_parameters threshold_parameters;

	static ns_posture_analysis_model dummy();
	ns_posture_analysis_method posture_analysis_method;
	std::string name;
};
#endif
