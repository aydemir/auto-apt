AC_INIT([auto-apt], [1.0], [krichter722@aol.de])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([foreign -Wall subdir-objects])
AC_PROG_CC
AM_PROG_AR

LT_INIT # needs to be after AM_PROGS_AR
AC_CONFIG_HEADERS([config.h])

PKG_CHECK_MODULES([CHECK], [check >= 0.9.4])

AC_CONFIG_FILES([pkgcdb/Makefile Makefile tests/Makefile])
AC_OUTPUT
