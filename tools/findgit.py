#!../venv/Scripts/python
import sys
import os

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
