#ifndef SCATHA_CODEGEN_UTILITY_H_
#define SCATHA_CODEGEN_UTILITY_H_

#include "MIR/Fwd.h"
#include "MIR/LiveInterval.h"

namespace scatha::cg {

/// \Returns `true` if the instruction \p inst has side effects and must not be
/// eliminated
bool hasSideEffects(mir::Instruction const& inst);

/// Computes and assigns the live range for the register \p reg
/// \pre Requires live-in and live-out sets computed
SCATHA_API void computeLiveRange(mir::Function& F, mir::Register& reg);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_UTILITY_H_
