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
    LifetimeOperation operation;

    bool operator==(CleanupOperation const&) const = default;
};

/// Stack of cleanup operations
class SCTEST_API CleanupStack {
public:
    /// Pushes \p cleanup unconditionally to the stack
    void push(CleanupOperation cleanup) { operations.push(cleanup); }

    /// Push cleanup operation for the object \p obj onto the stack if the
    /// derived cleanup operation is nontrivial. The operation is derived from
    /// the type of \p obj \Param ctx is used to push an issue if the object
    /// type has deleted destructor
    /// \Returns `true` if no error occured
    [[nodiscard]] bool push(Object* obj, AnalysisContext& ctx);

    /// \overload for expressions. Equivalent to `push(expr->object())`
    [[nodiscard]] bool push(ast::Expression* expr, AnalysisContext& ctx);

    /// If the object \p obj is of non-trivial lifetime type, this function
    /// pops the top element off the cleanup stack \p cleanups
    /// \Pre the object of \p expr must have a trivial destructor or be the top
    /// of \p cleanup stack \Note The \p obj parameter is only for validation
    /// that to top element actually corresponds to \p obj
    void pop(Object* obj);

    /// \overload for expressions. Equivalent to `pop(expr->object())`
    void pop(ast::Expression* expr);

    /// If the object \p obj is of non-trivial lifetime type, this function
    /// erases the cleanup of the object in the stack
    /// \Pre \p obj must have a trivial destructor or be in this stack
    void erase(Object* obj);

    /// \overload for expressions. Equivalent to `erase(expr->object())`
    void erase(ast::Expression* expr);

    /// Removes all cleanup operations from this stack
    void clear() { operations.clear(); }

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
