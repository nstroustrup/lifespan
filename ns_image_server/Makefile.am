AUTOMAKE_OPTIONS = subdir-objects
src_base = image_server/ns_image_statistics.cpp \
		image_server/ns_image_storage_handler.cpp \
		image_server/ns_image_server_message.cpp \
		image_server/ns_ini.cpp \
		image_server/ns_image_server.cpp \
		image_server/ns_graph.cpp \
		image_server/ns_image_server_images.cpp 
src_data_annotation = data_annotation/ns_experiment_storyboard.cpp \
		      data_annotation/ns_mask_management.cpp \
		      data_annotation/ns_death_time_annotation.cpp \
		      data_annotation/ns_machine_learning_training_set.cpp
src_image_acquisition = image_acquisition/ns_usb.cpp \
			image_acquisition/ns_image_capture_data_manager.cpp \
			image_acquisition/ns_capture_device_manager.cpp \
			image_acquisition/ns_barcode.cpp \
			image_acquisition/ns_image_server_dispatcher.cpp \
			image_acquisition/ns_capture_device.cpp \
			image_acquisition/ns_buffered_capture_scheduler.cpp \
			image_acquisition/ns_image_server_alerts.cpp \
			image_acquisition/ns_capture_schedule.cpp
src_image_base = image_base/ns_vector_bitmap_interface.cpp \
		 image_base/ns_image_socket.cpp \
		 image_base/ns_libtiff_interface.c \
		 image_base/ns_ojp2k.cpp \
		 image_base/ns_svg.cpp \
		 image_base/ns_image.cpp \
		 image_base/ns_jpeg.cpp \
		 image_base/ns_vector.cpp \
		 image_base/ns_font.cpp \
		 image_base/ns_tiff.cpp
src_image_processing = image_processing/ns_worm_detector.cpp \
		       image_processing/ns_xvid.cpp \
		       image_processing/ns_movement_visualization_generator.cpp \
		       image_processing/ns_worm_detection_constants.cpp \
		       image_processing/ns_worm_training_set_image.cpp \
		       image_processing/ns_segment_topology.cpp \
		       image_processing/ns_detected_worm_info.cpp \
		       image_processing/ns_bspline.cpp \
		       image_processing/ns_spatial_avg.cpp \
		       image_processing/ns_heat_map_interpolation.cpp \
		       image_processing/ns_difference_thresholder.cpp \
		       image_processing/ns_image_tools.cpp \
		       image_processing/ns_node_topology.cpp \
		       image_processing/ns_identify_contiguous_bitmap_regions.cpp \
		       image_processing/ns_spine_drawer.cpp \
		       image_processing/ns_complex_segment_cluster_solver.cpp 
src_job_scheduling =   job_scheduling/ns_processing_job_scheduler.cpp \
		       job_scheduling/ns_image_processing_pipeline.cpp \
		       job_scheduling/ns_processing_job_processor.cpp \
		       job_scheduling/ns_processing_job_push_scheduler.cpp
src_movement_analysis = movement_analysis/ns_time_path_solver.cpp \
			movement_analysis/ns_hidden_markov_model_posture_analyzer.cpp \
			movement_analysis/ns_time_path_image_analyzer.cpp
src_statistics = statistics/ns_survival_curve.cpp \
		 statistics/ns_normal_distribution.cpp \
		 statistics/ns_machine_analysis_data_loader.cpp \
		 statistics/ns_captured_image_statistics_set.cpp \
		 statistics/ns_movement_measurement.cpp
src_system_base = system_base/ns_dir.cpp \
	   system_base/ns_thread.cpp \
	   system_base/ns_sql.cpp \
	   system_base/ns_ex.cpp \
	   system_base/ns_xml.cpp \
	   system_base/ns_socket.cpp
src_image_server = image_server/ns_image_server_main.cpp
src_worm_browser = ../ns_worm_browser/ns_worm_browser_main.cpp \
	   ../ns_worm_browser/ns_worm_browser.cpp
src_barcodes = ../ns_image_server_utilities/ns_image_server_barcodes_main.cpp \
               image_acquisition/ns_barcode.cpp \
               image_base/ns_font.cpp \
               image_base/ns_image.cpp \
               image_base/ns_jpeg.cpp \
               image_base/ns_libtiff_interface.c \
               image_base/ns_ojp2k.cpp \
               image_base/ns_svg.cpp \
               image_base/ns_tiff.cpp \
               image_base/ns_vector.cpp \
               system_base/ns_dir.cpp \
               system_base/ns_ex.cpp \
               system_base/ns_thread.cpp
src_external = ../external_libraries/libsvm/svm.cpp \
	       ../external_libraries/ctmf/ctmf.c \
	       ../external_libraries/triangle/triangle.cpp \
	       ../external_libraries/libhungarian/hungarian.c \
	       ../external_libraries/wm4_bspline/Wm4Vector2.cpp \
	       ../external_libraries/wm4_bspline/Wm4IntpBSplineUniform.cpp \
	       ../external_libraries/wm4_bspline/Wm4BSplineCurve2.cpp \
	       ../external_libraries/wm4_bspline/Wm4Integrate1.cpp \
	       ../external_libraries/wm4_bspline/Wm4System.cpp \
	       ../external_libraries/wm4_bspline/Wm4BSplineBasis.cpp \
	       ../external_libraries/wm4_bspline/Wm4Math.cpp \
	       ../external_libraries/wm4_bspline/Wm4FoundationPCH.cpp \
	       ../external_libraries/wm4_bspline/Wm4Memory.cpp \
	       ../external_libraries/wm4_bspline/Wm4BSplineCurveFit.cpp \
	       ../external_libraries/wm4_bspline/Wm4IntpBSplineUniform2.cpp \
	       ../external_libraries/wm4_bspline/Wm4SingleCurve2.cpp \
	       ../external_libraries/wm4_bspline/Wm4Curve2.cpp \
	       ../external_libraries/wm4_bspline/Wm4BSplineFitBasis.cpp \
	       ../external_libraries/wm4_bspline/Wm4Command.cpp \
	       ../external_libraries/tinyxml/tinystr.cpp \
	       ../external_libraries/tinyxml/tinyxml.cpp \
	       ../external_libraries/tinyxml/tinyxmlerror.cpp \
	       ../external_libraries/tinyxml/tinyxmlparser.cpp
include_dir_external = -I../external_libraries/libsvm \
	       -I../external_libraries/ctmf \
	       -I../external_libraries/triangle \
	       -I../external_libraries/libhungarian \
	       -I../external_libraries/wm4_bspline \
	       -I../external_libraries/tinyxml \
	       -I../external_libraries/libtiff-proc

DEF_FONT = $(pkgdatadir)/default_font.ttf
INI_PATH = $(sysconfdir)/ns_image_server.ini

PRE_DEF = -DTRILIBRARY -DNO_TIMER -DANSI_DECLARATORS -DNS_DEFAULT_FONT="\"$(DEF_FONT)\"" -DNS_INI_PATH="\"$(INI_PATH)\""
bin_PROGRAMS = ns_image_server_barcodes ns_image_server
#serverdir = @datarootdir@
dist_pkgdata_DATA = ../files/default_font.ttf

if BUILD_BROWSER
bin_PROGRAMS += ns_worm_browser
dist_pkgdata_DATA += ../files/occupied_image.tif ../files/start.tif
PRE_DEF += -DNS_DATA_PATH="\"$(pkgdatadir)/\""
if TARGET_OSX
all-local: ns_worm_browser.app
_ns_worm_browser: ns_worm_browser$(EXEEXT)
	cp ns_worm_browser$(EXEEXT) _ns_worm_browser
_ns_worm_browser.app: _ns_worm_browser
	$(FLTK_CONFIG) --post _ns_worm_browser
ns_worm_browser.app: _ns_worm_browser.app clear_browser_app
	mv _ns_worm_browser.app ns_worm_browser.app
	rm _ns_worm_browser
clear_browser_app:
	rm -rf ns_worm_browser.app
clean-local:  clear_browser_app
# TODO: this .app bundle can only be used on the system it's built on:
# there are resource files and non-standard libraries in /usr/local. 
# A proper fix would be to bundle these in the .app.
# TODO: Figure out how best to distribute ns_image_server for the mac?
endif
endif

AM_CPPFLAGS = -Iimage_server -Istatistics -Isystem_base \
			   -Imovement_analysis -Ijob_scheduling \
			   -Iimage_processing -Iimage_base \
			   -Iimage_acquisition -Idata_annotation \
			   $(include_dir_external) \
			   -O3 \
			   $(MYSQL_CFLAGS) \
			   -I/usr/include/mysql -I/usr/local/include/mysql -I /usr/mysql \
			   -I/usr/include/freetype2 -I/usr/local/include/freetype2 -I/usr/freetype2 \
			   $(PRE_DEF) \
			   $(DEPS_CFLAGS) 
AM_CXXFLAGS = -g $(NS_CXXFLAGS)	-D$(NS_USB_1)
AM_CFLAGS = -O3 -pthread

AM_LDFLAGS=$(MYSQL_LDFLAGS) $(DEPS_LIBS) -lpthread $(NS_LDFLAGS)

src_common = $(src_base) $(src_data_annotation) $(src_image_acquisition) \
	   $(src_image_base) $(src_image_processing) $(src_job_scheduling) \
	   $(src_movement_analysis) $(src_statistics) $(src_system_base) \
	   $(src_external)

ns_image_server_SOURCES = $(src_common) $(src_image_server)
ns_image_server_barcodes_SOURCES = $(src_barcodes)

if BUILD_BROWSER
ns_worm_browser_SOURCES = $(src_common) $(src_worm_browser)
ns_worm_browser_LDFLAGS = $(AM_LDFLAGS) $(FLTK_LDFLAGS)
# If ns_image_server_CPPFLAGS doesn't match the ns_worm_browser_CPPFLAGS,
# automake will build all the .o files in $(src_common) separately for each.
# So just set the CPPFLAGS for all.
AM_CPPFLAGS += -I$(FLTK_INCLUDEDIR) 
endif
