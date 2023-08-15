#ifndef SCATHA_AST_LOWERING_VALUELOCATION_H_
#define SCATHA_AST_LOWERING_VALUELOCATION_H_

#include <utl/hash.hpp>

#include "Common/Base.h"
#include "IR/CFG/Value.h"
#include "Sema/Fwd.h"

namespace scatha::ast {

enum class ValueLocation : uint8_t { Register, Memory };

/// Represents an abstract value that is either in a register or in memory
class Value {
public:
    Value() = default;

    explicit Value(ir::Value* value,
                   ir::Type const* type,
                   ValueLocation location,
                   sema::ValueCategory valueCat = sema::ValueCategory::LValue):
        _val(value), _type(type), _loc(location), _cat(valueCat) {}

    explicit Value(ir::Value* value, ValueLocation location):
        Value(value, value->type(), location) {
        SC_ASSERT(
            location == ValueLocation::Register,
            "If the value is in memory the type must be specified explicitly");
    }

    /// \Returns either the value or the address of the value, depending on
    /// whether this value is in a register or in memory
    ir::Value* get() const { return _val; }

    /// \Returns the IR type of the _abstract_ value. This differs from
    /// `get()->type()` because if the value is in memory the concrete IR type
    /// is always `ptr`
    ir::Type const* type() const { return _type; }

    /// \Returns the location of the value
    ValueLocation location() const { return _loc; }

    /// \Returns `true` if this value is in a register
    bool isRegister() const { return location() == ValueLocation::Register; }

    /// \Returns `true` if this value is in memory
    bool isMemory() const { return location() == ValueLocation::Register; }

    /// The value category of this value. Only meaningful if the value is in
    /// memory Defaults to `LValue` \Note We store this information here,
    /// because in certain cases the memory of rvalue arguments can be reused
    sema::ValueCategory valueCategory() const { return _cat; }

    /// \Returns `valueCategory() == ValueCategory::LValue`
    bool isLValue() const {
        return valueCategory() == sema::ValueCategory::LValue;
    }

    /// \Returns `valueCategory() == ValueCategory::RValue`
    bool isRValue() const {
        return valueCategory() == sema::ValueCategory::RValue;
    }

    /// \Returns the same value but marked as lvalue to prevent further reuse
    Value toLValue() const {
        return Value(get(), type(), location(), sema::ValueCategory::LValue);
    }

    /// Test the value pointer for null
    explicit operator bool() const { return !!_val; }

    bool operator==(Value const&) const = default;

    size_t hashValue() const {
        return utl::hash_combine(_val, _type, _loc, _cat);
    }

private:
    ir::Value* _val = nullptr;
    ir::Type const* _type = nullptr;
    ValueLocation _loc = {};
    sema::ValueCategory _cat = {};
};

} // namespace scatha::ast

template <>
struct std::hash<scatha::ast::Value> {
    size_t operator()(scatha::ast::Value const& value) const {
        return value.hashValue();
    }
};

#endif // SCATHA_AST_LOWERING_VALUELOCATION_H_
