#!/bin/bash
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

COMPILER="scathac"
DEST="$PROJ_DIR/stdlib/build"
SOURCE="$PROJ_DIR/stdlib/src"

while [[ $# -gt 0 ]]; do
  case $1 in
    -C|--compiler)
      COMPILER="$2"
      shift 2
      ;;
    -D|--dest)
      DEST="$2"
      shift 2
      ;;
    *)
      shift
      ;;
  esac
done

$COMPILER -O1 -T staticlib -o "$DEST/std" \
    $SOURCE/Math.sc \
    $SOURCE/Print.sc \
    $SOURCE/String.sc
