#ifndef NS_MACHINE_LEARNING_TRAINING_SET_H
#define NS_MACHINE_LEARNING_TRAINING_SET_H

#include "ns_detected_worm_info.h"
#include "ns_progress_reporter.h"
#include "ns_image_easy_io.h"
#include "ns_region_metadata.h"


class ns_learning_results_decision{
public:
	typedef enum{ns_tp,ns_tn,ns_fp,ns_fn} ns_decision_type;
	std::string filename;
	ns_region_metadata metadata;
	ns_detected_worm_stats stat;
	ns_decision_type decision;

	bool load(std::istream & filenames, std::istream & metadata,std::istream &data_set, std::istream & results);
	bool load_model_data(std::istream &data_set, std::istream & filenames, std::istream & metadata,bool & is_a_worm);
};

class ns_learning_results_decision_set{
public:
	std::vector<ns_detected_worm_stats *> true_positives, true_negatives,false_positives,false_negatives;

	void from_decisions(std::vector<ns_learning_results_decision> & decisions,const std::string & genotype_to_consider="");

	inline unsigned long total_neg(){ return (unsigned long)(false_positives.size() + true_negatives.size());}
	inline unsigned long total_pos(){ return (unsigned long)(false_negatives.size() + true_positives.size());}
	inline unsigned long total(){return total_neg() + total_pos();}

	void out_summary(std::ostream & o, const std:: string & genotype,const std::string & endline);


	void output_html_error_summary(std::ostream & out);

	void produce_frequency_distribution(const std::string & directory);

	private:
	void output_html_image_group(std::ostream & out, const std::vector<ns_detected_worm_stats *> & worms);
};

class ns_training_file_generator{
public:
	void generate_from_curated_set(const std::string & directory, const ns_svm_model_specification & model_specification, bool use_training_collages,ns_sql * sql_for_looking_up_genotypes);

	void plot_errors_on_freq(const std::string & results_filename);

	void re_threshold_training_set(const std::string &directory, const ns_svm_model_specification & model);

	void repair_xml_metadata(const std::string & directory_with_problems, const std::string & reference_directory, const std::string & output_directory);

	void lookup_genotypes(ns_sql & sql);

	void mark_duplicates_in_training_set(const std::string & filename);

	ns_vector_2i image_already_exists(unsigned int cur_frame, const ns_image_standard & im, const ns_vector_2i & im_pos, const std::vector<std::vector<ns_image_standard *> > & images, const std::vector<std::vector<ns_packed_collage_position> > & source_image_locations);

	void split_training_set_into_different_regions(const std::string & results_filename);

	private:

	std::string training_set_base_dir;

	unsigned int worm_test_set_start_index,
		non_worm_test_set_start_index;

	std::vector<ns_detected_worm_stats> worm_stats;
	std::vector<ns_detected_worm_stats> non_worm_stats;
	std::vector<std::string> censored_object_filenames;

	std::vector<ns_graph_object> worm_distributions;
	std::vector<ns_graph_object> non_worm_distributions;


	void add_object_to_training_set(const ns_svm_model_specification & model, const bool is_a_worm, const std::string & filename, const std::string & relative_filename, ns_detected_worm_info & worm, const long long & region_id);

	void output_image_stats(std::ostream & out_training_data, const bool is_a_worm, ns_detected_worm_stats & stats);


	void re_threshold_image(const std::string & fname,const bool take_best_spine_solution, const ns_svm_model_specification & model);
	void re_threshold_image(ns_image_standard & im,std::vector<ns_image_standard> & new_test_images, const bool take_best_spine_solution, const ns_svm_model_specification & model);
	
	void output_training_set_excluding_stats(std::vector<ns_detected_worm_stats> & worm_stats, std::vector<ns_detected_worm_stats> & non_worm_stats, const std::vector<ns_detected_worm_classifier> & excluded_stats, const ns_svm_model_specification & model, const std::string & base_directory,const std::string & genotype_specific_directory,const std::string & base_filename);
	void output_training_set_including_stats(std::vector<ns_detected_worm_stats> & worm_stats, std::vector<ns_detected_worm_stats> & non_worm_stats, const std::vector<ns_detected_worm_classifier> & included_stats, const ns_svm_model_specification & model, const std::string & base_directory,const std::string & genotype_specific_directory,const std::string & base_filename);
	std::vector<std::string> genotypes_in_set;

};
#endif
