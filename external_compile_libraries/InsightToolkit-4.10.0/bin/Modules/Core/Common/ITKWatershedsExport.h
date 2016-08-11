
#ifndef ITKWatersheds_EXPORT_H
#define ITKWatersheds_EXPORT_H

#ifdef ITK_STATIC
#  define ITKWatersheds_EXPORT
#  define ITKWatersheds_TEMPLATE_EXPORT
#  define ITKWatersheds_HIDDEN
#else
#  ifndef ITKWatersheds_EXPORT
#    ifdef ITKWatersheds_EXPORTS
        /* We are building this library */
#      define ITKWatersheds_EXPORT 
#    else
        /* We are using this library */
#      define ITKWatersheds_EXPORT 
#    endif
#  endif

#  ifndef ITKWatersheds_TEMPLATE_EXPORT
#    ifdef ITKWatersheds_EXPORTS
        /* We are building this library */
#      define ITKWatersheds_TEMPLATE_EXPORT 
#    else
        /* We are using this library */
#      define ITKWatersheds_TEMPLATE_EXPORT 
#    endif
#  endif

#  ifndef ITKWatersheds_HIDDEN
#    define ITKWatersheds_HIDDEN 
#  endif
#endif

#ifndef ITKWATERSHEDS_DEPRECATED
#  define ITKWATERSHEDS_DEPRECATED __declspec(deprecated)
#endif

#ifndef ITKWATERSHEDS_DEPRECATED_EXPORT
#  define ITKWATERSHEDS_DEPRECATED_EXPORT ITKWatersheds_EXPORT ITKWATERSHEDS_DEPRECATED
#endif

#ifndef ITKWATERSHEDS_DEPRECATED_NO_EXPORT
#  define ITKWATERSHEDS_DEPRECATED_NO_EXPORT ITKWatersheds_HIDDEN ITKWATERSHEDS_DEPRECATED
#endif

#define DEFINE_NO_DEPRECATED 0
#if DEFINE_NO_DEPRECATED
# define ITKWATERSHEDS_NO_DEPRECATED
#endif

#endif
