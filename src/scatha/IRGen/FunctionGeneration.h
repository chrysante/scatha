#ifndef SCATHA_IRGEN_FUNCTIONGENERATION_H_
#define SCATHA_IRGEN_FUNCTIONGENERATION_H_

#include "IR/Fwd.h"
#include "IRGen/LoweringContext.h"
#include "Sema/Fwd.h"

namespace scatha::irgen {

/// Generates IR for the function \p semaFn
/// Dispatches to `generateAstFunction()` or `generateSynthFunction()`
void generateFunction(sema::Function const* semaFn, ir::Function& irFn,
                      LoweringContext& loweringContext);

/// Lowers the user defined function \p funcDecl from AST representation into
/// IR. The result is written into \p irFn
/// \Returns a list of functions called by the lowered function that are not yet
/// defined. This mechanism is part of lazy function generation.
void generateNativeFunction(sema::Function const* semaFn, ir::Function& irFn,
                            LoweringContext& loweringContext);

/// Generates IR for the compiler generated function \p semaFn
void generateSynthFunction(sema::Function const* semaFn, ir::Function& irFn,
                           LoweringContext& loweringContext);

/// Generates IR for the variable \p semaVar
GlobalVarMetadata generateGlobalVariable(sema::Variable const& semaVar,
                                         LoweringContext& lctx);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_FUNCTIONGENERATION_H_
