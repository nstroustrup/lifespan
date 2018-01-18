/* $Id: tif_unix.c,v 1.28 2017-01-11 19:02:49 erouault Exp $ */

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

#include "tif_config.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include <errno.h>

#include <stdarg.h>
#include <stdlib.h>
#include <sys/stat.h>

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_FCNTL_H
# include <fcntl.h>
#endif

#ifdef HAVE_IO_H
# include <io.h>
#endif

#include "tiffiop.h"


#define TIFF_IO_MAX 2147483647U


typedef union fd_as_handle_union
{
	int fd;
	thandle_t h;
} fd_as_handle_union_t;

static tmsize_t
_tiffReadProc(thandle_t fd, void* buf, tmsize_t size)
{
	fd_as_handle_union_t fdh;
        const size_t bytes_total = (size_t) size;
        size_t bytes_read;
        tmsize_t count = -1;
	if ((tmsize_t) bytes_total != size)
	{
		errno=EINVAL;
		return (tmsize_t) -1;
	}
	fdh.h = fd;
        for (bytes_read=0; bytes_read < bytes_total; bytes_read+=count)
        {
                char *buf_offset = (char *) buf+bytes_read;
                size_t io_size = bytes_total-bytes_read;
                if (io_size > TIFF_IO_MAX)
                        io_size = TIFF_IO_MAX;
                count=read(fdh.fd, buf_offset, (TIFFIOSize_t) io_size);
                if (count <= 0)
                        break;
        }
        if (count < 0)
                return (tmsize_t)-1;
        return (tmsize_t) bytes_read;
}

static tmsize_t
_tiffWriteProc(thandle_t fd, void* buf, tmsize_t size)
{
	fd_as_handle_union_t fdh;
	const size_t bytes_total = (size_t) size;
        size_t bytes_written;
        tmsize_t count = -1;
	if ((tmsize_t) bytes_total != size)
	{
		errno=EINVAL;
		return (tmsize_t) -1;
	}
	fdh.h = fd;
        for (bytes_written=0; bytes_written < bytes_total; bytes_written+=count)
        {
                const char *buf_offset = (char *) buf+bytes_written;
                size_t io_size = bytes_total-bytes_written;
                if (io_size > TIFF_IO_MAX)
                        io_size = TIFF_IO_MAX;
                count=write(fdh.fd, buf_offset, (TIFFIOSize_t) io_size);
                if (count <= 0)
                        break;
        }
        if (count < 0)
                return (tmsize_t)-1;
        return (tmsize_t) bytes_written;
	/* return ((tmsize_t) write(fdh.fd, buf, bytes_total)); */
}

static uint64
_tiffSeekProc(thandle_t fd, uint64 off, int whence)
{
	fd_as_handle_union_t fdh;
	_TIFF_off_t off_io = (_TIFF_off_t) off;
	if ((uint64) off_io != off)
	{
		errno=EINVAL;
		return (uint64) -1; /* this is really gross */
	}
	fdh.h = fd;
	return((uint64)_TIFF_lseek_f(fdh.fd,off_io,whence));
}

static int
_tiffCloseProc(thandle_t fd)
{
	fd_as_handle_union_t fdh;
	fdh.h = fd;
	return(close(fdh.fd));
}

static uint64
_tiffSizeProc(thandle_t fd)
{
	_TIFF_stat_s sb;
	fd_as_handle_union_t fdh;
	fdh.h = fd;
	if (_TIFF_fstat_f(fdh.fd,&sb)<0)
		return(0);
	else
		return((uint64)sb.st_size);
}

#ifdef HAVE_MMAP
#include <sys/mman.h>

static int
_tiffMapProc(thandle_t fd, void** pbase, toff_t* psize)
{
	uint64 size64 = _tiffSizeProc(fd);
	tmsize_t sizem = (tmsize_t)size64;
	if ((uint64)sizem==size64) {
		fd_as_handle_union_t fdh;
		fdh.h = fd;
		*pbase = (void*)
		    mmap(0, (size_t)sizem, PROT_READ, MAP_SHARED, fdh.fd, 0);
		if (*pbase != (void*) -1) {
			*psize = (tmsize_t)sizem;
			return (1);
		}
	}
	return (0);
}

static void
_tiffUnmapProc(thandle_t fd, void* base, toff_t size)
{
	(void) fd;
	(void) munmap(base, (off_t) size);
}
#else /* !HAVE_MMAP */
static int
_tiffMapProc(thandle_t fd, void** pbase, toff_t* psize)
{
	(void) fd; (void) pbase; (void) psize;
	return (0);
}

static void
_tiffUnmapProc(thandle_t fd, void* base, toff_t size)
{
	(void) fd; (void) base; (void) size;
}
#endif /* !HAVE_MMAP */
