##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.
AM_CPPFLAGS += -I. -I$(top_builddir)/include -I$(top_srcdir)/include

noinst_LTLIBRARIES = liblib.la

liblib_la_SOURCES  = md5.c
liblib_la_CPPFLAGS = -I$(top_srcdir)/include
liblib_la_LIBADD   = $(LTALLOCA) $(LTLIBOBJS)
