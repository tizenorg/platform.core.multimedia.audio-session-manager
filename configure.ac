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
