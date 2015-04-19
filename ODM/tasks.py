import os

from invoke import ctask

TARGETS = ['Debug', 'Release', 'tests']

@ctask(help={'config': 'Which configuration to build: debug/release',
             'args': 'Additional arguments for msbuild'})
def build(ctx, config=None, args=''):
    "Build the OutdoorMapper library"

    cmd = 'call "%VS100COMNTOOLS%\\vsvars32.bat" && msbuild '
    if config is not None:
        cmd += '/p:Configuration={config} '
    cmd += args
    ctx.run(cmd.format(config=config))

@ctask(help={'config': 'Which configuration to build: debug/release'})
def configure(ctx, config):
    "Setup libraries and perform an initial build"

    cmd = ' '.join(['cd libraries && invoke distclean download',
                    'build --config {config}',
                    'publish "--targets={targets}"'])
    targets = ';'.join(os.path.abspath(target) for target in TARGETS)
    ctx.run(cmd.format(config=config, targets=targets))
    build(ctx, config=config)

@ctask
def distclean(ctx):
    "Delete generated and downloaded files"

    ctx.run('cd libraries && invoke distclean')
    build(ctx, args='/t:Clean')
