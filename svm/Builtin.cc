#include "svm/BuiltinInternal.h"

#include <cmath>
#include <iostream>
#include <string_view>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>
#include <utl/utility.hpp>

#include "svm/Memory.h"

using namespace svm;

template <typename T, size_t N>
using wrap = T;

template <typename Float, size_t NumArgs>
static ExternalFunction::FuncPtr math(auto impl) {
    return [](u64* regPtr, VirtualMachine* vm, void*) {
        [&]<size_t... I>(std::index_sequence<I...>) {
            std::tuple<wrap<Float, I>...> args{ load<Float>(regPtr + I)... };
            store(regPtr, std::apply(decltype(impl){}, args));
            }(std::make_index_sequence<NumArgs>{});
    };
}

template <typename T>
static ExternalFunction::FuncPtr printVal() {
    return [](u64* regPtr, VirtualMachine* vm, void*) {
        T const value = load<T>(regPtr);
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

    /// ## Common math functions

#define MATH_STD_IMPL(name) [](auto... args) { return std::name(args...); }
    at(Builtin::abs_f64)   = math<double, 1>(MATH_STD_IMPL(abs));
    at(Builtin::exp_f64)   = math<double, 1>(MATH_STD_IMPL(exp));
    at(Builtin::exp2_f64)  = math<double, 1>(MATH_STD_IMPL(exp2));
    at(Builtin::exp10_f64) = math<double, 1>(
        []<typename T>(T const& arg) { return std::pow(arg, T(10)); });
    at(Builtin::log_f64)   = math<double, 1>(MATH_STD_IMPL(log));
    at(Builtin::log2_f64)  = math<double, 1>(MATH_STD_IMPL(log2));
    at(Builtin::log10_f64) = math<double, 1>(MATH_STD_IMPL(log10));
    at(Builtin::pow_f64)   = math<double, 2>(MATH_STD_IMPL(pow));
    at(Builtin::sqrt_f64)  = math<double, 1>(MATH_STD_IMPL(sqrt));
    at(Builtin::cbrt_f64)  = math<double, 1>(MATH_STD_IMPL(cbrt));
    at(Builtin::hypot_f64) = math<double, 2>(MATH_STD_IMPL(hypot));
    at(Builtin::sin_f64)   = math<double, 1>(MATH_STD_IMPL(sin));
    at(Builtin::cos_f64)   = math<double, 1>(MATH_STD_IMPL(cos));
    at(Builtin::tan_f64)   = math<double, 1>(MATH_STD_IMPL(tan));
    at(Builtin::asin_f64)  = math<double, 1>(MATH_STD_IMPL(asin));
    at(Builtin::acos_f64)  = math<double, 1>(MATH_STD_IMPL(acos));
    at(Builtin::atan_f64)  = math<double, 1>(MATH_STD_IMPL(atan));

    /// ## Memory allocation
    at(Builtin::memcpy) = [](u64* regPtr, VirtualMachine* vm, void*) {
        void* const dest   = load<void*>(regPtr);
        void* const source = load<void*>(regPtr + 1);
        size_t const size  = load<size_t>(regPtr + 2);
        std::memcpy(dest, source, size);
    };
    at(Builtin::alloc) = [](u64* regPtr, VirtualMachine* vm, void*) {
        i64 const size                   = load<i64>(regPtr);
        [[maybe_unused]] i64 const align = load<i64>(regPtr + 1);
        auto* addr = std::malloc(utl::narrow_cast<u64>(size));
        store(regPtr, addr);
        store(regPtr + 1, size);
    };
    at(Builtin::dealloc) = [](u64* regPtr, VirtualMachine* vm, void*) {
        void* const addr                    = load<void*>(regPtr);
        [[maybe_unused]] size_t const align = load<size_t>(regPtr + 1);
        std::free(addr);
    };

    /// ## Console output
    at(Builtin::putchar) = printVal<char>();
    at(Builtin::puti64)  = printVal<i64>();
    at(Builtin::putf64)  = printVal<f64>();
    at(Builtin::putstr)  = [](u64* regPtr, VirtualMachine* vm, void*) {
        char* const data  = load<char*>(regPtr);
        size_t const size = load<size_t>(regPtr + 1);
        std::cout << std::string_view(data, size);
    };

    /// ##
    at(Builtin::trap) = [](u64*, VirtualMachine*, void*) {
#if defined(__GNUC__)
        __builtin_trap();
#else
#error Unsupported compiler
#endif
    };

    assert(static_cast<size_t>(Builtin::_count) == k &&
           "Missing builtin functions.");
    return result;
}
