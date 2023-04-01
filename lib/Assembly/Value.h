#ifndef SCATHA_ASSEMBLY_VALUES_H_
#define SCATHA_ASSEMBLY_VALUES_H_

#include <utl/bit.hpp>
#include <utl/utility.hpp>
#include <utl/variant.hpp>

#include "Assembly/Common.h"
#include "Basic/Basic.h"

namespace scatha::Asm {

/// Base class of all values in the assembly module.
/// This defines the main interface of the value class.
class ValueBase {
public:
    u64 value() const { return _value; }

    bool operator==(ValueBase const&) const = default;

    Value64 widen() const;

protected:
    template <typename T>
    explicit ValueBase(utl::tag<T> type, auto value):
        _value(utl::bit_cast<u64>(widenFundType(utl::narrow_cast<T>(value)))) {}

    static f64 widenFundType(std::floating_point auto f) { return f; }
    static i64 widenFundType(std::signed_integral auto i) { return i; }
    static u64 widenFundType(std::unsigned_integral auto i) { return i; }

protected:
    u64 _value;
};

/// Represents an 8 bit value.
class Value8: public ValueBase {
public:
    explicit Value8(std::signed_integral auto value):
        ValueBase(utl::tag<i8>{}, value) {}
    explicit Value8(std::unsigned_integral auto value):
        ValueBase(utl::tag<u8>{}, value) {}
};

/// Represents a 16 bit value.
class Value16: public ValueBase {
public:
    explicit Value16(std::signed_integral auto value):
        ValueBase(utl::tag<i16>{}, value) {}
    explicit Value16(std::unsigned_integral auto value):
        ValueBase(utl::tag<u16>{}, value) {}
};

/// Represents a 32 bit value.
class Value32: public ValueBase {
public:
    explicit Value32(std::signed_integral auto value):
        ValueBase(utl::tag<i32>{}, value) {}
    explicit Value32(std::unsigned_integral auto value):
        ValueBase(utl::tag<u32>{}, value) {}
    explicit Value32(f32 value): ValueBase(utl::tag<f32>{}, value) {}
};

/// Represents a 64 bit value.
class Value64: public ValueBase {
public:
    explicit Value64(std::signed_integral auto value):
        ValueBase(utl::tag<i64>{}, value) {}
    explicit Value64(std::unsigned_integral auto value):
        ValueBase(utl::tag<u64>{}, value) {}
    explicit Value64(f64 value): ValueBase(utl::tag<f64>{}, value) {}
};

inline Value64 ValueBase::widen() const { return Value64(value()); }

/// Represents a register index.
class RegisterIndex: public ValueBase {
public:
    explicit RegisterIndex(std::integral auto index):
        ValueBase(utl::tag<u8>{}, index) {}

    void setValue(u64 index) { _value = utl::narrow_cast<u8>(index); }
};

/// Represents a memory address.
class MemoryAddress: public ValueBase {
    static u64 compose(u8 baseptrRegIdx,
                       u8 offsetCountRegIdx,
                       u8 constantOffsetMultiplier,
                       u8 constantInnerOffset) {
        return u64(baseptrRegIdx) | u64(offsetCountRegIdx) << 8 |
               u64(constantOffsetMultiplier) << 16 |
               u64(constantInnerOffset) << 24;
    }

    static std::array<u8, 4> decompose(u64 value) {
        return { static_cast<u8>((value >> 0) & 0xFF),
                 static_cast<u8>((value >> 8) & 0xFF),
                 static_cast<u8>((value >> 16) & 0xFF),
                 static_cast<u8>((value >> 24) & 0xFF) };
    }

public:
    /// See documentation in "OpCode.h"
    SCATHA_TESTAPI static RegisterIndex const InvalidRegisterIndex;

    explicit MemoryAddress(RegisterIndex baseptrRegIdx):
        MemoryAddress(baseptrRegIdx.value()) {}

    explicit MemoryAddress(std::integral auto baseptrRegIdx):
        MemoryAddress(baseptrRegIdx, InvalidRegisterIndex.value(), 0, 0) {}

    explicit MemoryAddress(RegisterIndex baseptrRegIdx,
                           RegisterIndex offsetCountRegIdx,
                           std::integral auto constantOffsetMultiplier,
                           std::integral auto constantInnerOffset):
        MemoryAddress(baseptrRegIdx.value(),
                      offsetCountRegIdx.value(),
                      constantOffsetMultiplier,
                      constantInnerOffset) {}

    explicit MemoryAddress(std::integral auto baseptrRegIdx,
                           std::integral auto offsetCountRegIdx,
                           std::integral auto constantOffsetMultiplier,
                           std::integral auto constantInnerOffset):
        ValueBase(utl::tag<u64>{},
                  compose(utl::narrow_cast<u8>(baseptrRegIdx),
                          utl::narrow_cast<u8>(offsetCountRegIdx),
                          utl::narrow_cast<u8>(constantOffsetMultiplier),
                          utl::narrow_cast<u8>(constantInnerOffset))) {}

    size_t baseptrRegisterIndex() const {
        auto const [baseptrRegIdx,
                    offsetCountRegIdx,
                    constantOffsetMultiplier,
                    constantInnerOffset] = decompose(value());
        return baseptrRegIdx;
    }

    size_t offsetCountRegisterIndex() const {
        auto const [baseptrRegIdx,
                    offsetCountRegIdx,
                    constantOffsetMultiplier,
                    constantInnerOffset] = decompose(value());
        return offsetCountRegIdx;
    }

    size_t constantOffsetMultiplier() const {
        auto const [baseptrRegIdx,
                    offsetCountRegIdx,
                    constantOffsetMultiplier,
                    constantInnerOffset] = decompose(value());
        return constantOffsetMultiplier;
    }

    size_t constantInnerOffset() const {
        auto const [baseptrRegIdx,
                    offsetCountRegIdx,
                    constantOffsetMultiplier,
                    constantInnerOffset] = decompose(value());
        return constantInnerOffset;
    }

    bool onlyEvaluatesInnerOffset() const {
        return offsetCountRegisterIndex() == InvalidRegisterIndex.value();
    }
};

namespace internal {

using ValueVariantBase = utl::cbvariant<ValueBase,
#define SC_ASM_VALUE_DEF(value) value
#define SC_ASM_VALUE_SEPARATOR  ,
#include "Assembly/Lists.def"
                                        >;

} // namespace internal

/// Concrete value class.
class Value: public internal::ValueVariantBase {
public:
    using internal::ValueVariantBase::ValueVariantBase;
    ValueType valueType() const { return static_cast<ValueType>(index()); }
};

Value promote(Value const& value, size_t size);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_VALUES_H_
