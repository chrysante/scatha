#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

cd $PROJ_DIR/stdlib

scathac -o -T staticlib -O build/std  *.sc
