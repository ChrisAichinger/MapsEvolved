import sys
import os
import glob
import hashlib
import itertools
import contextlib

import sipconfig


def inject_build_directory_to_buildfile(build_file, build_dir):
    """Include build_dir in the paths of files in build_file

    The SIP make system uses the paths stored in build_file to write the
    Makefile. We want out generated source code and the object files to go into
    a subdirectory, which is not directly supported by SIP. Thus, we modify the
    build_file to include the output directory within the source/header
    filenames directly.
    """

    with open(build_file, 'r+') as f:
        lines = f.readlines()
        output_lines = []
        for line in lines:
            key, value = [s.strip() for s in line.split('=', 1)]
            if key in ('sources', 'headers', 'target'):
                files = [os.path.join(build_dir, s) for s in value.split(' ')]
                value = ' '.join(files)
            output_lines.append(key + ' = ' + value)
        f.seek(0)
        f.truncate()
        f.write('\n'.join(output_lines))


@contextlib.contextmanager
def MTimeSaver(dirname, globs=None):
    """Reset files' mtime to earlier values if they did not change

    SIP overwrites the generated files each time it is called, even if the
    contents did not change. This causes `make` to pointlessly regenerate
    dependent files.

    To avoid this, store mtime and file contents' hash prior to calling SIP.
    Afterwords, restore the previous mtime if the files did not change.

    If globs is not given, the directory or file `dirname` is monitored.
    If globs is specified, `dirname/<globs>` is monitored.
    """

    if globs:
        globs = [glob.iglob(os.path.join(dirname, g)) for g in globs]
        it = itertools.chain(*globs)
    else:
        if os.path.isdir(dirname):
            it = glob.iglob(os.path.join(dirname, '*'))
        elif os.path.exists(dirname):
            it = [dirname]
        else:
            raise RuntimeError("MTimeSaver: Target directory does not exist")

    files = {}
    for fname in it:
        mtime = os.stat(fname).st_mtime
        fhash = hashlib.sha256(open(fname, 'rb').read()).digest()
        files[fname] = (mtime, fhash)

    yield

    for fname in files.keys():
        try:
            content = open(fname, 'rb').read()
        except (PermissonError, FileNotFoundError):
            continue
        st = os.stat(fname)
        new_mtime = st.st_mtime
        new_hash = hashlib.sha256(content).digest()
        old_mtime, old_hash = files[fname]
        if new_hash == old_hash and new_mtime > old_mtime:
            os.utime(fname, (st.st_atime, old_mtime))


def main():
    build_dir = '__build'
    binaries_dir = 'pymaplib'
    # The SIP build file used by the build system.
    build_file = os.path.join(build_dir, "maps.sbf")

    os.makedirs(build_dir, exist_ok=True)

    configuration = sipconfig.Configuration()

    with MTimeSaver(build_dir, ['*.cpp', '*.c', '*.h']):
        # Run SIP to generate the C++ code.
        retval = os.system(" ".join([
                            configuration.sip_bin,
                            "-c", build_dir,        # Generated code directory
                            "-b", build_file,       # Build file name
                            "-T",                   # Suppress timestamps
                            "-e",                   # Enable exception support
                            "-w",                   # Enable warnings
                            "-o",                   # Auto-generate docstrings
                            "-I src_sip",           # Additional include dir
                            "src_sip\\maps.sip"]))  # Input file
        if retval != 0:
            sys.exit(retval)

    inject_build_directory_to_buildfile(build_file, build_dir)


if __name__ == '__main__':
    main()
