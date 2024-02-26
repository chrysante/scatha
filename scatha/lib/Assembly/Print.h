#ifndef SCATHA_ASSEMBLY_PRINT_H_
#define SCATHA_ASSEMBLY_PRINT_H_

#include <iosfwd>

#include "Assembly/Common.h"
#include "Common/Base.h"

namespace scatha::Asm {

class AssemblyStream;
class Block;

SCATHA_API void print(Block const& block);
SCATHA_API void print(Block const& block, std::ostream& ostream);

SCATHA_API void print(Instruction const& inst);
SCATHA_API void print(Instruction const& inst, std::ostream& ostream);

SCATHA_API std::ostream& operator<<(std::ostream& ostream, Instruction const&);

SCATHA_API void print(Value const& value);
SCATHA_API void print(Value const& value, std::ostream& ostream);

SCATHA_API std::ostream& operator<<(std::ostream& ostream, Value const&);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_PRINT_H_
