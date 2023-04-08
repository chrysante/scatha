mkdir "gen"

# For now. Adding arguments for file to process would be handy.
./../build/bin/Debug/playground --emit-cfg

generate_graph() {
    dot -Tsvg "$1.gv" -o "$1.svg"
}

# Warning: This script depends on the playground executable to emit
# "gen/cfg-{@}.gv" files

for ((i=1;i<=$#;i++));
do
    generate_graph "gen/cfg-${!i}"
    open "gen/cfg-${!i}.svg"
done;

