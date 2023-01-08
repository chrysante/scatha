// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_OPT_MEM2REG_H_
#define SCATHA_OPT_MEM2REG_H_

#include <scatha/Basic/Basic.h>

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::opt {

SCATHA(API) void mem2Reg(ir::Module& mod);

} // namespace scatha::opt

#endif // SCATHA_OPT_MEM2REG_H_
