#ifndef SCATHA_AST_PRINTTREE_H_
#define SCATHA_AST_PRINTTREE_H_

#include <iosfwd>
#include <string>

#include "AST/AST.h"

namespace scatha::ast {

SCATHA(API) void printTree(AbstractSyntaxTree const*);
SCATHA(API) void printTree(AbstractSyntaxTree const*, std::ostream&);

SCATHA(API) std::string toString(Expression const&);

SCATHA(API) void printExpression(Expression const&);
SCATHA(API) void printExpression(Expression const&, std::ostream&);

} // namespace scatha::ast

#endif // SCATHA_AST_PRINTTREE_H_
