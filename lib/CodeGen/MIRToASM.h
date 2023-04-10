// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_CODEGEN_MIRTOASM_H_
#define SCATHA_CODEGEN_MIRTOASM_H_

#include <scatha/Basic/Basic.h>

namespace scatha::Asm {

class AssemblyStream;

} // namespace scatha::Asm

namespace scatha::mir {

class Module;

} // namespace scatha::mir

namespace scatha::cg {

[[nodiscard]] SCATHA_API Asm::AssemblyStream lowerToASM(mir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_MIRTOASM_H_
