#ifndef SCATHA_SEMA_ANALYSIS_LIFETIME_H_
#define SCATHA_SEMA_ANALYSIS_LIFETIME_H_

#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

class DTorStack;

/// Make a call to the constructor of \p type with arguments \p arguments
/// The `this` argument is added later and must be a part of \p arguments
/// An error is pushed to \p issueHandler if no matching constructor is found
UniquePtr<ast::ConstructorCall> makeConstructorCall(
    sema::ObjectType const* type,
    utl::small_vector<UniquePtr<ast::Expression>> arguments,
    DTorStack& dtors,
    SymbolTable& symbolTable,
    IssueHandler& issueHandler,
    SourceRange sourceRange);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_LIFETIME_H_
