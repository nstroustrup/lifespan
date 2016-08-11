# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src

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

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/ITKMetaIO-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/ITKMetaIO-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/ITKMetaIO-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/ITKMetaIO-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/localMetaConfiguration.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaArray.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaArrow.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaBlob.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaCommand.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaContour.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaDTITube.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaEllipse.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaFEMObject.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaForm.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaGaussian.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaGroup.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaITKUtils.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaImage.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaImageTypes.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaImageUtils.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaLandmark.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaLine.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaMesh.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaObject.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaOutput.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaScene.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaSurface.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaTransform.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaTube.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaTubeGraph.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaTypes.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaUtils.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaVesselTube.h"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/MetaIO/src/MetaIO/src/metaIOConfig.h"
    )
endif()

