mkdir "gen"

# For now. Adding arguments for file to process would be handy.
./../build/bin/Debug/playground --emit-interference-graph

# Warning: This script depends on the playground executable to emit the
# "gen/interference-graph.gv" file

dot -Tsvg "gen/interference-graph.gv" -o "gen/interference-graph.svg"
open "gen/interference-graph.svg"
