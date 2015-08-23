/*-
 * pathnode.c
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */
static char pathnode_rcsid[] __attribute__ ((unused)) = "$Id: pathnode.c,v 1.3 2000/07/13 16:34:30 ukai Exp $";

#include "pathnode.h"
#include "mempool.h"
#include "strtab.h"
#include "pkgtab.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <assert.h>

#define PATH_MEMPOOL_SIZE		8192 /* (4096*10) */

struct path_node {
    str_id path;
    char *pathname;
    pkg_id package;
    struct path_node *left;
    struct path_node *right;
    struct path_node *down;
    struct path_node *dups;
};

struct __pathnode_tree {
    struct mempool *p_st;
    struct path_node *top;
    struct path_node *pn_next;
    int pn_avail;

    int pn_alloc;
    int num_pn;
    int num_left;
    int num_right;
    StrTable st;
    pkg_id ignore_pkg_id;
};


/* XXX: hack 
 * This tree structure show good performance if tree is balanced
 * so, it's important that cmp()'s ouptput is balanced 
 *
 * str_id: sequential number (memory index in strtab:mempool)
 * 
 */
static inline int cmp(str_id s0, str_id s1) {
#define h(s)	((s & 0xff) << 24 | (s & 0xff00) << 8 | \
		 (s & 0xff0000) >> 8 | (s & 0xff000000) >> 24)
    return h(s0) - h(s1);
#undef h
}

#ifndef PKGCDB_AUTOAPT
static struct path_node *
pathnode_add(PathNodeTree pnt, str_id pathid, pkg_id pkgid)
{
    struct path_node *pn;
    assert(pnt != NULL);
    assert(pnt->st != NULL);
    
    if (pnt->pn_next == NULL || pnt->pn_avail <= 0) {
	pnt->p_st = mempool_alloc(pnt->p_st, 
			     PATH_MEMPOOL_SIZE, sizeof(struct path_node));
	pnt->pn_next = (struct path_node *)mempool_mem(pnt->p_st);
	pnt->pn_avail = PATH_MEMPOOL_SIZE;
	pnt->pn_alloc += PATH_MEMPOOL_SIZE;
    }
    pn = pnt->pn_next;
    memset(pn, 0, sizeof(struct path_node));
    pn->path = pathid;
    pn->pathname = str_symbol(pnt->st, pathid);
    pn->package = pkgid;

    pnt->pn_next = pn + 1;
    pnt->pn_avail--;
    pnt->num_pn++;
    return pn;
}
#endif

#ifndef PKGCDB_AUTOAPT
PathNodeTree
pathnode_alloc(StrTable st)
{
    PathNodeTree pnt;
    pnt = (PathNodeTree)malloc(sizeof(struct __pathnode_tree));
    if (pnt == NULL)
	return NULL;
    memset(pnt, 0, sizeof(struct __pathnode_tree));
    pnt->st = st;
    pnt->num_pn = 0;
    pnt->p_st = mempool_alloc(NULL, 
			      PATH_MEMPOOL_SIZE, sizeof(struct path_node));
    pnt->pn_next = (struct path_node *)mempool_mem(pnt->p_st);
    pnt->pn_avail = PATH_MEMPOOL_SIZE;
    pnt->pn_alloc = PATH_MEMPOOL_SIZE;
    pnt->top = pathnode_add(pnt, str_null, pkg_null);
    return pnt;
}
#endif

#ifndef PKGCDB_AUTOAPT
void
pathnode_release(PathNodeTree pnt)
{
    assert(pnt != NULL);
    assert(pnt->p_st != NULL);
    /* don't release pnt->st for some case StrTable is shared */
    pnt->st = NULL;
    mempool_release(pnt->p_st);
    pnt->pn_next = NULL;
    pnt->pn_avail = 0;
    pnt->pn_alloc = 0;
    free(pnt);
}
#endif

#ifndef PKGCDB_AUTOAPT
void
pathnode_ignore_package(PathNodeTree pnt, char *pkg)
{
    assert(pnt != NULL);
    pnt->ignore_pkg_id = pkg_intern(pnt->st, pkg);
}
#endif

/* package should be lowered (case ignored) */
#ifndef PKGCDB_AUTOAPT
struct path_node *
pathnode_insert(PathNodeTree pnt, struct path_node *pn, 
		char *path, pkg_id pkgid)
{
    struct path_node *npn;
    int r;
    str_id pathid = str_intern(pnt->st, path, 1);

#if DEBUGDEBUG
    assert(pnt != NULL);
    DPRINT(("insert: %p(%s) <= [%s, %s]\n",
	    pn, pathnode_pathname(pnt, pn),
	    path, pkg_symbol(pnt->st, pkgid)));
#endif
    if (pn->down == NULL) {
	npn = pathnode_add(pnt, pathid, pkgid);
	pn->down = npn;
#if DEBUGDEBUG
	DPRINT(("add: %p(%s)->down = %p [%s, %s]\n", 
		pn, pathnode_pathname(pnt, pn),
		npn, path, pkg_symbol(pnt->st, pkgid)));
#endif
	return npn;
    }
    pn = pn->down;

    for (;;) {
	if (pn->pathname == NULL) pn->pathname = str_symbol(pnt->st, pn->path);
	r = cmp(pn->path, pathid);
#if DEBUGDEBUG
	DPRINT(("check? %s<=>%s [%d]\n", 
		path, pn->pathname, r));
#endif
	
	if (r == 0) {
	    /* collision */
	    return pathnode_chain(pnt, pn, pkgid);
	}

	if (r < 0) {
	    if (pn->left == NULL)
		break;
	    pn = pn->left;
	    pnt->num_left++;
	} else {
	    if (pn->right == NULL)
		break;
	    pn = pn->right;
	    pnt->num_right++;
	}
    }

    /* the item was not found in the tree */
    npn = pathnode_add(pnt, pathid, pkgid);
    if (r < 0) {
	npn->left = pn->left;
	pn->left = npn;
#if DEBUGDEBUG
	DPRINT(("add: %p(%s)->left = %p [%s, %s]\n",
		pn, pathnode_pathname(pnt, pn),
		npn, path, pkg_symbol(pnt->st, pkgid)));
#endif
    } else {
	npn->right = pn->right;
	pn->right = npn;
#if DEBUGDEBUG
	DPRINT(("add: %p(%s)->right = %p [%s, %s]\n",
		pn, pathnode_pathname(pnt, pn),
		npn, path, pkg_symbol(pnt->st, pkgid)));
#endif
    }
    return npn;
}
#endif

#ifndef PKGCDB_AUTOAPT
struct path_node *
pathnode_chain(PathNodeTree pnt, struct path_node *pn, pkg_id pkgid)
{
    struct path_node *p;
    assert(pn != NULL);
    if (pkg_cmp(&pkgid, &pn->package) == 0) {
	return pn;
    }
    for (p = pn->dups; p != NULL; p = p->dups) {
	if (pkg_cmp(&pkgid, &p->package) == 0) {
	    return pn;
	}
	if (pkg_cmp(&pnt->ignore_pkg_id, &p->package) == 0) {
	    return pn;
	}
    }
    if (pn->pathname == NULL) pn->pathname = str_symbol(pnt->st, pn->path);
    /* new one */
    p = pathnode_add(pnt, pn->path, pkgid);
    p->dups = pn->dups;
    pn->dups = p;
#if DEBUGDEBUG
    DPRINT(("add: %p->dups = %p [%s, %s]\n",
	    pn, p, pn->pathname, pkg_symbol(pnt->st, pkgid)));
#endif
    return pn;
}
#endif


struct path_node *
pathnode_retrieve(PathNodeTree pnt, struct path_node *pn, char* path)
{
    int r;
    str_id pathid = str_intern(pnt->st, path, 0);

    assert(pn != NULL);
    DPRINT(("retr: %p(%s) %s\n", 
	    pn, pathnode_pathname(pnt, pn), path));
    pn = pn->down;
    if (pn == NULL) {
	DPRINT(("no child?\n"));
	return pn;
    }

    for (;;) {
#if DEBUGDEBUG
	DPRINT(("check?%p %s<=>%s[%d]\n", pn,
		path, str_symbol(pnt->st, pn->path), pn->path));
#endif
	if (pn->pathname == NULL) pn->pathname = str_symbol(pnt->st, pn->path);
	r = cmp(pn->path, pathid);
	if (r == 0) {
	    DPRINT(("found - ok %p\n", pn));
	    return pn;
	}

	if (r < 0) {
#if DEBUGDEBUG
	    DPRINT(("go %p(%s)->left = %p\n", 
		    pn, pathnode_pathname(pnt, pn), pn->left));
#endif
	    if (pn->left == NULL)
		break;
	    pn = pn->left;
	} else {
#if DEBUGDEBUG
	    DPRINT(("go %p(%s)->right = %p\n", 
		    pn, pathnode_pathname(pnt, pn), pn->right));
#endif
	    if (pn->right == NULL)
		break;
	    pn = pn->right;
	}
    }
    DPRINT(("not found\n"));
    return NULL;
}

#ifndef PKGCDB_AUTOAPT
void pathnode_traverse(PathNodeTree pnt,
				  char *path, struct path_node *pn,
				  void (*func)(PathNodeTree pnt,
					       char *path, 
					       struct path_node *pn, 
					       void *arg),
				  void *arg)
{
    char *npath;
    if (path == NULL) {
	pathnode_traverse(pnt, "", pn->down, func, arg);
	return;
    }
    if (pn == NULL)
	return;
    if (pn->left != NULL)
	pathnode_traverse(pnt, path, pn->left, func, arg);
    if (pn->right != NULL)
	pathnode_traverse(pnt, path, pn->right, func, arg);
    func(pnt, path, pn, arg);
    npath = alloca(strlen(path) + strlen(pathnode_pathname(pnt, pn)) + 1);
    sprintf(npath, "%s%s", path, pathnode_pathname(pnt, pn));
    if (pn->down)
	pathnode_traverse(pnt, npath, pn->down, func, arg);
}
#endif

#ifndef PKGCDB_AUTOAPT
StrTable
pathnode_strtab(PathNodeTree pnt)
{
    assert(pnt != NULL);
    return pnt->st;
}
#endif

struct path_node *
pathnode_top(PathNodeTree pnt)
{
    assert(pnt != NULL);
    return pnt->top;
}

#ifndef PKGCDB_AUTOAPT
str_id
pathnode_path(PathNodeTree pnt, struct path_node *pn)
{
    assert(pn != NULL);
    return pn->path;
}
#endif

char *
pathnode_pathname(PathNodeTree pnt, struct path_node *pn)
{
    assert(pn != NULL);
    if (pn->pathname == NULL) pn->pathname = str_symbol(pnt->st, pn->path);
    return pn->pathname;
}

#ifndef PKGCDB_AUTOAPT
pkg_id
pathnode_package(PathNodeTree pnt, struct path_node *pn)
{
    assert(pn != NULL);
    return pn->package;
}
#endif

char *
pathnode_packagename(PathNodeTree pnt, struct path_node *pn)
{
    assert(pn != NULL);
    return pkg_symbol(pnt->st, pn->package);
}

#ifndef PKGCDB_AUTOAPT
struct path_node *
pathnode_next(PathNodeTree pnt, struct path_node *pn)
{
    assert(pn != NULL);
    return pn->dups;
}
#endif

#ifndef PKGCDB_AUTOAPT
void
pathnode_delete(PathNodeTree pnt, struct path_node *pn)
{
    assert(pn != NULL);
    pn->package = pkg_null;
}
#endif

#ifndef PKGCDB_AUTOAPT
static int
pathnode_serialize(void *buf, void *ptr, int count, int siz, void *arg)
{
    int i;
    PathNodeTree pnt = (PathNodeTree)arg;
    struct path_node *pn, *pn0;

    memcpy(buf, ptr, count * siz);
    pn = (struct path_node *)buf;
    pn0 = (struct path_node *)ptr;
    for (i = 0; i < count; i++) {
	pn->pathname = NULL; /* clear */
	if (pn->left) pn->left = (void *)mempool_fetch(pnt->p_st, mempool_index(pnt->p_st, pn0->left));
	if (pn->right) pn->right = (void *)mempool_fetch(pnt->p_st, mempool_index(pnt->p_st, pn0->right));
	if (pn->down) pn->down = (void *)mempool_fetch(pnt->p_st, mempool_index(pnt->p_st, pn0->down));
	if (pn->dups) pn->dups = (void *)mempool_fetch(pnt->p_st, mempool_index(pnt->p_st, pn0->dups));
	pn++; pn0++;
    }
    return count * siz;
}
#endif

static void
pathnode_unserialize(struct mempool*mp, void *ptr, int count, int siz,
		     void *arg)
{
    int i;
    /* PathNodeTree pnt = (PathNodeTree)arg; */
    struct path_node *pn = ptr;
    for (i = 0; i < count; i++) {
	if (pn->left) {
            pn->left = mempool_fetch(mp, mempool_index(mp, pn->left));
        }
	if (pn->right) {
            pn->right = mempool_fetch(mp, mempool_index(mp, pn->right));
        }
	if (pn->down) {
            pn->down = mempool_fetch(mp, mempool_index(mp, pn->down));
        }
	if (pn->dups) {
            pn->dups = mempool_fetch(mp, mempool_index(mp, pn->dups));
        }
	pn++;
    }
}

#ifndef PKGCDB_AUTOAPT
int
pathnode_dump(int fd, PathNodeTree pnt, int shrink)
{
    double rl, rr;
    if (pnt->num_left > pnt->num_right) {
	rl = (double)pnt->num_left / pnt->num_right;
	rr = 1.0;
    } else {
	rl = 1.0;
	rr = (double)pnt->num_right / pnt->num_left;
    }
    DPRINT(("pathnode: %d entries, new %d alloc, left %d entries\n"
	    "   left=%d times, right=%d times  (%f:%f)\n", 
	    pnt->num_pn, pnt->pn_alloc, pnt->pn_avail,
	    pnt->num_left, pnt->num_right, rl, rr));
    if (shrink) {
	mempool_shrink(pnt->p_st, pnt->pn_avail);
	pnt->pn_avail = 0;
    }
    DPRINT(("pathnode: dump "));
    return mempool_dump(pnt->p_st, fd, pathnode_serialize, pnt);
}
#endif

PathNodeTree
pathnode_restore(int fd, StrTable st, int margin)
{
    PathNodeTree pnt;
    DPRINT(("pathnode: "));
    pnt = (PathNodeTree)malloc(sizeof(struct __pathnode_tree));
    if (pnt == NULL) {
	DPRINT(("not enough memory\n"));
	return NULL;
    }
    memset(pnt, 0, sizeof(struct __pathnode_tree));
    pnt->st = st;
    pnt->p_st = mempool_restore(fd, pathnode_unserialize, pnt, margin);
    if (pnt->p_st == NULL) {
	free(pnt);
	return NULL;
    }
    pnt->top = NULL;
    pnt->pn_next = mempool_mem_avail(pnt->p_st, margin);
    pnt->pn_avail = margin;
    pnt->top = (struct path_node *)mempool_mem(pnt->p_st);
    return pnt;
}
