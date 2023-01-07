#include "Builtins.h"

#include <iostream>

#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace vm;

utl::vector<ExternalFunction> vm::makeBuiltinTable() {
    return {
        /* puti64: */ [](u64* regPtr, VirtualMachine* vm) {
            i64 const value = static_cast<i64>(*regPtr);
            std::cout << value << "\n";
        },
    };
}
