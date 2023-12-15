#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

compile() {
    xcodebuild -project "$PROJ_DIR/$1" -configuration $2 -quiet
}
