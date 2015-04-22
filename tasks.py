import sys
import os
import shutil
import subprocess

from invoke import ctask

import mev_build_utils

TARGETS = ['pymaplib_cpp\\dist', 'pymaplib_cpp\\tests', 'pymaplib']

SIP_SRC = 'pymaplib_cpp\\sip'


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

@ctask(help={'config': 'Which configuration to build: debug/release (default: as last build)',
             'args': 'Additional arguments for msbuild'})
def build_cpp(ctx, config=None, args=''):
    "Build the pymaplib_cpp library"

    cmd = 'call "%VS100COMNTOOLS%\\vsvars32.bat" && cd pymaplib_cpp && msbuild '
    if config is not None:
        cmd += '/p:Configuration={config} '
    cmd += args
    ctx.run(cmd.format(config=config))

@ctask(help={'config': 'Which configuration to build: debug/release',
             'targets': 'Comma-separated list of build targets (default:all)'})
def build_sip(ctx, config, targets='all'):
    "Build pymaplib, after which MapsEvolved is ready to run"

    subprocess.check_call(['invoke', 'build', '--config', config,
                                              '--targets', targets],
                          cwd=SIP_SRC)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def build(ctx, config):
    "Build pymaplib_cpp and its SIP bindings"

    build_cpp(ctx, config)
    build_sip(ctx, config)

    os.environ['PYTHONUNBUFFERED'] = '1'
    cmd = ' '.join(['cd third-party && invoke publish "--targets={targets}"'])
    targets = ';'.join(os.path.abspath(target) for target in TARGETS)
    ctx.run(cmd.format(targets=targets))

    shutil.copy(os.path.join('pymaplib_cpp', 'dist', 'pymaplib_cpp.dll'), 'pymaplib')
    shutil.copy(os.path.join('pymaplib_cpp', 'dist', 'maplib_sip.pyd'), 'pymaplib')

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
