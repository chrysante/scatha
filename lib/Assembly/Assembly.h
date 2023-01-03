#ifndef SCATHA_CODEGEN_ASSEMBLY_H_
#define SCATHA_CODEGEN_ASSEMBLY_H_

#include <functional>
#include <iosfwd>
#include <variant>

#include <utl/bit.hpp>
#include <utl/hash.hpp>
#include <utl/utility.hpp>

#include "Basic/Basic.h"

namespace scatha::assembly {

/**
 The same instruction set as enum class OpCode in VM/OpCcode.h, but with a kind
 of overload resolution, meaning the exact opcode will be deduced from the
 arguments by the assembler. E.g. 'movRR', 'movRV', 'movMR', 'movRM' are here
 only represented as 'mov'.
 */

enum class Instruction : u8 {
    enterFn,
    setBrk,
    call,
    ret,
    terminate,
    mov,
    alloca_,
    jmp,
    je,
    jne,
    jl,
    jle,
    jg,
    jge,
    ucmp,
    icmp,
    fcmp,
    itest,
    utest,
    sete,
    setne,
    setl,
    setle,
    setg,
    setge,
    lnt,
    bnt,
    add,
    sub,
    mul,
    div,
    idiv,
    rem,
    irem,
    fadd,
    fsub,
    fmul,
    fdiv,
    sl,
    sr,
    And,
    Or,
    XOr,
    callExt,

    _count
};

std::ostream& operator<<(std::ostream&, Instruction);

struct Label {
    static constexpr u64 functionBeginIndex = static_cast<u64>(-1);

    explicit Label(u64 functionID, u64 index = functionBeginIndex): functionID(functionID), index(index) {}

    bool operator==(Label const&) const = default;

    u64 functionID;
    u64 index;
};

std::ostream& operator<<(std::ostream&, Label);

struct RegisterIndex {
    explicit RegisterIndex(std::integral auto index): index(utl::narrow_cast<u8>(index)) {}

    bool operator==(RegisterIndex const&) const = default;

    constexpr static size_t size() { return sizeof index; }

    u8 index;
};

std::ostream& operator<<(std::ostream&, RegisterIndex);

struct MemoryAddress {
    explicit MemoryAddress(u8 ptrRegIdx, u8 offset, u8 offsetShift):
        ptrRegIdx(ptrRegIdx), offset(offset), offsetShift(offsetShift) {}

    u8 ptrRegIdx;
    u8 offset;
    u8 offsetShift;
};

std::ostream& operator<<(std::ostream&, MemoryAddress);

struct Value8 {
    explicit Value8(u8 value): value(value) {}

    static constexpr size_t size() { return sizeof value; }

    u8 value;
};

std::ostream& operator<<(std::ostream&, Value8);

struct Value16 {
    explicit Value16(u16 value): value(value) {}

    u16 value;
};

std::ostream& operator<<(std::ostream&, Value16);

struct Value32 {
    explicit Value32(u32 value): value(value) {}

    u32 value;
};

std::ostream& operator<<(std::ostream&, Value32);

struct Value64 {
    enum Type { UnsignedIntegral, SignedIntegral, FloatingPoint };

    explicit Value64(u64 value, Type type = UnsignedIntegral): type(type), value(value) {}

    // Only used for debugging and testing
    Type type;
    u64 value;
};

std::ostream& operator<<(std::ostream&, Value64);

// Only used for hand-written assembly, e.g. for testing purposes.
inline Value64 Unsigned64(u64 value) {
    return Value64(static_cast<u64>(value), Value64::UnsignedIntegral);
}
inline Value64 Signed64(i64 value) {
    return Value64(static_cast<u64>(value), Value64::SignedIntegral);
}
inline Value64 Float64(f64 value) {
    return Value64(utl::bit_cast<u64>(value), Value64::FloatingPoint);
}

// Only for internal use.
struct EndOfProgram {};

enum class Marker : u16 {
    none = 0,

    // enum class Instruction
    Instruction = 1 << 0,

    // unique 64 bit wide identifier
    Label = 1 << 1,

    // u8
    RegisterIndex = 1 << 2,

    // u8 ptrRegIdx, u8 offset, u8 offsetShift
    MemoryAddress = 1 << 3,

    Value8  = 1 << 4,
    Value16 = 1 << 5,
    Value32 = 1 << 6,
    Value64 = 1 << 7,

    // End of program. Must not be written into the assembler, only used
    // internally.
    EndOfProgram = 1 << 8,
};

std::ostream& operator<<(std::ostream&, Marker);

void validate(Marker, size_t line);

template <typename>
struct ToMarker;

template <>
struct ToMarker<Instruction>: std::integral_constant<Marker, Marker::Instruction> {};
template <>
struct ToMarker<Label>: std::integral_constant<Marker, Marker::Label> {};
template <>
struct ToMarker<RegisterIndex>: std::integral_constant<Marker, Marker::RegisterIndex> {};
template <>
struct ToMarker<MemoryAddress>: std::integral_constant<Marker, Marker::MemoryAddress> {};
template <>
struct ToMarker<Value8>: std::integral_constant<Marker, Marker::Value8> {};
template <>
struct ToMarker<Value16>: std::integral_constant<Marker, Marker::Value16> {};
template <>
struct ToMarker<Value32>: std::integral_constant<Marker, Marker::Value32> {};
template <>
struct ToMarker<Value64>: std::integral_constant<Marker, Marker::Value64> {};
template <>
struct ToMarker<EndOfProgram>: std::integral_constant<Marker, Marker::EndOfProgram> {};

using ElementVariant = std::variant<assembly::Instruction,
                                    assembly::Label,
                                    assembly::RegisterIndex,
                                    assembly::MemoryAddress,
                                    assembly::Value8,
                                    assembly::Value16,
                                    assembly::Value32,
                                    assembly::Value64,
                                    EndOfProgram>;

struct Element: ElementVariant {
    using ElementVariant::ElementVariant;
    Marker marker() const { return Marker(1 << this->index()); }
    template <typename T>
    T get() const {
        return std::get<T>(*this);
    }
};

std::ostream& operator<<(std::ostream&, Element const&);

} // namespace scatha::assembly

template <>
struct std::hash<scatha::assembly::Label> {
    std::size_t operator()(scatha::assembly::Label l) const { return utl::hash_combine(l.functionID, l.index); }
};

#endif // SCATHA_CODEGEN_ASSEMBLY_H_
