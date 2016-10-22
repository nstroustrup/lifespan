#ifndef NS_MODEL_SPECIFICATION
#define NS_MODEL_SPECIFICATION

#define NS_USE_MACHINE_LEARNING
#undef NS_USE_TINYSVM

#ifdef NS_USE_MACHINE_LEARNING
	#ifdef NS_USE_TINYSVM
		#include "tinysvm.h"
	#else
		#include "svm.h"
	#endif
#endif

#include "ns_simple_cache.h"
#include "ns_worm_detection_constants.h"
#undef min
#undef max
///Support std::vector Machines work best when all features are normalized such that
///all values fall in the range [0,1].  We need to do this normalization ourselves,
///and so when feeding the SVM learner we collect statistics on features
///to be used in normalization.  This information is stored in ns_detected_worm_feature_range
///structures
struct ns_detected_worm_classifier_range{
	ns_detected_worm_classifier_range():min(0),max(0),avg(0),std(0),specified(false){}
	double max,min,avg,std,worm_avg;
	bool specified;
};

///A list of statistics on each feature, indexed by classifier id (as specified by the feature enum)
typedef std::vector<ns_detected_worm_classifier_range> ns_statistic_ranges;

///One option is to transform features into new bases (perhas the basis of the principal
///components of the feature set.  The transformation matrix is stored in
///ns_principal_component_transformation_specification structures.
struct ns_principal_component_transformation_specification{
	void read(const std::string & filename);
	std::vector< std::vector<double> > pc_vectors;
};

///ns_model_specification contains all the information needed to specify the model file for a SVM model, including
///statistic normalization data and all support vectors.
struct ns_svm_model_specification{
	ns_svm_model_specification():statistics_ranges(ns_stat_number_of_stats),included_statistics(ns_stat_number_of_stats,0){
	#ifdef NS_USE_MACHINE_LEARNING
		#ifndef NS_USE_TINYSVM
			model = 0;
		#endif
		#endif
	}
	std::string model_name;
	#ifdef NS_USE_MACHINE_LEARNING
		#ifdef NS_USE_TINYSVM
			mutable TinySVM::Model model;
		#else
			mutable struct svm_model * model;
		#endif
	#endif

	ns_statistic_ranges statistics_ranges;
	ns_principal_component_transformation_specification pca_spec;

	#ifdef NS_USE_MACHINE_LEARNING
		#ifndef NS_USE_TINYSVM
	~ns_svm_model_specification(){
		if (model != 0){
			svm_destroy_model(model);
			model = 0;
		}
		statistics_ranges.clear();
	}
	#endif
	#endif

	///Saves statistics required for normalization of features
	void write_statistic_ranges(const std::string & filename, bool write_all_features=false);
	///Loads in statistics required for normalization of features.
	void read_statistic_ranges(const std::string & filename);

	///Models can explicitly ignore calculated features,
	///not supplying them to either the SVM learner or SFM classifier
	///read_excluded_stats reads in a file containing the specification
	///for excluded statistics (if any)
	std::vector<char> included_statistics;
	void read_excluded_stats(const std::string & filename);
	void read_included_stats(const std::string & filename);
};


struct ns_svm_model_specification_entry_source {
	std::string model_directory;
	//worm_detection_model_directory()
	void set_directory(const std::string long_term_storage_directory, const std::string & worm_detection_dir);
};

class ns_svm_model_specification_entry : public ns_simple_cache_data<std::string, struct ns_svm_model_specification_entry_source, std::string> {
public:
	ns_svm_model_specification model_specification;
	template <class a, class b, bool c>
	friend class ns_simple_cache;
private:
	ns_64_bit size_in_memory_in_kbytes() const { return 0; }
	void load_from_external_source(const std::string & name, ns_svm_model_specification_entry_source & external_source);
	std::string to_id(const std::string & name) const { return name; }
	const std::string & id() const { return model_specification.model_name; }
	void clean_up(ns_svm_model_specification_entry_source & external_source) {}
};

typedef ns_simple_cache<ns_svm_model_specification_entry, std::string, true> ns_worm_detection_model_cache;


#endif
