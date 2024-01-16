#ifndef SCATHA_IRGEN_VALUE_H_
#define SCATHA_IRGEN_VALUE_H_

#include <iosfwd>
#include <string_view>

#include <utl/hash.hpp>

#include "Common/Base.h"
#include "IR/CFG/Value.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// Values can be in registers or in memory. This enum represents that property
enum class ValueLocation : uint8_t { Register, Memory };

/// Some types ("fat pointer" types) can have different representations depending on the context.
/// For example a pointer to a dynamic array `*[int]` can be represented as a value of type `{ ptr, i64 }` (packed) or as two seperate values of type `ptr`, `i64` (unpacked)
enum class ValueRepresentation {
    Packed, Unpacked
};

/// Convert to string
std::string_view toString(ValueLocation);

/// Print to ostream
std::ostream& operator<<(std::ostream& ostream, ValueLocation);

/// Represents an abstract value that is either in a register or in memory
class Value {
public:
    Value() = default;

    SC_NODEBUG explicit Value(ir::Value* value,
                              ir::Type const* type,
                              ValueLocation location,
                              ValueRepresentation repr):
        _val(value), _type(type), _loc(location), _repr(repr) {}

//    SC_NODEBUG explicit Value(ir::Value* value, ValueLocation location):
//        Value(value, value->type(), location) {
//        SC_ASSERT(location == ValueLocation::Register,
//                  "If the value is in memory the type must be specified "
//                  "explicitly");
//    }

    /// \Returns either the value or the address of the value, depending on
    /// whether this value is in a register or in memory
    SC_NODEBUG ir::Value* get() const { return _val; }

    /// \Returns the IR type of the _abstract_ value. This differs from
    /// `get()->type()` because if the value is in memory the concrete IR type
    /// is always `ptr`
    SC_NODEBUG ir::Type const* type() const { return _type; }

    /// \Returns the location of the value
    SC_NODEBUG ValueLocation location() const { return _loc; }

    /// \Returns `true` if this value is in a register
    SC_NODEBUG bool isRegister() const {
        return location() == ValueLocation::Register;
    }

    /// \Returns `true` if this value is in memory
    SC_NODEBUG bool isMemory() const {
        return location() == ValueLocation::Memory;
    }

    /// \Returns the representation of this value
    ValueRepresentation representation() const { return _repr; }
    
    /// \Returns `true` is this value is in packed representation
    bool isPacked() const { return representation() == ValueRepresentation::Packed; }
    
    /// \Returns `true` is this value is in unpacked representation
    bool isUnpacked() const { return representation() == ValueRepresentation::Unpacked; }
    
    /// Test the value pointer for null
    SC_NODEBUG explicit operator bool() const { return !!_val; }

    bool operator==(Value const&) const = default;

    SC_NODEBUG size_t hashValue() const {
        return utl::hash_combine(_val, _type, _loc);
    }

private:
    ir::Value* _val = nullptr;
    ir::Type const* _type = nullptr;
    ValueLocation _loc = {};
    ValueRepresentation _repr;
};

} // namespace scatha::irgen

template <>
struct std::hash<scatha::irgen::Value> {
    size_t operator()(scatha::irgen::Value const& value) const {
        return value.hashValue();
    }
};

#endif // SCATHA_IRGEN_VALUE_H_
