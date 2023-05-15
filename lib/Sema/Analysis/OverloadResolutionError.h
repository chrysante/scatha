#ifndef SCATHA_SEMA_OVERLOADRESOLUTIONERROR_H_
#define SCATHA_SEMA_OVERLOADRESOLUTIONERROR_H_

#include <span>

#include <utl/vector.hpp>

#include "Sema/Fwd.h"
#include "Sema/SemanticIssue.h"

namespace scatha::sema {

/// Base class of overload resolution errors
class SCATHA_API OverloadResolutionError: public SemanticIssue {
public:
    OverloadResolutionError(OverloadSet const* overloadSet);

    /// The overload set
    OverloadSet const* overloadSet() const { return os; }

private:
    OverloadSet const* os;
};

/// Error emitted if no function matches the arguments
class SCATHA_API NoMatchingFunction: public OverloadResolutionError {
public:
    using OverloadResolutionError::OverloadResolutionError;

private:
    void format(std::ostream&) const override;
};

/// Error emitted if more than one function matches the arguments
class SCATHA_API AmbiguousOverloadResolution: public OverloadResolutionError {
public:
    AmbiguousOverloadResolution(OverloadSet const* overloadSet,
                                utl::small_vector<Function const*> matches):
        OverloadResolutionError(overloadSet), _matches(std::move(matches)) {}

    /// The functions matching the given arguments
    std::span<Function const* const> matches() const { return _matches; }

private:
    void format(std::ostream&) const override;

    utl::small_vector<Function const*> _matches;
};

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADRESOLUTIONERROR_H_
