#ifndef SCATHA_CG_DESTROYSSA_H_
#define SCATHA_CG_DESTROYSSA_H_

#include "MIR/Fwd.h"

namespace scatha::cg {

/// Converts all `SSARegister`'s to `VirtualRegister`'s.
/// (And maybe three address instructions to two address instructions)
SCATHA_TESTAPI void destroySSA(mir::Function& F);

} // namespace scatha::cg

#endif // SCATHA_CG_DESTROYSSA_H_
