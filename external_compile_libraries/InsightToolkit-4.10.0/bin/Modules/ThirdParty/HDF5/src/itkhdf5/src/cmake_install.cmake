# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src

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

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itkhdf5" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/hdf5.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5api_adpt.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5public.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5version.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5overflow.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Apkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Apublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5ACpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5ACpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5B2pkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5B2public.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Bpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Bpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Dpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Dpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Edefin.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Einit.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Epkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Epubgen.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Epublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Eterm.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Fpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Fpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDcore.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDdirect.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDfamily.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDlog.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDmpi.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDmpio.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDmpiposix.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDmulti.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDsec2.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDstdio.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FSpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FSpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Gpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Gpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HFpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HFpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HGpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HGpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HLpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HLpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5MPpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Opkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Opublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Oshared.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Ppkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Ppublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Spkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Spublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5SMpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Tpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Tpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Zpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Zpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Cpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Cpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Ipkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Ipublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Lpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Lpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5MMpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Rpkg.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Rpublic.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDwindows.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5private.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Aprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5ACprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5B2private.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Bprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5CSprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Dprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Eprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FDprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Fprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FLprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FOprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5MFprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5MMprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Cprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5FSprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Gprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HFprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HGprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HLprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5HPprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Iprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Lprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5MPprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Oprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Pprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5RCprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Rprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5RSprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5SLprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5SMprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Sprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5STprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Tprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5TSprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Vprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5WBprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5Zprivate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/HDF5/src/itkhdf5/src/H5win32defs.h"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "libraries" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkhdf5-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkhdf5-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkhdf5-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkhdf5-4.10.lib")
  endif()
endif()

