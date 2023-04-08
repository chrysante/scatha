#ifndef SCATHA_MIR_PRINT_H_
#define SCATHA_MIR_PRINT_H_

#include <iosfwd>

#include "Basic/Basic.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

SCATHA_TESTAPI void print(mir::Module const& mod);

SCATHA_TESTAPI void print(mir::Module const& mod, std::ostream& ostream);

SCATHA_TESTAPI void print(mir::Function const& F);

SCATHA_TESTAPI void print(mir::Function const& F, std::ostream& ostream);

} // namespace scatha::mir

#endif // SCATHA_MIR_PRINT_H_
