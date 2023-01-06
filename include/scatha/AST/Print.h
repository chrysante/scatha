// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_AST_PRINTTREE_H_
#define SCATHA_AST_PRINTTREE_H_

#include <iosfwd>

#include <scatha/AST/Base.h>
#include <scatha/Basic/Basic.h>

namespace scatha::ast {

SCATHA(API) std::string toString(Expression const&);

SCATHA(API) void printExpression(Expression const&);

SCATHA(API) void printExpression(Expression const&, std::ostream&);

SCATHA(API) void printSource(AbstractSyntaxTree const&);

SCATHA(API) void printSource(AbstractSyntaxTree const&, std::ostream&);

SCATHA(API) void printTree(AbstractSyntaxTree const&);

SCATHA(API) void printTree(AbstractSyntaxTree const&, std::ostream&);

} // namespace scatha::ast

#endif // SCATHA_AST_PRINTTREE_H_
