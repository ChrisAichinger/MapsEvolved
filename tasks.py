import sys
import os
import glob
import shutil
import tarfile
import zipfile

from invoke import Collection, Task, ctask

import mev_build_utils


tasks_thirdparty = mev_build_utils.import_from_file('tasks_thirdparty',
                                                    'third-party/tasks.py')
tasks_sip = mev_build_utils.import_from_file('tasks_sip',
                                             'pymaplib_cpp/sip/tasks.py')

TARGETS = ['pymaplib_cpp\\dist', 'pymaplib']


@ctask(help={'config': 'Which configuration to build: debug/release'})
def configure(ctx, config):
    """Setup libraries and perform an initial build"""
    tasks_thirdparty.distclean(ctx)
    tasks_thirdparty.download(ctx)
    tasks_thirdparty.build(ctx, config=config)
    copy_thirdparty_libs(ctx)

@ctask
def copy_thirdparty_libs(ctx):
    """Extract built library files from the third-party directory"""
    for target in TARGETS:
        os.makedirs(target, exist_ok=True)
    targets = ';'.join(os.path.abspath(t) for t in TARGETS)
    tasks_thirdparty.publish(ctx, targets=targets)

@ctask(help={'config': 'Which configuration to build: debug/release (default: as last build)',
             'args': 'Additional arguments for msbuild'})
def build_cpp(ctx, config=None, args=''):
    """Build the pymaplib_cpp library"""
    cmd = 'call "%VS100COMNTOOLS%\\vsvars32.bat" && cd pymaplib_cpp && msbuild '
    if config is not None:
        cmd += '/p:Configuration={config} '
    cmd += args
    ctx.run(cmd.format(config=config))

@ctask(help={'config': 'Which configuration to build: debug/release'})
def build(ctx, config):
    """Build pymaplib_cpp and its SIP bindings"""
    build_cpp(ctx, config)
    tasks_sip.build(ctx, config)
    copy_thirdparty_libs(ctx)
    shutil.copy(os.path.join('pymaplib_cpp', 'dist', 'pymaplib_cpp.dll'), 'pymaplib')
    shutil.copy(os.path.join('pymaplib_cpp', 'dist', 'maplib_sip.pyd'), 'pymaplib')

    print()
    print('*******************************')
    print('* Build finished successfully *')
    print('*******************************')
    print()

@ctask(help={'repository': 'Path to the git repo to build',
             'target_dir': 'Working directory for checkout and build',
             'config': 'Which configuration to build: debug/release',
             'force': 'Delete target dir if it exists before starting'})
def checkout_and_build(ctx, repository, target_dir, config, force=False):
    '''Perform GIT checkout and build'''
    if os.path.exists(target_dir):
        if not force:
            print('Target dir already exists:', target_dir, file=sys.stderr)
            sys.exit(2)
        else:
            mev_build_utils.resilient_delete(target_dir, recursive=True)

    git = mev_build_utils.find_git_executable()
    if not git:
        print('Could not find git executable', file=sys.stderr)
        sys.exit(2)
    ctx.run([git, 'clone', repository, target_dir])

    with mev_build_utils.temporary_chdir(target_dir):
        # Disable Python output buffering to get consistent log output.
        os.environ['PYTHONUNBUFFERED'] = '1'
        ctx.run(['python', "bootstrap.py"])
        mev_build_utils.run_in_venv(ctx, 'venv',
                                    ['invoke',
                                     'configure', '--config', config,
                                     'build', '--config', config,
                                     'crtcheck', 'py2exe'])

@ctask
def clean(ctx):
    """Delete files generated during the pymaplib_cpp build"""

    paths = mev_build_utils.multiglob(
            'pymaplib/pymaplib_cpp.dll', 'pymaplib/*.pyd',
            'pymaplib_cpp/build', 'pymaplib_cpp/dist',
            )
    for p in paths:
        if not os.path.exists(p):
            continue
        if os.path.isdir(p):
            shutil.rmtree(p)
        else:
            os.unlink(p)

@ctask
def distclean(ctx):
    """Delete generated and downloaded files"""
    tasks_thirdparty.distclean(ctx)
    build_cpp(ctx, args='/t:Clean')

    paths = mev_build_utils.multiglob(
            'build', 'dist', '*.egg-info',
            'pymaplib/*.dll', 'pymaplib/*.pyd', 'pymaplib/csv',
            'pymaplib_cpp/build', 'pymaplib_cpp/dist',
            )
    for p in paths:
        if not os.path.exists(p):
            continue
        if os.path.isdir(p):
            shutil.rmtree(p)
        else:
            os.unlink(p)

@ctask
def crtcheck(ctx):
    """Verify that we link only against a single version of the CRT library"""
    files = list(mev_build_utils.multiglob('pymaplib/*.pyd', 'pymaplib/*.dll'))
    init_sh = os.path.join("tools", "init_shell.py")
    check = os.path.join("tools", "check_linked_libs.py")

    os.environ['PYTHONUNBUFFERED'] = '1'
    ctx.run([sys.executable, init_sh, 'venv', 'python', check] + files)

@ctask
def sdist(ctx):
    """Create a source distribution of our code"""
    if not os.path.exists('.git'):
        print("Can't build an sdist in this tree (need a git repository).")
        sys.exit(1)

    if sys.platform == 'win32':
        # Add Git directory to %PATH% to get 'git' and 'tar'
        gitdir = mev_build_utils.find_git_bindir()
        os.environ['PATH'] = ';'.join([gitdir, os.environ['PATH']])

    BUILDDIR = os.path.join('build', 'sdist')
    os.makedirs(BUILDDIR, exist_ok=True)
    os.makedirs('dist', exist_ok=True)

    print("Exporting from git...")
    ctx.run('git archive HEAD | tar -x -C {}'.format(BUILDDIR), shell=True)

    print("Generating metadata...")
    ctx.run([sys.executable, 'setup.py', 'egg_info', '--egg-base', BUILDDIR])
    shutil.copy(os.path.join(BUILDDIR, 'MapsEvolved.egg-info/PKG-INFO'),
                os.path.join(BUILDDIR, 'PKG-INFO'))

    # Making sane tar.gz files on Windows is hard, just don't do it for now.
    # Cf. nedbatchelder.com/blog/201009/making_good_tar_files_on_windows.html
    print("Creating zip file...")
    import setup
    basename = 'dist/{cfg[name]}-{cfg[version]}-src'.format(cfg=setup.cfg)
    zipname = shutil.make_archive(basename, 'zip', BUILDDIR)

    print("Cleaning up...")
    shutil.rmtree(BUILDDIR)

    print("Source distribution built in '{}'.".format(zipname))

@ctask
def py2exe(ctx):
    """Create a binary distribution for running on Windows"""

    os.makedirs('dist', exist_ok=True)
    ctx.run([sys.executable, 'setup.py', 'py2exe'])

    # Copy MSVC runtime libraries into the dist folder
    redist = r'%VS100COMNTOOLS%..\..\VC\redist\x86\Microsoft.VC100.CRT'
    redist = os.path.expandvars(redist)
    libs = glob.glob(os.path.join(redist, 'msvc?100.dll'))
    if not libs:
        print("Warning: Could not copy CRT libraries. Expect deployment problems.",
              file=sys.stderr)
        return
    BUILDDIR = 'dist\\MapsEvolved-win32'
    for lib in libs:
        shutil.copy(lib, BUILDDIR)
    zipname = shutil.make_archive(BUILDDIR, 'zip', BUILDDIR)
    print("Successfully created Windows binary distribution.",
          file=sys.stderr)


ns = Collection(*[obj for obj in vars().values() if isinstance(obj, Task)])
ns.add_collection(Collection.from_module(tasks_thirdparty, name='third-party', loaded_from='.'))
ns.add_collection(Collection.from_module(tasks_sip, name='sip', loaded_from='.'))
ns.configure({'run': { 'runner': mev_build_utils.LightInvokeRunner }})
