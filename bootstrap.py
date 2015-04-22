#!/usr/bin/python3.4


import sys
import os
import re
import venv
import shutil
import subprocess

def dircopy(src, dest):
    os.makedirs(dest, exist_ok=True)
    for file in os.listdir(src):
        shutil.copy(os.path.join(src, file), dest)

def make_venv(venv_dir):
    venv.create(venv_dir)

    python_include_dir = os.path.join(sys.prefix, 'include')
    venv_include_dir = os.path.join(venv_dir, 'include')
    dircopy(python_include_dir, venv_include_dir)

    python_libs_dir = os.path.join(sys.prefix, 'libs')
    venv_libs_dir = os.path.join(venv_dir, 'libs')
    dircopy(python_libs_dir, venv_libs_dir)

    # Enable building extensions with debug flags against a non-debug Python.
    # https://stackoverflow.com/questions/1236060
    with open(os.path.join(venv_include_dir, 'pyconfig.h'), 'r+') as f:
        content = f.read()
        content = re.sub(r'(python..)_d\.lib', r'\1.lib', content)
        content = re.sub(r'.*define Py_DEBUG.*', '', content, re.MULTILINE)
        f.seek(0)
        f.truncate()
        f.write(content)

def main():
    venv_dir = 'venv'
    make_venv(venv_dir)

    py_venv = os.path.join(venv_dir, 'Scripts', 'python')
    subprocess.check_call([py_venv, '-m', 'ensurepip'])

    # Don't call 'pip.exe' directly to enable clean upgrades on Windows.
    # See https://github.com/pypa/pip/issues/1299.
    subprocess.check_call([py_venv, '-m', 'pip', 'install', '--upgrade', 'pip'])
    subprocess.check_call([py_venv, '-m', 'pip', 'install', '-r', 'requirements.txt'])

    shutil.copy(os.path.join('tools', 'mev_build_utils.py'),
                os.path.join(venv_dir, 'lib', 'site-packages'))

    print('Bootstrapping finished. Next:')
    print(r'  venv\Scripts\activate && invoke configure -c DEBUG|RELEASE')

if __name__ == '__main__':
    main()
