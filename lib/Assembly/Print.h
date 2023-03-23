#ifndef SCATHA_ASSEMBLY_PRINT_H_
#define SCATHA_ASSEMBLY_PRINT_H_

#include <iosfwd>

#include "Assembly/Common.h"
#include "Basic/Basic.h"

namespace scatha::Asm {

class AssemblyStream;
class Block;

SCATHA_API void print(AssemblyStream const& assemblyStream);
SCATHA_API void print(AssemblyStream const& assemblyStream,
                      std::ostream& ostream);

SCATHA_API void print(Block const& block);
SCATHA_API void print(Block const& block, std::ostream& ostream);

SCATHA_API std::ostream& operator<<(std::ostream& ostream, Instruction const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, MoveInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, CMoveInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    UnaryArithmeticInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    ArithmeticInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, JumpInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, CallInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, CallExtInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, ReturnInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    TerminateInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, CompareInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, TestInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, SetInst const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, AllocaInst const&);

SCATHA_API std::ostream& operator<<(std::ostream& ostream, Value const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    RegisterIndex const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream,
                                    MemoryAddress const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, Value8 const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, Value16 const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, Value32 const&);
SCATHA_API std::ostream& operator<<(std::ostream& ostream, Value64 const&);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_PRINT_H_
