#!/bin/sh

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

source $SCRIPT_DIR/compile.sh

compile runtime/runtime.xcodeproj Release

clang++ -I$PROJ_DIR/runtime/include -L$PROJ_DIR/build/bin/Release -lruntime  -std=c++20 -shared -o $PROJ_DIR/build/bin/testlib.dylib  $PROJ_DIR/examples/testlib/testlib.cpp
