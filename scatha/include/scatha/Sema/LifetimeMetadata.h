#ifndef SCATHA_SEMA_LIFETIMEMETADATA_H_
#define SCATHA_SEMA_LIFETIMEMETADATA_H_

#include <array>
#include <iosfwd>

#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Common/Ranges.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/LifetimeOperation.h>

namespace scatha::sema {

/// Lifetime metadata for an object type with nontrivial lifetime
class LifetimeMetadata {
public:
    LifetimeMetadata(LifetimeOperation defaultConstructor,
                     LifetimeOperation copyConstructor,
                     LifetimeOperation moveConstructor,
                     LifetimeOperation destructor):
        LifetimeMetadata(std::array{ defaultConstructor, copyConstructor,
                                     moveConstructor, destructor }) {}

    explicit LifetimeMetadata(std::array<LifetimeOperation, 4> ops): ops(ops) {}

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

    /// \Returns the move constructor if the move constructor is not deleted,
    /// otherwise return the copy constructor
    /// \Note that the copy constructor may be deleted as well
    LifetimeOperation moveOrCopyConstructor() const {
        if (auto move = moveConstructor(); !move.isDeleted()) {
            return move;
        }
        return copyConstructor();
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
