AC_INIT([ns_image_server], [1.0])
AM_INIT_AUTOMAKE([foreign])
AM_CONFIG_HEADER([config.h])
AC_PROG_CXX
AC_CONFIG_FILES([Makefile])
AM_PROG_CC_C_O
AC_PROG_CC(gcc)
m4_include([m4/ax_lib_mysql.m4])
m4_include([m4/opj_check_lib.m4])
AX_LIB_MYSQL

CXXFLAGS="$CXXFLAGS -I/usr/local/include"
LDFLAGS="$LDFLAGS -L/usr/local/lib"

AC_SEARCH_LIBS([dmtxDecodeStructInit],[dmtx], [],AC_MSG_ERROR(Could not locate package dmtx.  Is it installed?),"-lm")
AC_SEARCH_LIBS(TIFFOpen,tiff,[],[AC_MSG_ERROR(Could not locate package libtiff.  Is it installed?,[1])] )
AC_SEARCH_LIBS(jpeg_destroy_decompress, jpeg, [], [AC_MSG_ERROR(Could not locate package libjpeg.  Is it installed?,[1])],"-lm")
AC_SEARCH_LIBS([FT_Load_Glyph], [freetype], [], AC_MSG_ERROR([Could not locate package FreeType.  Is it installed?],[1]),"-lm")
AC_SEARCH_LIBS([pthread_create], [pthread], AC_MSG_ERROR([Could not locate package pthread.  Is it installed?],[1]))
AC_SEARCH_LIBS(opj_setup_decoder, [openjpeg], [],AC_MSG_ERROR([Could not locate package openjpeg.  Is it installed?],[1]),"-lm")
AC_SEARCH_LIBS(xvid_encore,xvidcore, [],AC_MSG_ERROR([Could not locate package xvid.  Is it installed?],[1]),"-lm")

NS_USB_1 = 0
AC_SEARCH_LIBS(libusb_init,libusb-1.0, [],NS_USB_1=1,"-lm")
if test $NS_USB_1 == 0
then
AC_SEARCH_LIBS(usb_init,libusb, [],AC_MSG_ERROR([Could not either libusb or libusb-1.0.  Are either installed?],[1]),"-lm")
fi

AC_OUTPUT