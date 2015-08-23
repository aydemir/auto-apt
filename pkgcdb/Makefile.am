#
# Makefile
# $Id: Makefile,v 1.1 2000/07/12 03:42:47 ukai Exp $
#


lib_LTLIBRARIES = libpkgcdb2.la
libpkgcdb2_la_SOURCES = pkgcdb2.c pathnode.c pkgtab.c strtab.c mempool.c \
    pkgcdb2.h pathnode.h pkgtab.h strtab.h mempool.h
libpkgcdb2_la_CFLAGS = -fPIC

bin_PROGRAMS = main
main_SOURCES = ../main.c ../auto-apt-pkgcdb.c
main_LDADD = libpkgcdb2.la

ACLOCAL_AMFLAGS = -I m4
