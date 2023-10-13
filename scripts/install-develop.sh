#!/bin/bash

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

# Build yaml-cpp
cd "$PROJ_DIR/external/yaml-cpp"
mkdir "build"
cd "build"
cmake -DYAML_BUILD_SHARED_LIBS=ON ..
make -j config=release
cd "$PROJ_DIR"
mkdir -p build/bin/{Debug,Release}
cp external/yaml-cpp/build/libyaml-cpp* build/bin/Debug
cp external/yaml-cpp/build/libyaml-cpp* build/bin/Release
