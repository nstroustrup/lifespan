# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common

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
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkgdcmCommon-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkgdcmCommon-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkgdcmCommon-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkgdcmCommon-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmASN1.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmBase64.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmBoxRegion.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmByteSwap.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCAPICryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCAPICryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCommand.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDataEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDeflateStream.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDirectory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDummyValueGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmException.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmFileNameEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmFilename.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmFilenameGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmLegacyMacro.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmMD5.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmObject.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLCryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLCryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLP7CryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLP7CryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmProgressEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmRegion.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSHA1.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSmartPointer.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmStaticAssert.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmString.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSubject.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSwapCode.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSwapper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSystem.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTerminal.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTestDriver.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTesting.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTrace.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTypes.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmUnpacker12Bits.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmVersion.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmWin32.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/zipstreamimpl.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmByteSwap.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSwapper.txx"
    )
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmASN1.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmBase64.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmBoxRegion.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmByteSwap.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCAPICryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCAPICryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCommand.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmCryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDataEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDeflateStream.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDirectory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmDummyValueGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmException.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmFileNameEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmFilename.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmFilenameGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmLegacyMacro.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmMD5.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmObject.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLCryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLCryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLP7CryptoFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmOpenSSLP7CryptographicMessageSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmProgressEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmRegion.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSHA1.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSmartPointer.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmStaticAssert.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmString.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSubject.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSwapCode.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSwapper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSystem.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTerminal.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTestDriver.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTesting.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTrace.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmTypes.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmUnpacker12Bits.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmVersion.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmWin32.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/zipstreamimpl.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmByteSwap.txx"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmSwapper.txx"
    "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/GDCM/src/gdcm/Source/Common/gdcmConfigure.h"
    )
endif()

