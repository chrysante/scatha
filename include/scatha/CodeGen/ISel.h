#ifndef SCATHA_CODEGEN_ISEL_H_
#define SCATHA_CODEGEN_ISEL_H_

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>
#include <scatha/MIR/Fwd.h>

namespace scatha::cg {

/// Lowers the IR module \p mod to MIR representation
SCATHA_API mir::Module isel(ir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_ISEL_H_
