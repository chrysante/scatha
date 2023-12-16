#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

source $SCRIPT_DIR/compile.sh

#compile() {
#    xcodebuild -project "$PROJ_DIR/$1" -configuration Release -quiet
#}

copy() {
    cp "$PROJ_DIR/build/bin/Release/$1" "/usr/local/bin"
}

$SCRIPT_DIR/build-dependencies.sh

compile scathac/scathac.xcodeproj Release
copy scathac
copy libscatha.dylib

compile svm/svm.xcodeproj Release
copy svm

compile scathadb/scathadb.xcodeproj Release
copy scathadb
