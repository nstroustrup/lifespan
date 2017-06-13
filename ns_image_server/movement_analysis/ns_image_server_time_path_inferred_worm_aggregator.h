#ifndef NS_INFERRED_WORM_AGGREGATOR
#define NS_INFERRED_WORM_AGGREGATOR

#include "ns_image_server_images.h"
#include "ns_time_path_solver.h"
#include "ns_detected_worm_info.h"
#include "ns_thread_pool.h"
#include <map>
#include <iostream>

struct ns_image_server_time_path_inferred_worm_aggregator_image_info{
	unsigned long time;
	ns_64_bit	id,
				raw_image_id,
				  spatial_id,
				  threshold_id,
				  region_interpolated_id,
				  worm_detection_results_id,
				  worm_interpolated_results_id;
	bool problem,censored;
	//the lifespan machine can occasionally duplicate a time point.
	//we need to keep those up to date as well so they remain identical
	//and don't cause book keeping problems.

	//the duplication appears to occur by a sample image being masked twice simulaneously
	//by two image processing servers.  This should be fixable--probably a race condition
	//in the job processing code.
	std::vector<ns_image_server_time_path_inferred_worm_aggregator_image_info> duplicates_of_this_time_point;

	void remove_inferred_element_from_db(ns_sql & sql) const{
		if (region_interpolated_id != 0){
			ns_image_server_image im;
			im.load_from_db(region_interpolated_id,&sql);
			image_server.image_storage.delete_from_storage(im,ns_delete_long_term,&sql);
			sql << "UPDATE sample_region_images SET "
				<< ns_processing_step_db_column_name(ns_process_region_interpolation_vis)
				<< " = 0, worm_interpolation_results_id = 0 WHERE id = " << id;
			sql.send_query();

		}
		else{
			sql << "UPDATE sample_region_images SET worm_interpolation_results_id = 0 WHERE id = " << id;
			sql.send_query();
		}
		sql << "DELETE FROM worm_detection_results WHERE id = " << worm_interpolated_results_id;
		sql.send_query();
	}
	ns_image_server_time_path_inferred_worm_aggregator_image_info():problem(false),censored(false){}
};





class ns_image_server_time_path_inferred_worm_aggregator {
public:
	//creates new region interpolated images for locations where worms are inferred to exist.
	//stores their location in the collage image to the context_image_position_in_region_vis_image member 
	//of each inferred element in the solution.

	//returns false if corrupt images were found and the time path solution needs to be rebuilt
	bool create_images_for_solution(const ns_64_bit region_info_id, ns_time_path_solution & s, ns_sql & sql);
};

#endif

