
TESTS = check_auto_apt
check_PROGRAMS = check_auto_apt
check_auto_apt_SOURCES = ../auto-apt.h ../auto-apt.c check_auto_apt.c
check_auto_apt_CFLAGS = @CHECK_CFLAGS@
check_auto_apt_LDFLAGS = -lcheck_pic -lpthread -lrt -lm -ldl
check_auto_apt_LDADD = $(top_builddir)/pkgcdb/libpkgcdb2.la @CHECK_LIBS@
