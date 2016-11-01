cmake_minimum_required(VERSION 2.8)
project (ns_image_server)

option (ADVANCED_MOVEMENT_QUANTIFICATION
        "Use advanced movement quantitifiation" OFF) 

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/modules/")

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

list(REMOVE_DUPLICATES ns_image_server_base_INCLUDE_DIRS)

set (ns_image_minimimal_SOURCES  "")
list (APPEND ns_image_minimimal_SOURCES
"${PROJECT_SOURCE_DIR}/../image_acquisition/ns_barcode.cpp"
"${PROJECT_SOURCE_DIR}/../image_base/ns_font.cpp" 
"${PROJECT_SOURCE_DIR}/../image_base/ns_image.cpp" 
"${PROJECT_SOURCE_DIR}/../image_base/ns_jpeg.cpp" 
"${PROJECT_SOURCE_DIR}/../image_base/ns_libtiff_interface.c" 
"${PROJECT_SOURCE_DIR}/../image_base/ns_ojp2k.cpp" 
"${PROJECT_SOURCE_DIR}/../image_base/ns_svg.cpp" 
"${PROJECT_SOURCE_DIR}/../image_base/ns_tiff.cpp" 
"${PROJECT_SOURCE_DIR}/../image_base/ns_vector.cpp" 
"${PROJECT_SOURCE_DIR}/../system_base/ns_dir.cpp" 
"${PROJECT_SOURCE_DIR}/../system_base/ns_ex.cpp" 
"${PROJECT_SOURCE_DIR}/../system_base/ns_thread.cpp" )

set (ns_external_sources  "")
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

set(DEF_FONT "${pkgdatadir}/default_font.ttf")
set(INI_PATH "${sysconfdir}/ns_image_server.ini")

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11")

add_definitions(-DTRILIBRARY -DNO_TIMER -DANSI_DECLARATORS -DNS_DEFAULT_FONT="${DEF_FONT}" -DNS_INI_PATH="${INI_PATH}" -DUSE_NEW_USB=1)
   
add_library(ns_image_server_base STATIC ${ns_image_server_base_SOURCES} ${ns_external_SOURCES})
add_executable (ns_image_server "${PROJECT_SOURCE_DIR}/../ns_image_server/image_server/ns_image_server_main.cpp")
add_dependencies(ns_image_server ns_image_server_base)
target_link_libraries(ns_image_server ns_image_server_base)

SET(IPP_STATIC 1)
SET(IPP_MULTI_THREADED 0)
find_package(IPP REQUIRED)
include_directories(${IPP_INCLUDE_DIR})
target_link_libraries(ns_image_server ${IPP_LIBRARIES})

find_package(MYSQL REQUIRED)
include_directories(${MYSQL_INCLUDE_DIR})
target_link_libraries(ns_image_server ${MYSQL_LIBRARIES})

find_package(JPEG REQUIRED)
include_directories(${JPEG_INCLUDE_DIR})
target_link_libraries(ns_image_server ${JPEG_LIBRARIES})

target_link_libraries(ns_image_server m)
find_package(DMTX REQUIRED)
target_link_libraries(ns_image_server dmtx)
find_package(TIFF REQUIRED)
target_link_libraries(ns_image_server tiff)
find_package(Freetype REQUIRED)
include_directories(${FREETYPE_INCLUDE_DIRS})
target_link_libraries(ns_image_server freetype)
target_link_libraries(ns_image_server pthread)

find_package(OpenJPEG REQUIRED)
include_directories(ns_image_server ${OPENJPEG_DIRECTORIES})
target_link_libraries(ns_image_server ${OPENJPEG_LIBRARIES})
find_package(xvid REQUIRED)
include_directories(${XVID_INCLUDE_DIRS})
target_link_libraries(ns_image_server ${XVID_LIBRARIES})

find_package(ZLIB REQUIRED)
target_link_libraries(ns_image_server z)
find_package(libusb-1.0 REQUIRED)
include_directories(ns_image_server ${LIBUSB_1_INCLUDE_DIRS})
target_link_libraries(ns_image_server ${LIBUSB_1_LIBRARIES})

include_directories("${PROJECT_SOURCE_DIR}/../external_libraries/libsvm"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/ctmf"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/triangle"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/libhungarian"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/wm4_bspline"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/tinyxml"
	       "${PROJECT_SOURCE_DIR}/../external_libraries/libtiff-proc")
	       
	       

add_executable (ns_worm_browser "${PROJECT_SOURCE_DIR}/../ns_worm_browser/ns_worm_browser_main.cpp" "${PROJECT_SOURCE_DIR}/../ns_worm_browser/ns_worm_browser.cpp")
add_dependencies(ns_worm_browser ns_image_server_base)

include_directories(${ns_image_server_base_INCLUDE_DIRS})
include_directories("${PROJECT_SOURCE_DIR}/../ns_worm_browser")
