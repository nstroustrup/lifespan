#include "ns_libtiff_interface.h"
#include <fcntl.h>

//apologies for pulling in the OS-dependant tiff files as includes.
//Unfortunately libtiff conflates file handling and error handling by storing
//file descriptors in the client_data field by default.  Thus, to specify custon
//client_data we need to reimplement all the OS-dependant file handling code.
//Rather than rewriting this from scratch we just use wrappers to the extant code.
//Because all of the _tiff functions are declared as static we pull them in as includes.
#ifdef _WIN32 
#include <Windows.h>
#include "tif_win32_proconly.c"
#else
#include "tif_unix_proconly.c"
#endif

static tsize_t ns_tiffReadProc(thandle_t fd, tdata_t buf, tsize_t size){
	return _tiffReadProc(((struct ns_tiff_client_data *)fd)->file_descriptor,buf,size);
}

static tsize_t ns_tiffWriteProc(thandle_t fd, tdata_t buf, tsize_t size){
	return _tiffWriteProc(((struct ns_tiff_client_data *)fd)->file_descriptor,buf,size);
}

static toff_t ns_tiffSeekProc(thandle_t fd, toff_t off, int whence){
	return _tiffSeekProc(((struct ns_tiff_client_data *)fd)->file_descriptor, off,whence);
}
static int ns_tiffCloseProc(thandle_t fd){
	return _tiffCloseProc(((struct ns_tiff_client_data *)fd)->file_descriptor);
}
static toff_t ns_tiffSizeProc(thandle_t fd){
	return _tiffSizeProc(((struct ns_tiff_client_data *)fd)->file_descriptor);
}
static int ns_tiffMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize){
	return _tiffMapProc(((struct ns_tiff_client_data *)fd)->file_descriptor,pbase,psize);
}

static void ns_tiffUnmapProc(thandle_t fd, tdata_t base, toff_t size){
	_tiffUnmapProc(((struct ns_tiff_client_data *)fd)->file_descriptor,base,size);
}

#ifdef _WIN32 
static int ns_tiffDummyMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize){
	return _tiffDummyMapProc(((struct ns_tiff_client_data *)fd)->file_descriptor,pbase,psize);
}
static void ns_tiffDummyUnmapProc(thandle_t fd, tdata_t base, toff_t size){
	_tiffDummyUnmapProc(((struct ns_tiff_client_data *)fd)->file_descriptor,base,size);
}
#endif

TIFF* ns_tiff_fd_open(struct ns_tiff_client_data * client_data, const char* name, const char* mode){
	TIFF * tif;
	#ifdef _WIN32 
	int fSuppressMap = (mode[1] == 'u' || (mode[1]!=0 && mode[2] == 'u'));

	tif = TIFFClientOpen(name, mode, (thandle_t)client_data,
			ns_tiffReadProc, ns_tiffWriteProc,
			ns_tiffSeekProc, ns_tiffCloseProc, ns_tiffSizeProc,
			fSuppressMap ? ns_tiffDummyMapProc : ns_tiffMapProc,
			fSuppressMap ? ns_tiffDummyUnmapProc : ns_tiffUnmapProc);
	#else
		tif = TIFFClientOpen(name, mode,
			(thandle_t) client_data,
			ns_tiffReadProc, ns_tiffWriteProc,
			ns_tiffSeekProc, ns_tiffCloseProc, ns_tiffSizeProc,
			ns_tiffMapProc, ns_tiffUnmapProc);

	#endif
	if (tif)
		TIFFSetFileno(tif, (int)client_data->file_descriptor);
	return (tif);
}

TIFF* ns_tiff_open(const char* name, struct ns_tiff_client_data * client_data,const char* mode){
#ifdef _WIN32 
	static const char module[] = "TIFFOpen";

	int m;
	DWORD dwMode;
	TIFF* tif;

	m = _TIFFgetMode(mode, module);

	switch(m)
	{
	case O_RDONLY:
		dwMode = OPEN_EXISTING;
		break;
	case O_RDWR:
		dwMode = OPEN_ALWAYS;
		break;
	case O_RDWR|O_CREAT:
		dwMode = OPEN_ALWAYS;
		break;
	case O_RDWR|O_TRUNC:
		dwMode = CREATE_ALWAYS;
		break;
	case O_RDWR|O_CREAT|O_TRUNC:
		dwMode = CREATE_ALWAYS;
		break;
	default:
		return ((TIFF*)0);
	}
	client_data->file_descriptor = (thandle_t)CreateFileA(name,
		(m == O_RDONLY)?GENERIC_READ:(GENERIC_READ | GENERIC_WRITE),
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, dwMode,
		(m == O_RDONLY)?FILE_ATTRIBUTE_READONLY:FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (client_data->file_descriptor == INVALID_HANDLE_VALUE) {
		TIFFErrorExt(0, module, "%s: Cannot open", name);
		return ((TIFF *)0);
	}
	tif = ns_tiff_fd_open(client_data, name, mode);
	if(!tif)
		CloseHandle(client_data->file_descriptor);
	return tif;
#else
	static const char module[] = "TIFFOpen";
	int m;
        TIFF* tif;

	m = _TIFFgetMode(mode, module);
	if (m == -1)
		return ((TIFF*)0);

/* for cygwin and mingw */        
	#ifdef O_BINARY
        m |= O_BINARY;
	#endif        
	#ifdef _AM29K
	client_data->file_descriptor = open(name, m);
	#else
	client_data->file_descriptor = open(name, m, 0666);
	#endif
	if (client_data->file_descriptor < 0) {
		TIFFErrorExt(0, module, "%s: Cannot open", name);
		return ((TIFF *)0);
	}

	tif = ns_tiff_fd_open(client_data, name, mode);
	if(!tif)
		close(client_data->file_descriptor);
	return tif;
#endif
}