#ifndef SCATHA_SEMA_ANALYSIS_OBJECTSTACK_H_
#define SCATHA_SEMA_ANALYSIS_OBJECTSTACK_H_

#include <utl/stack.hpp>

#include "AST/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

class ObjectStack {
public:
    ///
    void push(Object* obj) { objs.push(obj); }

    ///
    void callDestructors(ast::CompoundStatement* body, size_t insertIndex);

private:
    utl::stack<Object*> objs;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_OBJECTSTACK_H_
