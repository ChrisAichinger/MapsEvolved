#!/bin/bash

set -e
#set -x

JPEG_DIR="libjpeg"
JPEG_URL="http://switch.dl.sourceforge.net/project/gnuwin32/jpeg/6b-4/jpeg-6b-4-src.zip"

TIF_DIR="libtiff"
TIF_URL="http://download.osgeo.org/libtiff/tiff-4.0.3.tar.gz"

GEOTIF_DIR="libgeotiff"
GEOTIF_URL="ftp://ftp.remotesensing.org/pub/geotiff/libgeotiff/libgeotiff-1.4.0.tar.gz"

PROJ4_DIR="proj4"
PROJ4_URL="http://download.osgeo.org/proj/proj-4.8.0.tar.gz"

GEOGRAPHICLIB_DIR="geographiclib"
GEOGRAPHICLIB_URL="http://switch.dl.sourceforge.net/project/geographiclib/distrib/GeographicLib-1.36.tar.gz"

COPY_TARGET1="../Debug"
COPY_TARGET2="../Release"
COPY_TARGET3="../Debug-DLL"
COPY_TARGET4="../Release-DLL"
COPY_TARGET5="../tests"

DO_MAKE="cscript.exe ..\\tools\\run_make.js"

abspath () {
    case "$1" in
        /*) printf "%s\n" "$1";;
        *) printf "%s\n" "$PWD/$1";;
    esac;
}

# Usage: download_one "$URL" "$TARGET_DIR" "$IGNORE_TAR_FAIL"
# Download URL, unpack the resulting file into TARGET_DIR
#   If IGNORE_TAR_FAIL is "true", tar failures are ignored
#   (necessary for libgeotiff-1.4.0)
function download_one() {
    echo "Downloading $1"
    curl -L -O "$1"
    local f_name=`basename "$1"`

    if echo "$f_name" | grep -q ".zip$"; then
        # ZIP file
        unzip -d "$2" "$f_name"

    elif echo "$f_name" | grep -q ".tar.gz$"; then
        # TAR.GZ file

        local f_name_abs=`abspath "$f_name"`

        pushd .
        mkdir -p "$2"
        cd "$2"

        # Ignore return code of tar, the latest geotiff shows warnings with
        # windows tar: Archive value 142957 is out of uid_t range 0..65535
        if $3; then
            tar xzf "$f_name_abs" ||  true
        else
            tar xzf "$f_name_abs"
        fi

        sleep 5

        shopt -s dotglob
        find . -type d -mindepth 1 -maxdepth 1 -print0 | \
        while IFS= read -r -d $'\0' subdir; do
            set -e
            mv $subdir/* . || { sleep 5; mv $subdir/* .; }
            rmdir "$subdir"
        done

        popd

    else
        # Unknown file
        echo "Unknown file extension: $f_name"
        exit
    fi
}

# libjpeg comes with a weird directory hierarchy; fix that.
# Essentially move src/jpeg/6b/jpeg-6b-src -> .
function fix_jpeg() {
    DIR="$1"    ## JPEG_DIR
    pushd .
    cd "$DIR"
    rm -rf manifest
    mv src src.weird_dirs
    mv src.weird_dirs/*/*/*-src/* .
    popd
}

# Print usage and exit
function usage() {
    set +x
    echo 1>&2
    echo "$1" 1>&2
    echo "commands:" 1>&2
    echo "  download                            -- Download & unpack" 1>&2
    echo "  build <DEBUG|RELEASE|CLEAN>         -- Build all" 1>&2
    echo "  delete                              -- Delete generated files" 1>&2
    echo "  copydll                             -- Copy dlls to ../Debug" 1>&2
    echo "  buildjpeg <DEBUG|RELEASE|CLEAN>     -- Only build jpeg" 1>&2
    echo "  buildproj <DEBUG|RELEASE|CLEAN>     -- Only build proj4" 1>&2
    echo "  buildtiff <DEBUG|RELEASE|CLEAN>     -- Only build libtiff" 1>&2
    echo "  buildgeotiff <DEBUG|RELEASE|CLEAN>  -- Only build geotiff" 1>&2
    exit 1
}

# Get the matching compiler flags for $1 == DEBUG/RELEASE/CLEAN
function debug_flags() {
    if [ "x$1" = "xDEBUG" ]; then
        echo "-D_MT -MDd /Zi /Od"
    elif [ "x$1" = "xRELEASE" ]; then
        echo "-D_MT -MD /Ox"
    elif [ "x$1" = "xCLEAN" ]; then
        echo "-D_MT -MD /Ox"
    else
        usage "buildjpeg needs as argument either DEBUG, RELEASE or CLEAN"
    fi
}

# Get make targets for $1 == DEBUG/RELEASE/CLEAN (-> all, all, clean)
function targets() {
    if [ "x$1" = "xDEBUG" ]; then
        echo ""
    elif [ "x$1" = "xRELEASE" ]; then
        echo ""
    elif [ "x$1" = "xCLEAN" ]; then
        echo "clean"
    else
        usage "buildjpeg needs as argument either DEBUG, RELEASE or CLEAN"
    fi
}

if [ -z "$1" ]; then
    usage
fi

while [ -n "$1" ]; do
    if [ "x$1" = "xdownload" ]; then
        download_one "$JPEG_URL" "$JPEG_DIR" false;
        download_one "$PROJ4_URL" "$PROJ4_DIR" false;
        download_one "$TIF_URL" "$TIF_DIR" false;
        download_one "$GEOTIF_URL" "$GEOTIF_DIR" true;
        download_one "$GEOGRAPHICLIB_URL" "$GEOGRAPHICLIB_DIR" false;

        fix_jpeg "$JPEG_DIR"
        patch -p0 --forward < jpeg.diff
        patch -p0 --forward < libtiff.diff
        patch -p0 --forward < libgeotiff.diff
        patch -p0 --forward < proj4.diff
        patch -p0 --forward < geographiclib.diff

    elif [ "x$1" = "xbuildjpeg" ]; then
        cp "$JPEG_DIR/jconfig.vc" "$JPEG_DIR/jconfig.h"

        dflags=`debug_flags "$2"`   # cl.exe flags: /MDd, /MD, /Zi, ...
        mtargets=`targets "$2"`     # all, clean, install, etc.
        $DO_MAKE "$JPEG_DIR" Makefile.vc "cvars= $dflags" $mtargets
        shift

    elif [ "x$1" = "xbuildproj" ]; then
        dflags=`debug_flags "$2"`   # cl.exe flags: /MDd, /MD, /Zi, ...
        mtargets=`targets "$2"`     # all, clean, install, etc.
        $DO_MAKE $PROJ4_DIR Makefile.vc "OPTFLAGS= $dflags" $mtargets
        shift

    elif [ "x$1" = "xbuildgeographiclib" ]; then
        dflags=`debug_flags "$2"`   # cl.exe flags: /MDd, /MD, /Zi, ...
        mtargets=`targets "$2"`     # all, clean, install, etc.
        $DO_MAKE $GEOGRAPHICLIB_DIR Makefile.vc "OPTFLAGS= $dflags" $mtargets
        shift

    elif [ "x$1" = "xbuildtiff" ]; then
        def_c_flags="-EHsc -W3 -D_CRT_SECURE_NO_DEPRECATE -D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE -DZFILLODER_LSB2MSB"
        dflags=`debug_flags "$2"`   # cl.exe flags: /MDd, /MD, /Zi, ...
        mtargets=`targets "$2"`     # all, clean, install, etc.
        $DO_MAKE $TIF_DIR Makefile.vc "OPTFLAGS= $def_c_flags $dflags" $mtargets
        shift

    elif [ "x$1" = "xbuildgeotiff" ]; then
        # Force rebuilding geo_config.h
        touch "$GEOTIF_DIR"/geo_config.h.vc
        $DO_MAKE $GEOTIF_DIR Makefile.vc geo_config.h

        dflags=`debug_flags "$2"`   # cl.exe flags: /MDd, /MD, /Zi, ...
        mtargets=`targets "$2"`     # all, clean, install, etc.
        $DO_MAKE $GEOTIF_DIR Makefile.vc WANT_PROJ4=1 "OPTFLAGS= -nologo $dflags" $mtargets
        shift

    elif [ "x$1" = "xbuild" ]; then
        "$0" buildjpeg "$2" buildproj "$2" buildtiff "$2" buildgeotiff "$2" buildgeographiclib "$2"
        shift

    elif [ "x$1" = "xcopydll" ]; then
        for target in "$COPY_TARGET1" "$COPY_TARGET2" "$COPY_TARGET3" \
                      "$COPY_TARGET4" "$COPY_TARGET5"
        do
            if [ ! -e "$target" ]; then mkdir -p "$target"; fi
            cp "$JPEG_DIR/libjpeg.dll" "$target"
            cp "$PROJ4_DIR/src/proj.dll" "$target"
            cp "$TIF_DIR/libtiff/libtiff.dll" "$target"
            cp "$GEOTIF_DIR/geotiff.dll" "$target"
            cp -r "$GEOTIF_DIR/csv" "$target"
        done

    elif [ "x$1" = "xdelete" ]; then
        rm -rf "$JPEG_DIR"
        rm -rf "$PROJ4_DIR"
        rm -rf "$TIF_DIR"
        rm -rf "$GEOTIF_DIR"
        rm -rf "$GEOGRAPHICLIB_DIR"
        rm `basename "$JPEG_URL"`
        rm `basename "$PROJ4_URL"`
        rm `basename "$TIF_URL"`
        rm `basename "$GEOTIF_URL"`
        rm `basename "$GEOGRAPHICLIB_URL"`

    else
        usage "Unknown command: %1"
    fi

    shift
done
