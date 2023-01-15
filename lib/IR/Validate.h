#ifndef SCATHA_IR_VALIDATE_H_
#define SCATHA_IR_VALIDATE_H_

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::ir {

SCATHA(API) void assertInvariants(Context const& ctx, Module const& mod);

SCATHA(API) void setupInvariants(Context& ctx, Module& mod);

SCATHA(API) void setupInvariants(Context& ctx, Function& function);

} // namespace scatha::ir

#endif // SCATHA_IR_VALIDATE_H_
