#ifndef SCATHA_SEMA_LIFETIMEMETADATA_H_
#define SCATHA_SEMA_LIFETIMEMETADATA_H_

#include <array>
#include <iosfwd>

#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Ranges.h>
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

    /// Construct a non trivial lifetime operation from a function
    /// `kind()` will be set to `Nontrivial` if \p function is nonnull or
    /// `Deleted` otherwise
    LifetimeOperation(Function* function):
        _kind(function ? Nontrivial : Deleted), fn(function) {}

    /// Implicitly construct a lifetime operation from its kind.
    /// \Pre \p kind must not be `Nontrivial`. Use other constructor to
    /// construct nontrivlal lifetime operation
    LifetimeOperation(Kind kind): _kind(kind), fn(nullptr) {
        SC_EXPECT(kind != Nontrivial);
    }

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
    Kind _kind;
    Function* fn = nullptr;
};

/// Pretty print the lifetime operation \p op to \p ostream
std::ostream& operator<<(std::ostream& ostream, LifetimeOperation op);

/// Lifetime metadata for an object type with nontrivial lifetime
class LifetimeMetadata {
public:
    LifetimeMetadata(LifetimeOperation defaultConstructor,
                     LifetimeOperation copyConstructor,
                     LifetimeOperation moveConstructor,
                     LifetimeOperation destructor):
        ops{ defaultConstructor, copyConstructor, moveConstructor,
             destructor } {}

    /// \Returns the default constructor
    LifetimeOperation defaultConstructor() const {
        return operation(SMFKind::DefaultConstructor);
    }

    /// \Returns the copy constructor
    LifetimeOperation copyConstructor() const {
        return operation(SMFKind::CopyConstructor);
    }

    /// \Returns the move constructor
    LifetimeOperation moveConstructor() const {
        return operation(SMFKind::MoveConstructor);
    }

    /// \Returns the destructor
    LifetimeOperation destructor() const {
        return operation(SMFKind::Destructor);
    }

    /// \Returns the lifetime operation \p kind
    LifetimeOperation operation(SMFKind kind) const {
        return ops[(size_t)kind];
    }

    /// \Returns a view over all lifetime operations
    std::span<LifetimeOperation const> operations() const { return ops; }

    /// \Returns `true` if the lifetime operations _copyConstructor_,
    /// _moveConstructor_ and _destructor_ are trivial
    bool trivialLifetime() const {
        return copyConstructor().isTrivial() && moveConstructor().isTrivial() &&
               destructor().isTrivial();
    }

private:
    std::array<LifetimeOperation, 4> ops;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_LIFETIMEMETADATA_H_
