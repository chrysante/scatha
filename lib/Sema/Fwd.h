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
/// ├─ Scope
/// │  ├─ GlobalScope
/// │  ├─ Function
/// │  ├─ Type
/// │  │  ├─ ObjectType
/// │  │  ├─ ReferenceType
/// │  │  └─ FunctionType [??, does not exist]
/// │  └─ AnonymousScope [??, does not exist either]
/// ├─ OverloadSet
/// └─ Variable
/// ```

namespace scatha::sema {

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

#endif // SCATHA_SEMA_FWD_H_
