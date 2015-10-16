/*
 * File:   main.c
 * Author: richter
 *
 * Created on 16. August 2015, 13:48
 */

#include <stdio.h>
#include <stdlib.h>
#include "pkgcdb/pkgcdb2.h"
#include "auto-apt-pkgcdb.h"

static void
usage(char* prog)
{
    fprintf(stderr, "usage: %s [option] [command ...]\n"
	    "command:\n"
	    "	put\n"
	    "	get filename\n"
	    "	list\n"
	    "	del package\n"
	    "option:\n"
	    "	-v	verbose\n"
	    "	-q	quiet\n"
	    "	-p	profile\n"
	    "	-d	debug\n"
	    "	-f dbfile\n"
	    "	-P paths.list\n"
	    ,  prog);
    exit(0);
}

/*
 *
 */
int main(int argc, char** argv) {
    char *cmd;
    extern char *optarg;
    extern int optind;
    int c;
    char *prog;
    int profile = 0, debug= 0;
    char *dbfile = PKGCDB_FILE;
    char *pathlist = PKGCDB_PATH_LIST;
    PathNodeTree pnt;
    struct timeval tv;
    int opterr, optopt;
    char *p;
    int ret;

    prog = argv[0];
    while ((c = getopt(argc, argv, "dvpqf:P:")) != EOF) {
	switch (c) {
	case 'v': verbose++; break;
	case 'p': profile++; break;
	case 'q': quiet++; break;
	case 'd': debug++; break;
	case 'f': dbfile = strdup(optarg); break;
	case 'P': pathlist = strdup(optarg); break;
	default:
	    usage(prog);
	}
    }
    if (argc < optind + 1) {
	usage(prog);
    }
    cmd = argv[optind];

    p = getenv("AUTO_APT_DB");
    if (p != NULL && *p == '/') {
	dbfile = strdup(p);
    }

    mempool_init();
    pkgtab_init();

    if (strcmp(cmd, "put") == 0) {
            ret = main_put(profile, tv, pnt, dbfile, pathlist);
    } else if (strcmp(cmd, "get") == 0) {
            char* key;
	if (argc < optind + 2) {
	    usage(prog);
	}
	key = argv[optind+1];
	ret = main_get(profile, tv, pnt, dbfile, key);
    } else if (strcmp(cmd, "del") == 0) {
	char *package;
	if (argc < optind + 2) {
	    usage(prog);
	}
	package = argv[optind+1];
	ret = main_del(pnt, dbfile, package);
    } else if (strcmp(cmd, "list") == 0) {
            ret = main_list(pnt, dbfile);
    } else {
	usage(prog);
    }

        return (ret);
}

