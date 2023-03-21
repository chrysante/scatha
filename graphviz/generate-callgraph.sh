# For now. Adding arguments for file to process would be handy.
./../build/bin/Debug/playground --emit-callgraph

# Warning: This script depends on the playground executable to the
# "callgraph.gv" file

dot -Tsvg "callgraph.gv" -o "callgraph.svg"
open "callgraph.svg"
