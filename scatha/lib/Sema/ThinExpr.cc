#include "Sema/ThinExpr.h"

#include "AST/AST.h"

using namespace scatha;
using namespace sema;

ThinExpr::ThinExpr(ast::Expression const* expr):
    _type(expr->type()), _valueCat(expr->valueCategory()) {}
