#ifndef NS_DETECTED_WORM_INFO_H
#define NS_DETECTED_WORM_INFO_H
#include "ns_detected_object.h"
#include "ns_image_tools.h"
#include "ns_font.h"
#include "ns_worm_detection_constants.h"
#include "ns_vector_bitmap_interface.h"
#include "ns_graph.h"
#include <vector>
#include <iostream>
#include "ns_image_server.h"
#include "ns_movement_state.h"
#include "ns_region_metadata.h"

//#define NS_RETHRESHOLD_TRAINING_SET

#define NS_REGION_VIS_WORM_THRESHOLD_VALUE 255
#define NS_REGION_VIS_ALL_THRESHOLDED_OBJECTS_VALUE 70


struct ns_object_hand_annotation_data{
	ns_object_hand_annotation_data():identified_as_a_worm_by_human(0),
									 identified_as_a_worm_by_machine(0),
									 identified_as_a_mangled_worm(0),
									 identified_as_misdisambiguated_multiple_worms(0),
									 identified_as_small_larvae(0){};

	typedef enum{ns_non_worm,ns_worm_by_machine,ns_worm_by_human,ns_mangled_worm,
				ns_misdisambiguated_multiple_worms,ns_small_larvae, ns_number_of_object_hand_annotation_types} ns_object_hand_annotation;
	static ns_color_8 annotation_color(const ns_object_hand_annotation a){
		switch(a){
			case ns_non_worm:
				return ns_color_8(0,0,0);
			case ns_worm_by_machine:
				return ns_color_8(200,200,200);
			case ns_worm_by_human:
				return ns_color_8(255,255,255);
			case ns_mangled_worm:
				return ns_color_8(255,0,0);
			case ns_misdisambiguated_multiple_worms:
				return ns_color_8(0,0,255);
			case ns_small_larvae:
				return ns_color_8(0,255,255);
			case ns_number_of_object_hand_annotation_types:
				throw ns_ex("ns_object_hand_annotation_data::annotation_color()::ns_number_of_object_hand_annotation_types specified");
			default: throw ns_ex("ns_object_hand_annotation_data::annotation_color()::Unknown annotation type: ") << (int)a;
		}

	}
	ns_object_hand_annotation dominant_label() const{
		if (identified_as_small_larvae)
			return ns_small_larvae;
		if(identified_as_misdisambiguated_multiple_worms)
			return ns_misdisambiguated_multiple_worms;
		if (identified_as_a_mangled_worm)
			return ns_mangled_worm;
		if (identified_as_a_worm_by_human)
			return ns_worm_by_human;
		if (identified_as_a_worm_by_machine)
			return ns_worm_by_machine;
		return ns_non_worm;
	}


	bool identified_as_a_worm_by_human,
		 identified_as_a_worm_by_machine,
		identified_as_a_mangled_worm,
		identified_as_misdisambiguated_multiple_worms,
		identified_as_small_larvae;

	static void out_header(std::ostream & o){
		o << "Identified as a worm by human,"
			 "Identified as a worm by machine,"
			 "Identified as an incorrectly thresholded worm,"
			 "Identified as an incorrectly disambiguated multiple worm group,"
			 "Identified as a small larvae";
	}
	void out_data(std::ostream & o)const{
		o << (identified_as_a_worm_by_human?"1":"0") << "," << 
			(identified_as_a_worm_by_machine?"1":"0") << "," << 
			(identified_as_a_mangled_worm?"1":"0") << "," << 
			(identified_as_misdisambiguated_multiple_worms?"1":"0") << "," << 
			(identified_as_small_larvae?"1":"0");
	}
};


///calculates and stores all feature information about a detected object.  All instances are created by ns_detected_worm_info::generate_stats()
class ns_detected_worm_stats{
public:
	ns_detected_worm_stats():statistics(ns_stat_number_of_stats,0),not_a_worm(false),_model(0){}

	std::vector<double> statistics;

	///specifies thens_svm_model_specification model to be used during feature value scaling
	void specifiy_model(const ns_svm_model_specification & model){_model = &model;}

	///Flagged if the detected worm stats contain invalid information as a way to inform
	///any worm detection performed later on that the region shouldn't be considered a worm
	///even if its stats are good.
	bool not_a_worm;

	
	static void draw_feature_frequency_distributions(const std::vector<ns_detected_worm_stats> & worm_stats, const std::vector<ns_detected_worm_stats> & non_worm_stats,const std::string & label,const std::string &output_directory);

	///returns the number of features calculated for each worm
	unsigned int size() const {return ns_stat_number_of_stats;} 

	///returns the unscaled value of the feature requested.
	inline double& operator[](const ns_detected_worm_classifier & val) {return statistics[(unsigned int)val];}	
	///returns the unscaled value of the feature requested.
	inline const double &operator[] (const ns_detected_worm_classifier & i)  const{return statistics[(unsigned int)i];}
	///returns the unscaled value of the feature requested.
	inline double& operator[](const unsigned int & val) {return statistics[val];}	
	///returns the unscaled value of the feature requested.
	inline const double &operator[] (const unsigned int &i)  const{return statistics[i];}
	
	///returns the value of the feature requested, scaled such that 
	///the feature's standard deviation and mean are ~1.  Uses feature
	///std and average from the specifiedns_svm_model_specification model
	double transformed_statistic(const ns_detected_worm_classifier & val) const;

	///returns the value of the feature requested, scaled such that
	///the feature's value falls between 0 and 1.  Uses feature
	///max and min from the specifiedns_svm_model_specification model
	double scaled_statistic(const ns_detected_worm_classifier & val) const;

	///returns a std::string specifying the set of calculated features to be used by third-party machine learning software 
	std::string parameter_string() const;
	
	void from_string(std::istream & str);
	void from_normalized_string(std::istream & str);

	#ifdef NS_USE_MACHINE_LEARNING
	#ifndef NS_USE_TINYSVM
	///Returns a tinySVM object containing the set of calculated features.  Used by tinySVM library to classify worms.
	svm_node * produce_vector() const;
	///Deallocates memory allocated by produce_vector()
	static void delete_vector(svm_node * node){delete[] node;}
	#endif
	#endif

	///output summary information about the worm to a text file.
	void output_html_worm_summary(std::ostream & out);
	void output_csv_data(const ns_64_bit region_id, const unsigned long capture_time, const ns_vector_2i & position, const ns_vector_2i & size,const ns_object_hand_annotation_data & hand_data,std::ostream & out);

	static void output_csv_header(std::ostream & out);

	///returns the model specification being for feature scaling
	const ns_svm_model_specification & model() const {return *_model;}

	std::string debug_filename;
	ns_region_metadata metadata;
private:
	///the model specification being used for feature scaling
	const ns_svm_model_specification * _model;
};

///Outputs a bitmap for debugging purposes.
void debug_bitmap_output(ns_image_bitmap & bitmap,unsigned int _l);
class ns_image_worm_detection_results;

//all the information needed to classify a worm (intensity values, thresholding of worm and cluster,
//etc, is stored in a worm context image.  These images can be annotated by hand to form 
//gold standard sets etc.

struct ns_worm_context_image{
	enum {ns_pixel_is_in_worm_cluster_bitmap=128,ns_pixel_is_in_worm_bitmap=255};
	ns_image_standard combined_image;
	static void generate(const bool part_of_a_worm_cluster, const ns_vector_2i & worm_cluster_bitmap_offset_in_context_image,
						const ns_image_standard & relative_grayscale, const ns_image_standard & absolute_grayscale,
						const ns_image_bitmap & worm_cluster_bitmap, const ns_image_bitmap & worm_bitmap, ns_image_standard & output);	
	void split(ns_image_standard & relative_grayscale, ns_image_standard & absolute_grayscale,
						ns_image_bitmap & worm_cluster_bitmap, ns_image_bitmap & worm_bitmap);

	ns_image_standard absolute_grayscale,
					  relative_grayscale;
	void deallocate_images(){combined_image.clear();absolute_grayscale.clear();relative_grayscale.clear();}
};


///Certain object features need to be normalized to statistics calculated over the entire image 
///in which they are found (average image pixel intensity, for example).  ns_image_region_stats
///stores such information.
struct ns_whole_image_region_intensity_stats{
	ns_whole_image_region_intensity_stats():minimum_intensity(0),maximum_intensity(0),average_intensity(0){}
	unsigned long minimum_intensity,
				  maximum_intensity,
				  average_intensity;
	bool operator==(const ns_whole_image_region_intensity_stats & i) const{
		return maximum_intensity == i.maximum_intensity &&
			minimum_intensity == i.minimum_intensity &&
			average_intensity == i.average_intensity;
	}
	void calculate(const ns_image_standard & im,const bool ignore_zero);
};

class ns_whole_image_region_stats{
public:


	static ns_whole_image_region_stats null(){return ns_whole_image_region_stats();}
	bool operator==(const ns_whole_image_region_stats & i) const{
		return relative_intensity_stats == i.relative_intensity_stats &&
			   absolute_intensity_stats == i.absolute_intensity_stats;
	}
	bool operator!=(const ns_whole_image_region_stats & i) const{
		return !(*this == i);
	}

	ns_whole_image_region_intensity_stats relative_intensity_stats,
										absolute_intensity_stats;
};

class ns_whole_image_statistic_specification{
public:
	ns_whole_image_region_stats whole_image_region_stats;
	ns_whole_image_region_stats worm_region_specific_region_stats;
};
///Contains all information (spine, bitmap, edge bitmap, grayscale image, etc) pertaining to a contiguous foreground object detected in an image.
class ns_detected_worm_info{
public:
	ns_detected_worm_info():area(0),_bitmap(new ns_image_bitmap),_edge_bitmap(new ns_image_bitmap),
									_spine_visualization(new ns_image_standard),_absolute_grayscale(new ns_image_standard),_relative_grayscale(new ns_image_standard),
									_worm_context_image(new ns_worm_context_image),_bitmap_of_worm_cluster(new ns_image_bitmap),must_be_a_worm(false),must_not_be_a_worm(false),
									interpolated(false),movement_state(ns_movement_not_calculated),part_of_a_multiple_worm_cluster(false),is_a_worm_(false),is_a_worm_set(false),
									region_size(0,0),region_position_in_source_image(0,0),context_position_in_source_image(0,0),context_image_size(0,0){}
	ns_vector_2i region_position_in_source_image;
	ns_vector_2i context_position_in_source_image;
	ns_vector_2i region_size;
	ns_vector_2i context_image_size;

	bool part_of_a_multiple_worm_cluster;

	ns_object_hand_annotation_data hand_annotations;

	unsigned int area;

	ns_whole_image_statistic_specification whole_image_stats;

	//if set to true, the specified region is always counted as a worm in later processing steps, pre-empting SVM categorization
	bool must_be_a_worm;
	//if set to true, the specified region is always counted as dirt (ie not a worm) in later processing steps, pre-empting SVM categorization
	bool must_not_be_a_worm;

	//if set to true, the worm was found through temporal interpolation rather than morphological detection.
	bool interpolated;
	//worms can be flagged as moving or nonmoving.
	ns_movement_state movement_state;

	///Store information about why the specified object was not classified a worm, to be displayed in visualizations
	ns_text_stream_t failure_reason;

	///Contains all information about the detected region's shape
	ns_worm_shape worm_shape;

	std::vector<ns_edge_2d> edges;

	///Returns feature statistics pertaining to the object, to be used in classification
	ns_detected_worm_stats generate_stats() const ;
	
	///Returns the bitmap assiciated with the object.  White pixels are part of the object; black pixels are background.
	inline ns_image_bitmap & bitmap(){
		if (_bitmap == 0)
			throw ns_ex("YIKES");
		return *_bitmap;
	}
	///Returns the bitmap assiciated with the object.  White pixels are part of the object; black pixels are background.
	inline const ns_image_bitmap & bitmap() const {
		if (_bitmap == 0)
			throw ns_ex("YIKES");
		return *_bitmap;
	}

	///Returns the bitmap white pixels around the outside edge of the object. 
	inline ns_image_bitmap & edge_bitmap(){return *_edge_bitmap;}
	///Returns the bitmap white pixels around the outside edge of the object. 
	inline const ns_image_bitmap & edge_bitmap() const {return *_edge_bitmap;}

	
	inline ns_image_bitmap & bitmap_of_worm_cluster(){return *_bitmap_of_worm_cluster;}
	inline const ns_image_bitmap & bitmap_of_worm_cluster() const {return *_bitmap_of_worm_cluster;}

	inline ns_worm_context_image & context_image(){return *_worm_context_image;}
	inline const  ns_worm_context_image & context_image() const{return *_worm_context_image;}
	

	///Returns the grayscale image assiciated with the object.
	inline ns_image_standard & relative_grayscale(){return *_relative_grayscale;}
	inline ns_image_standard & absolute_grayscale(){return *_absolute_grayscale;}
	///Returns the grayscale image assiciated with the object.
	inline const ns_image_standard & relative_grayscale() const {return *_relative_grayscale;}
	inline const ns_image_standard & absolute_grayscale() const {return *_absolute_grayscale;}

	///Returns the precalculated spine_visualization (calculated by ns_spine_drawer during spine calculation by a ns_image_worm_detection_results object)
	inline ns_image_standard & spine_visualization(){return *_spine_visualization;}
	inline ns_svg & spine_svg_visualization(){return _spine_svg_visualization;}
	///Returns the precalculated spine_visualization (calculated by ns_spine_drawer during spine calculation by a ns_image_worm_detection_results object)
	inline const ns_image_standard & spine_visualization() const {return *_spine_visualization;}
	inline const ns_svg & spine_svg_visualization() const {return _spine_svg_visualization;}

	///Draws a std::vector on the specified bitmap representing the angle of the major axis of the worm
	void draw_orientation_vector(ns_image_standard & out) const;

	///returns a graph of the object width as a function of arclength down the spine
	ns_image_standard * width_graph();
	void width_graph(ns_svg & svg);
	///returns a graph of the spine curvature as a function of arclength down the spine
	ns_image_standard * curvature_graph();
	void curvature_graph(ns_svg & svg);

	typedef enum {ns_vis_none,ns_vis_raster,ns_vis_svg,ns_vis_both} ns_visualization_type;
	typedef enum {ns_large_source_grayscale_images_provided,ns_individual_worm_source_grayscale_images_provided} ns_grayscale_image_type;
	///from_segment_cluster_solution() takes a std::vector of potential worms
	///and appends any objects classified as worms by the machine learning algorithm
	///to the provided worms std::vector.  objects are appended at the position specified by offest.  
	///The number of objects classified as worms is returned.
	///This is a bit convolutied (apologies) but isstructured as such
	///to avoid unneccisary repeated constructor calls to ns_detected_worm_info objects.
	static unsigned int from_segment_cluster_solution(ns_detected_object & region, 
									std::vector<ns_detected_worm_info> & worms, 
									unsigned int offset, std::vector<std::vector<ns_detected_worm_info *> > & mutually_exclusive_groups, 
									const ns_image_standard & relative_grayscale_image,
									const ns_image_standard & absolute_grayscale_image, 
									const ns_grayscale_image_type & type,
									const ns_visualization_type generate_visualization=ns_vis_raster);

	///Single contiguous regions can of course contain multiple worms, and 
	///the region bitmap must be divvied up between all the spines.
	///calculate_bitmap_corresponding_to_spine_segment() decides which pixels of the overall bitmap
	///should be assigned to the specified spine.
	static void calculate_bitmap_corresponding_to_worm_shape(const ns_worm_shape & worm_shape, ns_image_bitmap & bitmap);
	
	///Single contiguous regions can of course contain multiple worms, and 
	///the region bitmap must be divvied up between all the spines.
	///calculate_bitmap_corresponding_to_spine_segment() decides which pixels of the overall bitmap
	///should be assigned to the specified spine.
	void calculate_bitmap_corresponding_to_spine_segment(ns_detected_worm_info & worm, ns_image_bitmap & bitmap);

	///Using the specified machine learning model, returns true if the region is classified as a worm, false if it is not.
	bool is_a_worm(const ns_svm_model_specification & model);
	bool is_a_worm();


	
	~ns_detected_worm_info();
	void copy(const ns_detected_worm_info & wi);

	///copy constructor to allow placement in an std::std::vector
	ns_detected_worm_info(const ns_detected_worm_info & wi);
	ns_detected_worm_info & operator=(const ns_detected_worm_info & wi){copy(wi);return *this;}

	///To build machine learning test sets, objects should be inspected
	///in context of their surroundings.  generate_context_image returns
	///an image representing the worm (in color) against its background.
	void generate_training_set_visualization(ns_image_standard & output) const;
	void from_training_set_visualization(ns_worm_context_image & context_visualization, const ns_vector_2i & region_size, const ns_vector_2i & region_offset_in_context_image, const ns_vector_2i & context_image_size);

	///returns the spatial center of the worm (not the center of the spine)
	ns_vector_2d worm_center() const;

	void accept_images(ns_image_bitmap * bitmap,ns_image_bitmap * edge_bitmap, ns_image_standard * relative_grayscale, ns_image_standard * absolute_grayscale){
		if (_bitmap != 0)
			delete _bitmap;	
		if (_edge_bitmap != 0)
			delete _edge_bitmap;
		if (_absolute_grayscale != 0)
			delete _absolute_grayscale;
		if (_relative_grayscale != 0)
			delete _relative_grayscale;
		_bitmap = bitmap;
		_edge_bitmap = edge_bitmap;
		_absolute_grayscale = absolute_grayscale;
		_relative_grayscale = relative_grayscale;
	}
	

	//void set_bitmap_offset_in_context_image(const ns_vector_2i & v){bitmap_offset_in_context_image = v;}
	//void set_bitmap_size_in_context_image(const ns_vector_2i & v){bitmap_size_in_context_image = v;}
	inline ns_vector_2i get_region_offset_in_context_image()const{return region_position_in_source_image-context_position_in_source_image;}
private:
	
	void crop_images_to_region_size(const ns_vector_2i & region_offset_in_context_image);

		///the _worm_context_image is larger than the _bitmap image;
	///bitmap_bottom_in_context_image stores the offset of _bitmap
	///in _worm_context_image
	//ns_vector_2i bitmap_offset_in_context_image;
	//ns_vector_2i bitmap_size_in_context_image;

	ns_image_bitmap * _bitmap,
					* _edge_bitmap;
	ns_image_standard * _absolute_grayscale,
					 * _relative_grayscale,
					* _spine_visualization;
	ns_svg _spine_svg_visualization;

	ns_worm_context_image * _worm_context_image;

	ns_image_bitmap * _bitmap_of_worm_cluster;

	void build_object_context_image(ns_detected_object & region, const ns_image_standard & absolute_grayscale, const ns_image_standard & relative_grayscale);
	void extract_grayscale_from_large_image(const ns_vector_2i & position, const ns_image_standard & absolute_grayscale,const ns_image_standard & relative_grayscale_image);
	void finalize_stats_from_shape();

	void make_width_graph(ns_graph &graph);
	void make_curvature_graph(ns_graph &graph);

	bool is_a_worm_;
	bool is_a_worm_set;
};

struct ns_text_label{
	ns_text_label();
	ns_text_label(const ns_vector_2i & v, const std::string & t):pos(v),text(t){}
	ns_vector_2i pos;
	std::string text;
};


///After all worms are detected, a collage is made of all their bitmaps so that worm shapes can
///be inspected without having to re-perform thresholding to the original grayscale image in which
///worms were found.  ns_worm_collage_storage holds this collage
//The images are stored as context images, i.e. with an extra ring around the data of the close crop
class ns_worm_collage_storage{
public:
	ns_worm_collage_storage():id(0){}
	unsigned long id;
	void generate_collage();
	unsigned long number_of_worms();
	void extract_region_image(const unsigned long & i, ns_image_standard & im);
	void extract_region_context_image(const unsigned long & i, ns_image_standard & im);
	void load_images_from_db(ns_image_server_captured_image_region & region,const unsigned long expected_number_of_worms,ns_sql & sql,const bool interpolated, const bool only_load_context_absolute_grayscale);
	void populate_worm_images(std::vector<ns_detected_worm_info> & worms,const bool interpolated,const bool  only_load_context_absolute_grayscle=false);
	void specifiy_region_sizes(const std::vector<ns_detected_worm_info> & worms);
	ns_collage_info & info(){return collage_info;}
	const ns_image_standard & generate_collage(const ns_image_standard & absolute_grayscale,const ns_image_standard & relative_grayscale,const ns_image_standard & threshold,const std::vector<ns_detected_worm_info *> & worms);
	~ns_worm_collage_storage(){clear();}
	void clear();
	static ns_vector_2i context_border_size(){return ns_vector_2i(50,50);}
		
private:
	std::vector<ns_worm_context_image *> context_images;
	std::vector<ns_image_standard *> absolute_region_images,
									 relative_region_images,
									 bitmaps;

	ns_vector_2i region_context_position(const unsigned long & i);

	ns_collage_info collage_info;
	ns_vector_2i context_image_sizes(const unsigned long i)const{return region_image_sizes[i]+ns_vector_2i(context_border_size()*2);}
	std::vector<ns_vector_2i> region_image_sizes;
	std::vector<ns_vector_2i> region_offsets_in_context_image;
	ns_image_standard collage_cache;
};

///ns_image_worm_detection_results contains all information produced during nematode detection, such as
///identified worms, identified non-worms, etc.
///Calls to ns_detected_object_identifier::detect_worms() returns an ns_image_worm_detection_results object
///that summarizes the result of worm detection
class ns_image_worm_detection_results{
public:
	ns_64_bit id,
				 source_image_id,
				 capture_sample_id,
				 region_info_id;
	unsigned long capture_time;

	//ns_object_hand_annotation_data hand_annotations;

	ns_worm_collage_storage worm_collage;

	const ns_image_standard & generate_region_collage(const ns_image_standard & absolute_grayscale,const ns_image_standard & relative_grayscale, const ns_image_standard & threshold){
		return worm_collage.generate_collage(absolute_grayscale,relative_grayscale,threshold,actual_worms);
	}

	ns_image_worm_detection_results():id(0),source_image_id(0),capture_sample_id(0),region_info_id(0),capture_time(0){}
	///returns the number of worms detected in an image
	const unsigned int number_of_worms()const { return (unsigned int)actual_worms.size();}

	///clears all detection results
	void clear();

	std::map<std::string,unsigned long> give_worm_rejection_reasons() const;
	
	///process_regions() takes a set of contiguous regions (each of which may contain multiple worms)
	///and loads them into ns_image_worm_detection_results structures in preparation for classification 
	///into worm/non-worm categories.
	///Ownership of contiguous regions are taken by the ns_image_worm_detection object and so the regions std::vector is cleared.
	void process_segment_cluster_solutions(std::vector<ns_detected_object *> & objects, const ns_image_standard &relative_grayscale_source, 
			const ns_image_standard & absolute_grayscale_source,  const ns_detected_worm_info::ns_visualization_type visualization_type, const unsigned long maximum_number_of_worms=0);

	///Certain object features need to be normalized to statistics calculated over the entire image 
	///in which they are found (average image pixel intensity, for example).  calculate_image_region_stats()
	///calculates such information if necissary, stores it in the provided stats object, and then provides
	///the statistics to each detected object so that each object can normalize it's features.
	void calculate_image_region_stats();

	
	void set_whole_image_region_stats(const ns_whole_image_region_stats & stats);

	///Once contiguous regions are loaded into an ns_image_worm_detection object, they are sorted into worm and non-worm classes
	///by sort_putative_worms().  This sorting is complicated by the fact that spine topology disambiguation doesn't provide
	///single solutions for multiple-touching-worm disambiguation, but rather provides several possible solutions.  The ideal
	///solution to such multiple-touching-worm disambiguoation is made by picking the best solution from a mutually-exclusive
	///set of possible solutions.  This choice is made by sort_putative_worms().
	void sort_putative_worms(const ns_svm_model_specification & model);



	///The results of a worm detection can be stored in a database for future reference.  Only data about worms is stored; non-worm information
	///is discarded to save space.  Data is stored in a table at a row identified by the ns_image_worm_detection_results::id member.  if ns_image_worm_detection_results::id
	///is set to zero, a new entry is made in the database.

	void clear_images();
	
	void output_feature_statistics(std::ostream & o);

	void load_images_from_db(ns_image_server_captured_image_region & region, ns_sql & sql, bool interpolated=false, bool only_load_context_absolute_grayscale=false);

	///Creates a visualzation of object detection and worm classification.  The grayscale source image is drawn
	///with detected worms and non-worms are overlaid in color, with the additional option of drawing crosses over identified worms.
	void create_visualization(const unsigned int cross_height, const unsigned int cross_thickness, ns_image_standard & image, const std::string & data_label = "", const bool mark_crosshairs = true, const bool draw_labels = true, const bool draw_non_worms = true);
	
	///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
	///create_spine_visualizations() makes a collage of each worm's visualization.
	void create_spine_visualizations(ns_image_standard  & reciever);
	void create_spine_visualizations(std::vector<ns_svg> & objects);

	void create_edge_visualization(ns_image_standard & reciever);

	///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
	///create_spine_visualizations() makes a collage of each worm's spine visualization, justaposed
	///with graphs of various statistics calculated as a function of arclength along the worm spine (width, curvature, etc)
	void create_spine_visualizations_with_stats(ns_image_standard & reciever);
	void create_spine_visualizations_with_stats(std::vector<ns_svg> & objects);

	///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
	///create_spine_visualizations() makes a collage of each non-worm's visualization.
	///The resulting collage tends to be very large due to the large number of dirt objects present
	void create_reject_spine_visualizations(ns_image_standard & reciever);
	void create_reject_spine_visualizations(std::vector<ns_svg> & objects);

	///During object detection, an visualzation of an object's Delauny triangular mesh and spine is made.
	///create_spine_visualizations() makes a collage of each non-worm's spine visualization, justaposed
	///with graphs of various statistics calculated as a function of arclength along the worm spine (width, curvature, etc)
	void create_reject_spine_visualizations_with_stats(ns_image_standard & reciever);
	void create_reject_spine_visualizations_with_stats(std::vector<ns_svg> & objects);

	void add_interpolated_worm_area(const ns_interpolated_worm_area & worm_area){interpolated_worm_areas.push_back(worm_area);}


	///returns the number of contiguous regions detected in an image
	unsigned int number_of_putative_worms() const { return (unsigned int)putative_worms.size();}
	///returns the number of contiguous regions classified as worms
	unsigned int number_of_actual_worms() const { return (unsigned int)actual_worms.size();}
	///returns the number of contiguous regions classified as non-worms
	unsigned int number_of_non_worms() const { return (unsigned int)not_worms.size();}


	///returns the grayscale image of the specified contiguous region
	template<class whole_image> 
	void get_putative_worm_image(const unsigned int worm_id, whole_image & reciever){make_worm_image(putative_worms[worm_id],reciever);}
	
	///returns an image of the specified contiguous region to be used for making
	///test and training sets to train machine learning classifiers
	template<class whole_image> 
	void generate_putative_training_set_visualization(const unsigned int worm_id,whole_image & reciever){putative_worms[worm_id].generate_training_set_visualization(reciever);}
	
	///returns the grayscale image of the specified worm-classified contiguous region
	template<class whole_image> 
	void get_actual_worm_image(const unsigned int worm_id, whole_image & reciever){make_worm_image(*actual_worms[worm_id],reciever);}			
	
	///returns an image of the specified worm-classified contiguous region to be used for making
	///test and training sets to train machine learning classifiers
	template<class whole_image> 	
	void generate_actual_training_set_visualization(const unsigned int worm_id,whole_image & reciever){actual_worms[worm_id]->generate_training_set_visualization(reciever);}

	///returns the grayscale image of the specified non-worm-classified contiguous region
	template<class whole_image> 
	void get_non_worm_image(const unsigned int worm_id, whole_image & reciever){make_worm_image(*not_worms[worm_id],reciever);}	

	///returns an image of the specified non-worm-classified contiguous region to be used for making
	///test and training sets to train machine learning classifiers
	template<class whole_image> 
	void generate_non_worm_training_set_visualization(const unsigned int worm_id,whole_image & reciever){not_worms[worm_id]->generate_training_set_visualization(reciever);}

	//returns the feature statistics calculated for the specified contiguous-region
	ns_detected_worm_stats get_putative_worm_stats(const unsigned int id) const {return putative_worms[id].generate_stats();}

	///creates a collage of grayscale images of all contiguous regions classified as worms
/*	template<class whole_image>
	void create_grayscale_object_collage(whole_image  & reciever){
		cerr << "MAKE SURE TO DEBUG THIS";
		if (worm_region_collage.properties().height!=0)
			worm_region_collage.pump(reciever,1024);
		else{
			//std::vector<ns_image_standard> im(actual_worms.size());
			std::vector<const whole_image *> images(actual_worms.size());
			for (unsigned int i = 0; i < actual_worms.size(); i++){
				//actual_worms[i]->grayscale().pump(im[i],512);
				//cerr << "grayscale res = " << im[i].properties().resolution << "\n";
				//mask non-worm regions for visualization
				//for (unsigned int y = 0; y < actual_worms[i]->grayscale().properties().height; y++)
				//	for (unsigned int x = 0; x < actual_worms[i]->grayscale().properties().width; x++)
				//		if (!actual_worms[i]->bitmap()[y][x])
				//			im[i][y][x] = 0;
				images[i] = &actual_worms[i]->grayscale();
			}
			make_collage(images, reciever, 128);
		}
	}*/


	///returns the set of all contiguous regions classified as worms
	
	//const std::vector<ns_detected_worm_info *> & actual_worm_list() {return actual_worms;}
	const std::vector<const ns_detected_worm_info *> & actual_worm_list() const{
		return * reinterpret_cast<const std::vector<const ns_detected_worm_info *> *> (&actual_worms);

	}
	///returns the set of all contiguous regions classified as non-worms
	// const std::vectoconr<ns_detected_worm_info *> & non_worm_list() {return not_worms;}
	 const std::vector<const ns_detected_worm_info *> &non_worm_list() const{
		return * reinterpret_cast<const std::vector<const ns_detected_worm_info *> *> (&not_worms);
	 }

	 	///returns the set of all contiguous regions classified as non-worms
	const  std::vector<const ns_interpolated_worm_area> & interpolated_worm_area_list(){
		return * reinterpret_cast<const std::vector<const ns_interpolated_worm_area> *> (&interpolated_worm_areas);
	 }

	 void mark_all_as_interpolated(const bool interpolated = true){
		for (unsigned int i = 0; i < putative_worms.size(); i++)
			putative_worms[i].interpolated = interpolated;
	}

	void save_to_db(ns_sql & sql, const bool interpolated,const bool only_output_movement_tags=false){
		ns_image_server_captured_image_region tmp;
		save(tmp,interpolated,false,only_output_movement_tags,sql);
	}
	void save_to_disk(ns_image_server_captured_image_region & source_region, const bool interpolated,ns_sql & sql){
		save(source_region,interpolated,true,false,sql);
	}

	///Loades detected worm information from the database.  Data is loaded from the entry who's id is stored in ns_image_worm_detection_results::id
	void load_from_db(const bool load_worm_shape,const bool images_comes_from_interpolated_annotations,ns_sql & sql,const bool delete_from_db_on_error=true );

	std::vector<ns_detected_worm_info> & replace_actual_worms_access(){
		putative_worms.resize(0);
		not_worms.resize(0);
		actual_worms.resize(0);
		return putative_worms;
	}

	void replace_actual_worms(){
		actual_worms.resize(putative_worms.size());
		for (unsigned int i = 0; i < putative_worms.size(); i++){
			actual_worms[i] = &putative_worms[i];
		}
	}
private:
	
    ns_image_server_image data_storage_on_disk;
	void save(ns_image_server_captured_image_region & source_region,const bool interpolated, const bool save_to_disk, const bool only_output_movement_tags,ns_sql & sql);

	void save_data_to_disk(ns_image_server_captured_image_region & region,const bool interpolated, ns_sql & sql);

	void load_data_from_db(ns_sql & sql);
	void load_data_from_disk(ns_sql & sql);

	///The set of all contiguous regions detected in an image
	std::vector<ns_detected_worm_info> putative_worms;
	
	///The set of all detected contiguous regions classified that have been classified as worms
	std::vector<ns_detected_worm_info * > actual_worms;

	///The set of all detected contiguous regions classified that have been classified as non-worms
	std::vector<ns_detected_worm_info * > not_worms;

	///Spine detection doesn't produce one best disambiguation between multiple-touching worms.  It produces
	///a set of mutually exclusive solutions from which one must be picked.  mutually_exclusive_worm_groups
	///stores these choices.  mutually_exclusive_worm_groups[0..N] each represents a single multually-exclusive set
	///mutually_exclusive_worm_groups[0][0...N] represents one possible solution
	///mutually_exclusive_worm_groups[0][0][N] contains the worm spine choices made by the specified solution
	std::vector< std::vector< std::vector<ns_detected_worm_info *> > >mutually_exclusive_worm_groups;

	///areas where worms are known to reside (through temporal interpolation), but to which exact statistics
	///cannot be calculated
	std::vector<ns_interpolated_worm_area> interpolated_worm_areas;

	///Visualzations allow text to be placed next to each detected worm, explaining why it was classified in a certain way.
	///region_labes stores this information.
	std::vector<ns_text_label> region_labels;


	friend class ns_nearest_neighbor_map;
	friend class ns_worm_training_set_image;
	
	void add_data_to_db_query(ns_sql & sql);

};


unsigned int ns_count_number_of_worms_in_detected_object_group(std::vector<ns_detected_object *> & objects);


void ns_calculate_res_aware_edges(ns_image_bitmap & im, ns_image_bitmap & edge_bitmap, std::vector<ns_vector_2d> & output_coordinates, std::vector<ns_vector_2d> & holes, std::vector<ns_edge_ui> & edge_list, std::vector<ns_edge_2d> &edges);
void ns_calculate_res_aware_edges(ns_image_bitmap & im, ns_image_bitmap & edge_bitmap, std::vector<ns_edge_2d> &edges);
#endif
