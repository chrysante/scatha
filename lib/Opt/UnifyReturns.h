#ifndef SCATHA_OPT_UNIFYRETURNS_H_
#define SCATHA_OPT_UNIFYRETURNS_H_

#include "IR/Common.h"

namespace scatha::opt {

SCATHA_TESTAPI bool unifyReturns(ir::Context& context, ir::Function& function);

SCATHA_TESTAPI bool splitReturns(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_UNIFYRETURNS_H_
