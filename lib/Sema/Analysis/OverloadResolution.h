#ifndef SCATHA_SEMA_OVERLOADRESOLUTION_H_
#define SCATHA_SEMA_OVERLOADRESOLUTION_H_

#include <span>

#include <utl/vector.hpp>

#include "Sema/Fwd.h"

namespace scatha::sema {

/// Result type of overload resolution
struct OverloadResolutionResult {
    enum State { Success, NoMatchingFunction, Ambiguous };

    /// The state of the result
    State state;

    /// The unique resolved function. This is null unless `state == Success`
    Function* function;

    /// The possible canditates if `state == Ambiguous`
    utl::small_vector<Function*> ambiguousCandidates;

    /// \Returns `true` iff `state == Success`
    explicit operator bool() const { return state == Success; }
};

/// Performs overload resolution
OverloadResolutionResult performOverloadResolution(
    OverloadSet* overloadSet, std::span<QualType const*> argumentTypes);

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADRESOLUTION_H_
