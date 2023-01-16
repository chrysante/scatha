#ifndef SCATHA_OPT_SCC_H_
#define SCATHA_OPT_SCC_H_

#include "Basic/Basic.h"

namespace scatha::ir {

class Context;
class Module;

} // namespace scatha::ir

namespace scatha::opt {

SCATHA(API) void scc(ir::Context& context, ir::Module& mod);

} // namespace scatha::opt

#endif // SCATHA_OPT_SCC_H_
