/*-
 * pkgcdb2.c
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */

static char pkgcdb2_rcsid[] __attribute__((unused)) = "$Id: pkgcdb2.c,v 1.4 2000/07/13 16:34:30 ukai Exp $";

#include "pkgcdb2.h"
#include "strtab.h"
#include "mempool.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/file.h>
#include <fcntl.h>
#include <limits.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>
#include <time.h>

#ifdef DEBUG
int debug = 0;
#endif
int verbose = 0;
int quiet = 0;

typedef PathNodeTree PkgCDB;

#ifndef PKGCDB_AUTOAPT
PathNodeTree
pkgcdb_alloc()
{
    PathNodeTree pnt;
    StrTable st;
    st = strtab_alloc();
    if (st == NULL) {
	return NULL;
    }
    pnt = pathnode_alloc(st);
    return pnt;
}
#endif

#ifndef PKGCDB_AUTOAPT
void
pkgcdb_release(PathNodeTree pnt)
{
    StrTable st;
    assert(pnt != NULL);
    st = pathnode_strtab(pnt);
    assert(st != NULL);
    pathnode_release(pnt);
    strtab_release(st);
}
#endif

PathNodeTree
pkgcdb_load(char *dbfile, int str_margin, int pathnode_margin)
{
    int fd;
    char buf[8];
    PathNodeTree pnt;
    StrTable st;

    if (dbfile == NULL)
	return NULL;
    fd = open(dbfile, O_RDONLY);
    if (fd < 0) {
	return NULL;
    }
    if (read(fd, buf, sizeof(buf)) < 0) {
	return NULL;
    }
    if (strncmp(buf, PKGCDB_VERSION_TAG, sizeof(buf)) != 0) {
	MSG(("magic mismatch: %8s\n", buf));
	return NULL;
    }
    DPRINT(("fd=%d magic: %8s\n", fd, buf));
    st = strtab_restore(fd, str_margin);
    if (st == NULL) {
	MSG(("strtab read error\n"));
	close(fd);
	return NULL;
    }
    pnt = pathnode_restore(fd, st, pathnode_margin);
    if (pnt == NULL) {
	MSG(("pathnode read error\n"));
	close(fd);
	return NULL;
    }
    close(fd);
    return pnt;
}

#ifndef PKGCDB_AUTOAPT
int 
pkgcdb_save(char *dbfile, PathNodeTree pnt, int shrink)
{
    int fd;
    char buf[8];

    if (dbfile == NULL)
	return -1;
    if (pnt == NULL)
	return -1;
    fd = open(dbfile, O_WRONLY|O_TRUNC|O_CREAT, 0644);
    if (fd < 0)
	return fd;
    strncpy(buf, PKGCDB_VERSION_TAG, sizeof(buf));
    if (write(fd, buf, sizeof(buf)) < 0) {
	goto error;
    }
    if (strtab_dump(fd, pathnode_strtab(pnt), shrink) < 0) {
	goto error;
    }
    if (pathnode_dump(fd, pnt, shrink) < 0) {
	goto error;
    }
error:
    close(fd);
    return 0;
}
#endif


struct path_node *
pkgcdb_get(PathNodeTree pnt, char *file, char **matchfile, char **ext)
{
    char *p;
    char *filename;
    static char matchpath[PATH_MAX];
    char path[PATH_MAX];
    struct path_node *pn;
    struct path_node *match;

    DPRINT(("pkgcdb_get: %s\n", file));
    if (file[0] == '/') {
	file++;
    }
    filename = file;
    p = file;
    pn = pathnode_top(pnt);
    match = NULL;
    matchpath[0] = '\0';
    while (*p != '\0') {
	if (*p != '/' || p - file <= 1) {
	    p++;
	    continue;
	}
	p++;
	
	assert(p - file < PATH_MAX);
	strncpy(path, file, p - file);
	path[p - file] = '\0';
	pn = pathnode_retrieve(pnt, pn, path);
	if (pn == NULL) {
	    if (matchfile != NULL) {
		assert(file - filename + 2 < PATH_MAX);
		*matchfile = matchpath;
		matchpath[0] = '/';
		strncpy(matchpath + 1, filename, file - filename);
		matchpath[1 + file - filename] = '\0';
	    }
	    return match;
	}
	file = p;
	match = pn;
	assert(pn != NULL);
	assert(match != NULL);
	DPRINT(("match:%s[%s] rest>%s\n", 
		pathnode_pathname(pnt, pn), 
		pathnode_packagename(pnt, match), file));
    }
    DPRINT(("last?%s (%ld)\n", file, p - file));
    if (p - file >= 1) {
	pn = pathnode_retrieve(pnt, pn, file);
	if (pn == NULL) {
	    if (matchfile != NULL) {
		assert(file - filename + 2 < PATH_MAX);
		*matchfile = matchpath;
		matchpath[0] = '/';
		strncpy(matchpath + 1, filename, file - filename);
		matchpath[1 + file - filename] = '\0';
	    }
	    return match;
	}
    }
    if (matchfile != NULL) {
	*matchfile = matchpath;
	matchpath[0] = '/';
	strcpy(matchpath + 1, filename);
    }
    return pn;
}

#ifndef PKGCDB_AUTOAPT
struct pathlist {
    struct pathlist *next;
    char *path;
    int pathlen;
    char *pkg;
    struct path_node *n;
} *defpathlist = NULL;

int
pkgcdb_path_list_init(PathNodeTree pnt, char *file)
{
    FILE *fp;
    char buf[PATH_MAX];
    char *p, *pkg;
    struct pathlist *pl, *plp;
    int n = 0;

    if (file == NULL)
	return -1;
    VMSG(("paths.list loading from %s...", file));
    fp = fopen(file, "r");
    if (fp == NULL) {
	PERROR(("fopen paths.list"));
	return -1;
    }
    while (fgets(buf, sizeof(buf), fp) != NULL) {
	if (buf[0] == '#')
	    continue;
	if (buf[strlen(buf)-1] == '\n')
	    buf[strlen(buf)-1] = '\0';
	for (p = buf; *p != '\0'; p++) {
	    if (isspace(*p)) {
		*p++ = '\0';
		break;
	    }
	}
	for (; *p != '\0'; p++) {
	    if (!isspace(*p)) {
		break;
	    }
	}
	pkg = p;
	if (*p == '\0') {
	    pkg = DEFAULT_PATH_PACKAGE;
	} else {
	    for (; *p != '\0'; p++) {
		if (isspace(*p)) {
		    *p = '\0';
		    break;
		}
	    }
	}
	pl = (struct pathlist *)malloc(sizeof(struct pathlist));
	if (pl == NULL)
	    return -1;
	memset(pl, 0, sizeof(struct pathlist));
	pl->path = strdup(buf);
	pl->pathlen = strlen(pl->path);
	pl->pkg = strdup(pkg);
	pl->n = pkgcdb_put(pnt, pl->path, pl->pkg, NULL);

	if (defpathlist == NULL || pl->pathlen > defpathlist->pathlen) {
	    pl->next = defpathlist;
	    defpathlist = pl;
	} else {
	    for (plp = defpathlist; 
		 plp != NULL && plp->next != NULL; 
		 plp = plp->next) {
		if (pl->pathlen > plp->next->pathlen) {
		    pl->next = plp->next;
		    plp->next = pl;
		    break;
		}
	    }
	    if (plp != NULL && plp->next == NULL) {
		plp->next = pl;
	    }
	}
	n++;
    }
    VMSG(("%d done\n", n));
#ifdef DEBUG
    if (debug) {
	for (plp = defpathlist; plp != NULL; plp = plp->next) {
	    printf("%s %s\n", plp->path, plp->pkg);
	}
    }
#endif
    return 0;
}

static int
path_list_local_init(PathNodeTree pnt)
{
    struct pathlist *pl, *npl;
    struct stat st;
    char buf[PATH_MAX+2];
    for (pl = defpathlist; pl != NULL; pl = pl->next) {
	if (pl->path[pl->pathlen-1] == '/') {
	    snprintf(buf, PATH_MAX, "/%s", pl->path);
	    if (stat(buf, &st) == 0
		&& S_ISDIR(st.st_mode)) {
		npl = (struct pathlist *)malloc(sizeof(struct pathlist));
		if (npl == NULL)
		    continue;
		memset(npl, 0, sizeof(struct pathlist));
		npl->path = strdup(pl->path);
		npl->path[pl->pathlen-1] = '\0';
		npl->pathlen = pl->pathlen -1;
		npl->pkg = pl->pkg;
		npl->n = pkgcdb_put(pnt, npl->path, npl->pkg, NULL);
		npl->next = pl->next;
		pl->next = npl;
		pl = npl;
	    }
	}
    }
    return 0;
}

static struct pathlist *
path_list_check(char *file)
{
    struct pathlist *pl;
    int len;
    DPRINT(("path_list check? file=%s\n", file));
    len = strlen(file);
    for (pl = defpathlist; pl != NULL; pl = pl->next) {
	if (pl->n != NULL) {
	    if (len >= pl->pathlen &&
		strncmp(file, pl->path, pl->pathlen) == 0) {
		DPRINT(("path list match[%s]:%s <=> %s", 
			pl->pkg, pl->path, file));
		return pl;
	    }
	}
    }
    return NULL;
}
#endif

#ifndef PKGCDB_AUTOAPT
static char *
next_package(char *p, char **np)
{
    int i;
    static char package[PATH_MAX];
    
    *np = NULL;
    if (p == NULL || *p == '\0')
	return NULL;
    for (i = 0; p[i] != '\0'; i++) {
	if (p[i] == ',')
	    break;
	package[i] = tolower(p[i]);
    }
    if (p[i] == ',')
	*np = &p[i+1];
    package[i] = '\0';
    return package;
}
#endif

#ifndef PKGCDB_AUTOAPT
struct path_node *
pkgcdb_put(PathNodeTree pnt, char *file, char *pkg, int *nent)
{
    char *package, *dups;
    char *p;
    char path[PATH_MAX];
    struct path_node *pn, *npn;
    pkg_id pkgid, npkgid;
    static int inited = 0;
    static pkg_id pkg_defs;
#ifndef PKGCDB_AUTOAPT
    static int local_inited = 0;
    struct pathlist *pl;
#endif

    assert(pnt != NULL);
    if (!inited) {
	inited = 1;
	pkg_defs = pkg_intern(pathnode_strtab(pnt), DEFAULT_PATH_PACKAGE);
    }

    DPRINT(("put: <%s><%s>\n", file, pkg));
    package = next_package(pkg, &dups);
    pkgid = pkg_intern(pathnode_strtab(pnt), package);
    if (strncmp(file, "./", 2) == 0) {
	file += 2;
    }
    pn = pathnode_top(pnt);
    while (*file != '\0' && *file == '/')
	file++;

#ifndef PKGCDB_AUTOAPT
    if (strcmp(file, ".") == 0) {
	if (!local_inited) {
	    /* local mode */
	    local_inited = 1;
	    MSG(("local file list mode\n"));
	    path_list_local_init(pnt);
	}
	return NULL;
    }
    /* skip common dirs if available, it increase much performance */
    pl = path_list_check(file);
    if (pl != NULL) {
	file += pl->pathlen;
	pn = pl->n;
	if (strcmp(pl->pkg, DEFAULT_PATH_PACKAGE) == 0) {
	    DPRINT((" start=<%s> %p(%s)", 
		    file, pn, pathnode_pathname(pnt, pn)));
	} else {
	    /* all dir/files under local,man are ignored */
	    DPRINT((" ignore=<%s> %p(%s)",
		    file, pn, pathnode_pathname(pnt, pn)));
	    return pn;
	}
    }
#endif

    p = file;
    DPRINT((" file? %s\n", p));
    while (*p != '\0') {
	if (*p != '/' || p - file <= 1) {
	    p++;
	    continue;
	}
	p++;
	assert(p - file < PATH_MAX);
	strncpy(path, file, p - file);
	path[p - file] = '\0';
	DPRINT(("insert=<%s> %s|", path, pkg));
	npn = pathnode_insert(pnt, pn, path, pkgid);
	assert(npn != NULL);
	npkgid = pathnode_package(pnt, npn);
	if (nent != NULL)
	    (*nent)++;
	if (dups) {
	    for (package = next_package(dups, &dups);
		 package != NULL;
		 package = next_package(dups, &dups)) {
		pkgid = pkg_intern(pathnode_strtab(pnt), package);
		pathnode_chain(pnt, npn, pkgid);
	    }
	}
	if (pkg_cmp(&pkgid, &pkg_defs) != 0 /* default path? */
	    && pkg_cmp(&npkgid, &pkgid) == 0) {
	    return npn;
	}
	file = p;
	pn = npn;
	DPRINT(("next=<%s> %p(%s)", file, pn, pathnode_pathname(pnt, pn)));
    }
    if (p - file >= 1) {
	DPRINT(("insert=<%s> %s|", file, pkg));
	pn = pathnode_insert(pnt, pn, file, pkgid);
	if (dups) {
	    for (package = next_package(dups, &dups);
		 package != NULL;
		 package = next_package(dups, &dups)) {
		pkgid = pkg_intern(pathnode_strtab(pnt), package);
		pathnode_chain(pnt, pn, pkgid);
	    }
	}
    }
    return pn;
}
#endif

#ifndef PKGCDB_AUTOAPT
void
pkgcdb_del(PathNodeTree pnt, char *file, char *pkg, int *nent)
{
    char *package;
    char *p;
    int i;
    char path[PATH_MAX];
    struct path_node *pn, *npn;
    pkg_id pkgid, npkgid;
    struct pathlist *pl;

    assert(pnt != NULL);
    DPRINT(("put: <%s><%s>\n", file, pkg));
    package = malloc(strlen(pkg)+1);
    for (i = 0; pkg[i] != '\0'; i++) {
	if (pkg[i] == ',')
	    break;
	package[i] = tolower(pkg[i]);
    }
    package[i] = '\0';
    pkgid = pkg_intern(pathnode_strtab(pnt), package);
    free(package);
    if (strncmp(file, "./", 2) == 0) {
	file += 2;
    }
    pn = pathnode_top(pnt);
    while (*file != '\0' && *file == '/')
	file++;

    pl = path_list_check(file);
    if (pl != NULL) {
	file += pl->pathlen;
	pn = pl->n;
	if (strcmp(pl->pkg, DEFAULT_PATH_PACKAGE) == 0) {
	    DPRINT((" start=<%s> %p(%s)", 
		    file, pn, pathnode_pathname(pnt, pn)));
	} else {
	    /* all dir/files under local,man are ignored */
	    DPRINT((" ignore=<%s> %p(%s)",
		    file, pn, pathnode_pathname(pnt, pn)));
	    return;
	}
    }

    p = file;
    while (*p != '\0') {
	if (*p != '/' || p - file <= 1) {
	    p++;
	    continue;
	}
	p++;
	assert(p - file < PATH_MAX);
	strncpy(path, file, p - file);
	path[p - file] = '\0';
	npn = pathnode_retrieve(pnt, pn, path);
	if (npn == NULL)
	    return;
	for (pn = npn; pn != NULL; pn = pathnode_next(pnt, pn)) {
	    npkgid = pathnode_package(pnt, pn);
	    if (pkg_cmp(&npkgid, &pkgid) == 0) {
		pathnode_delete(pnt, pn);
		if (nent)
		    (*nent)--;
	    }
	}
	file = p;
	pn = npn;
	DPRINT(("next=<%s> %p(%s)", file, pn, pathnode_pathname(pnt, pn)));
    }
    if (p - file >= 1) {
	pn = pathnode_retrieve(pnt, pn, path);
	if (pn == NULL)
	    return;
	for (; pn != NULL; pn = pathnode_next(pnt, pn)) {
	    npkgid = pathnode_package(pnt, pn);
	    if (pkg_cmp(&npkgid, &pkgid) == 0) {
		pathnode_delete(pnt, pn);
		if (nent)
		    (*nent)--;
	    }
	}
    }
    return;
}
#endif

#ifndef PKGCDB_AUTOAPT

void
pkgcdb_traverse(PathNodeTree pnt, 
		void (*func)(PathNodeTree pnt, 
			     char *path, struct path_node *pn, void *arg),
		void *arg)
{
    struct path_node *pn;
    pn = pathnode_top(pnt);
    pathnode_traverse(pnt, NULL, pn, func, arg);
}
#endif
