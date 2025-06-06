#ifndef SCATHA_SEMA_FWD_H_
#define SCATHA_SEMA_FWD_H_

#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

#include <utl/utility.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::sema {

struct AnalysisResult;
class AnalysisContext;
class Conversion;
class SemanticIssue;
class SymbolTable;
class CleanupStack;
class NameMangler;
class LifetimeOperation;
class LifetimeMetadata;
class QualType;
class ThinExpr;
class VTable;

size_t constexpr InvalidSize = ~size_t(0);

/// Options struct for `sema::analyze()`
struct AnalysisOptions {
    /// Paths to search for libraries
    std::vector<std::filesystem::path> librarySearchPaths;
};

///
/// # Forward Declaration of all entity types
///

#define SC_SEMA_ENTITY_DEF(Type, ...) class Type;
#include <scatha/Sema/Lists.def.h>

/// List of all entity types
enum class EntityType {
#define SC_SEMA_ENTITY_DEF(Type, ...) Type,
#include <scatha/Sema/Lists.def.h>
    LAST = PoisonEntity
};

///
SCATHA_API std::string_view toString(EntityType);

///
SCATHA_API std::ostream& operator<<(std::ostream&, EntityType);

/// To make the base parent case in the dyncast macro work
using VoidParent = void;

} // namespace scatha::sema

/// Map types to enum values
#define SC_SEMA_ENTITY_DEF(Type, Parent, Corporeality)                         \
    SC_DYNCAST_DEFINE(::scatha::sema::Type, ::scatha::sema::EntityType::Type,  \
                      ::scatha::sema::Parent, Corporeality)
#include <scatha/Sema/Lists.def.h>

namespace scatha::sema {

enum class EntityCategory { Indeterminate, Value, Type, Namespace };

SCATHA_API std::ostream& operator<<(std::ostream&, EntityCategory);

/// Different ways to import a library.
/// - `Scoped` corresponds to an `import lib;` statement and means entities are
/// accessible via `lib.entity`
/// - `Unscoped` corresponds to a `use lib;` statement and means entities are
/// directly declared in the current scope
enum class ImportKind { Scoped, Unscoped };

/// Enum to disambiguate value categories
enum class ValueCategory { LValue, RValue };

SCATHA_API std::string_view toString(ValueCategory);

SCATHA_API std::ostream& operator<<(std::ostream&, ValueCategory);

/// \Returns `LValue` if both `a` and`b` are `LValue`, otherwise returns
/// `RValue`
ValueCategory commonValueCat(ValueCategory a, ValueCategory b);

/// Mutability qualifiers of `QualType`
enum class Mutability { Const, Mutable };

SCATHA_API std::string_view toString(Mutability);

SCATHA_API std::ostream& operator<<(std::ostream&, Mutability);

/// Binding modes for pointers and references
/// Dynamic pointers are wide pointers that carry a second pointer to a vtable
enum class PointerBindMode { Static, Dynamic };

SCATHA_API std::string_view toString(PointerBindMode);

SCATHA_API std::ostream& operator<<(std::ostream&, PointerBindMode);

///
enum class ReferenceKind { Reference, Pointer, UniquePointer };

///
enum class ScopeKind {
    Invalid,
    Global,
    Namespace,
    Function,
    Type,
};

SCATHA_API std::string_view toString(ScopeKind);

SCATHA_API std::ostream& operator<<(std::ostream&, ScopeKind);

/// Different kinds of property objects
enum class PropertyKind {
#define SC_SEMA_PROPERTY_KIND(Kind, _) Kind,
#include <scatha/Sema/Lists.def.h>
};

SCATHA_API std::string_view toString(PropertyKind);

SCATHA_API std::ostream& operator<<(std::ostream&, PropertyKind);

///
enum class FunctionKind : u8 { Native, Foreign, Generated };

SCATHA_API std::string_view toString(FunctionKind);

SCATHA_API std::ostream& operator<<(std::ostream&, FunctionKind);

///
enum class AccessControl : uint8_t {
#define SC_SEMA_ACCESS_CONTROL_DEF(Kind, Spelling) Kind,
#include <scatha/Sema/Lists.def.h>
};

///
SCATHA_API std::string_view toString(AccessControl accessControl);

///
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    AccessControl accessControl);

///
inline constexpr AccessControl InvalidAccessControl = AccessControl(-1);

/// Access control \p A is less than access control \p B if more scopes have
/// access to the declared entity, i.e. `Public` is less than `Internal` and
/// `Internal` is less than `Private`. So `A < B` can be thought of as `A` being
/// less access-restricted than `B`
inline std::strong_ordering operator<=>(AccessControl A, AccessControl B) {
    return (int)A <=> (int)B;
}

/// Signedness of arithmetic types
enum class Signedness { Signed, Unsigned };

/// Different kinds of special member functions
enum class SMFKind : uint8_t {
#define SC_SEMA_SMF_DEF(Name, Spelling) Name,
#include <scatha/Sema/Lists.def.h>
    LAST = Destructor
};

/// \Returns the name of \p kind
std::string toString(SMFKind kind);

///
std::ostream& operator<<(std::ostream& ostream, SMFKind kind);

/// \Returns the spelling (i.e. "new", "move" or "delete") of the special member
/// function kind \p kind
SCATHA_API std::string toSpelling(SMFKind kind);

///
enum class FunctionAttribute : unsigned {
    None = 0,
    All = unsigned(-1),
    Const = 1 << 0,
    Pure = 1 << 1
};

UTL_BITFIELD_OPERATORS(FunctionAttribute);

///
/// # Constant Expressions
///

#define SC_SEMA_CONSTKIND_DEF(Type, ...) class Type;
#include <scatha/Sema/Lists.def.h>

/// List of all constant value kinds
enum class ConstantKind {
#define SC_SEMA_CONSTKIND_DEF(Type, ...) Type,
#include <scatha/Sema/Lists.def.h>
    LAST = FloatValue
};

SCATHA_API std::string_view toString(ConstantKind);

SCATHA_API std::ostream& operator<<(std::ostream&, ConstantKind);

/// Conversion between reference qualifications
enum class ValueCatConversion : uint8_t {
#define SC_VALUECATCONV_DEF(Name, ...) Name,
#include <scatha/Sema/Conversion.def.h>
};

SCATHA_API std::string_view toString(ValueCatConversion conv);

SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    ValueCatConversion conv);

/// Conversion between same type with different qualifiers
enum class QualConversion : uint8_t {
#define SC_QUALCONV_DEF(Name, ...) Name,
#include <scatha/Sema/Conversion.def.h>
};

SCATHA_API std::string_view toString(QualConversion conv);

SCATHA_API std::ostream& operator<<(std::ostream& ostream, QualConversion conv);

/// Conversion between different object types
enum class ObjectTypeConversion : uint8_t {
#define SC_OBJTYPECONV_DEF(Name, ...) Name,
#include <scatha/Sema/Conversion.def.h>
};

SCATHA_API std::string_view toString(ObjectTypeConversion conv);

SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    ObjectTypeConversion conv);

/// \Returns `true` if \p conv is one of the cases corresponding to AST
/// construct expr nodes
SCATHA_API bool isConstruction(ObjectTypeConversion conv);

/// \Returns `true` if \p conv is a conversion between arithmetic types
SCATHA_API bool isArithmeticConversion(ObjectTypeConversion conv);

} // namespace scatha::sema

/// Map constant kinds to enum values
#define SC_SEMA_CONSTKIND_DEF(Type, Parent, Corporeality)                      \
    SC_DYNCAST_DEFINE(::scatha::sema::Type,                                    \
                      ::scatha::sema::ConstantKind::Type,                      \
                      ::scatha::sema::Parent, Corporeality)
#include <scatha/Sema/Lists.def.h>

namespace scatha::sema {

/// Insulated call to `delete` on the most derived base of \p entity
SCATHA_API void do_delete(sema::Entity& entity);

/// Insulated call to destructor on the most derived base of \p entity
SCATHA_API void do_destroy(sema::Entity& entity);

/// Insulated call to `delete` on the most derived base of \p value
SCATHA_API void do_delete(sema::Value& value);

/// Insulated call to destructor on the most derived base of \p value
SCATHA_API void do_destroy(sema::Value& type);

} // namespace scatha::sema

#endif // SCATHA_SEMA_FWD_H_
