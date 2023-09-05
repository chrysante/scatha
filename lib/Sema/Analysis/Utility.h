#ifndef SCATHA_SEMA_ANALYSIS_UTILITY_H_
#define SCATHA_SEMA_ANALYSIS_UTILITY_H_

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

struct OverloadResolutionResult;
class DTorStack;

/// \Returns the first statement that is an ancestor of \p node or `nullptr` if
/// nonesuch exists
ast::Statement* parentStatement(ast::ASTNode* node);

/// Insert the conversions necessary to make the call to the function selected
/// by overload resolution
SCATHA_TESTAPI void convertArguments(ast::CallLike& fc,
                                     OverloadResolutionResult const& orResult,
                                     DTorStack& dtors,
                                     Context& ctx);

/// Makes a copy of the value of \p expr
/// If the type of \p expr has a copy constructor, the copy constructor is
/// invoked, the AST is rewritten accordingly and a pointer to the
/// `ast::ConstructorCall` is returned.
/// Otherwise the expression is returned as is
/// \Note \p dtors may be null, in that case no destructor call will be emitted
SCATHA_TESTAPI ast::Expression* copyValue(ast::Expression* expr,
                                          DTorStack& dtors,
                                          Context& ctx);

/// \returns \p type downcast to `StructureType`, if \p type is a struct type
/// with non-trivial lifetime Otherwise returns `nullptr`
StructureType const* nonTrivialLifetimeType(ObjectType const* type);

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_UTILITY_H_
