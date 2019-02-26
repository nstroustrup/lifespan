#ifndef NS_POSTURE_ANALYSIS_MODELS
#define NS_POSTURE_ANALYSIS_MODELS
#include "ns_ex.h"
#include "ns_get_double.h"
#include <algorithm>
#include <iostream>
template<class T>
class ns_emperical_cdf_generator{
public:
	void add_sample(const T & v){
		samples_.push_back(v);
		number_of_samples++;
	}
	ns_emperical_cdf_generator():number_of_samples(0){}
	void clear_samples(){samples_.clear();}
	void generate_ntiles_from_samples(unsigned long N){
		if (samples_.size() == 0)
			throw ns_ex("ns_emperical_cdf_generator::generate_ntiles()::No samples provided!"); 
		std::sort(samples_.begin(),samples_.end());
		if (samples_.size() <= N)
			N = samples_.size();
		
		ntiles.resize(N+1);
		float step(samples_.size()/(float)N);
		for (unsigned int i = 0; i < N; i++){
			const float id(step*i);
			const long idi((long)id);
			//linear interpolation to find better estimate of ntile for sparse data
			if (idi == samples_.size()-1)
				ntiles[i] = samples_[idi];
			else{
				const float fid(id-idi);
				ntiles[i] = static_cast<T>(samples_[idi]*(1.0-fid)+samples_[idi+1]*fid);
			}
		}
		ntiles[N] = samples_[samples_.size()-1];
		//samples_.clear();
//		calculate_equality_map();
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
		/*
		if (d == ntile[greater_ntile])
			return greater_ntile/(double)ntiles.size();
			*/
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
	const std::vector<T> & samples()const{return samples_;}

	template<class T2>
	double remove_mixture(const ns_emperical_cdf_generator<T2> & g, const T absolute_bottom = 0){
		if (samples_.size() == 0)
			throw ns_ex("remove_mixture()::Removing mixtures requires samples.");
		T cutoff_value(3*g.ntiles[g.ntiles.size()/2]);
		if (cutoff_value < absolute_bottom)
			cutoff_value = absolute_bottom;
		for (typename std::vector<T>::iterator p = samples_.begin(); p != samples_.end();){
			if (*p <= cutoff_value)
				p = samples_.erase(p);
			else p++;
		}
		return 0;
	}
private:
	std::vector<T> samples_;
	std::vector<T> ntiles;
	std::vector<double> ntile_numbers;
	unsigned long number_of_samples;
};
class ns_analyzed_image_time_path;
struct ns_emperical_posture_quantification_value_estimator{
	ns_emperical_cdf_generator<double> raw_moving_cdf,
							   dead_cdf,
							   processed_moving_cdf;
	inline double fraction_of_moving_values_less_than(const double & d) const{
		//alive if most alive animals move less than you
		return processed_moving_cdf.fraction_values_lower(d);
	}
	inline double fraction_of_dead_samples_greater_than(const double & d) const{
		//dead if most dead animals move more than you
		return 1.0-dead_cdf.fraction_values_lower(d);
	}
	void read(std::istream & moving_cdf_in,std::istream & dead_cdf_in);
	void write(std::ostream & moving_cdf_out,std::ostream & dead_cdf_out,std::ostream * visualization_file, const std::string & experiment_name = "") const;
	bool add_by_hand_data_to_sample_set(int software_version,ns_analyzed_image_time_path * path);
	void generate_estimators_from_samples();
	void write_samples(std::ostream & o,const std::string & experiment_name="");
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
