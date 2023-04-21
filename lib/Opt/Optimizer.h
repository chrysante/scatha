// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_OPT_OPTIMIZER_H_
#define SCATHA_OPT_OPTIMIZER_H_

#include <scatha/Common/Base.h>

namespace scatha::ir {

class Context;
class Module;

} // namespace scatha::ir

namespace scatha::opt {

SCATHA_API void optimize(ir::Context& context, ir::Module& mod, int level);

} // namespace scatha::opt

#endif // SCATHA_OPT_OPTIMIZER_H_
