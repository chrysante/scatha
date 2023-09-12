#ifndef SCATHA_CODEGEN_UTILITY_H_
#define SCATHA_CODEGEN_UTILITY_H_

#include "MIR/Fwd.h"

namespace scatha::cg {

/// \Returns `true` if the instruction \p inst has side effects and may not be
/// eliminated
bool hasSideEffects(mir::Instruction const* inst);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_UTILITY_H_
