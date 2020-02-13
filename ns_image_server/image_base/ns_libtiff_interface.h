#ifndef NS_LIBTIFF_INTERFACE
#define NS_LIBTIFF_INTERFACE

#include "tiffio.h"
//#include <string.h>
typedef void* thandle_t;

typedef union fd_as_handle_union
{
	int fd;
	thandle_t h;
} fd_as_handle_union_t;

typedef struct ns_tiff_client_data_{
	fd_as_handle_union_t tiff_fd;

	void * error_storage;
	int store_errors;

	TIFFReadWriteProc ReadProc;
	TIFFReadWriteProc WriteProc;
	TIFFSeekProc SeekProc;
	TIFFCloseProc CloseProc;
	TIFFSizeProc SizeProc;
	TIFFMapFileProc MapFileProc;
	TIFFUnmapFileProc UnmapFileProc;
} ns_tiff_client_data;

#ifdef __cplusplus
extern "C"
#endif
TIFF* ns_tiff_open(const char* name, ns_tiff_client_data * client_data,const char* mode);

#ifdef __cplusplus
extern "C"
#endif
void ns_setup_libtiff();

#endif

