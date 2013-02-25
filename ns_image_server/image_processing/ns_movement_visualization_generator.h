#ifndef NS_MOVEMENT_VISUALIZTION_GENERATOR
#define NS_MOVEMENT_VISUALIZTION_GENERATOR
//#include "ns_movement_characterization.h"
#include "ns_graph.h"
#include "ns_machine_analysis_data_loader.h"

class ns_movement_visualization_generator{
public:
	void create_area_graph_for_capture_time(const long marker_time, const ns_worm_movement_summary_series & data, ns_graph & graph,  
		const std::string & title, const bool draw_dark,const bool optimize_for_small_graph,const ns_region_metadata & metadata) const;
	void create_scatter_proportion_graph_for_capture_time(const long marker_time, const ns_worm_movement_summary_series & data, 
		ns_graph & graph,  const std::string & title, const bool draw_dark,const bool optimize_for_small_graph,const ns_region_metadata & metadata) const;

	void create_survival_curve_for_capture_time(const long marker_time, const ns_region_metadata & metadata,const ns_survival_data_with_censoring & plate, 
											const ns_survival_data_with_censoring & strain, const std::vector<unsigned long > & plate_time, 
											const std::vector<unsigned long> & strain_time,const std::string & title, const bool draw_dark,const bool optimize_for_small_graph, ns_image_standard & im,ns_graph & graph) const;

	void create_time_path_analysis_visualization(const ns_image_server_captured_image_region & region, const ns_death_time_annotation_compiler_region & compiler_region,
													const ns_image_standard & grayscale, ns_image_standard & out,ns_sql & sql);

private:
	void make_x_axis(const ns_worm_movement_summary_series & movement_data, ns_graph_object & graph_x_axis,const ns_region_metadata & metadata) const;
};



#endif

