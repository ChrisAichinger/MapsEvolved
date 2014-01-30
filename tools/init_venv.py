import sys
import os
import io
import re
import venv
import urllib.request
import zipfile
import shutil
import contextlib
import glob
import argparse
import subprocess
import imp


sip_dir = '__sip_source'
sip_url = 'http://downloads.sourceforge.net/project/pyqt/sip/sip-4.15.4/sip-4.15.4.zip?r=&ts=1390046988&use_mirror=freefr'
setuptools_url = 'https://bitbucket.org/pypa/setuptools/raw/bootstrap/ez_setup.py'
wxpython_url = 'http://wxpython.org/Phoenix/snapshot-builds/wxPython_Phoenix-3.0.1.dev75695-py3.3-win32.egg'
gpxpy_url = 'https://github.com/tkrajina/gpxpy/archive/master.zip'
gpxpy_dir = '__gpxpy_source'
SCRIPT_DIR = os.path.dirname(os.path.abspath(sys.argv[0]))
init_shell_batch = 'init_shell.bat'


@contextlib.contextmanager
def temporary_chdir(path):
    curdir = os.getcwd()
    os.chdir(path)
    try: yield
    finally: os.chdir(curdir)


def copytree(src, dst, symlinks=False):
    names = os.listdir(src)
    if not os.path.isdir(dst):
        os.makedirs(dst)
    errors = []
    for name in names:
        srcname = os.path.join(src, name)
        dstname = os.path.join(dst, name)
        try:
            if symlinks and os.path.islink(srcname):
                linkto = os.readlink(srcname)
                os.symlink(linkto, dstname)
            elif os.path.isdir(srcname):
                copytree(srcname, dstname, symlinks)
            else:
                shutil.copy2(srcname, dstname)
            # XXX What about devices, sockets etc.?
        except OSError as why:
            errors.append((srcname, dstname, str(why)))
        # catch the Error from the recursive copytree so that we can
        # continue with other files
        except shutil.Error as err:
            errors.extend(err.args[0])
    try:
        shutil.copystat(src, dst)
    except OSError as why:
        # can't copy file access times on Windows
        if why.winerror is None:
            errors.extend((src, dst, str(why)))
    if errors:
        raise Error(errors)


def rerun_in_venv(venv_dir):
    venv_enter_command = os.path.join(SCRIPT_DIR, init_shell_batch)
    print(venv_enter_command, venv_dir, 'python.exe', sys.argv[0], '--rerun-in-venv')
    #os.execl(venv_enter_command, venv_dir, 'python.exe', sys.argv[0], '--rerun-in-venv')
    os.system(' '.join([venv_enter_command, venv_dir, 'python.exe', sys.argv[0], '--rerun-in-venv']))
    sys.exit(0)


def make_venv(venv_dir):
    venv.create(venv_dir, system_site_packages=True)

    python_include_dir = os.path.join(sys.prefix, 'include')
    venv_include_dir = os.path.join(venv_dir, 'include')
    copytree(python_include_dir, venv_include_dir)

    python_libs_dir = os.path.join(sys.prefix, 'libs')
    venv_libs_dir = os.path.join(venv_dir, 'libs')
    copytree(python_libs_dir, venv_libs_dir)

    # Enable building extensions with debug flags against a non-debug Python.
    # https://stackoverflow.com/questions/1236060
    with open(os.path.join(venv_include_dir, 'pyconfig.h'), 'r+') as f:
        content = f.read()
        content = re.sub(r'python33_d.lib', 'python33.lib', content)
        content = re.sub(r'.*define Py_DEBUG.*', '', content, re.MULTILINE)
        f.seek(0)
        f.truncate()
        f.write(content)


def install_sip(sip_url, source_dir, venv_dir):
    urlopener = urllib.request.urlopen(sip_url)
    f = io.BytesIO(urlopener.read())
    with zipfile.ZipFile(f, 'r') as zf:
        zf.extractall(path=source_dir)

    extracted_source_dir = glob.glob(source_dir + '/sip-*')[0]
    venv_dir = os.path.abspath(venv_dir)
    include_dir = os.path.join(venv_dir, 'Include')
    py_lib_dir = os.path.join(sys.prefix, 'libs')

    with temporary_chdir(extracted_source_dir):
        subprocess.check_call([
                'python.exe', 'configure.py', '--platform=win32-msvc2010',
                '-e', include_dir, 'INCDIR+=' + include_dir,
                'LFLAGS+=/DEBUG /PDB:$(TARGET).pdb',
                ])

        with temporary_chdir('sipgen'):
            subprocess.check_call('nmake')

        # We will link against the produced siplib, so we need to control its
        # compilation options (e.g. debug libs vs. regular).
        # We can't do this via configure.py, since the SIP make system sucks.
        flags = ' '.join([
                '-nologo -Zm200 /Zc:wchar_t /D "WIN32" /D "STRICT" ' +
                '/D "NOMINMAX" /D "_DEBUG" /D "_WINDOWS" /D "_WINDLL" ' +
                '/D "_UNICODE" /D "UNICODE" /W0 /Od /Oy- /Gm /RTC1 /GS ' +
                '/fp:precise -nologo /Zi /MDd -W0 -w34100 -w34189'])
        with temporary_chdir('siplib'):
            subprocess.check_call(['nmake',
                    'CFLAGS={}'.format(flags),
                    'CXXFLAGS={} /EHsc'.format(flags),
                    ])
        subprocess.check_call(['nmake', 'install'])


def install_setuptools(setuptools_url):
    module_code = urllib.request.urlopen(setuptools_url).read()
    ez_setup = imp.new_module('ez_setup')
    exec(module_code, ez_setup.__dict__)
    tarball = ez_setup.download_setuptools()
    ez_setup._install(tarball, [])
    os.remove(tarball)


def install_wxpython(wxpython_url):
    subprocess.check_call(['easy_install.exe', wxpython_url])


def install_gpxpy(gpxpy_dir, gpxpy_url):
    urlopener = urllib.request.urlopen(gpxpy_url)
    f = io.BytesIO(urlopener.read())
    with zipfile.ZipFile(f, 'r') as zf:
        zf.extractall(path=gpxpy_dir)

    real_dir = os.path.join(gpxpy_dir, 'gpxpy*')
    real_dir = glob.glob(real_dir)[0]
    with temporary_chdir(real_dir):
        subprocess.check_call(['python.exe', 'setup.py', 'install'])
    #shutil.rmtree(gpxpy_dir)


def main():
    parser = argparse.ArgumentParser(description='Create and populate a venv.')
    parser.add_argument('--venv', dest='venv_dir', action='store',
                        default='venv',
                        help="venv output directory (default: 'venv'")
    parser.add_argument('--rerun-in-venv', action='store_true', dest='rerun',
                        help='internal use only')

    args = parser.parse_args(arg for arg in sys.argv[1:] if arg)
    venv_dir = args.venv_dir

    # Change dir to the parent directory of the script folder.
    os.chdir(os.path.join(SCRIPT_DIR, '..'))

    if not args.rerun:
        venv_dir = sys.argv[1] if sys.argv[1:] else "venv"
        print("\nSetting up venv in '{}'".format(venv_dir))
        if os.path.exists(venv_dir):
            shutil.rmtree(venv_dir)
        make_venv(venv_dir)

        print("\nRerunning {} in venv".format(sys.argv[0]))
        rerun_in_venv(venv_dir)
    else:
        print("\nStarted back up within venv")
        # After we have entered the venv, we continue executing here.
        print("\nUnpacking SIP in '{}' and installing in venv".format(sip_dir))
        if os.path.exists(sip_dir):
            shutil.rmtree(sip_dir)
        install_sip(sip_url, sip_dir, venv_dir)

        print("\nInstalling setuptools")
        install_setuptools(setuptools_url)

        print("\nInstalling wxPython")
        install_wxpython(wxpython_url)

        print("\nInstalling gpxpy")
        install_gpxpy(gpxpy_dir, gpxpy_url)

        print("\nFinished setting up venv. Enjoy :)")


if __name__ == '__main__':
    main()
