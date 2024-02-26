SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR=$SCRIPT_DIR/..

source $SCRIPT_DIR/_impl/platform.sh

if [ "$OS" = "linux" ]; then
    GENERATOR=""
elif [ "$OS" = "windows" ]; then
    GENERATOR=""
elif [ "$OS" = "mac" ]; then
    GENERATOR=-GXcode
else
    error "Unknown OS \"$OS\""
fi

cmake -S $PROJ_DIR -B $PROJ_DIR/build $GENERATOR -DCPM_SOURCE_CACHE=build/srccache
