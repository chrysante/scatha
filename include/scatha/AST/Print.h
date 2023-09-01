// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_PRINTTREE_H_
#define SCATHA_AST_PRINTTREE_H_

#include <iosfwd>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>

namespace scatha::ast {

SCATHA_API std::string toString(Expression const&);

SCATHA_API void printExpression(Expression const&);

SCATHA_API void printExpression(Expression const&, std::ostream&);

SCATHA_API void printTree(ASTNode const&);

SCATHA_API void printTree(ASTNode const&, std::ostream&);

} // namespace scatha::ast

#endif // SCATHA_AST_PRINTTREE_H_
