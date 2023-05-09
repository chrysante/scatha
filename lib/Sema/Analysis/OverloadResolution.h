#ifndef SCATHA_SEMA_OVERLOADRESOLUTION_H_
#define SCATHA_SEMA_OVERLOADRESOLUTION_H_

#include <span>

#include <utl/vector.hpp>

#include "AST/Fwd.h"
#include "Common/Expected.h"
#include "Sema/Analysis/OverloadResolutionError.h"
#include "Sema/Fwd.h"

namespace scatha::sema {

/// Performs overload resolution
Expected<Function*, OverloadResolutionError*> performOverloadResolution(
    OverloadSet* overloadSet,
    std::span<ast::Expression const* const> arguments,
    bool isMemberCall);

} // namespace scatha::sema

#endif // SCATHA_SEMA_OVERLOADRESOLUTION_H_
