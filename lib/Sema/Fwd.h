#ifndef SCATHA_SEMA_FWD_H_
#define SCATHA_SEMA_FWD_H_

#include <iosfwd>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

/// Class hierarchy of `Entity`
///
/// ```
/// Entity
/// ├─ Variable
/// ├─ OverloadSet
/// └─ Scope
///    ├─ GlobalScope
///    ├─ AnonymousScope
///    ├─ Function
///    └─ Type
///       ├─ ObjectType
///       │  ├─ StructureType
///       │  └─ ArrayType
///       ├─ QualType
///       └─ FunctionType [??, does not exist]
/// ```

namespace scatha::sema {

class SymbolTable;

class Conversion;

class SemanticIssue;

///
/// # Forward Declaration of all entity types
///

#define SC_SEMA_ENTITY_DEF(Type, _) class Type;
#include <scatha/Sema/Lists.def>

/// List of all entity types types.
enum class EntityType {
#define SC_SEMA_ENTITY_DEF(Type, _) Type,
#include <scatha/Sema/Lists.def>
    _count
};

SCATHA_API std::string_view toString(EntityType);

SCATHA_API std::ostream& operator<<(std::ostream&, EntityType);

} // namespace scatha::sema

/// Map types to enum values.
#define SC_SEMA_ENTITY_DEF(Type, Abstractness)                                 \
    SC_DYNCAST_MAP(::scatha::sema::Type,                                       \
                   ::scatha::sema::EntityType::Type,                           \
                   Abstractness)
#include <scatha/Sema/Lists.def>

namespace scatha::sema {

enum class EntityCategory { Indeterminate, Value, Type, _count };

SCATHA_API std::ostream& operator<<(std::ostream&, EntityCategory);

EntityCategory categorize(EntityType entityType);

enum class ValueCategory : u8 { None, LValue, RValue, _count };

SCATHA_API std::ostream& operator<<(std::ostream&, ValueCategory);

///
enum class ScopeKind {
    Invalid,
    Global,
    Namespace,
    Variable,
    Function,
    Object,
    Anonymous,
    _count
};

SCATHA_API std::string_view toString(ScopeKind);

SCATHA_API std::ostream& operator<<(std::ostream&, ScopeKind);

///
enum class FunctionKind : u8 { Native, External, Special, _count };

SCATHA_API std::string_view toString(FunctionKind);

SCATHA_API std::ostream& operator<<(std::ostream&, FunctionKind);

///
enum class ConversionKind : u8 { SignedToUnsigned, UnsignedToSigned, _count };

SCATHA_API std::string_view toString(ConversionKind);

SCATHA_API std::ostream& operator<<(std::ostream&, ConversionKind);

///
enum class AccessSpecifier { Public, Private };

/// Reference qualifiers of `QualType`
enum class Reference { None = 0, Implicit = 1, Explicit = 2 };

/// Mutability qualifiers of `QualType`
enum class Mutability { Const, Mutable };

///
enum class FunctionAttribute : unsigned {
    None  = 0,
    All   = unsigned(-1),
    Const = 1 << 0,
    Pure  = 1 << 1
};

UTL_BITFIELD_OPERATORS(FunctionAttribute);

} // namespace scatha::sema

#endif // SCATHA_SEMA_FWD_H_
