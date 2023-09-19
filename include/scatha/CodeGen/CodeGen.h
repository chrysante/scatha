#ifndef SCATHA_CODEGEN_CODEGEN_H_
#define SCATHA_CODEGEN_CODEGEN_H_

#include <scatha/CodeGen/Logger.h>
#include <scatha/Common/Base.h>

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::Asm {

class AssemblyStream;

} // namespace scatha::Asm

namespace scatha::cg {

SCATHA_API Asm::AssemblyStream codegen(ir::Module const& mod);

SCATHA_API Asm::AssemblyStream codegen(ir::Module const& mod,
                                       cg::Logger& logger);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_CODEGEN_H_
