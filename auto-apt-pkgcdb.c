/*-
 * auto-apt-db.c
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */

#include "pkgcdb/debug.h"
#include "pkgcdb/mempool.h"
#include "pkgcdb/pkgtab.h"
#include "pkgcdb/strtab.h"
#include "pkgcdb/pathnode.h"
#include "pkgcdb/pkgcdb2.h"
#include "auto-apt-pkgcdb.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>
#include <sys/time.h>
#include <unistd.h>
#include <limits.h>

static void
delete_pathnode_entry(PathNodeTree pnt,
		      char *path, struct path_node *pn, void *arg)
{
    struct path_node *npn;
    for (npn = pn; npn != NULL; npn = pathnode_next(pnt, npn)) {
	pkg_id pid = pathnode_package(pnt, npn);
	if (pkg_cmp(&pid, (pkg_id *)arg) == 0) {
	    MSG(("%s%s\t%s\n",
		 path, pathnode_pathname(pnt, pn),
		 pathnode_packagename(pnt, npn)));
	    pathnode_delete(pnt, npn);
	}
    }
}

static void
print_pathnode_entry(PathNodeTree pnt,
		     char *path, struct path_node *pn, void *arg)
{
    struct path_node *npn;
    printf("%s%s\t", path, pathnode_pathname(pnt, pn));
    for (npn = pn; npn != NULL; npn = pathnode_next(pnt, npn)) {
	if (npn != pn) {
	    printf(",");
	}
	printf("%s", pathnode_packagename(pnt, npn));
    }
    printf("\n");
}

static int
timer_start(struct timeval *tvp)
{
    struct timezone tz;
    return gettimeofday(tvp, &tz);
}

static double
timer_stop(struct timeval *tvp)
{
    struct timeval tv1;
    struct timezone tz;
    double t;

    gettimeofday(&tv1, &tz);
    if (tvp->tv_usec > tv1.tv_usec) {
	tv1.tv_sec--;
	tv1.tv_usec += 1000000;
    }
    tv1.tv_sec -= tvp->tv_sec;
    tv1.tv_usec -= tvp->tv_usec;
    return t = tv1.tv_sec + (double)tv1.tv_usec/1000000.0;
}

static int validchartab[256] = {
/* 0x00 */   0, 0, 0, 0, 0, 0, 0, 0,	0, 0, 0, 0, 0, 0, 0, 0,
/* 0x10 */   0, 0, 0, 0, 0, 0, 0, 0,    0, 0, 0, 0, 0, 0, 0, 0,
/* 0x20 */   0, 1, 0, 1, 1, 1, 0, 0,    0, 0, 0, 1, 1, 1, 1, 1,
/* 0x30 */   1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 0, 0, 1, 0, 0,
/* 0x40 */   1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 1, 1, 1, 1, 1,
/* 0x50 */   1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 0, 0, 0, 1, 1,
/* 0x60 */   0, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 1, 1, 1, 1, 1,
/* 0x70 */   1, 1, 1, 1, 1, 1, 1, 1,    1, 1, 1, 0, 0, 0, 1, 0,
/* 0x80-*/   0,
};

static int
is_validchar(int c)
{
    return validchartab[(c & 0x0FF)];
}

static char *
get_line(FILE *f)
{
    char *buf  = NULL;
    char *new_buf  = NULL;
    size_t buf_size = 0;
    size_t last = 0;

    while (!feof(f)) {
	buf_size = buf_size ? buf_size * 2 : BUFSIZ;
	new_buf = realloc(buf, buf_size);
	if (new_buf == NULL) {
	    free(buf);
	    return NULL;
	}
	buf = new_buf;
	if (fgets(buf + last, buf_size - last, f) == NULL) {
	    free(buf);
	    return NULL;
	}
	last = strlen(buf);
	if (buf[last - 1] == '\n')
	    return buf;
    }
    return buf;
}

int main_put(int profile, struct timeval tv, PathNodeTree pnt, char* dbfile, char* pathlist) {
        int n = 0, nent = 0;
	int op = '+';
	time_t t0, t1, t;
	double tt, ta, min_t, max_t;
	char *max_file, *buf = NULL;

	t1 = t0 = t = time(NULL);
	t0--;
	if (profile)
	    timer_start(&tv);
	pnt = pkgcdb_load(dbfile, 0, 0);
	if (pnt == NULL) {
	    pnt = pkgcdb_alloc();
	    if (pnt == NULL) {
		PERROR(("pkgcdb_init"));
		exit(1);
	    }
	}
	if (profile)
	    MSG(("db load: %f sec\n", timer_stop(&tv)));
	if (profile)
	    timer_start(&tv);
	pkgcdb_path_list_init(pnt, pathlist);
	pathnode_ignore_package(pnt, DEFAULT_PATH_PACKAGE);
	if (profile)
	    MSG(("path init: %f sec\n", timer_stop(&tv)));

	ta = 0.0;
	min_t = 9999.9; max_t = 0.0;
	max_file = NULL;
	while (!feof(stdin)) {
	    char *fname, *pkg;
	    char *p;
	    int nslash = 0;

	    buf = get_line(stdin);
	    if (buf == NULL)
		break;
	    if (profile)
		timer_start(&tv);
            int index = strlen(buf)-1;
	    if (buf[index] == '\n') {
		buf[index] = '\0';
	    }
	    DPRINT((">%s<\n", buf));
	    fname = buf;
	    switch (fname[0]) {
	    case ' ': goto next_line; break;
	    case '-': op = fname[0]; fname++; break;
	    case '+': op = fname[0]; fname++; break;
	    default:  op = '+'; break;
	    }
	    if (strncmp(fname, "./", 2) == 0) {
		fname += 2;
	    }
	    /* search space from backward */
	    for (pkg = fname + strlen(fname)-1;
		 !isspace(*pkg) && pkg > fname;
		 --pkg) {
		if (*pkg == '/') {
		    nslash++;
		    if (nslash >= 3) {
			VMSG(("too many /: %s\n", buf));
			goto next_line;
		    }
		} else if (*pkg == ',') {
		    nslash = 0;
		} else if (!is_validchar(*pkg)) {
		    VMSG(("invalid line: %s\n", buf));
		    goto next_line;
		}
	    }
	    if (isspace(*pkg))
		pkg += 1;
	    /* search end of fname */
	    for (p = pkg - 1; isspace(*p) && p > fname; --p)
		*p = '\0';

	    for (p = fname; *p; ++p) {
		if (!is_validchar(*p)) {
		    VMSG(("invalid fname: %s\n", buf));
		    goto next_line;
		}
	    }

	    DPRINT(("I: file=[%s] pkg=[%s]\n", fname, pkg));
	    t1 = time(NULL);
	    switch (op) {
	    case '+':
		pkgcdb_put(pnt, fname, pkg, &nent);
		n++;
		if (t1 > t0) {
		    MSG(("put: %d files,  %d entries\r", n, nent));
		    fflush(stdout);
		    t0 = t1;
		}
		break;
	    case '-':
		pkgcdb_del(pnt, fname, pkg, &nent);
		n++;
		if (t1 > t0) {
		    MSG(("put: %d files,  %d entries\r", n, nent));
		    fflush(stdout);
		    t0 = t1;
		}
		break;
	    }
	    if (profile) {
		tt = timer_stop(&tv);
		ta += tt;
		if (tt < min_t)
		    min_t = tt;
		if (tt > max_t) {
		    max_t = tt;
		    if (max_file)
			free(max_file);
		    max_file = strdup(fname);
		}
	    }

	next_line:
	    free(buf);
	}
	if (profile)
	    MSG(("total %f sec/%d = avg %f sec\n"
		 "     min = %f, max = %f<%s>\n",
		 ta, n, ta/n, min_t, max_t, max_file));
	if (profile)
	    timer_start(&tv);
	pkgcdb_save(dbfile, pnt, 1);
	if (profile)
	    MSG(("db save: %f sec\n", timer_stop(&tv)));
	t1 = time(NULL);
	MSG(("put: %d files,  %d entries done (%ld sec)\n",
	     n, nent, (long)(t1 - t)));
}

int main_get(int profile, struct timeval tv, PathNodeTree pnt, char* dbfile, char* key) {
        int ret;
	struct path_node *match;
	char *mfile = NULL, *ext = NULL;
        
        if (profile)
	    timer_start(&tv);
	pnt = pkgcdb_load(dbfile, 0, 0);
	if (pnt == NULL) {
	    PERROR(("pkgcdb_load"));
	    exit(1);
	}
	if (profile)
	    MSG(("db load: %f sec\n", timer_stop(&tv)));
	if (profile)
	    timer_start(&tv);
	match = pkgcdb_get(pnt, key, &mfile, &ext);
	if (profile)
	    MSG(("db search: %f sec\n", timer_stop(&tv)));
	if (profile)
	    timer_start(&tv);
	if (match) {
	    struct path_node *pn;
	    int found = 0;
	    for (pn = match; pn != NULL; pn = pathnode_next(pnt, pn)) {
		if (pn != match)
		    printf(",");
		printf("%s", pathnode_packagename(pnt, pn));
		if (strlen(pathnode_packagename(pnt, pn)) > 0) {
		    found = 1;
		}
	    }
	    if (found)
		printf("\n");
	    if (mfile) {
		VMSG(("%s\n", mfile));
	    }
	    if (ext) {
		VMSG(("%s\n", ext));
		free(ext);
	    }
	    if (found == 0)
		ret = 1; /* not found */
	} else {
	    ret = 1; /* not found */
	}
	if (profile)
	    MSG(("output: %f sec\n", timer_stop(&tv)));
        return ret;
}

int main_del(PathNodeTree pnt, char* dbfile, char* package) {
	pkg_id delpkg;
pnt = pkgcdb_load(dbfile, 0, 0);
	if (pnt == NULL) {
	    PERROR(("pkgcdb_load"));
	    exit(1);
	}
	delpkg = pkg_intern(pathnode_strtab(pnt), package);
	pkgcdb_traverse(pnt, delete_pathnode_entry, &delpkg);
	pkgcdb_save(dbfile, pnt, 1);
}

int main_list(PathNodeTree pnt, char* dbfile) {
        pnt = pkgcdb_load(dbfile, 0, 0);
	if (pnt == NULL) {
	    PERROR(("pkgcdb_load"));
	    exit(1);
	}
	pkgcdb_traverse(pnt, print_pathnode_entry, NULL);
}
