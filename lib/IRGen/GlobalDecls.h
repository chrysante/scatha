#ifndef SCATHA_IRGEN_GLOBALDECLS_H_
#define SCATHA_IRGEN_GLOBALDECLS_H_

#include <span>

#include "AST/Fwd.h"
#include "Common/UniquePtr.h"
#include "IR/Fwd.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

class FunctionMap;
class TypeMap;

/// Translates \p semaType to an IR structure type
ir::StructType* generateType(sema::StructType const* semaType,
                             ir::Context& ctx,
                             ir::Module& mod,
                             TypeMap& typeMap);

/// Translates the function declaration \p semaFn to an IR function.
/// \Note This does not generate code
ir::Callable* declareFunction(sema::Function const* semaFn,
                              ir::Context& ctx,
                              ir::Module& mod,
                              TypeMap const& typeMap,
                              FunctionMap& functionMap);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_GLOBALDECLS_H_