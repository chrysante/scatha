#ifndef SCATHA_CODEGEN_ISELTEST_H_
#define SCATHA_CODEGEN_ISELTEST_H_

#include "Common/Base.h"
#include "IR/Fwd.h"

namespace scatha::cg {

SCATHA_API void isel(ir::Function const& function);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_ISELTEST_H_
