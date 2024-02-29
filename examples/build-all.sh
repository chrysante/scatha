SCRIPT_DIR=$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )

cd $SCRIPT_DIR

scathac -o -O build/binary-tree binary-tree.sc
scathac -o -O build/draw draw.sc
scathac -o -O build/hashtable hashtable.sc
scathac -o -O build/hello-world hello-world.sc
scathac -o -O build/mandelbrotset mandelbrotset.sc
scathac -o -O build/matrix matrix.sc
scathac -o -O build/memory-errors memory-errors.sc
scathac -o -O build/misc misc.sc
scathac -o -O build/multifile multifile-1.sc multifile-2.sc
scathac -o -O build/primesieve primesieve.sc
scathac -o -O build/print-args print-args.sc
scathac -o -O build/vector vector.sc
