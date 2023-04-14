./build/bin/Debug/scatha-c -t -f examples/mandelbrotset.sc -o 0 --objdir examples/mandelbrotset.sbin
./build/bin/Debug/scatha-c -t -f examples/mandelbrotset.sc -o 1 --objdir examples/mandelbrotset-o1.sbin
./build/bin/Debug/svm -t -f examples/mandelbrotset.sbin
./build/bin/Debug/svm -t -f examples/mandelbrotset-o1.sbin
./build/bin/Release/svm -t -f examples/mandelbrotset.sbin
./build/bin/Release/svm -t -f examples/mandelbrotset-o1.sbin
