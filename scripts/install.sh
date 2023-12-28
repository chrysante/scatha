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

build_project scathac Release
copy_to_system scathac
copy_to_system libscatha.{so,dylib}

build_project svm Release
copy_to_system svm

build_project scathadb Release
copy_to_system scathadb
