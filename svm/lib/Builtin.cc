#include "BuiltinInternal.h"

#include <cassert>
#include <charconv>
#include <cmath>
#include <iostream>
#include <random>
#include <string>
#include <string_view>

#include <utl/strcat.hpp>
#include <utl/utility.hpp>

#include "Common.h"
#include "Errors.h"
#include "ExternalFunction.h"
#include "Memory.h"
#include "VMImpl.h"
#include "VirtualMachine.h"

using namespace svm;

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
static BuiltinFunction::FuncPtr math(auto impl) {
    return [](u64* regPtr, VirtualMachine*) {
        [&]<size_t... I>(std::index_sequence<I...>) {
            std::tuple<wrap<Float, I>...> args{ load<Float>(regPtr + I)... };
            store(regPtr, std::apply(decltype(impl){}, args));
        }(std::make_index_sequence<NumArgs>{});
    };
}

template <typename T>
static BuiltinFunction::FuncPtr printVal() {
    return [](u64* regPtr, VirtualMachine* vm) {
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

template <typename T>
static T fract(T arg) {
    T i;
    return std::copysign(std::modf(arg, &i), T(1.0));
}

std::vector<BuiltinFunction> svm::makeBuiltinTable() {
    std::vector<BuiltinFunction> result(static_cast<size_t>(Builtin::_count));
    [[maybe_unused]] size_t currentIndex = 0;
    auto at = [&](Builtin index) -> auto& {
        size_t const i = static_cast<size_t>(index);
        assert(i == currentIndex++ && "Missing builtin function.");
        return result[i];
    };
    auto set = [&](Builtin index, auto fn) {
        at(index) = { utl::strcat("__builtin_", toString(index)), fn };
    };
    /// ## Common math functions

#define MATH_IMPL(name)                                                        \
    [](auto... args) {                                                         \
        using namespace std; /* sic! */                                        \
        return name(args...);                                                  \
    }
    set(Builtin::abs_f64, math<double, 1>(MATH_IMPL(abs)));
    set(Builtin::exp_f64, math<double, 1>(MATH_IMPL(exp)));
    set(Builtin::exp2_f64, math<double, 1>(MATH_IMPL(exp2)));
    set(Builtin::exp10_f64,
        math<double, 1>([](double arg) { return std::pow(arg, 10.0); }));
    set(Builtin::log_f64, math<double, 1>(MATH_IMPL(log)));
    set(Builtin::log2_f64, math<double, 1>(MATH_IMPL(log2)));
    set(Builtin::log10_f64, math<double, 1>(MATH_IMPL(log10)));
    set(Builtin::pow_f64, math<double, 2>(MATH_IMPL(pow)));
    set(Builtin::sqrt_f64, math<double, 1>(MATH_IMPL(sqrt)));
    set(Builtin::cbrt_f64, math<double, 1>(MATH_IMPL(cbrt)));
    set(Builtin::hypot_f64, math<double, 2>(MATH_IMPL(hypot)));
    set(Builtin::sin_f64, math<double, 1>(MATH_IMPL(sin)));
    set(Builtin::cos_f64, math<double, 1>(MATH_IMPL(cos)));
    set(Builtin::tan_f64, math<double, 1>(MATH_IMPL(tan)));
    set(Builtin::asin_f64, math<double, 1>(MATH_IMPL(asin)));
    set(Builtin::acos_f64, math<double, 1>(MATH_IMPL(acos)));
    set(Builtin::atan_f64, math<double, 1>(MATH_IMPL(atan)));
    set(Builtin::fract_f64, math<double, 1>(MATH_IMPL(fract)));
    set(Builtin::floor_f64, math<double, 1>(MATH_IMPL(floor)));
    set(Builtin::ceil_f64, math<double, 1>(MATH_IMPL(ceil)));

    set(Builtin::abs_f32, math<float, 1>(MATH_IMPL(abs)));
    set(Builtin::exp_f32, math<float, 1>(MATH_IMPL(exp)));
    set(Builtin::exp2_f32, math<float, 1>(MATH_IMPL(exp2)));
    set(Builtin::exp10_f32,
        math<float, 1>([](float arg) { return std::pow(arg, 10.0f); }));
    set(Builtin::log_f32, math<float, 1>(MATH_IMPL(log)));
    set(Builtin::log2_f32, math<float, 1>(MATH_IMPL(log2)));
    set(Builtin::log10_f32, math<float, 1>(MATH_IMPL(log10)));
    set(Builtin::pow_f32, math<float, 2>(MATH_IMPL(pow)));
    set(Builtin::sqrt_f32, math<float, 1>(MATH_IMPL(sqrt)));
    set(Builtin::cbrt_f32, math<float, 1>(MATH_IMPL(cbrt)));
    set(Builtin::hypot_f32, math<float, 2>(MATH_IMPL(hypot)));
    set(Builtin::sin_f32, math<float, 1>(MATH_IMPL(sin)));
    set(Builtin::cos_f32, math<float, 1>(MATH_IMPL(cos)));
    set(Builtin::tan_f32, math<float, 1>(MATH_IMPL(tan)));
    set(Builtin::asin_f32, math<float, 1>(MATH_IMPL(asin)));
    set(Builtin::acos_f32, math<float, 1>(MATH_IMPL(acos)));
    set(Builtin::atan_f32, math<float, 1>(MATH_IMPL(atan)));
    set(Builtin::fract_f32, math<float, 1>(MATH_IMPL(fract)));
    set(Builtin::floor_f32, math<float, 1>(MATH_IMPL(floor)));
    set(Builtin::ceil_f32, math<float, 1>(MATH_IMPL(ceil)));

    /// ## Memory
    set(Builtin::memcpy, [](u64* regPtr, VirtualMachine* vm) {
        auto dest = load<VirtualPointer>(regPtr);
        auto size = load<size_t>(regPtr + 1);
        auto source = load<VirtualPointer>(regPtr + 2);
        std::memcpy(deref(vm, dest, size), deref(vm, source, size), size);
    });
    set(Builtin::memmove, [](u64* regPtr, VirtualMachine* vm) {
        auto dest = load<VirtualPointer>(regPtr);
        auto size = load<size_t>(regPtr + 1);
        auto source = load<VirtualPointer>(regPtr + 2);
        std::memmove(deref(vm, dest, size), deref(vm, source, size), size);
    });
    set(Builtin::memset, [](u64* regPtr, VirtualMachine* vm) {
        auto dest = load<VirtualPointer>(regPtr);
        auto size = load<size_t>(regPtr + 1);
        auto value = load<int64_t>(regPtr + 2);
        std::memset(deref(vm, dest, size), static_cast<int>(value), size);
    });
    /// ## Allocation
    set(Builtin::alloc, [](u64* regPtr, VirtualMachine* vm) {
        u64 size = load<u64>(regPtr);
        u64 align = load<u64>(regPtr + 1);
        VirtualPointer addr = vm->impl->memory.allocate(size, align);
        store(regPtr, addr);
        store(regPtr + 1, size);
    });
    set(Builtin::dealloc, [](u64* regPtr, VirtualMachine* vm) {
        auto addr = load<VirtualPointer>(regPtr);
        auto size = load<i64>(regPtr + 1);
        auto align = load<i64>(regPtr + 2);
        vm->impl->memory.deallocate(addr, static_cast<size_t>(size),
                                    static_cast<size_t>(align));
    });

    /// ## Console output
    set(Builtin::putchar, printVal<char>());
    set(Builtin::puti64, printVal<i64>());
    set(Builtin::putf64, printVal<f64>());
    set(Builtin::putstr, [](u64* regPtr, VirtualMachine* vm) {
        auto data = load<VirtualPointer>(regPtr);
        size_t size = load<size_t>(regPtr + 1);
        *vm->impl->ostream << std::string_view(deref<char>(vm, data, size),
                                               size);
    });
    set(Builtin::putln, [](u64* regPtr, VirtualMachine* vm) {
        auto data = load<VirtualPointer>(regPtr);
        size_t size = load<size_t>(regPtr + 1);
        *vm->impl->ostream << std::string_view(deref<char>(vm, data, size),
                                               size)
                           << "\n";
    });
    set(Builtin::putptr, printVal<void*>());

    /// ## Console input
    set(Builtin::readline, [](u64* regPtr, VirtualMachine* vm) {
        std::string line;
        std::getline(*vm->impl->istream, line);
        auto buffer = vm->impl->memory.allocate(line.size(), 8);
        std::memcpy(deref(vm, buffer, line.size()), line.data(), line.size());
        store(regPtr, buffer);
        store(regPtr + 1, line.size());
    });

    /// ## String conversion
    set(Builtin::strtos64, [](u64* regPtr, VirtualMachine* vm) {
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
    });

    set(Builtin::strtof64, [](u64* regPtr, VirtualMachine* vm) {
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
    });

    /// # FString runtime support
    static constexpr auto fstringRealloc = [](VirtualMachine* vm,
                                              VirtualPointer& buffer, u64& size,
                                              u64 offset, u64 newSize) {
        VirtualPointer newBuffer = vm->allocateMemory(newSize, 1);
        auto* native = vm->derefPointer(newBuffer, newSize);
        std::memcpy(native, vm->derefPointer(buffer, offset), offset);
        buffer = newBuffer;
        size = newSize;
        return native;
    };
    static constexpr auto fstringWriteImpl = [](u64* regPtr, VirtualMachine* vm,
                                                std::string_view arg) {
        VirtualPointer buffer = load<VirtualPointer>(regPtr);
        u64 size = load<u64>(regPtr + 1);
        VirtualPointer offsetPtr = load<VirtualPointer>(regPtr + 2);
        u64* offset = &vm->derefPointer<u64>(offsetPtr);
        void* nativeBuffer;
        if (*offset + arg.size() <= size) {
            nativeBuffer = vm->derefPointer(buffer, size);
        }
        else {
            u64 newSize = std::max(size * 2, *offset + arg.size());
            nativeBuffer = fstringRealloc(vm, buffer, size, *offset, newSize);
        }
        std::memcpy((char*)nativeBuffer + *offset, arg.data(), arg.size());
        *offset += arg.size();
        store(regPtr, buffer);
        store(regPtr + 1, size);
    };
    set(Builtin::fstring_writestr, [](u64* regPtr, VirtualMachine* vm) {
        VirtualPointer argData = load<VirtualPointer>(regPtr + 3);
        u64 argSize = load<u64>(regPtr + 4);
        fstringWriteImpl(regPtr, vm,
                         { (char*)vm->derefPointer(argData, argSize),
                           argSize });
    });
    set(Builtin::fstring_writes64, [](u64* regPtr, VirtualMachine* vm) {
        i64 arg = load<i64>(regPtr + 3);
        char fmtBuffer[256];
        auto result =
            std::to_chars(fmtBuffer, fmtBuffer + sizeof fmtBuffer, arg);
        if (result.ec != std::error_code()) {
            // TODO: Throw error here
            assert(false);
        }
        fstringWriteImpl(regPtr, vm, { fmtBuffer, result.ptr });
    });
    set(Builtin::fstring_writef64, [](u64* regPtr, VirtualMachine* vm) {
        f64 arg = load<f64>(regPtr + 3);
        char fmtBuffer[256];
        auto result =
            std::to_chars(fmtBuffer, fmtBuffer + sizeof fmtBuffer, arg);
        if (result.ec != std::error_code()) {
            // TODO: Throw error here
            assert(false);
        }
        fstringWriteImpl(regPtr, vm, { fmtBuffer, result.ptr });
    });
    set(Builtin::fstring_trim, [](u64* regPtr, VirtualMachine* vm) {
        VirtualPointer buffer = load<VirtualPointer>(regPtr);
        u64 size = load<u64>(regPtr + 1);
        u64 offset = load<u64>(regPtr + 2);
        if (size < offset) {
            // TODO: Throw error here
            assert(false);
        }
        if (size > offset) {
            fstringRealloc(vm, buffer, size, offset, offset);
        }
        store(regPtr, buffer);
        store(regPtr + 1, size);
    });

    /// ##
    set(Builtin::trap, [](u64*, VirtualMachine*) { throwError<TrapError>(); });

    /// ## RNG
    set(Builtin::rand_i64, [](u64* regPtr, VirtualMachine*) {
        static thread_local std::mt19937_64 rng(std::random_device{}());
        u64 randomValue = rng();
        store(regPtr, randomValue);
    });

    assert(static_cast<size_t>(Builtin::_count) == currentIndex &&
           "Missing builtin functions.");
    return result;
}
