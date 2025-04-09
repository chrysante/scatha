#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

format_dir() {
    $PROJ_DIR/scripts/_impl/format.sh $PROJ_DIR/$1
}

format_dir include/
format_dir src/
format_dir test/
format_dir fuzz/
