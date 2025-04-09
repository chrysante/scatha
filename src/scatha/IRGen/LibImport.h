#ifndef SCATHA_IRGEN_LIBIMPORT_H_
#define SCATHA_IRGEN_LIBIMPORT_H_

#include "IRGen/LoweringContext.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

struct LoweringContext;

///
void importLibraries(sema::SymbolTable const& symbolTable,
                     LoweringContext& lctx);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_LIBIMPORT_H_
