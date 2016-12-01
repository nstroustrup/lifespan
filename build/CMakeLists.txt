cmake_minimum_required(VERSION 2.8)
project (ns_image_server)

option (ADVANCED_MOVEMENT_QUANTIFICATION
        "Use advanced movement quantitifiation" OFF) 
option (BUILD_WORM_BROWSER
        "Compile the worm browser" OFF)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/modules/")

set(CMAKE_BUILD_TYPE Release)

file(GLOB_RECURSE image_server_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_server//*.cpp")
file(GLOB_RECURSE image_server_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/image_server//*.h")
list(REMOVE_ITEM image_server_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_server//ns_image_server_main.cpp")
list(REMOVE_ITEM image_server_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_server//stdafx.cpp")

file(GLOB_RECURSE data_annotation_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/data_annotation/*.cpp")
file(GLOB_RECURSE data_annotation_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/data_annotation/*.h")

file(GLOB_RECURSE image_acquisition_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_acquisition/*.cpp")
file(GLOB_RECURSE image_acquisition_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/image_acquisition/*.h")

file(GLOB_RECURSE image_base_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_base/*.cpp")
file(GLOB_RECURSE image_base_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/image_base/*.h")
list(APPEND image_base_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_base/ns_libtiff_interface.c")


file(GLOB_RECURSE image_processing_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_processing/*.cpp")
file(GLOB_RECURSE image_processing_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/image_processing/*.h")

if(ADVANCED_MOVEMENT_QUANTIFICATION)
message(ERROR "Advanced movement quantification not currently supported under linux")
else()
 list(REMOVE_ITEM image_processing_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/image_processing/ns_optical_flow.cpp")
endif()

file(GLOB_RECURSE job_scheduling_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/job_scheduling/*.cpp")
file(GLOB_RECURSE job_scheduling_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/job_scheduling/*.h")

file(GLOB_RECURSE movement_analysis_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/movement_analysis/*.cpp")
file(GLOB_RECURSE movement_analysis_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/movement_analysis/*.h")

file(GLOB_RECURSE statistics_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/statistics/*.cpp")
file(GLOB_RECURSE statistics_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/statistics/*.h")

file(GLOB_RECURSE system_base_SOURCES "${PROJECT_SOURCE_DIR}/../ns_image_server/system_base/*.cpp")
file(GLOB_RECURSE system_base_HEADERS "${PROJECT_SOURCE_DIR}/../ns_image_server/system_base/*.h")

set (ns_image_server_base_HEADERS "")
list(APPEND ns_image_server_base_HEADERS ${image_server_HEADERS} ${image_acquisition_HEADERS} ${image_base_HEADERS} ${image_processing_HEADERS} ${job_scheduling_HEADERS} ${movement_analysis_HEADERS} ${statistics_HEADERS}  ${system_base_HEADERS} ${data_annotation_HEADERS})

set (ns_image_server_base_SOURCES "")
list(APPEND ns_image_server_base_SOURCES ${image_server_SOURCES} ${image_acquisition_SOURCES} ${image_base_SOURCES} ${image_processing_SOURCES} ${job_scheduling_SOURCES} ${movement_analysis_SOURCES} ${statistics_SOURCES}  ${system_base_SOURCES} ${data_annotation_SOURCES})

set (ns_image_server_base_INCLUDE_DIRS "")
foreach (_headerFile ${ns_image_server_base_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list (APPEND ns_image_server_base_INCLUDE_DIRS ${_dir})
endforeach()

set (ns_external_SOURCES  "")
list (APPEND ns_external_SOURCES "${PROJECT_SOURCE_DIR}/../external_libraries/libsvm/svm.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/ctmf/ctmf.c" 
"${PROJECT_SOURCE_DIR}/../external_libraries/triangle/triangle.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/libhungarian/hungarian.c" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4Vector2.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4IntpBSplineUniform.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4BSplineCurve2.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4Integrate1.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4System.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4BSplineBasis.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4Math.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4FoundationPCH.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4Memory.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4BSplineCurveFit.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4IntpBSplineUniform2.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4SingleCurve2.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4Curve2.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4BSplineFitBasis.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline/Wm4Command.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/tinyxml/tinystr.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/tinyxml/tinyxml.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/tinyxml/tinyxmlerror.cpp" 
"${PROJECT_SOURCE_DIR}/../external_libraries/tinyxml/tinyxmlparser.cpp")

set(NS_INC  "")
set(NS_LN "")

list(APPEND NS_INC ${ns_image_server_base_INCLUDE_DIRS})

SET(IPP_STATIC 0)
find_package(IPP REQUIRED)
LIST(APPEND NS_INC ${IPP_INCLUDE_DIR})
LIST(APPEND NS_LN ${IPP_LIBRARIES})

find_package(MYSQL REQUIRED)
LIST(APPEND NS_INC ${MYSQL_INCLUDE_DIR})
LIST(APPEND NS_LN ${MYSQL_LIBRARIES})

find_package(JPEG REQUIRED)
list(APPEND NS_INC ${JPEG_INCLUDE_DIR})
LIST(APPEND NS_LN  ${JPEG_LIBRARIES})

LIST(APPEND NS_LN "m")
find_package(DMTX REQUIRED)
LIST(APPEND NS_LN "dmtx")
find_package(TIFF REQUIRED)
LIST(APPEND NS_LN "tiff")
find_package(Freetype REQUIRED)
LIST(APPEND NS_INC ${FREETYPE_INCLUDE_DIRS})
LIST(APPEND NS_LN "freetype")
LIST(APPEND NS_LN "pthread")

find_package(OpenJPEG REQUIRED)
LIST(APPEND NS_INC ${OPENJPEG_DIRECTORIES})
LIST(APPEND NS_LN ${OPENJPEG_LIBRARIES})
find_package(xvid REQUIRED)
LIST(APPEND NS_INC ${XVID_INCLUDE_DIRS})
LIST(APPEND NS_LN ${XVID_LIBRARIES})

find_package(ZLIB REQUIRED)
LIST(APPEND NS_LN "z")
find_package(libusb-1.0 REQUIRED)
LIST(APPEND NS_INC ${LIBUSB_1_INCLUDE_DIRS})
LIST(APPEND NS_LN ${LIBUSB_1_LIBRARIES})

LIST(APPEND NS_INC "${PROJECT_SOURCE_DIR}/../external_libraries/libsvm"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/ctmf"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/triangle"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/libhungarian"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/tinyxml"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/libtiff-proc")
LIST(REMOVE_DUPLICATES NS_INC)
LIST(REMOVE_DUPLICATES NS_LN)
include(GNUInstallDirs)

set(FONT_FILE "${CMAKE_INSTALL_FULL_DATADIR}/default_font.ttf")
set(INI_FILE "${CMAKE_INSTALL_FULL_SYSCONFDIR}/ns_image_server.ini")

  if (CMAKE_VERSION VERSION_LESS "3.1")
      set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()

add_definitions(-DTRILIBRARY -DNO_TIMER -DANSI_DECLARATORS -DNS_DEFAULT_FONT="${FONT_FILE}" -DNS_INI_PATH="${INI_FILE}" -DUSE_NEW_USB=1 -DNS_USE_INTEL_IPP -DNS_DATA_PATH="${CMAKE_INSTALL_FULL_DATADIR}/")

set(CMAKE_INSTALL_RPATH ${NS_LN})
set(CMAKE_INSTALL_RPATH_USE_LINK_PATH TRUE)
   
#build base library used for image server and worm browser
add_library(ns_image_server_base STATIC ${ns_image_server_base_SOURCES} ${ns_external_SOURCES})
target_include_directories(ns_image_server_base PRIVATE ${NS_INC})

#build the image server
add_executable (ns_image_server "${PROJECT_SOURCE_DIR}/../ns_image_server/image_server/ns_image_server_main.cpp")
target_include_directories(ns_image_server PRIVATE ${NS_INC})
target_link_libraries(ns_image_server ns_image_server_base)
target_link_libraries(ns_image_server ${NS_LN})


#the barcode generator
add_executable(ns_image_server_barcodes ../ns_image_server_utilities/ns_image_server_barcodes_main.cpp)
target_include_directories(ns_image_server_barcodes PRIVATE ${NS_INC})
target_link_libraries(ns_image_server_barcodes ns_image_server_base)
target_link_libraries(ns_image_server_barcodes ${NS_LN})


#the worm browser

if (BUILD_WORM_BROWSER)
 FIND_PACKAGE(FLTK REQUIRED)
 FIND_PACKAGE(OpenGL REQUIRED)

add_executable (ns_worm_browser "${PROJECT_SOURCE_DIR}/../ns_worm_browser/ns_worm_browser_main.cpp" "${PROJECT_SOURCE_DIR}/../ns_worm_browser/ns_worm_browser.cpp")
target_include_directories(ns_worm_browser PRIVATE ${NS_INC})
target_include_directories(ns_worm_browser PRIVATE "${PROJECT_SOURCE_DIR}/../ns_worm_browser")
target_link_libraries(ns_worm_browser ${FLTK_LIBRARIES})
target_link_libraries(ns_worm_browser ${OPENGL_LIBRARIES})
target_link_libraries(ns_worm_browser ns_image_server_base)
target_link_libraries(ns_worm_browser ${NS_LN})
INSTALL(TARGETS ns_worm_browser RUNTIME DESTINATION bin)
endif()

INSTALL(TARGETS ns_image_server RUNTIME DESTINATION bin)
INSTALL(TARGETS ns_image_server_barcodes RUNTIME DESTINATION bin)
INSTALL(FILES "../files/default_font.ttf" DESTINATION ${CMAKE_INSTALL_FULL_DATADIR})
INSTALL(FILES "../files/occupied_image.tif" DESTINATION ${CMAKE_INSTALL_FULL_DATADIR})

INSTALL(FILES "../files/start.tif" DESTINATION ${CMAKE_INSTALL_FULL_DATADIR})
