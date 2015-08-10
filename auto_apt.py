#!/usr/bin/python
# -*- coding: utf-8 -*-

#    This program is free software: you can redistribute it and/or modify
#    it under the terms of the GNU General Public License as published by
#    the Free Software Foundation, either version 3 of the License, or
#    (at your option) any later version.
#
#    This program is distributed in the hope that it will be useful,
#    but WITHOUT ANY WARRANTY; without even the implied warranty of
#    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
#    GNU General Public License for more details.
#
#    You should have received a copy of the GNU General Public License
#    along with this program.  If not, see <http://www.gnu.org/licenses/>.

# auto-apt -- on demand package installation tool
# Copyright (c) 2000 Fumitoshi UKAI <ukai@debian.or.jp>
# GPL

# AUTO_APT_DEBUG=<defined?>
# AUTO_APT_VERBOSE=<defined?>
# AUTO_APT_QUIET=<defined?>
# AUTO_APT_LOG=<filename>	-- obsolate?
# AUTO_APT_ACCEPT={main,non-US,non-free,contrib,none}
# AUTO_APT_SIMULATE=<defined?>
# AUTO_APT_CACHEDIR=/var/cache/auto-apt/
# AUTO_APT_DETECT=<dbfile>
# AUTO_APT_YES=<defined?>
# AUTO_APT_DB=/var/cache/auto-apt/pkgcontents.bin
# AUTO_APT_HOOK={exec,open,access,stat}
#
# APT_CONFIG=/etc/apt/apt.conf
# SOURCES_LIST=/etc/auto-apt/sources.list (cf: apt-config -dump)

__author__ = "richter"
__date__ = "$23.03.2015 15:19:28$"

import os
import logging
import subprocess as sp
import re
import plac
import auto_apt_globals
import datetime

logger = logging.getLogger(__name__)
logger.setLevel(logging.INFO)
ch = logging.StreamHandler()
ch.setLevel(logging.INFO)
logger.addHandler(ch)

# cover all three states because it's more elegant than with an X and a no-X
# flag and avoids trouble with plac
X_USAGE_YES = 1
X_USAGE_NO= 0
X_USAGE_AUTO = 2
x_usage_default = X_USAGE_AUTO
simulate_default = False
assume_yes_default= False
quiet_default = False
no_bg_default = False
no_detect_default = False

AUTO_APT_SIMULATE=True
AUTO_APT_YES=True
AUTO_APT_QUIET=True
AUTO_APT_NOBG=True
AUTO_APT_X=False
SUBCOMMAND_RUN = "run"
SUBCOMMAND_UPDATE = "update"
SUBCOMMAND_UPDATE_DB = "update-db"
SUBCOMMAND_UPDATE_DB_LOCAL = "update-db-local"
SUBCOMMAND_UPDATE_LOCAL = "update-local"
SUBCOMMAND_MERGE = "merge"
SUBCOMMAND_MERGE_LOCAL = "merge-local"
SUBCOMMAND_CHECK = "check"
SUBCOMMAND_CHECK_LOCAL = "check-local"
SUBCOMMAND_INSTALL = "install"
SUBCOMMAND_LIST = "list"
SUBCOMMAND_LIST_LOCAL = "list-local"
SUBCOMMAND_DELETE = "delete"
SUBCOMMAND_DELETE_LOCAL = "delete-local"
SUBCOMMAND_SEARCH = "search"
SUBCOMMAND_SEARCH_LOCAL = "search-local"
SUBCOMMAND_DEBUILD = "debuild"
SUBCOMMAND_STATUS = "status"

AUTO_APT_ID = '$Id: auto-apt.sh,v %s %s ukai Exp $' % (
    auto_apt_globals.app_version_string, str(datetime.datetime.now()), ) # used in
    # usage message
AUTO_APT_PKGCDB='/usr/lib/auto-apt/auto-apt-pkgcdb'
AUTO_APT_SO='/lib/auto-apt.so'
AUTO_APT_ACCEPT=["main", "non-US", "non-free", "contrib"]
AUTO_APT_CACHEDIR="/var/cache/auto-apt"
AUTO_APT_DB=os.path.join(AUTO_APT_CACHEDIR, "pkgcontents.bin")
AUTO_APT_FILEDB=os.path.join(AUTO_APT_CACHEDIR, "pkgfiles.bin")
AUTO_APT_HOOK="all"
AUTO_APT_SOURCES_LIST="/etc/auto-apt/sources.list"

# binaries
apt_get_default = "/usr/bin/apt-get"
sudo_default = "/usr/bin/sudo"

def __check_x_terminal_emulator__():
    if os.path.exists("/usr/bin/x-terminal-emulator") and os.access("/usr/bin/x-terminal-emulator", os.X_OK):
        return True
    return False

@plac.annotations(
    x_usage=('Whether or not to use a present X server instance', 'option', "x"),
    simulate=('simulate', 'flag'),
    assume_yes=("Add `--assume-yes` to invokations of `apt-get`", "flag", "y"),
    quiet=("Don't print any output", "flag"),
    no_bg=("Don't run commands in background", "flag", "i"),
    no_detect=("Don't detect ? by default", "flag"),
    verbose=("Produce verbose (more) output", "flag"),
    accept=("A filter of dists parts to accept", "option"),
    hook=("??", "option"),
    db=("The path to the database", "option"),
    file_db=("The path to the file database", "option"),
    apt_get=("Path to the apt-get binary to use", "option"),
    sudo=("Path to the sudo binary to use", "option"),
    subcommands=('the subcommand', 'positional'),
)
def auto_apt(x_usage=x_usage_default, simulate=simulate_default,
    assume_yes=assume_yes_default, quiet=quiet_default, no_bg=no_bg_default,
    no_detect=no_detect_default, verbose=False,
    accept=AUTO_APT_ACCEPT, hook=AUTO_APT_HOOK,
    db=AUTO_APT_DB, file_db=AUTO_APT_FILEDB,
    apt_get=apt_get_default, sudo=sudo_default, *subcommands):
    """`auto-apt` uses the "LD_PRELOAD trick" to intercept certain library
    functions defined in `apt-get` by wrapping their behavior in ifself which
    allows to install `apt` packages on demand (e.g. while running `make` during
    a compilation
    """
    if x_usage == X_USAGE_YES:
        x= True
    elif x_usage == X_USAGE_NO:
        x = False
    elif x_usage == X_USAGE_AUTO:
        x = __check_x_terminal_emulator__()
    if verbose:
        logger.setLevel(logging.DEBUG)
        ch.setLevel(logging.DEBUG)

    if len(subcommands) == 0:
        raise ValueError("no subcommand specified")
    subcommand = subcommands[0]
    subcommand_args = [i for i in subcommands[1:]]
    if subcommand == SUBCOMMAND_RUN:
        run(cmds=subcommand_args, apt_get=apt_get, sudo=sudo, quiet=quiet)
    elif subcommand == SUBCOMMAND_UPDATE_DB_LOCAL:
        update(file_db = AUTO_APT_FILEDB)
    elif subcommand == SUBCOMMAND_UPDATE_LOCAL:
        updatelocal()
    elif subcommand == SUBCOMMAND_MERGE:
        merge()
    elif subcommand == SUBCOMMAND_MERGE_LOCAL:
        merge(file_db=AUTO_APT_DB)
    elif subcommand == SUBCOMMAND_CHECK:
        check()
    elif subcommand == SUBCOMMAND_CHECK_LOCAL:
        check(db=AUTO_APT_FILEDB)
    elif subcommand == SUBCOMMAND_INSTALL:
        install()
    elif subcommand == SUBCOMMAND_LIST:
        list()
    elif subcommand == SUBCOMMAND_LIST_LOCAL:
        list(db=AUTO_APT_FILEDB)
    elif subcommand == SUBCOMMAND_DELETE:
        delete()
    elif subcommand == SUBCOMMAND_DELETE_LOCAL:
        delete(db=AUTO_APT_FILEDB)
    elif subcommand == SUBCOMMAND_SEARCH:
        search()
    elif subcommand == SUBCOMMAND_SEARCH_LOCAL:
        search(db=AUTO_APT_FILEDB)
    elif subcommand == SUBCOMMAND_DEBUILD:
        debuild()
    elif subcommand == SUBCOMMAND_STATUS:
        status()
    else:
        raise ValueError("subcommand '%s' not supported" % (subcommand, ))

def requirepkg(cmd, pkg, apt_get):
    """
    Checks whether binary `cmd` exists and is accessible and install `apt`
    package `pkg` if it doesn't exist using the `apt-get` binary `apt_get` and
    fails if it isn't accessible.
    """
    if not os.path.exists(cmd):
        logger.info("I: Install:%s	file:	%s" % (pkg, cmd));
        sp.check_call([apt_get, "-y", "install", pkg])
        # assume that apt-get produces accessible packages
        return
    if not os.access(cmd, os.X_OK):
        raise ValueError("cmd '%s' isn't executable" % (cmd, ))

def usage():
    logger.info("%s",
        """%s

        Usage: auto-apt [options] command [arg ...]

        auto-apt is a simple command line interface for setting up auto-apt
        environment and/or search packages by filename.

        Commands:
            run - Enter auto-apt environment
                run [command [cmdarg]]
            update - Retrieve new lists of Contents (available file list)
                update
            updatedb - Regenerate lists of Contents (available file list, no download)
                updatedb
            update-local - Generate installed file lists
                update-local
            merge - Merge lists of Contents
                merge
            del - Delete package list
                del package
            check - Check which package will provide the filename
                check [-v] [-f] filename
            list - List filelist in dbfile
                list [-v] [-f]
            search - Search package by filename (grep)
                search [-v] [-f] pattern
            debuild - debuild with auto-apt
                useful to get build-depends:
            status - Report current environments (auto-apt or not)

         For some commands, command name with "-local" suffix, it will use
         pkgfiles.db, which is created by update-local command, instead of
         pkgcontents.db

        Options:
            [-s] [-y] [-q] [-i] [-x] [-X]
            [-a dists] [-p hooks]
            [-D pkgcontents.bin] [-F pkgfiles.bin] [-L detect.list]

        See the auto-apt(1) manual pages.
        """ % (AUTO_APT_ID, )
    )
    return 0

def run(cmds, apt_get, sudo, quiet):
    """
    Runs the list of command parts `cmds` with `auto-apt run` subcommand.
    """
    if str(type(cmds)) != "<type 'list'>" and str(type(cmds)) != "<class 'list'>":
        raise ValueError("cmds needs to be a list") # avoid nonsense error
            # message if something different from a list if passed to
            # subprocess.*
    if not os.path.exists(apt_get):
        raise ValueError("apt-get binary '%s' doesn't exist" % (apt_get, ))
    if not os.access(apt_get, os.X_OK):
        raise ValueError("apt-get binary '%s' isn't executable" % (apt_get, ))
    if not os.path.exists(sudo):
        raise ValueError("sudo binary '%s' doesn't exist" % (sudo, ))
    if not os.access(sudo, os.X_OK):
        raise ValueError("sudo binary '%s' isn't executable" % (sudo, ))
    if not quiet:
        logger.info("Entering auto-apt mode: ")
        logger.info("Exit the command to leave auto-apt mode.")

    try:
        sp.check_call(cmds, stdout=sp.PIPE, env={'LD_PRELOAD': '%s' % (AUTO_APT_SO, )})
        logger.debug("auto-apt subprocess for '%s' succeeded" % (str(cmds)))
    except sp.CalledProcessError as ex:
        logger.error("E: Exec $1 failed, auto-apt failed")
        raise ex

def download_content_file(content_file, content_file_url, wget):
    protocol = urlparse.parseurl(content_file_url).scheme
    # proxy setting
    a = augeas.Augeas()
    host = a.match("/files/etc/apt/apt.conf/*") # doesn't work... maybe try
        # installing lenses from source -> skip
    logger.warn("proxy support not supported")

#             if test "${method}_proxy" = ""; then
#                host=`echo $url | sed -e 's=^//==' -e 's=/.*=='`
#		eval `apt-config shell ${method}_proxy Acquire::${method}::Proxy`
#		eval `apt-config shell ${method}_proxy Acquire::${method}::Proxy::${host}`
#		if test "${method}_proxy" = "DIRECT"; then
#	            unset ${method}_proxy
#		else
#		    export ${method}_proxy
#	        fi
#	     fi
    a.close()

    # other options
    wgetopt= ["--timestamping", "--continue", "--progress=bar:force"]
    if protocol == "ftp":
        #if apt-config shell passive_ftp Acquire::ftp::Passive
        # @TODO: test whether Acquire::ftp::Passive is `true`
        logger.warn("ftp passive check not supported")
        #wgetopt.append("--passive-ftp")
    requirepkg("/usr/bin/wget", "wget")
    sp.check_call([wget, ]+wgetopt+["-O", content_file, protocol])

def update(arg1, file_db, dpkg, apt_config, wget, rsh, ssh, gunzip, nodownload=False):
    nodownload = False
    if arg1 == "db":
        nodownload = True
        # @TODO: shift
    cache_dir = os.path.realpath(AUTO_APT_DB)
    if not os.path.exists(cache_dir):
        raise ValueError("E: update: cache dir not found: %s" % (cache_dir, ))
    if not os.access(cache_dir, os.W_OK):
        raise ValueError("E: You need write permission for %s" % (cache_dir, ))

    arch = sp.check_output([dpkg, "--print-architecture"])

    os.remove(AUTO_APT_DB)
    if not os.path.exists(AUTO_APT_SOURCES_LIST):
        sp.check_output([apt_config, "shell", APT_ETC, "Dir::Etc/", SOURCES_LIST, "Dir::Etc::sourcelist"], cwd=cache_dir)
        if not os.path.exists(os.path.join(APT_ETC, SOURCES_LIST)):
            raise ValueError("E: apt sources.list:$APT_ETC/$SOURCES_LIST not found")
        os.symlink(os.path.join(APT_ETC, SOURCES_LIST), AUTO_APT_SOURCES_LIST)

        a = augeas.Augeas()
        auto_apt_sources_list_entries = a.match(os.path.join("/files", AUTO_APT_SOURCES_LIST.lstrip("/"), "*"))
        pkgs_to_store = [] # the package names to store into pkgcdb
        for entry in auto_apt_sources_list_entries:
            uri = a.get(os.path.join(entry, "uri"))
            import urlparse
            method = urlparse.parseurl(uri).scheme
            content_file_name = "Contents-%s.gz" % (arch, )
            u= "prefix" # there's an undocumented (grrr...) file name prefix `u` in the
                # original shell script which is created with
                # u=`echo $url | sed -e 's,^/*,,' -e 's:/:_:g' -e 's:__*:_:g'`
            content_file_name_prefixed = os.path.join(u, content_file_name)
            if method in ["http", "ftp"]:
                content_file_url = os.path.join(uri, content_file_name)
                logger.debug("Downloading content file '%s'" % (content_file_url, ))
                download_content_file(content_file_url)
            if method in ["rsh", "ssh"]:
                content_file_url = os.path.join(uri, content_file_name)
                logger.debug("Downloading content file '%s'" % (content_file_url, ))
                if method == "rsh":
                    requirepkg(rsh, "rsh-client")
                    cmd="rcp"
                elif method == ssh:
                    requirepkg(ssh, "ssh")
                    cmd="scp"
                sp.check_call([cmd, content_file_url])
                logger.debug("done downloading content file '%s'" % (content_file_url, ))
            if method == "file":
                os.symlink(os.path.join(uri, content_file_name), content_file_name_prefixed)

            if not os.path.exists(content_file_name_prefixed):
                raise ValueError("download or linking failed")
            parse_content_file_gz(content_file_name_prefixed)
            # store into pkgcdb
            sp.check_call([AUTO_APT_PKGCDB, "-f", AUTO_APT_DB, "put", str.join(" ", content_file_content)])
        a.close()

def parse_content_file_gz(content_file_path, gunzip):
    """extracts content file (file locations and package names in the form
    of [dist]/[category]/[package name], e.g. universe/utils/pcp are
    put after the header `\n\nFILE +LOCATION`
    """
    sp.check_call([gunzip, content_file_path])
    content_file = open(content_file_path)
    content_file_content = content_file.read()
    content_file_content = content_file_content.split("\n\nFILE[\\s]+LOCATION\n")[1]
    return content_file_content

def updatelocal():
    if os.path.exists(AUTO_APT_FILEDB):
        os.remove(AUTO_APT_FILEDB)
    pkgs_to_store = []
    for filename in os.listdir("/var/lib/dpkg/info"):
        if not filename.endswith(".list"):
            continue
        pkg = filename.rstrip(".list")
        pkgs_to_store.append(pkg)
    sp.check_call([AUTO_APT_PKGCDB, "-f", AUTO_APT_DB, "put", str.join(" ", pkgs_to_store)])

def merge(pkgs):
    sp.check_call([AUTO_APT_PKGCDB, "-f", AUTO_APT_DB, "put", str.join(" ", pkgs)])

def delete(pkgs):
    if pkg is None or pkg == "":
        raise ValueError("E: del: missing packagename")
    for pkg in pkgs:
        sp.check_call([AUTO_APT_PKGCDB, "-f", AUTO_APT_DB, "del", pkg])

@plac.annotations(
    full=('show help', 'flag'),
    verbose=('verbose output', 'flag'),
)
def check(filename, full, verbose):
    if full is True:
        for content_file_name in os.listdir(AUTO_APT_DB):
            if re.match("*_Contents-*.gz", content_file_name) is None:
                continue
            content_file_content = parse_content_file_gz(os.path.join(AUTO_APT_DB, content_file_name))
    else:
        pkgcdbopt=["test"]
        if verbose:
            pkgcdbopt.append("-v")
        sp.check_call([AUTO_APT_PKGCDB, "-f", AUTO_APT_DB, "get"]+pkgcdbopt+[content_file_content])

@plac.annotations(
    simulate=("simulate"),
    download=("download"),
    upgraded=("upgraded"),
    assumeyes=("assume yes"),
    verbose=("verbose"),
)
def install(filename, simulate, download, upgraded, assumeyes, verbose):
    if verbose:
        logger.debug("N: try to install a package providing file %s" % (filename, ))
    pkgs= check(filename)
    logger.debug("pkgs to install: %s", pkgs)
    if len(pkgs) == 0:
        raise ValueError("E: no candidate found for file %s" % (filename, ))
    if len(pkgs > 1):
       raise ValueError("E: multiple candidates %s found for file %s" % (pkgs, filename, ))

    aptopt=[]
    if simulate:
        aptopt.append("-s")
    if download:
        aptopt.append("-d")
    if upgraded:
        aptopt.append("-u")
    if assummeyes:
        aptopt.append("-y")
    pkg = pkgs[0]
    apt_cmds = [apt_get]+aptopt+["install", pkg]
    if verbose:
        logger.debug("invoking command '%s' to install packages" % (str.join(" ", apt_cmds)))
    sp.check_call(apt_cmds)

@plac.annotations(
    full=("full"),
    verbose=("verbose"),
)
def list(db, full, verbose):
    if full:
        found_db = False
        for content_file_name in os.listdir(db):
            if re.match("*_Contents-*.gz", content_file_name) is None:
                continue
            found_db = True
        if not found_db:
            raise Exception("E: no auto-apt db, run 'auto-apt update' first")
    else:
        if os.path.exists(db):
            sp.check_call([AUTO_APT_PKGCDB, "-f", db, "list"])

def search(pattern, full, verbose):
    ret_value = []
    list_result = list(full=full, verbose=verbose)
    for list_item in list_result:
        if pattern in list_item:
            ret_value.append(list_item)
    return ret_value

def debuild(pkgs, db, yes):
    if yes:
        answer = None
        while not anwer in ["y", "Y", "n", "N"]:
            answer = raw_input("""You have no $AUTO_APT_FILEDB
                It is recommended to create $fdb to get right build-depends,
                but it takes too much time to generate $fdb.\n
                Create it now ? [Y/n] """)
        if answer == "y" or answer == "Y":
            update_local()
        else:
            logger.info("ok, run without $fdb")
    else:
        update_local()

    requirepkg("/usr/bin/dpkg-parsechangelog", "dpkg-dev")

    # parse package
    dpkg_parse_changelog_output=sp.check_output(["dpkg-parsechangelog"])
    dpkg_parse_changelog_match = re.match("^Source: //p")
    if dpkg_parse_changelog_match is None:
        raise Exception("E: badly formatted changelog file, no Source:?")
    package = dpkg_parse_changelog_match.group(0)

    # parse version
    dpkg_parse_changelog_match_version = re.match("^Version: //p")
    if dpkg_parse_changelog_match_version is None:
        raise Exception("E: badly formatted changelog file, no Version:?")

    # determine architecture by architecture of dpkg-dev package and fall back
    # to `dpkg --print-architecture`
    arch = sp.check_output(["dpkg-architecture", "-q%s" % (DEB_HOST_ARCH, )])
    if arch is None:
        arch = sp.check_output([dpkg, "--print-architecture"])
    ddb = os.path.join(db, "%s%s:%s.lists" % (package, version, arch, ))

    if os.path.exists(ddb):
        if yes:
            answer = None
            while not answer in ["y", "Y", "n", "N"]:
                answer = raw_input("""detect.lists $AUTO_APT_DETECT already existed
                    Remove it, before running debuild? [Y/n] """)
            if answer == "y" or answer == "Y":
                os.remove(ddb)
            else:
                logger.info("""ok, append record to $AUTO_APT_DETECT
                    Remove it, before running debuild? [Y/n]""")
                answer1 = None
                while not answer1 in ["y", "Y", "n", "N"]:
                    pass # @TODO: migrate from shell script

    logger.debug("$prog: record file access to $DDB")
    requirepkg("/usr/bin/debuild", "devscripts")
    run(["/usr/bin/debuild", "-E"]+ pkgs)
    logger.debug("Build dependencies will be ... (in ../${pva}.build-depends)")
    os.remove(os.path.join(db, "essential-packages-list"))
    requirepkg("/usr/share/doc/build-essential/essential-packages-list", "build-essential")
    shutils.copy("/usr/share/doc/build-essential/essential-packages-list", os.path.join(db, "essential-packages-list"))
    # @TODO: migrate from shell script
    logger.debug("Note: file access record is stored $DDB")

def status(ld_preload):
    raise Exception("not implemented yet (find out what the different modes are (documentation is ridiculous currently)")

# necessary for setuptools entry_points
def main():
    plac.call(auto_apt)

if __name__ == "__main__":
    main()
