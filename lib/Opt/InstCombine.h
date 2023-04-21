#ifndef SCATHA_OPT_INSTCOMBINE_H_
#define SCATHA_OPT_INSTCOMBINE_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::opt {

SCATHA_TESTAPI bool instCombine(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_INSTCOMBINE_H_
