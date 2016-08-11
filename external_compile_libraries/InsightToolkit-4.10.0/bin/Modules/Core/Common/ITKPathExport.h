
#ifndef ITKPath_EXPORT_H
#define ITKPath_EXPORT_H

#ifdef ITK_STATIC
#  define ITKPath_EXPORT
#  define ITKPath_TEMPLATE_EXPORT
#  define ITKPath_HIDDEN
#else
#  ifndef ITKPath_EXPORT
#    ifdef ITKPath_EXPORTS
        /* We are building this library */
#      define ITKPath_EXPORT 
#    else
        /* We are using this library */
#      define ITKPath_EXPORT 
#    endif
#  endif

#  ifndef ITKPath_TEMPLATE_EXPORT
#    ifdef ITKPath_EXPORTS
        /* We are building this library */
#      define ITKPath_TEMPLATE_EXPORT 
#    else
        /* We are using this library */
#      define ITKPath_TEMPLATE_EXPORT 
#    endif
#  endif

#  ifndef ITKPath_HIDDEN
#    define ITKPath_HIDDEN 
#  endif
#endif

#ifndef ITKPATH_DEPRECATED
#  define ITKPATH_DEPRECATED __declspec(deprecated)
#endif

#ifndef ITKPATH_DEPRECATED_EXPORT
#  define ITKPATH_DEPRECATED_EXPORT ITKPath_EXPORT ITKPATH_DEPRECATED
#endif

#ifndef ITKPATH_DEPRECATED_NO_EXPORT
#  define ITKPATH_DEPRECATED_NO_EXPORT ITKPath_HIDDEN ITKPATH_DEPRECATED
#endif

#define DEFINE_NO_DEPRECATED 0
#if DEFINE_NO_DEPRECATED
# define ITKPATH_NO_DEPRECATED
#endif

#endif
