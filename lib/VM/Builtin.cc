#include "VM/Builtin.h"

#include <iostream>
#include <cmath>

#include "Basic/Memory.h"
#include "VM/VirtualMachine.h"

using namespace scatha;
using namespace vm;

template <typename T>
static auto printVal() {
    return [](u64* regPtr, VirtualMachine* vm) {
        T const value = read<T>(regPtr);
        std::cout << value;
    };
}

utl::vector<ExternalFunction> vm::makeBuiltinTable() {
    utl::vector<ExternalFunction> result(static_cast<size_t>(Builtin::_count));
    size_t k = 0;
    auto at  = [&](Builtin index) -> auto& {
        size_t const i = static_cast<size_t>(index);
        SC_ASSERT(i == k++, "Missing builtin function.");
        return result[i];
    };
    at(Builtin::putchar) = printVal<char>();
    at(Builtin::puti64)  = printVal<i64>();
    at(Builtin::putf64)  = printVal<f64>();
    at(Builtin::sqrtf64)  = [](u64* regPtr, VirtualMachine* vm) {
        f64 const arg = read<f64>(regPtr);
        f64 const result = std::sqrt(arg);
        store(regPtr, result);
    };
    SC_ASSERT(static_cast<size_t>(Builtin::_count) == k, "Missing builtin functions.");
    return result;
}
