mkdir "gen"

# For now. Adding arguments for file to process would be handy.
./../build/bin/Debug/playground --emit-selection-dag

# Warning: This script depends on the playground executable to emit the
# "gen/callgraph.gv" file

dot -Tsvg "gen/selection-dag.gv" -o "gen/selection-dag.svg"
open "gen/selection-dag.svg"
