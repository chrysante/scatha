SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )
PROJ_DIR="$SCRIPT_DIR/.."
 
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/lib"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/include"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/test"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/scathac/src"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/playground"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/svm/src"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/svm/lib"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/svm/include"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/scathadb/src"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/sctool/src"
"$PROJ_DIR/scripts/format.sh" "$PROJ_DIR/runtime"
