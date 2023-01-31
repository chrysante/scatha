# For now. Adding arguments for file to process would be handy.
./../build/bin/Debug/playground --emit-cfg

generate_graph() {
    dot -Tsvg "$1.gv" -o "$1.svg"
}

generate_graph "cfg"
generate_graph "cfg-m2r"
generate_graph "cfg-scc"
generate_graph "cfg-dce"
