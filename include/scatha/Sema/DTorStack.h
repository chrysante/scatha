#ifndef SCATHA_SEMA_ANALYSIS_OBJECTSTACK_H_
#define SCATHA_SEMA_ANALYSIS_OBJECTSTACK_H_

#include <range/v3/view.hpp>
#include <utl/stack.hpp>

#include "AST/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Represents a call to a destructor
struct DestructorCall {
    Object* object;
    Function* destructor;
};

/// Stack of destructor calls
class DTorStack {
public:
    /// Push destructor call for the object \p obj onto the stack.
    /// The destructor function is derived from the type of \p obj
    void push(Object* obj);

    /// Push destructor call \p dtorCall onto the stack.
    void push(DestructorCall dtorCall) { dtorCalls.push(dtorCall); }

    /// Pop the top destructor call off the stack
    void pop() { dtorCalls.pop(); }

    /// \Returns `true` if the stack is empty
    bool empty() const { return dtorCalls.empty(); }

    /// \Returns the top of the stack
    DestructorCall top() const { return dtorCalls.top(); }

    /// \Returns an iterator to the top of the stack
    auto begin() const { return dtorCalls.rbegin(); }

    /// \Returns an iterator past the bottom of the stack
    auto end() const { return dtorCalls.rend(); }

private:
    utl::stack<DestructorCall> dtorCalls;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_ANALYSIS_OBJECTSTACK_H_
