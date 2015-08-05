#
# Makefile
# $Id: Makefile,v 1.13 2001/12/19 16:36:05 ukai Exp $
#
DESTDIR=
package=auto-apt
SHLIBDIR=$(DESTDIR)/lib
BINDIR=$(DESTDIR)/usr/bin
LIBDIR=$(DESTDIR)/usr/lib/$(package)
CACHEDIR=$(DESTDIR)/var/cache/$(package)
ETCDIR=$(DESTDIR)/etc/$(package)
PYTHON ?= python

DEFS=-DUSE_DETECT -DDEBUG
CC=gcc
CFLAGS=-g -Wall -finline-functions -Ipkgcdb $(DEFS)

all: auto-apt.so auto-apt-pkgcdb
	$(PYTHON) setup.py build

auto-apt.so: auto-apt.o pkgcdb/pkgcdb2.a
	$(CC) -shared -o auto-apt.so auto-apt.o -lc -ldl 

auto-apt.o: auto-apt.c 
	$(CC) $(CFLAGS) -fPIC -o auto-apt.o -c auto-apt.c

auto-apt-pkgcdb: auto-apt-pkgcdb.o pkgcdb/pkgcdb2.a
	$(CC) $(CFLAGS) -o auto-apt-pkgcdb auto-apt-pkgcdb.o pkgcdb/pkgcdb2.a

pkgcdb/pkgcdb2.a:
	(cd pkgcdb && \
	 $(MAKE) pkgcdb2.a CC="$(CC)" DEFS="$(DEFS)" CFLAGS="$(CFLAGS)")

install: all
	install -m 755 auto-apt.sh $(BINDIR)/auto-apt
	install -m 755 auto-apt-pkgcdb $(LIBDIR)/
	install -m 755 auto-apt-installer.pl $(LIBDIR)/auto-apt-installer
	install -m 644 auto-apt.so $(SHLIBDIR)/
	install -m 644 paths.list $(ETCDIR)/
#	install -m 644 paths.list commands.list $(ETCDIR)/
	$(PYTHON) setup.py install

clean:
	(cd pkgcdb && $(MAKE) clean)
	-rm -f auto-apt.so auto-apt.o
	-rm -f auto-apt-pkgcdb auto-apt-pkgcdb.o
	-rm -rf cache

distclean: clean
	(cd pkgcdb && $(MAKE) distclean)
	-rm -f *~ *.bak *.orig *.db *.o *.so

