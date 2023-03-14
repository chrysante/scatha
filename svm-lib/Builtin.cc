#include <svm/Builtin.h>

#include <cmath>
#include <iostream>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>

#include "Memory.h"

using namespace svm;

template <typename T>
static auto printVal() {
    return [](u64* regPtr, VirtualMachine* vm) {
        T const value = read<T>(regPtr);
        std::cout << value;
    };
}

utl::vector<ExternalFunction> svm::makeBuiltinTable() {
    utl::vector<ExternalFunction> result(static_cast<size_t>(Builtin::_count));
    size_t k = 0;
    auto at  = [&](Builtin index) -> auto& {
        size_t const i = static_cast<size_t>(index);
        assert(i == k++ && "Missing builtin function.");
        return result[i];
    };
    at(Builtin::putchar) = printVal<char>();
    at(Builtin::puti64)  = printVal<i64>();
    at(Builtin::putf64)  = printVal<f64>();
    at(Builtin::sqrtf64) = [](u64* regPtr, VirtualMachine* vm) {
        f64 const arg    = read<f64>(regPtr);
        f64 const result = std::sqrt(arg);
        store(regPtr, result);
    };
    assert(static_cast<size_t>(Builtin::_count) == k &&
           "Missing builtin functions.");
    return result;
}
