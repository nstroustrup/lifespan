# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/KWSys/src/KWSys

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

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "RuntimeLibraries" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/share/doc/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/KWSys/src/KWSys/Copyright.txt")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Directory.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/DynamicLoader.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Encoding.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Glob.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/RegularExpression.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/SystemTools.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/CommandLineArguments.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/FStream.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/SystemInformation.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Configure.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/SharedForward.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Process.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Base64.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Encoding.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/MD5.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/System.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/Configure.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/String.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/hashtable.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/hash_fun.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/hash_map.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/hash_set.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/itksys" TYPE FILE FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/KWSys/src/itksys/auto_ptr.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  if("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Dd][Ee][Bb][Uu][Gg])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itksys-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itksys-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itksys-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itksys-4.10.lib")
  endif()
endif()

