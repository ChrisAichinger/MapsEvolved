import os
import io
import sys
import shutil
import hashlib
import inspect
import tarfile
import zipfile
import collections
import urllib.request

from invoke import ctask

import mev_build_utils

class ModuleValidationError(RuntimeError):
    pass

def SourceForgeURL(fname):
    url = 'http://downloads.sourceforge.net/project/{}?use_mirror=autoselect'
    return url.format(fname)

AVAILABLE_MODULES = collections.OrderedDict([
   ('unxutils', {
       'compression': 'zip',
       'url': SourceForgeURL('unxutils/unxutils/current/UnxUtils.zip'),
       'sha256': 'b8c694072723a417194022a2ba265750eec61e15d1725e39449df1763e224b45',
       'unpack_location': 'unxutils2',
       'rename': [('unxutils2/usr/local/wbin', 'unxutils'),
                  ('unxutils2/bin/sh.exe', 'unxutils/sh.exe'),
                  ('unxutils2', 'unxutils/misc'),],
   }),
   ('cmake', {
       'compression': 'zip',
       'url': 'http://www.cmake.org/files/v3.2/cmake-3.2.2-win32-x86.zip',
       'sha256': '8728119bbb48468ab45230a579463729e7b12d6babfa1ad77e771b239b5430db',
       'rename': [('cmake-3.2.2-win32-x86', 'cmake')],
   }),
   ('libjpeg-turbo', {
       'compression': 'tar',
       'url': SourceForgeURL('libjpeg-turbo/1.4.0/libjpeg-turbo-1.4.0.tar.gz'),
       'sha256': 'd93ad8546b510244f863b39b4c0da0fa4c0d53a77b61a8a3880f258c232bbbee',
       'rename': [('libjpeg-turbo-1.4.0', 'libjpeg-turbo')],
   }),
   ('libjpeg', {
       'compression': 'zip',
       'url': SourceForgeURL('gnuwin32/jpeg/6b-4/jpeg-6b-4-src.zip'),
       'sha256': 'fdf5a7d3c14647fabc689a91d5a03538e839467be08a553f21ad9f4b8d87d372',
       'unpack_location': 'libjpeg2',
       'rename': [('libjpeg2/src/jpeg/6b/jpeg-6b-src', 'libjpeg'),
                  ('libjpeg2', 'libjpeg/zip-file-folders'),
                  ('libjpeg/jconfig.vc', 'libjpeg/jconfig.h'),],
       'patches': ['jpeg.diff'],
       'build': [('nmake', '/f Makefile.vc "cvars= {flags}"')],
       'publish': ['libjpeg.dll'],
   }),
   ('proj4', {
       'compression': 'tar',
       'url': 'http://download.osgeo.org/proj/proj-4.9.1.tar.gz',
       'sha256': 'fca0388f3f8bc5a1a803d2f6ff30017532367992b30cf144f2d39be88f36c319',
       'rename': [('proj-4.9.1', 'proj4')],
       'patches': ['proj4.diff'],
       'build': [('cmake', '-DBUILD_LIBPROJ_SHARED:BOOL="1" ' +
                           '-DCMAKE_INSTALL_PREFIX=instdir ' +
                           '-DINCLUDEDIR=instdir\\include ' +
                           '-DLIBDIR=instdir\\lib ' +
                           '-DPROJ_CORE_TARGET_OUTPUT_NAME=proj ' +
                           '-G "Visual Studio 10 2010"'),
                 ('cmake', '--build . --target install --config {config}'),
                ],
       'publish': ['instdir\\bin\\proj.dll'],
   }),
   ('libtiff', {
       'compression': 'zip',
       'url': 'http://download.osgeo.org/libtiff/tiff-4.0.4beta.zip',
       'sha256': '612dada703c859ea0e024e12b1bdb10d65eaf6807e06a57ac9d3ae73afe39945',
       'rename': [('tiff-4.0.4beta', 'libtiff')],
       'patches': ['libtiff.diff'],
       'build': [('nmake', '/f Makefile.vc ' +
                           '"OPTFLAGS= -EHsc -W3 ' +
                           '-D_CRT_SECURE_NO_DEPRECATE ' +
                           '-D_CRT_SECURE_NO_WARNINGS ' +
                           '-D_CRT_NONSTDC_NO_DEPRECATE ' +
                           '-DZFILLODER_LSB2MSB {flags}"')],
       'publish': ['libtiff\\libtiff.dll'],
   }),
   ('libgeotiff', {
       'compression': 'zip',
       'url': 'ftp://ftp.remotesensing.org/pub/geotiff/libgeotiff/libgeotiff-1.4.1.zip',
       'sha256': '13e723ce4672d84f640eb3fc321e73a615bf5cec41760751af00a70168f25e0d',
       'rename': [('libgeotiff-1.4.1', 'libgeotiff')],
       'patches': ['libgeotiff.diff'],
       'build': [('touch', 'geo_config.h.vc'),
                 ('nmake', '/f Makefile.vc WANT_PROJ4=1 "OPTFLAGS= -nologo {flags}" geo_config.h'),
                 ('nmake', '/f Makefile.vc WANT_PROJ4=1 "OPTFLAGS= -nologo {flags}"'),
                ],
       'publish': ['geotiff.dll', 'csv'],
   }),
   ('geographiclib', {
       'compression': 'zip',
       'url': SourceForgeURL('geographiclib/distrib/GeographicLib-1.41.zip'),
       'sha256': 'fd48b6608fffa9419cb221c3f0663426900961ad5a02b8db3065101f53528bfa',
       'rename': [('GeographicLib-1.41', 'geographiclib')],
       'patches': ['geographiclib.diff'],
       'build': [('nmake', '/f Makefile.vc "OPTFLAGS= -nologo {flags}"'),],
   }),
   ('sip', {
       'compression': 'zip',
       'url': SourceForgeURL('pyqt/sip/sip-4.15.4/sip-4.15.4.zip'),
       'sha256': 'd4b522caf93620675a6f1a7f3ed6ddcb9533cce71482d77d5015f61b98e426d2',
       'rename': [('sip-4.15.4', 'sip'),],
       'build': [('shell', 'python configure.py --platform=win32-msvc2010 ' +
                           '-e "{py_inc_dir}" INCDIR+="{py_inc_dir}" ' +
                           'LFLAGS+="/DEBUG /PDB:$(TARGET).pdb"'),
                 ('shell', 'cd sipgen && nmake'),
                 ('shell', 'cd siplib && nmake "CFLAGS=%(f)s" "CXXFLAGS=%(f)s /EHsc"' %
                           {'f': ' '.join([
                                 '-nologo -Zm200 /Zc:wchar_t /D WIN32 /D STRICT',
                                 '/D NOMINMAX /D _DEBUG /D _WINDOWS /D _WINDLL',
                                 '/D _UNICODE /D UNICODE /W0 /Oy- /Gm /GS',
                                 '/fp:precise -nologo -W0 -w34100 -w34189',
                                 '{flags}'])
                           }),
                 ('shell', 'IF /I [{config}] == [DEBUG] nmake install'),
                 ('shell', 'IF /I [{config}] == [RELEASE] nmake install'),
                ],
   }),
])


FLAGS = {
    'debug': "-D_MT -MDd /Zi /RTC1 /Od",
    'release': "-D_MT -MD /Ox",
}
PUBLISH_DIR = 'published'
PY_INC_DIR = os.path.join(os.environ['VIRTUAL_ENV'], 'Include')
# We can't easily use UnxUtils patch.exe as it lacks a manifest.
# Cf. http://stackoverflow.com/questions/533939
PATCH_PROG = os.path.abspath(os.path.join(mev_build_utils.find_git_bindir(), 'patch'))

def extract_filename(url):
    return os.path.basename(url.split('?')[0])

def unpack(url, sha256, compression, unpack_location='.'):
    """Fetch a remote archive, check its hash and decompress it

    Download the file ``url``, ensure its hash matches the ``sha256`` argument,
    then decompress it using ``compression`` method (either 'tar' or 'zip').
    The unpacked files will be written to ``unpack_location``.
    """

    print("Downloading", extract_filename(url))
    urlopener = urllib.request.urlopen(url)
    f = io.BytesIO(urlopener.read())
    sha256_found = hashlib.sha256(f.getvalue()).hexdigest()
    if sha256_found != sha256:
        raise ModuleValidationError('Failed to validate downloaded package',
                                    url, sha256_found, sha256)
    available_compressors = {
            'tar': lambda file, mode: tarfile.open(fileobj=file, mode=mode),
            'zip': lambda file, mode: zipfile.ZipFile(file=file, mode=mode),
            }
    compressor = available_compressors[compression]
    with compressor(f, 'r') as cf:
        cf.extractall(unpack_location)

def templated_run(template, ctx, directory, config, args):
    """Substitute arguments in a command template and execute it

    First, perform ``str.format()`` expansion on ``args`` with these keywords:

    * config: the desired build configuration (release/debug)
    * flags: the current build flags
    * target: the current build target

    Then substitute the resulting args string into the command
    template (keywords: 'args' and 'directory') and execute the resulting
    expression in the shell.
    """

    args = args.format(config=config,
                       flags=FLAGS[config.lower()],
                       py_inc_dir=PY_INC_DIR)

    ctx.run(template.format(directory=directory, args=args))

def nmake(ctx, directory, config, args):
    cmd = ('call "%VS100COMNTOOLS%\\vsvars32.bat" && ' +
           'cd "{directory}" && nmake {args} || pause')
    templated_run(cmd, ctx, directory, config, args)

def msbuild(ctx, directory, config, args):
    cmd = ('call "%VS100COMNTOOLS%\\vsvars32.bat" && ' +
           'cd "{directory}" && msbuild {args} || pause')
    templated_run(cmd, ctx, directory, config, args)

def cmake(ctx, directory, config, args):
    cmake = os.path.abspath('cmake/bin/cmake.exe')
    cmd = 'cd "{directory}" && "%s" {args} || pause' % cmake
    templated_run(cmd, ctx, directory, config, args)

def touch(ctx, directory, config, fname):
    "A Python version of the UNIX touch command"

    fname = os.path.join(directory, fname)
    with open(fname, 'a'):
        os.utime(fname)

def shell(ctx, directory, config, args):
    cmd = ('call "%VS100COMNTOOLS%\\vsvars32.bat" && ' +
           'cd "{directory}" && {args}')
    templated_run(cmd, ctx, directory, config, args)

def select_modules(modules):
    "Yield items from AVAILABLE_MODULES selected by the modules parameter"

    if modules is None or modules == 'all':
        return AVAILABLE_MODULES.items()
    return ((m, AVAILABLE_MODULES[m]) for m in modules.split(';'))

@ctask(help={'modules': 'Semicolon-separated list of modules to operate on (default: all)'})
def download(ctx, modules=None):
    "Download third-party modules required for building MapsEvolved"

    for modulename, module in select_modules(modules):
        args = set(inspect.getargspec(unpack).args)
        args = {k: v for k, v in module.items() if k in args}
        unpack(**args)
        for src, dest in module.get('rename', []):
            mev_build_utils.resiliant_rename(src, dest)
        for patch in module.get('patches', []):
            patch = os.path.abspath(patch)
            cmd = 'cd {modulename} && "{patch_prog}" -p1 --forward --fuzz 0 < "{patch}"'
            ctx.run(cmd.format(modulename=modulename, patch_prog=PATCH_PROG,
                               patch=patch))

@ctask(help={'config': 'Which configuration to build: debug/release',
             'modules': 'Semicolon-separated list of modules to operate on (default: all)'})
def build(ctx, config, modules=None):
    "Build third-party modules required for building MapsEvolved"

    for modulename, module in select_modules(modules):
        if not module.get('build'):
            continue
        for funcname, args in module['build']:
            func = globals()[funcname]
            func(ctx, modulename, config, args)

@ctask(help={'modules': 'Semicolon-separated list of modules to operate on (default: all)'})
def distclean(ctx, modules=None):
    "Remove the downloaded modules completely"

    for modulename, module in select_modules(modules):
        if os.path.exists(modulename):
            shutil.rmtree(modulename)

@ctask(help={'modules': 'Semicolon-separated list of modules to operate on (default: all)',
             'targets': 'Semicolon-separated list of directories to copy the libraries into'})
def publish(ctx, targets, modules=None):
    "Publish built libraries to a target location"

    targets = targets.split(';')
    for modulename, module in select_modules(modules):
        for src in module.get('publish', []):
            src = os.path.join(modulename, src)
            if not os.path.exists(src):
                raise RuntimeError("Publishing source doesn't exist: %s" % src)
            if os.path.isfile(src):
                for target in targets:
                    shutil.copy(src, target)
            elif os.path.isdir(src):
                for target in targets:
                    dest = os.path.join(target, os.path.basename(src))
                    if os.path.isdir(dest):
                        shutil.rmtree(dest)
                    shutil.copytree(src, dest)
            else:
                raise NotImplementedError('Can only copy files and directories!')

@ctask
def listmodules(ctx):
    "List available third-party modules"

    print('\n'.join(AVAILABLE_MODULES.keys()))
# chcp 65001
