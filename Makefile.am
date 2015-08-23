SUBDIRS = pkgcdb . tests

ACLOCAL_AMFLAGS = -I m4

#
# Makefile
# $Id: Makefile,v 1.13 2001/12/19 16:36:05 ukai Exp $
#
#DESTDIR=
#package=auto-apt
#SHLIBDIR=$(DESTDIR)/lib
#BINDIR=$(DESTDIR)/usr/bin
#LIBDIR=$(DESTDIR)/usr/lib/$(package)
#CACHEDIR=$(DESTDIR)/var/cache/$(package)
#ETCDIR=$(DESTDIR)/etc/$(package)
PYTHON ?= python

#DEFS=-DUSE_DETECT -DDEBUG
#CC=gcc
#CFLAGS=-g -Wall -finline-functions -Ipkgcdb $(DEFS)

wrapper:
	$(PYTHON) setup.py build

wrapper-install:
	$(PYTHON) setup.py install

build-local: wrapper

install-exec-local: wrapper-install
