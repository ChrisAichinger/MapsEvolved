#!/bin/bash
# This script calculates some variables and then calls make with the arguments
# supplied on the command line. No active building happens in here, we just set
# everything up for the Makefile to do its job.
# Unfortunately, this approach is required as nmake.exe is extremely limited.

set -e

ODM_PATH=../OutdoorMapper
PROJECT=$ODM_PATH/OutdoorMapper.vcxproj
BUILD_PATH=build

function assert_1_line() {
    if [ `echo "$1" | wc -l` -ne 1 ]; then
        echo "$2"
        exit 1
    fi
}

function extract_xml() {
echo "$1" | sed -e "s/.*[>]\(.\+\)[<]\/.*/\1/"            \
          | sed -e 's/[$](SolutionDir)/../g'              \
          | sed -e 's/%(AdditionalIncludeDirectories)//'  \
          | sed -e 's/%(AdditionalDependencies)//'        \
          | sed -e 's/%(AdditionalLibraryDirectories)//'  \
          | sed -e 's/;*$//'
}

function csv_to_cmdline() {
    echo "$1$2" | sed -e "s/;/ $1/g"
}

function basename_to_buildpath() {
    for i in "$@"; do
        echo "$BUILD_PATH/$i" | sed -e 's/\.cpp/\.obj/'
    done
}

# Fish include dirs, dependencies and lib dirs out of the project file.
# uniq unifies debug and release build lines
INCL=`grep "<AdditionalIncludeDirectories>" "$PROJECT" | uniq`
DEPS=`grep "<AdditionalDependencies>" "$PROJECT" | uniq`
LIBDIR=`grep "<AdditionalLibraryDirectories>" "$PROJECT" | uniq`

# Error out if debug and release builds have different values
assert_1_line "$INCL" "Different include dirs between Release & Debug!"
assert_1_line "$DEPS" "Different link dependencies between Release & Debug!"
assert_1_line "$LIBDIR" "Different library dirs between Release & Debug!"

# Strip xml and build a valid commandline from the data
INCL=`extract_xml "$INCL"`
INCL=`csv_to_cmdline -I "$INCL"`

DEPS=`extract_xml "$DEPS"`
DEPS=`csv_to_cmdline "" "$DEPS"`

LIBDIR=`extract_xml "$LIBDIR"`
LIBDIR=`csv_to_cmdline "-LIBPATH:" "$LIBDIR"`


# .OBJ file targets for all .cpp's in OutdoorMapper (i.e. the code to test)
# e.g. build\win_main.obj build\OutdoorMapper.obj build\projection.obj
#      Notice the backslashes; this is handled by `tr / \134`
odm_src_basenames=`ls $ODM_PATH/*.cpp | xargs -n 1 basename | tr '\n' ' '`
odm_src_targets=`basename_to_buildpath $odm_src_basenames | tr '\n/' ' \134'`


# Final invocation of nmake through run_make.js
cscript.exe ..\\tools\\run_make.js "" Makefile                     \
    "odm_path= $ODM_PATH" "build_path= $BUILD_PATH"                \
    "extra_include_dirs= $INCL"                                    \
    "extra_library_dirs= $LIBDIR" "extra_lib_dependencies= $DEPS"  \
    "odm_src_targets= $odm_src_targets"                            \
    "$@"
