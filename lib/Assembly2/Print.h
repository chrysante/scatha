#ifndef SCATHA_ASSEMBLY2_PRINT_H_
#define SCATHA_ASSEMBLY2_PRINT_H_

#include <iosfwd>

#include "Assembly2/Common.h"
#include "Basic/Basic.h"

namespace scatha::asm2 {
	
class AssemblyStream;

SCATHA(API) void print(AssemblyStream const& assemblyStream);
SCATHA(API) void print(AssemblyStream const& assemblyStream, std::ostream& ostream);

SCATHA(API) std::ostream& operator<<(std::ostream& ostream, Element const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, MoveInst const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, ArithmeticInst const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, JumpInst const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, CallInst const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, ReturnInst const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, CompareInst const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, SetInst const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, StoreRegAddress const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, Label const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, RegisterIndex const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, MemoryAddress const&);
SCATHA(API) std::ostream& operator<<(std::ostream& ostream, Value const&);

} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_PRINT_H_

