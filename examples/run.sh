./build/bin/Debug/scatha-c -t -f "examples/$1.sc" -o 0 --objdir "examples/$1.sbin"
./build/bin/Debug/scatha-c -t -f "examples/$1.sc" -o 1 --objdir "examples/$1-o1.sbin"
./build/bin/Debug/svm -t -f "examples/$1.sbin"
./build/bin/Debug/svm -t -f "examples/$1-o1.sbin"
./build/bin/Release/svm -t -f "examples/$1.sbin"
./build/bin/Release/svm -t -f "examples/$1-o1.sbin"