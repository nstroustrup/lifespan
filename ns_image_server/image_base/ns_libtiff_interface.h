#ifndef NS_LIBTIFF_INTERFACE
#define NS_LIBTIFF_INTERFACE

#include "tiffio.h"
//#include <string.h>


struct ns_tiff_client_data{
	thandle_t file_descriptor;

	void * error_storage;
	int store_errors;
};
#ifdef __cplusplus
extern "C"
#endif
TIFF* ns_tiff_open(const char* name, struct ns_tiff_client_data * client_data,const char* mode);

#endif

