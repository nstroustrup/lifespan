/* include/sane/config.h.  Generated from config.h.in by configure.  */
/* include/sane/config.h.in.  Generated from configure.ac by autoheader.  */

/* Define if building universal (internal helper macro) */
/* #undef AC_APPLE_UNIVERSAL_BUILD */

/* Define to one of `_getb67', `GETB67', `getb67' for Cray-2 and Cray-YMP
   systems. This function is required for `alloca.c' support on those systems.
   */
/* #undef CRAY_STACKSEG_END */

/* Define to 1 if using `alloca.c'. */
/* #undef C_ALLOCA */

/* Define to 1 if the system supports IPv6 */
#define ENABLE_IPV6 1

/* Define to 1 if device locking should be enabled. */
/* #undef ENABLE_LOCKING */

/* Define to 1 if translation of program messages to the user's native
   language is requested. */
#define ENABLE_NLS 1

/* Define if GPLogFunc does not take a va_list. */
/* #undef GPLOGFUNC_NO_VARGS */

/* Define to 1 if struct sockaddr_storage has an ss_family member */
#define HAS_SS_FAMILY 1

/* Define to 1 if struct sockaddr_storage has __ss_family instead of ss_family
   */
/* #undef HAS___SS_FAMILY */

/* Define to 1 if you have `alloca', as a function or macro. */
#define HAVE_ALLOCA 1

/* Define to 1 if you have <alloca.h> and it should be used (not on Ultrix).
   */
#define HAVE_ALLOCA_H 1

/* Define to 1 if you have the <apollo/scsi.h> header file. */
/* #undef HAVE_APOLLO_SCSI_H */

/* Define to 1 if you have the <asm/io.h> header file. */
/* #undef HAVE_ASM_IO_H */

/* Define to 1 if you have the <asm/types.h> header file. */
#define HAVE_ASM_TYPES_H 1

/* Define to 1 if you have the `atexit' function. */
#define HAVE_ATEXIT 1

/* Define to 1 if you have the <be/kernel/OS.h> header file. */
/* #undef HAVE_BE_KERNEL_OS_H */

/* Define to 1 if you have the <bsd/dev/scsireg.h> header file. */
/* #undef HAVE_BSD_DEV_SCSIREG_H */

/* Define to 1 if you have the <camlib.h> header file. */
/* #undef HAVE_CAMLIB_H */

/* Define to 1 if you have the MacOS X function CFLocaleCopyCurrent in the
   CoreFoundation framework. */
/* #undef HAVE_CFLOCALECOPYCURRENT */

/* Define to 1 if you have the `cfmakeraw' function. */
#define HAVE_CFMAKERAW 1

/* Define to 1 if you have the MacOS X function CFPreferencesCopyAppValue in
   the CoreFoundation framework. */
/* #undef HAVE_CFPREFERENCESCOPYAPPVALUE */

/* Define if the GNU dcgettext() function is already present or preinstalled.
   */
#define HAVE_DCGETTEXT 1

/* Define to 1 if you have the <ddk/ntddscsi.h> header file. */
/* #undef HAVE_DDK_NTDDSCSI_H */

/* Define to 1 if you have the <dev/ppbus/ppi.h> header file. */
/* #undef HAVE_DEV_PPBUS_PPI_H */

/* Is /dev/urandom available? */
#define HAVE_DEV_URANDOM 1

/* Define to 1 if you have the <dlfcn.h> header file. */
#define HAVE_DLFCN_H 1

/* Define to 1 if you have the `dlopen' function. */
#define HAVE_DLOPEN 1

/* Define to 1 if you have the <dl.h> header file. */
/* #undef HAVE_DL_H */

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the `getaddrinfo' function. */
#define HAVE_GETADDRINFO 1

/* Define to 1 if you have the `getenv' function. */
#define HAVE_GETENV 1

/* Define to 1 if you have the `getnameinfo' function. */
#define HAVE_GETNAMEINFO 1

/* Define to 1 if you have the <getopt.h> header file. */
#define HAVE_GETOPT_H 1

/* Define to 1 if you have the `getopt_long' function. */
#define HAVE_GETOPT_LONG 1

/* Define to 1 if you have the `getpagesize' function. */
#define HAVE_GETPAGESIZE 1

/* Define to 1 if you have the `getpass' function. */
#define HAVE_GETPASS 1

/* Define if the GNU gettext() function is already present or preinstalled. */
#define HAVE_GETTEXT 1

/* Define to 1 if you have the `getuid' function. */
#define HAVE_GETUID 1

/* Define to 1 if you have the `gp_camera_init' function. */
/* #undef HAVE_GP_CAMERA_INIT */

/* Define to 1 if you have the `gp_port_info_get_path' function. */
/* #undef HAVE_GP_PORT_INFO_GET_PATH */

/* Define to 1 if you have the <gscdds.h> header file. */
/* #undef HAVE_GSCDDS_H */

/* Define to 1 if you have the `i386_set_ioperm' function. */
/* #undef HAVE_I386_SET_IOPERM */

/* Define if you have the iconv() function and it works. */
/* #undef HAVE_ICONV */

/* Define to 1 if you have the <ifaddrs.h> header file. */
#define HAVE_IFADDRS_H 1

/* Define to 1 if you have the `inet_addr' function. */
#define HAVE_INET_ADDR 1

/* Define to 1 if you have the `inet_aton' function. */
#define HAVE_INET_ATON 1

/* Define to 1 if you have the `inet_ntoa' function. */
#define HAVE_INET_NTOA 1

/* Define to 1 if you have the `inet_ntop' function. */
#define HAVE_INET_NTOP 1

/* Define to 1 if you have the `inet_pton' function. */
#define HAVE_INET_PTON 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define to 1 if you have the <IOKit/cdb/IOSCSILib.h> header file. */
/* #undef HAVE_IOKIT_CDB_IOSCSILIB_H */

/* Define to 1 if you have the
   <IOKit/scsi-commands/SCSICommandOperationCodes.h> header file. */
/* #undef HAVE_IOKIT_SCSI_COMMANDS_SCSICOMMANDOPERATIONCODES_H */

/* Define to 1 if you have the <IOKit/scsi/SCSICommandOperationCodes.h> header
   file. */
/* #undef HAVE_IOKIT_SCSI_SCSICOMMANDOPERATIONCODES_H */

/* Define to 1 if you have the <IOKit/scsi/SCSITaskLib.h> header file. */
/* #undef HAVE_IOKIT_SCSI_SCSITASKLIB_H */

/* Define to 1 if you have the `ioperm' function. */
#define HAVE_IOPERM 1

/* Define to 1 if you have the `iopl' function. */
#define HAVE_IOPL 1

/* Define to 1 if you have the <io/cam/cam.h> header file. */
/* #undef HAVE_IO_CAM_CAM_H */

/* Define to 1 if you have the `isfdtype' function. */
#define HAVE_ISFDTYPE 1

/* Define to 1 if you have the <libc.h> header file. */
/* #undef HAVE_LIBC_H */

/* Define to 1 if you have the `ieee1284' library (-lcam). */
#define HAVE_LIBIEEE1284 1

/* Define to 1 if you have the libjpeg library. */
#define HAVE_LIBJPEG 1

/* Define to 1 if you have the libpng library. */
#define HAVE_LIBPNG 1

/* Define to 1 if you have the net-snmp library. */
#define HAVE_LIBSNMP 1

/* Define to 1 if you have libusb-1.0 */
#define HAVE_LIBUSB 1

/* Define to 1 if you have libusb-0.1 */
/* #undef HAVE_LIBUSB_LEGACY */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the <linux/ppdev.h> header file. */
#define HAVE_LINUX_PPDEV_H 1

/* Define if the long long type is available. */
#define HAVE_LONG_LONG 1

/* Define to 1 if you have the <lusb0_usb.h> header file. */
/* #undef HAVE_LUSB0_USB_H */

/* Define to 1 if you have the <machine/cpufunc.h> header file. */
/* #undef HAVE_MACHINE_CPUFUNC_H */

/* Define to 1 if you have the <mach-o/dyld.h> header file. */
/* #undef HAVE_MACH_O_DYLD_H */

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `mkdir' function. */
#define HAVE_MKDIR 1

/* Define to 1 if you have a working `mmap' system call. */
#define HAVE_MMAP 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define to 1 if you have the `NSLinkModule' function. */
/* #undef HAVE_NSLINKMODULE */

/* Define to 1 if you have the <ntddscsi.h> header file. */
/* #undef HAVE_NTDDSCSI_H */

/* Define to 1 if you have the <os2.h> header file. */
/* #undef HAVE_OS2_H */

/* Define to 1 if you have the `poll' function. */
#define HAVE_POLL 1

/* Define to 1 if you have the `pthread_cancel' function. */
#define HAVE_PTHREAD_CANCEL 1

/* Define to 1 if you have the `pthread_create' function. */
#define HAVE_PTHREAD_CREATE 1

/* Define to 1 if you have the `pthread_detach' function. */
#define HAVE_PTHREAD_DETACH 1

/* Define to 1 if you have the <pthread.h> header file. */
#define HAVE_PTHREAD_H 1

/* Define to 1 if you have the `pthread_join' function. */
#define HAVE_PTHREAD_JOIN 1

/* Define to 1 if you have the `pthread_kill' function. */
#define HAVE_PTHREAD_KILL 1

/* Define to 1 if you have the `pthread_testcancel' function. */
#define HAVE_PTHREAD_TESTCANCEL 1

/* Define to 1 if you have the <pwd.h> header file. */
#define HAVE_PWD_H 1

/* define if you have the resmgr library */
/* #undef HAVE_RESMGR */

/* Define to 1 if you have the `scsireq_enter' function. */
/* #undef HAVE_SCSIREQ_ENTER */

/* Define if SCSITaskSGElement is available. */
/* #undef HAVE_SCSITASKSGELEMENT */

/* Define to 1 if you have the <scsi.h> header file. */
/* #undef HAVE_SCSI_H */

/* Define to 1 if you have the <scsi/sg.h> header file. */
#define HAVE_SCSI_SG_H 1

/* Define to 1 if you have the `setitimer' function. */
#define HAVE_SETITIMER 1

/* Define if sg_header.target_status is available. */
#define HAVE_SG_TARGET_STATUS 1

/* Define to 1 if you have the `shl_load' function. */
/* #undef HAVE_SHL_LOAD */

/* Define to 1 if you have the `sigprocmask' function. */
#define HAVE_SIGPROCMASK 1

/* Define to 1 if you have the `sleep' function. */
#define HAVE_SLEEP 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcasecmp' function. */
#define HAVE_STRCASECMP 1

/* Define to 1 if you have the `strcasestr' function. */
#define HAVE_STRCASESTR 1

/* Define to 1 if you have the `strdup' function. */
#define HAVE_STRDUP 1

/* Define to 1 if you have the `strftime' function. */
#define HAVE_STRFTIME 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncasecmp' function. */
#define HAVE_STRNCASECMP 1

/* Define to 1 if you have the `strndup' function. */
#define HAVE_STRNDUP 1

/* Define to 1 if you have the `strsep' function. */
#define HAVE_STRSEP 1

/* Define to 1 if you have the `strstr' function. */
#define HAVE_STRSTR 1

/* Define to 1 if you have the `strtod' function. */
#define HAVE_STRTOD 1

/* Define if struct flock is available. */
#define HAVE_STRUCT_FLOCK 1

/* Define to 1 if you have the `syslog' function. */
#define HAVE_SYSLOG 1

/* Is /usr/include/systemd/sd-daemon.h available? */
#define HAVE_SYSTEMD 1

/* Define to 1 if you have the <sys/dsreq.h> header file. */
/* #undef HAVE_SYS_DSREQ_H */

/* Define to 1 if you have the <sys/hw.h> header file. */
/* #undef HAVE_SYS_HW_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/io.h> header file. */
#define HAVE_SYS_IO_H 1

/* Define to 1 if you have the <sys/ipc.h> header file. */
#define HAVE_SYS_IPC_H 1

/* Define to 1 if you have the <sys/param.h> header file. */
#define HAVE_SYS_PARAM_H 1

/* Define to 1 if you have the <sys/passthrudef.h> header file. */
/* #undef HAVE_SYS_PASSTHRUDEF_H */

/* Define to 1 if you have the <sys/poll.h> header file. */
#define HAVE_SYS_POLL_H 1

/* Define to 1 if you have the <sys/scanio.h> header file. */
/* #undef HAVE_SYS_SCANIO_H */

/* Define to 1 if you have the <sys/scsicmd.h> header file. */
/* #undef HAVE_SYS_SCSICMD_H */

/* Define to 1 if you have the <sys/scsiio.h> header file. */
/* #undef HAVE_SYS_SCSIIO_H */

/* Define to 1 if you have the <sys/scsi.h> header file. */
/* #undef HAVE_SYS_SCSI_H */

/* Define to 1 if you have the <sys/scsi/scsi.h> header file. */
/* #undef HAVE_SYS_SCSI_SCSI_H */

/* Define to 1 if you have the <sys/scsi/sgdefs.h> header file. */
/* #undef HAVE_SYS_SCSI_SGDEFS_H */

/* Define to 1 if you have the <sys/scsi/targets/scgio.h> header file. */
/* #undef HAVE_SYS_SCSI_TARGETS_SCGIO_H */

/* Define to 1 if you have the <sys/sdi_comm.h> header file. */
/* #undef HAVE_SYS_SDI_COMM_H */

/* Define to 1 if you have the <sys/select.h> header file. */
#define HAVE_SYS_SELECT_H 1

/* Define to 1 if you have the <sys/sem.h> header file. */
#define HAVE_SYS_SEM_H 1

/* Define to 1 if you have the <sys/shm.h> header file. */
#define HAVE_SYS_SHM_H 1

/* Define to 1 if you have the <sys/signal.h> header file. */
#define HAVE_SYS_SIGNAL_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have the `tcsendbreak' function. */
#define HAVE_TCSENDBREAK 1

/* Define to 1 if you have the <tiffio.h> header file. */
#define HAVE_TIFFIO_H 1

/* Define if union semun is available. */
/* #undef HAVE_UNION_SEMUN */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define to 1 if you have usbcall.dll. */
/* #undef HAVE_USBCALLS */

/* Define to 1 if you have the `usleep' function. */
#define HAVE_USLEEP 1

/* Define to 1 if the system has the type `u_char'. */
#define HAVE_U_CHAR 1

/* Define to 1 if the system has the type `u_int'. */
#define HAVE_U_INT 1

/* Define to 1 if the system has the type `u_long'. */
#define HAVE_U_LONG 1

/* Define to 1 if the system has the type `u_short'. */
#define HAVE_U_SHORT 1

/* Define to 1 if you have the `vsyslog' function. */
#define HAVE_VSYSLOG 1

/* Define to 1 if you have the <windows.h> header file. */
/* #undef HAVE_WINDOWS_H */

/* Define to 1 if you have the <winsock2.h> header file. */
/* #undef HAVE_WINSOCK2_H */

/* Define to 1 if you have the `_portaccess' function. */
/* #undef HAVE__PORTACCESS */

/* Define to the sub-directory in which libtool stores uninstalled libraries.
   */
#define LT_OBJDIR ".libs/"

/* Name of package */
#define PACKAGE "sane-backends"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "sane-devel@lists.alioth.debian.org"

/* Define to the full name of this package. */
#define PACKAGE_NAME "sane-backends"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "sane-backends 1.0.27"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "sane-backends"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "1.0.27"

/* SANE DLL revision number */
#define SANE_DLL_V_BUILD 27

/* SANE DLL major number */
#define SANE_DLL_V_MAJOR 1

/* SANE DLL minor number */
#define SANE_DLL_V_MINOR 0

/* Define to 1 if you have the <sys/io.h> providing inb,outb. */
#define SANE_HAVE_SYS_IO_H_WITH_INB_OUTB 1

/* SCSI command buffer size */
#define SCSIBUFFERSIZE 131072

/* The size of `char', as computed by sizeof. */
/* #undef SIZEOF_CHAR */

/* The size of `int', as computed by sizeof. */
/* #undef SIZEOF_INT */

/* The size of `long', as computed by sizeof. */
/* #undef SIZEOF_LONG */

/* The size of `short', as computed by sizeof. */
/* #undef SIZEOF_SHORT */

/* The size of `void*', as computed by sizeof. */
/* #undef SIZEOF_VOIDP */

/* If using the C implementation of alloca, define if you know the
   direction of stack growth for your system; otherwise it will be
   automatically deduced at runtime.
	STACK_DIRECTION > 0 => grows toward higher addresses
	STACK_DIRECTION < 0 => grows toward lower addresses
	STACK_DIRECTION = 0 => direction of growth unknown */
/* #undef STACK_DIRECTION */

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Define if pthreads should be used instead of forked processes. */
#define USE_PTHREAD "yes"

/* Enable extensions on AIX 3, Interix.  */
#ifndef _ALL_SOURCE
# define _ALL_SOURCE 1
#endif
/* Enable GNU extensions on systems that have them.  */
#ifndef _GNU_SOURCE
# define _GNU_SOURCE 1
#endif
/* Enable threading extensions on Solaris.  */
#ifndef _POSIX_PTHREAD_SEMANTICS
# define _POSIX_PTHREAD_SEMANTICS 1
#endif
/* Enable extensions on HP NonStop.  */
#ifndef _TANDEM_SOURCE
# define _TANDEM_SOURCE 1
#endif
/* Enable general extensions on Solaris.  */
#ifndef __EXTENSIONS__
# define __EXTENSIONS__ 1
#endif


/* Version number of package */
#define VERSION "1.0.27"

/* define if Avahi support is enabled for saned and the net backend */
/* #undef WITH_AVAHI */

/* Define WORDS_BIGENDIAN to 1 if your processor stores words with the most
   significant byte first (like Motorola and SPARC, unlike Intel). */
#if defined AC_APPLE_UNIVERSAL_BUILD
# if defined __BIG_ENDIAN__
#  define WORDS_BIGENDIAN 1
# endif
#else
# ifndef WORDS_BIGENDIAN
/* #  undef WORDS_BIGENDIAN */
# endif
#endif

/* Define to 1 if on MINIX. */
/* #undef _MINIX */

/* Define to 2 if the system does not provide POSIX.1 features except with
   this defined. */
/* #undef _POSIX_1_SOURCE */

/* Define to 1 if you need to in order for `stat' and other things to work. */
/* #undef _POSIX_SOURCE */

/* Define to empty if `const' does not conform to ANSI C. */
/* #undef const */

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `int' if <sys/types.h> does not define. */
/* #undef pid_t */

/* Define scsireq_t as \'struct scsireq\' if necessary. */
/* #undef scsireq_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define socklen_t as \'int\' if necessary. */
/* #undef socklen_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define for OS/2 only */
/* #undef strcasecmp */

/* Define for OS/2 only */
/* #undef strncasecmp */



#if defined(__MINGW32__)
#define _BSDTYPES_DEFINED
#endif

#ifndef HAVE_U_CHAR
#define u_char unsigned char
#endif
#ifndef HAVE_U_SHORT
#define u_short unsigned short
#endif
#ifndef HAVE_U_INT
#define u_int unsigned int
#endif
#ifndef HAVE_U_LONG
#define u_long unsigned long
#endif

/* Prototype for getenv */
#ifndef HAVE_GETENV
#define getenv sanei_getenv
char * getenv(const char *name);
#endif

/* Prototype for inet_ntop */
#ifndef HAVE_INET_NTOP
#define inet_ntop sanei_inet_ntop
#include <sys/types.h>
const char * inet_ntop (int af, const void *src, char *dst, size_t cnt);
#endif

/* Prototype for inet_pton */
#ifndef HAVE_INET_PTON
#define inet_pton sanei_inet_pton
int inet_pton (int af, const char *src, void *dst);
#endif

/* Prototype for isfdtype */
#ifndef HAVE_ISFDTYPE
#define isfdtype sanei_isfdtype
int isfdtype(int fd, int fdtype);
#endif

/* Prototype for sigprocmask */
#ifndef HAVE_SIGPROCMASK
#define sigprocmask sanei_sigprocmask
int sigprocmask (int how, int *new, int *old);
#endif

/* Prototype for snprintf */
#ifndef HAVE_SNPRINTF
#define snprintf sanei_snprintf
#include <sys/types.h>
int snprintf (char *str,size_t count,const char *fmt,...);
#endif

/* Prototype for strcasestr */
#ifndef HAVE_STRCASESTR
#define strcasestr sanei_strcasestr
char * strcasestr (const char *phaystack, const char *pneedle);
#endif

/* Prototype for strdup */
#ifndef HAVE_STRDUP
#define strdup sanei_strdup
char *strdup (const char * s);
#endif

/* Prototype for strndup */
#ifndef HAVE_STRNDUP
#define strndup sanei_strndup
#include <sys/types.h>
char *strndup(const char * s, size_t n);
#endif

/* Prototype for strsep */
#ifndef HAVE_STRSEP
#define strsep sanei_strsep
char *strsep(char **stringp, const char *delim);
#endif

/* Prototype for usleep */
#ifndef HAVE_USLEEP
#define usleep sanei_usleep
unsigned int usleep (unsigned int useconds);
#endif

/* Prototype for vsyslog */
#ifndef HAVE_VSYSLOG
#include <stdarg.h>
void vsyslog(int priority, const char *format, va_list args);
#endif

