#!/bin/sh
SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

SHARD_COUNT=64

# https://unix.stackexchange.com/a/216475/593255
# initialize a semaphore with a given number of tokens
open_sem() {
    mkfifo pipe-$$
    exec 3<>pipe-$$
    rm pipe-$$
    local i=$1
    for((;i>0;i--)); do
        printf %s 000 >&3
    done
}

# run the given command asynchronously and pop/push tokens
run_with_lock() {
    local x
    # this read waits until there is something to read
    read -u 3 -n 3 x && ((0==x)) || exit $x
    (
     ( "$@"; )
    # push the return code of the command to the semaphore
    printf '%.3d' $? >&3
    )&
}

N=12
open_sem $N
for (( i=0; i<SHARD_COUNT; i++ ))
do
    run_with_lock "$PROJ_DIR/build/bin/debug/scatha-test" "--shard-count $SHARD_COUNT" "--shard-index $i" --idempotency=yes --passes=yes
done

