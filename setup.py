#!/usr/bin/python3

import sys
import os
import io

from setuptools import setup


# allow setup.py to be run from any path
os.chdir(os.path.normpath(os.path.join(os.path.abspath(__file__), os.pardir)))

def read(*filenames, encoding='utf-8', filesep='\n'):
    buf = []
    for filename in filenames:
        with io.open(filename, encoding=encoding) as f:
            buf.append(f.read())
    return filesep.join(buf)


setup(
    name='MapsEvolved',
    version='0.1-dev',
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
        'pymaplib': ['csv/*.c', 'csv/*.csv', '*.dll', '*.pyd'],
        'mapsevolved': ['data/*.*', 'data/famfamfam_silk_icons/*.*'],
    },
    zip_safe=False,
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
