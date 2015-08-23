/* 
 * auto-apt.so
 * on demand package installation tool
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 * File:   auto-apt.h
 * Author: richter
 *
 * Created on 13. August 2015, 17:50
 */

#ifndef AUTO_APT_H
#define	AUTO_APT_H

#define LARGEFILE_SOURCE
#define LARGEFILE64_SOURCE
#define __USE_LARGEFILE64 1
#define __USE_FILE_OFFSET64 1

#include <unistd.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <dlfcn.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include <limits.h>

#define PKGCDB_AUTOAPT 1
#include "pkgcdb/debug.h"
#include "pkgcdb/pkgcdb2.h"
#include "pkgcdb/mempool.h"
#include "pkgcdb/strtab.h"
#include "pkgcdb/pkgtab.h"
#include "pkgcdb/pathnode.h"

#define APT_HOOK_EXEC	0
#define APT_HOOK_OPEN	1
#define APT_HOOK_ACCESS	2
#define APT_HOOK_STAT	3
#define NUM_APT_HOOK	4

#ifdef __alpha__
#define LIBCPATH "/lib/libc.so.6.1"
#else
#define LIBCPATH "/lib/libc.so.6"
#endif

typedef int (*funcptr)();

int
execl(const char *path, const char *arg, ...);
int
execle(const char *path, const char *arg, ...);

/**
 * 
 * @param filename
 * @param argv
 * @param envp
 * @return 
 */
int
execve(const  char  *filename, char *const argv [], char *const envp[]);

/**
 * Does what libc's `execv` does except adding auto-apt functionality (the
 * original `execv` is provided by `load_library_symbol`)
 * @param filename
 * @param argv
 * @return 
 */
int
execv(const  char  *filename, char *const argv []);

#undef open
int
open(const char *filename, int flags, ...);

#undef __libc_open
int
__libc_open(const char *filename, int flags, ...);

#undef open64
int
open64(const char *filename, int flags, ...);

#undef __libc_open64
int
__libc_open64(const char *filename, int flags, ...);

int 
access(const char *filename, int type);

int 
euidaccess(const char *filename, int type);

#undef __xstat
int
__xstat(int ver, const char *filename, struct stat *buf);

#undef __xstat64
struct stat64; /* XXX */
int
__xstat64(int ver, const char *filename, struct stat64 *buf);

#undef __lxstat
int
__lxstat(int ver, const char *filename, struct stat *buf);

#undef __lxstat64
int
__lxstat64(int ver, const char *filename, struct stat64 *buf);

#endif	/* AUTO_APT_H */

