#include "AST/Base.h"

#include "AST/AST.h"

using namespace scatha;
using namespace ast;

void scatha::internal::privateDelete(ast::AbstractSyntaxTree* node) {
    visit(*node, [](auto& derived) { delete &derived; });
}
