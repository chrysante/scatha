mkdir "gen"

# For now. Adding arguments for file to process would be handy.
./../build/bin/Debug/playground --emit-callgraph

# Warning: This script depends on the playground executable to emit the
# "gen/callgraph.gv" file

dot -Tsvg "gen/callgraph.gv" -o "gen/callgraph.svg"
open "gen/callgraph.svg"
