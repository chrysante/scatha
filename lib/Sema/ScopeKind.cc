#include "ScopeKind.h"

#include <ostream>

#include <utl/utility.hpp>

namespace scatha::sema {

std::string_view toString(ScopeKind k) {
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

std::ostream& operator<<(std::ostream& str, ScopeKind k) {
    return str << toString(k);
}

} // namespace scatha::sema
