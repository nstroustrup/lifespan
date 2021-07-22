/*=========================================================================
 *
 *  Copyright Insight Software Consortium
 *
 *  Licensed under the Apache License, Version 2.0 (the "License");
 *  you may not use this file except in compliance with the License.
 *  You may obtain a copy of the License at
 *
 *         http://www.apache.org/licenses/LICENSE-2.0.txt
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 *=========================================================================*/
#include "itkNumericTraits.h"

namespace itk
{

#if !defined(_WIN32) || (ITK_COMPILED_CXX_VERSION >= 201103L)
ITK_CONSTEXPR bool NumericTraits< bool >:: Zero;
ITK_CONSTEXPR bool NumericTraits< bool >:: One;

ITK_CONSTEXPR unsigned char NumericTraits< unsigned char >:: Zero;
ITK_CONSTEXPR unsigned char NumericTraits< unsigned char >:: One;

ITK_CONSTEXPR signed char NumericTraits< signed char >:: Zero;
ITK_CONSTEXPR signed char NumericTraits< signed char >:: One;

ITK_CONSTEXPR char NumericTraits< char >:: Zero;
ITK_CONSTEXPR char NumericTraits< char >:: One;

ITK_CONSTEXPR unsigned short NumericTraits< unsigned short >:: Zero;
ITK_CONSTEXPR unsigned short NumericTraits< unsigned short >:: One;

ITK_CONSTEXPR short NumericTraits< short >:: Zero;
ITK_CONSTEXPR short NumericTraits< short >:: One;

ITK_CONSTEXPR unsigned int NumericTraits< unsigned int >:: Zero;
ITK_CONSTEXPR unsigned int NumericTraits< unsigned int >:: One;

ITK_CONSTEXPR int NumericTraits< int >:: Zero;
ITK_CONSTEXPR int NumericTraits< int >:: One;

ITK_CONSTEXPR unsigned long NumericTraits< unsigned long >:: Zero;
ITK_CONSTEXPR unsigned long NumericTraits< unsigned long >:: One;

ITK_CONSTEXPR long NumericTraits< long >:: Zero;
ITK_CONSTEXPR long NumericTraits< long >:: One;

ITK_CONSTEXPR long long NumericTraits< long long >:: Zero;
ITK_CONSTEXPR long long NumericTraits< long long >:: One;

ITK_CONSTEXPR unsigned long long NumericTraits< unsigned long long >:: Zero;
ITK_CONSTEXPR unsigned long long NumericTraits< unsigned long long >:: One;
#endif

// If not C++11, then use static initialization for real types
#if !(ITK_COMPILED_CXX_VERSION >= 201103L)

const float NumericTraits< float >:: Zero = 0.0F;
const float NumericTraits< float >:: One = 1.0F;

const double NumericTraits< double >:: Zero = 0.0;
const double NumericTraits< double >:: One = 1.0;

const long double NumericTraits< long double >:: Zero = 0.0;
const long double NumericTraits< long double >:: One = 1.0;

#else

ITK_CONSTEXPR float NumericTraits< float >:: Zero;
ITK_CONSTEXPR float NumericTraits< float >:: One;

ITK_CONSTEXPR double NumericTraits< double >:: Zero;
ITK_CONSTEXPR double NumericTraits< double >:: One;

ITK_CONSTEXPR long double NumericTraits< long double >:: Zero;
ITK_CONSTEXPR long double NumericTraits< long double >:: One;


#endif

const std::complex< char >  NumericTraits< std::complex< char > >:: Zero = std::complex< char >(0, 0);
const std::complex< char >  NumericTraits< std::complex< char > >:: One  = std::complex< char >(1, 0);

const std::complex< unsigned char >  NumericTraits< std::complex< unsigned char > >:: Zero = std::complex< unsigned char >(0, 0);
const std::complex< unsigned char >  NumericTraits< std::complex< unsigned char > >:: One  = std::complex< unsigned char >(1, 0);

const std::complex< short >  NumericTraits< std::complex< short > >:: Zero = std::complex< short >(0, 0);
const std::complex< short >  NumericTraits< std::complex< short > >:: One  = std::complex< short >(1, 0);

const std::complex< unsigned short >  NumericTraits< std::complex< unsigned short > >:: Zero = std::complex< unsigned short >(0, 0);
const std::complex< unsigned short >  NumericTraits< std::complex< unsigned short > >:: One  = std::complex< unsigned short >(1, 0);

const std::complex< int >  NumericTraits< std::complex< int > >:: Zero = std::complex< int >(0, 0);
const std::complex< int >  NumericTraits< std::complex< int > >:: One  = std::complex< int >(1, 0);

const std::complex< unsigned int >  NumericTraits< std::complex< unsigned int > >:: Zero = std::complex< unsigned int >(0, 0);
const std::complex< unsigned int >  NumericTraits< std::complex< unsigned int > >:: One  = std::complex< unsigned int >(1, 0);

const std::complex< long >  NumericTraits< std::complex< long > >:: Zero = std::complex< long >(0L, 0L);
const std::complex< long >  NumericTraits< std::complex< long > >:: One  = std::complex< long >(1L, 0L);

const std::complex< unsigned long >  NumericTraits< std::complex< unsigned long > >:: Zero = std::complex< unsigned long >(0UL, 0UL);
const std::complex< unsigned long >  NumericTraits< std::complex< unsigned long > >:: One  = std::complex< unsigned long >(1UL, 0UL);

const std::complex< float >  NumericTraits< std::complex< float > >:: Zero = std::complex< float >(0.0f, 0.0f);
const std::complex< float >  NumericTraits< std::complex< float > >:: One  = std::complex< float >(1.0f, 0.0f);

const std::complex< double >  NumericTraits< std::complex< double > >:: Zero = std::complex< double >(0.0, 0.0);
const std::complex< double >  NumericTraits< std::complex< double > >:: One  = std::complex< double >(1.0, 0.0);

const std::complex< long double >  NumericTraits< std::complex< long double > >:: Zero = std::complex< long double >(0.0, 0.0);
const std::complex< long double >  NumericTraits< std::complex< long double > >:: One  = std::complex< long double >(1.0, 0.0);

} // end namespace itk