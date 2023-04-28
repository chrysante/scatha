#include "Sema/Fwd.h"

using namespace scatha;
using namespace sema;

std::string_view sema::toString(EntityType t) {
    return std::array{
#define SC_SEMA_ENTITY_DEF(Type, _) std::string_view(#Type),
#include "Sema/Lists.def"
    }[static_cast<size_t>(t)];
}

std::ostream& sema::operator<<(std::ostream& str, EntityType t) {
    return str << toString(t);
}

std::string_view sema::toString(ScopeKind k) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(k, {
        { ScopeKind::Invalid,   "Invalid" },
        { ScopeKind::Global,    "Global" },
        { ScopeKind::Namespace, "Namespace" },
        { ScopeKind::Variable,  "Variable" },
        { ScopeKind::Function,  "Function" },
        { ScopeKind::Object,    "Object" },
        { ScopeKind::Anonymous, "Anonymous" },
    });
    // clang-format on
}

std::ostream& sema::operator<<(std::ostream& str, ScopeKind k) {
    return str << toString(k);
}
