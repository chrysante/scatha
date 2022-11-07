#ifndef SCATHA_AST_PRINTTREE_H_
#define SCATHA_AST_PRINTTREE_H_

#include <iosfwd>

#include "AST/Base.h"
#include "Basic/Basic.h"

namespace scatha::ast {

SCATHA(API) void printTree(AbstractSyntaxTree const&);
SCATHA(API) void printTree(AbstractSyntaxTree const&, std::ostream&);

} // namespace scatha::ast

#endif // SCATHA_AST_PRINTTREE_H_
