# Install script for directory: C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo

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
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Debug/itkvnl_algo-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ee][Aa][Ss][Ee])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/Release/itkvnl_algo-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Mm][Ii][Nn][Ss][Ii][Zz][Ee][Rr][Ee][Ll])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/MinSizeRel/itkvnl_algo-4.10.lib")
  elseif("${CMAKE_INSTALL_CONFIG_NAME}" MATCHES "^([Rr][Ee][Ll][Ww][Ii][Tt][Hh][Dd][Ee][Bb][Ii][Nn][Ff][Oo])$")
    file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/lib" TYPE STATIC_LIBRARY FILES "C:/server/InsightToolkit-4.10.0/bin/lib/RelWithDebInfo/itkvnl_algo-4.10.lib")
  endif()
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_algo_fwd.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_netlib.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_svd.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_svd.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_svd_economy.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_svd_economy.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_svd_fixed.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_svd_fixed.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_matrix_inverse.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_matrix_inverse.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_qr.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_qr.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_scatter_3x3.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_scatter_3x3.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_cholesky.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_ldl_cholesky.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_sparse_lu.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_real_eigensystem.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_complex_eigensystem.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_symmetric_eigensystem.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_symmetric_eigensystem.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_generalized_eigensystem.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_sparse_symmetric_eigensystem.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_generalized_schur.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_complex_generalized_schur.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_discrete_diff.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_levenberg_marquardt.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_sparse_lm.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_conjugate_gradient.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_lbfgs.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_lbfgsb.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_amoeba.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_powell.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_brent.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_lsqr.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_solve_qp.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_bracket_minimum.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_brent_minimizer.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_rpoly_roots.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_cpoly_roots.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_rnpoly_solve.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fit_parabola.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_base.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_base.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_1d.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_1d.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_2d.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_2d.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_prime_factors.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_fft_prime_factors.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_convolve.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_convolve.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_determinant.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_determinant.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_chi_squared.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_chi_squared.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_gaussian_kernel_1d.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_adjugate.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_adjugate.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_orthogonal_complement.hxx")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_orthogonal_complement.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_matrix_update.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_simpson_integral.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_adaptsimpson_integral.h")
endif()

if("${CMAKE_INSTALL_COMPONENT}" STREQUAL "Development" OR NOT CMAKE_INSTALL_COMPONENT)
  file(INSTALL DESTINATION "${CMAKE_INSTALL_PREFIX}/include/ITK-4.10/vnl/algo" TYPE FILE PERMISSIONS OWNER_WRITE OWNER_READ GROUP_READ WORLD_READ FILES "C:/server/InsightToolkit-4.10.0/bin/Modules/ThirdParty/VNL/src/vxl/core/vnl/algo/vnl_algo_export.h")
endif()

