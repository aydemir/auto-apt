/*-
 * pkgcdb2.h 
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */

#ifndef _pkgcdb_h_
#define _pkgcdb_h_

#include "pathnode.h"

#define PKGCDB_VERSION_TAG	"AUTO_APT"
#define PKGCDB_VERSION_ID	"1"
#ifndef PKGCDB_FILE
#define PKGCDB_FILE	"/var/cache/auto-apt/pkgcontents.bin"
#define FILEDB_FILE	"/var/cache/auto-apt/pkgfiles.bin"
#define PKGCDB_PATH_LIST "/etc/auto-apt/paths.list"
#endif

#define DEFAULT_PATH_PACKAGE	"*"

#ifndef PKGCDB_AUTOAPT
PathNodeTree pkgcdb_alloc();
#endif
#ifndef PKGCDB_AUTOAPT
void pkgcdb_release(PathNodeTree pnt);
#endif
PathNodeTree pkgcdb_load(char *dbfile, 
				    int str_margin, int pathnode_margin);
#ifndef PKGCDB_AUTOAPT
int pkgcdb_save(char *dbfile, PathNodeTree pnt, int shrink);
#endif
#ifndef PKGCDB_AUTOAPT
int pkgcdb_path_list_init(PathNodeTree pnt, char *file);
#endif

struct path_node *pkgcdb_get(PathNodeTree pnt,
					char *flie, 
					char **matchfile, char **ext);
#ifndef PKGCDB_AUTOAPT
struct path_node *pkgcdb_put(PathNodeTree pnt,
					char *file, char *pkg, 
					int *nent);
#endif
#ifndef PKGCDB_AUTOAPT
void pkgcdb_del(PathNodeTree pnt, 
			   char *file, char *pkg, int *nent);
#endif

#ifndef PKGCDB_AUTOAPT
void pkgcdb_traverse(PathNodeTree pnt, 
				void (*func)(PathNodeTree pnt, 
					     char *path, 
					     struct path_node *pn, void *arg),
				void *arg);
#endif
#endif /* _pkgcdb_h_ */
