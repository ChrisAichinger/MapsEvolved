import sys
import os

from invoke import ctask

import findgit

TARGETS = ['ODM\\Debug', 'ODM\\Release', 'ODM\\tests']

def temporary_chdir(path):
    curdir = os.getcwd()
    os.chdir(path)
    try: yield
    finally: os.chdir(curdir)

@ctask(help={'config': 'Which configuration to build: debug/release (default: as last build)',
             'args': 'Additional arguments for msbuild'})
def build_odm(ctx, config=None, args=''):
    "Build the OutdoorMapper library"

    cmd = 'call "%VS100COMNTOOLS%\\vsvars32.bat" && cd ODM && msbuild '
    if config is not None:
        cmd += '/p:Configuration={config} '
    cmd += args
    ctx.run(cmd.format(config=config))

import subprocess
@ctask(help={'config': 'Which configuration to build: debug/release',
             'targets': 'Comma-separated list of build targets (default:all)'})
def build_pymaplib(ctx, config, targets='all'):
    print('---', sys.executable)
    init_sh = os.path.join("tools", "init_shell.py")
    ctx.run([sys.executable, init_sh, 'venv', "python", "configure.py"])

    gmake = os.path.join("ODM", "libraries", "unxutils", "make.exe")
    cmd = [sys.executable, init_sh, 'venv', gmake]
    config_arg = { 'release': ['RELEASE=1'], 'debug': [] }[config.lower()]
    targets = targets.split(',')
    subprocess.check_call(cmd + config_arg + targets)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def configure(ctx, config):
    "Setup libraries and perform an initial build"

    for target in TARGETS:
        os.makedirs(target, exist_ok=True)

    cmd = ' '.join(['cd ODM\\libraries && invoke distclean download',
                    'build --config {config}',
                    'publish "--targets={targets}"'])
    targets = ';'.join(os.path.abspath(target) for target in TARGETS)
    ctx.run(cmd.format(config=config, targets=targets))
    build_odm(ctx, config=config)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def build(ctx, config):
    build_odm(ctx, config)
    build_pymaplib(ctx, config)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def checkout_and_build(ctx, repository, target_dir, config):
    git = findgit.find_git_executable()
    if not git:
        print("Could not find git executable", file=sys.stderr)
        sys.exit(2)
    ctx.run([git, "clone", repository, target_dir])
    with temporary_chdir(target_dir):
        ctx.run(['python', "bootstrap.py"])
        ctx.run([os.path.join('venv', 'scripts', 'invoke'),
                 'configure', '--config', config,
                 'build', '--config', config])

@ctask
def distclean(ctx):
    "Delete generated and downloaded files"

    ctx.run('cd ODM\\libraries && invoke distclean')
    build_odm(ctx, args='/t:Clean')
