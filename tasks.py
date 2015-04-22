import sys
import os
import shutil

from invoke import ctask

import mev_build_utils

TARGETS = ['pymaplib_cpp\\dist', 'pymaplib_cpp\\tests', 'pymaplib']
SIP_BUILD_DIR = '__build'


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
    configuration = sipconfig.Configuration()
    os.makedirs(SIP_BUILD_DIR, exist_ok=True)
    sip_build_file = os.path.join(SIP_BUILD_DIR, "maps.sbf")

    with mev_build_utils.save_mtime(SIP_BUILD_DIR, ['*.cpp', '*.c', '*.h']):
        ctx.run([configuration.sip_bin,
                 "-c", SIP_BUILD_DIR,    # Generated code directory
                 "-b", sip_build_file,   # Build file name
                 "-T",                   # Suppress timestamps
                 "-e",                   # Enable exception support
                 "-w",                   # Enable warnings
                 "-o",                   # Auto-generate docstrings
                 "-I", "src_sip",        # Additional include dir
                 "src_sip\\maps.sip"])   # Input file

    sip_inject_build_directory(sip_build_file, SIP_BUILD_DIR)


@ctask(help={'config': 'Which configuration to build: debug/release (default: as last build)',
             'args': 'Additional arguments for msbuild'})
def build_cpp(ctx, config=None, args=''):
    "Build the pymaplib_cpp library"

    cmd = 'call "%VS100COMNTOOLS%\\vsvars32.bat" && cd pymaplib_cpp && msbuild '
    if config is not None:
        cmd += '/p:Configuration={config} '
    cmd += args
    ctx.run(cmd.format(config=config))

import subprocess
@ctask(help={'config': 'Which configuration to build: debug/release',
             'targets': 'Comma-separated list of build targets (default:all)'})
def build_pymaplib(ctx, config, targets='all'):
    "Build pymaplib, after which MapsEvolved is ready to run"

    run_sip(ctx)

    init_sh = os.path.join("tools", "init_shell.py")
    gmake = os.path.join("third-party", "unxutils", "make.exe")
    cmd = [sys.executable, init_sh, 'venv', gmake]
    config_arg = { 'release': ['RELEASE=1'], 'debug': [] }[config.lower()]
    targets = targets.split(',')
    subprocess.check_call(cmd + config_arg + targets)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def configure(ctx, config):
    "Setup libraries and perform an initial build"

    for target in TARGETS:
        os.makedirs(target, exist_ok=True)

    # Disable Python output buffering to get consistent log output.
    os.environ['PYTHONUNBUFFERED'] = '1'
    cmd = ' '.join(['cd third-party && invoke distclean download',
                    'build --config {config}',
                    'publish "--targets={targets}"'])
    targets = ';'.join(os.path.abspath(target) for target in TARGETS)
    ctx.run(cmd.format(config=config, targets=targets))
    build_cpp(ctx, config=config)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def build(ctx, config):
    "Build pymaplib_cpp and its SIP bindings"

    build_cpp(ctx, config)
    build_pymaplib(ctx, config)

    os.environ['PYTHONUNBUFFERED'] = '1'
    cmd = ' '.join(['cd third-party && invoke publish "--targets={targets}"'])
    targets = ';'.join(os.path.abspath(target) for target in TARGETS)
    ctx.run(cmd.format(targets=targets))

    shutil.copy(os.path.join('pymaplib_cpp', 'dist', 'pymaplib_cpp.dll'), 'pymaplib')
    shutil.copy(os.path.join('__build', 'maplib_sip.pyd'), 'pymaplib')

    print()
    print('*******************************')
    print('* Build finished successfully *')
    print('*******************************')
    print()

@ctask(help={'config': 'Which configuration to build: debug/release'})
def checkout_and_build(ctx, repository, target_dir, config):
    '''Perform GIT checkout and build'''

    git = mev_build_utils.find_git_executable()
    if not git:
        print('Could not find git executable', file=sys.stderr)
        sys.exit(2)
    ctx.run([git, 'clone', repository, target_dir])

    with mev_build_utils.temporary_chdir(target_dir):
        # Change codepage so we don't get errors printing to the console.
        ctx.run(['chcp', '65001'])
        # Disable Python output buffering to get consistent log output.
        os.environ['PYTHONUNBUFFERED'] = '1'
        ctx.run(['python', "bootstrap.py"])
        mev_build_utils.run_in_venv(ctx, 'venv',
                                    ['invoke',
                                     'configure', '--config', config,
                                     'build', '--config', config,
                                     'crtcheck'])

@ctask
def distclean(ctx):
    "Delete generated and downloaded files"

    ctx.run('cd third-party && invoke distclean')
    build_cpp(ctx, args='/t:Clean')

@ctask
def crtcheck(ctx):
    "Verify that we link only against a single version of the CRT library"

    files = list(mev_build_utils.multiglob('pymaplib/*.pyd', 'pymaplib/*.dll'))
    init_sh = os.path.join("tools", "init_shell.py")
    check = os.path.join("tools", "check_linked_libs.py")

    os.environ['PYTHONUNBUFFERED'] = '1'
    ctx.run([sys.executable, init_sh, 'venv', 'python', check] + files)
