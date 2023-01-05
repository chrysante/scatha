#ifndef SCATHA_CODEGEN2_CODEGENERATOR_H_
#define SCATHA_CODEGEN2_CODEGENERATOR_H_

#include "Basic/Basic.h"

namespace scatha::Asm {

class AssemblyStream;

} // namespace scatha::Asm

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::cg {

SCATHA(API) Asm::AssemblyStream codegen(ir::Module const& mod);

} // namespace scatha::cg

#endif // SCATHA_CODEGEN2_CODEGENERATOR_H_
