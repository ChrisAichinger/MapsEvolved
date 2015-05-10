import sys
import os
import subprocess

from invoke import Collection, Task, ctask

import mev_build_utils

SIP_SRC = os.path.abspath(os.path.dirname(__file__))
SIP_INPUT = os.path.join(SIP_SRC, 'maps.sip')
SIP_BUILD_DIR_REL = os.path.join('..', 'build', 'sip')
SIP_BUILD_DIR = os.path.join(SIP_SRC, SIP_BUILD_DIR_REL)
SIP_BUILDFILE = os.path.join(SIP_BUILD_DIR, "maps.sbf")

ROOT = os.path.join(SIP_SRC, '..', '..')

def sip_inject_build_directory(build_file, build_dir):
    """Include build_dir in the paths of files in build_file

    The SIP make system uses the paths stored in build_file to write the
    Makefile. We want out generated source code and the object files to go
    into a subdirectory, which is not directly supported by SIP. Thus, we
    modify the build_file to include the output directory within the
    source/header filenames directly.
    """

    with open(build_file, 'r+') as f:
        output_lines = []
        for line in f.readlines():
            key, value = [s.strip() for s in line.split('=', 1)]
            if key in ('sources', 'headers', 'target'):
                files = [os.path.join(build_dir, s) for s in value.split(' ')]
                value = ' '.join(files)
            output_lines.append(key + ' = ' + value)
        f.seek(0)
        f.truncate()
        f.write('\n'.join(output_lines))

def run_sip(ctx):
    import sipconfig
    os.makedirs(SIP_BUILD_DIR, exist_ok=True)
    with mev_build_utils.save_mtime(SIP_BUILD_DIR, ['*.cpp', '*.c', '*.h']):
        ctx.run([sipconfig.Configuration().sip_bin,
                 "-c", SIP_BUILD_DIR,    # Generated code directory
                 "-b", SIP_BUILDFILE,    # Build file name
                 "-T",                   # Suppress timestamps
                 "-e",                   # Enable exception support
                 "-w",                   # Enable warnings
                 "-o",                   # Auto-generate docstrings
                 "-I", SIP_SRC,          # Additional include dir
                 SIP_INPUT])             # Input file

    sip_inject_build_directory(SIP_BUILDFILE, SIP_BUILD_DIR_REL)

def sanitized_env():
    """Return a santitized environment safe for running GNU Make

    For the build process on Windows, GNU Make should use cmd.exe, not any
    sh.exe that happens to be on $PATH. The build fails otherwise, due to
    missing/removed directory separators (backslashes).

    Return an environment dict that hides all 'sh' shells on the system.
    """

    path = os.environ['PATH'].split(';')
    path = [d for d in path if not os.path.exists(os.path.join(d, 'sh.exe'))]
    san_env = dict(os.environ)
    san_env['PATH'] = ';'.join(path)

    # Apparently, make can find sh even from _OLD_VIRTUAL_PATH.
    if '_OLD_VIRTUAL_PATH' in san_env:
        del san_env['_OLD_VIRTUAL_PATH']
    return san_env

@ctask(help={'config': 'Which configuration to build: debug/release',
             'targets': 'Comma-separated list of build targets (default:all)'})
def build(ctx, config, targets='all'):
    """Build the sip bindings for pymaplib_cpp"""

    run_sip(ctx)

    init_sh = os.path.join(ROOT, "tools", "init_shell.py")
    gmake = os.path.join(ROOT, "third-party", "unxutils", "make.exe")
    venv = os.path.join(ROOT, 'venv')
    cmd = [sys.executable, init_sh, venv, gmake, '-j', '4']
    config_arg = { 'release': ['RELEASE=1'], 'debug': [] }[config.lower()]
    targets = targets.split(',')
    ctx.run(cmd + config_arg + targets, env=sanitized_env(), cwd=SIP_SRC)

ns = Collection(*[obj for obj in vars().values() if isinstance(obj, Task)])
ns.configure({'run': { 'runner': mev_build_utils.LightInvokeRunner }})
