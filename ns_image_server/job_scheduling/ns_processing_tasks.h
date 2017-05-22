#ifndef NS_PROCESSING_TASKS_H
#define NS_PROCESSING_TASKS_H

typedef enum{  
	ns_unprocessed , 
	ns_process_apply_mask ,
	ns_process_spatial, 
	ns_process_lossy_stretch,
	ns_process_threshold,  
	ns_process_thumbnail,  
	ns_process_worm_detection, 
	ns_process_worm_detection_labels,
	ns_process_worm_detection_with_graph,
	ns_process_region_vis, 
	ns_process_region_interpolation_vis,
	ns_process_accept_vis,
	ns_process_reject_vis,
	ns_process_interpolated_vis,
	ns_process_add_to_training_set,
	ns_process_analyze_mask,
	ns_process_compile_video,
	ns_process_movement_coloring,
	ns_process_movement_mapping, 
	ns_process_posture_vis,
	ns_process_movement_coloring_with_graph,
	ns_process_heat_map,
	ns_process_static_mask,
	ns_process_compress_unprocessed,
	ns_process_movement_coloring_with_survival,
	ns_process_movement_paths_visualization,
	ns_process_movement_paths_visualition_with_mortality_overlay,
	ns_process_movement_posture_visualization,
	ns_process_movement_posture_aligned_visualization,
	ns_process_unprocessed_backup,
	ns_process_last_task_marker,
} ns_processing_task;

#endif
