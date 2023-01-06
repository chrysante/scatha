// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_SYMBOLID_H_
#define SCATHA_SEMA_SYMBOLID_H_

#include <functional>
#include <iosfwd>
#include <string_view>

#include <utl/common.hpp>

#include <scatha/Basic/Basic.h>

namespace scatha::sema {

enum class SymbolCategory { Invalid, Variable, Namespace, OverloadSet, Function, ObjectType, Anonymous, _count };

static_assert(static_cast<int>(SymbolCategory::_count) <= 1 << 4,
              "SymbolCategory values shall fit into the last four bytes of the SymbolID structure.");

SCATHA(API) std::string_view toString(SymbolCategory);

SCATHA(API) std::ostream& operator<<(std::ostream&, SymbolCategory);

/// Identifies a semantic symbol. Use the symbol table to access the corresponding entity.
class SymbolID {
public:
    static SymbolID const Invalid;

public:
    constexpr SymbolID() = default;
    constexpr explicit SymbolID(u64 rawValue, SymbolCategory category): _value(rawValue), _cat(category) {
        SC_EXPECT(rawValue < (u64(1) << 60), "The SymbolID index must fit into the first 60 bits of the structure.");
    }

    constexpr u64 rawValue() const { return _value; }

    constexpr bool operator==(SymbolID const& rhs) const {
        bool result = _value == rhs._value;
        SC_ASSERT(!result || _cat == rhs._cat, "If values are equal then categories must also be equal.");
        return result;
    }

    u64 hash() const;

    explicit operator bool() const { return *this != Invalid; }

    SymbolCategory category() const { return _cat; }

private:
    u64 _value          : 60 = 0;
    SymbolCategory _cat : 4  = SymbolCategory::Invalid;
};

inline SymbolID const SymbolID::Invalid = SymbolID(0, SymbolCategory::Invalid);

SCATHA(API) std::ostream& operator<<(std::ostream&, SymbolID);

/// Special kind of SymbolID refererring to types.
struct TypeID: SymbolID {
    TypeID() = default;
    constexpr explicit TypeID(u64 rawValue): SymbolID(rawValue, SymbolCategory::ObjectType) {}
    constexpr explicit TypeID(SymbolID id): SymbolID(id) {
        SC_ASSERT(id.category() == SymbolCategory::ObjectType ||
                      id.category() == SymbolCategory::Invalid && id == SymbolID::Invalid,
                  "This symbol id is not a type.");
    }

    static TypeID const Invalid;
};

inline TypeID const TypeID::Invalid = TypeID(SymbolID::Invalid);

} // namespace scatha::sema

template <>
struct std::hash<scatha::sema::SymbolID> {
    std::size_t operator()(scatha::sema::SymbolID id) const { return std::hash<scatha::u64>{}(id.rawValue()); }
};

template <>
struct std::hash<scatha::sema::TypeID> {
    std::size_t operator()(scatha::sema::TypeID id) const { return std::hash<scatha::sema::SymbolID>{}(id); }
};

#endif // SCATHA_SEMA_SYMBOLID_H_
