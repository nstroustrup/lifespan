AUTOMAKE_OPTIONS = subdir-objects

IS_DIR = ns_image_server/
DA_DIR = ns_image_server/data_annotation/
IA_DIR = ns_image_server/image_acquisition/
IB_DIR = ns_image_server/image_base/
IP_DIR = ns_image_server/image_processing/
JS_DIR = ns_image_server/job_scheduling/
MA_DIR = ns_image_server/movement_analysis/
ST_DIR = ns_image_server/statistics/
SB_DIR = ns_image_server/system_base/

src_base = ns_image_server/ns_file_location_specification.h \
		ns_image_server/ns_ini.h \
		ns_image_server/ns_image_statistics.cpp \
		ns_image_server/ns_image_storage_handler.cpp \
		ns_image_server/ns_image_storage_handler.h \
		ns_image_server/ns_single_thread_coordinator.h \
		ns_image_server/ns_image_server_main.cpp \
		ns_image_server/ns_progress_reporter.h \
		ns_image_server/ns_simple_image_cache.h \
		ns_image_server/ns_image_stream_buffers.h \
		ns_image_server/ns_image_server.h \
		ns_image_server/ns_image_server_message.cpp \
		ns_image_server/ns_image_server_message.h \
		ns_image_server/ns_image_server_stock_quotes.h \
		ns_image_server/ns_get_double.h \
		ns_image_server/ns_image_storage.h \
		ns_image_server/ns_performance_statistics.h \
		ns_image_server/ns_high_precision_timer.h \
		ns_image_server/ns_hand_annotation_loader.h \
		ns_image_server/ns_image_simple_cache.h \
		ns_image_server/ns_graph.h \
		ns_image_server/ns_ini.cpp \
		ns_image_server/ns_image_server.cpp \
		ns_image_server/ns_image_server_sql.h \
		ns_image_server/ns_image_server_images.h \
		ns_image_server/ns_image_server_results_storage.h \
		ns_image_server/ns_image_statistics.h \
		ns_image_server/ns_graph.cpp \
		ns_image_server/ns_image_server_images.cpp 
		
src_data_annotation = ns_image_server/data_annotation/ns_experiment_storyboard.cpp \
		      ns_image_server/data_annotation/ns_death_time_annotation.h \
		      ns_image_server/data_annotation/ns_mask_management.cpp \
		      ns_image_server/data_annotation/ns_death_time_annotation.cpp \
		      ns_image_server/data_annotation/ns_machine_learning_training_set.h \
		      ns_image_server/data_annotation/ns_death_time_annotation_set.h \
		      ns_image_server/data_annotation/ns_region_metadata.h \
		      ns_image_server/data_annotation/ns_mask_management.h \
		      ns_image_server/data_annotation/ns_machine_learning_training_set.cpp \
		      ns_image_server/data_annotation/ns_experiment_storyboard.h
src_image_acquisition = ns_image_server/image_acquisition/ns_usb.cpp \
			ns_image_server/image_acquisition/ns_image_capture_data_manager.cpp \
			ns_image_server/image_acquisition/ns_captured_image_statistics_set.h \
			ns_image_server/image_acquisition/ns_asynchronous_read.h \
			ns_image_server/image_acquisition/ns_buffered_capture_scheduler.h \
			ns_image_server/image_acquisition/ns_image_server_dispatcher.h \
			ns_image_server/image_acquisition/ns_capture_device_manager.cpp \
			ns_image_server/image_acquisition/ns_barcode.cpp \
			ns_image_server/image_acquisition/ns_image_cache.h \
			ns_image_server/image_acquisition/ns_image_capture_data_manager.h \
			ns_image_server/image_acquisition/ns_capture_device.h \
			ns_image_server/image_acquisition/ns_image_server_dispatcher.cpp \
			ns_image_server/image_acquisition/ns_capture_device.cpp \
			ns_image_server/image_acquisition/ns_image_server_automated_job_scheduler.h \
			ns_image_server/image_acquisition/ns_capture_device_manager.h \
			ns_image_server/image_acquisition/ns_image_server_alerts.h \
			ns_image_server/image_acquisition/ns_capture_schedule.h \
			ns_image_server/image_acquisition/ns_buffered_capture_scheduler.cpp \
			ns_image_server/image_acquisition/ns_barcode.h \
			ns_image_server/image_acquisition/ns_image_server_alerts.cpp \
			ns_image_server/image_acquisition/ns_usb.h \
			ns_image_server/image_acquisition/ns_capture_schedule.cpp
src_image_base = ns_image_server/image_base/ns_svg.h \
		 ns_image_server/image_base/ns_vector_bitmap_interface.cpp \
		 ns_image_server/image_base/ns_image_socket.cpp \
		 ns_image_server/image_base/ns_jpeg.h \
		 ns_image_server/image_base/ns_tiff.h \
		 ns_image_server/image_base/xs_Config.h \
		 ns_image_server/image_base/ ns_image_easy_io.h \
		 ns_image_server/image_base/ns_libtiff_interface.c \
		 ns_image_server/image_base/ ns_libtiff_interface.h \
		 ns_image_server/image_base/ns_buffered_random_access_image.h \
		 ns_image_server/image_base/ns_ojp2k.cpp \
		 ns_image_server/image_base/ns_svg.cpp \
		 ns_image_server/image_base/ns_image_easy_io.h.bak \
		 ns_image_server/image_base/ns_vector_bitmap_interface.h \
		 ns_image_server/image_base/ns_vector.h \
		 ns_image_server/image_base/ns_image.cpp \
		 ns_image_server/image_base/ns_jpeg.cpp \
		 ns_image_server/image_base/ns_vector.cpp \
		 ns_image_server/image_base/ns_image_socket.h \
		 ns_image_server/image_base/ns_image.h \
		 ns_image_server/image_base/ns_font.cpp \
		 ns_image_server/image_base/ns_ojp2k.h \
		 ns_image_server/image_base/xs_Float.h \
		 ns_image_server/image_base/ns_font.h \
		 ns_image_server/image_base/ns_xmp_encoder.h \
		 ns_image_server/image_base/ns_tiff.cpp
src_image_processing = ns_image_server/image_processing/ns_worm_detector.cpp \
		       ns_image_server/image_processing/ns_xvid.cpp \
		       ns_image_server/image_processing/ns_movement_visualization_generator.cpp \
		       ns_image_server/image_processing/ns_movement_visualization_generator.h  \
		       ns_image_server/image_processing/ns_worm_detection_constants.cpp \
		       ns_image_server/image_processing/ns_image_tools.h \
		       ns_image_server/image_processing/ns_worm_training_set_image.cpp \
		       ns_image_server/image_processing/ns_worm_training_set_image.h \
		       ns_image_server/image_processing/ns_worm_detection_constants.h \
		       ns_image_server/image_processing/ns_spatial_avg.h \
		       ns_image_server/image_processing/ns_svm_model_specification.h \
		       ns_image_server/image_processing/ns_node_topology_primitives.h \
		       ns_image_server/image_processing/ns_node_topology.h \
		       ns_image_server/image_processing/ns_segment_topology.cpp \
		       ns_image_server/image_processing/ns_region_growing_segmenter.h \
		       ns_image_server/image_processing/ns_process_mask_regions.h \
		       ns_image_server/image_processing/ns_identify_contiguous_bitmap_regions.h \
		       ns_image_server/image_processing/ns_detected_worm_info.cpp \
		       ns_image_server/image_processing/ns_bspline.cpp \
		       ns_image_server/image_processing/ns_process_16_bit_images.h \ \
		       ns_image_server/image_processing/ns_spatial_avg.cpp \
		       ns_image_server/image_processing/ns_heat_map_interpolation.cpp \
		       ns_image_server/image_processing/ns_xvid.h \
		       ns_image_server/image_processing/ns_bspline.h \
		       ns_image_server/image_processing/ns_resampler.h \
		       ns_image_server/image_processing/ns_image_registration.h \
		       ns_image_server/image_processing/ns_worm_detector.h \
		       ns_image_server/image_processing/ns_difference_thresholder.cpp \
		       ns_image_server/image_processing/ns_complex_segment_cluster_solver.h \
		       ns_image_server/image_processing/ns_image_tools.cpp \
		       ns_image_server/image_processing/ns_node_topology.cpp \
		       ns_image_server/image_processing/ns_identify_contiguous_bitmap_regions.cpp \
		       ns_image_server/image_processing/ns_detected_worm_info.h \
		       ns_image_server/image_processing/ns_detected_object.h \
                       ns_image_server/image_processing/ns_difference_thresholder.h_server  \
		       ns_image_server/image_processing/ns_spine_drawer.cpp \
		       ns_image_server/image_processing/ns_spine_drawer.h \
		       ns_image_server/image_processing/ns_complex_segment_cluster_solver.cpp \
		       ns_image_server/image_processing/ns_heat_map_interpolation.h \
		       ns_image_server/image_processing/ns_segment_topology.h
src_job_scheduling =   ns_image_server/job_scheduling/ns_processing_job_push_scheduler.h \
		       ns_image_server/job_scheduling/ns_processing_job_pull_scheduler.h \
		       ns_image_server/job_scheduling/ns_processing_tasks.h \
		       ns_image_server/job_scheduling/ns_processing_job_processor.h \
		       ns_image_server/job_scheduling/ns_processing_job_scheduler.cpp \
		       ns_image_server/job_scheduling/ns_processing_job_scheduler.h \
                       ns_image_server/job_scheduling/ns_processing_job.hutogen.sh \
		       ns_image_server/job_scheduling/ns_image_processing_pipeline.cpp \
		       ns_image_server/job_scheduling/ns_processing_job_processor.cpp \
		       ns_image_server/job_scheduling/ns_image_processing_pipeline.h \
		       ns_image_server/job_scheduling/ns_processing_job_push_scheduler.cpp
src_movement_analysis = ns_image_server/movement_analysis/ns_time_path_solver.cpp \
			ns_image_server/movement_analysis/ns_image_server_time_path_inferred_worm_aggregator.h \
			ns_image_server/movement_analysis/ns_hidden_markov_model_posture_analyzer.cpp \
			ns_image_server/movement_analysis/ns_time_path_solver.h \
			ns_image_server/movement_analysis/ns_time_path_image_analyzer.h \
			ns_image_server/movement_analysis/ns_threshold_movement_posture_analyzer.h \
			ns_image_server/movement_analysis/ns_time_path_posture_movement_solution.h \
			ns_image_server/movement_analysis/ns_hidden_markov_model_posture_analyzer.h \
			ns_image_server/movement_analysis/ns_hidden_markov_model.h \
			ns_image_server/movement_analysis/ns_time_path_image_analyzer.cpp \
src_statistics = ns_image_server/statistics/ns_movement_measurement.h \
		 ns_image_server/statistics/ns_survival_curve.cpp \
		 ns_image_server/statistics/ns_lifespan_statistics.h \
		 ns_image_server/statistics/ns_linear_regression_model.h \
		 ns_image_server/statistics/ns_posture_analysis_models.h \
		 ns_image_server/statistics/ns_normal_distribution.cpp \
		 ns_image_server/statistics/ns_by_hand_lifespan.h \
		 ns_image_server/statistics/ns_jmp_file.h \
		 ns_image_server/statistics/ns_machine_analysis_data_loader.cpp \
		 ns_image_server/statistics/ns_survival_curve.h \
		 ns_image_server/statistics/ns_captured_image_statistics_set.cpp \
		 ns_image_server/statistics/ns_movement_measurement.cpp \
		 ns_image_server/statistics/ns_normal_distribution.h \
		 ns_image_server/statistics/ns_machine_analysis_data_loader.h \
		 ns_image_server/statistics/ns_movement_state.h
src_system_base = ns_image_server/system_base/ns_dir.cpp \
	   ns_image_server/system_base/ns_thread.h \
	   ns_image_server/system_base/ns_xml.h \
	   ns_image_server/system_base/ns_managed_pointer.h \
	   ns_image_server/system_base/ns_thread.cpp \
	   ns_image_server/system_base/ns_ex.h \
	   ns_image_server/system_base/ns_sql.cpp \
	   ns_image_server/system_base/ns_socket.h \
	   ns_image_server/system_base/ns_sql.h \
	   ns_image_server/system_base/ns_dir.h \
	   ns_image_server/system_base/ns_ex.cpp \
	   ns_image_server/system_base/ns_xml.cpp \
	   ns_image_server/system_base/ns_lock.h \
	   ns_image_server/system_base/ns_os_signal_handler.h \
	   ns_image_server/system_base/ns_socket.cpp
	   
src_external = external_libraries/libsvm/svm.cpp \
	       external_libraries/libsvm/svm.h \
	       external_libraries/ctmf/ctmf.h \
	       external_libraries/ctmf/ctmf.c \
	       external_libraries/triangle/triangle.cpp \
	       external_libraries/triangle/triangle.h \
	       external_libraries/libhungarian/hungarian.c \
	       external_libraries/libhungarian/hungarian.h \
	       external_libraries/wm4_bspline/Wm4TStringHashTable.inl \
	       external_libraries/wm4_bspline/Wm4Vector2.cpp \
	       external_libraries/wm4_bspline/Wm4TMinHeap.inl \
	       external_libraries/wm4_bspline/Wm4THashSet.h \
	       external_libraries/wm4_bspline/Wm4THashTable.inl \
	       external_libraries/wm4_bspline/Wm4Math.h \
	       external_libraries/wm4_bspline/Wm4Platforms.h \
	       external_libraries/wm4_bspline/Wm4IntpBSplineUniform.cpp \
	       external_libraries/wm4_bspline/Wm4TTuple.inl \
	       external_libraries/wm4_bspline/Wm4BSplineCurve2.cpp \
	       external_libraries/wm4_bspline/Wm4Integrate1.cpp \
	       external_libraries/wm4_bspline/Wm4Memory.h \
	       external_libraries/wm4_bspline/Wm4System.cpp \
	       external_libraries/wm4_bspline/Wm4System.h \
	       external_libraries/wm4_bspline/Wm4THashSet.inl \
	       external_libraries/wm4_bspline/Wm4Curve2.h \
	       external_libraries/wm4_bspline/Wm4SingleCurve2.h \
	       external_libraries/wm4_bspline/Wm4BSplineBasis.cpp \
	       external_libraries/wm4_bspline/Wm4BandedMatrix.inl \
	       external_libraries/wm4_bspline/Wm4TSmallUnorderedSet.inl  \
	       external_libraries/wm4_bspline/Wm4BSplineFitBasis.h \
	       external_libraries/wm4_bspline/Wm4IntpBSplineUniform2.h \
	       external_libraries/wm4_bspline/Wm4Math.inl \
	       external_libraries/wm4_bspline/Wm4Vector2.inl \
	       external_libraries/wm4_bspline/Wm4System.inl \
	       external_libraries/wm4_bspline/Wm4BSplineBasis.h \
	       external_libraries/wm4_bspline/Wm4TTuple.h \
	       external_libraries/wm4_bspline/Wm4BSplineCurveFit.h \
	       external_libraries/wm4_bspline/Wm4BandedMatrix.h \
	       external_libraries/wm4_bspline/Wm4Math.cpp \
	       external_libraries/wm4_bspline/Wm4FoundationPCH.cpp \
	       external_libraries/wm4_bspline/Wm4Memory.cpp \
	       external_libraries/wm4_bspline/Wm4FoundationPCH.h \
	       external_libraries/wm4_bspline/Wm4THashTable.h \
	       external_libraries/wm4_bspline/Wm4IntpBSplineUniform.h \
	       external_libraries/wm4_bspline/Wm4BSplineCurveFit.cpp \
	       external_libraries/wm4_bspline/Wm4BSplineCurve2.h \
	       external_libraries/wm4_bspline/Wm4FoundationLIB.h \
	       external_libraries/wm4_bspline/Wm4Memory.inl \
	       external_libraries/wm4_bspline/Wm4IntpBSplineUniform2.cpp \
	       external_libraries/wm4_bspline/Wm4SingleCurve2.cpp \
	       external_libraries/wm4_bspline/Wm4Integrate1.h \
	       external_libraries/wm4_bspline/Wm4Curve2.cpp \
	       external_libraries/wm4_bspline/Wm4Vector2.h \
	       external_libraries/wm4_bspline/Wm4TSmallUnorderedSet.h \
	       external_libraries/wm4_bspline/Wm4MathMCR.h \
	       external_libraries/wm4_bspline/Wm4TStringHashTable.h \
	       external_libraries/wm4_bspline/Wm4Command.h \
	       external_libraries/wm4_bspline/Wm4BSplineFitBasis.cpp \
	       external_libraries/wm4_bspline/Wm4Command.cpp \
	       external_libraries/wm4_bspline/Wm4TMinHeap.h \
	       external_libraries/tinyxml/tinystr.cpp \
	       external_libraries/tinyxml/tinustr.h \
	       external_libraries/tinyxml/tinyxml.cpp \
	       external_libraries/tinyxml/tinyxml.h \
	       external_libraries/tinyxml/tinyxmlerror.cpp \
	       external_libraries/tinyxml/tinyxmlparser.cpp
	       
include_dir_external = -Iexternal_libraries/libsvm \
	       -Iexternal_libraries/ctmf \
	       -Iexternal_libraries/triangle \
	       -Iexternal_libraries/libhungarian \
	       -Iexternal_libraries/wm4_bspline \
	       -Iexternal_libraries/tinyxml \
	       -Iexternal_libraries/tiff-3.8.2/libtiff
	       
DEF_FONT = @datarootdir@/default_font.ttf
INI_PATH = @datarootdir@/ns_image_server.ini

PRE_DEF = -DTRILIBRARY -DNO_TIMER -DANSI_DECLARATORS -DNS_DEFAULT_FONT="\"$(DEF_FONT)\"" -DNS_INI_PATH="\"$(INI_PATH)\""
       
	   
bin_PROGRAMS = ns_image_server

AM_CPPFLAGS = -Ins_image_server -Ins_image_server/statistics -Ins_image_server/system_base \
			   -Ins_image_server/movement_analysis -Ins_image_server/job_scheduling \
			   -Ins_image_server/image_processing -Ins_image_server/image_base \
			   -Ins_image_server/image_acquisition -Ins_image_server/data_annotation \
			   $(include_dir_external) \
			   -O3 \
			   $(MYSQL_CFLAGS) \
			   -I/usr/include/mysql -I/usr/local/include/mysql -I /usr/mysql \
			   -I/usr/include/freetype2 -I/usr/local/include/freetype2 -I/usr/freetype2 \
			   $(PRE_DEF) \
			   $(DEPS_CFLAGS)
AM_CXXFLAGS = -g	
AM_CFLAGS = -O3 

AM_LDFLAGS=$(MYSQL_LDFLAGS) $(DEPS_LIBS)

ns_image_server_SOURCES = $(src_base) $(src_data_annotation) $(src_image_acquisition) \
	   $(src_image_base) $(src_image_processing) $(src_job_scheduling) \
	   $(src_movement_analysis) $(src_statistics) $(src_system_base) \
	   $(src_external)
	   
	   

#-ljpeg -lusb -ltiff -lrt -lxvidcore -ldmtx -lmysqlclient -lfreetype -lpthread 
			   
	   