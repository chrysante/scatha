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
/// ├─ Generic
/// ├─ Scope
/// │  ├─ GlobalScope
/// │  ├─ AnonymousScope
/// │  ├─ Function
/// │  └─ Type
/// │     ├─ ObjectType
/// │     │  ├─ BuiltinType
/// │     │  │  ├─ VoidType
/// │     │  │  └─ ArithmeticType
/// │     │  │     ├─ BoolType
/// │     │  │     ├─ ByteType
/// │     │  │     ├─ IntType
/// │     │  │     └─ FloatType
/// │     │  ├─ StructureType
/// │     │  └─ ArrayType
/// │     ├─ QualType
/// │     └─ FunctionType [??, does not exist]
/// └─ PoisonEntity
/// ```

namespace scatha::sema {

class SymbolTable;

class Conversion;

class SemanticIssue;

class FunctionSignature;

///
/// # Forward Declaration of all entity types
///

#define SC_SEMA_ENTITY_DEF(Type, _) class Type;
#include <scatha/Sema/Lists.def>

/// List of all entity types
enum class EntityType {
#define SC_SEMA_ENTITY_DEF(Type, _) Type,
#include <scatha/Sema/Lists.def>
    _count
};

SCATHA_API std::string_view toString(EntityType);

SCATHA_API std::ostream& operator<<(std::ostream&, EntityType);

} // namespace scatha::sema

/// Map types to enum values
#define SC_SEMA_ENTITY_DEF(Type, Abstractness)                                 \
    SC_DYNCAST_MAP(::scatha::sema::Type,                                       \
                   ::scatha::sema::EntityType::Type,                           \
                   Abstractness)
#include <scatha/Sema/Lists.def>

namespace scatha::sema {

enum class EntityCategory { Indeterminate, Value, Type, _count };

SCATHA_API std::ostream& operator<<(std::ostream&, EntityCategory);

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

/// Signedness of arithmetic types
enum class Signedness { Signed, Unsigned };

/// Reference qualifiers of `QualType`
enum class Reference {
    None          = 0,
    ConstImplicit = 1,
    MutImplicit   = 2,
    ConstExplicit = 3,
    MutExplicit   = 4,
};

inline constexpr Reference RefConstImpl = Reference::ConstImplicit;

inline constexpr Reference RefMutImpl = Reference::MutImplicit;

inline constexpr Reference RefConstExpl = Reference::ConstExplicit;

inline constexpr Reference RefMutExpl = Reference::MutExplicit;

/// Mutability qualifiers of `QualType`
enum class Mutability { Const, Mutable };

/// \Returns
/// - `Const` if `type` is a const reference type
/// - `Mutable` if `type` is a mutable reference type
/// - `type->mutability()` if `type` is not a reference type
Mutability baseMutability(QualType const* type);

/// \Returns
/// - `RefConstExpl` if `mut == Const`
/// - `RefMutExpl` if `mut == Mutable`
Reference toExplicitRef(Mutability mut);

/// \Returns
/// - `RefConstImpl` if `mut == Const`
/// - `RefMutImpl` if `mut == Mutable`
Reference toImplicitRef(Mutability mut);

///
enum class FunctionAttribute : unsigned {
    None  = 0,
    All   = unsigned(-1),
    Const = 1 << 0,
    Pure  = 1 << 1
};

UTL_BITFIELD_OPERATORS(FunctionAttribute);

/// # Constant Expressions

#define SC_SEMA_CONSTKIND_DEF(Type, _) class Type;
#include <scatha/Sema/Lists.def>

/// List of all constant value kinds
enum class ConstantKind {
#define SC_SEMA_CONSTKIND_DEF(Type, _) Type,
#include <scatha/Sema/Lists.def>
    _count
};

SCATHA_API std::string_view toString(ConstantKind);

SCATHA_API std::ostream& operator<<(std::ostream&, ConstantKind);

} // namespace scatha::sema

/// Map constant kinds to enum values
#define SC_SEMA_CONSTKIND_DEF(Type, Abstractness)                              \
    SC_DYNCAST_MAP(::scatha::sema::Type,                                       \
                   ::scatha::sema::ConstantKind::Type,                         \
                   Abstractness)
#include <scatha/Sema/Lists.def>

namespace scatha::internal {

/// Insulated call to `delete` on the most derived base of \p *entity
SCATHA_API void privateDelete(sema::Entity* entity);

/// Insulated call to destructor on the most derived base of \p *entity
SCATHA_API void privateDestroy(sema::Entity* entity);

/// Insulated call to `delete` on the most derived base of \p *value
SCATHA_API void privateDelete(sema::Value* value);

/// Insulated call to destructor on the most derived base of \p *value
SCATHA_API void privateDestroy(sema::Value* type);

} // namespace scatha::internal

#endif // SCATHA_SEMA_FWD_H_
