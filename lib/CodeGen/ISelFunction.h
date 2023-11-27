#ifndef SCATHA_CODEGEN_ISELFUNCTION_H_
#define SCATHA_CODEGEN_ISELFUNCTION_H_

#include "Common/Base.h"
#include "IR/Fwd.h"
#include "MIR/Fwd.h"

namespace scatha::cg {

class ValueMap;

/// Lowers the IR function \p irFn to MIR representation. Result is written into
/// the empty MIR function \p mirFn
/// \p globalMap is a map of all global declarations
SCATHA_API void iselFunction(ir::Function const& irFn,
                             mir::Function& mirFn,
                             ValueMap const& globalMap);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_ISELFUNCTION_H_
