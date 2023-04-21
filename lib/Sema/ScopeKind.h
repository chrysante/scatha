// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_SEMA_SCOPEKIND_H_
#define SCATHA_SEMA_SCOPEKIND_H_

#include <ostream>
#include <string_view>

#include <scatha/Common/Base.h>

namespace scatha::sema {

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

} // namespace scatha::sema

#endif // SCATHA_SEMA_SCOPEKIND_H_
