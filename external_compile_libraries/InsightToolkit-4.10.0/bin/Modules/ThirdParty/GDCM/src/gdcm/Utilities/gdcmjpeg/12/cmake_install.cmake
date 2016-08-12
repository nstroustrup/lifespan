# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12

# Set the install prefix
if(NOT DEFINED CMAKE_INSTALL_PREFIX)
  set(CMAKE_INSTALL_PREFIX "C:/Program Files/ITK")
endif()
string(REGEX REPLACE "/$" "" CMAKE_INSTALL_PREFIX "${CMAKE_INSTALL_PREFIX}")

# Set the install configuration name.
if(NOT DEFINED CMAKE_INSTALL_CONFIG_NAME)
  if(BUILD_TYPE)
    string(REGEX REPLACE "^[^A-Za-z0-9_]+" ""
           CMAKE_INSTALL_CONFIG_NAME "${BUILD_TYPE}")
  else()
    set(CMAKE_INSTALL_CONFIG_NAME "Release")
  endif()
  message(STATUS "Install configuration: \"${CMAKE_INSTALL_CONFIG_NAME}\"")
endif()

# Set the component getting installed.
if(NOT CMAKE_INSTALL_COMPONENT)
  if(COMPONENT)
    message(STATUS "Install component: \"${COMPONENT}\"")
    set(CMAKE_INSTALL_COMPONENT "${COMPONENT}")
  else()
    set(CMAKE_INSTALL_COMPONENT)
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "DebugDevel" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkgdcmjpeg12-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkgdcmjpeg12-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkgdcmjpeg12-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkgdcmjpeg12-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "DebugDevel" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/gdcmjpeg/12" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12/jconfig.h"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12/jerror.h"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12/jinclude.h"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12/jmorecfg.h"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12/jpegcmake.h"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12/jpeglib.h"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Utilities/gdcmjpeg/12/mangle_jpeg12bits.h"
    )
endif()

