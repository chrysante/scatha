// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_CODEGEN_IR2BYTECODE_CODEGENERATOR_H_
#define SCATHA_CODEGEN_IR2BYTECODE_CODEGENERATOR_H_

#include <scatha/Basic/Basic.h>

namespace scatha::Asm {

class AssemblyStream;

} // namespace scatha::Asm

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::cg {

SCATHA(API) Asm::AssemblyStream codegen(ir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN_IR2BYTECODE_CODEGENERATOR_H_
