#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

cd $PROJ_DIR
premake5 xcode4

mkdir -p build/bin/{Debug,Release}

# UTL
cd $PROJ_DIR/external/utility
premake5 xcode4
xcodebuild -project utility.xcodeproj -configuration Release -quiet
cd $PROJ_DIR
cp external/utility/build/bin/Release/libutility.a build/bin/Debug
cp external/utility/build/bin/Release/libutility.a build/bin/Release

# Catch
cd $PROJ_DIR
mkdir -p build/Catch
cmake -DCMAKE_CXX_STANDARD=17 -S external/Catch -B build/Catch
cd build/Catch
make -j

cp src/*.a ../bin/Debug
cp src/*.a ../bin/Release


# FTXUI
cd $PROJ_DIR
mkdir -p build/FTXUI
cmake -S external/FTXUI -B build/FTXUI
cd build/FTXUI
make -j

cp libftxui-*.a ../bin/Debug
cp libftxui-*.a ../bin/Release
