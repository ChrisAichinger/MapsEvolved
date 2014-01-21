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


class LocalizedMakefile(sipconfig.SIPModuleMakefile):
    """Makefile generator that adds rules to build within a subdirectory

    We want out build to happen in a subdirectory, so include some rules in the
    Makefile to enable this. We define the desired output directory as
    BUILDDIR, then write a rule to build .cpp files to .obj within $(BUILDDIR).
    """
    def __init__(self, configuration, build_file, install_dir=None, static=0,
                 console=0, qt=0, opengl=0, threaded=0, warnings=1, debug=0,
                 dir=None, makefile="Makefile", installs=None, strip=1,
                 export_all=0, universal=None, arch=None, prot_is_public=0,
                 deployment_target=None, build_dir=None, binaries_dir=None,
                 binaries_copy=[]):

        sipconfig.SIPModuleMakefile.__init__(self, configuration, build_file,
                install_dir, static, console, qt, opengl, threaded, warnings,
                debug, dir, makefile, installs, strip, export_all, universal,
                arch, deployment_target)

        self._build["extra_objects"] = ""
        self._build_dir = build_dir
        self._bin_dir = binaries_dir
        self._binaries_copy = binaries_copy

        if sys.platform == "win32":
            self.rmdir = "rmdir /S /Q"
        else:
            self.rmdir = "rm -rf"

    def generate_macros_and_rules(self, mfile):
        if self._build_dir is not None:
            mfile.write("BUILDDIR = %s\n" % self._build_dir)

        sipconfig.SIPModuleMakefile.generate_macros_and_rules(self, mfile)

        if self._build_dir is not None:
            input_dirs = ["$(BUILDDIR)", 'src_sip']
            for d in input_dirs:
                mfile.write("\n")
                mfile.write("{" + d + "}.cpp{$(BUILDDIR)}.obj::\n")
                mfile.write("\t$(CXX) -c $(CXXFLAGS) $(CPPFLAGS) " +
                                         "-Fo$(BUILDDIR)\\ -Fd$(BUILDDIR)\\ " +
                                         "@<<\n")
                mfile.write("\t$<\n")
                mfile.write("<<\n")

        if self._bin_dir is not None:
            target_names = [os.path.join(self._bin_dir, os.path.basename(f))
                            for f in self._binaries_copy]
            mfile.write("\nall: $(TARGET) %s\n\n" % ' '.join(target_names))

            for src, dest in zip(self._binaries_copy, target_names):
                mfile.write("%s: %s\n" % (dest, src))
                mfile.write("\t%s %s %s\n\n" % (self.copy, src, dest))

        if self._build['extra_objects']:
            mfile.write("$(TARGET): %s\n" % self._build['extra_objects'])


    def generate_target_clean(self, mfile):
        if self._build_dir is None:
            sipconfig.SIPModuleMakefile.generate_target_clean(self, mfile)
        else:
            # Guard against the most common cases of deleting important dirs.
            assert self._build_dir not in ('.', '..')
            mfile.write("\nclean:\n")
            mfile.write("\t-%s $(BUILDDIR)\n" % self.rmdir)

        if self._bin_dir is not None:
            # Delete binaries_dir/`basename $(TARGET)`
            target_names = [os.path.join(self._bin_dir, os.path.basename(f))
                            for f in self._binaries_copy]
            for target_name in target_names:
                mfile.write("\t-%s %s\n" % (self.rm, target_name))


def main():
    build_dir = '__build'
    binaries_dir = 'pymaplib'
    # The SIP build file used by the build system.
    build_file = os.path.join(build_dir, "maps.sbf")

    os.makedirs(build_dir, exist_ok=True)

    configuration = sipconfig.Configuration()

    with MTimeSaver(build_dir, ['*.cpp', '*.c', '*.h']):
        # Run SIP to generate the C++ code.
        os.system(" ".join([configuration.sip_bin,
                            "-c", build_dir,        # Generated code directory
                            "-b", build_file,       # Build file name
                            "-T",                   # Suppress timestamps
                            "-e",                   # Enable exception support
                            "-w",                   # Enable warnings
                            "-o",                   # Auto-generate docstrings
                            "-I src_sip",           # Additional include dir
                            "src_sip\\maps.sip"]))  # Input file

    inject_build_directory_to_buildfile(build_file, build_dir)

    # Override the default CFLAGS/CXXFLAGS, since they contain
    # -Zc:wchar_t-, which we don't want.
    configuration.build_macros()['CFLAGS'] = '-nologo'
    configuration.build_macros()['CXXFLAGS'] = '-nologo'
    configuration.build_macros()['CFLAGS_RELEASE'] = '/Zi /MDd'
    configuration.build_macros()['CXXFLAGS_RELEASE'] = '/Zi /MDd'

    makefile = LocalizedMakefile(configuration, build_file,
                                 build_dir=build_dir,
                                 binaries_dir=binaries_dir, binaries_copy=[
                                     os.path.join(build_dir, 'maplib_sip.pyd'),
                                     "ODM\\Debug-DLL\\OutdoorMapper.dll"
                                     ]
                                 )

    makefile.extra_cxxflags.append('/EHsc')          # Exception support
    makefile.extra_cxxflags.append('/Zc:wchar_t')    # wchar_t as builtin type
    makefile.extra_cxxflags.append('/D "WIN32" /D "STRICT" /D "NOMINMAX"')
    makefile.extra_cxxflags.append('/D "_DEBUG" /D "_WINDOWS" /D "_WINDLL"')
    makefile.extra_cxxflags.append('/D "_UNICODE" /D "UNICODE"')
    makefile.extra_cxxflags.append('/W3 /WX /Od /Oy- /Gm /RTC1 /GS')
    makefile.extra_cxxflags.append('/fp:precise')
    makefile.extra_lflags.append('/DEBUG /PDB:maps.pdb')

    # Add the library we are wrapping. The name doesn't include any platform
    # specific prefixes or extensions.
    makefile.extra_libs = ["OutdoorMapper"]
    makefile.extra_include_dirs.append('ODM/OutdoorMapper')
    makefile.extra_include_dirs.append('src_sip')
    makefile.extra_lib_dirs.append('ODM/Debug-DLL')
    makefile._build["extra_objects"] += " ODM\\Debug-DLL\\OutdoorMapper.lib"

    makefile._build["objects"] += " " + os.path.join(build_dir,
                                                     "smartptr_proxy.obj")

    # Add Makefile to headers, so everything is rebuilt if it changes.
    makefile._build["headers"] += " src_sip\\smartptr_proxy.h"
    makefile._build["headers"] += " Makefile"
    # Force a rebuild of $(TARGET) if the lib changed

    # Generate the Makefile itself.
    with MTimeSaver('Makefile'):
        makefile.generate()


if __name__ == '__main__':
    main()
