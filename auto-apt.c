/*
 * auto-apt.so
 * on demand package installation tool
 * Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
 * GPL
 *
 */
#include "auto-apt.h"

static char auto_apt_rcsid[] __attribute__ ((unused)) = "$Id: auto-apt.c,v 1.28 2000/12/04 14:27:47 ukai Exp $";

static int apt_hook[NUM_APT_HOOK];

static char *pkgcdb_file = PKGCDB_FILE;
static PathNodeTree pkgcdb_tree = NULL;

static char *filedb_file = NULL;
static PathNodeTree filedb_tree = NULL;

#ifdef __alpha__
#define LIBCPATH "/lib/libc.so.6.1"
#else
#define LIBCPATH "/lib/libc.so.6"
#endif

static struct realfunctab {
    char *name;
    funcptr fptr;
} rftab[] = {
    {"execve", NULL},	/* execve(2) */
    /* XXX: execl(3), execle(3) */
    /* execlp(3)->execvp(3) */
    /* execvp(3)->execv(3) */
    {"execv", NULL},
    {"open", NULL},
    {"open64", NULL},
#if 1
    {"__libc_open", NULL},
    {"__libc_open64", NULL},
#endif
    {"access", NULL},
    {"euidaccess", NULL},
    {"__xstat", NULL},
    {"__xstat64", NULL},
    {"__lxstat", NULL},
    {"__lxstat64", NULL},
    {NULL, NULL}
};

static struct path_node *filename2package(PathNodeTree pnt, 
					  const char *filename, 
					  char **detect);
static int command_line_name(char *buf, size_t siz);

static funcptr 
load_library_symbol(char *name) 
{
    void *handle;
    const char *error;
    struct realfunctab *ft;
    char *libcpath = NULL;

    for (ft = rftab; ft->name; ft++) {
	if (strcmp(name, ft->name) == 0) {
	    if (ft->fptr != NULL) {
		return ft->fptr;
	    }
	    break;
	}
    }
    if (ft->name == NULL) {
	DPRINT(("func:%s not found\n", name));
	return NULL;
    }

    if ((libcpath = getenv("LIBC_PATH")) == NULL)
	libcpath = LIBCPATH;

    handle = dlopen (libcpath, RTLD_LAZY);
    if (!handle) {
	DPRINT(("%s", dlerror()));
	return NULL;
    }
    ft->fptr = dlsym(handle, ft->name);
    if ((error = dlerror()) != NULL)  {
	DPRINT(("dysym(%s)=%s\n", ft->name, error));
	ft->fptr = NULL;
    }
    dlclose(handle);
    return ft->fptr;
}

static int
open_internal(const char *filename, int flag, int mode)
{
    funcptr __open = load_library_symbol("__libc_open64");
    if (__open == NULL) __open = load_library_symbol("__libc_open");
    if (__open == NULL) __open = load_library_symbol("open64");
    if (__open == NULL) __open = load_library_symbol("open");
    if (__open == NULL)
	return -1;
    return __open(filename, flag, mode);
}


static char *
auto_apt_conf_var(char *name, char *def)
{
    char *p = getenv(name);
    if (p == NULL)
	return def;
    if (*p == '\0')
	return def;
    return p;
}

static int
auto_apt_conf_switch(char *name)
{
    char *p = auto_apt_conf_var(name, NULL);
    if (p == NULL)
	return 0;
    if (strcasecmp(p, "no") == 0 || strcasecmp(p, "off") == 0)
	return 0;
    return 1;
}

static char *detectdb_file = NULL;
static char *detectdb_lockfile = NULL;

/* detectdb will access from multiple process, need lock */
static int
detectdb_lock()
{
    struct flock fl;
    int fd = open_internal(detectdb_lockfile, 
                           O_RDWR|O_CREAT|O_TRUNC, 0660);
    if (fd == -1) {
        abort();
        return -1;
    }
again:
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if (fcntl(fd, F_SETLK, &fl) == -1) {
        if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EACCES)
            goto again; 
        close(fd);
        abort();
        return -1;
    }
    return fd;
    abort();
    return -1;
}

static void
detectdb_unlock(int fd)
{
    struct flock fl;
    if (fd >= 0) {
        fl.l_type = F_UNLCK;
        fl.l_whence = SEEK_SET;
        fl.l_start = 0;
        fl.l_len = 1;
        if (fcntl(fd, F_SETLK, &fl) == -1) {
            /* cannot happen? */
        }
        close(fd);
    }
    unlink(detectdb_lockfile);
}

static int
detect_package(const char *filename, const char *func)
{
    int e = 0;
    struct path_node *pn;
    char *file = NULL;
    char *p = NULL;
    char pkg[PATH_MAX];
    int lockfd;

    if (detectdb_file == NULL)
	return -1;
    if (filedb_tree == NULL)
	return -1;
    DPRINT(("detect: %s\n", filename));
    /* ignore db files */
    if (strcmp(filename, pkgcdb_file) == 0) {
	return 0;
    } else if (strcmp(filename, detectdb_file) == 0) {
	return 0;
    } else if (detectdb_lockfile &&
	       strcmp(filename, detectdb_lockfile) == 0) {
	return 0;
    } else if (filedb_file &&
	       strcmp(filename, filedb_file) == 0) {
	return 0;
    }
    
    DPRINT(("check by filedb: %s\n", filename));
    pn = filename2package(filedb_tree, filename, &file);
    if (pn == NULL) {
	DPRINT(("no package file, ignore\n"));
	return -1;
    }

    p = pathnode_packagename(filedb_tree, pn);
    if (p == NULL || *p == '!' || *p == '@') {
	DPRINT(("dummy package, ignore\n"));
	return -1;
    }
    strncpy(pkg, p, PATH_MAX-1);
    DPRINT(("put it to detect file: %s => %s %s\n", filename, file, pkg));
    if (file == NULL)
	file = (char *)filename;

    /* XXX: need lock */
    lockfd = detectdb_lock();
    if (lockfd < 0)
	goto done;
    {
	char cmd[PATH_MAX];
	int fd = open_internal(detectdb_file, 
			       O_WRONLY|O_APPEND|O_CREAT, 0644);
	if (fd < 0) {
	    abort();
	    goto done;
	}
	/* <file> <package> <func>(<filename>) <cmd> */
	write(fd, file, strlen(file));
	write(fd, "\t", 1);
	write(fd, pkg, strlen(pkg));
	write(fd, "\t", 1);
	write(fd, func, strlen(func));
	write(fd, "(", 1);
	write(fd, filename, strlen(filename));
	write(fd, ")\t", 2);
	command_line_name(cmd, PATH_MAX-1);
	write(fd, cmd, strlen(cmd));
	write(fd, "\n", 1);
	close(fd);
    }
done:
    /* XXX: need unlock */
    detectdb_unlock(lockfd);
    return e;
}

/* _init() ? */
static void
auto_apt_setup()
{
    static int inited = 0;

    if (!inited) {
	char *p;
	int i;
	inited = 1;
	mempool_init();
	pkgtab_init();

	for (i = 0; i < NUM_APT_HOOK; i++) {
	    apt_hook[i] = 0;
	}
	p = auto_apt_conf_var("AUTO_APT_HOOK", "none");
	if (p != NULL) {
	    apt_hook[APT_HOOK_EXEC] = (strstr(p, "exec") != NULL);
	    apt_hook[APT_HOOK_OPEN] = (strstr(p, "open") != NULL);
	    apt_hook[APT_HOOK_ACCESS] = (strstr(p, "access") != NULL);
	    apt_hook[APT_HOOK_STAT] = (strstr(p, "stat") != NULL);
	    if (strcmp(p, "all") == 0) {
		for (i = 0; i < NUM_APT_HOOK; i++) {
		    apt_hook[i] = 1;
		}
	    }
	    if (strstr(p, "none") != NULL) {
		for (i = 0; i < NUM_APT_HOOK; i++) {
		    apt_hook[i] = 0;
		}
	    }
	}
        if (auto_apt_conf_switch("AUTO_APT_DEBUG")) {
#ifdef DEBUG
	    debug = 1;
#endif
	}
	if (auto_apt_conf_switch("AUTO_APT_QUIET")) {
	    quiet = 1;
	}
	if (auto_apt_conf_switch("AUTO_APT_VERBOSE")) {
	    verbose = 1;
	}

	p = auto_apt_conf_var("AUTO_APT_DB", PKGCDB_FILE);
	if (p != NULL && *p == '/') {
	    pkgcdb_file = strdup(p);
	    pkgcdb_tree = pkgcdb_load(pkgcdb_file, 0, 0);
	    if (pkgcdb_tree == NULL) {
		VMSG(("auto-apt %s load failed, auto-apt off\n", pkgcdb_file));
		unsetenv("LD_PRELOAD");
		free(pkgcdb_file);
		pkgcdb_file = NULL;
		/* should exit() here ?*/
	    }
	}
#ifdef USE_DETECT
	p = auto_apt_conf_var("AUTO_APT_DETECT", NULL);
	if (p != NULL) {
	    detectdb_file = strdup(p);
	    DPRINT(("detectdb_file=%s\n", detectdb_file));
	    detectdb_lockfile = malloc(strlen(detectdb_file) + 5);
	    if (detectdb_lockfile != NULL) {
		sprintf(detectdb_lockfile, "%s.lck", detectdb_file);
		DPRINT(("lockfile=%s\n", detectdb_lockfile));
	    }
	}
	if (detectdb_file != NULL) {
	    p = auto_apt_conf_var("AUTO_APT_FILEDB", FILEDB_FILE);
	    if (p != NULL) {
		filedb_file = strdup(p);
		DPRINT(("filedb: %s\n", filedb_file));
		filedb_tree = pkgcdb_load(filedb_file, 0, 0);
		if (filedb_tree == NULL) {
		    VMSG(("auto-apt filedb %s not loaded, use %s\n", 
			 filedb_file, pkgcdb_file));
		    free(filedb_file);
		    filedb_file = NULL;
		} else {
		    VMSG(("auto-apt filedb: %s\n", filedb_file));
		}
	    }
	}
#endif
    }
    return;
}

static struct path_node *
filename2package(PathNodeTree pnt, const char *filename, char **detected_file)
{
    funcptr __stat = NULL;
    char *fname = strdup(filename);
    struct path_node *pn = NULL;
#ifdef __USE_LARGEFILE64
    struct stat64 st;
#else
    struct stat st;
#endif
    int n = 0;

 again:
    if (++n >= 16)	/* too much */
	goto done;
    if (fname == NULL)
	goto done;
    DPRINT(("fname:%s\n", fname));

    /* XXX: normalize fname? */
    if (strstr(fname, "/../")) {
	goto done;
    }
    if (fname[0] != '/') {
	goto done;
    }

    if (detected_file == NULL) {
#ifdef __USE_LARGEFILE64
	__stat = load_library_symbol("__xstat64");
#endif
	if (__stat == NULL) __stat = load_library_symbol("__xstat");
	if (__stat == NULL) __stat = stat;
	if (__stat == NULL)
	    goto no_file;
	if (__stat(_STAT_VER, fname, &st) == 0) {
	    DPRINT(("stat(%s) ok: %0x\n", fname, st.st_mode));
	    if (S_ISREG(st.st_mode)) {
		char magic[4096];
		/* filename found */
		int len;
		int fd = open_internal(fname, O_RDONLY, 0);
		if (fd < 0) {
		    DPRINT(("fname:%s open fail\n", fname));
		    goto no_file;
		}
		if ((len = read(fd, magic, sizeof magic -1)) > 0) {
		    magic[len] = '\0';
		    if (len > 2 && magic[0] == '#' && magic[1] == '!') {
			char *p, *q;
			// DPRINT(("magic[%d]=[%s]\n", len, magic));
			p = &magic[2];
			q = strchr(p, ' ');
			if (q == NULL) {
			    q = strchr(p, '\n');
			    if (q == NULL) {
				close(fd);
				goto done;
			    }
			}
			*q = '\0';
			DPRINT(("magic#! found:%s\n", p));
			free(fname);
			fname = strdup(p);
			close(fd);
			goto again;
		    }
		}
		DPRINT(("magic check done\n"));
		close(fd);
	    }
	}
    }

 no_file:
    {
      char *mfile = NULL;

      pn = pkgcdb_get(pnt, fname, &mfile, NULL);
      if (pn && mfile) {
	  DPRINT(("matched: file=%s\n", mfile));

	  if (detected_file) {
	      *detected_file = mfile;
	  } else {
	      char list[PATH_MAX];
	      if (__stat && __stat(_STAT_VER, mfile, &st) == 0) {
		  /* already exists */
		  DPRINT(("package:%s seems already installed, ignored\n",
			  pathnode_packagename(pnt, pn)));
		  pn = NULL;
		  goto done;
	      }

	      /* 
	       * check /var/lib/dpkg/info/<package>.list whether 
	       * this package is already installed
	       */
	      snprintf(list, sizeof(list)-1, "/var/lib/dpkg/info/%s.list", 
		       pathnode_packagename(pnt, pn));
	      if (__stat && __stat(_STAT_VER, list, &st) == 0) {
		  /* exists */
		  int len;
		  int fd = open_internal(list, O_RDONLY, 0);
		  if (fd < 0) {
		      /* ??? */
		      goto done;
		  }
		  if ((len = read(fd, list, sizeof(list)-1)) > 0) {
		      if (strncmp(list, "/.\n", 3) == 0) {
			  /* installed! */
			  close(fd);
			  return NULL;
		      }
		  }
		  close(fd);
	      }
	  }
      }
    }
    DPRINT(("done; package=%s\n", 
	    pn ? pathnode_packagename(pnt, pn) : "(null)"));
 done:
    if (fname)
	free(fname);
    return pn;
}

static int
command_line_name(char *buf, size_t siz)
{
    int len;
    int pfd;
    memset(buf, 0, siz);
    pfd = open_internal("/proc/self/cmdline", O_RDONLY, 0);
    if (pfd < 0) {
	return -1;
    }
    if ((len = read(pfd, buf, siz-1)) > 0) {
	int i;
	char *p;
	for (i = 0; i < len; i++) {
	    if (buf[i] == '\0') buf[i] = ' '; /* XXX */
	}
	buf[len] = '\0';
	/* remove tail spaces */
	for (p = &buf[len-1]; p > &buf[0]; --p) {
	    if (*p == ' ')
		*p = '\0';
	    else
		break;
	}
    }
    close(pfd);
    return 0;
}

#define DPKG_LOCKFILE	"/var/lib/dpkg/lock"

static int
check_dpkglock()
{
    int fd;
    struct flock fl;
    if (getuid()) {
	/* not running root */
	return 0;
    }
    fd = open_internal(DPKG_LOCKFILE, O_RDWR|O_CREAT|O_TRUNC, 0660);
    if (fd == -1)
	return 0;
    fl.l_type = F_WRLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if (fcntl(fd, F_SETLK, &fl) == -1) {
	close(fd);
	if (errno == EWOULDBLOCK || errno == EAGAIN || errno == EACCES)
	    return 1;
	/* unexpected error? */
	return 1;
    }
    fl.l_type = F_UNLCK;
    fl.l_whence = SEEK_SET;
    fl.l_start = 0;
    fl.l_len = 1;
    if (fcntl(fd, F_SETLK, &fl) == -1) {
	/* cannot hannen? */
    }
    close(fd);
    return 0;
}

static int
apt_get_install_for_file(const char *filename)
{
    int status;
    pid_t pid;
    struct path_node *pn;
    char *package;
    int ok = 0;

    if (pkgcdb_tree == NULL)
	return -1;
    pn = filename2package(pkgcdb_tree, filename, NULL);
    if (pn != NULL) {
	char pkg[PATH_MAX];
	char *path, *opath;
	char *logfile = getenv("AUTO_APT_LOG");
	int simulate = auto_apt_conf_switch("AUTO_APT_SIMULATE");
	int auto_apt_nobg = auto_apt_conf_switch("AUTO_APT_NOBG");
	char cmdname[PATH_MAX];
	int logfd = -1;
	char *accept = getenv("AUTO_APT_ACCEPT");
#if 0
	int auto_apt_x = auto_apt_conf_switch("AUTO_APT_X");
#endif

	if (accept && strcmp(accept, "none") == 0) {
	    simulate = 1;
	}
	if (logfile) {
	    logfd = open_internal(logfile, O_WRONLY|O_APPEND|O_CREAT, 0644);
	}
	opath = getenv("PATH");
	path = malloc(strlen("PATH=/usr/sbin:/sbin:") + strlen(opath) + 2);
	if (path)
	    sprintf(path, "PATH=/usr/sbin:/sbin:%s", opath);

	package = pathnode_packagename(pkgcdb_tree, pn);
	strncpy(pkg, package, PATH_MAX-1);
	command_line_name(cmdname, sizeof(cmdname));

	DPRINT(("install: %s by %s\n", pkg, cmdname));
	switch (pkg[0]) {
	case '!': case '*': case ' ': 
	    /* ignore! */
	    goto done;
	}
#if 0 /* check done in auto-apt-installer */
	for (pkg = package; *pkg; pkg++) {
	    if (*pkg != '/')
		continue;
	    *pkg++ = '\0';
	    if (accept) {
		if (strstr(accept, "non-US") == NULL 
		    && strcmp(package, "non-US") == 0) {
		    if (logfd >= 0) {
			write(logfd, "Ignore[non-US]:", 15);
			write(logfd, pkg, strlen(pkg));
			write(logfd, "\tfile:", 6);
			write(logfd, filename, strlen(filename));
			write(logfd, "\tby:", 4);
			write(logfd, cmdname, strlen(cmdname));
			write(logfd, "\n", 1);
		    }
		    goto done;
		}
		if (strstr(accept, "non-free") == NULL
		    && strcmp(package, "non-free") == 0) {
		    if (logfd >= 0) {
			write(logfd, "Ignore[non-free]", 17);
			write(logfd, pkg, strlen(pkg));
			write(logfd, "\tfile:", 6);
			write(logfd, filename, strlen(filename));
			write(logfd, "\tby:", 4);
			write(logfd, cmdname, strlen(cmdname));
			write(logfd, "\n", 1);
		    }
		    goto done;
		}
		if (strstr(accept, "contrib") == NULL
		    && strcmp(package, "contrib") == 0) {
		    if (logfd >= 0) {
			write(logfd, "Ignore[contrib]:", 16);
			write(logfd, pkg, strlen(pkg));
			write(logfd, "\tfile:", 6);
			write(logfd, filename, strlen(filename));
			write(logfd, "\tby:", 4);
			write(logfd, cmdname, strlen(cmdname));
			write(logfd, "\n", 1);
		    }
		    goto done;
		}
	    }
	    package = pkg;
	}
#endif

/* do_install: */
	if (check_dpkglock()) {
	    if (logfd >= 0) {
		write(logfd, "Install:", 8);
		write(logfd, package, strlen(package));
		write(logfd, "\tfile:", 6);
		write(logfd, filename, strlen(filename));
		write(logfd, "\tby:", 4);
		write(logfd, cmdname, strlen(cmdname));
		write(logfd, "\n", 1);
		write(logfd, " - ignored, dpkg is running\n", 29);
	    }
	    goto done;
	}

	if (logfd >= 0) {
	    write(logfd, "Install:", 8);
	    write(logfd, package, strlen(package));
	    write(logfd, "\tfile:", 6);
	    write(logfd, filename, strlen(filename));
	    write(logfd, "\tby:", 4);
	    write(logfd, cmdname, strlen(cmdname));
	    write(logfd, "\n", 1);
	}
	if (logfd >= 0) {
	    close(logfd);
	    logfd = -1;
	}

	if (simulate) {
	    if (quiet || !isatty(1)) {
		/* do nothing */;
	    } else {
		printf("Install:%s\tfile:%s\tby:%s\n", 
		       package, filename, cmdname);
		fflush(stdout);
	    }
	} else {
	    if (auto_apt_nobg && !isatty(1)) {
		goto done;
	    }
	    pid = fork();
	    if (pid == 0) {
#if 1
		const char *progpath = "/usr/lib/auto-apt/auto-apt-installer";
		char *argv[5];
		argv[0] = "auto-apt-installer";
		argv[1] = cmdname;
		argv[2] = (char *)filename;
		argv[3] = package;
		argv[4] = NULL;
		unsetenv("LD_PRELOAD");
		if (path)
		    putenv(path);
		execv(progpath, argv);
		exit(-1);
#else /* old internal apt-get */
		const char *argv0;
		const char *x_argv0 = "/usr/bin/x-terminal-emulator";
		const char *tty_argv0 = "/usr/bin/sudo";
		char *const *argv;
		char *const x_argv[8] = { "x-terminal-emulator", "-e",
					  "sudo",
					  "apt-get", "install", package,
					  NULL };
		char *const tty_argv[] = { "sudo",
					   "apt-get", "-y", "install", package,
					   NULL };
		unsetenv("LD_PRELOAD");
		if (path)
		    putenv(path);
		argv0 = tty_argv0;
		argv = tty_argv;
		if (auto_apt_x) {
		    argv0 = x_argv0;
		    argv = x_argv;
		} else if (quiet || !isatty(1)) {
		    int fd = open_internal("/dev/null", O_RDWR, 0);
		    /* redirect /dev/null */
		    dup2(fd, 0);
		    dup2(fd, 1);
		    dup2(fd, 2);
		} else {
		    printf("Install:%s\tfile:%s\tby:%s\n", 
			   package, filename, cmdname);
		    fflush(stdout);
		    if (auto_apt_conf_switch("AUTO_APT_YES") == 0) {
			char inbuf[64];
			printf("Do you want to install %s now? [Y/n] ", 
			       package);
			fflush(stdout);
			while (fgets(inbuf, sizeof(inbuf)-1, stdin) != NULL) {
			    if (inbuf[0] == 'n' || inbuf[0] == 'N' ||
				inbuf[0] == '\003') {
				exit(-1);
			    }
			    if (inbuf[0] == 'y' || inbuf[0] == 'Y' ||
				inbuf[0] == '\n' || inbuf[0] == '\r') {
				break;
			    }
			}
		    }
		}
		if (logfd >= 0) {
		    close(logfd);
		    logfd = -1;
		}
		execv(argv0, argv);
		exit(-1);
#endif
	    }
	    /* check wchan is read_chan to catch whether package waits
	     * user interaction (?)
	     */
	    waitpid(pid, &status, 0);	/* need timeout? */
	    ok = (status == 0);
	}
    done:
	if (logfd >= 0) {
	    close(logfd);
	    logfd = -1;
	}
	if (path)
	    free(path);
	return ok;
    }
    return 0;
}


int
execl(const char *path, const char *arg, ...)
{
    size_t argv_max = 1024;
    const char **argv = alloca(argv_max * sizeof(const char *));
    unsigned int i;
    va_list args;

    auto_apt_setup();
    argv[0] = arg;
    va_start(args, arg);
    i = 0;
    while (argv[i++] != NULL) {
	if (i == argv_max) {
	    const char **nptr = 
		alloca ((argv_max *= 2) * sizeof(const char *));
	    argv = (const char **)memmove(nptr, argv, i);
	    argv_max += i;
        }
	argv[i] = va_arg(args, const char *);
    }
    va_end(args);
    return execv(path, (char *const *)argv);
}

int
execle(const char *path, const char *arg, ...)
{
    size_t argv_max = 1024;
    const char **argv = alloca(argv_max * sizeof(const char *));
    const char *const *envp;
    unsigned int i;
    va_list args;
    argv[0] = arg;
    
    auto_apt_setup();
    va_start(args, arg);
    i = 0;
    while (argv[i++] != NULL) {
	if (i == argv_max) {
	    const char **nptr = 
		alloca((argv_max *= 2) * sizeof(const char *));
	    argv = (const char **)memmove(nptr, argv, i);
	    argv_max += i;
        }
	argv[i] = va_arg (args, const char *);
    }
    envp = va_arg (args, const char *const *);
    va_end (args);
    return execve(path, (char *const *)argv, (char *const *)envp);
}

/**
 * 
 * @param filename
 * @param argv
 * @param envp
 * @return 
 */
int
execve(const  char  *filename, char *const argv [], char *const envp[])
{
    int e;
    int apt = 0;
    funcptr  __execve;

    auto_apt_setup();
 again:
    DPRINT(("execve: filename=%s \n", filename));
    if (!apt && detectdb_file) {
        detect_package(filename, __FUNCTION__);
    }
    __execve = load_library_symbol("execve");
    if (__execve == NULL) {
	errno = EINVAL;
	return -1;
    }
    DPRINT(("execve = %p\n", __execve));
    e = __execve(filename, argv, envp);
    DPRINT(("execve: filename=%s, e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_EXEC] && e < 0) {
	switch (errno) {
	case ENOENT:
	    DPRINT(("execve: filename=%s not found\n", filename));
	    if (!apt && apt_get_install_for_file(filename)) {
		apt = 1;
		goto again;
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}

/**
 * Does what libc's `execv` does except adding auto-apt functionality (the
 * original `execv` is provided by `load_library_symbol`)
 * @param filename
 * @param argv
 * @return 
 */
int
execv(const  char  *filename, char *const argv [])
{
    int e;
    int apt = 0;
    funcptr  __execv;

    auto_apt_setup();
 again:
    DPRINT(("execv: filename=%s \n", filename));
    if (!apt && detectdb_file) { detect_package(filename, __FUNCTION__); }
    __execv = load_library_symbol("execv");
    if (__execv == NULL) {
	errno = EINVAL;
	return -1;
    }
    DPRINT(("execv = %p :filename=%s %d,%s\n", 
	    __execv, filename, apt, detectdb_file));
    e = __execv(filename, argv);
    DPRINT(("execvp: filename=%s, e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_EXEC] && e < 0) {
	switch (errno) {
	case ENOENT:
	    DPRINT(("execv: filename=%s not found\n", filename));
	    if (!apt && apt_get_install_for_file(filename)) {
		apt = 1;
		goto again;
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}

#undef open
/**
 * 
 * @param filename
 * @param flags present for compatibility reasons, not used
 * @param ...
 * @return 
 */
int
open(const char *filename, int flags, ...)
{
    int apt = 0;
    int e;
    funcptr __open;
    mode_t mode;
    va_list ap;
    static int o = 0; /* XXX: guard for open() in detect_pacage? */
    
    auto_apt_setup();
 again:
    DPRINT(("open: filename=%s \n", filename));
    if (!apt && detectdb_file && !o) { 
	o = 1; detect_package(filename, __FUNCTION__); o = 0; 
    }
    __open = load_library_symbol("open");
    if (__open == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("open = %p\n", __open));
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
    e = __open(filename, flags, mode);
    DPRINT(("open: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_OPEN] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}

#if 1
#undef __libc_open
int
__libc_open(const char *filename, int flags, ...)
{
    int apt = 0;
    int e;
    funcptr __open;
    mode_t mode;
    va_list ap;
    static int o = 0; /* XXX */
    
    auto_apt_setup();
 again:
    DPRINT(("__libc_open: filename=%s \n", filename));
    if (!apt && detectdb_file && !o) { 
	o = 1; detect_package(filename, __FUNCTION__); o = 0; 
    }
    __open = load_library_symbol("__libc_open");
    if (__open == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("__libc_open = %p\n", __open));
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
    e = __open(filename, flags, mode);
    DPRINT(("__libc_open: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_OPEN] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;

}
#endif

#undef open64
int
open64(const char *filename, int flags, ...)
{
    int apt = 0;
    int e;
    funcptr __open;
    mode_t mode;
    va_list ap;
    static int o = 0; /* XXX */
    
    auto_apt_setup();
 again:
    DPRINT(("open64: filename=%s \n", filename));
    if (!apt && detectdb_file && !o) { 
	o = 1; detect_package(filename, __FUNCTION__); o = 0; 
    }
    __open = load_library_symbol("open64");
    if (__open == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("open64 = %p\n", __open));
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
    e = __open(filename, flags, mode);
    DPRINT(("open64: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_OPEN] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}

#if 1
#undef __libc_open64
int
__libc_open64(const char *filename, int flags, ...)
{
    int apt = 0;
    int e;
    funcptr __open;
    mode_t mode;
    va_list ap;
    static int o = 0; /* XXX */
    
    auto_apt_setup();
 again:
    DPRINT(("__libc_open64: filename=%s \n", filename));
    if (!apt && detectdb_file && !o) { 
	o = 1; detect_package(filename, __FUNCTION__); o = 0; 
    }
    __open = load_library_symbol("__libc_open64");
    if (__open == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("__libc_open64 = %p\n", __open));
    va_start(ap, flags);
    mode = va_arg(ap, mode_t);
    va_end(ap);
    e = __open(filename, flags, mode);
    DPRINT(("__libc_open64: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_OPEN] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}
#endif

int 
access(const char *filename, int type)
{
    int apt = 0;
    int e;
    funcptr __access;
    
    auto_apt_setup();
 again:
    DPRINT(("access: filename=%s \n", filename));
    if (!apt && detectdb_file) detect_package(filename, __FUNCTION__);
    __access = load_library_symbol("access");
    if (__access == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("access = %p\n", __access));
    e = __access(filename, type);
    DPRINT(("access: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_ACCESS] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}

int 
euidaccess(const char *filename, int type)
{
    int apt = 0;
    int e;
    funcptr __euidaccess;
    
    auto_apt_setup();
 again:
    DPRINT(("euidaccess: filename=%s \n", filename));
    if (!apt && detectdb_file) detect_package(filename, __FUNCTION__);
    __euidaccess = load_library_symbol("euidaccess");
    if (__euidaccess == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("euidaccess = %p\n", __euidaccess));
    e = __euidaccess(filename, type);
    DPRINT(("euidaccess: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_ACCESS] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}


#undef __xstat
int
__xstat(int ver, const char *filename, struct stat *buf)
{
    int apt = 0;
    int e;
    funcptr __stat;
    
    auto_apt_setup();
 again:
    DPRINT(("stat: filename=%s \n", filename));
    if (!apt && detectdb_file) detect_package(filename, __FUNCTION__);
    __stat = load_library_symbol("__xstat");
    if (__stat == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("stat = %p\n", __stat));
    e = __stat(ver, filename, buf);
    DPRINT(("stat: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_STAT] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}

#undef __xstat64
struct stat64; /* XXX */
int
__xstat64(int ver, const char *filename, struct stat64 *buf)
{
    int apt = 0;
    int e;
    funcptr __stat;
    
    auto_apt_setup();
 again:
    DPRINT(("stat64: filename=%s \n", filename));
    if (!apt && detectdb_file) detect_package(filename, __FUNCTION__);
    __stat = load_library_symbol("__xstat64");
    if (__stat == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("stat64 = %p\n", __stat));
    e = __stat(ver, filename, buf);
    DPRINT(("stat64: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_STAT] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}


#undef __lxstat
int
__lxstat(int ver, const char *filename, struct stat *buf)
{
    int apt = 0;
    int e;
    funcptr __stat;
    
    auto_apt_setup();
 again:
    DPRINT(("lstat: filename=%s \n", filename));
    if (!apt && detectdb_file) detect_package(filename, __FUNCTION__);
    __stat = load_library_symbol("__lxstat");
    if (__stat == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("lstat = %p\n", __stat));
    e = __stat(ver, filename, buf);
    DPRINT(("lstat: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_STAT] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}

#undef __lxstat64
/**
 * does what `__lxstat64` does
 * @param ver one of `_STAT_VER`, `_STAT_VER_KERNEL`, `_STAT_VER_LINUX`
 * @param filename
 * @param buf
 * @return 
 */
int
__lxstat64(int ver, const char *filename, struct stat64 *buf)
{
    int apt = 0;
    int e;
    funcptr __stat;
    
    auto_apt_setup();
 again:
    DPRINT(("lstat64: filename=%s \n", filename));
    if (!apt && detectdb_file) detect_package(filename, __FUNCTION__);
    __stat = load_library_symbol("__lxstat64");
    if (__stat == NULL) {
	errno = ENOENT;
	return -1;
    }
    DPRINT(("lstat64 = %p\n", __stat));
    e = __stat(ver, filename, buf);
    DPRINT(("lstat64: filename=%s e=%d\n", filename, e));
    if (apt_hook[APT_HOOK_STAT] && e < 0) {
	switch (errno) {
	case ENOENT:
	    if (*filename == '/') {
		if (!apt && apt_get_install_for_file(filename)) {
		    apt = 1;
		    goto again;
		}
	    }
	    break;
	default:
	    break;
	}
    }
    return e;
}
