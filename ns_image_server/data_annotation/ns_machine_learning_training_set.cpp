#include "ns_machine_learning_training_set.h"

#include "ns_detected_worm_info.h"
#include "ns_progress_reporter.h"
#include "ns_image_easy_io.h"
#include "ns_worm_detector.h"
#include "ns_difference_thresholder.h"
#include "ns_worm_training_set_image.h"

using namespace std;

std::string ns_remove_bad_filename_characters(const std::string & st,char rep = '_')  //from somewhere on stackoverflow
{
	std::string s(st);
    string::iterator it;
    for (it = s.begin() ; it < s.end() ; ++it){
        switch(*it){
        case '/':case '\\':case ':':case '?':case '"':case '<':case '>':case '|':
            *it = rep;
        }
    }
	return s;
}

bool ns_learning_results_decision::load(istream & filenames, istream & metadata,istream &data_set, istream & results){

	//load model data
	bool is_a_worm;
	if (!load_model_data(data_set,filenames,metadata,is_a_worm))
		return false;

	//load SVM result
	int result;
	results >> result;
	if (results.fail()) throw ns_ex("Invalid results file");
	
	if (is_a_worm){
		if(result == 1)	
			decision=ns_tp;
		else			
			decision=ns_fn;
	}
	else{
		if(result == 1)
			decision=ns_fp;
		else			
			decision=ns_tn;
	}
	return true;
}
bool ns_learning_results_decision::load_model_data(istream &data_set, istream & filenames, istream & metadata_f, bool & is_a_worm){
	//load filename
	filenames >> filename;
	if (filenames.fail()) return false;
	stat.debug_filename = filename;

	//load metadata
	metadata_f >> metadata.genotype;
	if (metadata_f.fail()) return false;

	std::string is_a_worm_str;
	data_set >> is_a_worm_str;
	if (data_set.fail()) 
		return false;
	if (is_a_worm_str == "+1") is_a_worm = true;
	else if (is_a_worm_str == "-1") is_a_worm = false;
	else throw ns_ex("Invalid data file(2)");
	stat.from_normalized_string(data_set);
	if (is_a_worm) decision=ns_tp;
	else decision=ns_tn;
	if (data_set.fail()) //we hit the end of the file while reading in the last sa
		return false;
	return true;
}

std::vector<ns_detected_worm_stats *> true_positives, true_negatives,false_positives,false_negatives;
void ns_learning_results_decision_set::from_decisions(std::vector<ns_learning_results_decision> & decisions,const std::string & genotype){
	for (unsigned int i = 0; i < decisions.size(); i++){
		if (genotype!="" & decisions[i].metadata.genotype != genotype)
			continue;
		switch(decisions[i].decision){
			case ns_learning_results_decision::ns_tp: true_positives.push_back(&decisions[i].stat); break;
			case ns_learning_results_decision::ns_tn: true_negatives.push_back(&decisions[i].stat); break;
			case ns_learning_results_decision::ns_fp: false_positives.push_back(&decisions[i].stat); break;
			case ns_learning_results_decision::ns_fn: false_negatives.push_back(&decisions[i].stat); break;
			default: throw ns_ex("Unknown decisision: ") << decisions[i].decision;
		}
	}
}

void ns_learning_results_decision_set::out_summary(ostream & o, const std:: string & genotype="",const std::string & endline="\n"){
	o << "Results for genotype \"" << genotype << "\"";
	if (total() != 0)
		o << "Accuracy = " << total()-false_positives.size() - false_negatives.size() << " / " << total()
		  << " = " << 100.0 - (100.0*false_positives.size() + 100.0*false_negatives.size())/(total()) << "%" << endline;
	if (total_neg() != 0)
		o << "FP = " << false_positives.size() << "/" << total_neg() << " = " << (100.0*false_positives.size())/total_neg() << "%" << endline;
	else o << "No Positive Examples" << endline;
	if (total_pos() != 0)
		o << "FN = " << false_negatives.size() << "/" << total_pos() << " = " << (100.0*false_negatives.size())/total_pos() << "%" << endline;
	else o << "No Negative Examples" << endline;
	
}


void ns_learning_results_decision_set::output_html_error_summary(ostream & out){
	out << "<HTML><HEAD></HEAD><BODY bgcolor=\"#000000\" text=\"#FFFFFF\">";
	out << "<h1><font color=\"#CCCCFF\">Summary</font></h1>";
	out_summary(out,"<BR>\n");
	out << "<h1><font color=\"#CCCCFF\">False Positives</font></h1>";
	output_html_image_group(out,false_positives);
	out << "<h1><font color=\"#CCCCFF\">False Negatives</font></h1>";
	output_html_image_group(out,false_negatives);
	out << "<h1><font color=\"#CCCCFF\">True Positives</font></h1>";
	output_html_image_group(out,true_positives);
	out << "<h1><font color=\"#CCCCFF\">True Negatives</font></h1>";
	output_html_image_group(out,true_negatives);
	out << "</BODY></HTML>";
}

void ns_learning_results_decision_set::produce_frequency_distribution(const std::string & directory){
	for (unsigned int s = 0; s < (unsigned int)ns_stat_number_of_stats; s++){
		try{
			ns_graph_object fp(ns_graph_object::ns_graph_dependant_variable),
							fn(ns_graph_object::ns_graph_dependant_variable),
							tp(ns_graph_object::ns_graph_dependant_variable),
							tn(ns_graph_object::ns_graph_dependant_variable);

			for (unsigned int i = 0; i < false_positives.size(); i++) fp.y.push_back((*false_positives[i])[(ns_detected_worm_classifier)s]);
			for (unsigned int i = 0; i < false_negatives.size(); i++) fn.y.push_back((*false_negatives[i])[(ns_detected_worm_classifier)s]);			
			for (unsigned int i = 0; i < true_positives.size(); i++) tp.y.push_back((*true_positives[i])[(ns_detected_worm_classifier)s]);
			for (unsigned int i = 0; i < true_negatives.size(); i++) tn.y.push_back((*true_negatives[i])[(ns_detected_worm_classifier)s]);			

			fp.properties.area_fill.color = ns_color_8(125,0,0);
			fp.properties.point.draw = false;
			fp.properties.line.draw = true;
			fp.properties.line.width = 1;
			fp.properties.line.opacity = 1;
			fp.properties.line.color = ns_color_8(0,0,0);
			fp.properties.area_fill.draw = true;
			fp.properties.area_fill.opacity = .5;
			fp.properties.line_hold_order = ns_graph_properties::ns_zeroth_centered;
			fp.properties.draw_vertical_lines = ns_graph_properties::ns_full_line;

			fn.properties = fp.properties;
			fn.properties.area_fill.color=ns_color_8(0,125,0);

			tp.properties = fp.properties;
			tp.properties.area_fill.color=ns_color_8(10,255,50);

			tn.properties = fp.properties;
			tn.properties.area_fill.color=ns_color_8(255,10,50);

			std::vector<const ns_graph_object *> graph_objects;
			std::vector<double> normalization;
			if (tp.y.size() > 0){
				graph_objects.push_back(&tp);
				normalization.push_back(1.0);
			}
			if (tn.y.size() > 0){
				graph_objects.push_back(&tn);
				normalization.push_back(1.0);
			}
			if (fp.y.size() != 0){
				graph_objects.push_back(&fp);
				normalization.push_back(-(1.0*fp.y.size()/total_pos()));
			}
			if (fn.y.size() != 0){
				graph_objects.push_back(&fn);
				normalization.push_back(-(1.0*fn.y.size()/total_neg()));
			}
			ns_graph graph;
			ns_graph_axes axes(graph.add_frequency_distribution(graph_objects,normalization,true));
			graph.x_axis_properties.text_decimal_places=2;
			graph.y_axis_properties.text_decimal_places=2;
			ns_image_standard freq_graph;
			ns_image_properties prop;
			prop.width = 900;
			prop.height = 600;
			prop.components = 3;
			freq_graph.prepare_to_recieve_image(prop);
			graph.set_graph_display_options(ns_classifier_label((ns_detected_worm_classifier)s),axes);
			graph.draw(freq_graph);

			std::string filename = directory + "\\" + ns_classifier_abbreviation((ns_detected_worm_classifier)s) + ".tif";
			std::string filename_vector = directory + "\\" + ns_classifier_abbreviation((ns_detected_worm_classifier)s) + ".svg";
			ofstream o(filename_vector.c_str());
			if (o.fail())
				throw ns_ex("Could not open for writing:") << filename_vector;
			graph.draw(o);
			o.close();
			ns_tiff_image_output_file<ns_8_bit> im_out;
			ns_image_stream_file_sink<ns_8_bit > file_sink(filename,im_out,1.0,128);
			freq_graph.pump(file_sink,128);
		}
		catch(ns_ex & ex){
			cerr << "Error while processing " << ns_classifier_label((ns_detected_worm_classifier)s) << ":" << ex.text() << "\n";
		}
	}
}
void ns_learning_results_decision_set::output_html_image_group(ostream & out, const std::vector<ns_detected_worm_stats *> & worms){
	if (worms.size() == 0)
		out << "(None)";
	unsigned int col_w(8);
	for (unsigned int i = 0; i < worms.size(); i++){
		std::string debug_filename = worms[i]->debug_filename;
		if (debug_filename.size() != 0 && debug_filename[0] == '-' || debug_filename[0] == '+')
			debug_filename = debug_filename.substr(1);
		std::string::size_type e = debug_filename.find_last_of("_");
		if (e == debug_filename.npos)
			throw ns_ex("Invalid formatting on filename (_) ") << debug_filename;
		std::string collage_filename(debug_filename.substr(4,e-4));
		collage_filename+=".tif";
		out << "<a href=\"" << collage_filename << "\" border=0><img src=\"" << debug_filename << "\"></a>\n";
		
		//if (i%col_w == 0 && i!=0) out << "<BR>";
	}
	out << "<BR>";
}

void ns_calc_max_min_by_cropping_outliers(const std::vector<ns_detected_worm_stats *> stats, unsigned long stat_id,double & mmax, double & mmin, double avg, double & std){
	mmin = 0;
	mmax = 0;
	std = 0;
	avg = 0;
	std::vector<double> d(stats.size());
	for (unsigned int i = 0; i < stats.size(); i++)
		d[i] = (*stats[i])[stat_id];
	std::sort(d.begin(),d.end());
	long c((long)d.size()/20);
	long count = 0;
	for (long i = c; i < (long)d.size()-c; i++){
		avg+=d[i];
		count++;
	}
	if (count == 0) return;
	mmin = d[c];
	mmax = d[(long)d.size()-c];
	avg/=count;
	for (int i = c; i < (int)d.size()-c; i++){
		std+=(d[i]-avg)*(d[i]-avg);
	}
	std = sqrt(std/count);
}

void ns_training_file_generator::repair_xml_metadata(const std::string & directory_with_problems, const std::string & reference_directory, const std::string & output_directory){
	ns_dir problem_dir;	
	problem_dir.load_masked(directory_with_problems,"tif",problem_dir.files);
	ns_dir reference_dir;	
	reference_dir.load_masked(reference_directory,"tif",reference_dir.files);
	for (unsigned int i = 0; i < problem_dir.files.size(); i++){
		bool found_match(false);
		for (unsigned int j= 0; j < reference_dir.files.size(); j++){
			if (reference_dir.files[j] == problem_dir.files[i]){
				found_match = true;
				break;
			}
		}
		if (!found_match)
			throw ns_ex("Could not find match for ") << problem_dir.files[i];
	}
	ns_image_standard im;
	im.use_more_memory_to_avoid_reallocations(true);

	for (unsigned int i = 0; i < problem_dir.files.size(); i++){
		bool found_match(false);
		for (unsigned int j= 0; j < reference_dir.files.size(); j++){
			if (reference_dir.files[j] == problem_dir.files[i]){
				ns_load_image(directory_with_problems + DIR_CHAR_STR + problem_dir.files[i],im);
				ns_tiff_image_input_file<ns_8_bit> reference_in;
				reference_in.open_file(reference_directory + DIR_CHAR_STR + reference_dir.files[j]);
				if (im.properties().description.size() > reference_in.properties().description.size())
					throw ns_ex("Found cropped metadata in the reference file ") << reference_dir.files[j];
				if (im.properties().description.size() < reference_in.properties().description.size()){
					cerr << "Found cropped metadata in " << problem_dir.files[i] << ".  Replacing with reference metadata.\n";
					im.set_description(reference_in.properties().description);
				}
				ns_save_image(output_directory + DIR_CHAR_STR + reference_dir.files[j],im);
				found_match = true;
				break;
			}
		}
		if (!found_match)
			throw ns_ex("Could not find match for ") << problem_dir.files[i];
	}
}

void ns_training_file_generator::generate_from_curated_set(const std::string & directory, const  ns_svm_model_specification & model_specification, bool use_training_collages=true,ns_sql * sql_for_looking_up_genotypes=0){
	std::string source_dir = ns_dir::extract_path(directory);
	cerr << "Loading training set images from " << directory << "...\n";
	ns_dir dir;	
	dir.load_masked(source_dir,"tif",dir.files);
	cerr << "Found " << dir.files.size() << " files\n";
	std::sort(dir.files.begin(),dir.files.end());

	std::string analysis_dir = source_dir + DIR_CHAR_STR + "analysis";
	training_set_base_dir = analysis_dir + DIR_CHAR_STR + "training_sets";
	std::string feature_dir = analysis_dir + DIR_CHAR_STR + "feature_data";
	std::string vis_path = analysis_dir + DIR_CHAR_STR + "vis";
	
	ns_dir::create_directory_recursive(analysis_dir);
	ns_dir::create_directory_recursive(training_set_base_dir);
	ns_dir::create_directory_recursive(feature_dir);
	ns_dir::create_directory_recursive(vis_path);
	ns_dir::create_directory_recursive(vis_path + DIR_CHAR_STR + "worms");
	ns_dir::create_directory_recursive(vis_path + DIR_CHAR_STR + "non_worms");
	ns_dir::create_directory_recursive(vis_path + DIR_CHAR_STR + "censored");
	
	/*
	std::string training_basename =  base_dir + DIR_CHAR_STR + "all_train",
			   test_basename = base_dir + DIR_CHAR_STR + "all_test",
			   training_filenames_basename = base_dir + DIR_CHAR_STR + "all_filenames_train",
			   test_filenames_basename = base_dir + DIR_CHAR_STR + "all_filenames_test";
			   */
	ns_svm_model_specification spec;
	std::string pca_spec_file = analysis_dir + DIR_CHAR_STR + "all_pca_spec.txt";
	if (ns_dir::file_exists(pca_spec_file)){
		spec.pca_spec.read(pca_spec_file);
		cout << "Using Principal Component Transformation\n";
		//training_basename =  base_dir + DIR_CHAR_STR + "all_pca_test";
		//test_basename = base_dir + DIR_CHAR_STR + "all_pca_train";
	}

	//reset all ranges to zero (we are going to recalculate the ranges from raw data)
	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){
		spec.statistics_ranges[s].max = 0;
		spec.statistics_ranges[s].min = 0;
		spec.statistics_ranges[s].std = 0;
		spec.statistics_ranges[s].avg = 0;
		spec.statistics_ranges[s].worm_avg = 0;
		spec.statistics_ranges[s].specified = false;
	}
		

	if (use_training_collages){
		std::string log_filename = analysis_dir + DIR_CHAR_STR + "log.txt";
		ofstream log(log_filename.c_str());
		if (log.fail())
			throw ns_ex("Could not open logfile ") << log_filename;
		ns_progress_reporter pr(dir.files.size(),10);
		for (unsigned int i = 0; i < dir.files.size(); i++){	
			cerr << i/(double)dir.files.size() << " : ";
			std::string collage_fname = source_dir + DIR_CHAR_STR + dir.files[i];
			log << ns_format_time_string_for_human(ns_current_time())  << "\t" << dir.files[i] << ":: ";
			cerr << ns_format_time_string_for_human(ns_current_time())  << "\t" << dir.files[i] << ":: ";
			log.flush();
			try{
			
				ns_image_standard collage;
				ns_load_image(collage_fname,collage);


				ns_annotated_training_set training_set;
				ns_worm_training_set_image::decode(collage,training_set);
				
				std::string metadata_filename = feature_dir + DIR_CHAR_STR + ns_dir::extract_filename(ns_dir::extract_filename_without_extension(collage_fname)) + "_tr.txt";
				ofstream metadata_out(metadata_filename.c_str());
				//write the quantitiative statistics calculated for each worm, visual inspection if needed.  This file format is not needed by the SVM
				ns_detected_worm_stats::output_csv_header(metadata_out);
				metadata_out << "\n";
				for (unsigned int j = 0; j < training_set.objects.size(); j++){
					training_set.objects[j].object.generate_stats().output_csv_data(training_set.objects[j].region_info_id,	
																	training_set.objects[j].capture_time,	
																	training_set.objects[j].object.region_position_in_source_image,	
																	training_set.objects[j].object.region_size,
																	training_set.objects[j].hand_annotation_data,metadata_out);
					metadata_out << "\n";
				}
				metadata_out.close();
				cerr << "Found " << training_set.worms.size() << " worms, " << training_set.non_worms.size() << " non-worms, and " << training_set.censored_worms.size() << " censored worms out of " << training_set.objects.size() << " objects.\n";

				//for each tye of object, add its statistics and info to a big list
				for (unsigned int j = 0; j < training_set.worms.size(); j++){
					std::string jpg_filename = ns_dir::extract_filename_without_extension(dir.files[i]) + "_"+ ns_to_string(j);
					std::string rel_fname = std::string("vis")	+ DIR_CHAR_STR + "worms" + DIR_CHAR_STR + jpg_filename;
					std::string abs_fname =	vis_path			+ DIR_CHAR_STR + "worms" + DIR_CHAR_STR + jpg_filename;
					add_object_to_training_set(spec,true,collage_fname,rel_fname + ".jpg",training_set.worms[j]->object,training_set.worms[j]->region_info_id);	
					ns_save_image(abs_fname + ".jpg",training_set.worms[j]->object.absolute_grayscale());
					ns_save_image(abs_fname + "_rel.jpg",training_set.worms[j]->object.relative_grayscale());
				}	
				for (unsigned int j = 0; j < training_set.non_worms.size(); j++){
					std::string jpg_filename = ns_dir::extract_filename_without_extension(dir.files[i]) + "_"+ ns_to_string(j);
					std::string rel_fname = std::string("vis")	+ DIR_CHAR_STR + "non_worms" + DIR_CHAR_STR  + jpg_filename;
					std::string abs_fname =			vis_path	+ DIR_CHAR_STR + "non_worms" + DIR_CHAR_STR  + jpg_filename;
					add_object_to_training_set(spec,false,collage_fname,rel_fname + ".jpg",training_set.non_worms[j]->object,training_set.non_worms[j]->region_info_id);	
					ns_save_image(abs_fname + ".jpg",training_set.non_worms[j]->object.absolute_grayscale());
					ns_save_image(abs_fname + "_rel.jpg",training_set.non_worms[j]->object.relative_grayscale());
				}
				for (unsigned int j = 0; j < training_set.censored_worms.size(); j++){
					std::string jpg_filename = ns_dir::extract_filename_without_extension(dir.files[i])  + "_"+ ns_to_string(j);
					std::string rel_fname = std::string("vis")	+ DIR_CHAR_STR + "censored" + DIR_CHAR_STR  + jpg_filename;
					std::string abs_fname = vis_path			+ DIR_CHAR_STR + "censored" + DIR_CHAR_STR  + jpg_filename;
					censored_object_filenames.push_back(rel_fname + ".jpg");
					ns_save_image(abs_fname + ".jpg",training_set.censored_worms[j]->object.absolute_grayscale());
					ns_save_image(abs_fname + "_rel.jpg",training_set.censored_worms[j]->object.relative_grayscale());
				}
				log << "Processed.\n";
				log.flush();
				cerr << "Processed.\n";
			}
			catch(ns_ex & ex){
				log << "Error: " << ex.text() << "\n";
				log.flush();
				cerr <<"Error: " << ex.text() << "\n";
			}
		}
		pr(dir.files.size());
		log.close();
	}
	
	if(worm_stats.size() == 0 || non_worm_stats.size() == 0)
		throw ns_ex("Lacking data for one class of objects: ") << (unsigned long)worm_stats.size() << " worms, " << (unsigned long)non_worm_stats.size() << " non worms.";

	cout << "\nCalculating Ranges...";
	//calculate range and average for all statistics
	std::vector<ns_detected_worm_stats *> all_stats;
	all_stats.reserve(worm_stats.size() + non_worm_stats.size());
	for (unsigned int i = 0; i < worm_stats.size(); i++)
		all_stats.push_back(&worm_stats[i]);
	for (unsigned int i = 0; i < non_worm_stats.size(); i++)
		all_stats.push_back(&non_worm_stats[i]);

	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){
		ns_calc_max_min_by_cropping_outliers(all_stats,s,spec.statistics_ranges[s].max,spec.statistics_ranges[s].min,spec.statistics_ranges[s].avg,spec.statistics_ranges[s].std);
		spec.statistics_ranges[s].worm_avg = 0;
	}
	for (unsigned int i = 0; i < worm_stats.size(); i++){
		for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){
			spec.statistics_ranges[s].worm_avg+=worm_stats[i][s];
		}
	}
	
	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){
		spec.statistics_ranges[s].specified = true;
		if (worm_stats.size() != 0)
			spec.statistics_ranges[s].worm_avg/=worm_stats.size();
	}
	
	//generate training sets using various subsets of the features calculated.

	worm_test_set_start_index = (unsigned int)(worm_stats.size()*3)/5;
	non_worm_test_set_start_index = (unsigned int)(non_worm_stats.size()*3)/5;

	//ensure independant segregation of objects into test and training sets.
	std::random_shuffle(worm_stats.begin(),worm_stats.end());
	std::random_shuffle(non_worm_stats.begin(),non_worm_stats.end());

	if (sql_for_looking_up_genotypes != 0)
		lookup_genotypes(*sql_for_looking_up_genotypes);

	std::vector<ns_detected_worm_classifier> excluded_stats;
	const std::string strain_specific_directory(training_set_base_dir + DIR_CHAR_STR + "strain_specific");
	output_training_set_excluding_stats(worm_stats,non_worm_stats,excluded_stats,spec, training_set_base_dir,strain_specific_directory, "all");

	//without intensity
	for (ns_detected_worm_classifier c = ns_stat_relative_intensity_average; c<=ns_stat_relative_intensity_containing_image_region_average; c = (ns_detected_worm_classifier)((int)c+1)) excluded_stats.push_back(c);
	for (ns_detected_worm_classifier c = ns_stat_absolute_intensity_average; c<=ns_stat_absolute_intensity_containing_image_region_average; c = (ns_detected_worm_classifier)((int)c+1))excluded_stats.push_back(c);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"no_unnormalized_intensity");
	
	for (ns_detected_worm_classifier c = ns_stat_absolute_intensity_normalized_average; c<=ns_stat_absolute_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1))excluded_stats.push_back(c);
	for (ns_detected_worm_classifier c = ns_stat_relative_intensity_normalized_average; c<=ns_stat_relative_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1))excluded_stats.push_back(c);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"no_intensity");

	excluded_stats.resize(0);
	for (ns_detected_worm_classifier c = ns_stat_relative_intensity_variance; c<=ns_stat_relative_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1))excluded_stats.push_back(c);
	for (ns_detected_worm_classifier c = ns_stat_absolute_intensity_variance; c<=ns_stat_absolute_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1))excluded_stats.push_back(c);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"only_simple_intensity");

	excluded_stats.resize(0);
	for (ns_detected_worm_classifier c = ns_stat_absolute_intensity_normalized_average; c<=ns_stat_absolute_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1))
		excluded_stats.push_back(c);
	for (ns_detected_worm_classifier c = ns_stat_relative_intensity_normalized_average; c<=ns_stat_relative_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1))
		excluded_stats.push_back(c);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory, "no_normalized_intensity");

	//without widths
	excluded_stats.resize(0);
	excluded_stats.push_back(ns_stat_width_at_center);
	excluded_stats.push_back(ns_stat_width_at_end_0);
	excluded_stats.push_back(ns_stat_width_at_end_1);
	excluded_stats.push_back(ns_stat_spine_length_to_max_width_ratio);
	excluded_stats.push_back(ns_stat_spine_length_to_average_width);
	excluded_stats.push_back(ns_stat_end_width_to_middle_width_ratio_0);
	excluded_stats.push_back(ns_stat_end_width_to_middle_width_ratio_1);
	excluded_stats.push_back(ns_stat_end_width_ratio);
	excluded_stats.push_back(ns_stat_spine_length_to_average_width);
	excluded_stats.push_back(ns_stat_spine_length_to_average_width);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"only_max_average_width");

	excluded_stats.push_back(ns_stat_max_width);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"only_average_width");

	excluded_stats.push_back(ns_stat_average_width);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"no_width");

	//without end information
	excluded_stats.resize(0);
	excluded_stats.push_back(ns_stat_width_at_end_0);
	excluded_stats.push_back(ns_stat_width_at_end_1);
	excluded_stats.push_back(ns_stat_end_width_to_middle_width_ratio_0);
	excluded_stats.push_back(ns_stat_end_width_to_middle_width_ratio_1);
	excluded_stats.push_back(ns_stat_end_width_ratio);
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"no_ends");

	//only simple stats
	excluded_stats.resize(0);
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		if (i == ns_stat_relative_intensity_average ||
			i == ns_stat_absolute_intensity_average ||
			i == ns_stat_bitmap_height || 
			i == ns_stat_bitmap_width || 
			i == ns_stat_pixel_area) continue;
		excluded_stats.push_back((ns_detected_worm_classifier)i);
	}
	output_training_set_excluding_stats(worm_stats, non_worm_stats, excluded_stats,spec, training_set_base_dir,strain_specific_directory,"simple");

	
	std::vector<ns_detected_worm_classifier> stats_to_use;
	stats_to_use.push_back(ns_stat_pixel_area);
	stats_to_use.push_back(ns_stat_bitmap_width);							
	stats_to_use.push_back(ns_stat_bitmap_height);							
	stats_to_use.push_back(ns_stat_bitmap_diagonal);						
	stats_to_use.push_back(ns_stat_spine_length);							
	stats_to_use.push_back(ns_stat_distance_between_ends);					
	stats_to_use.push_back(ns_stat_average_width);							
	stats_to_use.push_back(ns_stat_max_width);								
	stats_to_use.push_back(ns_stat_width_at_center);						
	stats_to_use.push_back(ns_stat_spine_length_to_bitmap_diagonal_ratio);	
	stats_to_use.push_back(ns_stat_average_curvature);						
	stats_to_use.push_back(ns_stat_curvature_variance);	
	//unnormalized relative intensity
	for (ns_detected_worm_classifier c = ns_stat_relative_intensity_average; c<=ns_stat_relative_intensity_containing_image_region_average; c = (ns_detected_worm_classifier)((int)c+1)) stats_to_use.push_back(c);
	//normalized absolute intensity
	for (ns_detected_worm_classifier c = ns_stat_absolute_intensity_normalized_average; c<=ns_stat_absolute_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1)) stats_to_use.push_back(c);
		output_training_set_including_stats(worm_stats, non_worm_stats, stats_to_use,spec, training_set_base_dir,strain_specific_directory,"best_set");

	stats_to_use.resize(0);
	stats_to_use.push_back(ns_stat_pixel_area);
	stats_to_use.push_back(ns_stat_bitmap_width);							
	stats_to_use.push_back(ns_stat_bitmap_height);							
	stats_to_use.push_back(ns_stat_bitmap_diagonal);						
	stats_to_use.push_back(ns_stat_spine_length);							
	stats_to_use.push_back(ns_stat_distance_between_ends);					
	stats_to_use.push_back(ns_stat_average_width);							
	stats_to_use.push_back(ns_stat_max_width);								
	stats_to_use.push_back(ns_stat_width_at_center);						
	stats_to_use.push_back(ns_stat_spine_length_to_bitmap_diagonal_ratio);	
	stats_to_use.push_back(ns_stat_average_curvature);						
	stats_to_use.push_back(ns_stat_curvature_variance);						
	//unnormalized relative intensity
	for (ns_detected_worm_classifier c = ns_stat_relative_intensity_average; c<=ns_stat_relative_intensity_spine_variance; c = (ns_detected_worm_classifier)((int)c+1)) stats_to_use.push_back(c);
	//normalized absolute intensity
	for (ns_detected_worm_classifier c = ns_stat_absolute_intensity_normalized_average; c<=ns_stat_absolute_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1)) stats_to_use.push_back(c);
	output_training_set_including_stats(worm_stats, non_worm_stats, stats_to_use,spec, training_set_base_dir,strain_specific_directory,"best_set_wo_entropy_or_dk");


	
	stats_to_use.clear();
	stats_to_use.push_back(ns_stat_relative_intensity_roughness_2);
	stats_to_use.push_back(ns_stat_absolute_intensity_roughness_2);							
	stats_to_use.push_back(ns_stat_relative_intensity_spine_average);							
	stats_to_use.push_back(ns_stat_absolute_intensity_spine_average);						
	stats_to_use.push_back(ns_stat_relative_intensity_distance_from_neighborhood);							
	stats_to_use.push_back(ns_stat_relative_intensity_average);					
	stats_to_use.push_back(ns_stat_absolute_intensity_normalized_spine_average);							
	stats_to_use.push_back(ns_stat_relative_intensity_normalized_average);			
	stats_to_use.push_back(ns_stat_relative_intensity_normalized_spine_average);						
	stats_to_use.push_back(ns_stat_spine_length);						
	stats_to_use.push_back(ns_stat_pixel_area);	
	stats_to_use.push_back(ns_stat_relative_intensity_variance);						
	stats_to_use.push_back(ns_stat_bitmap_diagonal);						
	stats_to_use.push_back(ns_stat_distance_between_ends);							
	stats_to_use.push_back(ns_stat_absolute_intensity_distance_from_neighborhood);							
	stats_to_use.push_back(ns_stat_edge_length);							
	stats_to_use.push_back(ns_stat_relative_intensity_dark_pixel_average);							
	stats_to_use.push_back(ns_stat_absolute_intensity_variance);							
	stats_to_use.push_back(ns_stat_edge_to_area_ratio);	
	stats_to_use.push_back(ns_stat_intensity_profile_edge);	
	stats_to_use.push_back(ns_stat_intensity_profile_center);		
	stats_to_use.push_back(ns_stat_intensity_profile_max);		
	stats_to_use.push_back(ns_stat_intensity_profile_variance);
	output_training_set_including_stats(worm_stats, non_worm_stats, stats_to_use,spec, training_set_base_dir,strain_specific_directory, "high_13_fscore_features");
					
	stats_to_use.push_back(ns_stat_relative_intensity_skew);
	stats_to_use.push_back(ns_stat_spine_length_to_max_width_ratio);
	stats_to_use.push_back(ns_stat_absolute_intensity_normalized_max);
	stats_to_use.push_back(ns_stat_absolute_intensity_max);
	stats_to_use.push_back(ns_stat_absolute_intensity_spine_variance);
	output_training_set_including_stats(worm_stats, non_worm_stats, stats_to_use,spec, training_set_base_dir,strain_specific_directory,"high_28_fscore_features");

	stats_to_use.push_back(ns_stat_relative_intensity_skew);
	stats_to_use.push_back(ns_stat_spine_length_to_max_width_ratio);
	stats_to_use.push_back(ns_stat_absolute_intensity_normalized_max);
	stats_to_use.push_back(ns_stat_absolute_intensity_max);
	stats_to_use.push_back(ns_stat_absolute_intensity_spine_variance);
	output_training_set_including_stats(worm_stats, non_worm_stats, stats_to_use,spec, training_set_base_dir ,strain_specific_directory, "high_28_fscore_features");


	stats_to_use.push_back(ns_stat_average_width);
	stats_to_use.push_back(ns_stat_min_width);
	stats_to_use.push_back(ns_stat_average_curvature);
	
	output_training_set_including_stats(worm_stats, non_worm_stats, stats_to_use,spec, training_set_base_dir ,strain_specific_directory, "high_24_plus_fscore_features");

	//unnormalized relative intensity
	for (ns_detected_worm_classifier c = ns_stat_relative_intensity_average; c<=ns_stat_relative_intensity_containing_image_region_average; c = (ns_detected_worm_classifier)((int)c+1)) stats_to_use.push_back(c);
	//normalized absolute intensity
	for (ns_detected_worm_classifier c = ns_stat_absolute_intensity_normalized_average; c<=ns_stat_absolute_intensity_normalized_spine_average; c = (ns_detected_worm_classifier)((int)c+1)) stats_to_use.push_back(c);
		output_training_set_including_stats(worm_stats, non_worm_stats, stats_to_use,spec, training_set_base_dir ,strain_specific_directory, "best_set");



	cout << "\nOutputing Worm Rap Sheets...";
	std::string html_filename = analysis_dir + DIR_CHAR_STR + "worm_stats.html";
	ofstream html(html_filename.c_str());
	if (html.fail())
		throw ns_ex("Could not open html file for output: ") << html_filename;
	html<< "<html><head><title>Worm Statistics Summmary</title></head><body>\n";
	for (unsigned int i = 0; i < worm_stats.size(); i++)
		worm_stats[i].output_html_worm_summary(html);
	for (unsigned int i = (unsigned int)non_worm_stats.size(); i < non_worm_stats.size(); i++)
		non_worm_stats[i].output_html_worm_summary(html);

	html << "</body></html>";
	html.close();

	
	cout << "\nOutputing Worm Summary";
	html_filename = analysis_dir + DIR_CHAR_STR + "annotation_summary.html";
	html.open(html_filename.c_str());
	if (html.fail())
		throw ns_ex("Could not open html file for output: ") << html_filename;
	html<< "<html><head><title>Worm Annotation Summary</title></head><body bgcolor=\"#000000\" text=\"#FFFFFF\">\n";
	html << "<h1>Worms</h1>\n";
	for (unsigned int i = 0; i < worm_stats.size(); i++)
		html << "<a href=\"" << worm_stats[i].debug_filename << "\"><img src=\"" << worm_stats[i].debug_filename << "\" border = 0></a>\n";
	html << "<BR><h1>Non-worms</h1>\n";
	for (unsigned int i = 0; i < non_worm_stats.size(); i++)
		html << "<a href=\"" << non_worm_stats[i].debug_filename << "\"><img src=\"" << non_worm_stats[i].debug_filename << "\" border = 0></a>\n";
	html << "<BR><h1>Censored Objects</h1>\n";
	for (unsigned int i = 0; i < censored_object_filenames.size(); i++)
		html << "<a href=\"" << censored_object_filenames[i] << "\"><img src=\"" << censored_object_filenames[i] << "\" border = 0></a>\n";

	html << "</body></html>";
	html.close();


	cout << "\nOutputing Classifier Comparrisons...";
	unsigned int h_w = 20;

	html_filename = analysis_dir + DIR_CHAR_STR + "classifier_comp.html";
	html.open(html_filename.c_str());
	if (html.fail())
		throw ns_ex("Could not open html file for output: ") << html_filename;
	html<< "<html><head><title>Classifiers Comparrison</title></head><body bgcolor=\"#000000\" text=\"#FFFFFF\">\n";
	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){
		html << "<b>" << ns_classifier_label((ns_detected_worm_classifier)s) << "</b>";
		html << "<table cellspacing=0 cellpadding=1><tr>";
		unsigned int wi = 0;
		for (unsigned int i = 0; i < worm_stats.size(); i++){
			html << "<td><img src=\"" << worm_stats[i].debug_filename << "\" border = 0>";
			html << worm_stats[i].statistics[s] << "</td>";
			if (wi != 0 && wi%h_w == 0)
				html<< "</tr>\n<tr>";
			wi++;
		}
		for (unsigned int i = ((unsigned int)non_worm_stats.size()*4)/5; i < non_worm_stats.size(); i++){
			html << "<td><img src=\"" << non_worm_stats[i].debug_filename << "\" border = 0>";
			html << non_worm_stats[i].statistics[s] << "</td>";
			if (wi != 0 && wi%h_w == 0)
				html<< "</tr>\n<tr>";
			wi++;
		}
		if ((wi-1)%h_w != 0)
			html << "</tr>\n";
		html << "</table>\n";
	}
	html << "</body></html>";
	html.close();

	std::string raw_data_file = analysis_dir + DIR_CHAR_STR + "features.csv";
	ofstream rd(raw_data_file.c_str());
	rd << "is_a_worm,";
	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++)
		rd <<  ns_classifier_abbreviation((ns_detected_worm_classifier)s) << ",";
	rd << "\n";
	for (unsigned int i = 0; i < worm_stats.size(); i++){
		rd << "1,";
		for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++)
			rd << worm_stats[i][(ns_detected_worm_classifier)s] << ",";
		rd << "\n";
	}
	for (unsigned int i = 0; i < non_worm_stats.size(); i++){
		rd << "0,";
		for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++)
			rd << non_worm_stats[i][(ns_detected_worm_classifier)s] << ",";
		rd << "\n";
	}
	rd.close();
	if(worm_stats.size() == 0 || non_worm_stats.size() == 0)
		throw ns_ex("Lacking data for one class of objects: ") << (unsigned long)worm_stats.size() << " worms, " << (unsigned long)non_worm_stats.size() << " non worms.";
	cerr << "Drawing feature distributions...\n";
	ns_detected_worm_stats::draw_feature_frequency_distributions(worm_stats, non_worm_stats,"training_set",analysis_dir + "\\freq");
	cerr << "Outputting matlab data set\n";

	//output data sets for matlab
	std::string output_filename = analysis_dir + DIR_CHAR_STR + "training_set_matlab.m";
	ofstream output_matlab(output_filename.c_str());
	output_matlab << "%ns_image_server feature sets\n";
	output_matlab << "%Nicholas Stroustrup, 2017\n\n";
	for (unsigned int s = 0; s < (unsigned int) ns_stat_number_of_stats; s++){
		unsigned int nl = 0;
		output_matlab << ns_classifier_abbreviation((ns_detected_worm_classifier)s) << "_false = [";
		for (unsigned int i = 0; i < non_worm_stats.size(); i++){
			output_matlab << " " << non_worm_stats[i].scaled_statistic((ns_detected_worm_classifier)s);
			if (nl == 100){
				nl = 0;
				output_matlab << " ...\n";
			}
			nl++;
		}
		output_matlab << " ];\n";
		
		output_matlab << ns_classifier_abbreviation((ns_detected_worm_classifier)s) << "_true = [";
		nl = 0;
		for (unsigned int i = 0; i < worm_stats.size(); i++){
				output_matlab << " " << worm_stats[i].scaled_statistic((ns_detected_worm_classifier)s);
			if (nl == 100){
				nl = 0;
				output_matlab << " ...\n";
			}
			nl++;
		}
		output_matlab << " ];\n";
	}

	output_matlab << "\n\n";

	//output labels
	output_matlab << "%Feature Labels\n";
	output_matlab << "classifier_names = { ...\n";
	for (unsigned int i = 0; i <= (unsigned int)ns_stat_number_of_stats; i++){
		output_matlab << "\t'" << ns_classifier_label((ns_detected_worm_classifier)i) << "'";
		if ((ns_detected_worm_classifier)i != ns_stat_number_of_stats)
			output_matlab << ",...\n";
	}
	output_matlab << "};\n\n";
	
	//output data matricies;
	output_matlab << "%Feature Matricies\n";
	output_matlab << "data_true = [...\n";
	for (unsigned int  i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		output_matlab << "\t" << ns_classifier_abbreviation((ns_detected_worm_classifier)i) << "_true" ;
		if (i != ((unsigned int)ns_stat_number_of_stats -1))
			output_matlab << ";...\n";
	}
	output_matlab << "];\n\n";

	output_matlab << "data_false = [...\n";
	for (unsigned int i = 0; i < (unsigned int)ns_stat_number_of_stats; i++){
		output_matlab << "\t" << ns_classifier_abbreviation((ns_detected_worm_classifier)i) << "_false" ;
		if (i != ((unsigned int)ns_stat_number_of_stats -1))
			output_matlab << ";...\n";
	}
	output_matlab << "];\n\n";

	output_matlab.close();
	cout << "Done!\n";
	//output graphing code for matlab
/*	for (unsigned int i = 0; i < (unsigned int) ns_stat_number_of_stats; i++){

		unsigned int subplot_x = (unsigned int)sqrt((float)((unsigned int)ns_stat_number_of_stats - i));
		unsigned int subplot_y = ((unsigned int)ns_stat_number_of_stats - i)/subplot_x;
		if (((unsigned int)ns_stat_number_of_stats - i)%subplot_x > 0)
			subplot_y++;

		output_matlab << "figure(" << i+1 << ");\n";
		unsigned int subplot_num = 1;
			
		for (unsigned int j = i+1; j < (unsigned int) ns_stat_number_of_stats; j++){
			output_matlab << "subplot(" << subplot_x << "," << subplot_y << "," << subplot_num << ");\n";

			
			output_matlab << "hold on;\n";
			output_matlab << "h = scatter(" << ns_classifier_abbreviation((ns_detected_worm_classifier)j) << "_false ," <<
							  ns_classifier_abbreviation((ns_detected_worm_classifier)i) << "_false, 'gs','filled');\n";
			output_matlab << "set(h,'CData',[0 .5 0]);\n";

			output_matlab << "h = scatter(" << ns_classifier_abbreviation((ns_detected_worm_classifier)j) << "_true ," <<
							  ns_classifier_abbreviation((ns_detected_worm_classifier)i) << "_true, 'rs','filled');\n";
			output_matlab << "set(h,'SizeData',2);\n";

			output_matlab << "ylabel('" << ns_classifier_label((ns_detected_worm_classifier)i) << "');\n";
			output_matlab << "xlabel('" << ns_classifier_label((ns_detected_worm_classifier)j) << "');\n";
			output_matlab << "set(gca,'Color','k');\n";
			output_matlab << "hold off;\n\n";
			subplot_num++;
		}
	}
	output_matlab.close();
*/
}


void ns_training_file_generator::plot_errors_on_freq(const std::string & results_filename){
	//calulate filenames
	std::string::size_type t = results_filename.find_last_of("_");
	if (t == std::string::npos)
		throw ns_ex("Could not read suffix of filename");
	
	std::string model_path = results_filename.substr(0,t);
	std::string	test_set_filename = model_path + "_test.txt",
						test_filenames_filename = model_path  + "_filenames_test.txt",
						training_set_filename = model_path + "_train.txt",
						training_filenames_filename = model_path  + "_filenames_train.txt",
						test_metadata_filename = model_path  + "_metadata_test.txt",
						training_metadata_filename = model_path  + "_metadata_train.txt";

	//open results file
	ifstream results(results_filename.c_str());
	if (results.fail())
		throw ns_ex("Could not load result file");

	//open filenames file
	ifstream test_filenames(test_filenames_filename.c_str());
	if (test_filenames.fail())
		throw ns_ex("Could not open test filenames file ") << test_filenames_filename;
	ifstream training_filenames(training_filenames_filename.c_str());
	if (training_filenames.fail())
		throw ns_ex("Could not open training filenames file ") << training_filenames_filename;
	ifstream test_metadata(test_metadata_filename.c_str());
	if (test_metadata.fail())
		throw ns_ex("Could not open test metadata file ") << test_metadata_filename;
	ifstream training_metadata(training_metadata_filename.c_str());
	if (training_metadata.fail())
		throw ns_ex("Could not open training metadata file ") << training_metadata_filename;

	//open model scaling info
	ns_svm_model_specification model;
	model.read_statistic_ranges(model_path + "_range.txt");
	model.read_included_stats(model_path + "_included_stats.txt");
	//open test set 
	ifstream test_set_f(test_set_filename.c_str());
	if (test_set_f.fail())
		throw ns_ex("Could not open test set ") << test_set_filename;
	//open training set
	ifstream training_set_f(training_set_filename.c_str());
	if (training_set_f.fail())
		throw ns_ex("Could not open training set ") << training_set_filename;

	std::vector<ns_learning_results_decision> decisions;
	std::vector<ns_learning_results_decision> training_data;
	decisions.reserve(1000);
	training_data.reserve(1000);
	std::map<string, int> genotypes_found;
	//load test set and results
	unsigned int s = 0;
	while(true){
		s++;
		decisions.resize(s);
		decisions[s-1].stat.specifiy_model(model);
		if (!decisions[s-1].load(test_filenames,test_metadata,test_set_f,results)){
			break;
		}
		genotypes_found[decisions[s-1].metadata.genotype] = 1;
	}
	decisions.resize(s-1);
	test_filenames.close();
	test_set_f.close();
	results.close();
	if (s == 0) throw ns_ex("Empty test set found");
	cerr << "Loaded " << s << " test set elements\n";

	//load training set for reference
	s = 0;
	bool dummy;
	while(true){
		s++;
		training_data.resize(s);
		training_data[s-1].stat.specifiy_model(model);
		if (!training_data[s-1].load_model_data(training_set_f,training_filenames,training_metadata,dummy))
			break;
		training_data[s-1].decision = dummy?ns_learning_results_decision::ns_tp:ns_learning_results_decision::ns_tn;
		genotypes_found[training_data[s-1].metadata.genotype] = 1;
	}
	training_data.resize(s-1);
	training_filenames.close();
	training_set_f.close();
	cerr << "Loaded " << s << " training set elements.\n";
	cerr << "Found" << genotypes_found.size() << " different genotypes.\n";
	if (s == 0) throw ns_ex("Empty training set found");
	

	std::string training_set_base_dir = ns_dir::extract_path(model_path);
	std::string::size_type t2 = training_set_base_dir.find_last_of("training_sets");
	string bdir;
	if (t2!=results_filename.npos){
		bdir = training_set_base_dir.substr(0,t2- 13);
	}
	else{
		bdir = training_set_base_dir + DIR_CHAR_STR + "analysis";
	}
	string model_name = ns_dir::extract_filename(model_path);
	std::string	test_dir = bdir + "distributions_test_set_" + model_name,
			train_dir = bdir + "distributions_training_set_" + model_name;
	//		simple_dir = bdir + "distributions";
	ns_dir::create_directory_recursive(test_dir);
	ns_dir::create_directory_recursive(train_dir);
	//ns_dir::create_directory_recursive(simple_dir);

	
	vector<std::string> genotypes_to_output;
	genotypes_to_output.push_back("");
	for (std::map<std::string,int>::iterator p = genotypes_found.begin(); p!= genotypes_found.end(); p++){
		genotypes_to_output.push_back(p->first);
	}
	std::string test_results_fname(bdir + DIR_CHAR_STR + string("test_results_") + model_name + ".txt");
	ofstream test_results(test_results_fname.c_str());
	if (test_results.fail())
		throw ns_ex("Could not open file ") << test_results_fname;
	for (unsigned int i =0; i < genotypes_to_output.size(); i++){
		ns_learning_results_decision_set results_set;
		results_set.from_decisions(decisions,genotypes_to_output[i]);

		string gt(genotypes_to_output[i]);
		if (gt=="")
			gt = "all_genotypes";

		results_set.out_summary(test_results,gt);
		


		std::string summary_filename = bdir + DIR_CHAR_STR + string("error_summary_") + model_name + "=" + ns_remove_bad_filename_characters(gt) + ".html";
		ofstream summary_file(summary_filename.c_str());
		if (summary_file.fail())
			throw ns_ex("Could not open summary file: ") << summary_filename;
		results_set.output_html_error_summary(summary_file);
	}

	ns_learning_results_decision_set training_set;
	training_set.from_decisions(training_data);

	
	std::string training_summary_filename = bdir + DIR_CHAR_STR + string("training_set_summary_") + model_name + ".html";
	ofstream training_summary_file(training_summary_filename.c_str());
	if (training_summary_file.fail())
		throw ns_ex("Could not open summary file: ") << training_summary_filename;
	training_set.output_html_error_summary(training_summary_file);
	training_summary_file.close();
	ns_learning_results_decision_set results_set;
	results_set.from_decisions(decisions,"");
	cerr << "Generating Test Set Distributions...\n";
	cerr << "Writing to " << test_dir << "...\n";
	results_set.produce_frequency_distribution(test_dir);
	cerr << "Generating Training Set Distributions...\n";
	cerr << "Writing to " << train_dir << "...\n";
	training_set.produce_frequency_distribution(train_dir);
	cerr << "Done.\n";
}

void ns_training_file_generator::re_threshold_training_set(const std::string &directory,const  ns_svm_model_specification & model){
	std::string base_dir = ns_dir::extract_path(directory);
	training_set_base_dir = base_dir;
	ns_dir dir;
	dir.load(base_dir);	
	std::string worm_dir;
	std::string junk_dir;
	for (unsigned int i = 0; i < dir.dirs.size(); i++){
		if (dir.dirs[i].find("worm") != dir.dirs[i].npos)
			worm_dir = dir.dirs[i];
		else if (dir.dirs[i].find("dirt") != dir.dirs[i].npos)
			junk_dir = dir.dirs[i];
	}
	if (worm_dir == "")
		throw ns_ex("Could not find worm directory");
	if (junk_dir == "")
		throw ns_ex("Could not find non-worm directory");

	std::string worm_path = base_dir + DIR_CHAR_STR + worm_dir,
		   junk_path = base_dir + DIR_CHAR_STR + junk_dir;

	ns_dir worm_files(worm_path + DIR_CHAR_STR + "*.tif"),
		   junk_files(junk_path + DIR_CHAR_STR + "*.tif");

	cerr << "Loading " << worm_files.files.size() << " worms from " << worm_path << "\n";
	cerr << "Loading " << junk_files.files.size() << " non-worm images from " << junk_path << "\n";
	//ns_image_standard im;
	
	//calculate statistics for all files
	unsigned int res = 20;
	unsigned int percent = 0;
	unsigned int step = (unsigned int)(worm_files.files.size())/res ;
	unsigned int counter = 0;
	/*for (unsigned int i = 0; i < worm_files.files.size(); i++){
		
		std::string fname = worm_path + DIR_CHAR_STR + worm_files.files[i];
		re_threshold_image(fname,false);

		if (counter == step){
			percent+=1;
			cerr << (100*percent*step)/worm_files.files.size() + 1 << "%...";
			counter = 0;				
		}
		counter++;
	}*/

	ns_progress_reporter pr(junk_files.files.size(),10);
	for (unsigned int i = 0; i < junk_files.files.size(); i++){
		pr(i);
		std::string fname = junk_path + DIR_CHAR_STR + junk_files.files[i];
		re_threshold_image(fname,true,model);
	}
	pr(junk_files.files.size());

}



void ns_training_file_generator::mark_duplicates_in_training_set(const std::string & filename){
	std::string base_dir = ns_dir::extract_path(filename);
	training_set_base_dir = base_dir;
	ns_dir dir;
	dir.load_masked(base_dir,"tif",dir.files);	
	std::sort(dir.files.begin(),dir.files.end());
	/*for (unsigned int i = 0; i < 50; i++)
		cerr << dir.files[i] << "\n";
	return;*/
	std::vector< std::vector<ns_image_standard *> > all_worms(dir.files.size());
	std::vector< std::vector<ns_image_standard *> > non_worms(dir.files.size());
	std::vector< std::vector<ns_image_standard *> > worms(dir.files.size());
	std::vector< std::vector<ns_packed_collage_position> > source_worm_locations(dir.files.size());
	ns_progress_reporter pr(dir.files.size(),10);
	unsigned real(0),duplicate(0);
	for (unsigned int i = 0; i < dir.files.size(); i++){
		pr(i);
		std::string collage_fname = base_dir + DIR_CHAR_STR + dir.files[i];
		ns_image_standard collage;
		ns_load_image(collage_fname,collage);
		
		ns_annotated_training_set training_set;
		all_worms[i].resize(training_set.objects.size());
		worms[i].resize(training_set.worms.size());
		non_worms[i].resize(training_set.non_worms.size());
		source_worm_locations[i].resize(training_set.objects.size());
		for (unsigned int j = 0; j < all_worms[i].size(); j++)all_worms[i][j] = &training_set.objects[j].object.relative_grayscale();
		for (unsigned int j = 0; j < worms[i].size(); j++)worms[i][j] = &training_set.worms[j]->object.relative_grayscale();
		for (unsigned int j = 0; j < non_worms[i].size(); j++)non_worms[i][j] = &training_set.non_worms[j]->object.relative_grayscale();
		for (unsigned int j = 0; j < source_worm_locations[i].size(); j++)source_worm_locations[i][j] = training_set.objects[j].collage_position;

		ns_worm_training_set_image::decode(collage,training_set);
	//	cerr << "Found " << training_set.worms.size() << " worms, " << training_set.non_worms.size() << ", and non-worms out of " << training_set.objects.size() << " objects.\n";
	
		bool found_duplicate = false;
		for (unsigned int j = 0; j < all_worms[i].size(); j++){
			ns_vector_2i original_of_duplicate(image_already_exists(i,*all_worms[i][j],source_worm_locations[i][j].original_bitmap_position,all_worms,source_worm_locations));
			if (original_of_duplicate.x != -1){
					/*std::string dup_dir_name = base_dir + DIR_CHAR_STR + "duplicate_visualization";
					ns_dir::create_directory_recursive(dup_dir_name);
					std::string filename_suffix =  + "dup_" + ns_to_string(original_of_duplicate.x) + "_" + ns_to_string(original_of_duplicate.y) + "=";
					ns_save_image(dup_dir_name + DIR_CHAR_STR + filename_suffix + ns_to_string(i) + "_" + ns_to_string(j) + ".tif",all_worms[i][j]);
					ns_save_image(dup_dir_name + DIR_CHAR_STR + filename_suffix + "source.tif",all_worms[original_of_duplicate.x][original_of_duplicate.y]);
					*/
					all_worms[i][j]->clear();
					source_worm_locations[i][j].original_bitmap_position = ns_vector_2i(-1,-1);
					found_duplicate = true;

					ns_vector_2i pos_to_color = source_worm_locations[i][j].pos + ns_vector_2i (4,4);
					for (unsigned int y = 0; y < 5; y++){
						for (unsigned int x = 0; x < 5; x++){
							collage[pos_to_color.y+y][3*(pos_to_color.x+x)+0] = 255;
							collage[pos_to_color.y+y][3*(pos_to_color.x+x)+1] = 0;
							collage[pos_to_color.y+y][3*(pos_to_color.x+x)+2] = 0;
						}
					}
					duplicate++;
			}
			else{
				/*std::string dir_name = base_dir + DIR_CHAR_STR + "non_duplicate_visualization";
				ns_dir::create_directory_recursive(dir_name);
				ns_save_image(dir_name + DIR_CHAR_STR + ns_to_string(i) + "_" + ns_to_string(j) + ".tif",all_worms[i][j]);*/
				real++;
			}
			cerr << "RvD: " << real << "v" << duplicate << "\n";
		}
	
		if (found_duplicate)
			ns_save_image(collage_fname,collage);
	}
}

	ns_vector_2i ns_training_file_generator::image_already_exists(unsigned int cur_frame, const ns_image_standard & im, const ns_vector_2i & im_pos, const std::vector<std::vector<ns_image_standard *> > & images, const std::vector<std::vector<ns_packed_collage_position> > & source_image_locations){
	int start  = cur_frame - 50,
		stop = cur_frame + 50;
	if (start < 0) start = 0;
	if (stop >= (int)images.size())
		stop = (int)images.size()-1;
	for (int i = start; i < stop; i++){
		if (i == cur_frame) continue;
		for (unsigned int j =0; j < images[i].size(); j++){
			ns_bitmap_overlap_calculation_results overlap(ns_calculate_bitmap_overlap(im,im_pos,*images[i][j],source_image_locations[i][j].original_bitmap_position));
			if (10*overlap.overlap_area > 6*overlap.image_1_absolute_area &&
				10*overlap.overlap_area > 6*overlap.image_2_absolute_area)
				return ns_vector_2i(i,j);
		}
	}
	return ns_vector_2i(-1,-1);
}

	void ns_training_file_generator::split_training_set_into_different_regions(const std::string & results_filename){
	//calulate filenames
	std::string::size_type t = results_filename.find_last_of("_");
	if (t == std::string::npos)
		throw ns_ex("Could not read suffix of filename");
	
	std::string	model_name = results_filename.substr(0,t),
			test_set_filename = model_name + "_test.txt",
			test_filenames_filename = model_name  + "_filenames_test.txt",
			training_set_filename = model_name + "_train.txt",
			training_filenames_filename = model_name  + "_filenames_train.txt";

	//open filenames file
	/*ifstream test_filenames(test_filenames_filename.c_str());
	if (test_filenames.fail())
		throw ns_ex("Could not open test filenames file ") << test_filenames_filename;*/
	ifstream training_filenames(training_filenames_filename.c_str());
	if (training_filenames.fail())
		throw ns_ex("Could not open training filenames file ") << training_filenames_filename;


	//open test set 
/*	ifstream test_set_f(test_set_filename.c_str());
	if (test_set_f.fail())
		throw ns_ex("Could not open test set ") << test_set_filename;*/
	//open training set
	ifstream training_set_f(training_set_filename.c_str());
	if (training_set_f.fail())
		throw ns_ex("Could not open training set ") << training_set_filename;


	std::string region_base_dir = ns_dir::extract_path(model_name);
	std::string	region_dir = region_base_dir + "\\analysis\\region_test_sets";
	ns_dir::create_directory_recursive(region_dir);

	std::vector<std::string> region_names;
	region_names.push_back("cf18544");
	region_names.push_back("cf512");
	region_names.push_back("tj1060");
	region_names.push_back("tj1062");
	std::vector<ofstream *> region_filename_files(region_names.size());
	std::vector<ofstream *> region_training_set_files(region_names.size());
	for (unsigned int i = 0; i < region_names.size(); i++){
		std::string base_filename = region_dir + DIR_CHAR_STR + region_names[i];
		region_filename_files[i] = new ofstream((base_filename + "_filenames_train.txt").c_str());
		region_training_set_files[i] = new ofstream((base_filename + "_train.txt").c_str());
		if (region_filename_files[i]->fail() || region_training_set_files[i]->fail())
			throw ns_ex("Could not open file ") << base_filename << ".* for writing";
		//copy statistics and range info
		ns_dir::copy_file(model_name + "_range.txt",base_filename + "_range.txt");
		ns_dir::copy_file(model_name + "_excluded_stats.txt",base_filename+ "_excluded_stats.txt");
	}

	while(true){
		std::string filename, data;
		getline(training_filenames,filename);
		getline(training_set_f,data);
		if (training_filenames.fail() || training_set_f.fail())
			break;
		bool found=false;
		for (unsigned int i = 0; i < region_names.size(); i++){
			if (filename.find(region_names[i])!=filename.npos){
					*region_filename_files[i] << filename << "\n";
					*region_training_set_files[i] << data << "\n";
					found = true;
				break;
			}
		}
		if (!found)
			throw ns_ex("Could not identify region info for file") << filename;
	}
	
	for (unsigned int i = 0; i < region_names.size(); i++){
		
		region_filename_files[i]->close();
		region_training_set_files[i]->close();
		delete region_filename_files[i];
		delete region_training_set_files[i];
	}
	training_set_f.close();
	training_filenames.close();
}

void ns_training_file_generator::lookup_genotypes(ns_sql & sql){
	std::map<std::string,int> genotypes_found;
	for (unsigned int i = 0; i < worm_stats.size(); i++){
		if (worm_stats[i].metadata.region_id!=0)
			worm_stats[i].metadata.load_from_db(worm_stats[i].metadata.region_id,"",sql);
		genotypes_found[worm_stats[i].metadata.device_regression_match_description()] = 1;
	}
	for (unsigned int i = 0; i < non_worm_stats.size(); i++){
		if (non_worm_stats[i].metadata.region_id!=0)
			non_worm_stats[i].metadata.load_from_db(non_worm_stats[i].metadata.region_id,"",sql);
		genotypes_found[non_worm_stats[i].metadata.device_regression_match_description()] = 1;
	}

	genotypes_in_set.resize(0);
	genotypes_in_set.reserve(genotypes_found.size());
	for (std::map<std::string,int>::iterator p = genotypes_found.begin(); p != genotypes_found.end();p++)
		genotypes_in_set.push_back(p->first);
}

void ns_training_file_generator::add_object_to_training_set(const ns_svm_model_specification & model, const bool is_a_worm, const std::string & filename, const std::string & relative_filename, ns_detected_worm_info & worm, const long long & region_id){

	ns_detected_worm_stats stats = worm.generate_stats();

	if (is_a_worm){
		worm_stats.push_back(stats);
		worm_stats[worm_stats.size()-1].specifiy_model(model);
		worm_stats[worm_stats.size()-1].debug_filename = relative_filename;
		worm_stats[worm_stats.size()-1].metadata.region_id = region_id;
	}
	else {
		non_worm_stats.push_back(stats);
		non_worm_stats[non_worm_stats.size()-1].specifiy_model(model);
		non_worm_stats[non_worm_stats.size()-1].debug_filename = relative_filename;
		non_worm_stats[non_worm_stats.size()-1].metadata.region_id = region_id;
	}
}
/*
void ns_training_file_generator::calc_image_stats(ns_svm_model_specification & model, const bool is_a_worm, const std::string & filename, const std::string & relative_filename, ns_image_standard & im){
		//apply image threshold
	
		ns_whole_image_region_stats image_region_stats;
		ns_image_standard source;
		ns_image_standard thresholded;
		//old grayscale-style worm image
		if (im.properties().components == 1){
			//this will work, but several classifiers will not be valid.
			throw ns_ex("Encountered an old-school training data image.");
			im.pump(source,512);
			image_region_stats.absolute_intensity_stats.average_intensity = 0;
			image_region_stats.absolute_intensity_stats.maximum_intensity = 255;
			image_region_stats.absolute_intensity_stats.minimum_intensity = 127;
			image_region_stats.relative_intensity_stats = image_region_stats.absolute_intensity_stats;
		}
		//new, contextual worm images
		else{
			ns_image_properties p(im.properties());
			p.components = 1;
			p.resolution = 3200;
			source.prepare_to_recieve_image(p);
			thresholded.prepare_to_recieve_image(p);
			for (unsigned int y = 0; y < p.height; y++){
				for (unsigned int x = 0; x < p.width; x++){		
					source[y][x] = im[y][3*x+2];
					if (im[y][3*x+0] == 255 && im[y][3*x+1] == 255	//use red and yellow channels as a mask
											&& !(y == 0 && x == 0))	//top left pixel contains contextual information
						thresholded[y][x] = im[y][3*x+2]>0;
					else
						thresholded[y][x] = 0;
				}
			}
			
			//extract contextual information
			image_region_stats.minimum_region_intensity_in_containing_image = im[0][0];
			image_region_stats.maximum_region_intensity_in_containing_image = im[0][1];
			image_region_stats.average_region_intensity_in_containing_image = im[0][2];
		}

		source.pump(im,512);
		ns_image_properties prop = im.properties();
		for (unsigned int y = 0; y < prop.height; y++)
				for (unsigned int x = 0; x < prop.width; x++)
					im[y][x]*=thresholded[y][x];

		ns_worm_detector<ns_image_standard> worm_detector;
		ns_image_worm_detection_results * dr = worm_detector.run(0,0,thresholded,
				source,
				0,
				ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,thresholded.properties().resolution),
				ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,thresholded.properties().resolution),
				ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,thresholded.properties().resolution),
			model,"",ns_detected_worm_info::ns_vis_none,image_region_stats);
		try{
			
			//dr->create_visualization(10,2,vis,"",true,false,true);
		//	draw_image(-1,-1,1,1,vis);
			//only allow one worm per region in worm set.
			if (is_a_worm){
				if (dr->number_of_putative_worms() == 0)
					throw ns_ex("No worms found in image ") << filename;
				if (dr->number_of_putative_worms() > 1){
					cerr << dr->number_of_putative_worms() << " worms found in region.\n";
					delete dr;
					return;
				}
					//throw ns_ex("Too many worms found in image ") << filename;
				ns_detected_worm_stats stat(dr->get_putative_worm_stats(0));
				if (!stat.not_a_worm){
					worm_stats.push_back(dr->get_putative_worm_stats(0));
					worm_stats[worm_stats.size()-1].specifiy_model(model);
					worm_stats[worm_stats.size()-1].debug_filename = relative_filename;
				}
			}
			//non worms can have many subworms.
			else {
				for (unsigned int i = 0; i < dr->number_of_putative_worms(); i++){			
					ns_detected_worm_stats stat(dr->get_putative_worm_stats(i));
					if (stat.not_a_worm)
						continue;
					//disregard too small or too large
					if (stat[ns_stat_pixel_area] >ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,3200))
						continue;
					if (stat[ns_stat_pixel_area] <ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,3200))
						continue;
					if (stat[ns_stat_bitmap_diagonal] >ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,3200))
						continue;
	
					non_worm_stats.push_back(stat);
					non_worm_stats[non_worm_stats.size()-1].specifiy_model(model);
					non_worm_stats[non_worm_stats.size()-1].debug_filename = relative_filename;
					
				}
			}
			delete dr;
		}
		catch (ns_ex & ex){
			delete dr;
			cerr << ex.text() << "\n";
		}
		catch (...){
			delete dr;
			throw;
		}

}		*/
void ns_training_file_generator::output_image_stats(ostream & out_training_data, const bool is_a_worm, ns_detected_worm_stats & stats){
	if (is_a_worm)
		 out_training_data << "+1 ";
	else out_training_data << "-1 ";
	out_training_data << stats.parameter_string();
	out_training_data << "\n";
}		


void ns_training_file_generator::re_threshold_image(const std::string & fname,const bool take_best_spine_solution, const ns_svm_model_specification & model){
	ns_image_standard im;
	ns_load_image(fname, im);
	std::vector<ns_image_standard> new_images;

	re_threshold_image(im,new_images,take_best_spine_solution,model);
	if (new_images.size() == 0)
		cerr << "Image produced no putative worms\n";

	std::string path =  ns_dir::extract_path(fname);
	path += DIR_CHAR_STR;
	path += "new";
	ns_dir::create_directory_recursive(path);
	for (unsigned int i = 0; i < new_images.size(); i++){
		std::string filename;
		filename += path + DIR_CHAR_STR;
		filename += ns_dir::extract_filename_without_extension(ns_dir::extract_filename(fname));
		filename += ns_to_string(i)+".tif";
		ns_tiff_image_output_file<ns_8_bit> tiff_out;
		ns_image_stream_file_sink<ns_8_bit > file_sink(filename,tiff_out,1.0,512);
		new_images[i].pump(file_sink,128);
	}

}

void ns_training_file_generator::re_threshold_image(ns_image_standard & im,std::vector<ns_image_standard> & new_test_images, const bool take_best_spine_solution, const ns_svm_model_specification & model){
	throw ns_ex("Needs to be reimpemented for new training set encoding");
	ns_image_properties p = im.properties();
	if (p.components != 3)
		throw ns_ex("Invalid file type");
	p.resolution = 3200;
	p.components = 1;
	ns_image_standard source;
	source.prepare_to_recieve_image(p);
	unsigned long pixel_area = 0;
	for (unsigned int y = 0; y < p.height; y++){
		for (unsigned int x = 0; x < p.width; x++){	
			if (im[y][3*x+0] == 255 && im[y][3*x+1] == 255	//use red and yellow channels as a mask
								    && !(y == 0 && x == 0))	//top left pixel contains contextual information
				pixel_area++;
			source[y][x] = im[y][3*x+2];
		}
	}
	ns_8_bit context_info[6];
	//grab contextual information from source image before we wipe it.
	for (unsigned int x = 0; x < 6; x++)
		context_info[x] = im[0][x];
	source.pump(im,512);
	
	ns_image_standard thresh;
	ns_two_stage_difference_thresholder::run(source,thresh,0,ns_two_stage_difference_parameters(),false);
	//ns_whole_image_region_stats image_region_stats;
	ns_worm_detector<ns_image_standard> worm_detector;
	ns_image_standard unprocessed;
	throw ns_ex("Unprocessed image not loaded");
	ns_image_worm_detection_results * dr = worm_detector.run(0,0,unprocessed,thresh,
			im,
			0,
				ns_worm_detection_constants::get(ns_worm_detection_constant::minimum_worm_region_area,thresh.properties().resolution),
				ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_worm_region_area,thresh.properties().resolution),
				ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_region_diagonal,thresh.properties().resolution),
			model,ns_worm_detection_constants::get(ns_worm_detection_constant::maximum_number_of_actual_worms_per_image),0,"",ns_detected_worm_info::ns_vis_none);
	try{
		new_test_images.resize(0);
		if (dr->number_of_putative_worms() == 0)
			return;

		if (take_best_spine_solution){
			//we only want the longest spine outputted, so find it and output it.
			unsigned long best_id = 0;
			double diff = 99999999;
			for (unsigned int i = 0; i < dr->number_of_putative_worms(); i++){
				ns_detected_worm_stats s =  dr->get_putative_worm_stats(i);
				double d = abs(s[ns_stat_pixel_area]-pixel_area);
				if (d < diff){
					best_id = i;
					diff = d;
				}
			}
			new_test_images.resize(1);
			dr->generate_putative_training_set_visualization(best_id,new_test_images[0]);
		}
		else{
			//output all putative spines.
			new_test_images.resize(dr->number_of_putative_worms());
			for (unsigned int i = 0; i < new_test_images.size(); i++)
				dr->generate_putative_training_set_visualization(i,new_test_images[i]);
		}
		//restore contextual information in re-threshold  
		for (unsigned int i = 0; i < new_test_images.size(); i++)
			for (unsigned int x = 0; x < 6; x++)
					new_test_images[i][0][x] = context_info[x];
		delete dr;
	}
	catch(...){
		delete dr;
		throw;
	}
}

void ns_training_file_generator::output_training_set_including_stats(std::vector<ns_detected_worm_stats> & worm_stats, std::vector<ns_detected_worm_stats> & non_worm_stats, const std::vector<ns_detected_worm_classifier> & included_stats, const ns_svm_model_specification & model_base, const std::string & base_directory,const std::string & genotype_specific_directory,const std::string & base_filename){
	if (worm_stats.size() == 0)
		return;
	ns_svm_model_specification model = model_base;
	vector<string> genotypes_to_output;
	genotypes_to_output.push_back("");//all genotypes
	genotypes_to_output.insert(genotypes_to_output.end(),genotypes_in_set.begin(),genotypes_in_set.end());

	for (unsigned int g = 0; g < genotypes_to_output.size(); g++){
		std::string basename;
		if (genotypes_to_output[g]==""){
			basename = base_directory + DIR_CHAR_STR + base_filename;
		}
		else{
			basename = genotype_specific_directory + DIR_CHAR_STR + ns_remove_bad_filename_characters(genotypes_to_output[g]);
			ns_dir::create_directory_recursive(basename);
			basename = basename + DIR_CHAR_STR + base_filename;
		}
		std::string test_name = basename + "_test.txt",
			   train_name = basename + "_train.txt",
			   included_name = basename + "_included_stats.txt",
			   training_filenames = basename + "_filenames_train.txt",
			   test_filenames = basename + "_filenames_test.txt",
			   test_metadata = basename + "_metadata_test.txt",
			   training_metadata = basename + "_metadata_train.txt";

		//all worms use the same model pointer, so we can just modify the first.
		for (unsigned int i = 0; i < worm_stats[0].model().included_statistics.size(); i++)
			model.included_statistics[i] = 0;
		for (unsigned int i = 0; i < included_stats.size(); i++)
			model.included_statistics[included_stats[i]] = 1;
	
		model.write_statistic_ranges(basename + "_range.txt");
	
		std::ofstream training_set(train_name.c_str());
		std::ofstream test_set(test_name.c_str());
		std::ofstream included_set(included_name.c_str());
		std::ofstream training_filenames_set(training_filenames.c_str());
		std::ofstream test_filenames_set(test_filenames.c_str());
		std::ofstream test_metadata_set(test_metadata.c_str());
		std::ofstream training_metadata_set(training_metadata.c_str());
		if (training_set.fail())
			throw ns_ex("Could not open training set file for writing:") << train_name;
		if (test_set.fail())
			throw ns_ex("Could not open test set file for writing:") << test_name;
		if (included_set.fail())
			throw ns_ex("Could not open exclusion file for writing:") << included_name;

		for (unsigned int i = 0; i < included_stats.size(); i++)
			included_set << included_stats[i] << "\t" 
			<< ns_classifier_abbreviation(included_stats[i]) << "\t" 
			<< ns_classifier_label(included_stats[i]) << "\n";

		included_set.close();

		for (unsigned int i = 0; i < worm_stats.size(); i++){
			if (genotypes_to_output[g]!="" &&			//only output requested genotype
				genotypes_to_output[g] != worm_stats[i].metadata.device_regression_match_description())
				continue;

			if (i < worm_test_set_start_index){
				output_image_stats(training_set, true, worm_stats[i]);
				training_filenames_set << "+" << worm_stats[i].debug_filename << "\n";
				training_metadata_set << worm_stats[i].metadata.device_regression_match_description() << "\n";
			}
			else {
				output_image_stats(test_set, true, worm_stats[i]);
				test_filenames_set << "+" << worm_stats[i].debug_filename << "\n";
				test_metadata_set << worm_stats[i].metadata.device_regression_match_description() << "\n";
			}
		}	
		for (unsigned int i = 0; i < non_worm_stats.size(); i++){
			if (genotypes_to_output[g]!="" &&			//only output requested genotype
				genotypes_to_output[g] != non_worm_stats[i].metadata.device_regression_match_description())
				continue;

			non_worm_stats[i].specifiy_model(model);
			if (i < non_worm_test_set_start_index){
				output_image_stats(training_set, false, non_worm_stats[i]);
				training_filenames_set << "-" << non_worm_stats[i].debug_filename << "\n";
				training_metadata_set << non_worm_stats[i].metadata.device_regression_match_description() << "\n";
			}
			else {
				output_image_stats(test_set, false, non_worm_stats[i]);
				test_filenames_set << "-" << non_worm_stats[i].debug_filename << "\n";
				test_metadata_set << non_worm_stats[i].metadata.device_regression_match_description() << "\n";
			}
		}
	}

}


void ns_training_file_generator::output_training_set_excluding_stats(std::vector<ns_detected_worm_stats> & worm_stats, std::vector<ns_detected_worm_stats> & non_worm_stats, const std::vector<ns_detected_worm_classifier> & excluded_stats, const ns_svm_model_specification & model, const std::string & base_directory,const std::string & genotype_specific_directory,const std::string & base_filename){
	vector<char> use_stat(model.included_statistics.size(),1);

	for (unsigned int i = 0; i < excluded_stats.size(); i++)
		use_stat[excluded_stats[i]] = 0;

	vector<ns_detected_worm_classifier> included_stats;
	for (unsigned int i = 0; i < use_stat.size(); i++){
		if (use_stat[i])
			included_stats.push_back((ns_detected_worm_classifier)i);
	}
	output_training_set_including_stats(worm_stats, non_worm_stats,included_stats, model,base_directory,genotype_specific_directory,base_filename);
}
