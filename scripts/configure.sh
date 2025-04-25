#!/bin/bash

set -e

SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR=$SCRIPT_DIR/..

source "$SCRIPT_DIR/_impl/platform.sh"

# Parse -G option and forward the rest
USER_GENERATOR=""
CMAKE_ARGS=()

while [[ $# -gt 0 ]]; do
    case "$1" in
        -G)
            shift
            if [[ $# -eq 0 ]]; then
                echo "error: -G requires an argument" >&2
                exit 1
            fi
            USER_GENERATOR="$1"
            ;;
        *)
            CMAKE_ARGS+=("$1")
            ;;
    esac
    shift
done

# Default generator if none was provided
if [[ -z "$USER_GENERATOR" ]]; then
    if [[ "$OS" == "linux" ]]; then
        USER_GENERATOR=""
    elif [[ "$OS" == "windows" ]]; then
        USER_GENERATOR=""
    elif [[ "$OS" == "mac" ]]; then
        USER_GENERATOR="Xcode"
    else
        echo "error: Unknown OS \"$OS\"" >&2
        exit 1
    fi
fi

# Assemble final -G option only if needed
if [[ -n "$USER_GENERATOR" ]]; then
    GENERATOR_OPTION=(-G "$USER_GENERATOR")
else
    GENERATOR_OPTION=()
fi

cmake -S "$PROJ_DIR" -B "$PROJ_DIR/build" "${GENERATOR_OPTION[@]}" -DCPM_SOURCE_CACHE=build/srccache "${CMAKE_ARGS[@]}"
