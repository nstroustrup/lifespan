/* $Id: tif_unix.c,v 1.12 2006/03/21 16:37:51 dron Exp $ */

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
 * TIFF Library UNIX-specific Routines. These are should also work with the
 * Windows Common RunTime Library.
 */

#include <sys/types.h>
#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include "tiffio.h"
#define HAVE_MMAP 1

static tsize_t
_tiffReadProc(thandle_t fd, tdata_t buf, tsize_t size)
{
	return ((tsize_t) read((int) fd, buf, (size_t) size));
}

static tsize_t
_tiffWriteProc(thandle_t fd, tdata_t buf, tsize_t size)
{
	return ((tsize_t) write((int) fd, buf, (size_t) size));
}

static toff_t
_tiffSeekProc(thandle_t fd, toff_t off, int whence)
{
	return ((toff_t) lseek((int) fd, (off_t) off, whence));
}

static int
_tiffCloseProc(thandle_t fd)
{
	return (close((int) fd));
}


static toff_t
_tiffSizeProc(thandle_t fd)
{
#ifdef _AM29K
	long fsize;
	return ((fsize = lseek((int) fd, 0, SEEK_END)) < 0 ? 0 : fsize);
#else
	struct stat sb;
	return (toff_t) (fstat((int) fd, &sb) < 0 ? 0 : sb.st_size);
#endif
}

#ifdef HAVE_MMAP
#include <sys/mman.h>

static int
_tiffMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize)
{
	toff_t size = _tiffSizeProc(fd);
	if (size != (toff_t) -1) {
		*pbase = (tdata_t)
		    mmap(0, size, PROT_READ, MAP_SHARED, (int) fd, 0);
		if (*pbase != (tdata_t) -1) {
			*psize = size;
			return (1);
		}
	}
	return (0);
}

static void
_tiffUnmapProc(thandle_t fd, tdata_t base, toff_t size)
{
	(void) fd;
	(void) munmap(base, (off_t) size);
}
#else /* !HAVE_MMAP */
static int
_tiffMapProc(thandle_t fd, tdata_t* pbase, toff_t* psize)
{
	(void) fd; (void) pbase; (void) psize;
	return (0);
}

static void
_tiffUnmapProc(thandle_t fd, tdata_t base, toff_t size)
{
	(void) fd; (void) base; (void) size;
}
#endif /* !HAVE_MMAP */
