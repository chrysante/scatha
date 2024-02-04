/// This file defines functions used by a test case in scatha-test

#include <stdint.h>
#include <iostream>
#include <string_view>


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

extern "C" bool isNull(void* p) {
    return p == nullptr;
}

namespace {

struct MyStruct {
    int value;
};

} // namespace

extern "C" MyStruct* MyStruct_make(int value) { return new MyStruct{ value }; }

extern "C" void MyStruct_free(MyStruct* ptr) { delete ptr; }

extern "C" int MyStruct_value(MyStruct* ptr) { return ptr->value; }

extern "C" void printString(std::string_view text) {
    std::cout << text << " : Size = " << text.size();
}
