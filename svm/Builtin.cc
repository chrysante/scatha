#include "svm/BuiltinInternal.h"

#include <cmath>
#include <iostream>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>

#include "svm/Memory.h"

using namespace svm;

template <typename From, typename To>
static ExternalFunction::FuncPtr cast() {
    return [](u64* regPtr, VirtualMachine* vm, void*) {
        From const value = load<From>(regPtr);
        store(regPtr, static_cast<To>(value));
    };
}

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
    /// ## Conversion functions
    at(Builtin::f64toi64) = cast<double, int64_t>();
    at(Builtin::i64tof64) = cast<int64_t, double>();

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

    /// ## Console output
    at(Builtin::putchar) = printVal<char>();
    at(Builtin::puti64)  = printVal<i64>();
    at(Builtin::putf64)  = printVal<f64>();

    assert(static_cast<size_t>(Builtin::_count) == k &&
           "Missing builtin functions.");
    return result;
}
