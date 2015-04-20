#!/usr/bin/python3.4


import sys
import os
import re
import venv
import shutil
import subprocess

def make_venv(venv_dir):
    venv.create(venv_dir)

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
        content = re.sub(r'(python..)_d\.lib', r'\1.lib', content)
        content = re.sub(r'.*define Py_DEBUG.*', '', content, re.MULTILINE)
        f.seek(0)
        f.truncate()
        f.write(content)

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

def main():
    venv_dir = 'venv'
    make_venv(venv_dir)

    py_venv = os.path.join(venv_dir, 'Scripts', 'python.exe')
    subprocess.check_call([py_venv, '-m', 'ensurepip'])

    pip = os.path.join(venv_dir, 'Scripts', 'pip3')
    subprocess.check_call([pip, 'install', '-r', 'requirements.txt'])

    print('Bootstrapping finished. Next:')
    print(r'  venv\Scripts\activate && cd ODM && invoke configure -c DEBUG|RELEASE')

if __name__ == '__main__':
    main()
