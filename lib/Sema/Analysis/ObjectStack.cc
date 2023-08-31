#include "Sema/Analysis/ObjectStack.h"

#include "AST/AST.h"
#include "Sema/Analysis/Lifetime.h"

using namespace scatha;
using namespace sema;

void ObjectStack::callDestructors(ast::CompoundStatement* body,
                                  size_t insertIndex) {
    while (!objs.empty()) {
        auto* obj = objs.pop();
        auto stmt = makeDestructorCallStmt(obj);
        if (stmt) {
            body->insertChild(insertIndex++, std::move(stmt));
        }
    }
}
