#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

premake5 xcode4 --file=$PROJ_DIR/premake5.lua

xcodebuild -project "$PROJ_DIR/scathac.xcodeproj" -configuration Release
xcodebuild -project "$PROJ_DIR/svm.xcodeproj" -configuration Release

cp "$PROJ_DIR/build/bin/Release/scathac" "/usr/local/bin"
cp "$PROJ_DIR/build/bin/Release/libscatha.dylib" "/usr/local/bin"
cp "$PROJ_DIR/build/bin/Release/svm" "/usr/local/bin"
