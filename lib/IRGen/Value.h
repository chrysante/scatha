#ifndef SCATHA_IRGEN_VALUE_H_
#define SCATHA_IRGEN_VALUE_H_

#include <initializer_list>
#include <iosfwd>
#include <span>
#include <string_view>

#include <range/v3/view.hpp>
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

/// Some types ("fat pointer" types) can have different representations
/// depending on the context. For example a pointer to a dynamic array `*[int]`
/// can be represented as a value of type `{ ptr, i64 }` (packed) or as two
/// seperate values of type `ptr`, `i64` (unpacked)
enum class ValueRepresentation { Packed, Unpacked };

/// Convert to string
std::string_view toString(ValueRepresentation);

/// Print to ostream
std::ostream& operator<<(std::ostream& ostream, ValueRepresentation);

/// Represents an abstract value that is either in a register or in memory
class Value {
public:
    /// # Static constructors

    explicit Value(std::string name, sema::ObjectType const* type,
                   std::span<ir::Value* const> values, ValueLocation loc,
                   ValueRepresentation repr):
        _name(std::move(name)),
        _type(type),
        _vals(values | ToSmallVector<2>),
        _loc(loc),
        _repr(repr) {}

    explicit Value(std::string name, sema::ObjectType const* type,
                   std::initializer_list<ir::Value*> values, ValueLocation loc,
                   ValueRepresentation repr):
        Value(std::move(name), type, std::span(values), loc, repr) {}

    /// \Returns the name of this value
    std::string const& name() const { return _name; }

    /// TODO: Document this
    std::span<ir::Value* const> get() const { return _vals; }

    /// TODO: Document this
    ir::Value* get(size_t index) const {
        SC_EXPECT(index < _vals.size());
        return _vals[index];
    }

    /// TODO: Document this
    sema::ObjectType const* type() const { return _type; }

    /// \Returns the location of the value
    ValueLocation location() const { return _loc; }

    /// \Returns `true` if this value is in a register
    bool isRegister() const { return location() == ValueLocation::Register; }

    /// \Returns `true` if this value is in memory
    bool isMemory() const { return location() == ValueLocation::Memory; }

    /// \Returns the representation of this value
    ValueRepresentation representation() const { return _repr; }

    /// \Returns `true` is this value is in packed representation
    bool isPacked() const {
        return representation() == ValueRepresentation::Packed;
    }

    /// \Returns `true` is this value is in unpacked representation
    bool isUnpacked() const {
        return representation() == ValueRepresentation::Unpacked;
    }

private:
    std::string _name;
    sema::ObjectType const* _type;
    utl::small_vector<ir::Value*, 2> _vals;
    ValueLocation _loc;
    ValueRepresentation _repr;
};

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_VALUE_H_
