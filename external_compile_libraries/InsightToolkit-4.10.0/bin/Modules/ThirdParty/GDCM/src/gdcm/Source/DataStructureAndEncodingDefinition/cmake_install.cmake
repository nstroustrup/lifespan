# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition

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
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkgdcmDSED-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkgdcmDSED-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkgdcmDSED-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkgdcmDSED-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmAttribute.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmBasicOffsetTable.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmByteBuffer.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmByteSwapFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmByteValue.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmCP246ExplicitDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmCSAElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmCSAHeader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmCodeString.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmDataSet.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmDataSetEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmExplicitDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmExplicitImplicitDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmFile.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmFileMetaInformation.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmFileSet.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmFragment.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmImplicitDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmItem.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmLO.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmMediaStorage.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmPDBElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmPDBHeader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmParseException.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmParser.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmPreamble.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmPrivateTag.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmReader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmSequenceOfFragments.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmSequenceOfItems.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmTag.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmTagToVR.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmTransferSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmUNExplicitDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmUNExplicitImplicitDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmVL.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmVM.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmVR.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmVR16ExplicitDataElement.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmValue.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmValueIO.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmWriter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmCP246ExplicitDataElement.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmCSAHeader.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmDataSet.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmExplicitDataElement.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmExplicitImplicitDataElement.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmFragment.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmImplicitDataElement.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmItem.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmSequenceOfFragments.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmSequenceOfItems.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmUNExplicitDataElement.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmUNExplicitImplicitDataElement.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmVR16ExplicitDataElement.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmValue.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/DataStructureAndEncodingDefinition/gdcmValueIO.txx"
    )
endif()

