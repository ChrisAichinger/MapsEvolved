#!../venv/Scripts/python
import sys
import re
import glob
import subprocess
import collections

import mev_build_utils

class CRTLib:
    """Encapsulate C runtime (CRT) name, version and configuration

    Extract runtime name, version and configuration from a CRT library
    filename.

    Attributes:

    * `name`: Base CRT name (typically 'MSVC')
    * `version`: Runtime version number (`int`)
    * `config`: Build configuration

    CRTLib supports equality comparison and is hashable.
    """

    def __init__(self, libname):
        """Parse the a CRT library filename"""

        m = re.match('MSVC[PR](\d+)(D?)(.dll)?', libname, re.IGNORECASE)
        if not m:
            raise ValueError("Failed to parse crtlib name: '%s'" % libname)
        self.name = 'MSVC'
        self.version = int(m.group(1))
        self.config = 'debug' if m.group(2) else 'release'

    def __key(self):
        return (self.name, self.version, self.config)
    def __hash__(self):
        return hash(self.__key())
    def __eq__(self, other):
        return self.__key() == other.__key()
    def __ne__(self, other):
        return not self == other
    def __str__(self):
        return ' '.join((self.name, str(self.version), self.config))

def get_file_dependencies(filename):
    """Return runtime library dependencies of a file

    Return a CRTLib object for every runtime library dependence of
    `filename`. Only CRT dependencies are analyzed, since they are the
    easiest to get wrong.
    """

    dumpbin = mev_build_utils.get_vs_executable('dumpbin.exe')
    output = subprocess.check_output([dumpbin, '/DEPENDENTS', filename],
                                     universal_newlines=True)
    lines = output.split('\n')
    libnames = {line.strip() for line in lines if 'MSVC' in line}
    return {CRTLib(n) for n in libnames}

def verify_consistent_crtlibs(files):
    """Check if all files link to the same CRT libraries

    For every filename in `files`, determine which CRT libraries it links
    against. If all files link against the same version/configuration of the
    CRT, return True.

    Otherwise print an error message to the console and return False.
    """

    crtlib_map = {fname: get_file_dependencies(fname) for fname in files}
    collected_libs = [lib for crtlibs in crtlib_map.values()
                          for lib in crtlibs]
    if len(set(collected_libs)) <= 1:
        return True

    # A sorted list of (lib, nr_of_occurrences), most common lib at the end.
    counts = collections.Counter(collected_libs)
    counts = sorted(counts.items(), key=lambda item: item[1])

    libs = [lib for lib, count in counts]
    most_common = libs.pop()
    print('Error: multiple runtime library versions detected.')
    print('Most common: {lib}'.format(lib=most_common))
    print('Files with diverging runtime libraries:')
    diverging = set(libs)
    for fname in crtlib_map:
        fname_divergant_libs = crtlib_map[fname].intersection(diverging)
        if fname_divergant_libs:
            print(fname, ':', '; '.join(str(l) for l in fname_divergant_libs))
    return False


def main():
    """Usage: check_linked_libs <files>

    Verify that <files> link only against a single version of the platform
    runtime library.

    We must not mix different library releases (versions, e.g. MSVCR100.dll vs.
    MSVCR120.dll) or configurations (release with debug, e.g. MSVCR100D.dll).
    Otherwise we run into problems on deployment.

    For an outdated, but well written introduction, see
    http://www.virtualdub.org/blog/pivot/entry.php?id=296
    """

    if not sys.argv[1:]:
        print(main.__doc__)
        sys.exit(1)

    args = []
    for arg in sys.argv[1:]:
        if '*' in arg or '?' in arg:
            args.extend(glob.iglob(arg))
        else:
            args.append(arg)

    print('Verifying linked CRT libraries of the following files:')
    print('  ' + '\n  '.join(args))
    print()

    if verify_consistent_crtlibs(args):
        print('Runtime library linkage is consistent (good thing).')
    else:
        sys.exit(1)

if __name__ == '__main__':
    main()
