#ifndef SCATHA_IR_IRPRINT_H_
#define SCATHA_IR_IRPRINT_H_

#include <iosfwd>

#include "Basic/Basic.h"
#include "IR/Fwd.h"

namespace scatha::ir {

SCATHA(API) void print(Program const& program, SymbolTable const& symbolTable);

SCATHA(API) void print(Program const& program, SymbolTable const& symbolTable, std::ostream& ostream);
	
} // namespace scatha::ir 

#endif // SCATHA_IR_IRPRINT_H_

