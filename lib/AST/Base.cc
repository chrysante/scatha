#include "AST/Base.h"

#include "AST/AST.h"

using namespace scatha;
using namespace ast;

void AbstractSyntaxTree::privateDestroy() {
    visit(*this, [](auto& derived) {
        std::destroy_at(&derived);
    });
}
