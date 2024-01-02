#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

source $SCRIPT_DIR/util.sh

copy_to_system() {
    if [ "$OS" = "linux" ]; then
        mkdir -p ~/bin
        cp "$PROJ_DIR/build/bin/Release/$1" ~/bin
    elif [ "$OS" = "windows" ]; then
        error "Not implemented"
    elif [ "$OS" = "mac" ]; then
        cp "$PROJ_DIR/build/bin/Release/$1" "/usr/local/bin"
    else
        error "Unknown OS \"$OS\""
    fi
}

$SCRIPT_DIR/build-dependencies.sh

cd $PROJ_DIR
build_project scathac/ scathac Release
copy_to_system scathac
copy_to_system libscatha.so
copy_to_system libscatha.dylib

cd $PROJ_DIR
build_project svm/ svm Release
copy_to_system svm

cd $PROJ_DIR
build_project scathadb/ scathadb Release
copy_to_system scathadb
