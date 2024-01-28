#ifndef SCATHA_OPT_INLINECALLSITE_H_
#define SCATHA_OPT_INLINECALLSITE_H_

#include "Common/UniquePtr.h"
#include "IR/Fwd.h"

namespace scatha::ir {

class Context;

} // namespace scatha::ir

namespace scatha::opt {

SCATHA_API void inlineCallsite(ir::Context& ctx, ir::Call* call);

SCATHA_API void inlineCallsite(ir::Context& ctx, ir::Call* call,
                               UniquePtr<ir::Function> calleeClone);

} // namespace scatha::opt

#endif // SCATHA_OPT_INLINECALLSITE_H_
