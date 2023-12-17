/// This file defines functions used by a test case in scatha-test

#include <iostream>
#include <stdint.h>

#include "Bridging.h"

static int64_t foo(int64_t a, int64_t b) {
    return a + b;
}

SC_EXPORT(foo, foo)

static void bar(int64_t a, int64_t b) {
    std::cout << "bar(" << a << ", " << b << ")\n";
}

SC_EXPORT(bar, bar)

static int64_t baz() {
    return 42;
}

SC_EXPORT(baz, baz)

static void quux() {
    std::cout << "quux\n";
}

SC_EXPORT(quux, quux)
