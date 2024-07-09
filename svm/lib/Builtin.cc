#include "BuiltinInternal.h"

#include <cassert>
#include <charconv>
#include <cmath>
#include <format>
#include <iostream>
#include <random>
#include <string>
#include <string_view>

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

/// Loads two consecutive registers as an array pointer structure
/// `{ T*, size_t }`
template <typename T>
static std::span<T> loadArray(u64* regPtr) {
    T* data = load<char*>(regPtr);
    size_t size = load<u64>(regPtr + 1);
    return std::span<T>(data, size);
}

namespace {

template <Builtin>
struct BuiltinImpl;

} // namespace

/// ## Common math functions

template <typename T>
static T fract(T arg) {
    T i;
    return std::copysign(std::modf(arg, &i), T(1.0));
}

template <typename T, size_t N>
using wrap = T;

template <typename Float, size_t NumArgs>
static constexpr BuiltinFunction::FuncPtr math(auto impl) {
    return [](u64* regPtr, VirtualMachine*) {
        [&]<size_t... I>(std::index_sequence<I...>) {
            std::tuple<wrap<Float, I>...> args{ load<Float>(regPtr + I)... };
            store(regPtr, std::apply(decltype(impl){}, args));
        }(std::make_index_sequence<NumArgs>{});
    };
}

#define MATH_FUNCTION(name, ...)                                               \
    [](auto... args) {                                                         \
        using namespace std; /* So we find names from std but can also provide \
                                functions not in std */                        \
        return name(args... __VA_OPT__(, ) __VA_ARGS__);                       \
    }

#define DEFINE_MATH_WRAPPER(BuiltinType, ValueType, NumArgs, Function)         \
    template <>                                                                \
    struct BuiltinImpl<Builtin::BuiltinType> {                                 \
        static constexpr auto* impl = math<ValueType, NumArgs>(Function);      \
    };

DEFINE_MATH_WRAPPER(abs_f64, double, 1, MATH_FUNCTION(abs))
DEFINE_MATH_WRAPPER(exp_f64, double, 1, MATH_FUNCTION(exp))
DEFINE_MATH_WRAPPER(exp2_f64, double, 1, MATH_FUNCTION(exp2))
DEFINE_MATH_WRAPPER(exp10_f64, double, 1, MATH_FUNCTION(pow, 10.0))
DEFINE_MATH_WRAPPER(log_f64, double, 1, MATH_FUNCTION(log))
DEFINE_MATH_WRAPPER(log2_f64, double, 1, MATH_FUNCTION(log2))
DEFINE_MATH_WRAPPER(log10_f64, double, 1, MATH_FUNCTION(log10))
DEFINE_MATH_WRAPPER(pow_f64, double, 2, MATH_FUNCTION(pow))
DEFINE_MATH_WRAPPER(sqrt_f64, double, 1, MATH_FUNCTION(sqrt))
DEFINE_MATH_WRAPPER(cbrt_f64, double, 1, MATH_FUNCTION(cbrt))
DEFINE_MATH_WRAPPER(hypot_f64, double, 2, MATH_FUNCTION(hypot))
DEFINE_MATH_WRAPPER(sin_f64, double, 1, MATH_FUNCTION(sin))
DEFINE_MATH_WRAPPER(cos_f64, double, 1, MATH_FUNCTION(cos))
DEFINE_MATH_WRAPPER(tan_f64, double, 1, MATH_FUNCTION(tan))
DEFINE_MATH_WRAPPER(asin_f64, double, 1, MATH_FUNCTION(asin))
DEFINE_MATH_WRAPPER(acos_f64, double, 1, MATH_FUNCTION(acos))
DEFINE_MATH_WRAPPER(atan_f64, double, 1, MATH_FUNCTION(atan))
DEFINE_MATH_WRAPPER(fract_f64, double, 1, MATH_FUNCTION(fract))
DEFINE_MATH_WRAPPER(floor_f64, double, 1, MATH_FUNCTION(floor))
DEFINE_MATH_WRAPPER(ceil_f64, double, 1, MATH_FUNCTION(ceil))

DEFINE_MATH_WRAPPER(abs_f32, float, 1, MATH_FUNCTION(abs))
DEFINE_MATH_WRAPPER(exp_f32, float, 1, MATH_FUNCTION(exp))
DEFINE_MATH_WRAPPER(exp2_f32, float, 1, MATH_FUNCTION(exp2))
DEFINE_MATH_WRAPPER(exp10_f32, float, 1, MATH_FUNCTION(pow, 10.0f))
DEFINE_MATH_WRAPPER(log_f32, float, 1, MATH_FUNCTION(log))
DEFINE_MATH_WRAPPER(log2_f32, float, 1, MATH_FUNCTION(log2))
DEFINE_MATH_WRAPPER(log10_f32, float, 1, MATH_FUNCTION(log10))
DEFINE_MATH_WRAPPER(pow_f32, float, 2, MATH_FUNCTION(pow))
DEFINE_MATH_WRAPPER(sqrt_f32, float, 1, MATH_FUNCTION(sqrt))
DEFINE_MATH_WRAPPER(cbrt_f32, float, 1, MATH_FUNCTION(cbrt))
DEFINE_MATH_WRAPPER(hypot_f32, float, 2, MATH_FUNCTION(hypot))
DEFINE_MATH_WRAPPER(sin_f32, float, 1, MATH_FUNCTION(sin))
DEFINE_MATH_WRAPPER(cos_f32, float, 1, MATH_FUNCTION(cos))
DEFINE_MATH_WRAPPER(tan_f32, float, 1, MATH_FUNCTION(tan))
DEFINE_MATH_WRAPPER(asin_f32, float, 1, MATH_FUNCTION(asin))
DEFINE_MATH_WRAPPER(acos_f32, float, 1, MATH_FUNCTION(acos))
DEFINE_MATH_WRAPPER(atan_f32, float, 1, MATH_FUNCTION(atan))
DEFINE_MATH_WRAPPER(fract_f32, float, 1, MATH_FUNCTION(fract))
DEFINE_MATH_WRAPPER(floor_f32, float, 1, MATH_FUNCTION(floor))
DEFINE_MATH_WRAPPER(ceil_f32, float, 1, MATH_FUNCTION(ceil))

/// Other functions

#define BUILTIN_DEF(BuiltinType, ...)                                          \
    template <>                                                                \
    struct BuiltinImpl<Builtin::BuiltinType> {                                 \
        static void impl(__VA_ARGS__);                                         \
    };                                                                         \
    void BuiltinImpl<Builtin::BuiltinType>::impl(__VA_ARGS__)

/// ## Memory

BUILTIN_DEF(memcpy, u64* regPtr, VirtualMachine* vm) {
    auto dest = load<VirtualPointer>(regPtr);
    auto size = load<size_t>(regPtr + 1);
    auto source = load<VirtualPointer>(regPtr + 2);
    std::memcpy(deref(vm, dest, size), deref(vm, source, size), size);
}

BUILTIN_DEF(memmove, u64* regPtr, VirtualMachine* vm) {
    auto dest = load<VirtualPointer>(regPtr);
    auto size = load<size_t>(regPtr + 1);
    auto source = load<VirtualPointer>(regPtr + 2);
    std::memmove(deref(vm, dest, size), deref(vm, source, size), size);
}

BUILTIN_DEF(memset, u64* regPtr, VirtualMachine* vm) {
    auto dest = load<VirtualPointer>(regPtr);
    auto size = load<size_t>(regPtr + 1);
    auto value = load<int64_t>(regPtr + 2);
    std::memset(deref(vm, dest, size), static_cast<int>(value), size);
}

BUILTIN_DEF(alloc, u64* regPtr, VirtualMachine* vm) {
    u64 size = load<u64>(regPtr);
    u64 align = load<u64>(regPtr + 1);
    VirtualPointer addr = vm->impl->memory.allocate(size, align);
    store(regPtr, addr);
    store(regPtr + 1, size);
}

BUILTIN_DEF(dealloc, u64* regPtr, VirtualMachine* vm) {
    auto addr = load<VirtualPointer>(regPtr);
    auto size = load<i64>(regPtr + 1);
    auto align = load<i64>(regPtr + 2);
    vm->impl->memory.deallocate(addr, static_cast<size_t>(size),
                                static_cast<size_t>(align));
}

template <typename T>
static void printVal(u64* regPtr, VirtualMachine* vm) {
    T value = load<T>(regPtr);
    *vm->impl->ostream << value;
}

BUILTIN_DEF(putchar, u64* regPtr, VirtualMachine* vm) {
    printVal<char>(regPtr, vm);
}

BUILTIN_DEF(puti64, u64* regPtr, VirtualMachine* vm) {
    printVal<i64>(regPtr, vm);
}

BUILTIN_DEF(putf64, u64* regPtr, VirtualMachine* vm) {
    printVal<f64>(regPtr, vm);
}

static void putstrImpl(u64* regPtr, VirtualMachine* vm) {
    auto data = load<VirtualPointer>(regPtr);
    size_t size = load<size_t>(regPtr + 1);
    *vm->impl->ostream << std::string_view(deref<char>(vm, data, size), size);
}

BUILTIN_DEF(putstr, u64* regPtr, VirtualMachine* vm) {
    auto data = load<VirtualPointer>(regPtr);
    size_t size = load<size_t>(regPtr + 1);
    *vm->impl->ostream << std::string_view(deref<char>(vm, data, size), size);
}

BUILTIN_DEF(putln, u64* regPtr, VirtualMachine* vm) {
    putstrImpl(regPtr, vm);
    *vm->impl->ostream << "\n";
}

BUILTIN_DEF(putptr, u64* regPtr, VirtualMachine* vm) {
    printVal<void*>(regPtr, vm);
}

/// ## Console input

BUILTIN_DEF(readline, u64* regPtr, VirtualMachine* vm) {
    std::string line;
    std::getline(*vm->impl->istream, line);
    auto buffer = vm->impl->memory.allocate(line.size(), 8);
    std::memcpy(deref(vm, buffer, line.size()), line.data(), line.size());
    store(regPtr, buffer);
    store(regPtr + 1, line.size());
}

/// ## String conversion

BUILTIN_DEF(strtos64, u64* regPtr, VirtualMachine* vm) {
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
}

BUILTIN_DEF(strtof64, u64* regPtr, VirtualMachine* vm) {
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
}

/// # FString runtime support

static void* fstringRealloc(VirtualMachine* vm, VirtualPointer& buffer,
                            u64& size, u64 offset, u64 newSize) {
    VirtualPointer newBuffer = vm->allocateMemory(newSize, 1);
    auto* native = vm->derefPointer(newBuffer, newSize);
    if (buffer != VirtualPointer::Null) {
        std::memcpy(native, vm->derefPointer(buffer, offset), offset);
        vm->deallocateMemory(buffer, size, 1);
    }
    buffer = newBuffer;
    size = newSize;
    return native;
}

static void fstringWriteImpl(u64* regPtr, VirtualMachine* vm,
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
}

BUILTIN_DEF(fstring_writestr, u64* regPtr, VirtualMachine* vm) {
    VirtualPointer argData = load<VirtualPointer>(regPtr + 3);
    u64 argSize = load<u64>(regPtr + 4);
    fstringWriteImpl(regPtr, vm,
                     { (char*)vm->derefPointer(argData, argSize), argSize });
}

template <typename T>
static void fstringScalar(u64* regPtr, VirtualMachine* vm) {
    T arg = load<T>(regPtr + 3);
    char fmtBuffer[256];
    auto result = std::to_chars(fmtBuffer, fmtBuffer + sizeof fmtBuffer, arg);
    if (result.ec != std::error_code()) {
        // TODO: Throw error here
        assert(false);
    }
    fstringWriteImpl(regPtr, vm, { fmtBuffer, result.ptr });
}

BUILTIN_DEF(fstring_writes64, u64* regPtr, VirtualMachine* vm) {
    fstringScalar<i64>(regPtr, vm);
}

BUILTIN_DEF(fstring_writeu64, u64* regPtr, VirtualMachine* vm) {
    fstringScalar<u64>(regPtr, vm);
}

BUILTIN_DEF(fstring_writef64, u64* regPtr, VirtualMachine* vm) {
    fstringScalar<f64>(regPtr, vm);
}

BUILTIN_DEF(fstring_writechar, u64* regPtr, VirtualMachine* vm) {
    char arg = load<char>(regPtr + 3);
    fstringWriteImpl(regPtr, vm, { &arg, 1 });
}

BUILTIN_DEF(fstring_writebool, u64* regPtr, VirtualMachine* vm) {
    bool arg = load<bool>(regPtr + 3);
    fstringWriteImpl(regPtr, vm, arg ? "true" : "false");
}

BUILTIN_DEF(fstring_writeptr, u64* regPtr, VirtualMachine* vm) {
    void* arg = load<void*>(regPtr + 3);
    fstringWriteImpl(regPtr, vm, std::format("{}", arg));
}

BUILTIN_DEF(fstring_trim, u64* regPtr, VirtualMachine* vm) {
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
}

BUILTIN_DEF(openfile, u64*, VirtualMachine*) {
    // Unimplemented
    throwError<TrapError>();
}

BUILTIN_DEF(trap, u64*, VirtualMachine*) { throwError<TrapError>(); }

BUILTIN_DEF(rand_i64, u64* regPtr, VirtualMachine*) {
    static thread_local std::mt19937_64 rng(std::random_device{}());
    u64 randomValue = rng();
    store(regPtr, randomValue);
}

std::vector<BuiltinFunction> svm::makeBuiltinTable() {
    // clang-format off
    return {
#define SVM_BUILTIN_DEF(Name, ...)                                             \
    { "__builtin_" #Name, BuiltinImpl<Builtin::Name>::impl },
#include <svm/Builtin.def.h>
    }; // clang-format on
}
