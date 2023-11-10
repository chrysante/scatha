#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

$SCRIPT_DIR/install-develop.sh

xcodebuild -project "$PROJ_DIR/scathac.xcodeproj" -configuration Release -quiet
xcodebuild -project "$PROJ_DIR/svm.xcodeproj" -configuration Release -quiet
xcodebuild -project "$PROJ_DIR/scathadb.xcodeproj" -configuration Release -quiet

cp "$PROJ_DIR/build/bin/Release/scathac" "/usr/local/bin"
cp "$PROJ_DIR/build/bin/Release/libscatha.dylib" "/usr/local/bin"
cp "$PROJ_DIR/build/bin/Release/svm" "/usr/local/bin"
cp "$PROJ_DIR/build/bin/Release/scathadb" "/usr/local/bin"
