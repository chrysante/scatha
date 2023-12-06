#ifndef SCATHA_CODEGEN_ISEL_H_
#define SCATHA_CODEGEN_ISEL_H_

#include "Common/Base.h"
#include "IR/Fwd.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

class ValueMap;
class SelectionDAG;

/// Perform instruction selection on the selection dag \p DAG
SCATHA_API void isel(SelectionDAG& DAG,
                     mir::Context& ctx,
                     mir::Module& mod,
                     mir::Function& mirFn,
                     ValueMap& valueMap);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_ISEL_H_
