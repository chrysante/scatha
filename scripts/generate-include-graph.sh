#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

$PROJ_DIR/scripts/cinclude2dot.pl \
    --merge=file \
    --include=$PROJ_DIR/include,$PROJ_DIR/include/scatha,$PROJ_DIR/lib \
    --src=$PROJ_DIR/lib \
    > $PROJ_DIR/debug/includegraph.gv

dot -Kneato  -s[1000] -Tsvg $PROJ_DIR/debug/includegraph.gv -o $PROJ_DIR/debug/includegraph.svg

open $PROJ_DIR/debug/includegraph.svg
