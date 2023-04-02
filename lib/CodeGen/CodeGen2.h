#ifndef SCATHA_CODEGEN_CODEGEN2_H_
#define SCATHA_CODEGEN_CODEGEN2_H_

#include "Basic/Basic.h"

namespace scatha::Asm {

class AssemblyStream;

} // namespace scatha::Asm

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::cg {

SCATHA_API Asm::AssemblyStream codegen2(ir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_CODEGEN2_H_
