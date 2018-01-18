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

TIFF* ns_tiff_fd_open(ns_tiff_client_data * client_data, const char* name, const char* mode){
	TIFF* tif;
	#ifdef _WIN32 
	int fSuppressMap;
	int m;
	fSuppressMap = 0;
	for (m = 0; mode[m] != 0; m++)
	{
		if (mode[m] == 'u')
		{
			fSuppressMap = 1;
			break;
		}
	}
	tif = TIFFClientOpen(name, mode, client_data->tiff_fd.h, /* FIXME: WIN64 cast to pointer warning */
			_tiffReadProc, _tiffWriteProc,
			_tiffSeekProc, _tiffCloseProc, _tiffSizeProc,
			fSuppressMap ? _tiffDummyMapProc : _tiffMapProc,
			fSuppressMap ? _tiffDummyUnmapProc : _tiffUnmapProc);
	#else
	    
		tif = TIFFClientOpen(name, mode,
			client_data->tiff_fd.h,
			_tiffReadProc, _tiffWriteProc,
			_tiffSeekProc, _tiffCloseProc, _tiffSizeProc,
			_tiffMapProc, _tiffUnmapProc);
	

	#endif	
	if (tif)
	tif->tif_fd = client_data->tiff_fd.fd;
	return (tif);
}


int ns_TIFFgetMode(const char* mode, const char* module)
{
	int m = -1;

	switch (mode[0]) {
	case 'r':
		m = O_RDONLY;
		if (mode[1] == '+')
			m = O_RDWR;
		break;
	case 'w':
	case 'a':
		m = O_RDWR|O_CREAT;
		if (mode[0] == 'w')
			m |= O_TRUNC;
		break;
	default:
		TIFFErrorExt(0, module, "\"%s\": Bad mode", mode);
		break;
	}
	return (m);
}


TIFF* ns_tiff_open(const char* name, ns_tiff_client_data * client_data,const char* mode){
#ifdef _WIN32 
	static const char module[] = "TIFFOpen";

	int m;
	DWORD dwMode;
	TIFF* tif;

	m = ns_TIFFgetMode(mode, module);

	switch (m) {
	case O_RDONLY:			dwMode = OPEN_EXISTING; break;
	case O_RDWR:			dwMode = OPEN_ALWAYS;   break;
	case O_RDWR | O_CREAT:		dwMode = OPEN_ALWAYS;   break;
	case O_RDWR | O_TRUNC:		dwMode = CREATE_ALWAYS; break;
	case O_RDWR | O_CREAT | O_TRUNC:	dwMode = CREATE_ALWAYS; break;
	default:			return ((TIFF*)0);
	}
	client_data->tiff_fd.fd = (thandle_t)CreateFileA(name,
		(m == O_RDONLY)?GENERIC_READ:(GENERIC_READ | GENERIC_WRITE),
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, dwMode,
		(m == O_RDONLY)?FILE_ATTRIBUTE_READONLY:FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (client_data->tiff_fd.fd == INVALID_HANDLE_VALUE) {
		TIFFErrorExt(0, module, "%s: Cannot open", name);
		return ((TIFF *)0);
	}
	tif = ns_tiff_fd_open(client_data, name, mode);
	if(!tif)
		CloseHandle(client_data->tiff_fd.fd);
	return tif;
#else
	static const char module[] = "TIFFOpen";
	int m;
        TIFF* tif;

	m = ns_TIFFgetMode(mode, module);
	if (m == -1)
		return ((TIFF*)0);

/* for cygwin and mingw */        
	#ifdef O_BINARY
        m |= O_BINARY;
	#endif        
	#ifdef _AM29K
	client_data->tiff_fd.fd = open(name, m);
	#else
	client_data->tiff_fd.fd = open(name, m, 0666);
	#endif
	if (client_data->tiff_fd.fd < 0) {
		if (errno > 0 && strerror(errno) != NULL) {
			TIFFErrorExt(0, module, "%s: %s", name, strerror(errno));
		}
		else {
			TIFFErrorExt(0, module, "%s: Cannot open", name);
		}
		return ((TIFF *)0);
	}


	tif = ns_tiff_fd_open(client_data, name, mode);
	if(!tif)
		close(client_data->fd);
	return tif;
#endif
}