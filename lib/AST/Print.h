// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_PRINTTREE_H_
#define SCATHA_AST_PRINTTREE_H_

#include <iosfwd>

#include <scatha/AST/Base.h>
#include <scatha/Basic/Basic.h>

namespace scatha::ast {

SCATHA_API std::string toString(Expression const&);

SCATHA_API void printExpression(Expression const&);

SCATHA_API void printExpression(Expression const&, std::ostream&);

SCATHA_API void printSource(AbstractSyntaxTree const&);

SCATHA_API void printSource(AbstractSyntaxTree const&, std::ostream&);

SCATHA_API void printTree(AbstractSyntaxTree const&);

SCATHA_API void printTree(AbstractSyntaxTree const&, std::ostream&);

} // namespace scatha::ast

#endif // SCATHA_AST_PRINTTREE_H_
