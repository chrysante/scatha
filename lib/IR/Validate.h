#ifndef SCATHA_IR_VALIDATE_H_
#define SCATHA_IR_VALIDATE_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::ir {

SCATHA_API void assertInvariants(Context& ctx, Module const& mod);

SCATHA_API void assertInvariants(Context& ctx, Function const& function);

} // namespace scatha::ir

#endif // SCATHA_IR_VALIDATE_H_
