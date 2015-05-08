#!../venv/Scripts/python
import sys
import os
import re
import glob
import time
import shutil
import hashlib
import pathlib
import functools
import importlib
import itertools
import contextlib
import subprocess

try:
    import winreg
except ImportError:
    winreg = None


def multiglob(*patterns, unique=True):
    """Yield paths matching one or more of multiple pathname patterns

    This function is similar to glob.iglob() but accepts multiple arguments.

    If `unique` is False, the same path may be yielded multiple times if it
    matches more than one pattern. If `unique` is True, each path will be
    yielded only once.
    """

    if unique:
        yield from set(multiglob(*patterns, unique=False))
        return
    for p in patterns:
        yield from glob.iglob(p)


def copy_any(src, dest_folder, *, overwrite=False):
    """Copy a source file or directory to a destination folder

    ``src`` must refer to an existing file or directory.

    The destination ``dest_folder`` must be an existing folder. A new file or
    subdirectory, named ``os.path.basename(src)``, will be created under the
    that folder.

    If the destination file/subdirectory already exists, the result depends
    on the ``overwrite`` parameter. However, a file never overwrites a
    directory and vice versa. A FileExistsError is raised in such cases.

    ``src`` and ``dest_folder`` can be either ``pathlib.Path`` objects or a
    path string.

    On success, the ``pathlib.Path`` object of the destination file or subdir
    is returned.
    """

    dest_folder = pathlib.Path(dest_folder)
    if not dest_folder.is_dir():
        raise FileNotFoundError('Destination folder not found', dest_folder)

    src = pathlib.Path(src)
    if not src.exists():
        raise FileNotFoundError('Source not found', dest_folder)

    dest_name = dest_folder / src.parts[-1]
    if src.is_dir():
        if dest_name.is_dir() and overwrite:
            shutil.rmtree(str(dest_name))
        if dest_name.is_file():
            raise FileExistsError('The destination exists and is a file',
                                  dest_name)
        shutil.copytree(str(src), str(dest_name))
    else:
        if dest_name.is_file() and overwrite:
            os.unlink(str(dest_name))
        if dest_name.is_dir():
            raise FileExistsError('The destination exists and is a directory',
                                  dest_name)
        shutil.copy(str(src), str(dest_name))

    return dest_name


@functools.lru_cache()
def get_vs_paths():
    "Return a list of search paths for Visual Studio binaries"

    vsvars = os.path.expandvars('%VS100COMNTOOLS%\\vsvars32.bat')
    output = subprocess.check_output(
            ['call', vsvars, '>nul', '&&',
             'set', 'PATH'],
            shell=True, universal_newlines=True).strip('\n')

    # Select the path= strings and remove the portion before the content.
    output = [line[5:] for line in output.split('\n')
                       if line.lower().startswith('path=')]
    new_path_str = output[0]
    old_path = [p.strip('"') for p in os.environ['PATH'].split(os.pathsep)]
    new_path = [p.strip('"') for p in new_path_str.split(os.pathsep)]
    new_dirs = new_path[:-len(old_path)]
    return new_dirs
    vs_dirs = set(new_path) - set(old_path)

    # Preserve the order of vs_dirs in the path.
    return [p for p in new_path if p in vs_dirs]

def get_vs_executable(name):
    """Return the full path for a Visual Studio program

    The argument `name` must include the file extension.
    If the program could not be found, `ValueError` is raised.
    """

    for path in get_vs_paths():
        fname = os.path.join(path, name)
        if os.path.isfile(fname):
            return fname
    raise ValueError("VisualStudio executable couldn't be found", name)


def is_venv_active():
    "Return True if a virtual environment is currently active"

    return 'VIRTUAL_ENV' in os.environ

def run_in_venv(ctx, venv_dir, command, **kwargs):
    """Execute command in a venv

    Run command in the venv specified by venv_dir, using the pyinvoke context
    ctx. Keyword arguments are pass on to the runner.

    The command argument should be a list!
    """

    cmd = []
    if is_venv_active():
        cmd += ['deactivate', '&&']
    cmd += [os.path.join(venv_dir, 'scripts', 'activate'), '&&']
    cmd += command
    return ctx.run(cmd, **kwargs)

def resilient_rename(src, dest, retries=3, delay=0.5):
    """Rename src file to dest, retrying on temprary errors

    Rename src to dest. If a PermissionError is encountered, wait briefly,
    then retry (for up to retries times).

    This may be necessary on Windows where virus scanners, Dropbox, ... may
    briefly prevent us from renaming files.
    """

    for i in range(retries - 1):
        try:
            return os.rename(src, dest)
        except PermissionError:
            time.sleep(delay)
    return os.rename(src, dest)

def resilient_delete(pathname, recursive=False, retries=3, delay=0.5):
    """Delete pathname, retrying on temprary errors

    Delete pathname. If `recursive` is `True`, call `shutil.rmtree`, otherwise
    `os.unlink`. If a PermissionError is encountered, wait briefly, then retry
    (for up to `retries` times).

    This may be necessary on Windows where virus scanners, Dropbox, ... may
    briefly prevent access to files.
    """

    for i in range(retries):
        try:
            if recursive:
                shutil.rmtree(pathname)
            else:
                os.unlink(pathname)
            return
        except PermissionError as e:
            if i == retries - 1:
                raise
            time.sleep(delay)

@contextlib.contextmanager
def temporary_chdir(path):
    "Context manager for cd'ing to path and back"

    curdir = os.getcwd()
    os.chdir(path)
    try: yield
    finally: os.chdir(curdir)

@contextlib.contextmanager
def save_mtime(dirname, globs=None):
    """Reset files' mtime to earlier values if they did not change

    SIP overwrites the generated files each time it is called, even if the
    contents did not change. This causes `make` to pointlessly regenerate
    dependent files.

    To avoid this, store mtime and file contents' hash prior to calling SIP.
    Afterwords, restore the previous mtime if the files did not change.

    If globs is not given, the directory or file `dirname` is monitored.
    If globs is specified, `dirname/<globs>` is monitored.
    """

    if globs:
        globs = [glob.iglob(os.path.join(dirname, g)) for g in globs]
        it = itertools.chain(*globs)
    else:
        if os.path.isdir(dirname):
            it = glob.iglob(os.path.join(dirname, '*'))
        elif os.path.exists(dirname):
            it = [dirname]
        else:
            raise RuntimeError("save_mtime: target directory does not exist")

    files = {}
    for fname in it:
        mtime = os.stat(fname).st_mtime
        fhash = hashlib.sha256(open(fname, 'rb').read()).digest()
        files[fname] = (mtime, fhash)

    yield

    for fname in files.keys():
        try:
            content = open(fname, 'rb').read()
        except (PermissonError, FileNotFoundError):
            continue
        st = os.stat(fname)
        new_mtime = st.st_mtime
        new_hash = hashlib.sha256(content).digest()
        old_mtime, old_hash = files[fname]
        if new_hash == old_hash and new_mtime > old_mtime:
            os.utime(fname, (st.st_atime, old_mtime))

def _which(program):
    '''Search for a program on $PATH

    Mimic the UNIX `which` utility.

    If `program` is an absolute path, return `program` if the file exists and
    can be executed. If `program` is not absolute, search every directory on
    $PATH for an executable file named `program` and return the file name if
    found. In either case, `None` is returned on failure.
    '''

    def is_executable(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_executable(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_executable(exe_file):
                return exe_file

    if not program.endswith('.exe') and sys.platform == 'win32':
        return _which(program + '.exe')

    return None


def _app_from_registry(app, *, hkey=None, keyname=None, valname=None, fname=None):
    """Try to find an application by examining the Windows registry

    Search for `app` in the Windows registry and return an executable filename
    if successful. If `app` is not found, return None.

    `hkey`, `keyname`, `valname` can be used to override the search location
    in the registry. The default values are appropriate for git and doxygen.

    `fname` can be used to set the search path within the installation folder.
    By default, `$INSTDIR\\app` and `$INSTDIR\\bin\\app` are searched.

    Example:

        >>> _app_from_registry('git')
        "C:\\....\\git\\bin\\git.exe"
    """

    if not winreg:
        return None

    if hkey is None:
        hkey = winreg.HKEY_LOCAL_MACHINE
    if keyname is None:
        keyname = r'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\{}_is1'
        keyname = keyname.format(app)
    if valname is None:
        valname = 'InstallLocation'
    if fname is None:
        fname = app

    bin_fname = 'bin\\' + fname
    return (_app_from_registry_3264(32, hkey, keyname, valname, fname) or
            _app_from_registry_3264(64, hkey, keyname, valname, fname) or
            _app_from_registry_3264(32, hkey, keyname, valname, bin_fname) or
            _app_from_registry_3264(64, hkey, keyname, valname, bin_fname)
           )

def _app_from_registry_3264(regbits, hkey, keyname, valname, fname):
    """Internal helper function for _app_from_registry()

    Search `hkey/keyname/value` in the Windows registry. If the result is a
    string, append `fname` and check if the resulting file is executable.
    Return a valid executable filename if successful, otherwise None.

    `regbits` must be either 32 or 64 and determines whether the 32-bit or the
    64-bit registry tree is opened..
    """

    if not winreg:
        return None

    access_dict = { 32: winreg.KEY_READ | winreg.KEY_WOW64_32KEY,
                    64: winreg.KEY_READ | winreg.KEY_WOW64_64KEY }
    access = access_dict[regbits]

    try:
        with winreg.OpenKeyEx(hkey, keyname, access=access) as key:
            path, valuetype = winreg.QueryValueEx(key, valname)
        if valuetype != winreg.REG_SZ:
            return None
    except OSError:
        return None

    return _which(os.path.join(path, fname))

def find_executable(name):
    '''Find a program using any means available

    Search for `name` on $PATH and in the Windows registry if available.
    On success, return the path of the desired executable.
    If no file could be found, a `ValueError` is raised.
    '''

    executable = _which(name) or _app_from_registry(name) or None
    if not executable:
        raise ValueError(
            "Could not find executable '{}'. Is it installed?".format(name))
    return executable

def get_mev_path():
    """Get the main MapsEvolved directory

    The mapsevolved package will be a subdirectory in this dir.
    """

    old_p = None
    p = pathlib.Path(__file__).absolute().parent
    while p != old_p:
        mainfile = p / 'MapsEvolved.py'
        if mainfile.exists():
            return str(p)
        old_p, p = p, p.parent
    raise FileNotFoundError("Couldn't locate MapsEvolved directory.",
                            startpoint=__file__)

def add_mev_to_path(at_beginning=False):
    """Add the MapsEvolved dir to sys.path

    Normally, the directory is appended to sys.path. If ``at_beginning`` is
    True, it is inserted at the beginning instead.
    """

    if at_beginning:
        sys.path.insert(0, get_mev_path())
    else:
        sys.path.append(get_mev_path())


class LightInvokeRunner:
    """A lightweight invoke Runner that exits cleanly on errors

    The default `invoke.runner.Local` runner has several drawbacks:
    * It tries to capture and store command output, but this garbles it when
      the console encoding doesn't match `locale.getpreferredencoding()`.
      Also, all colorization of the output is lost, which makes finding
      error messages harder in the msbuild output.
    * An exception is raised by default if a non-zero exit status is
      encountered. This gives a long traceback after a failed command,
      which is usually not helpful.
    * It doesn't support a `cwd=` argument to set the working directory.

    This class is a lightweight replacement for the default invoke runner.
    It does not capture the program output, it supports `cwd=` and
    `env=` keyword arguments, and if `quickexit=True` (the default), command
    failure leads program termination via `sys.exit(1)`.
    """

    def run(self, command, *, echo=False, hide=None, quickexit=True, **kwargs):
        """Run the specified command

        Supported keyword variables:

         * `echo`: Echo the command output, same as the invoke Runner.
         * `hide`: Currently, supports only triggering all output on/off.
         * `quickexit`: Die with a brief failure message on command failures.
         * `cwd`: Set the working dir for the called process.
         * `env`: Set the environment for the called process.

        Other keywords are accepted but are silently ignored for compatibility
        with invoke.
        """

        if echo:
            print("\033[1;37m{0}\033[0m".format(command))

        callargs = {k: kwargs[k] for k in kwargs if k in set(['env', 'cwd'])}
        if hide:
            callargs.update({ 'stdout': subprocess.DEVNULL,
                              'stderr': subprocess.DEVNULL })

        try:
            subprocess.check_call(command, shell=True, **callargs)
        except subprocess.CalledProcessError as e:
            if not quickexit:
                raise
            else:
                is_str = isinstance(command, str)
                printable_command = command if is_str else ' '.join(command)
                msg = "Running child process failed returncode {}\n" \
                      "Call: {}".format(e.returncode, printable_command)
                print(msg, file=sys.stderr)
                sys.exit(1)

def import_from_file(name, fpath):
    """Import a module given a filesystem path"""
    return importlib.machinery.SourceFileLoader(name, fpath).load_module()

if __name__ == '__main__':
    main()
