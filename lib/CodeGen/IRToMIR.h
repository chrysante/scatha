#ifndef SCATHA_CODEGEN_IRTOMIR_H_
#define SCATHA_CODEGEN_IRTOMIR_H_

#include "Basic/Basic.h"
#include "IR/Common.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

SCATHA_TESTAPI mir::Module lowerToMIR(ir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_IRTOMIR_H_
