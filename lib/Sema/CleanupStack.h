#ifndef SCATHA_SEMA_CLEANUPSTACK_H_
#define SCATHA_SEMA_CLEANUPSTACK_H_

#include <iosfwd>

#include <range/v3/view.hpp>
#include <utl/stack.hpp>

#include "AST/Fwd.h"
#include "Common/Base.h"
#include "Sema/Fwd.h"
#include "Sema/LifetimeMetadata.h"

namespace scatha::sema {

/// Represents a call to a destructor
struct CleanupOperation {
    Object* object;
    LifetimeOperation destroy;

    bool operator==(CleanupOperation const&) const = default;
};

/// Stack of cleanup operations
class SCTEST_API CleanupStack {
public:
    /// Push cleanup operation for the object \p obj onto the stack.
    /// The operation is derived from the type of \p obj
    void push(Object* obj);

    /// Push cleanup operation \p op onto the stack.
    void push(CleanupOperation op);

    /// Pop the top cleanup operation off the stack
    void pop() { operations.pop(); }

    /// Erases the cleanup operation \p op from the stack
    /// \Pre \p op must be in the stack
    void remove(CleanupOperation op);

    /// \Returns `true` if the stack is empty
    bool empty() const { return operations.empty(); }

    /// \Returns the number of operations in this cleanup stack
    size_t size() const { return operations.size(); }

    /// \Returns the top of the stack
    CleanupOperation top() const { return operations.top(); }

    /// \Returns an iterator to the top of the stack
    auto begin() const { return operations.rbegin(); }

    /// \Returns an iterator past the bottom of the stack
    auto end() const { return operations.rend(); }

private:
    utl::stack<CleanupOperation> operations;
};

///
void SCTEST_API print(CleanupStack const& stack, std::ostream& str);

///
void SCTEST_API print(CleanupStack const& stack);

} // namespace scatha::sema

#endif // SCATHA_SEMA_CLEANUPSTACK_H_
