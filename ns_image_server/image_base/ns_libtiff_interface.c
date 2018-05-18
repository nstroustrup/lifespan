#include "ns_libtiff_interface.h"
#include <fcntl.h>

#include "tiffio.h"

#ifdef _WIN32
#include <Windows.h>
#else
#include <stdlib.h>
#endif

//global variable used to store pointers to internal TIFF library functions
//This is done so we can declare our own TIFF error handlers using the TIFFlib C interface.

TIFF * ref_tif = 0;
int ns_DummyMapProc(thandle_t fd, void** pbase, toff_t* psize){return 0;}
void ns_DummyUnmapProc(thandle_t fd, void* base, toff_t size){}


tmsize_t ns_ReadProc(thandle_t client_data, void*b, tmsize_t c) {
	ns_tiff_client_data * cd = (ns_tiff_client_data *)(client_data);
	return ((cd->ReadProc))(cd->tiff_fd.fd, b, c);
}
tmsize_t ns_WriteProc(thandle_t client_data, void*b, tmsize_t c) {
		ns_tiff_client_data * cd = (ns_tiff_client_data *)(client_data);
		return ((cd->WriteProc))(cd->tiff_fd.fd, b, c);
	}
toff_t ns_SeekProc(thandle_t client_data, toff_t b, int c) {
	ns_tiff_client_data * cd = (ns_tiff_client_data *)(client_data);
	return ((cd->SeekProc))(cd->tiff_fd.fd,b,c);
}
int ns_CloseProc(thandle_t client_data) {
	ns_tiff_client_data * cd = (ns_tiff_client_data *)(client_data);
	return ((cd->CloseProc))(cd->tiff_fd.fd);
}
toff_t ns_SizeProc(thandle_t client_data) {
	ns_tiff_client_data * cd = (ns_tiff_client_data *)(client_data);
	return ((cd->SizeProc))(cd->tiff_fd.fd);
}
int ns_MapFileProc(thandle_t client_data, void** base, toff_t* size) {
	ns_tiff_client_data * cd = (ns_tiff_client_data *)(client_data);
	return ((cd->MapFileProc))(cd->tiff_fd.fd,base,size);
}
void ns_UnmapFileProc(thandle_t client_data, void* base, toff_t size) {
	ns_tiff_client_data * cd = (ns_tiff_client_data *)(client_data);
	if (cd->UnmapFileProc == 0)
		return;
	((cd->UnmapFileProc))(cd->tiff_fd.fd,base,size);
}

TIFF* ns_tiff_fd_open(ns_tiff_client_data * client_data, const char* name, const char* mode){
	TIFF* tif;
	int m;
	int fSuppressMap = 0;
	#ifdef _WIN32
	
	for (m = 0; mode[m] != 0; m++)
	{
		if (mode[m] == 'u')
		{
			fSuppressMap = 1;
			break;
		}
	}
	#endif
	client_data->ReadProc = TIFFGetReadProc(ref_tif);
	client_data->WriteProc = TIFFGetWriteProc(ref_tif);
	client_data->SeekProc = TIFFGetSeekProc(ref_tif);
	client_data->CloseProc = TIFFGetCloseProc(ref_tif);
	client_data->SizeProc = TIFFGetSizeProc(ref_tif);
	client_data->MapFileProc = fSuppressMap ? ns_DummyMapProc : TIFFGetMapFileProc(ref_tif);
	client_data->UnmapFileProc = fSuppressMap ? ns_DummyUnmapProc : TIFFGetUnmapFileProc(ref_tif);

	tif = TIFFClientOpen(name, mode, client_data, 
		ns_ReadProc,
		ns_WriteProc,
		ns_SeekProc,
		ns_CloseProc,
		ns_SizeProc,
		ns_MapFileProc,
		ns_UnmapFileProc
	);
	
	if (tif)
	  TIFFSetFileno(tif, client_data->tiff_fd.fd);

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


static void ns_initialization_tiff_error_handler(thandle_t client_data, const char * module, const char * fmt, va_list ap) {
	printf("libtiff::Initialization Error: ");
	printf(fmt, ap);
	exit(1);
}

static void ns_initialization_tiff_warning_handler(const char* module, const char* fmt, va_list ap) {
	printf("libtiff::Initialization Warning: ");
	printf(fmt, ap);
}

void ns_setup_libtiff(){
	#ifndef _WIN32
		int fd;
	#endif
  //We attempt to load a file here, not to open anything
  //but to get pointers to the internal TIFF library functions
  //so we can use them for our own handlers.
  if (ref_tif==0){
	  TIFFSetErrorHandler(0);
	  TIFFSetErrorHandlerExt(ns_initialization_tiff_error_handler);
	  TIFFSetWarningHandler(ns_initialization_tiff_warning_handler);
	#ifdef _WIN32
		ref_tif = TIFFOpen("nul","w");
	#else

		  fd = open("/dev/null",O_RDWR);
		  if (fd == 0){
			   printf("Trying file\n");
			   fd = open("ns_image_server_tifflib.tif",O_RDWR);
		  }
		  ref_tif = TIFFFdOpen(fd, "ns_image_server_tifflib.tif", "w");
		  close(fd);
		  /*printf("%p\n",(void*)TIFFGetReadProc(ref_tif));
		  printf("%p\n",(void*)TIFFGetWriteProc(ref_tif));
		  printf("%p\n",(void*)TIFFGetSeekProc(ref_tif));
		  printf("%p\n",(void*)TIFFGetCloseProc(ref_tif));
		  printf("%p\n",(void*)TIFFGetSizeProc(ref_tif));
		  printf("%p\n",(void*)TIFFGetMapFileProc(ref_tif));
		  printf("%p\n",(void*)TIFFGetUnmapFileProc(ref_tif));*/
	#endif

	}

}
TIFF* ns_tiff_open(const char* name, ns_tiff_client_data * client_data,const char* mode){

	client_data->tiff_fd.fd = 0;
	client_data->tiff_fd.h = 0;
	client_data->error_storage = 0;
	client_data->ReadProc = 0;
	client_data->WriteProc = 0;
	client_data->SeekProc = 0;
	client_data->CloseProc = 0;
	client_data->SizeProc = 0;
	client_data->MapFileProc = 0;
	client_data->UnmapFileProc = 0;

  //printf("NS OPENING %s\n",name);
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
	client_data->tiff_fd.h = (thandle_t)CreateFileA(name,
		(m == O_RDONLY)?GENERIC_READ:(GENERIC_READ | GENERIC_WRITE),
		FILE_SHARE_READ | FILE_SHARE_WRITE, NULL, dwMode,
		(m == O_RDONLY)?FILE_ATTRIBUTE_READONLY:FILE_ATTRIBUTE_NORMAL,
		NULL);
	if (client_data->tiff_fd.h == INVALID_HANDLE_VALUE) {
		TIFFErrorExt(0, module, "%s: Cannot open", name);
		return ((TIFF *)0);
	}
	tif = ns_tiff_fd_open(client_data, name, mode);
	if(!tif)
		CloseHandle(client_data->tiff_fd.h);
	return tif;
#else
	static const char module[] = "TIFFOpen";
	int m;
        TIFF* tif;

	m = ns_TIFFgetMode(mode, module);
	if (m == -1)
		return ((TIFF*)0);
	#ifdef O_BINARY
        m |= O_BINARY;
	#endif
	#ifdef _AM29K
	client_data->tiff_fd.fd = open(name, m);
	#else
	client_data->tiff_fd.fd = open(name, m, 0666);
	#endif
	if (client_data->tiff_fd.fd < 0) {
			client_data->tiff_fd.fd = 0;
			TIFFErrorExt(0, module, "%s: Cannot open", name);
		return ((TIFF *)0);
	}


	tif = ns_tiff_fd_open(client_data, name, mode);
	if(!tif)
		close(client_data->tiff_fd.fd);
	return tif;
#endif

}
