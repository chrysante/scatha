#ifndef SCATHA_OPT_INLINECALLSITE_H_
#define SCATHA_OPT_INLINECALLSITE_H_

#include "IR/Common.h"

namespace scatha::ir {

class Context;

} // namespace scatha::ir

namespace scatha::opt {

SCATHA_API void inlineCallsite(ir::Context& ctx, ir::FunctionCall* call);

} // namespace scatha::opt

#endif // SCATHA_OPT_INLINECALLSITE_H_
