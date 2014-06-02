#!/usr/bin/python3.3

"""A tool to checkout MapsEvolved from git and build it

Checkout MapsEvolved from a git repository specified on the commandline,
then do a release or debug build.
"""

import sys
import os
import subprocess
import argparse

def parse_args():
    parser = argparse.ArgumentParser(
            description='Checkout and build MapsEvolved')

    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--release', dest="mode", action='store_const',
                       const="RELEASE")
    group.add_argument('--debug', dest="mode", action='store_const',
                       const="DEBUG")

    parser.add_argument('gitpath', metavar='repo-path', action='store',
                       help='Path of the git repository')

    return parser.parse_args()

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

def main():
    args = parse_args()

    mev_git = args.gitpath + '/.git'
    odm_git = args.gitpath + '/ODM/.git'
    mev_dir = os.path.abspath('mapsevolved')
    odm_dir = os.path.abspath(os.path.join(mev_dir, 'ODM'))

    git = find_git_executable()
    if not git:
        print("Could not find git executable", file=sys.stderr)
        sys.exit(2)

    subprocess.check_call([git, "clone", mev_git, mev_dir])
    subprocess.check_call([git, "clone", odm_git, odm_dir])

    # Run ODM/build.sh using GIT bash. Spawn a login shell to force sourcing
    # /etc/profile on Windows to set $PATH (add /bin and /usr/bin).
    os.chdir(odm_dir)
    bash = os.path.join(os.path.dirname(git), 'bash')
    subprocess.check_call([bash, '--login', "./build.sh", args.mode])

    os.chdir(mev_dir)
    init_venv = os.path.join("tools", "init_venv.py")
    subprocess.check_call([sys.executable, init_venv])

    init_sh = os.path.join("tools", "init_shell.py")
    subprocess.check_call([sys.executable, init_sh, 'venv', "python", "configure.py"])

    gmake = os.path.join("__unxutils", "make.exe")
    if args.mode == 'RELEASE':
        subprocess.check_call([sys.executable, init_sh, 'venv', gmake, "RELEASE=1", "all"])
    else:
        subprocess.check_call([sys.executable, init_sh, 'venv', gmake, "all"])

if __name__ == '__main__':
    main()
