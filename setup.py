#!/usr/bin/python3
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

import auto_apt_globals
from setuptools import setup, find_packages

setup(
    name = auto_apt_globals.app_name,
    version = auto_apt_globals.app_version_string,
    packages = ["."],
    install_requires = [],
    
    # metadata for upload to PyPI
    author = auto_apt_globals.app_author,
    author_email = auto_apt_globals.app_author_email,
    url=auto_apt_globals.app_website,
    description = "A tool to automatically install requested files which are not yet installed using apt-get",
    license = "GPLv3",
    keywords = "apt,apt-get",
    entry_points = {'console_scripts': [
        'auto-apt = auto_apt:main',
        ],
    },
)
