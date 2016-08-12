# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat

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
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkgdcmMSFF-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkgdcmMSFF-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkgdcmMSFF-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkgdcmMSFF-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmAnonymizeEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmAnonymizer.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmApplicationEntity.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmAudioCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmBitmap.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmBitmapToBitmapFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmCoder.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmConstCharWrapper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmCurve.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDICOMDIR.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDICOMDIRGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDataSetHelper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDecoder.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDeltaEncodingCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDictPrinter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDirectionCosines.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDirectoryHelper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmDumper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmEncapsulatedDocument.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmFiducials.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmFileAnonymizer.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmFileChangeTransferSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmFileDecompressLookupTable.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmFileDerivation.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmFileExplicitFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmFileStreamer.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmIPPSorter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmIconImage.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmIconImageFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmIconImageGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImage.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageApplyLookupTable.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageChangePhotometricInterpretation.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageChangePlanarConfiguration.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageChangeTransferSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageConverter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageFragmentSplitter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageHelper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageReader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageRegionReader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageToImageFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmImageWriter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmJPEG12Codec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmJPEG16Codec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmJPEG2000Codec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmJPEG8Codec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmJPEGCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmJPEGLSCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmJSON.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmKAKADUCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmLookupTable.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmMeshPrimitive.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmOrientation.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmOverlay.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPDFCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPGXCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPNMCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPVRGCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPersonName.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPhotometricInterpretation.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPixelFormat.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPixmap.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPixmapReader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPixmapToPixmapFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPixmapWriter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmPrinter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmRAWCodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmRLECodec.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmRescaler.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmScanner.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSegment.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSegmentHelper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSegmentReader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSegmentWriter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSegmentedPaletteColorLookupTable.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSerieHelper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSimpleSubjectWatcher.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSorter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSpacing.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSpectroscopy.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSplitMosaicFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmStreamImageReader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmStreamImageWriter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmStrictScanner.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmStringFilter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSurface.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSurfaceHelper.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSurfaceReader.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmSurfaceWriter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmTagPath.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmUIDGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmUUIDGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmValidate.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmWaveform.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcmXMLPrinter.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcm_j2k.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MediaStorageAndFileFormat/gdcm_jp2.h"
    )
endif()

