#ifndef SCATHA_OPT_COMMON_H_
#define SCATHA_OPT_COMMON_H_

#include "Basic/Basic.h"
#include "IR/Common.h"

namespace scatha::opt {

SCATHA(TEST_API) bool preceeds(ir::Instruction const* a, ir::Instruction const* b);

SCATHA(TEST_API) void replaceValue(ir::Value* oldValue, ir::Value* newValue);

} // namespace scatha::opt

#endif // SCATHA_OPT_COMMON_H_
