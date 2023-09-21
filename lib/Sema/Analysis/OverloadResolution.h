#ifndef SCATHA_SEMA_OVERLOADRESOLUTION_H_
#define SCATHA_SEMA_OVERLOADRESOLUTION_H_

#include <span>

#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Fwd.h"
#include "Sema/SemanticIssue.h"

namespace scatha::sema {

/// Result structure returned from `performOverloadResolution()`
struct OverloadResolutionResult {
    /// The selected function if overload resolution succeeded
    Function* function;

    /// The conversions required for each argument type to call selected
    /// function
    utl::small_vector<Conversion> conversions;

    /// The error if overload resolution failed. `nullptr` otherwise
    std::unique_ptr<OverloadResolutionError> error;
};

/// Performs overload resolution
SCTEST_API OverloadResolutionResult performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<std::pair<QualType, ValueCategory> const> argumentTypes,
    bool isMemberCall);

/// \overload for expressions
SCTEST_API OverloadResolutionResult
    performOverloadResolution(OverloadSet* overloadSet,
                              std::span<ast::Expression const* const> arguments,
                              bool isMemberCall);

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADRESOLUTION_H_
