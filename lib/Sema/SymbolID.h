#ifndef SCATHA_SEMA_SYMBOLID_H_
#define SCATHA_SEMA_SYMBOLID_H_

#include <functional>
#include <iosfwd>
#include <string_view>

#include <utl/common.hpp>

#include "Basic/Basic.h"

namespace scatha::sema {

class SymbolID {
  public:
    constexpr SymbolID() = default;
    constexpr explicit SymbolID(u64 rawValue): _value(rawValue) {}

    static SymbolID const Invalid;

    constexpr u64         rawValue() const { return _value; }

    constexpr bool        operator==(SymbolID const &) const = default;

    u64                   hash() const;

    explicit              operator bool() const { return *this != Invalid; }

  private:
    u64 _value = 0;
};

inline SymbolID const     SymbolID::Invalid = SymbolID(0);

SCATHA(API) std::ostream &operator<<(std::ostream &, SymbolID);

// Special kind of SymbolID
struct TypeID: SymbolID {
    constexpr explicit TypeID(SymbolID id): SymbolID(id) {}
    using SymbolID::SymbolID;

    static TypeID const Invalid;
};

inline TypeID const TypeID::Invalid = TypeID(SymbolID::Invalid);

enum class SymbolCategory {
    Variable    = 1 << 0,
    Namespace   = 1 << 1,
    OverloadSet = 1 << 2,
    Function    = 1 << 3,
    ObjectType  = 1 << 4,
    _count
};

UTL_ENUM_OPERATORS(SymbolCategory);

SCATHA(API) std::string_view toString(SymbolCategory);

SCATHA(API) std::ostream &operator<<(std::ostream &, SymbolCategory);

} // namespace scatha::sema

template <> struct std::hash<scatha::sema::SymbolID> {
    std::size_t operator()(scatha::sema::SymbolID id) const { return std::hash<scatha::u64>{}(id.rawValue()); }
};

template <> struct std::hash<scatha::sema::TypeID> {
    std::size_t operator()(scatha::sema::TypeID id) const { return std::hash<scatha::sema::SymbolID>{}(id); }
};

#endif // SCATHA_SEMA_SYMBOLID_H_
