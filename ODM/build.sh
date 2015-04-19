#!/bin/bash
# RUN WITH GIT BASH

set -e

if [ "x$1" = "xDEBUG" ]; then
    cd libraries
    invoke distclean
    invoke download build --config DEBUG publish --targets ../Debug;../Release;../tests
    cd ..
    cmd.exe \/c 'tools\\build.bat /p:Configuration=Debug'
elif [ "x$1" = "xRELEASE" ]; then
    cd libraries
    invoke distclean
    invoke download build --config RELEASE publish --targets ../Debug;../Release;../tests
    cd ..
    cmd.exe \/c 'tools\\build.bat /p:Configuration=Release'
else
    echo "Usage: ./build <RELEASE|DEBUG>"
    echo "  Run this once to setup libraries and perform an initial build."
    echo "  Perform further builds using Visual Studio 2010 Express."
fi


