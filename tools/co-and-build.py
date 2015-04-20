#!/usr/bin/python3.3

"""A tool to checkout MapsEvolved from git and build it

Checkout MapsEvolved from a git repository specified on the commandline,
then do a release or debug build.
"""

import sys
import os
import subprocess
import argparse

import findgit

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


def main():
    args = parse_args()

    mev_git = args.gitpath + '/.git'
    mev_dir = os.path.abspath('mapsevolved')
    odm_dir = os.path.abspath(os.path.join(mev_dir, 'ODM'))

    git = findgit.find_git_executable()
    if not git:
        print("Could not find git executable", file=sys.stderr)
        sys.exit(2)

    subprocess.check_call([git, "clone", mev_git, mev_dir])

    os.chdir(mev_dir)
    subprocess.check_call([sys.executable, 'bootstrap.py'])

    invoke = os.path.abspath(os.path.join('venv', 'Scripts', 'invoke'))
    os.chdir(odm_dir)
    subprocess.check_call([invoke, 'configure', '--config', args.mode])

    os.chdir(mev_dir)
    init_sh = os.path.join("tools", "init_shell.py")
    subprocess.check_call([sys.executable, init_sh, 'venv', "python", "configure.py"])

    gmake = os.path.join("ODM", "libraries", "unxutils", "make.exe")
    if args.mode == 'RELEASE':
        subprocess.check_call([sys.executable, init_sh, 'venv', gmake, "RELEASE=1", "all"])
    else:
        subprocess.check_call([sys.executable, init_sh, 'venv', gmake, "all"])

if __name__ == '__main__':
    main()
