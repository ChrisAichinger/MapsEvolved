import sys
import os

from invoke import ctask

import mev_build_utils

TARGETS = ['ODM\\Debug', 'ODM\\Release', 'ODM\\tests']
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

    cmd = ' '.join(['cd third-party && invoke distclean download',
                    'build --config {config}',
                    'publish "--targets={targets}"'])
    targets = ';'.join(os.path.abspath(target) for target in TARGETS)
    ctx.run(cmd.format(config=config, targets=targets))
    build_odm(ctx, config=config)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def build(ctx, config):
    "Build ODM and pymaplib"

    build_odm(ctx, config)
    build_pymaplib(ctx, config)

@ctask(help={'config': 'Which configuration to build: debug/release'})
def checkout_and_build(ctx, repository, target_dir, config):
    "Perform GIT checkout and build"

    git = mev_build_utils.find_git_executable()
    if not git:
        print("Could not find git executable", file=sys.stderr)
        sys.exit(2)
    ctx.run([git, "clone", repository, target_dir])
    with mev_build_utils.temporary_chdir(target_dir):
        ctx.run(['python', "bootstrap.py"])
        ctx.run([os.path.join('venv', 'scripts', 'invoke'),
                 'configure', '--config', config,
                 'build', '--config', config])

@ctask
def distclean(ctx):
    "Delete generated and downloaded files"

    ctx.run('cd third-party && invoke distclean')
    build_odm(ctx, args='/t:Clean')
