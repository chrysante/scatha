#include "svm/BuiltinInternal.h"

#include <cassert>
#include <charconv>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <string_view>

#include <svm/Common.h>
#include <svm/ExternalFunction.h>
#include <svm/VirtualMachine.h>
#include <utl/utility.hpp>

#include "svm/Memory.h"
#include "svm/VMImpl.h"

using namespace svm;

/// We can change this to proper error reporting later
#define ENSURE(...) assert(__VA_ARGS__)

static void* deref(VirtualMachine* vm, VirtualPointer ptr, size_t size) {
    return vm->impl->memory.dereference(ptr, size);
}

template <typename T>
static T* deref(VirtualMachine* vm, VirtualPointer ptr, size_t size, int = 0) {
    return reinterpret_cast<T*>(deref(vm, ptr, size));
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
        *vm->impl->ostream << value;
    };
}

/// Loads two consecutive registers as an array pointer structure
/// `{ T*, size_t }`
template <typename T>
static std::span<T> loadArray(u64* regPtr) {
    T* data = load<char*>(regPtr);
    size_t size = load<u64>(regPtr + 1);
    return std::span<T>(data, size);
}

std::vector<ExternalFunction> svm::makeBuiltinTable() {
    std::vector<ExternalFunction> result(static_cast<size_t>(Builtin::_count));
    size_t k = 0;
    auto at = [&](Builtin index) -> auto& {
        size_t const i = static_cast<size_t>(index);
        assert(i == k++ && "Missing builtin function.");
        return result[i];
    };

    /// ## Common math functions

#define MATH_STD_IMPL(name) [](auto... args) { return std::name(args...); }
    at(Builtin::abs_f64) = math<double, 1>(MATH_STD_IMPL(abs));
    at(Builtin::exp_f64) = math<double, 1>(MATH_STD_IMPL(exp));
    at(Builtin::exp2_f64) = math<double, 1>(MATH_STD_IMPL(exp2));
    at(Builtin::exp10_f64) = math<double, 1>(
        []<typename T>(T const& arg) { return std::pow(arg, T(10)); });
    at(Builtin::log_f64) = math<double, 1>(MATH_STD_IMPL(log));
    at(Builtin::log2_f64) = math<double, 1>(MATH_STD_IMPL(log2));
    at(Builtin::log10_f64) = math<double, 1>(MATH_STD_IMPL(log10));
    at(Builtin::pow_f64) = math<double, 2>(MATH_STD_IMPL(pow));
    at(Builtin::sqrt_f64) = math<double, 1>(MATH_STD_IMPL(sqrt));
    at(Builtin::cbrt_f64) = math<double, 1>(MATH_STD_IMPL(cbrt));
    at(Builtin::hypot_f64) = math<double, 2>(MATH_STD_IMPL(hypot));
    at(Builtin::sin_f64) = math<double, 1>(MATH_STD_IMPL(sin));
    at(Builtin::cos_f64) = math<double, 1>(MATH_STD_IMPL(cos));
    at(Builtin::tan_f64) = math<double, 1>(MATH_STD_IMPL(tan));
    at(Builtin::asin_f64) = math<double, 1>(MATH_STD_IMPL(asin));
    at(Builtin::acos_f64) = math<double, 1>(MATH_STD_IMPL(acos));
    at(Builtin::atan_f64) = math<double, 1>(MATH_STD_IMPL(atan));

    /// ## Memory
    at(Builtin::memcpy) = [](u64* regPtr, VirtualMachine* vm, void*) {
        auto dest = load<VirtualPointer>(regPtr);
        auto size = load<size_t>(regPtr + 1);
        auto source = load<VirtualPointer>(regPtr + 2);
        std::memcpy(deref(vm, dest, size), deref(vm, source, size), size);
    };
    at(Builtin::memset) = [](u64* regPtr, VirtualMachine* vm, void*) {
        auto dest = load<VirtualPointer>(regPtr);
        auto size = load<size_t>(regPtr + 1);
        auto value = load<int64_t>(regPtr + 2);
        std::memset(deref(vm, dest, size), static_cast<int>(value), size);
    };
    /// ## Allocation
    at(Builtin::alloc) = [](u64* regPtr, VirtualMachine* vm, void*) {
        i64 size = load<i64>(regPtr);
        i64 align = load<i64>(regPtr + 1);
        ENSURE(size >= 0);
        ENSURE(align >= 0);
        VirtualPointer addr =
            vm->impl->memory.allocate(static_cast<size_t>(size),
                                      static_cast<size_t>(align));
        store(regPtr, addr);
        store(regPtr + 1, size);
    };
    at(Builtin::dealloc) = [](u64* regPtr, VirtualMachine* vm, void*) {
        auto addr = load<VirtualPointer>(regPtr);
        auto size = load<i64>(regPtr + 1);
        auto align = load<i64>(regPtr + 2);
        ENSURE(size >= 0);
        ENSURE(align >= 0);
        vm->impl->memory.deallocate(addr,
                                    static_cast<size_t>(size),
                                    static_cast<size_t>(align));
    };

    /// ## Console output
    at(Builtin::putchar) = printVal<char>();
    at(Builtin::puti64) = printVal<i64>();
    at(Builtin::putf64) = printVal<f64>();
    at(Builtin::putstr) = [](u64* regPtr, VirtualMachine* vm, void*) {
        auto data = load<VirtualPointer>(regPtr);
        size_t size = load<size_t>(regPtr + 1);
        *vm->impl->ostream << std::string_view(deref<char>(vm, data, size),
                                               size);
    };
    at(Builtin::putptr) = printVal<void*>();

    /// ## Console input
    at(Builtin::readline) = [](u64* regPtr, VirtualMachine* vm, void*) {
        std::string line;
        std::getline(*vm->impl->istream, line);
        auto buffer = vm->impl->memory.allocate(line.size(), 8);
        std::memcpy(deref(vm, buffer, line.size()), line.data(), line.size());
        store(regPtr, buffer);
        store(regPtr + 1, line.size());
    };

    /// ## String conversion
    at(Builtin::strtos64) = [](u64* regPtr, VirtualMachine* vm, void*) {
        auto dest = load<VirtualPointer>(regPtr);
        auto data = load<VirtualPointer>(regPtr + 1);
        auto size = load<size_t>(regPtr + 2);
        auto base = load<int>(regPtr + 3);
        i64 value = 0;
        auto* text = deref<char>(vm, data, size);
        auto result = std::from_chars(text, text + size, value, base);
        if (result.ec == std::errc{}) {
            store(regPtr, u64{ 1 });
            store(deref(vm, dest, sizeof(value)), value);
        }
        else {
            store(regPtr, u64{ 0 });
        }
    };

    at(Builtin::strtof64) = [](u64* regPtr, VirtualMachine* vm, void*) {
        auto dest = load<VirtualPointer>(regPtr);
        auto data = load<VirtualPointer>(regPtr + 1);
        auto size = load<size_t>(regPtr + 2);
        std::string_view text(deref<char>(vm, data, size), size);
        /// We copy the text into a `std::string` to make it null-terminated :(
        auto strText = std::string(text.begin(), text.end());
        char* strEnd = nullptr;
        double value = std::strtod(strText.data(), &strEnd);
        if (strEnd != strText.data()) {
            store(regPtr, u64{ 1 });
            store(deref(vm, dest, sizeof(value)), value);
        }
        else {
            store(regPtr, u64{ 0 });
        }
    };

    /// ##
    at(Builtin::trap) = [](u64*, VirtualMachine*, void*) {
#if defined(__GNUC__)
        __builtin_trap();
#elif defined(_MSC_VER)
        assert(false);
#else
#error Unsupported compiler
#endif
    };

    /// ## RNG
    at(Builtin::rand_i64) = [](u64* regPtr, VirtualMachine*, void*) {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        u64 randomValue = rng();
        store(regPtr, randomValue);
    };

    assert(static_cast<size_t>(Builtin::_count) == k &&
           "Missing builtin functions.");
    return result;
}
