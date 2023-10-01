#ifndef SCATHA_IRGEN_SYNTHETICFUNCTION_H_
#define SCATHA_IRGEN_SYNTHETICFUNCTION_H_

#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// Generates IR for the compiler generated function \p semaFn
void generateSyntheticFunction(sema::Function const* semaFn,
                               ir::Function* irFn,
                               ir::Context& ctx);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_SYNTHETICFUNCTION_H_
