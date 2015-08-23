/*-
 * pkgtab.c
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */
static char pkgtab_rcsid[] __attribute__((unused)) = "$Id: pkgtab.c,v 1.1 2000/07/12 03:42:47 ukai Exp $";

#include "pkgtab.h"
#include <string.h>
#include <limits.h>
#include <assert.h>
#include <stdlib.h>

pkg_id pkg_null;

void
pkgtab_init()
{
    static int inited = 0;
    int i;
    if (inited)
	return;
    inited = 1;
    for (i = 0; i < 3; i++)
	pkg_null.s[i] = str_null;
}

#ifndef PKGCDB_AUTOAPT
void
pkg_init(pkg_id *pid)
{
    assert(pid != NULL);
    memcpy(pid, &pkg_null, sizeof(pkg_id));
}
#endif

#ifndef PKGCDB_AUTOAPT
pkg_id
pkg_intern(StrTable st, char *pkg)
{
    char *p;
    char *buf = NULL;
    int i = 0;
    pkg_id pkgid;

    memset(&pkgid, 0, sizeof(pkgid));

    p = pkg;
    if (*p == '@') {
	/* special ! */
	p = p + strlen(p);
	goto done;
    }

    while (*p != '\0') {
	if (*p != '/' || p - pkg <= 1) {
	    p++;
	    continue;
	}
	p++;
	buf = (char *)malloc(p - pkg + 1);
	if (buf == NULL) {
	    abort();
	}
	strncpy(buf, pkg, p - pkg);
	buf[p - pkg] = '\0';
	pkgid.s[i] = str_intern(st, buf, 1);
	free(buf);
	i++;
	pkg = p;
	assert(i < 3);
    }
done:
    if (p - pkg >= 1) {
	pkgid.s[i] = str_intern(st, pkg, 1);
    }
    return pkgid;
}
#endif

char *
pkg_symbol(StrTable st, pkg_id pid)
{
    int len = 0;
    int i;
    char *p[3] = { NULL, NULL, NULL };
    static char *buf;

    for (i = 0; i < 3; i++) {
	if (pid.s[i] == 0) {
	    break;
	}
	p[i] = str_symbol(st, pid.s[i]);
	len += strlen(p[i]);
    }
    if (buf != NULL) {
	free(buf);
    }
    buf = (char *)malloc(len + 1);
    if (buf == NULL) {
	abort();
    }
    buf[0] = '\0';
    for (i = 0; i < 3; i++) {
	if (p[i]) {
	    strcat(buf, p[i]);
	} else {
	    break;
	}
    }
    return buf;
}

#ifndef PKGCDB_AUTOAPT
int
pkg_cmp(pkg_id *p0, pkg_id *p1)
{
    return memcmp(p0, p1, sizeof(pkg_id));
}
#endif
