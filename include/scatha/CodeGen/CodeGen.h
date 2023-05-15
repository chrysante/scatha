// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_CODEGEN_CODEGEN_H_
#define SCATHA_CODEGEN_CODEGEN_H_

#include <scatha/Common/Base.h>

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::Asm {

class AssemblyStream;

} // namespace scatha::Asm

namespace scatha::cg {

SCATHA_API Asm::AssemblyStream codegen(ir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_CODEGEN_H_
