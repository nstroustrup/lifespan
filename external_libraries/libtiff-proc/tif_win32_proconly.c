/* $Id: tif_win32.c,v 1.42 2017-01-11 19:02:49 erouault Exp $ */

/*
 * Copyright (c) 1988-1997 Sam Leffler
 * Copyright (c) 1991-1997 Silicon Graphics, Inc.
 *
 * Permission to use, copy, modify, distribute, and sell this software and
 * its documentation for any purpose is hereby granted without fee, provided
 * that (i) the above copyright notices and this permission notice appear in
 * all copies of the software and related documentation, and (ii) the names of
 * Sam Leffler and Silicon Graphics may not be used in any advertising or
 * publicity relating to the software without the specific, prior written
 * permission of Sam Leffler and Silicon Graphics.
 *
 * THE SOFTWARE IS PROVIDED "AS-IS" AND WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS, IMPLIED OR OTHERWISE, INCLUDING WITHOUT LIMITATION, ANY
 * WARRANTY OF MERCHANTABILITY OR FITNESS FOR A PARTICULAR PURPOSE.
 *
 * IN NO EVENT SHALL SAM LEFFLER OR SILICON GRAPHICS BE LIABLE FOR
 * ANY SPECIAL, INCIDENTAL, INDIRECT OR CONSEQUENTIAL DAMAGES OF ANY KIND,
 * OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS,
 * WHETHER OR NOT ADVISED OF THE POSSIBILITY OF DAMAGE, AND ON ANY THEORY OF
 * LIABILITY, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE
 * OF THIS SOFTWARE.
 */

/*
 * TIFF Library Win32-specific Routines.  Adapted from tif_unix.c 4/5/95 by
 * Scott Wagner (wagner@itek.com), Itek Graphix, Rochester, NY USA
 */

/*
  CreateFileA/CreateFileW return type 'HANDLE'.

  thandle_t is declared like

    DECLARE_HANDLE(thandle_t);

  in tiffio.h.

  Windows (from winnt.h) DECLARE_HANDLE logic looks like

  #ifdef STRICT
    typedef void *HANDLE;
  #define DECLARE_HANDLE(name) struct name##__ { int unused; }; typedef struct name##__ *name
  #else
    typedef PVOID HANDLE;
  #define DECLARE_HANDLE(name) typedef HANDLE name
  #endif

  See http://bugzilla.maptools.org/show_bug.cgi?id=1941 for problems in WIN64
  builds resulting from this.  Unfortunately, the proposed patch was lost.

*/

#include "tiffiop.h"

#include <windows.h>

static tmsize_t
_tiffReadProc(thandle_t fd, void* buf, tmsize_t size)
{
	/* tmsize_t is 64bit on 64bit systems, but the WinAPI ReadFile takes
	 * 32bit sizes, so we loop through the data in suitable 32bit sized
	 * chunks */
	uint8* ma;
	uint64 mb;
	DWORD n;
	DWORD o;
	tmsize_t p;
	ma=(uint8*)buf;
	mb=size;
	p=0;
	while (mb>0)
	{
		n=0x80000000UL;
		if ((uint64)n>mb)
			n=(DWORD)mb;
		if (!ReadFile(fd,(LPVOID)ma,n,&o,NULL))
			return(0);
		ma+=o;
		mb-=o;
		p+=o;
		if (o!=n)
			break;
	}
	return(p);
}

static tmsize_t
_tiffWriteProc(thandle_t fd, void* buf, tmsize_t size)
{
	/* tmsize_t is 64bit on 64bit systems, but the WinAPI WriteFile takes
	 * 32bit sizes, so we loop through the data in suitable 32bit sized
	 * chunks */
	uint8* ma;
	uint64 mb;
	DWORD n;
	DWORD o;
	tmsize_t p;
	ma=(uint8*)buf;
	mb=size;
	p=0;
	while (mb>0)
	{
		n=0x80000000UL;
		if ((uint64)n>mb)
			n=(DWORD)mb;
		if (!WriteFile(fd,(LPVOID)ma,n,&o,NULL))
			return(0);
		ma+=o;
		mb-=o;
		p+=o;
		if (o!=n)
			break;
	}
	return(p);
}

static uint64
_tiffSeekProc(thandle_t fd, uint64 off, int whence)
{
	LARGE_INTEGER offli;
	DWORD dwMoveMethod;
	offli.QuadPart = off;
	switch(whence)
	{
		case SEEK_SET:
			dwMoveMethod = FILE_BEGIN;
			break;
		case SEEK_CUR:
			dwMoveMethod = FILE_CURRENT;
			break;
		case SEEK_END:
			dwMoveMethod = FILE_END;
			break;
		default:
			dwMoveMethod = FILE_BEGIN;
			break;
	}
	offli.LowPart=SetFilePointer(fd,offli.LowPart,&offli.HighPart,dwMoveMethod);
	if ((offli.LowPart==INVALID_SET_FILE_POINTER)&&(GetLastError()!=NO_ERROR))
		offli.QuadPart=0;
	return(offli.QuadPart);
}

static int
_tiffCloseProc(thandle_t fd)
{
	return (CloseHandle(fd) ? 0 : -1);
}

static uint64
_tiffSizeProc(thandle_t fd)
{
	ULARGE_INTEGER m;
	m.LowPart=GetFileSize(fd,&m.HighPart);
	return(m.QuadPart);
}

static int
_tiffDummyMapProc(thandle_t fd, void** pbase, toff_t* psize)
{
	(void) fd;
	(void) pbase;
	(void) psize;
	return (0);
}

/*
 * From "Hermann Josef Hill" <lhill@rhein-zeitung.de>:
 *
 * Windows uses both a handle and a pointer for file mapping,
 * but according to the SDK documentation and Richter's book
 * "Advanced Windows Programming" it is safe to free the handle
 * after obtaining the file mapping pointer
 *
 * This removes a nasty OS dependency and cures a problem
 * with Visual C++ 5.0
 */
static int
_tiffMapProc(thandle_t fd, void** pbase, toff_t* psize)
{
	uint64 size;
	tmsize_t sizem;
	HANDLE hMapFile;

	size = _tiffSizeProc(fd);
	sizem = (tmsize_t)size;
	if ((uint64)sizem!=size)
		return (0);

	/* By passing in 0 for the maximum file size, it specifies that we
	   create a file mapping object for the full file size. */
	hMapFile = CreateFileMapping(fd, NULL, PAGE_READONLY, 0, 0, NULL);
	if (hMapFile == NULL)
		return (0);
	*pbase = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(hMapFile);
	if (*pbase == NULL)
		return (0);
	*psize = size;
	return(1);
}

static void
_tiffDummyUnmapProc(thandle_t fd, void* base, toff_t size)
{
	(void) fd;
	(void) base;
	(void) size;
}

static void
_tiffUnmapProc(thandle_t fd, void* base, toff_t size)
{
	(void) fd;
	(void) size;
	UnmapViewOfFile(base);
}

