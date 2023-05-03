#ifndef SCATHA_SEMA_CONVERSION_H_
#define SCATHA_SEMA_CONVERSION_H_

#include "AST/Fwd.h"
#include "Issue/IssueHandler.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Checks wether an implicit conversion from type \p from to type \p to exists
bool isImplicitlyConvertible(QualType const* to, QualType const* from);

/// Insert an `ImplicitConversion` node between \p expr and it's parent
/// \pre An implicit conversion from \p expr 's type to type \p to must exist
ast::ImplicitConversion* insertImplicitConversion(ast::Expression* expr,
                                                  sema::QualType const* to);

/// Find the common type of \p a and \p b
QualType const* commonType(QualType const* a, QualType const* b);

/// If necessary it inserts an `ImplicitConversion` node into the AST
/// If \p expr is not convertible to \p to an error is pushed to \p issueHandler
/// \Returns `true` iff \p expr is implicitly convertible to type \p to
bool convertImplicitly(ast::Expression* expr,
                       QualType const* to,
                       IssueHandler& issueHandler);

} // namespace scatha::sema

#endif // SCATHA_SEMA_CONVERSION_H_