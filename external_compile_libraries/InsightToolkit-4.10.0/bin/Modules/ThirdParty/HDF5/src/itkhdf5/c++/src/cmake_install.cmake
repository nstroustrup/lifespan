# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src

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

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "cppheaders" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itkhdf5/cpp" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5AbstractDs.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Alltypes.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5ArrayType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5AtomType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Attribute.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Classes.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5CommonFG.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5CompType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Cpp.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5CppDoc.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5DataSet.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5DataSpace.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5DataType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5DcreatProp.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5DxferProp.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5EnumType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Exception.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5FaccProp.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5FcreatProp.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5File.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5FloatType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Group.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5IdComponent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Include.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5IntType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Library.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5Object.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5PredType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5PropList.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5StrType.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/c++/src/H5VarLenType.h"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "libraries" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkhdf5_cpp-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkhdf5_cpp-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkhdf5_cpp-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkhdf5_cpp-4.10.lib")
  endif()
endif()

