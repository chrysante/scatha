#ifndef SCATHA_IRGEN_VALUE_H_
#define SCATHA_IRGEN_VALUE_H_

#include <iosfwd>
#include <string_view>

#include <utl/hash.hpp>

#include "Common/Base.h"
#include "IR/CFG/Value.h"
#include "Sema/Fwd.h"

namespace scatha::ast {

enum class ValueLocation : uint8_t { Register, Memory };

/// Convert to string
std::string_view toString(ValueLocation);

/// Print to ostream
std::ostream& operator<<(std::ostream& ostream, ValueLocation);

/// Represents an abstract value that is either in a register or in memory
class Value {
public:
    Value() = default;

    explicit Value(uint32_t id,
                   ir::Value* value,
                   ir::Type const* type,
                   ValueLocation location):

        _val(value), _type(type), _id(id), _loc(location) {}

    explicit Value(uint32_t id, ir::Value* value, ValueLocation location):
        Value(id, value, value->type(), location) {
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
    bool isMemory() const { return location() == ValueLocation::Memory; }

    /// The unique ID of this value
    uint32_t ID() const { return _id; }

    void setID(uint32_t id) { _id = id; }

    /// Test the value pointer for null
    explicit operator bool() const { return !!_val; }

    bool operator==(Value const&) const = default;

    size_t hashValue() const { return utl::hash_combine(_val, _type, _loc); }

private:
    ir::Value* _val = nullptr;
    ir::Type const* _type = nullptr;
    uint32_t _id;
    ValueLocation _loc = {};
};

} // namespace scatha::ast

template <>
struct std::hash<scatha::ast::Value> {
    size_t operator()(scatha::ast::Value const& value) const {
        return value.hashValue();
    }
};

#endif // SCATHA_IRGEN_VALUE_H_
