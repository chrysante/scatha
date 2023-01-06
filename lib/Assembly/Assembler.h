// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY2_ASSEMBLER_H_
#define SCATHA_ASSEMBLY2_ASSEMBLER_H_

#include <string>

#include <scatha/Basic/Basic.h>

namespace scatha::vm {

class Program;
 
} // namespace scatha::vm

namespace scatha::Asm {

class AssemblyStream;

struct AssemblerOptions {
    std::string startFunction;
};

[[nodiscard]] SCATHA(API) vm::Program assemble(AssemblyStream const& assemblyStream, AssemblerOptions options = {});

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY2_ASSEMBLER_H_
