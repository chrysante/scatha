#ifndef SCATHA_CODEGEN_PASSES_H_
#define SCATHA_CODEGEN_PASSES_H_

#include "Common/Base.h"
#include "MIR/Fwd.h"

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::Asm {

class AssemblyStream;

} // namespace scatha::Asm

namespace scatha::cg {

/// Lower IR module \p mod to MIR module
SCTEST_API mir::Module lowerToMIR(mir::Context& ctx, ir::Module const& mod);

/// Computes the live-in and live-out sets of function \p F
/// \pre Requires \p F to be in SSA form
SCTEST_API void computeLiveSets(mir::Context& ctx, mir::Function& F);

/// Eliminate dead instructions in function \p F
/// Not as powerful as DCE of IR, as it won't catch dead cycles,
/// but it should suffice here as DCE has already run on the IR
///
/// \Returns `true` if any changes have been made to \p F
///
/// \pre Requires \p F to be in SSA form
SCTEST_API bool deadCodeElim(mir::Context& ctx, mir::Function& F);

/// Convert function \p F out of SSA form
///
/// Converts all `SSARegister`'s to `VirtualRegister`'s and replaces phi nodes
/// with copy instructions, inserts necessary copies for call and return
/// instructions and if possible replaces tail calls by jump intructions
///
/// \pre Requires \p F to be in SSA form
SCTEST_API void destroySSA(mir::Context& ctx, mir::Function& F);

/// Convert registers of function \p F to hardware registers. Redundant copy
/// instructions will be elided.
///
/// \pre Requires \p F to be in virtual register form
SCTEST_API void allocateRegisters(mir::Context& ctx, mir::Function& F);

/// Reorder the basic blocks of function \p F to elide terminating jump
/// instructions
SCTEST_API void elideJumps(mir::Context& ctx, mir::Function& F);

/// Lower MIR module \p mod to assembly
SCATHA_API Asm::AssemblyStream lowerToASM(mir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_PASSES_H_
