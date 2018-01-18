#ifndef NS_LIBTIFF_INTERFACE
#define NS_LIBTIFF_INTERFACE

#include "tiffiop.h"
//#include <string.h>


typedef union fd_as_handle_union
{
#ifdef _WIN32
	thandle_t fd;
#else
	int fd;
#endif
	thandle_t h;
} fd_as_handle_union_t;

typedef struct ns_tiff_client_data_{
	fd_as_handle_union_t tiff_fd;

	void * error_storage;
	int store_errors;
} ns_tiff_client_data;

#ifdef __cplusplus
extern "C"
#endif
TIFF* ns_tiff_open(const char* name, ns_tiff_client_data * client_data,const char* mode);

#endif

