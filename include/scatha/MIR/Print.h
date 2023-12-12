#ifndef SCATHA_MIR_PRINT_H_
#define SCATHA_MIR_PRINT_H_

#include <iosfwd>

#include <scatha/Common/Base.h>
#include <scatha/MIR/Fwd.h>

namespace scatha::mir {

/// Print the module \p mod to `std::cout`
SCTEST_API void print(mir::Module const& mod);

/// Print the module \p mod to \p ostream
SCTEST_API void print(mir::Module const& mod, std::ostream& ostream);

/// Print the function \p F to `std::cout`
SCTEST_API void print(mir::Function const& F);

/// Print the function \p F to \p ostream
SCTEST_API void print(mir::Function const& F, std::ostream& ostream);

/// Print the instruction \p inst to `std::cout`
SCTEST_API void print(mir::Instruction const& inst);

/// Print the instruction \p inst to \p ostream
SCTEST_API void print(mir::Instruction const& inst, std::ostream& ostream);

/// Print the value \p value to `std::cout`
SCTEST_API void printDecl(mir::Value const& value);

/// Print the value \p value to \p ostream
SCTEST_API void printDecl(mir::Value const& value, std::ostream& ostream);

/// Calls `printDecl()`
std::ostream& operator<<(std::ostream& ostream, mir::Value const& value);

} // namespace scatha::mir

#endif // SCATHA_MIR_PRINT_H_
