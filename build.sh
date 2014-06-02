#!/bin/bash
# RUN WITH GIT BASH

set -e

if [ "x$1" = "xDEBUG" ]; then
    cd libraries
    ./build_environ.sh delete || true
    ./build_environ.sh download build DEBUG copydll
    cd ..
    cmd.exe \/c 'tools\\build.bat /p:Configuration=Debug-DLL'
elif [ "x$1" = "xRELEASE" ]; then
    cd libraries
    ./build_environ.sh delete || true
    ./build_environ.sh download build RELEASE copydll
    cd ..
    cmd.exe \/c 'tools\\build.bat /p:Configuration=Release-DLL'
else
    echo "Usage: ./build <RELEASE|DEBUG>"
    echo "  Run this once to setup libraries and perform an initial build."
    echo "  Perform further builds using Visual Studio 2010 Express."
fi


