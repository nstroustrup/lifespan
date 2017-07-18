/*
 * epsonds-net.c - SANE library for Epson scanners.
 *
 * Copyright (C) 2006-2016 Tower Technologies
 * Author: Alessandro Zummo <a.zummo@towertech.it>
 *
 * This file is part of the SANE package.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation, version 2.
 */

#define DEBUG_DECLARE_ONLY

#include "sane/config.h"

#ifdef HAVE_SYS_SELECT_H
#include <sys/select.h>
#endif

#include "sane/sane.h"
#include "sane/saneopts.h"
#include "sane/sanei_tcp.h"
#include "sane/sanei_config.h"
#include "sane/sanei_backend.h"

#include "epsonds.h"
#include "epsonds-net.h"

#include "byteorder.h"

#include "sane/sanei_debug.h"

static int
epsonds_net_read_raw(epsonds_scanner *s, unsigned char *buf, ssize_t wanted,
		       SANE_Status *status)
{
	int ready, read = -1;
	fd_set readable;
	struct timeval tv;

	tv.tv_sec = 10;
	tv.tv_usec = 0;

	FD_ZERO(&readable);
	FD_SET(s->fd, &readable);

	ready = select(s->fd + 1, &readable, NULL, NULL, &tv);
	if (ready > 0) {
		read = sanei_tcp_read(s->fd, buf, wanted);
	} else {
		DBG(15, "%s: select failed: %d\n", __func__, ready);
	}

	*status = SANE_STATUS_GOOD;

	if (read < wanted) {
		*status = SANE_STATUS_IO_ERROR;
	}

	return read;
}

int
epsonds_net_read(epsonds_scanner *s, unsigned char *buf, ssize_t wanted,
		       SANE_Status * status)
{
	ssize_t size;
	ssize_t read = 0;
	unsigned char header[12];

	/* read from buffer, if available */
	if (wanted && s->netptr != s->netbuf) {
		DBG(23, "reading %lu from buffer at %p, %lu available\n",
			(u_long) wanted, s->netptr, (u_long) s->netlen);

		memcpy(buf, s->netptr, wanted);
		read = wanted;

		s->netlen -= wanted;

		if (s->netlen == 0) {
			DBG(23, "%s: freeing %p\n", __func__, s->netbuf);
			free(s->netbuf);
			s->netbuf = s->netptr = NULL;
			s->netlen = 0;
		}

		return read;
	}

	/* receive net header */
	size = epsonds_net_read_raw(s, header, 12, status);
	if (size != 12) {
		return 0;
	}

	if (header[0] != 'I' || header[1] != 'S') {
		DBG(1, "header mismatch: %02X %02x\n", header[0], header[1]);
		*status = SANE_STATUS_IO_ERROR;
		return 0;
	}

	// incoming payload size
	size = be32atoh(&header[6]);

	DBG(23, "%s: wanted = %lu, available = %lu\n", __func__,
		(u_long) wanted, (u_long) size);

	*status = SANE_STATUS_GOOD;

	if (size == wanted) {

		DBG(15, "%s: full read\n", __func__);

		if (size) {
			read = epsonds_net_read_raw(s, buf, size, status);
		}

		if (s->netbuf) {
			free(s->netbuf);
			s->netbuf = NULL;
			s->netlen = 0;
		}

		if (read < 0) {
			return 0;
		}

	} else if (wanted < size) {

		DBG(23, "%s: long tail\n", __func__);

		read = epsonds_net_read_raw(s, s->netbuf, size, status);
		if (read != size) {
			return 0;
		}

		memcpy(buf, s->netbuf, wanted);
		read = wanted;

		free(s->netbuf);
		s->netbuf = NULL;
		s->netlen = 0;

	} else {

		DBG(23, "%s: partial read\n", __func__);

		read = epsonds_net_read_raw(s, s->netbuf, size, status);
		if (read != size) {
			return 0;
		}

		s->netlen = size - wanted;
		s->netptr += wanted;
		read = wanted;

		DBG(23, "0,4 %02x %02x\n", s->netbuf[0], s->netbuf[4]);
		DBG(23, "storing %lu to buffer at %p, next read at %p, %lu bytes left\n",
			(u_long) size, s->netbuf, s->netptr, (u_long) s->netlen);

		memcpy(buf, s->netbuf, wanted);
	}

	return read;
}

SANE_Status
epsonds_net_request_read(epsonds_scanner *s, size_t len)
{
	SANE_Status status;
	epsonds_net_write(s, 0x2000, NULL, 0, len, &status);
	return status;
}

int
epsonds_net_write(epsonds_scanner *s, unsigned int cmd, const unsigned char *buf,
			size_t buf_size, size_t reply_len, SANE_Status *status)
{
	unsigned char *h1, *h2;
	unsigned char *packet = malloc(12 + 8);

	/* XXX check allocation failure */

	h1 = packet;		// packet header
	h2 = packet + 12;	// data header

	if (reply_len) {
		s->netbuf = s->netptr = malloc(reply_len);
		s->netlen = reply_len;
		DBG(24, "allocated %lu bytes at %p\n",
			(u_long) reply_len, s->netbuf);
	}

	DBG(24, "%s: cmd = %04x, buf = %p, buf_size = %lu, reply_len = %lu\n",
		__func__, cmd, buf, (u_long) buf_size, (u_long) reply_len);

	memset(h1, 0x00, 12);
	memset(h2, 0x00, 8);

	h1[0] = 'I';
	h1[1] = 'S';

	h1[2] = cmd >> 8;	// packet type
	h1[3] = cmd;		// data type

	h1[4] = 0x00;
	h1[5] = 0x0C; // data offset

	DBG(24, "H1[0]: %02x %02x %02x %02x\n", h1[0], h1[1], h1[2], h1[3]);

	// 0x20 passthru
	// 0x21 job control

	if (buf_size) {
		htobe32a(&h1[6], buf_size);
	}

	if((cmd >> 8) == 0x20) {

		htobe32a(&h1[6], buf_size + 8);	// data size (data header + payload)

		htobe32a(&h2[0], buf_size);	// payload size
		htobe32a(&h2[4], reply_len);	// expected answer size

		DBG(24, "H1[6]: %02x %02x %02x %02x (%lu)\n", h1[6], h1[7], h1[8], h1[9], (u_long) (buf_size + 8));
		DBG(24, "H2[0]: %02x %02x %02x %02x (%lu)\n", h2[0], h2[1], h2[2], h2[3], (u_long) buf_size);
		DBG(24, "H2[4]: %02x %02x %02x %02x (%lu)\n", h2[4], h2[5], h2[6], h2[7], (u_long) reply_len);
	}

	if ((cmd >> 8) == 0x20 && (buf_size || reply_len)) {

		// send header + data header
		sanei_tcp_write(s->fd, packet, 12 + 8);

	} else {
		sanei_tcp_write(s->fd, packet, 12);
	}

	// send payload
	if (buf_size)
		sanei_tcp_write(s->fd, buf, buf_size);

	free(packet);

	*status = SANE_STATUS_GOOD;
	return buf_size;
}

SANE_Status
epsonds_net_lock(struct epsonds_scanner *s)
{
	SANE_Status status;
	unsigned char buf[7] = "\x01\xa0\x04\x00\x00\x01\x2c";

	DBG(1, "%s\n", __func__);

	epsonds_net_write(s, 0x2100, buf, 7, 0, &status);
	epsonds_net_read(s, buf, 1, &status);

	// buf[0] should be ACK, 0x06

	return status;
}

SANE_Status
epsonds_net_unlock(struct epsonds_scanner *s)
{
	SANE_Status status;

	DBG(1, "%s\n", __func__);

	epsonds_net_write(s, 0x2101, NULL, 0, 0, &status);
/*	epsonds_net_read(s, buf, 1, &status); */
	return status;
}
