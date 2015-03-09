#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.61)
AC_INIT(AUDIOSESSIONMGR, 1.0, BUG-REPORT-ADDRESS)
AC_CONFIG_HEADER([config.h])

AM_INIT_AUTOMAKE(audio-session-manager, 1.0)

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_PROG_LIBTOOL

# Checks for libraries.
PKG_CHECK_MODULES(AUDIOSESSIONMGR, glib-2.0 gthread-2.0)
AC_SUBST(AUDIOSESSIONMGR_CFLAGS)
AC_SUBST(AUDIOSESSIONMGR_LIBS)

PKG_CHECK_MODULES(MMLOG, mm-common)
AC_SUBST(MMCOMMON_CFLAGS)
AC_SUBST(MMCOMMON_LIBS)

PKG_CHECK_MODULES(MMLOG, mm-log)
AC_SUBST(MMLOG_CFLAGS)
AC_SUBST(MMLOG_LIBS)

PKG_CHECK_MODULES(VCONF, vconf)
AC_SUBST(VCONF_CFLAGS)
AC_SUBST(VCONF_LIBS)

PKG_CHECK_MODULES(GIO, gio-2.0)
AC_SUBST(GIO_CFLAGS)
AC_SUBST(GIO_LIBS)

dnl use security ---------------------------------------------------------------
AC_ARG_ENABLE(security, AC_HELP_STRING([--enable-security], [using security]),
[
case "${enableval}" in
    yes) USE_SECURITY=yes ;;
    no)  USE_SECURITY=no ;;
    *)   AC_MSG_ERROR(bad value ${enableval} for --enable-security) ;;
esac
],[USE_SECURITY=no])

if test "x$USE_SECURITY" = "xyes"; then
    PKG_CHECK_MODULES(SECURITY, security-server)
    AC_SUBST(SECURITY_CFLAGS)
    AC_SUBST(SECURITY_LIBS)
fi
AM_CONDITIONAL(USE_SECURITY, test "x$USE_SECURITY" = "xyes")
dnl end ------------------------------------------------------------------------

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([fcntl.h stdlib.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_HEADER_STDBOOL
AC_C_CONST
AC_TYPE_MODE_T

# Checks for library functions.
AC_FUNC_MALLOC
AC_CHECK_FUNCS([memset])

AC_CONFIG_FILES([Makefile
		 test/Makefile
		 audio-session-mgr.pc
		 ])
AC_OUTPUT
