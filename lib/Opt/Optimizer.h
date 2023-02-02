// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_OPT_OPTIMIZER_H_
#define SCATHA_OPT_OPTIMIZER_H_

#include <scatha/Basic/Basic.h>

namespace scatha::ir {

class Context;
class Module;

} // namespace scatha::ir

namespace scatha::opt {

SCATHA(API) void optimize(ir::Context& context, ir::Module& mod, int level);

} // namespace scatha::opt

#endif // SCATHA_OPT_OPTIMIZER_H_