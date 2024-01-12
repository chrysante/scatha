#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

rm -r $PROJ_DIR/stdlib/build

cd $PROJ_DIR/scripts
./compile-stdlib.sh

cd $PROJ_DIR/stdlib

scathac -o -T staticlib -O build/testframework testframework/testframework.sc

for filename in test/*.sc; do
    name=${filename##*/}
    test_exec="build/${name%.*}"
    scathac -o -L build -O $test_exec $filename
    ./$test_exec
done
