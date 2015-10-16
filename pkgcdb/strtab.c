/*-
 * strtab.c
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */
static char strtab_rcsid[] __attribute__ ((unused)) = "$Id: strtab.c,v 1.4 2000/10/19 10:14:50 ukai Exp $";

#include "strtab.h"
#include "mempool.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <unistd.h>

#define HASH_SIZE		16381 /* 8191 */
#define STR_MEMPOOL_SIZE	(4096*10)

struct str_entry {
    str_id s_next;
    char data[0];
};

struct __strtab {
    str_id hashtab[HASH_SIZE];
    struct mempool *s_st;
    struct str_entry *st_next;
    int str_avail;

    /* stat */
    int str_alloc;
    int num_str;
    int num_hash;
    int num_conflicts;
    int num_max_depth;
};

static int str_hash(char *s);
static str_id str_add(StrTable st, char *s, str_id s_next);
static struct str_entry *str_get(StrTable st, str_id sid);

static int
str_hash(char *s)
{
    int *p;
    char *ss;
    int i, n, l;
    register int h = 0;
    char buf[sizeof(int)];

    l = strlen(s);
    ss = alloca(l+1);
    strcpy(ss, s);
    n = l/sizeof(int);
    p = (int *)ss;
    for (i = 0; i < n; i++) {
	h ^= *p++;
    }
    n = l - n*sizeof(int);
    if (n != 0) {
	memset(buf, 0, sizeof(buf));
	strncpy(buf+n, (char *)p, 4-n);
	p = (int *)buf;
	h ^= *p;
    }
    return h % HASH_SIZE;
}

static str_id
str_add(StrTable st, char *s, str_id s_next)
{
    int len;
    struct str_entry *se;
    assert(st != NULL);
    assert(s != NULL);

    len = ((sizeof(str_id) + strlen(s) + 1 + 3) & ~3); /* aligned */
    if (st->st_next == NULL || st->str_avail <= len) {
	st->s_st = mempool_alloc(st->s_st, STR_MEMPOOL_SIZE, 1);
	st->str_avail = STR_MEMPOOL_SIZE;
	st->st_next = (struct str_entry *)mempool_mem(st->s_st);
	st->str_alloc += STR_MEMPOOL_SIZE;
    }
    se = st->st_next;
    se->s_next = s_next;
    strcpy(se->data, s);
    st->st_next = (struct str_entry *)((char *)se + len);
    st->str_avail -= len;
    st->num_str++;
    return (str_id)mempool_index(st->s_st, se);
}

static struct str_entry *
str_get(StrTable st, str_id sid)
{
    struct str_entry *se;
    assert(st != NULL);

    se = mempool_fetch(st->s_st, sid);
    return se;
}

#ifndef PKGCDB_AUTOAPT
StrTable
strtab_alloc()
{
    int i;
    StrTable st;

    st = (StrTable)malloc(sizeof(struct __strtab));
    if (st == NULL)
	return NULL;
    memset(st, 0, sizeof(struct __strtab));

    st->num_str = 0;
    for (i = 0; i < HASH_SIZE; i++) {
	st->hashtab[i] = str_null;
    }
    st->s_st = mempool_alloc(NULL, STR_MEMPOOL_SIZE, 1);
    st->st_next = (struct str_entry *)mempool_mem(st->s_st);
    st->str_avail = STR_MEMPOOL_SIZE;
    st->str_alloc = STR_MEMPOOL_SIZE;

    str_add(st, "", str_null);	/* NULL entry */
    return st;
}
#endif

#ifndef PKGCDB_AUTOAPT
void
strtab_release(StrTable st)
{
    assert(st != NULL);
    assert(st->s_st != NULL);
    mempool_release(st->s_st);
    st->s_st = NULL;
    st->st_next = NULL;
    st->str_avail = 0;
    st->str_alloc = 0;
    free(st);
}
#endif

str_id
str_intern(StrTable st, char *string, int add)
{
    int h;
    str_id sid;

    assert(st != NULL);
    if (string == NULL || string[0] == '\0') {
	return str_null;
    }

    h = str_hash(string);
    if (st->hashtab[h] != str_null) {
	struct str_entry *se;
	int depth = 0;
	for (sid = st->hashtab[h], se = str_get(st, sid);
	     se != NULL && sid != str_null;
	     sid = se->s_next, se = str_get(st, sid)) {
	    depth++;
	    if (strcmp(se->data, string) == 0) {
		if (st->num_max_depth < depth) {
		    st->num_max_depth = depth;
		}
		return sid;
	    }
	}
	if (add)
	    st->num_conflicts++;
    } else {
	if (add)
	    st->num_hash++;
    }
    if (!add) {
	return str_null;
    }
    sid = str_add(st, string, st->hashtab[h]);
    st->hashtab[h] = sid;
    return sid;
}

char *
str_symbol(StrTable st, str_id sid)
{
    struct str_entry *se;
    assert(st != NULL);

    se = str_get(st, sid);
    if (se == NULL) {
	return ""; /* XXX */
    }
    return se->data;
}

/* dump/restore */
#ifndef PKGCDB_AUTOAPT
int
strtab_dump(int fd, StrTable st, int shrink)
{
    DPRINT(("strtab: %d strings, new %d alloc, %d bytes left\n"
	    "  hash %d used (%3.1f%%), %d conflicts, %d depth\n",
	    st->num_str, st->str_alloc, st->str_avail,
	    st->num_hash, (double)st->num_hash*100.0/HASH_SIZE,
	    st->num_conflicts, st->num_max_depth));
    if (shrink) {
	mempool_shrink(st->s_st, st->str_avail);
	st->str_avail = 0;
    }
    if (write(fd, st->hashtab, HASH_SIZE * sizeof(str_id)) < 1) {
	return -1;
    }
    DPRINT(("strtab: dump "));
    return mempool_dump(st->s_st, fd, NULL, NULL);
}
#endif


StrTable
strtab_restore(int fd, int str_margin)
{
    StrTable st;
    DPRINT(("strtab: restore "));
    st = (StrTable)malloc(sizeof(struct __strtab));
    if (st == NULL) {
	MSG(("not enough memory"));
	return NULL;
    }
    DPRINT(("hash %d ", HASH_SIZE));
    memset(st, 0, sizeof(struct __strtab));
    if (read_data(fd, st->hashtab, HASH_SIZE * sizeof(str_id)) < 1) {
	return NULL;
    }
    DPRINT(("done\n"));
    DPRINT((" symtab:"));
    st->s_st = mempool_restore(fd, NULL, NULL, str_margin);
    if (st->s_st == NULL) {
	free(st);
	return NULL;
    }
    st->st_next = mempool_mem_avail(st->s_st, str_margin);
    st->str_avail = str_margin;
    return st;
}
