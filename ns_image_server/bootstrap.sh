#!/bin/sh
rm -r -f compile config.guess config.sub depcomp missing install-sh INSTALL autom4te.cache aclocal.m4
aclocal
autoheader
automake  --force-missing --add-missing
autoconf