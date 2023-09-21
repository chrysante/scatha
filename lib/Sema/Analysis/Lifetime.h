#ifndef SCATHA_SEMA_ANALYSIS_LIFETIME_H_
#define SCATHA_SEMA_ANALYSIS_LIFETIME_H_

#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Analysis/Context.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

class DTorStack;

/// Make a call to the constructor of \p type with arguments \p arguments
/// The `this` argument is added later and must not be a part of \p arguments
/// If the type does not have a constructor a `TrivialConstructExpr` is returned
/// if possible.
/// An error is pushed to \p issueHandler if no matching constructor
/// is found
UniquePtr<ast::Expression> makePseudoConstructorCall(
    sema::ObjectType const* type,
    UniquePtr<ast::Expression> objectArgument,
    utl::small_vector<UniquePtr<ast::Expression>> arguments,
    DTorStack& dtors,
    Context& ctx,
    SourceRange sourceRange);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_LIFETIME_H_
