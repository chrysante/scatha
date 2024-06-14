#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

format_dir() {
    $PROJ_DIR/scripts/_impl/format.sh $PROJ_DIR/$1
}

format_dir scatha/lib
format_dir scatha/include
format_dir scatha/test
format_dir scatha/fuzz-test
format_dir scathac/src
format_dir playground
format_dir svm/src
format_dir svm/lib
format_dir svm/include
format_dir svm/test
format_dir scathadb/src
format_dir runtime/include
format_dir runtime/src
format_dir runtime/test
format_dir runtime
