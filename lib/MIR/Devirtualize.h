#ifndef SCATHA_MIR_DEVIRTUALIZE_H_
#define SCATHA_MIR_DEVIRTUALIZE_H_

#include "MIR/Fwd.h"

namespace scatha::mir {

SCATHA_TESTAPI bool devirtualize(mir::Function& F);

} // namespace scatha::mir

#endif // SCATHA_MIR_DEVIRTUALIZE_H_
