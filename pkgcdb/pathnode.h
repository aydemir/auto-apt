/*-
 * pathnode.h
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */

#ifndef _pathnode_h_
#define _pathnode_h_

#include "debug.h"
#include "strtab.h"
#include "pkgtab.h"

typedef struct __pathnode_tree *PathNodeTree;

#ifndef PKGCDB_AUTOAPT
PathNodeTree pathnode_alloc(StrTable st);
#endif
#ifndef PKGCDB_AUTOAPT
void pathnode_release(PathNodeTree pnt);
#endif

#ifndef PKGCDB_AUTOAPT
void pathnode_ignore_package(PathNodeTree pnt, char *pkg);
#endif

#ifndef PKGCDB_AUTOAPT
struct path_node *pathnode_insert(PathNodeTree pnt,
					     struct path_node *pn,
					     char *path, pkg_id pkgid);
#endif
#ifndef PKGCDB_AUTOAPT
struct path_node *pathnode_chain(PathNodeTree pnt,
					    struct path_node *pn,
					    pkg_id pkgid);
#endif
struct path_node *pathnode_retrieve(PathNodeTree pnt,
					       struct path_node *pn,
					       char *path);
#ifndef PKGCDB_AUTOAPT
void pathnode_traverse(PathNodeTree pnt,
				  char *path, struct path_node *pn,
				  void (*func)(PathNodeTree pnt,
					       char *path, 
					       struct path_node *pn, 
					       void *arg),
				  void *arg);
#endif

#ifndef PKGCDB_AUTOAPT
StrTable pathnode_strtab(PathNodeTree pnt);
#endif
struct path_node *pathnode_top(PathNodeTree pnt);
#ifndef PKGCDB_AUTOAPT
str_id pathnode_path(PathNodeTree pnt, struct path_node *pn);
#endif
char *pathnode_pathname(PathNodeTree pnt, struct path_node *pn);
#ifndef PKGCDB_AUTOAPT
pkg_id pathnode_package(PathNodeTree pnt, struct path_node *pn);
#endif
char *pathnode_packagename(PathNodeTree pnt, 
				      struct path_node *pkg_id);
#ifndef PKGCDB_AUTOAPT
struct path_node *pathnode_next(PathNodeTree pnt, 
					   struct path_node *pn);
#endif
#ifndef PKGCDB_AUTOAPT
void pathnode_delete(PathNodeTree pnt, struct path_node *pn);
#endif

#ifndef PKGCDB_AUTOAPT
int pathnode_dump(int fd, PathNodeTree pnt, int shrink);
#endif
PathNodeTree pathnode_restore(int fd, StrTable st, int margin);

#endif /* _pathnode_h_ */
