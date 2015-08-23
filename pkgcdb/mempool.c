/*-
 * mempool.c 
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */

static char mempool_rcsid[] __attribute__ ((unused)) = "$Id: mempool.c,v 1.4 2003/06/01 16:29:19 ukai Exp $";

#include "mempool.h"

void
mempool_init()
{
    return;
}

struct mempool *
mempool_alloc(struct mempool *mp, int count, size_t siz)
{
    int start;
    struct mempool *nmp;

    assert(count != 0);
    assert(siz != 0);
    if (mp == NULL) {
	start = 0;
    } else {
	start = mp->start + mp->count;
	assert(mp->siz == siz);
    }
    nmp = (struct mempool *)malloc(sizeof(struct mempool));
    if (nmp == NULL) {
	/* not enough memory */
	abort();
    }
    memset(nmp, 0, sizeof(struct mempool));
    nmp->mem = malloc(count * siz);
    if (nmp->mem == NULL) {
	/* not enough memory */
	abort();
    }
    memset(nmp->mem, 0, count * siz);
    nmp->m_next = mp;
    nmp->start = start;
    nmp->count = count;
    nmp->siz = siz;
    return nmp;
}

#ifndef PKGCDB_AUTOAPT
void
mempool_release(struct mempool *mp)
{
    struct mempool *cmp, *nmp;
    assert(mp != NULL);
    for (cmp = mp; cmp != NULL; cmp = nmp) {
	nmp = cmp->m_next;
	if (cmp->mem != NULL)
	    free(cmp->mem);
	cmp->mem = 0;
	cmp->start = 0;
	cmp->count = 0;
	cmp->siz = 0;
	cmp->m_next = NULL;
	free(cmp);
    }
    return;
}
#endif

#ifndef PKGCDB_AUTOAPT
void
mempool_shrink(struct mempool *mp, int num)
{
    assert(mp != NULL);
    assert(num < mp->count);
    mp->count -= num;
}
#endif

void *
mempool_mem(struct mempool *mp)
{
    assert(mp != NULL);
    assert(mp->mem != NULL);
    return mp->mem;
}

void *
mempool_mem_avail(struct mempool *mp, int avail)
{
    assert(mp != NULL);
    assert(mp->mem != NULL);
    assert(mp->count >= avail);
    if (avail == 0) {
	return NULL;
    }
    return ((char *)mp->mem + (mp->count - avail) * mp->siz);
}

int
mempool_index(struct mempool *mp, void *ptr)
{
    for (; mp != NULL; mp = mp->m_next) {
	if (mp->mem <= ptr 
	    && (char *)ptr < ((char *)mp->mem + (mp->count * mp->siz))) {
	    assert(((char *)ptr - (char *)mp->mem)%mp->siz == 0);
	    return ((char *)ptr - (char *)mp->mem)/mp->siz + mp->start;
	}
    }
    return -1;
}

void *
mempool_fetch(struct mempool *mp, int idx)
{
    for (; mp != NULL; mp = mp->m_next) {
	if (mp->start <= idx
	    && idx < mp->start + mp->count) {
	    return ((char *)mp->mem + (idx - mp->start) * mp->siz);
	}
    }
    return NULL;
}

#ifndef PKGCDB_AUTOAPT
static int
mempool_dump_rec(struct mempool *mp, int count, int siz, int fd, 
		 int (*serialize)(void *buf, 
				  void *ptr, int count, int siz, 
				  void *arg),
		 void *arg)
{
    int e;
    void *buf = NULL;
    int len = 0;
    if (mp == NULL) {
	DPRINT(("-header: begin\n"));
	DPRINT(("  count=%d\n", count));
	e = write(fd, &count, sizeof(int));
	if (e < 1) {
	    PERROR(("write count"));
	    return -1;
	}
	DPRINT(("  siz=%d\n", siz));
	e = write(fd, &siz, sizeof(int));
	if (e < 1) {
	    PERROR(("write siz"));
	    return -1;
	}
	DPRINT(("-header: end\n"));
	return 0;
    }
    e = mempool_dump_rec(mp->m_next, count + mp->count, mp->siz, fd, 
			 serialize, arg);
    if (e < 0) {
	PERROR(("write data"));
	return e;
    }
    DPRINT(("."));
    if (serialize != NULL) {
	buf = malloc(mp->count * mp->siz);
	len = serialize(buf, mp->mem, mp->count, mp->siz, arg);
	if (len < 0) {
	    DPRINT(("-serialize error? %d\n", len));
	    free(buf);
	    return -1;
	}
    } else {
	buf = mp->mem;
	len = mp->count * mp->siz;
    }
    DPRINT(("-data: %d * %d => (%d)\n", mp->count, (int)mp->siz, len));
    e = write(fd, buf, len);
    if (serialize != NULL)
	free(buf);
    if (e < 1) {
	PERROR(("write data"));
	return -1;
    }
    return 0;
}
#endif

#ifndef PKGCDB_AUTOAPT
int
mempool_dump(struct mempool *mp, int fd, 
	     int (*serialize)(void *buf, 
			      void *ptr, int count, int siz, 
			      void *arg),
	     void *arg)
{
    int e;
    DPRINT(("\ndump: mem=%p\n", mp));
    e = mempool_dump_rec(mp, 0, mp->siz, fd, serialize, arg);
    DPRINT((" done\n"));
    return e;
}
#endif

struct mempool *
mempool_restore(int fd, 
		void (*unserialize)(struct mempool *mp,
				    void *ptr, int count, int siz,
				    void *arg),
		void *arg,
		int margin)
{
    struct mempool *mp;
    int e;

    DPRINT(("restore "));
    mp = (struct mempool *)malloc(sizeof(struct mempool));
    if (mp == NULL) {
	DPRINT(("mempool_restore: not enough memory for mempool header\n"));
	abort();
    }
    memset(mp, 0, sizeof(struct mempool));
    e = read_data(fd, &mp->count, sizeof(int));
    if (e != sizeof(int)) {
	DPRINT(("read count fd:%d e:%d errno:%d\n", fd, e, errno));
	PERROR(("read count"));
	abort();
    }
    DPRINT(("count %d + %d, ", mp->count, margin));
    e = read_data(fd, &mp->siz, sizeof(int));
    if (e != sizeof(int)) {
	DPRINT(("read siz fd:%d e:%d errno:%d\n", fd, e, errno));
	PERROR(("read siz"));
	abort();
    }
    DPRINT(("siz %d, ", (unsigned int)mp->siz));
    mp->mem = malloc((mp->count + margin) * mp->siz);
    if (mp->mem == NULL) {
	DPRINT(("mempool_restore: not enough memory for mempool contents\n"));
	abort();
    }
    e = read_data(fd, mp->mem, mp->count * mp->siz);
    if (e != mp->count * mp->siz) {
	DPRINT(("read data fd:%d e:%d errno:%d\n", fd, e, errno));
	PERROR(("read data"));
	abort();
    }
    DPRINT(("..."));
    if (unserialize != NULL) {
	unserialize(mp, mp->mem, mp->count, mp->siz, arg);
    }
    mp->count += margin;
    DPRINT(("done\n"));
    return mp;
}
