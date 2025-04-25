SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."

generate_dir() {
    $SCRIPT_DIR/_impl/generate-include-guards.sh $PROJ_DIR/$1 $PROJ_DIR/$2
}

generate_dir src src
generate_dir include include
generate_dir test test
