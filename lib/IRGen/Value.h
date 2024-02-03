#ifndef SCATHA_IRGEN_VALUE_H_
#define SCATHA_IRGEN_VALUE_H_

#include <initializer_list>
#include <iosfwd>
#include <span>
#include <string_view>

#include <range/v3/view.hpp>
#include <utl/hash.hpp>
#include <utl/ipp.hpp>
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

/// Represents one IR value that can either be in a register or in memory
class Atom {
public:
    static Atom Register(ir::Value* value) {
        return Atom(value, ValueLocation::Register);
    }

    static Atom Memory(ir::Value* value) {
        return Atom(value, ValueLocation::Memory);
    }

    Atom(ir::Value* value, ValueLocation location): _value(value, location) {}

    /// \Returns the location of the value
    ValueLocation location() const { return _value.integer(); }

    /// \Returns `true` if this value is in a register
    bool isRegister() const { return location() == ValueLocation::Register; }

    /// \Returns `true` if this value is in memory
    bool isMemory() const { return location() == ValueLocation::Memory; }

    /// \Returns the `ir::Value` pointer
    ir::Value* get() const { return _value.pointer(); }

    ///
    ir::Value* operator->() const { return get(); }

    ///
    ir::Value& operator*() const { return *get(); }

private:
    utl::ipp<ir::Value*, ValueLocation, 1> _value;
};

void print(Atom atom);

void print(Atom atom, std::ostream& ostream);

std::ostream& operator<<(std::ostream& ostream, Atom atom);

/// Represents an abstract value that is composed of multiple atoms
class Value {
public:
    using const_iterator = utl::vector<Atom>::const_iterator;
    using iterator = const_iterator;

    static Value Packed(std::string name, sema::ObjectType const* type,
                        Atom elem) {
        return Value(std::move(name), type, { elem },
                     ValueRepresentation::Packed);
    }

    static Value Unpacked(std::string name, sema::ObjectType const* type,
                          std::span<Atom const> elems) {
        return Value(std::move(name), type, elems,
                     ValueRepresentation::Unpacked);
    }

    static Value Unpacked(std::string name, sema::ObjectType const* type,
                          std::initializer_list<Atom> elems) {
        return Unpacked(std::move(name), type, std::span<Atom const>(elems));
    }

    explicit Value(std::string name, sema::ObjectType const* type,
                   std::span<Atom const> elems, ValueRepresentation repr):
        _name(std::move(name)),
        typeAndRepr(type, repr),
        elems(elems | ToSmallVector<2>) {}

    explicit Value(std::string name, sema::ObjectType const* type,
                   std::initializer_list<Atom> elems, ValueRepresentation repr):
        Value(std::move(name), type, std::span(elems), repr) {}

    /// \Returns the name of this value
    std::string const& name() const { return _name; }

    /// TODO: Document this
    sema::ObjectType const* type() const { return typeAndRepr.pointer(); }

    /// \Returns the representation of this value
    ValueRepresentation representation() const { return typeAndRepr.integer(); }

    /// \Returns `true` is this value is in packed representation
    bool isPacked() const {
        return representation() == ValueRepresentation::Packed;
    }

    /// \Returns `true` is this value is in unpacked representation
    bool isUnpacked() const {
        return representation() == ValueRepresentation::Unpacked;
    }

    /// \Returns a view over the atoms of this value.
    /// This is the same range that this class models with the iterator
    /// interface
    std::span<Atom const> elements() const { return elems; }

    /// \Returns the single atom of this value
    /// \Pre Must consist of single atom
    Atom single() const {
        SC_EXPECT(size() == 1);
        return (*this)[0];
    }

    /// Range interface @{
    iterator begin() const { return elems.begin(); }
    iterator end() const { return elems.end(); }
    size_t size() const { return elems.size(); }
    bool empty() const { return elems.empty(); }
    Atom operator[](size_t index) const {
        SC_EXPECT(index < elems.size());
        return elems[index];
    }
    /// @}

private:
    std::string _name;
    utl::ipp<sema::ObjectType const*, ValueRepresentation, 1> typeAndRepr;
    utl::small_vector<Atom, 2> elems;
};

void print(Value const& value);

void print(Value const& value, std::ostream& ostream);

std::ostream& operator<<(std::ostream& ostream, Value const& value);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_VALUE_H_
