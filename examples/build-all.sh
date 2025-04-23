SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

cd $SCRIPT_DIR

scathac -O1 -o build/binary-tree binary-tree.sc
scathac -O1 -o build/draw draw.sc
scathac -O1 -o build/hashtable -T staticlib hashtable.sc
scathac -O1 -o build/hashtable-test -L build/ -- hashtable-test.sc
scathac -O1 -o build/hello-world hello-world.sc
scathac -O1 -o build/mandelbrotset mandelbrotset.sc
scathac -O1 -o build/matrix matrix.sc
scathac -O1 -o build/memory-errors memory-errors.sc
scathac -O1 -o build/misc misc.sc
scathac -O1 -o build/multifile multifile-1.sc multifile-2.sc
scathac -O1 -o build/primesieve primesieve.sc
scathac -O1 -o build/print-args print-args.sc
scathac -O1 -o build/vector vector.sc
