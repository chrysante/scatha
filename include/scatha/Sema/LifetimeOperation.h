#ifndef SCATHA_SEMA_LIFETIMEOPERATION_H_
#define SCATHA_SEMA_LIFETIMEOPERATION_H_

#include <iosfwd>

#include <scatha/Sema/Fwd.h>

namespace scatha::sema {

/// Represents the lifetime operations default-, copy- and move construction,
/// copy- and move assignment and destruction
class LifetimeOperation {
public:
    enum Kind {
        /// Default kind
        Trivial,

        /// Nontrivlal lifetime operation. Lifetime operation is performed by a
        /// call to `function()`
        /// \Note this state does not imply that the function is user defined.
        /// Nontrivial lifetime functions can be compiler generated, e.g. for
        /// structs with data members with non-trivial lifetime
        Nontrivial,

        /// Nontrivlal lifetime operation. Lifetime operation is performed by
        /// generated inline code
        NontrivialInline,

        /// This lifetime operation is unavailable
        Deleted,
    };

    /// For deserialization
    LifetimeOperation() = default;

    /// Construct a non trivial lifetime operation from a function
    /// `kind()` will be set to `Nontrivial` if \p function is nonnull or
    /// `Deleted` otherwise
    LifetimeOperation(Function* function):
        LifetimeOperation(function ? Nontrivial : Deleted, function) {}

    /// Implicitly construct a lifetime operation from its kind.
    /// \Pre \p kind must not be `Nontrivial`. Use other constructor to
    /// construct nontrivlal lifetime operation
    LifetimeOperation(Kind kind): LifetimeOperation(kind, nullptr) {
        SC_EXPECT(kind != Nontrivial);
    }

    /// For deserialization
    LifetimeOperation(Kind kind, Function* function):
        _kind(kind), fn(function) {}

    /// \Returns the kind of this lifetime operation
    Kind kind() const { return _kind; }

    /// \Returns the function that performs this operation. This is non-null iff
    /// `kind() == Nontrivial`
    Function* function() const { return fn; }

    /// \Returns `kind() == Trivial`
    bool isTrivial() const { return kind() == Trivial; }

    /// \Returns `kind() == Deleted`
    bool isDeleted() const { return kind() == Deleted; }

    bool operator==(LifetimeOperation const&) const = default;

private:
    Kind _kind{};
    Function* fn = nullptr;
};

/// Pretty print the lifetime operation \p op to \p ostream
std::ostream& operator<<(std::ostream& ostream, LifetimeOperation op);

} // namespace scatha::sema

#endif // SCATHA_SEMA_LIFETIMEOPERATION_H_
