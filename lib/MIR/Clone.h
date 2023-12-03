#ifndef SCATHA_MIR_CLONE_H_
#define SCATHA_MIR_CLONE_H_

#include <concepts>

#include "Common/UniquePtr.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

/// Implemention
UniquePtr<Instruction> cloneImpl(Instruction& inst);

/// \Returns a clone of \p inst
/// Because the returned instruction uses the same operands as \p inst the
/// argument is not const
template <std::derived_from<Instruction> Inst>
UniquePtr<Inst> clone(Inst& inst) {
    return uniquePtrCast<Inst>(cloneImpl(inst));
}

} // namespace scatha::mir

#endif // SCATHA_MIR_CLONE_H_
