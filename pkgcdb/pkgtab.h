/*-
 * pkgtab.h
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */

#ifndef _pkgtab_h_
#define _pkgtab_h_
static char pkgtab_h_rcsid[] __attribute__((unused)) = "$Id: pkgtab.h,v 1.1 2000/07/12 03:42:47 ukai Exp $";

#include "debug.h"
#include "strtab.h"

typedef struct {
    str_id s[3];	/* [<dist>/]<sect>/<package> */
} pkg_id;

pkg_id pkg_null;

void pkgtab_init();
#ifndef PKGCDB_AUTOAPT
void pkg_init(pkg_id *pid);
#endif
#ifndef PKGCDB_AUTOAPT
pkg_id pkg_intern(StrTable st, char *pkg);
#endif
char *pkg_symbol(StrTable st, pkg_id pid);
#ifndef PKGCDB_AUTOAPT
int pkg_cmp(pkg_id *p0, pkg_id *p1);
#endif

/* dump/restore - by strtab */

#endif /* _pkgtab_h_ */
