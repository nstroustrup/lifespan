# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition

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
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkgdcmMEXD-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkgdcmMEXD-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkgdcmMEXD-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkgdcmMEXD-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Headers" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10" TYPE FILE FILES
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAAbortPDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAAssociateACPDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAAssociateRJPDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAAssociateRQPDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmARTIMTimer.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAReleaseRPPDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAReleaseRQPDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAbstractSyntax.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmApplicationContext.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmAsynchronousOperationsWindowSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmBaseCompositeMessage.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmBaseNormalizedMessage.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmBasePDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmBaseQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmBaseRootQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmCEchoMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmCFindMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmCMoveMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmCStoreMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmCommandDataSet.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmCompositeMessageFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmCompositeNetworkFunctions.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmDIMSE.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmFindPatientRootQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmFindStudyRootQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmImplementationClassUIDSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmImplementationUIDSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmImplementationVersionNameSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmMaximumLengthSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmModalityPerformedProcedureStepCreateQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmModalityPerformedProcedureStepSetQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmMovePatientRootQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmMoveStudyRootQuery.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNActionMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNCreateMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNDeleteMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNEventReportMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNGetMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNSetMessages.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNetworkEvents.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNetworkStateID.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNormalizedMessageFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmNormalizedNetworkFunctions.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmPDUFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmPDataTFPDU.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmPresentationContext.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmPresentationContextAC.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmPresentationContextGenerator.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmPresentationContextRQ.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmPresentationDataValue.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmQueryBase.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmQueryFactory.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmQueryImage.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmQueryPatient.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmQuerySeries.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmQueryStudy.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmRoleSelectionSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmSOPClassExtendedNegociationSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmServiceClassApplicationInformation.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmServiceClassUser.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmTransferSyntaxSub.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULAction.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULActionAA.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULActionAE.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULActionAR.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULActionDT.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULBasicCallback.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULConnection.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULConnectionCallback.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULConnectionInfo.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULConnectionManager.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULEvent.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULTransitionTable.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmULWritingCallback.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmUserInformation.h"
    "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/GDCM/src/gdcm/Source/MessageExchangeDefinition/gdcmWLMFindQuery.h"
    )
endif()

