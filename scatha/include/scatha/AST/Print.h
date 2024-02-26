#ifndef SCATHA_AST_PRINT_H_
#define SCATHA_AST_PRINT_H_

#include <iosfwd>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>

namespace scatha::ast {

/// Hierarchically prints the AST node \p node  to \p ostream
SCATHA_API void print(ASTNode const&, std::ostream&);

/// \Overload
SCATHA_API void print(ASTNode const& node);

} // namespace scatha::ast

#endif // SCATHA_AST_PRINT_H_
