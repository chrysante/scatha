#ifndef SCATHA_SEMA_FWD_H_
#define SCATHA_SEMA_FWD_H_

#include <filesystem>
#include <iosfwd>
#include <string_view>
#include <vector>

#include <scatha/Common/Base.h>
#include <scatha/Common/Dyncast.h>

namespace scatha::sema {

struct AnalysisResult;
class AnalysisContext;
class Conversion;
class FunctionSignature;
class SemanticIssue;
class SymbolTable;
class DtorStack;

/// Options struct for `sema::analyze()`
struct AnalysisOptions {
    /// Paths to search for libraries
    std::vector<std::filesystem::path> librarySearchPaths;
};

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

/// Forward declaration of `QualType` class
/// See "Sema/QualType.h" for details.
class QualType;

enum class EntityCategory { Indeterminate, Value, Type, Namespace };

SCATHA_API std::ostream& operator<<(std::ostream&, EntityCategory);

///
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
#include <scatha/Sema/Lists.def>
};

SCATHA_API std::string_view toString(PropertyKind);

SCATHA_API std::ostream& operator<<(std::ostream&, PropertyKind);

///
enum class FunctionKind : u8 { Native, Foreign, Generated };

SCATHA_API std::string_view toString(FunctionKind);

SCATHA_API std::ostream& operator<<(std::ostream&, FunctionKind);

/// `public` or `private`. Defines whether the name is allowed to be referenced
/// in a specific context.
enum class AccessSpecifier : uint8_t { Public, Private };

/// `export` or `internal`. Defines whether a function or member functions of a
/// type will have entries in the binary symbol table after compilation. All
/// functions default to `internal` except for `main()` which defaults to
/// `export`. Note that there is no keyword for `internal`, because everything
/// defaults to that.
enum class BinaryVisibility : uint8_t { Export, Internal };

/// Signedness of arithmetic types
enum class Signedness { Signed, Unsigned };

/// Special member functions
/// These are all constructors, destructor, and perhaps more to come
enum class SpecialMemberFunction : uint8_t {
#define SC_SEMA_SPECIAL_MEMBER_FUNCTION_DEF(Name, str) Name,
#include <scatha/Sema/Lists.def>
};

} // namespace scatha::sema

SC_ENUM_SIZE_LAST_DEF(scatha::sema::SpecialMemberFunction, Delete);

namespace scatha::sema {

SCATHA_API std::string_view toString(SpecialMemberFunction);

SCATHA_API std::ostream& operator<<(std::ostream&, SpecialMemberFunction);

/// Special lifetime functions
/// These are the default constructor, the copy and move constructors and the
/// destructor These are special in the sense that they will be automatically
/// generated by the compiler
enum class SpecialLifetimeFunction : uint8_t {
#define SC_SEMA_SPECIAL_LIFETIME_FUNCTION_DEF(Name) Name,
#include <scatha/Sema/Lists.def>
};

/// Get the corresponding special member function (`new`, `move` or `delete`)
SCATHA_API SpecialMemberFunction toSMF(SpecialLifetimeFunction);

} // namespace scatha::sema

SC_ENUM_SIZE_LAST_DEF(scatha::sema::SpecialLifetimeFunction, Destructor);

namespace scatha::sema {

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

namespace scatha::sema {

/// Insulated call to `delete` on the most derived base of \p entity
SCATHA_API void privateDelete(sema::Entity* entity);

/// Insulated call to destructor on the most derived base of \p entity
SCATHA_API void privateDestroy(sema::Entity* entity);

/// Insulated call to `delete` on the most derived base of \p value
SCATHA_API void privateDelete(sema::Value* value);

/// Insulated call to destructor on the most derived base of \p value
SCATHA_API void privateDestroy(sema::Value* type);

} // namespace scatha::sema

#endif // SCATHA_SEMA_FWD_H_
