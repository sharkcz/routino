# Python interface setup script
#
# Part of the Routino routing software.
#
# This file Copyright 2018, 2022 Andrew M. Bishop
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU Affero General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Affero General Public License for more details.
#
# You should have received a copy of the GNU Affero General Public License
# along with this program.  If not, see <http://www.gnu.org/licenses/>.
#

import os
import re
from setuptools import setup, Extension

routino_router = Extension('routino._router',
                           sources = ['src/_router.c'],
                           include_dirs = ['../src'],
                           library_dirs = ['../src'],
                           libraries = ['routino'])

# Note: the database needs access to all symbols, not just those
# exported by the libroutino library so it must link with the object
# files and not just the library.

lib_files = []

for file in os.listdir('../src'):
    if re.search("-lib.o", file) and not re.search("-slim-lib.o", file):
        lib_files.append("../src/" + file)

routino_database = Extension('routino._database',
                             sources = ['src/_database.cc', 'src/database.cc'],
                             include_dirs = ['../src'],
                             extra_objects = lib_files,
                             library_dirs = ['../src'])

setup (name = 'Routino',
       version = '1.0',
       author="Andrew M. Bishop", author_email='amb@routino.org',
       url='http://routino.org/',
       description = 'Interfaces to Routino in Python',
       packages = ['routino'],
       package_dir = {'routino': 'src'},
       py_modules = ['routino', 'routino.router', 'routino.database'],
       ext_modules = [routino_router, routino_database])
