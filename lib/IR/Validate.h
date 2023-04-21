#ifndef SCATHA_IR_VALIDATE_H_
#define SCATHA_IR_VALIDATE_H_

#include "Basic/Basic.h"
#include "IR/Fwd.h"

namespace scatha::ir {

SCATHA_API void assertInvariants(Context& ctx, Module const& mod);

SCATHA_API void assertInvariants(Context& ctx, Function const& function);

SCATHA_API void setupInvariants(Context& ctx, Module& mod);

SCATHA_API void setupInvariants(Context& ctx, Function& function);

} // namespace scatha::ir

#endif // SCATHA_IR_VALIDATE_H_
