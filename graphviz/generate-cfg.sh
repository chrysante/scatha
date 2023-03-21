# For now. Adding arguments for file to process would be handy.
./../build/bin/Debug/playground --emit-cfg

generate_graph() {
    dot -Tsvg "$1.gv" -o "$1.svg"
}

# Warning: This script depends on the playground executable to emit
# "cfg-{@}.gv" files

for ((i=1;i<=$#;i++));
do
    generate_graph "cfg-${!i}"
    open "cfg-${!i}.svg"
done;

