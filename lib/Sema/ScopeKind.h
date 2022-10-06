#ifndef SCATHA_SEMA_SCOPEKIND_H_
#define SCATHA_SEMA_SCOPEKIND_H_

#include <ostream>
#include <string_view>

#include "Basic/Basic.h"

namespace scatha::sema {

enum class ScopeKind { Global, Namespace, Variable, Function, Object, Anonymous, _count };

SCATHA(API) std::string_view toString(ScopeKind);

SCATHA(API) std::ostream &operator<<(std::ostream &, ScopeKind);

} // namespace scatha::sema

#endif // SCATHA_SEMA_SCOPEKIND_H_
