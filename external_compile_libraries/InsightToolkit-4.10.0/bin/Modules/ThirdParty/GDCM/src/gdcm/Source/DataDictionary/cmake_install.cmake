# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary

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
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkgdcmDICT-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkgdcmDICT-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkgdcmDICT-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkgdcmDICT-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmCSAHeaderDict.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmCSAHeaderDictEntry.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmDict.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmDictConverter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmDictEntry.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmDicts.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmGlobal.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmGroupDict.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmSOPClassUIDToIOD.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmTagKeywords.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmTagToType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/gdcmUIDs.h"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Libraries" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/ITK-4.10/gdcm/XML" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/CSAHeader.xml"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/Part6.xml"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/Part7.xml"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/UIDs.xml"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataDictionary/cp699.xml"
    )
endif()

