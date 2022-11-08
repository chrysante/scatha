#ifndef SCATHA_AST_PRINTEXPRESSION_H_
#define SCATHA_AST_PRINTEXPRESSION_H_

#include <iosfwd>
#include <string>

#include "Basic/Basic.h"

namespace scatha::ast {

struct Expression;

SCATHA(API) std::string toString(Expression const&);

SCATHA(API) void printExpression(Expression const&);
SCATHA(API) void printExpression(Expression const&, std::ostream&);

} // namespace scatha::ast

#endif // SCATHA_AST_PRINTEXPRESSION_H_
