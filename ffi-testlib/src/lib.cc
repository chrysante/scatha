/// This file defines functions used by a test case in scatha-test

#include <iostream>
#include <stdint.h>

extern "C" int64_t foo(int64_t a, int64_t b) {
    return a + b;
}

extern "C" void bar(int64_t a, int64_t b) {
    std::cout << "bar(" << a << ", " << b << ")\n";
}

extern "C" int64_t baz() {
    return 42;
}

extern "C" void quux() {
    std::cout << "quux\n";
}
