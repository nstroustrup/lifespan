##  Makefile.am -- an automake template for Makefile.in file
##  Copyright (C) 2013 Stéphane Voltz and Sane Developers.
##
##  This file is part of the "Sane" build infra-structure.  See
##  included LICENSE file for license information.

all:
	@echo "run 'make check' to run tests"

SANEDESC  = ../../tools/sane-desc$(EXEEXT)
TESTFILE  = $(srcdir)/data/testfile.desc
OUTFILES  = *.res
DEVICE    = test

EXTRA_DIST = data/testfile.desc data/ascii.ref data/db.ref data/hal-new.ref \
	     data/hal.ref data/html-backends-split.ref data/html-mfgs.ref \
	     data/hwdb.ref data/plist.ref data/statistics.ref \
	     data/udev+acl.ref data/udev+hwdb.ref data/udev.ref \
	     data/usermap.ref data/xml.ref
CLEANFILES = $(OUTFILES)

all: help

help:
	@echo "Use 'make check' to run the tests."

check: check.local

check.local: 
	@echo "**** Testing $(SANEDESC) with $(TESTFILE)"
	@for mode in ascii html-backends-split html-mfgs xml statistics usermap db udev udev+acl udev+hwdb hwdb plist hal hal-new; \
	do \
	  $(SANEDESC) -m $$mode -s $(srcdir)/data >$$mode.res ;\
	  if diff -I "[ 012][0-9]:[0-5][0-9]:[0-6][0-9] 20[0-9][0-9]" \
	          -I "sane-backends 1\.0\.[0-9]\+\(git\)\?$$" \
	       $(srcdir)/data/$$mode.ref $$mode.res ; \
	  then \
		echo "PASS: sane-desc -m $$mode -s $(srcdir)/data"; \
	  else \
		echo "FAIL: sane-desc -m $$mode -s $(srcdir)/data"; \
		exit 1 ;\
	  fi; \
	done ;\
	echo "================" ;\
	echo "All tests passed" ;\
	echo "================"
