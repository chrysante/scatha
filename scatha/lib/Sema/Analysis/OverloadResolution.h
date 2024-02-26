#ifndef SCATHA_SEMA_OVERLOADRESOLUTION_H_
#define SCATHA_SEMA_OVERLOADRESOLUTION_H_

#include <span>

#include <range/v3/view.hpp>
#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Sema/Analysis/Conversion.h"
#include "Sema/Fwd.h"
#include "Sema/SemaIssues.h"

namespace scatha::sema {

/// Result structure returned from `performOverloadResolution()`
struct OverloadResolutionResult {
    /// The selected function if overload resolution succeeded
    Function* function = nullptr;

    /// The conversions required for each argument type to call selected
    /// function.
    /// Only non-empty if overload resolution succeeded
    utl::small_vector<Conversion> conversions = {};

    /// The error if overload resolution failed
    std::unique_ptr<ORError> error = nullptr;
};

/// Kinds of overload resolution. This distinction is necessary because for
/// member function calls we convert the first argument explicitly
enum class ORKind { FreeFunction, MemberFunction };

/// Performs overload resolution
/// \param parentExpr Needed to construct OR errors
/// \param overloadSet Must not be empty
SCTEST_API OverloadResolutionResult performOverloadResolution(
    ast::Expression const* parentExpr, std::span<Function* const> overloadSet,
    std::span<ast::Expression const* const> arguments,
    ORKind kind = ORKind::FreeFunction);

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADRESOLUTION_H_
