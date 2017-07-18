dnl Process this file with autoconf to produce a configure script.

dnl ******************************************************************
dnl  Set up autoconf and automake
dnl  When preparing a release, modify the numeric version components
dnl  and remove the git suffix.
dnl ******************************************************************
AC_INIT([sane-backends],[1.0.27],
        [sane-devel@lists.alioth.debian.org])
AC_PREREQ(2.69)                 dnl minimum autoconf version required
AC_CONFIG_MACRO_DIR([m4])
AC_CONFIG_HEADERS([include/sane/config.h])

AM_INIT_AUTOMAKE([1.11.6 subdir-objects -Wall])
AM_SILENT_RULES
dnl Turn off feature were automake will automatically run autoreconf.
dnl This is because we used to check in generated files into CVS which
dnl has known timestamp issues.
AM_MAINTAINER_MODE

dnl ******************************************************************
dnl  Determine SANE version components and release status
dnl ******************************************************************
AS_IF([test x = "x$AWK"],[AC_PROG_AWK])
AS_IF([test x = "x`echo $VERSION | sed 's/[[.0-9]]//g'`"],
            [is_release=yes],
            [is_release=no])
V_MAJOR=`echo $VERSION | $AWK -F. '{print $1}'`
V_MINOR=`echo $VERSION | $AWK -F. '{print $2}'`
V_REV=`echo $VERSION | $AWK -F. '{print $3}' | sed 's/[[^0-9]]//g'`;

AC_DEFINE_UNQUOTED(SANE_DLL_V_MAJOR, $V_MAJOR, [SANE DLL major number])
AC_DEFINE_UNQUOTED(SANE_DLL_V_MINOR, $V_MINOR, [SANE DLL minor number])
AC_DEFINE_UNQUOTED(SANE_DLL_V_BUILD, $V_REV, [SANE DLL revision number])
AC_SUBST(V_MAJOR)
AC_SUBST(V_MINOR)
AC_SUBST(V_REV)

dnl ******************************************************************
dnl  Set up the compiler and linker
dnl ******************************************************************
AC_PROG_CC
AM_PROG_CC_C_O
sane_save_CC=$CC
AC_PROG_CC_C99                  dnl enables extensions to ISO C99 :-(
AS_IF([test xno != "x$ac_cv_prog_cc_c99"],
      [AC_MSG_CHECKING([for $sane_save_CC option for ISO C99 w/o extensions])
       AS_CASE([$ac_cv_prog_cc_c99],
               [-std=gnu99],      [sane_prog_cc_c99="-std=c99"],
               [-qlanglvl=extc99],[sane_prog_cc_c99="-qlanglvl=stdc99"])
       AS_IF([test "x$ac_cv_prog_cc_c99" = "x$sane_prog_cc_c99"],
             [AC_MSG_RESULT([$ac_cv_prog_cc_99])],
             [AC_MSG_RESULT([$sane_prog_cc_c99])
              CC="$sane_save_CC $sane_prog_cc_c99"])
      ])
AC_PROG_GCC_TRADITIONAL
AC_USE_SYSTEM_EXTENSIONS        dnl call before running the C compiler

AM_PROG_AR
LT_INIT([disable-static win32-dll])
LT_PREREQ([2.4.2])

dnl *****************************************************************
dnl  Set up I18N/L10N support
dnl *****************************************************************
AM_GNU_GETTEXT([external])
AM_GNU_GETTEXT_VERSION([0.18.1])

dnl ***********************************************************************
dnl Checks for programs.
dnl ***********************************************************************
AC_PATH_PROG(SANE_CONFIG_PATH, sane-config, no)

dnl Call explicitely before using PKG_*
PKG_PROG_PKG_CONFIG

AM_CONDITIONAL(CROSS_COMPILING, test x$cross_compiling = xyes)

dnl ***********************************************************************
dnl set compiler/linker flags
dnl ***********************************************************************
SANE_SET_AM_CFLAGS([$is_release])
SANE_SET_AM_LDFLAGS

dnl ***********************************************************************
dnl Checks for unix variants
dnl ***********************************************************************
AC_SEARCH_LIBS([strerror],[cposix])

AC_NEED_BYTEORDER_H([include/byteorder.h])
AX_CREATE_STDINT_H([include/_stdint.h])

dnl ***********************************************************************
dnl Checks for libraries
dnl ***********************************************************************
AC_ARG_ENABLE(dynamic,
	AS_HELP_STRING([--disable-dynamic],
		       [Disable dynamic loading of backends]),
	[enable_dynamic=$enableval], [enable_dynamic=auto])
SANE_CHECK_DLL_LIB
dnl Checks for Backend libraries.
AC_CHECK_LIB(m, sqrt, MATH_LIB="-lm")
AC_SUBST(MATH_LIB)
case ${host_os} in
  os2*)
  AC_CHECK_LIB(syslog, sylog, SYSLOG_LIBS="-lsyslog")
  ;;
  beos*)
  AC_CHECK_LIB(be, syslog, SYSLOG_LIBS="-lbe")
  ;;
esac
AC_SUBST(SYSLOG_LIBS)

SANE_CHECK_JPEG
SANE_CHECK_TIFF
SANE_CHECK_PNG
SANE_CHECK_IEEE1284
SANE_CHECK_PTHREAD
SANE_CHECK_LOCKING
SANE_CHECK_GPHOTO2


AC_ARG_WITH(v4l,
	    AS_HELP_STRING([--with-v4l],
			   [include the v4l backend @<:@default=yes@:>@]),
	    [# If --with-v4l=no or --without-v4l, disable backend
             # as "$with_v4l" will be set to "no"])

if test "$with_v4l" != "no" ; then
  PKG_CHECK_MODULES(LIBV4L, libv4l1, have_libv4l1=yes, have_libv4l1=no)
fi

AC_ARG_ENABLE(avahi,
	AS_HELP_STRING([--enable-avahi], [enable Avahi support for saned and the net backend]),
	[enable_avahi=$enableval], [enable_avahi=no])

if test "$enable_avahi" = "yes"; then
  PKG_CHECK_MODULES(AVAHI, [ avahi-client >= 0.6.24 ],
                    [AC_DEFINE(WITH_AVAHI, 1, [define if Avahi support is enabled for saned and the net backend])], enable_avahi=no)
fi

dnl check sane to make sure we don't have two installations
AC_CHECK_LIB(sane, sane_init, LIBSANE_EXISTS="yes")


dnl **************************************************************
dnl SNMP CHECKS
dnl **************************************************************

AC_ARG_WITH(snmp,
	AS_HELP_STRING([--with-snmp], [enable SNMP support @<:@default=yes@:>@]))

if test "$with_snmp" = "no"; then
	echo "Not including SNMP support"
else
	AC_PATH_PROG(SNMP_CONFIG_PATH, net-snmp-config, "no")
	AC_CHECK_LIB([netsnmp], [snmp_timeout], LIBSNMP_EXISTS="yes")
	AC_MSG_CHECKING(for proper SNMP version)
	if test "$SNMP_CONFIG_PATH" != "no" ; then
		snmp_version=`$SNMP_CONFIG_PATH --version 2>/dev/null`
		vers=`echo $snmp_version | awk 'BEGIN { FS = "."; } { printf "%d", ($1 * 1000 + $2);}'`
		if test -n "$vers" && test "$vers" -ge 5006; then
			SNMP_LIBS=`$SNMP_CONFIG_PATH --libs`
			SNMP_CFLAGS=`$SNMP_CONFIG_PATH --cflags`
			AC_MSG_RESULT(yes)
			with_snmp="yes"
		else
			AC_MSG_RESULT(no)
			AC_MSG_WARN([You need at least net-snmp 5.6, your version is $snmp_version])
			with_snmp="no"
		fi
	else
		with_snmp="no"
		AC_MSG_RESULT(no)
	fi
fi

if test "$with_snmp" = "yes" && test "${LIBSNMP_EXISTS}x" = "yesx"; then
	AC_SUBST(SNMP_LIBS)
	AC_SUBST(SNMP_CFLAGS)
	AC_DEFINE(HAVE_LIBSNMP, 1, [Define to 1 if you have the net-snmp library.])
else
	AC_MSG_WARN([net-snmp library disabled, autodetecting network scanners will not be supported.])
fi





dnl ***********************************************************************
dnl Checks for header files.
dnl ***********************************************************************
AC_HEADER_STDC
AC_CHECK_HEADERS(fcntl.h unistd.h libc.h sys/dsreq.h sys/select.h \
    sys/time.h sys/shm.h sys/ipc.h sys/signal.h sys/scanio.h os2.h \
    sys/socket.h sys/io.h sys/hw.h sys/types.h linux/ppdev.h \
    dev/ppbus/ppi.h machine/cpufunc.h sys/sem.h sys/poll.h \
    windows.h be/kernel/OS.h limits.h sys/ioctl.h asm/types.h\
    netinet/in.h tiffio.h ifaddrs.h pwd.h getopt.h)
AC_CHECK_HEADERS([asm/io.h],,,[#include <sys/types.h>])

SANE_CHECK_MISSING_HEADERS

AC_CHECK_HEADERS(winsock2.h, SOCKET_LIB="-lws2_32")

AC_CHECK_HEADER(resmgr.h,[
	AC_CHECK_LIB(
		resmgr,
		rsm_open_device,[
			AC_DEFINE(HAVE_RESMGR,1,[define if you have the resmgr library])
			RESMGR_LIBS="-lresmgr"
		]
	)
])
AC_SUBST(RESMGR_LIBS)

dnl ***********************************************************************
dnl Checks for types and structures
dnl ***********************************************************************

AC_TYPE_SIZE_T
AC_TYPE_PID_T
AC_TYPE_SSIZE_T
SANE_CHECK_U_TYPES

# from Python, check for "long long" type
AC_MSG_CHECKING(for long long support)
have_long_long=no
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[]], [[long long x; x = (long long)0;]])],[AC_DEFINE(HAVE_LONG_LONG, 1, Define if the long long type is available.) have_long_long=yes],[])
AC_MSG_RESULT($have_long_long)

AC_MSG_CHECKING([for socklen_t in <sys/socket.h>])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/socket.h>
]], [[socklen_t len]])],AC_MSG_RESULT(yes),
[AC_MSG_RESULT(no); AC_DEFINE(socklen_t,int,
[Define socklen_t as \'int\' if necessary.])])

AC_MSG_CHECKING([for union semun in <sys/sem.h>])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
]], [[union semun test_semun]])],[AC_MSG_RESULT(yes);
AC_DEFINE(HAVE_UNION_SEMUN,1,[Define if union semun is available.])],
[AC_MSG_RESULT(no)])

AC_MSG_CHECKING([for struct flock in fcntl.h])
AC_EGREP_HEADER([struct flock], fcntl.h, [AC_MSG_RESULT(yes) ;
                AC_DEFINE(HAVE_STRUCT_FLOCK, 1,
                [Define if struct flock is available.])], AC_MSG_RESULT(no))

AC_MSG_CHECKING([for Linux ioctl defines])
AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/ioctl.h>
#include <asm/types.h>
]],[[
__u32 houba = _IOR('v',14, unsigned long);
]])],[AC_MSG_RESULT(yes);
have_linux_ioctl_defines="yes";],AC_MSG_RESULT(no))

dnl ***********************************************************************
dnl Checks for compiler characteristics
dnl ***********************************************************************
AC_C_BIGENDIAN
AC_C_CONST
AC_C_INLINE

dnl ***********************************************************************
dnl Checks for library functions
dnl ***********************************************************************

dnl Functions for which we provide an implementation if missing
AC_CONFIG_LIBOBJ_DIR([lib])
AC_FUNC_ALLOCA
AC_REPLACE_FUNCS([getenv inet_ntop inet_pton isfdtype sigprocmask \
    sleep snprintf strcasestr strdup strndup strsep syslog usleep \
    vsyslog])
AS_IF([test x != x$ALLOCA],
      [LTALLOCA=`echo "$ALLOCA" | sed 's/\.o$//; s/\.obj$//'`.lo])
AC_SUBST(LTALLOCA)

dnl Define SOCKET_LIB, NSL_LIB, BIND_LIB, and RESOLV_LIB  when required 
dnl for functions we use.
AC_CHECK_FUNC(gethostbyaddr,, [AC_CHECK_LIB(bind, gethostbyaddr, BIND_LIB="-lbind")])
AC_CHECK_FUNC(gethostbyaddr,, [AC_CHECK_LIB(nsl, gethostbyaddr, NSL_LIB="-lnsl")])
AC_CHECK_FUNC(socket,, [AC_CHECK_LIB(socket, socket, SOCKET_LIB="-lsocket")])
AC_CHECK_FUNC(inet_aton,, [AC_CHECK_LIB(resolv, inet_aton, RESOLV_LIB="-lresolv")])

dnl Group related network libraries together so they can always be linked
dnl in.
SOCKET_LIBS="$SOCKET_LIB $NSL_LIB $BIND_LIB $RESOLV_LIB"
AC_SUBST(SOCKET_LIBS)

dnl define HAVE_* values for network functions.  This may require
dnl SOCKET_LIBS so set LIBS temporarily.
save_LIBS="$LIBS"
LIBS="$LIBS $SOCKET_LIBS"
AC_CHECK_FUNCS(inet_addr inet_aton inet_ntoa)
LIBS="$save_LIBS"

if test "$ac_cv_header_be_kernel_OS_h" = "yes" ; then
    dnl those are known to be broken in BeOS (BONE)
    ac_cv_func_getaddrinfo=no
    ac_cv_func_getnameinfo=no
fi

AC_FUNC_MMAP
AC_CHECK_FUNCS(atexit ioperm i386_set_ioperm \
    mkdir strftime strstr strtod  \
    cfmakeraw tcsendbreak strcasecmp strncasecmp _portaccess \
    getaddrinfo getnameinfo poll setitimer iopl getuid getpass)

dnl sys/io.h might provide ioperm but not inb,outb (like for
dnl non i386/x32/x86_64 with musl libc)
if test "${ac_cv_header_sys_io_h}" = "yes"; then
  AC_MSG_CHECKING([for inb,outb (provided by sys/io.h)])
  AC_LINK_IFELSE(
    [AC_LANG_PROGRAM([[#include <sys/io.h>]],
      [[inb(0);outb(0,0);]])],
    [AC_MSG_RESULT([yes])
      sane_cv_have_sys_io_h_with_inb_outb="yes"],
    [AC_MSG_RESULT([no])
      sane_cv_have_sys_io_h_with_inb_outb="no"
      AC_MSG_WARN([sys/io.h does not provide inb,outb (non i386/x32/x86_64 arch?)])])
  if test "$sane_cv_have_sys_io_h_with_inb_outb" = "yes"; then
    AC_DEFINE(SANE_HAVE_SYS_IO_H_WITH_INB_OUTB, 1, [Define to 1 if you have the <sys/io.h> providing inb,outb.])
  fi
else
  sane_cv_have_sys_io_h_with_inb_outb="no"
fi

SANE_PROTOTYPES

if test "$ac_cv_header_os2_h" = "yes" ; then
  AC_DEFINE(strncasecmp, strnicmp, [Define for OS/2 only])
  AC_DEFINE(strcasecmp, stricmp, [Define for OS/2 only])
fi

if test "$ac_cv_header_getopt_h" = "yes" ; then
  AC_CHECK_FUNCS(getopt_long)
fi

# Slightly abuse the AC_LIBOBJ macro to mark files as replacement code
AS_IF([test x$ac_cv_header_getopt_h != xyes \
       && test x$ac_cv_func_getopt_long != xyes],
      [AC_LIBOBJ(getopt)
       AC_LIBOBJ(getopt1)])

dnl ***********************************************************************
dnl checks for system services
dnl ***********************************************************************
if test -c /dev/urandom ; then
    AC_DEFINE(HAVE_DEV_URANDOM, 1, [Is /dev/urandom available?])
fi

AC_ARG_WITH(systemd,
	AS_HELP_STRING([--with-systemd], [enable systemd support @<:@default=yes@:>@]))
if test "x$with_systemd" != xno ; then
  PKG_CHECK_MODULES(SYSTEMD, [libsystemd], have_systemd=yes, have_systemd=no)
  if test "x$have_systemd" = xno; then
    PKG_CHECK_MODULES(SYSTEMD, [libsystemd-daemon], have_systemd=yes, have_systemd=no)
  fi
  if test "x$have_systemd" = xyes; then
    AC_DEFINE(HAVE_SYSTEMD, 1, [Is /usr/include/systemd/sd-daemon.h available?])
  else
    if test "x$with_systemd" = xyes; then
      AC_MSG_ERROR([Systemd support was requested but systemd was not found])
    fi
  fi
fi

dnl ******************************************************************
dnl Check for USB support
dnl ******************************************************************

AC_ARG_WITH(usb,
  AS_HELP_STRING([--with-usb], [enable USB support @<:@default=check@:>@]),
  [],
  [with_usb=check])
AS_IF([test xno != "x$with_usb"],
  [have_usb=no
   AS_CASE(x$host_os,           dnl odd-ball operating systems first
    [beos*],
      [AC_CHECK_HEADER(be/drivers/USB_scanner.h, [have_usb=yes])],
    [os2*],
      [AC_CHECK_HEADER(usbcalls.h,
        [AC_CHECK_LIB(usbcall, UsbQueryNumberDevices,
          [USB_LIBS="-lusbcall"
           have_usb=yes
           AC_DEFINE(HAVE_USBCALLS, [1],
             [Define to 1 if you have usbcall.dll.])
          ])
        ],
        [],
        [#include <usb.h>
         #include <os2.h>
        ])
      ],
    [dnl default to libusb-1.x, fall back to libusb-0.x if missing
     PKG_CHECK_MODULES([USB], [libusb-1.0],
      [AC_DEFINE([HAVE_LIBUSB], [1],
        [Define to 1 if you have libusb-1.0])
       have_usb=yes
      ],
      [PKG_CHECK_MODULES([USB], [libusb >= 0.1.8],
        [AC_DEFINE([HAVE_LIBUSB_LEGACY], [1],
          [Define to 1 if you have libusb-0.1])
         have_usb=yes
        ],
        [dnl 10+ years old libusb or Windows version
         AC_CHECK_HEADER(usb.h,
          AC_CHECK_LIB(usb, usb_interrupt_read,
            [USB_LIBS="-lusb"
             have_usb=yes
            ]))
         AC_CHECK_HEADERS(lusb0_usb.h,
           AC_CHECK_LIB(usb, usb_interrupt_read,
             [USB_LIBS="-lusb"
              have_usb=yes
             ]))
        ])
      ])
    ])
  ])
AS_IF([test xyes = "x$with_usb" && test xyes != "x$have_usb"],
  [AC_MSG_ERROR([USB support requested but required libraries not found.])
  ])
AM_CONDITIONAL([have_usblib], [test x != "x$USB_LIBS"])

dnl ************
dnl SCSI Support
dnl ************

dnl FIXME: These are a lot of header files to scan.  We should
dnl scan for just one that is unique per platform and then do
dnl conditional scans for more specific only as needed.

# Unset VERSION during the SCSI header check
sed "s!^#define VERSION .*!/* & */!" confdefs.h > confdefs.h.tmp
mv confdefs.h.tmp confdefs.h
 
AC_CHECK_HEADERS(IOKit/scsi/SCSITaskLib.h IOKit/cdb/IOSCSILib.h \
  IOKit/scsi/SCSICommandOperationCodes.h \
  IOKit/scsi-commands/SCSICommandOperationCodes.h scsi.h sys/scsi.h \
  sys/scsicmd.h sys/scsiio.h bsd/dev/scsireg.h scsi/sg.h \
  camlib.h gscdds.h sys/scsi/scsi.h sys/scsi/sgdefs.h \
  sys/scsi/targets/scgio.h apollo/scsi.h sys/sdi_comm.h \
  sys/passthrudef.h)

# Restore VERSION
sed "s!/\* \(#define VERSION .*\) \*/!\1!" confdefs.h > confdefs.h.tmp
mv confdefs.h.tmp confdefs.h

AC_CHECK_HEADERS([io/cam/cam.h],,,[#include <io/common/iotypes.h>])
AC_CHECK_HEADERS([ntddscsi.h ddk/ntddscsi.h],,,[#include <windows.h>])

dnl FreeBSD < 3
if test "$ac_cv_header_sys_scsiio_h" = "yes" \
  && test "$ac_cv_header_scsi_h" = "yes"; then
  AC_MSG_CHECKING([if 'scsireq_t' needs to be defined as 'struct scsireq'])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/scsiio.h>
#include <scsi.h>
]], [[scsireq_t req]])],,
   [AC_MSG_RESULT(yes);
    AC_DEFINE(scsireq_t, struct scsireq_t,
	      [Define scsireq_t as \'struct scsireq\' if necessary.])])
fi
AC_CHECK_LIB(scsi, scsireq_enter, SCSI_LIBS="-lscsi") # FreeBSD needs this

dnl FreeBSD >= 3
AC_CHECK_LIB(cam, cam_open_device, SCSI_LIBS="-lcam")  # FreeBSD 3+ needs this

AC_CHECK_FUNCS(scsireq_enter)

if test "$ac_cv_header_scsi_sg_h" = "yes"; then
  AC_MSG_CHECKING([for sg_header.target_status in <scsi/sg.h>])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#include <sys/types.h>
#include <scsi/sg.h>
]],[[
struct sg_header hdr;
hdr.target_status = 1;
return 0;
]])],[AC_MSG_RESULT(yes);
   AC_DEFINE(HAVE_SG_TARGET_STATUS,1,
	     [Define if sg_header.target_status is available.])],
	     AC_MSG_RESULT(no))
fi

if test "$ac_cv_header_IOKit_scsi_SCSITaskLib_h" = "yes"; then
  AC_MSG_CHECKING([for SCSITaskSGElement in IOKit/scsi/SCSITaskLib.h])
  AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[
#undef VERSION
#include <IOKit/scsi/SCSITaskLib.h>
]],[[
SCSITaskSGElement range;
return 0;
]])],[AC_MSG_RESULT(yes);
  AC_DEFINE(HAVE_SCSITASKSGELEMENT, 1,
	    [Define if SCSITaskSGElement is available.])],
  AC_MSG_RESULT(no))
fi

# Multiple platforms can set SCSI_LIBS so do substitution at end.
AC_SUBST(SCSI_LIBS)

AC_ARG_ENABLE(scsibuffersize, 
  AS_HELP_STRING([--enable-scsibuffersize=N],
                 [specify the default size in bytes of the buffer for SCSI
                  commands @<:@default=131072@:>@]),
  [set_scsibuffersize="$enableval"], [set_scsibuffersize=131072])
AC_DEFINE_UNQUOTED(SCSIBUFFERSIZE, $set_scsibuffersize,
                   [SCSI command buffer size])
dnl FIXME: Move tihs to configuration printout area but probably
dnl but probably needs to be wrapped by what ever uses it so its
dnl only printed when used.
echo "scsi buffersize: $set_scsibuffersize"

AC_ARG_ENABLE(scsi-directio,
  AS_HELP_STRING([--enable-scsi-directio],
                 [enable SCSI direct IO (Linux only, dangerous, see
                  README.linux)]),
  [
    if eval "test x$enable_scsi_directio = xyes"; then
      AM_CFLAGS="$AM_CFLAGS -DENABLE_SCSI_DIRECTIO"
    fi
  ])

dnl ****
dnl IPv6
dnl ****

dnl check for IPv6 (can be overriden by --enable-ipv6)
if test "$ac_cv_func_getnameinfo" = "yes" \
  && test "$ac_cv_func_getaddrinfo" = "yes" ; then
  SANE_CHECK_IPV6
else
  ipv6="no"
fi

dnl ***********************************************************************
dnl initialize libtool
dnl ***********************************************************************
AC_ARG_ENABLE(preload,
	AS_HELP_STRING([--disable-preload],
		       [Disable preloading of backends]),
	[enable_preload=$enableval], [enable_preload=auto])

dnl Windows (cygwin/mingw), BeOS, and OS/2 need this.
case $host_os in
  cygwin* | mingw* | beos* | os2*)
  AM_LDFLAGS="$AM_LDFLAGS -no-undefined"
esac

dnl Check for lock dir

AC_ARG_WITH(lockdir, AS_HELP_STRING([--with-lockdir=DIR],
                                    [set SANE lockdir @<:@localstatedir/lock/sane@:>@]),
                                    [locksanedir=$withval],[locksanedir=${localstatedir}/lock/sane])
AC_SUBST(locksanedir)

configdir="${sysconfdir}/sane.d"
AC_SUBST(configdir)

dnl ***********************************************************************
dnl enable/disable backends and features based on previous tests and user's
dnl choice
dnl ***********************************************************************

AM_CPPFLAGS="${AM_CPPFLAGS} -DPATH_SANE_CONFIG_DIR=\$(configdir) \
	  -DPATH_SANE_DATA_DIR=\$(datadir) \
	  -DPATH_SANE_LOCK_DIR=\$(locksanedir) \
	  -DV_MAJOR=${V_MAJOR} -DV_MINOR=${V_MINOR}"

if test "${ac_cv_header_sys_socket_h}" = "no"; then
  echo "*** disabling saned (sys/socket.h not found)"
  SANED=
else
  SANED=saned
fi
AM_CONDITIONAL(COMPILE_SANED, test x$SANED = xsaned)

dnl These are the backends that are build in any case:

AC_ARG_ENABLE(local-backends,
  AS_HELP_STRING([--disable-local-backends],
                 [turn off compilation of all backends but net]))

ALL_BACKENDS="abaton agfafocus apple artec artec_eplus48u as6e \
        avision bh canon canon630u canon_dr canon_pp cardscan \
        coolscan coolscan2 coolscan3 dc25 dc210 dc240 \
        dell1600n_net dmc epjitsu epson epson2 epsonds fujitsu genesys \
        gphoto2 gt68xx hp hp3500 hp3900 hp4200 hp5400 \
        hp5590 hpsj5s hpljm1005 hs2p ibm kodak kodakaio kvs1025 kvs20xx \
        kvs40xx leo lexmark ma1509 magicolor \
        matsushita microtek microtek2 mustek mustek_pp \
        mustek_usb mustek_usb2 nec net niash pie pieusb pint \
        pixma plustek plustek_pp qcam ricoh rts8891 s9036 \
        sceptre sharp sm3600 sm3840 snapscan sp15c st400 \
        stv680 tamarack teco1 teco2 teco3 test u12 umax 
        umax_pp umax1220u v4l xerox_mfp p5"

# If user specifies backends manually then cause configure
# to fail if its detected it can't be compiled.  If we
# are in automatic mode then remove backend from list instead.
user_selected_backends="yes"

AC_ARG_VAR(BACKENDS, [list of backends to compile])
if eval "test x$enable_local_backends = xno"; then 
   BACKENDS="net"
else
  if test "${BACKENDS}" != "" ; then
    AC_MSG_NOTICE([Manually selected backends: ${BACKENDS}])
  else
    BACKENDS="$ALL_BACKENDS"
    user_selected_backends="no"
  fi
fi

SANE_CHECK_BACKENDS

if test "${sane_cv_use_libjpeg}" = "yes"; then
  SANEI_SANEI_JPEG_LO="../sanei/sanei_jpeg.lo"
fi
AM_CONDITIONAL(HAVE_JPEG, test x$sane_cv_use_libjpeg = xyes)
AC_SUBST(SANEI_SANEI_JPEG_LO)

AC_ARG_ENABLE(pnm-backend, 
  AS_HELP_STRING([--enable-pnm-backend],
                 [enable the pnm backend for testing frontends (possible security risk, see PROBLEMS file)]),
                 [FILTERED_BACKENDS="${FILTERED_BACKENDS} pnm"],
                 [echo "*** pnm backend not manually selected - disabling"])

BACKENDS="$FILTERED_BACKENDS"
BACKEND_LIBS_ENABLED=""
BACKEND_CONFS_ENABLED=""
BACKEND_MANS_ENABLED=""
for backend in ${BACKENDS} ; do
  BACKEND_LIBS_ENABLED="${BACKEND_LIBS_ENABLED} libsane-${backend}.la"
  BACKEND_CONFS_ENABLED="${BACKEND_CONFS_ENABLED} ${backend}.conf"
  BACKEND_MANS_ENABLED="${BACKEND_MANS_ENABLED} sane-${backend}.5"
  if test x$backend = xumax_pp; then
    install_umax_pp_tools=yes
  fi
done
AC_SUBST(BACKEND_LIBS_ENABLED)
AM_CONDITIONAL(INSTALL_UMAX_PP_TOOLS, test xyes = x$install_umax_pp_tools)

AC_ARG_VAR(PRELOADABLE_BACKENDS, [list of backends to preload into single DLL])
if test "${enable_preload}" = "auto"; then
  if test "${enable_shared}" = "no" || test "${enable_dynamic}" != "yes"; then
    enable_preload=yes
  fi
fi
if test "${enable_preload}" = "yes"; then
  echo "preloading backends into DLL"

  # If user specifies backends manually then cause configure
  # to fail if its detected it can't be compiled.  If we
  # are in automatic mode then remove backend from list instead.
  user_selected_backends="yes"

  if eval "test x$enable_local_backends = xno"; then 
    PRELOADABLE_BACKENDS="net"
  else
    if test "${PRELOADABLE_BACKENDS}" != "" ; then
      AC_MSG_NOTICE([Manually selected preloadable backends: ${PRELOADABLE_BACKENDS}])
    else
      PRELOADABLE_BACKENDS="$BACKENDS"
      user_selected_backends="no"
    fi
  fi

  saved_BACKENDS="$BACKENDS"
  BACKENDS="${PRELOADABLE_BACKENDS}"

  SANE_CHECK_BACKENDS

  PRELOADABLE_BACKENDS="$FILTERED_BACKENDS"
  BACKENDS="$saved_BACKENDS"
else
  PRELOADABLE_BACKENDS=""
fi
PRELOADABLE_BACKENDS_ENABLED=""
# Do not initialize BACKEND_CONFS_ENABLED so that its a combination
# of all backends.
for backend in ${PRELOADABLE_BACKENDS} ; do
  BACKEND_CONFS_ENABLED="${BACKEND_CONFS_ENABLED} ${backend}.conf"
  BACKEND_MANS_ENABLED="${BACKEND_MANS_ENABLED} sane-${backend}.5"
  # Special hacks.  Normally, we create a convenience library that
  # matches the backend's name but in some cases that will conflict
  # with an external library name that also matches backend name.
  # Work around that here by renaming internal library.
  if test "${backend}" = "gphoto2"; then
    backend=gphoto2_i
  fi
  PRELOADABLE_BACKENDS_ENABLED="${PRELOADABLE_BACKENDS_ENABLED} lib${backend}.la"
done

AC_SUBST(PRELOADABLE_BACKENDS)
AC_SUBST(PRELOADABLE_BACKENDS_ENABLED)
AC_SUBST(BACKEND_CONFS_ENABLED)
AC_SUBST(BACKEND_MANS_ENABLED)

dnl in BeOS, backends are not installed in lib/sane, but add-ons/sane/
dnl we account for both installing in /boot/beos and ~/config
case $host_os in
  beos*)
  libdir='${exec_prefix}/add-ons'
  ;;
esac

AC_ARG_ENABLE(parport-directio,
  AS_HELP_STRING([--enable-parport-directio],
                 [enable direct hardware access to the parallel port, so 
                  frontends will require special permission level]),
  [
    if eval "test x$enable_parport_directio = xyes"; then
      AM_CFLAGS="$AM_CFLAGS -DENABLE_PARPORT_DIRECTIO"
    fi
  ])

dnl ******************************************************************
dnl  SANE API specification format conversion support
dnl ******************************************************************
AC_ARG_WITH(api-spec,
  AS_HELP_STRING([--with-api-spec],
    [convert API spec to supported output formats @<:@default=check@:>@]),
    [],
    [with_api_spec=check])
dnl  Test for all tools that may be involved.  These tests are fast and
dnl  running them allows for the Makefile targets to be formulated such
dnl  that any non-requested formats can be made using a one-off without
dnl  the need to reconfigure.
AC_PATH_PROG(MAKEINDEX, makeindex, no)
AC_PATH_PROG(DVIPS, dvips, no)
AC_PATH_PROG(LATEX, latex, no)
AC_PATH_PROG(PDFLATEX, pdflatex, no)
AC_PATH_PROG(FIG2DEV, fig2dev, no)
AC_PATH_PROG(GS, gs, no)
AC_PATH_PROG(DLH, dlh, no)
AC_PATH_PROG(PPMTOGIF, ppmtogif, no)
AS_IF([test xno != "x$with_api_spec"],
  [dnl  Flag formats for which all required tools have been found
   AS_IF([   test xno != "x$MAKEINDEX" \
          && test xno != "x$DVIPS" \
          && test xno != "x$FIG2DEV" \
          && test xno != "x$LATEX"],       [with_api_ps=yes])
   AS_IF([   test xno != "x$MAKEINDEX" \
          && test xno != "x$PDFLATEX" \
          && test xno != "x$FIG2DEV" \
          && test xno != "x$GS"],          [with_api_pdf=yes])
   AS_IF([   test xno != "x$MAKEINDEX" \
          && test xno != "x$DVIPS" \
          && test xno != "x$FIG2DEV" \
          && test xno != "x$DLH" \
          && test xno != "x$GS" \
          && test xno != "x$PPMTOFIG" ],   [with_api_html=yes])

   AS_IF([test xyes = "x$with_api_spec" \
          && test xyes != "x$with_api_ps" \
          && test xyes != "x$with_api_pdf" \
          && test xyes != "x$with_api_html"],
     [AC_MSG_ERROR([tools to convert the API spec are missing])
     ])
  ])
AM_CONDITIONAL(WITH_API_PS, [test x$with_api_ps = xyes])
AM_CONDITIONAL(WITH_API_PDF, [test x$with_api_pdf = xyes])
AM_CONDITIONAL(WITH_API_HTML, [test x$with_api_html = xyes])

dnl ***********************************************************************
dnl Write output files
dnl ***********************************************************************

AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_CFLAGS)
AC_SUBST(AM_LDFLAGS)

AC_CONFIG_FILES([Makefile lib/Makefile sanei/Makefile frontend/Makefile \
  japi/Makefile backend/Makefile include/Makefile doc/Makefile \
  po/Makefile.in testsuite/Makefile testsuite/sanei/Makefile testsuite/tools/Makefile \
  tools/Makefile doc/doxygen-sanei.conf doc/doxygen-genesys.conf])
AC_CONFIG_FILES([tools/sane-config], [chmod a+x tools/sane-config])
AC_CONFIG_FILES([tools/sane-backends.pc])
AC_OUTPUT

dnl ***********************************************************************
dnl print configuration information
dnl ***********************************************************************

echo "-> Variables used for compilation/linking:"
echo AM_CPPFLAGS=\"${AM_CPPFLAGS}\"
echo AM_CFLAGS=\"${AM_CFLAGS}\"
echo AM_LDFLAGS=\"${AM_LDFLAGS}\"
echo LIBS=\"${LIBS}\"
echo "-> Installation directories:"
echo "Configuration: `eval eval echo ${sysconfdir}`"
echo "Libraries:     `eval eval echo ${libdir}`"
echo "Binaries:      `eval eval echo ${bindir}` and `eval eval echo ${sbindir}`"
echo "Manpages:      `eval eval echo ${mandir}`"
echo "Documentation: `eval eval echo ${docdir}`"
if eval "test x$INSTALL_LOCKPATH = xinstall-lockpath" ; then
  echo "Lockfiles:     `eval eval echo ${locksanedir}`"
else
  echo "Lockfiles:     Feature is disabled!"
fi
echo "-> Network parameters:"
if test "${SANED}" = "saned" ; then
  echo "Build saned:   yes"
else
  echo "Build saned:   no"
fi
echo "IPv6 support:  `eval eval echo ${ipv6}`"
echo "Avahi support: `eval eval echo ${enable_avahi}`"
echo "SNMP support:  `eval eval echo ${with_snmp}`"
echo "-> The following backends will be built:"
for backend in ${BACKENDS} ; do
  echo $ECHO_N "${backend} " 
done
echo 
echo
echo "-> The following preload backends will be built:"
for backend in ${PRELOADABLE_BACKENDS} ; do
  echo $ECHO_N "${backend} " 
done
echo

if test "$SANE_CONFIG_PATH" != "no" ; then
  SANE_INSTALLED_VERSION=`$SANE_CONFIG_PATH --version`
  SANE_INSTALLED_PREFIX=`$SANE_CONFIG_PATH --prefix`
  if test "$SANE_INSTALLED_PREFIX" != "$prefix" ; then
    echo "*** WARNING: SANE is already installed (version $SANE_INSTALLED_VERSION). The old"
    echo "*** installation is at $SANE_INSTALLED_PREFIX while SANE will now be installed"
    echo "*** at $prefix. It is recommended to uninstall the old SANE version"
    echo "*** before installing the new one to avoid problems."
  fi
else
  if test "$LIBSANE_EXISTS" = "yes" ; then
    echo "*** Warning: An old version of SANE was detected but the sane-config program"
    echo "*** couldn't be found. If you encounter any problems with SANE remove the old"
    echo "*** SANE files and reinstall this version."
  fi
fi
if eval "test x${sysconfdir} = x/usr/etc" ; then
  echo "*** WARNING: Configuration files are stored in /usr/etc. If this is not"
  echo "*** intended, use --sysconfdir to set another path (e.g. "/etc")."
fi
if test "$SELECTED_BACKENDS" != "" ; then
  echo "*** Warning: Local backends have been disabled.  This means that scanners"
  echo "*** connected to your local computer won't be supported.  Only a network"
  echo "*** connection to a remote host is possible."
fi
if test "$with_usb" != "no" && test "$have_usb" != "yes" ; then
  echo "*** Warning: sane-backends will be built without USB support.  There may"
  echo "*** be valid reasons to do so, e.g. if you don't use USB scanners or on"
  echo "*** platforms without USB support but generally this means that you"
  echo "*** can't use USB devices with SANE. The most probable cause is that"
  if test "${ac_cv_header_usb_h}" != "yes"; then
    echo "*** the libusb header file usb.h is not installed. If you use Linux"
    echo "*** you may need a package called 'libusb-dev', 'libusb-devel' or similar."
  else
    echo "*** libusb is not installed at all or is too old. See README."
  fi
fi

echo "****************************************************************"
echo "* Please be sure to read file PROBLEMS in this directory       *"
echo "* BEFORE running any of the SANE applications.  Some devices   *"
echo "* may be damaged by improper operation, so please do heed this *"
echo "* advice.                                                      *"
echo "****************************************************************"
