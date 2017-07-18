##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2009 Chris Bagwell and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

#
#	Makefile for a Java project with native methods.
#

JAVAROOT=.
MY_CLASSPATH=.
JAVAH=javah -jni -cp $(MY_CLASSPATH)

#	We'll need something in 'configure' for the Java includes.
AM_CPPFLAGS += -I. -I$(srcdir) -I$(top_builddir)/include \
	      -I$(top_srcdir)/include -I${JDK_HOME}/include \
	      -I${JDK_HOME}/include/genunix
AM_LDFLAGS += $(STRICT_LDFLAGS) -version-number $(V_MAJOR):$(V_MINOR):$(V_REV)

JAVA_SRC = SaneDevice.java SaneOption.java SaneRange.java\
	   Sane.java SaneParameters.java ScanIt.java \
	   ImageCanvas.java ImageCanvasClient.java \
	   Test.java Jscanimage.java 

dist_noinst_JAVA = $(JAVA_SRC)

LIBSANE = ../backend/libsane.la

EXTRA_DIST = README.JAVA

lib_LTLIBRARIES = libsanej.la

BUILT_SOURCES = Sane.h
CLEANFILES = Sane.h
nodist_libsanej_la_SOURCES = Sane.h
libsanej_la_SOURCES = Sane.c
libsanej_la_LIBADD = $(LIBSANE)

# Make sure that java classes get created before create Sane.h
Sane.h: classdist_noinst.stamp
	$(JAVAH) Sane

