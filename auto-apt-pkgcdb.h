/* 
 * File:   auto-apt-pkgcdb.h
 * Author: richter
 *
 * Created on 16. August 2015, 23:30
 */

#ifndef AUTO_APT_PKGCDB_H
#define	AUTO_APT_PKGCDB_H

int main_put(int profile, struct timeval tv, PathNodeTree pnt, char* dbfile, char* pathlist);

int main_get(int profile, struct timeval tv, PathNodeTree pnt, char* dbfile, char* key);

int main_del(PathNodeTree pnt, char* dbfile, char* package);

int main_list(PathNodeTree pnt, char* dbfile);

#ifdef	__cplusplus
extern "C" {
#endif




#ifdef	__cplusplus
}
#endif

#endif	/* AUTO_APT_PKGCDB_H */

