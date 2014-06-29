/* $Id: tif_win32.c,v 1.18 2006/02/07 11:03:29 dron Exp $ */

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

#include "tiffio.h"

static tsize_t
_tiffReadProc(thandle_t fd, tdata_t buf, tsize_t size)
{
	DWORD dwSizeRead;
	if (!ReadFile(fd, buf, size, &dwSizeRead, NULL))
		return(0);
	return ((tsize_t) dwSizeRead);
}

static tsize_t
_tiffWriteProc(thandle_t fd, tdata_t buf, tsize_t size)
{
	DWORD dwSizeWritten;
	if (!WriteFile(fd, buf, size, &dwSizeWritten, NULL))
		return(0);
	return ((tsize_t) dwSizeWritten);
}

static toff_t
_tiffSeekProc(thandle_t fd, toff_t off, int whence)
{
	DWORD dwMoveMethod, dwMoveHigh;

        /* we use this as a special code, so avoid accepting it */
        if( off == 0xFFFFFFFF )
            return 0xFFFFFFFF;
        
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
        dwMoveHigh = 0;
	return ((toff_t)SetFilePointer(fd, (LONG) off, (PLONG)&dwMoveHigh,
                                       dwMoveMethod));
}

static int
_tiffCloseProc(thandle_t fd)
{
	return (CloseHandle(fd) ? 0 : -1);
}

static toff_t
_tiffSizeProc(thandle_t fd)
{
	return ((toff_t)GetFileSize(fd, NULL));
}

#ifdef __BORLANDC__
#pragma argsused
#endif
static int
_tiffDummyMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize)
{
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
_tiffMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize)
{
	toff_t size;
	HANDLE hMapFile;

	if ((size = _tiffSizeProc(fd)) == 0xFFFFFFFF)
		return (0);
	hMapFile = CreateFileMapping(fd, NULL, PAGE_READONLY, 0, size, NULL);
	if (hMapFile == NULL)
		return (0);
	*pbase = MapViewOfFile(hMapFile, FILE_MAP_READ, 0, 0, 0);
	CloseHandle(hMapFile);
	if (*pbase == NULL)
		return (0);
	*psize = size;
	return(1);
}

#ifdef __BORLANDC__
#pragma argsused
#endif
static void
_tiffDummyUnmapProc(thandle_t fd, tdata_t base, toff_t size)
{
}

static void
_tiffUnmapProc(thandle_t fd, tdata_t base, toff_t size)
{
	UnmapViewOfFile(base);
}