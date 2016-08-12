/* libtiff/tif_config.h.in.  Generated from configure.ac by autoheader.  */
#ifndef __tif_config_h
#define __tif_config_h

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

#if defined( HAVE_STDINT_H )
#include <stdint.h>
#else
// the system doesn't have the C or C++ version of stdint so lets use
// KWIML's macros for fixed widths
#include "itk_kwiml.h"
#endif

/* Define if building universal (internal helper macro) */
#if defined(__APPLE__)
#define AC_APPLE_UNIVERSAL_BUILD
#endif

/* Support CCITT Group 3 & 4 algorithms */
#define CCITT_SUPPORT 1

/* Pick up YCbCr subsampling info from the JPEG data stream to support files
   lacking the tag (default enabled). */
/* #undef CHECK_JPEG_YCBCR_SUBSAMPLING */

/* enable partial strip reading for large strips (experimental) */
#undef CHUNKY_STRIP_READ_SUPPORT

/* Support C++ stream API (requires C++ compiler) */
/* #undef CXX_SUPPORT */

/* Treat extra sample as alpha (default enabled). The RGBA interface will
   treat a fourth sample with no EXTRASAMPLE_ value as being ASSOCALPHA. Many
   packages produce RGBA files but don't mark the alpha properly. */
/* #undef DEFAULT_EXTRASAMPLE_AS_ALPHA */

/* enable deferred strip/tile offset/size loading (experimental) */
#undef DEFER_STRILE_LOAD

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `floor' function. */
#define HAVE_FLOOR 1

/* Define to 1 if you have the `getopt' function. */
/* #undef HAVE_GETOPT */

/* Define to 1 if you have the <GLUT/glut.h> header file. */
#undef HAVE_GLUT_GLUT_H

/* Define to 1 if you have the <GL/glut.h> header file. */
#undef HAVE_GL_GLUT_H

/* Define to 1 if you have the <GL/glu.h> header file. */
#undef HAVE_GL_GLU_H

/* Define to 1 if you have the <GL/gl.h> header file. */
#undef HAVE_GL_GL_H

/* Define as 0 or 1 according to the floating point format suported by the
   machine */
#define HAVE_IEEEFP 1

/* Define to 1 if the system has the type `int16'. */
/* #undef HAVE_INT16 */

/* Define to 1 if the system has the type `int32'. */
/* #undef HAVE_INT32 */

/* Define to 1 if the system has the type `int8'. */
/* #undef HAVE_INT8 */

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <io.h> header file. */
#undef HAVE_IO_H

/* Define to 1 if you have the `isascii' function. */
/* #undef HAVE_ISASCII */

/* Define to 1 if you have the `jbg_newlen' function. */
#undef HAVE_JBG_NEWLEN

/* Define to 1 if you have the `lfind' function. */
#undef HAVE_LFIND

/* Define to 1 if you have the `c' library (-lc). */
/* #undef HAVE_LIBC */

/* Define to 1 if you have the `m' library (-lm). */
/* #undef HAVE_LIBM */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <malloc.h> header file. */
#define HAVE_MALLOC_H 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the `mmap' function. */
/* #undef HAVE_MMAP */

/* Define to 1 if you have the <OpenGL/glu.h> header file. */
#undef HAVE_OPENGL_GLU_H

/* Define to 1 if you have the <OpenGL/gl.h> header file. */
#undef HAVE_OPENGL_GL_H

/* Define to 1 if you have the `pow' function. */
#define HAVE_POW 1

/* Define if you have POSIX threads libraries and header files. */
/* #undef HAVE_PTHREAD */

/* Define to 1 if you have the <search.h> header file. */
#define HAVE_SEARCH_H

/* Define to 1 if you have the `setmode' function. */
#undef HAVE_SETMODE

/* Define to 1 if you have the `sqrt' function. */
#define HAVE_SQRT 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
/* #undef HAVE_STRCASECMP */

/* Define to 1 if you have the `strchr' function. */
#define HAVE_STRCHR 1

/* Define to 1 if you have the <strings.h> header file. */
/* #undef HAVE_STRINGS_H */

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strrchr' function. */
#define HAVE_STRRCHR 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtol' function. */
#define HAVE_STRTOL 1

/* Define to 1 if you have the `strtoul' function. */
#define HAVE_STRTOUL 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
/* #undef HAVE_SYS_TIME_H */

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the <unistd.h> header file. */
/* #undef HAVE_UNISTD_H */

/* Define to 1 if you have the <windows.h> header file. */
#define HAVE_WINDOWS_H 1


/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
  #define WORDS_BIGENDIAN 1
# endif
#else
/* #undef WORDS_BIGENDIAN */
#endif


/* Native cpu byte order: 1 if big-endian (Motorola) or 0 if little-endian
   (Intel) */
#undef HOST_BIGENDIAN
/* Set the native cpu bit order (FILLORDER_LSB2MSB or FILLORDER_MSB2LSB) */
#undef HOST_FILLORDER

#ifdef WORDS_BIGENDIAN
# define HOST_BIGENDIAN 1
# define HOST_FILLORDER FILLORDER_LSB2MSB
#else
# define HOST_BIGENDIAN 0
# define HOST_FILLORDER FILLORDER_MSB2LSB
#endif



/* Support ISO JBIG compression (requires JBIG-KIT library) */
#undef JBIG_SUPPORT

/* 8/12 bit libjpeg dual mode enabled */
#undef JPEG_DUAL_MODE_8_12

/* Support JPEG compression (requires IJG JPEG library) */
/* #undef JPEG_SUPPORT */

/* 12bit libjpeg primary include file with path */
#undef LIBJPEG_12_PATH

/* Support LogLuv high dynamic range encoding */
#define LOGLUV_SUPPORT 1

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#undef LT_OBJDIR

/* Support LZMA2 compression */
#undef LZMA_SUPPORT

/* Support LZW algorithm */
#define LZW_SUPPORT 1

/* Support Microsoft Document Imaging format */
#undef MDI_SUPPORT

/* Support NeXT 2-bit RLE algorithm */
#define NEXT_SUPPORT 1

/* Define to 1 if your C compiler doesn't accept -c and -o together. */
/* #undef NO_MINUS_C_MINUS_O */

/* Support Old JPEG compresson (read contrib/ojpeg/README first! Compilation
   fails with unpatched IJG JPEG library) */
/* #undef OJPEG_SUPPORT */

/* Name of package */
/* #undef PACKAGE */

/* Define to the address where bug reports for this package should be sent. */
/* #undef PACKAGE_BUGREPORT */

/* Define to the full name of this package. */
/* #undef PACKAGE_NAME */

/* Define to the full name and version of this package. */
/* #undef PACKAGE_STRING */

/* Define to the one symbol short name of this package. */
/* #undef PACKAGE_TARNAME */

/* Define to the home page for this package. */
#undef PACKAGE_URL

/* Define to the version of this package. */
/* #undef PACKAGE_VERSION */

/* Support Macintosh PackBits algorithm */
#define PACKBITS_SUPPORT 1

/* Support Pixar log-format algorithm (requires Zlib) */
/* #undef PIXARLOG_SUPPORT */

/* Define to the necessary symbol if this constant uses a non-standard name on
   your system. */
/* #undef PTHREAD_CREATE_JOINABLE */

/* The size of `signed int', as computed by sizeof. */
#define SIZEOF_SIGNED_INT 4

/* The size of `signed long', as computed by sizeof. */
#define SIZEOF_SIGNED_LONG 4

/* The size of `signed long long', as computed by sizeof. */
#undef SIZEOF_SIGNED_LONG_LONG

/* The size of `signed short', as computed by sizeof. */
#define SIZEOF_SIGNED_SHORT 

/* The size of `unsigned char *', as computed by sizeof. */
#undef SIZEOF_UNSIGNED_CHAR_P

/* The size of `unsigned int', as computed by sizeof. */
#define SIZEOF_UNSIGNED_INT 4

/* The size of `unsigned long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG 4

/* On Apple, universal builds use different settings on each build.
the sizes can be different.*/
#if defined(__APPLE__)
#undef SIZEOF_SIGNED_LONG
#  if defined(__LP64__) && __LP64__
#    define SIZEOF_SIGNED_LONG 8
#  else
#    define SIZEOF_SIGNED_LONG 4
#  endif
#undef SIZEOF_UNSIGNED_LONG
#  if defined(__LP64__) && __LP64__
#    define SIZEOF_UNSIGNED_LONG 8
#  else
#    define SIZEOF_UNSIGNED_LONG 4
#  endif
#endif

/* The size of `unsigned long long', as computed by sizeof. */
#define SIZEOF_UNSIGNED_LONG_LONG 4

/* The size of `unsigned short', as computed by sizeof. */
#define SIZEOF_UNSIGNED_SHORT 

/* Define to 1 if you have the ANSI C header files. */
#undef STDC_HEADERS
/* #undef STDC_HEADERS */

/* Support strip chopping (whether or not to convert single-strip uncompressed
   images to mutiple strips of specified size to reduce memory usage) */
/* #undef STRIPCHOP_DEFAULT */

/* Default size of the strip in bytes (when strip chopping enabled) */
#undef STRIP_SIZE_DEFAULT

/* Enable SubIFD tag (330) support */
/* #undef SUBIFD_SUPPORT */

/* Support ThunderScan 4-bit RLE algorithm */
/* #undef THUNDER_SUPPORT */

/* Signed 16-bit type */
#undef TIFF_INT16_T
#if defined( HAVE_STDINT_H )
#define TIFF_INT16_T  int16_t
#else
#define TIFF_INT16_T  KWIML_INT_int16_t
#endif

/* Signed 32-bit type formatter */
#define TIFF_INT32_FORMAT "%d"

/* Signed 32-bit type */
#undef TIFF_INT32_T
#if defined( HAVE_STDINT_H )
#define TIFF_INT32_T  int32_t
#else
#define TIFF_INT32_T  KWIML_INT_int32_t
#endif

/* Signed 64-bit type formatter */
#define TIFF_INT64_FORMAT "%ld"

/* Signed 64-bit type */
#undef TIFF_INT64_T
#if defined( HAVE_STDINT_H )
#define TIFF_INT64_T  int64_t
#else
#define TIFF_INT64_T  KWIML_INT_int64_t
#endif

/* Signed 8-bit type */
#undef TIFF_INT8_T
#if defined( HAVE_STDINT_H )
#define TIFF_INT8_T  int8_t
#else
#define TIFF_INT8_T  KWIML_INT_int8_t
#endif

/* Pointer difference type formatter */
#define TIFF_PTRDIFF_FORMAT "%ld"

/* Pointer difference type */
#undef TIFF_PTRDIFF_T

/* Signed size type formatter */
#define TIFF_SSIZE_FORMAT "%zd"

/* Signed size type */
#undef TIFF_SSIZE_T
#define TIFF_SSIZE_T  intptr_t

/* Unsigned 16-bit type */
#undef TIFF_UINT16_T
#if defined( HAVE_STDINT_H )
#define TIFF_UINT16_T  uint16_t
#else
#define TIFF_UINT16_T  KWIML_INT_uint16_t
#endif

/* Unsigned 32-bit type formatter */
#define TIFF_UINT32_FORMAT "%d"

/* Unsigned 32-bit type */
#undef TIFF_UINT32_T
#if defined( HAVE_STDINT_H )
#define TIFF_UINT32_T  uint32_t
#else
#define TIFF_UINT32_T  KWIML_INT_uint32_t
#endif

/* Unsigned 64-bit type formatter */
#define TIFF_UINT64_FORMAT  "%ld"

/* Unsigned 64-bit type */
#undef TIFF_UINT64_T
#if defined( HAVE_STDINT_H )
#define TIFF_UINT64_T  uint64_t
#else
#define TIFF_UINT64_T  KWIML_INT_uint64_t
#endif

/* Unsigned 8-bit type */
#undef TIFF_UINT8_T
#if defined( HAVE_STDINT_H )
#define TIFF_UINT8_T  uint8_t
#else
#define TIFF_UINT8_T  KWIML_INT_uint8_t
#endif

/* Define to 1 if you can safely include both <sys/time.h> and <time.h>. */
/* #undef TIME_WITH_SYS_TIME */

/* Define to 1 if your <sys/time.h> declares `struct tm'. */
/* #undef TM_IN_SYS_TIME */

/* define to use win32 IO system */
#undef USE_WIN32_FILEIO

/* Version number of package */
/* #undef VERSION */

/* Define to 1 if the X Window System is missing or not being used. */
/* #undef X_DISPLAY_MISSING */

/* Support Deflate compression */
/* #undef ZIP_SUPPORT */

/* Number of bits in a file offset, on hosts where this is settable. */
/* #undef _FILE_OFFSET_BITS */

/* Define for large files, on AIX-style hosts. */
/* #undef _LARGE_FILES */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* MSVC does not support C99 inline, so just make the inline keyword
   disappear for C.  */
#ifndef __cplusplus
#  ifdef _MSC_VER
#    define inline
#  endif
#endif

/* Define to `long int' if <sys/types.h> does not define. */
/* #undef off_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

#endif
