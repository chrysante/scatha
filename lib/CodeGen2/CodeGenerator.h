#ifndef SCATHA_CODEGEN2_CODEGENERATOR_H_
#define SCATHA_CODEGEN2_CODEGENERATOR_H_

#include "Basic/Basic.h"

namespace scatha::assembly {

class AssemblyStream;

} // namespace scatha::assembly

namespace scatha::ir {

class Module;

} // namespace scatha::ir

namespace scatha::cg2 {

SCATHA(API) assembly::AssemblyStream codegen(ir::Module const& mod);
	
} // namespace scatha::cg2

#endif // SCATHA_CODEGEN2_CODEGENERATOR_H_

