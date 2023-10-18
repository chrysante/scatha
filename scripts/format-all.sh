SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."
 
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/lib"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/include"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/test"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/scathac"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/playground"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/svm"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/svm-test"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/passtool"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/include/svm"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/runtime"
