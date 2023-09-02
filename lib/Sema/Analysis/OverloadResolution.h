#ifndef SCATHA_SEMA_OVERLOADRESOLUTION_H_
#define SCATHA_SEMA_OVERLOADRESOLUTION_H_

#include <span>

#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Analysis/OverloadResolutionError.h"
#include "Sema/Fwd.h"

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
SCATHA_TESTAPI OverloadResolutionResult
    performOverloadResolution(OverloadSet* overloadSet,
                              std::span<QualType const> argumentTypes,
                              bool isMemberCall);

/// \overload for expressions
SCATHA_TESTAPI OverloadResolutionResult
    performOverloadResolution(OverloadSet* overloadSet,
                              std::span<ast::Expression const* const> arguments,
                              bool isMemberCall);

/// Insert the conversions necessary to make the call to the function selected
/// by overload resolution
SCATHA_TESTAPI bool convertArguments(ast::CallLike& fc,
                                     OverloadResolutionResult const& orResult,
                                     SymbolTable& sym,
                                     IssueHandler& issueHandler);

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADRESOLUTION_H_
