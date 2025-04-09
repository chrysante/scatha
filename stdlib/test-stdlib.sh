#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

rm -r $PROJ_DIR/stdlib/build 2> /dev/null

cd $PROJ_DIR/stdlib
./compile-stdlib.sh

cd $PROJ_DIR/stdlib

scathac -O1 -T staticlib -o build/testframework testframework/testframework.sc

for filename in test/*.sc; do
    name=${filename##*/}
    test_exec="build/${name%.*}"
    scathac -O --stdlib build -o $test_exec $filename
    ./$test_exec
done
