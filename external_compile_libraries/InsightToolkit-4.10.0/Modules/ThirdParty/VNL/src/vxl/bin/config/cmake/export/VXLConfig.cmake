# vxl/config/cmake/VXLConfig_export.cmake.in
#   also configured by CMake to
# C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/bin/config/cmake/export/VXLConfig.cmake
#
# This CMake module is configured by VXL's build process to export the
# project settings for use by client projects.  A client project may
# find VXL and include this module using the FIND_PACKAGE command:
#
#  find_package(VXL)
#
# After this command executes, projects may test VXL_FOUND for whether
# VXL has been found.  If so, the settings listed below in this file
# have been loaded and are available for use.
#
# Typically, a client project will include UseVXL.cmake from the
# directory specified by the VXL_CMAKE_DIR setting:
#
#  find_package(VXL)
#  if(VXL_FOUND)
#    include(${VXL_CMAKE_DIR}/UseVXL.cmake)
#  else()
#    message("VXL_DIR should be set to the VXL build directory.")
#  endif()
#
# See vxl/config/cmake/UseVXL.cmake for details.
#

# The build settings file.
set(VXL_BUILD_SETTINGS_FILE "")

# The VXL library directory.
set(VXL_LIBRARY_DIR "C:/Program Files/vxl/lib")

# The VXL CMake support directory.
# Clients projects should not use the Find*.cmake files in this directory.
set(VXL_CMAKE_DIR "C:/Program Files/vxl/share/vxl/cmake")

# VXL Configuration options. You don't have to build with the same options as VXL, but it often helps.
set(VXL_BUILD_SHARED_LIBS "OFF")
set(VXL_BUILD_TESTS "")
set(VXL_BUILD_EXAMPLES "ON")
set(VXL_EXTRA_CMAKE_CXX_FLAGS "")
set(VXL_EXTRA_CMAKE_C_FLAGS "")
set(VXL_EXTRA_CMAKE_EXE_LINKER_FLAGS "")
set(VXL_EXTRA_CMAKE_MODULE_LINKER_FLAGS "")
set(VXL_EXTRA_CMAKE_SHARED_LINKER_FLAGS "")

# VXL has many parts that are optional, depending on selections made
# when building.  The stanzas below give a consistent (though
# pedantic) interface to each part.  Clients use these settings to
# determine whether a part was built (_FOUND), where its headers are
# located (_INCLUDE_DIR) and in some cases what libraries must be
# linked to use the part (_LIBRARIES).  Most client projects will
# likely still refer to individual VXL libraries such as vcl, for
# example, by hard-wired "vcl" instead of using the variable
# VXL_VCL_LIBRARIES, but it is there just in case.

set(VXL_VCL_FOUND "YES" ) # VXL vcl is always FOUND.  It is not optional.
set(VXL_VCL_INCLUDE_DIR "C:/Program Files/vxl/include/vxl/vcl")
set(VXL_VCL_LIBRARIES "vcl")

set(VXL_CORE_FOUND "YES" ) # VXL core is always FOUND.  It is not optional.
set(VXL_CORE_INCLUDE_DIR "C:/Program Files/vxl/include/vxl/core")
# VXL core has many libraries

set(VXL_CORE_VIDEO_FOUND "ON" )
set(VXL_CORE_VIDEO_INCLUDE_DIR "C:/Program Files/vxl/include/vxl/core")
set(VXL_CORE_VIDEO_LIBRARIES "vidl")
set(VXL_CORE_VIDEO_FFMPEG_FOUND "" )

set(VXL_VGUI_FOUND "")
set(VXL_VGUI_INCLUDE_DIR "")
set(VXL_VGUI_LIBRARIES "vgui")

set(VXL_VGUI_WX_FOUND "")

set(VXL_CONTRIB_FOUND "")
# VXL contrib has subdirectories handled independently below
# VXL contrib has many libraries

set(VXL_BRL_FOUND "")
set(VXL_BRL_INCLUDE_DIR "")
# VXL BRL has many libraries

set(VXL_BGUI3D_FOUND "")
set(VXL_BGUI3D_INCLUDE_DIR "")

set(VXL_COIN3D_FOUND "")
set(VXL_COIN3D_INCLUDE_DIR "")

set(VXL_GEL_FOUND "")
set(VXL_GEL_INCLUDE_DIR "")
# VXL GEL has many libraries

set(VXL_MUL_FOUND "")
set(VXL_MUL_INCLUDE_DIR "")
# VXL MUL has many libraries

set(VXL_OUL_FOUND "")
set(VXL_OUL_INCLUDE_DIR "")
# VXL OUL has many libraries

set(VXL_OXL_FOUND "")
set(VXL_OXL_INCLUDE_DIR "")
# VXL OXL has many libraries

set(VXL_PRIP_FOUND "")
set(VXL_PRIP_INCLUDE_DIR "")
# VXL PRIP has many libraries

set(VXL_RPL_FOUND "")
set(VXL_RPL_RGTL_FOUND "")
set(VXL_RPL_INCLUDE_DIR "")
# VXL RPL has many libraries

set(VXL_TBL_FOUND "")
set(VXL_TBL_INCLUDE_DIR "")
# VXL TBL has many libraries

set(VXL_CONVERSIONS_FOUND "")
set(VXL_CONVERSIONS_INCLUDE_DIR "")
# VXL CONVERSIONS has no libraries

set(VXL_TARGETJR_FOUND "")

# Client projects use these setting to find and use the 3rd party
# libraries that VXL either found on the system or built for itself.
# Sometimes, VXL will point client projects to the library VXL built
# for itself, and sometimes VXL will point client projects to the
# system library it found.

set(VXL_NETLIB_FOUND "YES")
set(VXL_NETLIB_INCLUDE_DIR "C:/Program Files/vxl/include/vxl/v3p/netlib")
set(VXL_NETLIB_LIBRARIES "netlib;v3p_netlib")

set(VXL_ZLIB_FOUND "FALSE")
set(VXL_ZLIB_INCLUDE_DIR "")
set(VXL_ZLIB_LIBRARIES "ZLIB_LIBRARY-NOTFOUND")

set(VXL_PNG_FOUND "FALSE")
set(VXL_PNG_INCLUDE_DIR "")
set(VXL_PNG_LIBRARIES "")

set(VXL_JPEG_FOUND "FALSE")
set(VXL_JPEG_INCLUDE_DIR "")
set(VXL_JPEG_LIBRARIES "")

set(VXL_TIFF_FOUND "FALSE")
set(VXL_TIFF_INCLUDE_DIR "TIFF_INCLUDE_DIR-NOTFOUND")
set(VXL_TIFF_LIBRARIES "TIFF_LIBRARY-NOTFOUND")

set(VXL_GEOTIFF_FOUND "NO")
set(VXL_GEOTIFF_INCLUDE_DIR "GEOTIFF_INCLUDE_DIR-NOTFOUND")
set(VXL_GEOTIFF_LIBRARIES "")

set(VXL_MPEG2_FOUND "")
set(VXL_MPEG2_INCLUDE_DIR "")
set(VXL_MPEG2_LIBRARIES "")

set(VXL_COIN3D_FOUND "")
set(VXL_COIN3D_INCLUDE_DIR "")
set(VXL_COIN3D_LIBRARY "")

set(VXL_PYTHON_FOUND "")
set(VXL_PYTHON_INCLUDE_PATH "")
set(VXL_PYTHON_PC_INCLUDE_PATH "")
set(VXL_PYTHON_LIBRARY "")
set(VXL_PYTHON_DEBUG_LIBRARY "")

set(VXL_EXPAT_FOUND "")
set(VXL_EXPAT_INCLUDE_DIR "")
set(VXL_EXPAT_LIBRARIES "")

# Tell UseVXL.cmake that VXLConfig.cmake has been included.
set(VXL_CONFIG_CMAKE 1)

# Import VXL targets.
if(NOT VXL_TARGETS_IMPORTED)
  set(VXL_TARGETS_IMPORTED 1)
  include("${CMAKE_CURRENT_LIST_DIR}/VXLTargets.cmake")
endif()
