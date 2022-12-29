#ifndef SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_
#define SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_

#include <iosfwd>
#include <optional>

#include "Assembly/AssemblyStream.h"
#include "VM/OpCode.h"

namespace scatha::sema {

class SymbolTable;

} // namespace scatha::sema

namespace scatha::assembly {

vm::OpCode mapUnaryInstruction(Instruction);
vm::OpCode mapBinaryInstruction(Instruction, Element const&, Element const&);

void SCATHA(API) print(AssemblyStream const&);
void SCATHA(API) print(AssemblyStream const&, std::ostream&);
void SCATHA(API) print(AssemblyStream const&, sema::SymbolTable const&);
void SCATHA(API) print(AssemblyStream const&, sema::SymbolTable const&, std::ostream&);

} // namespace scatha::assembly

#endif // SCATHA_ASSEMBLY_ASSEMBLYUTIL_H_
