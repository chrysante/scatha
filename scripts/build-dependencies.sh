#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

source $SCRIPT_DIR/util.sh

cd $PROJ_DIR
run_premake

mkdir -p build/bin/{Debug,Release}

# UTL
cd $PROJ_DIR/external/utility
run_premake
build_project . utility Release

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
