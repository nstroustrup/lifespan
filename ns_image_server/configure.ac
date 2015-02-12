AC_INIT([ns_image_server], [1.0])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])
AC_PROG_CXX
AC_CONFIG_FILES([Makefile])
AC_PROG_CC(gcc)
AM_PROG_CC_C_O
m4_include([m4/ax_lib_mysql.m4])
#m4_include([m4/opj_check_lib.m4])
#m4_include([m4/ax_pthread.m4])
AX_LIB_MYSQL

# setting CXXFLAGS explicitly in configure.ac is strongly discouraged by autoconf
# http://stackoverflow.com/questions/323790/autotools-how-to-set-global-compilation-flag
# instead we set a custom var that gets passed to Makefile.am
# http://stackoverflow.com/questions/20230827/how-to-set-include-paths-with-autotools
# 
NS_CXXFLAGS="-I/usr/local/include"
NS_LDFLAGS="-L/usr/local/lib"

#
# Platform specific setup
#
#############################
AC_CANONICAL_HOST
# Check for which host we are on and setup a few things
# specifically based on the host
case $host_os in
  darwin* )
        # no need to look for clock_gettime -- we use the mach kernel's timer
        target_osx=yes
        ;;
    *)
        # default
        AC_SEARCH_LIBS(clock_gettime,rt, [],AC_MSG_ERROR(Could not locate package rt.  Is it installed?),"-lrt")
        NS_LDFLAGS="$NS_LDFLAGS -Wl,-rpath=/usr/local/lib"
        target_osx=no
        ;;
esac
AM_CONDITIONAL([TARGET_OSX],[test $target_osx = yes])



AC_SEARCH_LIBS(cos,m, [],AC_MSG_ERROR(Could not locate package m.  Is it installed?),"-lm")
AC_SEARCH_LIBS(dmtxDecodeCreate,dmtx, [],AC_MSG_ERROR(Could not locate package dmtx.  Is it installed?))
AC_SEARCH_LIBS(TIFFOpen,tiff,[],[AC_MSG_ERROR(Could not locate package libtiff.  Is it installed?,[1])] )
AC_SEARCH_LIBS(jpeg_destroy_decompress, jpeg, [], [AC_MSG_ERROR(Could not locate package libjpeg.  Is it installed?,[1])])
AC_SEARCH_LIBS([FT_Load_Glyph], [freetype], [], AC_MSG_ERROR([Could not locate package FreeType.  Is it installed?],[1]))
#AX_PTHREAD([],AC_MSG_ERROR([Could not locate package pthread.  Is it installed?] ))
#AC_SEARCH_LIBS([pthread_create], [pthread], AC_MSG_ERROR([Could not locate package pthread.  Is it installed?],[1]))
AC_SEARCH_LIBS(opj_setup_decoder, [openjpeg], [],AC_MSG_ERROR([Could not locate package openjpeg.  Is it installed?],[1]))
AC_SEARCH_LIBS(xvid_encore,xvidcore, [],AC_MSG_ERROR([Could not locate package xvid.  Is it installed?],[1]))
AC_SEARCH_LIBS([zlibVersion], [z], [],AC_MSG_ERROR([Could not locate package zlib.  Is it installed?],[1]))

AC_SEARCH_LIBS(libusb_init,[usb-1.0], NS_USB_1=1,NS_USB_1=0)
if test "$NS_USB_1" = "0"
then
AC_SEARCH_LIBS(usb_init,usb, [],AC_MSG_ERROR([Could not either libusb or libusb-1.0.  Are either installed?],[1]))
AC_DEFINE(NS_USB_1,0,[Use usb-1.0 library instead of usb-0.1])
else
AC_DEFINE(NS_USB_1,1,[Use usb-1.0 library instead of usb-0.1])
fi

# worm browser build and fltk path 
AC_ARG_ENABLE([browser],[AS_HELP_STRING([--enable-browser[=path-to-fltk]], [build worm browser (requires FLTK libraries, which path can be optionally specified if nonstandard)])], [], [enable_browser=no])

case "_$enable_browser" in
	_no)
	build_browser=no
	;;

	_|_yes)
	build_browser=yes
	AC_PATH_PROG([FLTK_CONFIG], [fltk-config])
	;;

	*)
	build_browser=yes
	AC_PATH_PROG([FLTK_CONFIG], [fltk-config], ,[$enable_browser/bin])
	;;
esac

FLTK_LDFLAGS=
FLTK_INCLUDEDIR=

have_fltk=no

if test -n "$FLTK_CONFIG"
then
	have_fltk=yes
	FLTK_LDFLAGS=`$FLTK_CONFIG --use-gl --use-images --ldflags`
	FLTK_INCLUDEDIR=`$FLTK_CONFIG --includedir`
fi

if  test "$have_fltk" = no -a "$build_browser" = yes
then
        AC_MSG_ERROR([Cannot find FLTK to build worm browser])
fi
AM_CONDITIONAL([BUILD_BROWSER], [test $build_browser = yes])
AC_SUBST([FLTK_CONFIG])
AC_SUBST([FLTK_LDFLAGS])
AC_SUBST([FLTK_INCLUDEDIR])


AC_SUBST(NS_LDFLAGS)
AC_SUBST(NS_CXXFLAGS)

AC_OUTPUT
