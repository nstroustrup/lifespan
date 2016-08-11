/*============================================================================
  KWSys - Kitware System Library
  Copyright 2000-2009 Kitware, Inc., Insight Software Consortium

  Distributed under the OSI-approved BSD License (the "License");
  see accompanying file Copyright.txt for details.

  This software is distributed WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
  See the License for more information.
============================================================================*/
#ifndef itksys_Configure_hxx
#define itksys_Configure_hxx

/* Include C configuration.  */
#include <itksys/Configure.h>

/* Whether wstring is available.  */
#define itksys_STL_HAS_WSTRING 1

/* If building a C++ file in kwsys itself, give the source file
   access to the macros without a configured namespace.  */
#if defined(KWSYS_NAMESPACE)
# if !itksys_NAME_IS_KWSYS
#  define kwsys     itksys
# endif
# define KWSYS_NAME_IS_KWSYS            itksys_NAME_IS_KWSYS
# define KWSYS_STL_HAS_WSTRING          itksys_STL_HAS_WSTRING
#endif

#endif
