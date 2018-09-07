cmake_minimum_required(VERSION 2.8)
project (ns_jcon_test)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${PROJECT_SOURCE_DIR}/modules/")

set(CMAKE_BUILD_TYPE Release)

file(GLOB_RECURSE ns_jcon_SOURCES "${PROJECT_SOURCE_DIR}/ns_wcon_test.cpp")
file(GLOB_RECURSE ns_jcon_HEADERS "${PROJECT_SOURCE_DIR}/*.h")


set (ns_jcon_INCLUDE_DIRS "")
foreach (_headerFile ${ns_jcon_HEADERS})
    get_filename_component(_dir ${_headerFile} PATH)
    list (APPEND ns_jcon_INCLUDE_DIRS ${_dir})
endforeach()

find_package(nlohmann_json REQUIRED)
LIST(APPEND NS_INC ${NLOHMANN_JSON_INCLUDE_DIRS})
LIST(APPEND NS_LN ${NLOHMANN_JSON_LIBRARIES})

  if (CMAKE_VERSION VERSION_LESS "3.1")
      set (CMAKE_CXX_FLAGS "--std=c++11 ${CMAKE_CXX_FLAGS}")
  else ()
    set (CMAKE_CXX_STANDARD 11)
  endif ()

#add_definitions();

   
#build base library used for image server and worm browser

#build the image server
add_executable (ns_wcon_test_e "${PROJECT_SOURCE_DIR}/ns_wcon_test.cpp")
target_include_directories(ns_wcon_test_e PRIVATE ${NS_INC})
target_link_libraries(ns_wcon_test_e ${NS_LN})
