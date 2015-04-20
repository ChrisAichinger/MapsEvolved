#!../venv/Scripts/python
import sys
import os
import glob
import hashlib
import itertools
import contextlib

@contextlib.contextmanager
def temporary_chdir(path):
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
