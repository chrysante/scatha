#ifndef SCATHA_OPT_PASSES_H_
#define SCATHA_OPT_PASSES_H_

#include <functional>

#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>
#include <scatha/Opt/Pass.h>

namespace scatha::opt {

/// # Global passes

/// The inliner
SCATHA_API bool inlineFunctions(ir::Context& ctx, ir::Module& mod);

/// \overload
SCATHA_API bool inlineFunctions(ir::Context& ctx,
                                ir::Module& mod,
                                LocalPass localPass);

/// Eliminate all function that do not get called by any externally visible
/// function
SCATHA_API bool deadFuncElim(ir::Context& ctx, ir::Module& mod);

/// \overload
/// \Note The parameter \p localPass is ignored
SCATHA_API bool deadFuncElim(ir::Context& ctx,
                             ir::Module& mod,
                             LocalPass localPass);

/// Execute \p localPass for each function in the module \p mod
SCATHA_API bool forEach(ir::Context& ctx, ir::Module& mod, LocalPass localPass);

/// # Canonicalization passes

/// The default canonicalization passes
SCATHA_API bool canonicalize(ir::Context& ctx, ir::Function& function);

/// Transform the function to have a single exit block
SCATHA_API bool unifyReturns(ir::Context& context, ir::Function& function);

/// Split the single exit block
SCATHA_API bool splitReturns(ir::Context& context, ir::Function& function);

/// This pass transform while loops into guarded do/while loops
SCATHA_API bool rotateLoops(ir::Context& context, ir::Function& function);

/// # Local passes

/// The default set of local passes for good optimization
SCATHA_API bool defaultPass(ir::Context& ctx, ir::Function& function);

/// Removes critical edges from \p function by inserting empty basic blocks
SCATHA_API bool splitCriticalEdges(ir::Context& ctx, ir::Function& function);

/// Run sparse conditional constant propagation algorithm over \p function
/// Folds constants and eliminates dead code.
/// \returns `true` if \p function was modified in the pass.
SCATHA_API bool propagateConstants(ir::Context& context,
                                   ir::Function& function);

SCATHA_API bool propagateInvariants(ir::Context& context,
                                    ir::Function& function);

/// Eliminate dead code in \p function
/// \Returns `true` if \p function was modified in the pass.
SCATHA_API bool dce(ir::Context& context, ir::Function& function);

/// Perform redundancy elimination by global value numbering
SCATHA_API bool globalValueNumbering(ir::Context& context,
                                     ir::Function& function);

/// Perform many peephole optimizations
SCATHA_API bool instCombine(ir::Context& context, ir::Function& function);

/// Perform memory to register promotion on \p function
/// \Returns `true` if \p function was modified in the pass.
SCATHA_API bool memToReg(ir::Context& context, ir::Function& function);

/// Perform scalar replacement of aggregates on \p function
/// \Returns `true` if \p function was modified in the pass.
SCATHA_API bool sroa(ir::Context& context, ir::Function& function);

SCATHA_API bool sroa2(ir::Context& context, ir::Function& function);

/// Simplify the control flow graph by merging and erasing unneeded blocks
SCATHA_API bool simplifyCFG(ir::Context& ctx, ir::Function& function);

SCATHA_API bool simplifyCFG2(ir::Context& ctx, ir::Function& function);

/// Tail recursion elimination.
///
/// \details
/// This pass replaces tail recursive calls with `goto`'s to the start of the
/// function, thus creating loops.
SCATHA_API bool tailRecElim(ir::Context& context, ir::Function& function);

} // namespace scatha::opt

#endif // SCATHA_OPT_PASSES_H_
