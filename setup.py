#!/usr/bin/python3

import sys
import os
import io
import glob
import importlib

from setuptools import setup
import py2exe

# Allow setup.py to be run from any path.
os.chdir(os.path.normpath(os.path.join(os.path.abspath(__file__), os.pardir)))


# Manually load mapsevolved to get the local version independent of sys.path.
loader = importlib.machinery.SourceFileLoader("mapsevolved",
                                              "mapsevolved/__init__.py")
mapsevolved = loader.load_module()


def read(*filenames, encoding='utf-8', filesep='\n'):
    buf = []
    for filename in filenames:
        with io.open(filename, encoding=encoding) as f:
            buf.append(f.read())
    return filesep.join(buf)


cfg = dict(
    name='MapsEvolved',
    version=mapsevolved.__version__,
    url='',
    license='TBD',
    description='Map Viewer Optimized for Hiking and Ski Touring',
    long_description=read('README.mkd'),
    author='Christian Aichinger',
    author_email='Greek0@gmx.net',
    install_requires=['wxPython-Phoenix>=3.0.3', 'gpxpy>=0.9.8'],
    extras_require={
        'convert_amapv5_poi': ['pyodbc'],
    },
    packages=['pymaplib', 'mapsevolved', 'mapsevolved.tools'],
    platforms='Windows',
    package_data = {
        'pymaplib': ['*.dll', '*.pyd'],
    },
    zip_safe=False,
    data_files=[
        ('.', ['README.mkd', 'LICENSE.mkd']),
        ('csv', glob.glob('pymaplib/csv/*.*')),
        ('mapsevolved', glob.glob('mapsevolved/*.xrc')),
        ('mapsevolved/data', glob.glob('mapsevolved/data/*.*')),
        ('mapsevolved/data/famfamfam_silk_icons',
         glob.glob('mapsevolved/data/famfamfam_silk_icons/*.*')),
    ],
    entry_points={
        'gui_scripts': [
            'MapsEvolved = mapsevolved.main:main',
        ],
        'console_scripts': [
            'map2orux = mapsevolved.tools.map2orux:main',
            'convert_amapv5_poi = mapsevolved.tools.convert_amapv5_poi:main',
            'gvgdump = mapsevolved.tools.gvgdump:main',
        ],
    },
    options={"py2exe": {
        "compressed": True,
        "optimize": 1,
        'bundle_files': 1,
        "dist_dir": "dist/MapsEvolved-win32",
        "packages": ['wx.lib.pubsub',
                     'wx.lib.pubsub.core',
                     'wx.lib.pubsub.core.kwargs'],
        "includes": ['sip'],
        "ignores": ['lxml', 'ipdb'],
        },
    },
    windows=[{
        "script": "MapsEvolved.py",
        "dest_base": "MapsEvolved",  # Output executable name (MapsEvolved.exe)
        "version": mapsevolved.__version__.replace('-dev', ''),
        "company_name": u"",
        "copyright": u"Copyright 2012-2015 Christian Aichinger",
        "name": "MapsEvolved",
        "description": "MapsEvolved",
        'icon_resources': [(1, os.path.join('mapsevolved', 'data',
                                            'famfamfam_silk_icons', 'map.ico'))],
        },
    ],
    #test_suite='sandman.test.test_sandman',
    #tests_require=['pytest'],
    classifiers=[
        'Programming Language :: Python',
        'Development Status :: 4 - Beta',
        'Natural Language :: English',
        'Intended Audience :: End Users/Desktop',
        'Environment :: Win32 (MS Windows)',
        'Operating System :: Microsoft :: Windows'
        'Programming Language :: C++',
        'Programming Language :: Python :: 3',
        'Programming Language :: Python :: 3.4',
        'Topic :: Multimedia :: Graphics :: Viewers',
        'Topic :: Scientific/Engineering :: GIS',
        #'License :: OSI Approved :: Apache Software License',
    ],
)

if __name__ == '__main__':
    if sys.argv[-1] == 'sdist':
        sys.exit(os.system('invoke sdist'))
    else:
        setup(**cfg)
