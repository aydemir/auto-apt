/*-
 * mempool.h
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */

#ifndef _mempool_h_
#define _mempool_h_

#include "debug.h"
#include <stdio.h>
#include <sys/types.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <errno.h>

void mempool_init();
struct mempool *mempool_alloc(struct mempool *mp, 
					 int count, size_t siz);
#ifndef PKGCDB_AUTOAPT
void mempool_release(struct mempool *mp);
#endif

#ifndef PKGCDB_AUTOAPT
void mempool_shrink(struct mempool *mp, int num);
#endif
void *mempool_mem(struct mempool *mp);
void *mempool_mem_avail(struct mempool *mp, int avail);
int mempool_index(struct mempool *mp, void *ptr);
void *mempool_fetch(struct mempool *mp, int idx);
#ifndef PKGCDB_AUTOAPT
int mempool_count(struct mempool *mp);
#endif

#ifndef PKGCDB_AUTOAPT
int mempool_dump(struct mempool *mp, int fd, 
			    int (*serialize)(void *buf, /* buf[count*siz] */
					     void *ptr, int count, int siz, 
					     void *arg),
			    void *arg);
#endif
struct mempool *mempool_restore(int fd,
					   void (*unserialize)(
					       struct mempool *mp,
					       void *ptr, 
					       int count, int siz,
					       void *arg),
					   void *arg,
					   int margin);

struct mempool {
    struct mempool *m_next;
    int start; /* start index */
    int count; /* count in this pool */
    size_t siz;	/* element size */
    void *mem;	/* (count * siz) bytes */
};

void
mempool_init();

struct mempool *
mempool_alloc(struct mempool *mp, int count, size_t siz);

#ifndef PKGCDB_AUTOAPT
void
mempool_release(struct mempool *mp);
#endif

#ifndef PKGCDB_AUTOAPT
void
mempool_shrink(struct mempool *mp, int num);
#endif

void *
mempool_mem(struct mempool *mp);

void *
mempool_mem_avail(struct mempool *mp, int avail);

int
mempool_index(struct mempool *mp, void *ptr);

void *
mempool_fetch(struct mempool *mp, int idx);

#ifndef PKGCDB_AUTOAPT
int
mempool_dump(struct mempool *mp, int fd, 
	     int (*serialize)(void *buf, 
			      void *ptr, int count, int siz, 
			      void *arg),
	     void *arg);
#endif

struct mempool *
mempool_restore(int fd, 
		void (*unserialize)(struct mempool *mp,
				    void *ptr, int count, int siz,
				    void *arg),
		void *arg,
		int margin);

#endif /* _mempool_h_ */
