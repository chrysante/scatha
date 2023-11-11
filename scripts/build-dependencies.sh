#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

cd $PROJ_DIR
premake5 xcode4

mkdir -p build/bin/{Debug,Release}

# FTXUI
mkdir -p build/FTXUI
cmake -S external/FTXUI -B build/FTXUI
cd build/FTXUI
make -j

cp libftxui-*.a ../bin/Debug
cp libftxui-*.a ../bin/Release
