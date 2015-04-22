#!../venv/Scripts/python
import sys
import os
import re
import glob
import time
import hashlib
import functools
import itertools
import contextlib
import subprocess

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

def resiliant_rename(src, dest, retries=3, delay=0.5):
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

def which(program):
    def is_exe(fpath):
        return os.path.isfile(fpath) and os.access(fpath, os.X_OK)

    fpath, fname = os.path.split(program)
    if fpath:
        if is_exe(program):
            return program
    else:
        for path in os.environ["PATH"].split(os.pathsep):
            path = path.strip('"')
            exe_file = os.path.join(path, program)
            if is_exe(exe_file):
                return exe_file

    return None

def _find_git_registry(regbits):
    try:
        import winreg
    except ImportError:
        return None

    hklm = winreg.HKEY_LOCAL_MACHINE
    keystr = "SOFTWARE\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Git_is1"
    if regbits == 32:
        access = winreg.KEY_READ | winreg.KEY_WOW64_32KEY
    elif regbits == 64:
        access = winreg.KEY_READ | winreg.KEY_WOW64_64KEY
    else:
        raise ValueError("regbits must be either 32 or 64")

    try:
        with winreg.OpenKeyEx(hklm, keystr, access=access) as key:
            path, valuetype = winreg.QueryValueEx(key, "InstallLocation")
        if valuetype != winreg.REG_SZ:
            return None
    except OSError:
        return None

    return which(os.path.join(path, 'bin\\git.exe'))

def find_git_executable():
    git = which('git')
    if git:
        return git

    git = which('git.exe')
    if git:
        return git

    git = _find_git_registry(32)
    if git:
        return git

    git = _find_git_registry(64)
    if git:
        return git

    return None

def find_git_bindir():
    return os.path.dirname(find_git_executable())

def main():
    git = find_git_executable()
    if not git:
        print('Git not found', file=sys.stderr)
        sys.exit(1)
    print(git)

if __name__ == '__main__':
    main()
