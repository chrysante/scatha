/// This file defines functions used by a test case in scatha-test

#include <iostream>
#include <stdint.h>
#include <string_view>

#ifdef _MSC_VER
#define EXPORT __declspec(dllexport) 
#else
#define EXPORT 
#endif

extern "C" {

EXPORT int64_t foo(int64_t a, int64_t b) {
    return a + b;
}

EXPORT void bar(int64_t a, int64_t b) {
    std::cout << "bar(" << a << ", " << b << ")\n";
}

EXPORT int64_t baz() { return 42; }

EXPORT void quux() { std::cout << "quux\n"; }

EXPORT bool isNull(void* p) { return p == nullptr; }

namespace {

struct MyStruct {
    int value;
};

} // namespace

EXPORT MyStruct* MyStruct_make(int value) {
    return new MyStruct{ value };
}

EXPORT void MyStruct_free(MyStruct* ptr) { delete ptr; }

EXPORT int MyStruct_value(MyStruct* ptr) { return ptr->value; }

EXPORT void printString(std::string_view text) {
    std::cout << text << " : Size = " << text.size();
}

EXPORT MyStruct MyStruct_passByValue(MyStruct s) {
    s.value++;
    return s;
}

}