"""Create sample maps for testing

Available modes:
* from-installed: create test files from maps in the MapsEvolved database
"""

import sys
import os
import glob
import shutil
import argparse
import contextlib
import subprocess

import logging
logger = logging.getLogger(__name__)

import mev_build_utils
mev_build_utils.add_mev_to_path()

import pymaplib
import mapsevolved.config


DATADIR = os.path.join(os.path.dirname(__file__), 'data')
TEMPFILE = os.path.join(DATADIR, '.testdata.tmp')
OUTPUTDIR = os.path.join(DATADIR, 'sample-tifs')
BLAND_TIF = os.path.join(DATADIR, 'no-geoinfo.tif')
LIBGEOTIF_WIN = os.path.join(DATADIR, '..', '..', 'third-party', 'libgeotiff')
LIBTIFF_WIN = os.path.join(DATADIR, '..', '..', 'third-party', 'libtiff', 'tools')


def sanitized_filename(proj):
    """Remove undesired characters from a filename component"""
    proj = proj.replace('+', '')
    for c in r'/\:*?"<>|':
        proj = proj.replace(c, '_')
    return proj.strip().replace(' ', '-')

def copy_libs(destdir):
    """Copy third-party dll files to destdir"""
    for lib in glob.glob(os.path.join('pymaplib', '*.dll')):
        dest = os.path.join(destdir, os.path.basename(lib))
        if not os.path.exists(dest):
            shutil.copy(lib, dest)

def get_tiff_binary(name):
    """Return filename of a libtiff binary

    On Windows, also ensure that the file is actually runable by copying the
    required dll files, if necessary.
    """

    if sys.platform != 'win32':
        return name
    copy_libs(LIBTIFF_WIN)
    return os.path.join(LIBTIFF_WIN, name)

def get_geotif_binary(name):
    """Return filename of a geotiff binary

    On Windows, also ensure that the file is actually runable by copying the
    required dll files, if necessary.
    """

    if sys.platform != 'win32':
        return name
    copy_libs(LIBGEOTIF_WIN)
    return os.path.join(LIBGEOTIF_WIN, name)

def user_maps_by_projection():
    """Return the current user's maps grouped by projection

    Retrieve a maplist from the current user's config and return a dict mapping
    proj4 strings a list of maps with that projection.
    """

    filelist = pymaplib.FileList()
    with mapsevolved.config.Config.read() as conf:
        filelist.retrieve_from(conf)

    result = dict()
    for map in filelist.maplist:
        if not map.entry_type == map.TYPE_MAP:
            continue
        proj = map.drawable.GetProj().GetProjString().decode('ascii')
        result.setdefault(proj, []).append(map)
    return result

def run_silent(cmd):
    subprocess.check_call(cmd, stdout=subprocess.DEVNULL,
                               stderr=subprocess.DEVNULL)

@contextlib.contextmanager
def unlinker(*args):
    """Context manager to ensure file deletion"""
    try:
        yield
    finally:
        for fname in args:
            if os.path.exists(fname):
                os.unlink(fname)

def make_tif_with_geodata(input_tif, output):
    """Create a TIF test file from the geodata in input_tif"""
    listgeo = get_geotif_binary('listgeo')
    geotifcp = get_geotif_binary('geotifcp')
    tiffset = get_tiff_binary('tiffset')

    geoinfo_bytes = subprocess.check_output([listgeo, input_tif])
    with unlinker(TEMPFILE):
        with open(TEMPFILE, 'wb') as f:
            f.write(geoinfo_bytes)
        run_silent([geotifcp, '-g', TEMPFILE, BLAND_TIF, output])
        run_silent([tiffset, '-sf', 'ImageDescription', TEMPFILE, output])

def tif_testfiles_from_installed(force=False):
    """Create TIF test files from maps in the MapsEvolved database

    If ``force`` is ``True``, already existing files are overwritten.
    """

    logger.info('Loading maps...')
    os.makedirs(OUTPUTDIR, exist_ok=True)
    for proj, maps in user_maps_by_projection().items():
        # We only care about one sample TIF for each projection.
        input_tif = maps[0].storagename
        output = os.path.join(OUTPUTDIR, sanitized_filename(proj) + '.tif')

        if not input_tif.endswith('.tif'):
            continue
        if os.path.exists(output) and not force:
            logger.info('File exists, skipping %s', output)
            continue

        logger.info('Creating %s', output)
        make_tif_with_geodata(input_tif, output)

def main():
    logging.basicConfig(format='%(message)s', level=logging.INFO)
    commands = {
        'from-installed': tif_testfiles_from_installed,
    }

    parser = argparse.ArgumentParser(
                description=__doc__,
                formatter_class=argparse.RawTextHelpFormatter)
    parser.add_argument('mode', help='select test data generation mode')
    parser.add_argument('-f', '--force', action='store_true',
                        help='overwrite existing test data files')
    args = parser.parse_args()

    if args.mode not in commands:
        parser.error("unknown mode '{}'".format(args.mode))

    commands[args.mode](force=args.force)


if __name__ == '__main__':
    main()
