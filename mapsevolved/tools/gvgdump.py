#!/usr/bin/python3

import sys
import os
import io
import glob
import argparse

try:
    import pymaplib
except ImportError:
    sys.path.append(os.path.join(os.path.dirname(__file__), '..', '..'))
    import pymaplib


def analyze(fname):
    gvgf = pymaplib.GVGFile(fname)
    gmpi = pymaplib.MakeBestResolutionGmpImage(gvgf)
    print(fname)
    print(gvgf.RawDataString())
    print(gmpi.DebugData())
    print()
    sys.stdout.flush()

def main():
    # Ensure we don't crash writing to the Windows console.
    sys.stdout = io.TextIOWrapper(sys.stdout.buffer, encoding='utf8',
                                  errors='replace')

    parser = argparse.ArgumentParser(description='Dump GVG metadata.')
    parser.add_argument('files', nargs='+', help='GVG files to process')
    args = parser.parse_args()
    files = []
    for fname in args.files:
        if '*' in fname:
            files.extend(glob.glob(fname))
        else:
            files.append(fname)
    for fname in files:
        analyze(fname)

if __name__ == '__main__':
    main()
