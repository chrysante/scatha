#ifndef SCATHA_IRGEN_VALUE_H_
#define SCATHA_IRGEN_VALUE_H_

#include <iosfwd>
#include <string_view>
#include <span>
#include <initializer_list>

#include <utl/hash.hpp>
#include <utl/vector.hpp>

#include "Common/Base.h"
#include "Common/Ranges.h"
#include "IR/CFG/Value.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// Values can be in registers or in memory. This enum represents that property
enum class ValueLocation : uint8_t { Register, Memory };

/// Convert to string
std::string_view toString(ValueLocation);

/// Print to ostream
std::ostream& operator<<(std::ostream& ostream, ValueLocation);

/// Some types ("fat pointer" types) can have different representations depending on the context.
/// For example a pointer to a dynamic array `*[int]` can be represented as a value of type `{ ptr, i64 }` (packed) or as two seperate values of type `ptr`, `i64` (unpacked)
enum class ValueRepresentation {
    Packed, Unpacked
};

/// Convert to string
std::string_view toString(ValueRepresentation);

/// Print to ostream
std::ostream& operator<<(std::ostream& ostream, ValueRepresentation);

/// Represents an abstract value that is either in a register or in memory
class Value {
public:
    /// # Static constructors
    
    /// Create a packed value
    static Value Packed(sema::Object const* semaObj,
                  ir::Value* value,
                  ValueLocation loc) {
        return Value(semaObj, std::array{ value }, loc, ValueRepresentation::Packed);
    }
    
    /// Create an unpacked value
    static Value Unpacked(sema::Object const* semaObj,
                    std::span<ir::Value* const> values,
                    ValueLocation loc) {
        return Value(semaObj, values, loc, ValueRepresentation::Unpacked);
    }
    
    /// \overload
    static Value Unpacked(sema::Object const* semaObj,
                    std::initializer_list<ir::Value*> values,
                    ValueLocation loc) {
        return Unpacked(semaObj, std::span(values), loc);
    }
    
    /// TODO: Document this
    sema::Object const* semaObject() const { return semaObj; }
    
    /// TODO: Document this
    SC_NODEBUG std::span<ir::Value* const> get() const { return _vals; }

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
    
private:
    explicit Value(sema::Object const* semaObj,
                   std::span<ir::Value* const> values,
                   ValueLocation location,
                   ValueRepresentation repr):
        semaObj(semaObj),
        _vals(values | ToSmallVector<>),
        _loc(location),
        _repr(repr) {}
    
    sema::Object const* semaObj;
    utl::small_vector<ir::Value*, 2> _vals;
    ValueLocation _loc = {};
    ValueRepresentation _repr;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_VALUE_H_
