#!/usr/bin/python3
#
# Copyright 2015 Christian Aichinger <Greek0@gmx.net>
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Build a small boost zipfile from the full Windows binary distribution.

The binary distributions for Windows are >200MB and come in an executable
package. Unpacked, they weigh in at ~2GB.
This is impractically large for MapsEvolved, so extract only the desired
files from the unpacked distribution and repack them into a zip file.
"""

import sys
import os
import re
import shutil
import pathlib
import argparse

import logging
logger = logging.getLogger(__name__)

import mev_build_utils

DEFAULT_DEST = 'boost-mini'

HEADERS = ['boost\\thread.hpp', 'boost\\optional.hpp', 'boost\\atomic.hpp']
LIBS = ['boost_chrono-*', 'boost_system-*', 'boost_thread-*',
        'boost_date_time-*', 'boost_atomic-*']
FILES = ['*.css', '*.png', '*.htm', '*.html', 'LICENSE*']


def _find_includes_recursive(fname, files=None):
    """Recursive helper function for find_includes()

    Recursively traverse all files included by ``fname``, results
    are accumulated in ``files`` and then returned.
    """

    if files is None:
        files = set()

    if fname in files:
        return files

    files.add(fname)

    l = []
    with open(fname, 'r') as f:
        for line in f:
            m = re.match(r'#\s*include\s*[<"](.*?)[>"]', line)
            if not m:
                continue
            inc = m.group(1).replace('/', '\\')
            if not inc.startswith('boost'):
                continue
            l.append(inc)
    for inc in l:
        _find_includes_recursive(inc, files)

    return files

def find_includes(desired_headers):
    """Find all headers necessary to include the desired_headers

    Return a set of all include files required by at least one file in
    ``desired_headers``.
    """

    headers = set()
    for h in desired_headers:
        headers = _find_includes_recursive(h, headers)
    return headers

def first_path_components(p, n):
    """Extract the first 'n' components of path 'p'"""
    p = pathlib.Path(p)
    return pathlib.Path(os.path.join(*p.parts[:n]))

def copy_headers(desired_headers, destdir):
    """Copy include files required by the desired headers"""
    logger.info('Collecting included headers')

    includes = find_includes(desired_headers)
    toplevel = set(first_path_components(p, 2) for p in includes)

    logger.info('Copying headers')
    destdir = os.path.join(destdir, 'boost')
    os.makedirs(destdir, exist_ok=True)
    for p in toplevel:
        mev_build_utils.copy_any(p, destdir)

def copy_libs(libs, destdir):
    """Copy runtime libraries"""
    logger.info('Copying libraries')
    libdir = list(pathlib.Path('.').glob('lib??-msvc-*'))[0]
    destdir = os.path.join(destdir, str(libdir))
    os.makedirs(destdir, exist_ok=True)
    for lib in libs:
        for p in libdir.glob(lib):
            mev_build_utils.copy_any(p, destdir)

def copy_misc_files(files, destdir):
    """Copy files from the toplevel such as documentation"""
    logger.info('Copying miscellaneous files')
    paths = mev_build_utils.multiglob(*files)
    for p in paths:
        mev_build_utils.copy_any(p, destdir)

def main():
    logging.basicConfig(format='%(message)s', level=logging.INFO)

    parser = argparse.ArgumentParser(
                description=__doc__,
                formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('--dest', default=DEFAULT_DEST,
                        help='destination directory')
    parser.add_argument('-z', '--zip', action='store_true',
                        help='produce a zip output file')
    parser.add_argument('-f', '--force', action='store_true',
                        help='overwrite existing destination directory')
    args = parser.parse_args()

    if os.path.exists(args.dest):
        if args.force:
            shutil.rmtree(args.dest)
        else:
            logger.error("Destination directory '%s' exists, aborting.",
                         args.dest)
            sys.exit(1)

    copy_headers(HEADERS, args.dest)
    copy_libs(LIBS, args.dest)
    copy_misc_files(FILES, args.dest)

    if args.zip:
        zipname = shutil.make_archive(args.dest, 'zip', args.dest)
        logger.info("Successfully created zip file '%s'", zipname)

if __name__ == '__main__':
    main()
