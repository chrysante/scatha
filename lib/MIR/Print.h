#ifndef SCATHA_MIR_PRINT_H_
#define SCATHA_MIR_PRINT_H_

#include <iosfwd>

#include "Common/Base.h"
#include "MIR/Fwd.h"

namespace scatha::mir {

/// Print the module \p mod to `std::cout`
SCTEST_API void print(mir::Module const& mod);

/// Print the module \p mod to \p ostream
SCTEST_API void print(mir::Module const& mod, std::ostream& ostream);

/// Print the function \p F to `std::cout`
SCTEST_API void print(mir::Function const& F);

/// Print the function \p F to \p ostream
SCTEST_API void print(mir::Function const& F, std::ostream& ostream);

} // namespace scatha::mir

#endif // SCATHA_MIR_PRINT_H_
