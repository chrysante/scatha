#!/bin/sh

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

clang++ -std=c++11 -shared -o $PROJ_DIR/examples/testlib/testlib.dylib  $PROJ_DIR/examples/testlib/testlib.cpp
