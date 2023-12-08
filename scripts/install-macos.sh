#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

$SCRIPT_DIR/build-dependencies.sh

compile() {
    xcodebuild -project "$PROJ_DIR/$1" -configuration Release -quiet
}

copy() {
    cp "$PROJ_DIR/build/bin/Release/$1" "/usr/local/bin"
}

compile scathac/scathac.xcodeproj
copy scathac
copy libscatha.dylib

compile svm/svm.xcodeproj
copy svm

compile scathadb/scathadb.xcodeproj
copy scathadb

compile sctool/sctool.xcodeproj
copy sctool
