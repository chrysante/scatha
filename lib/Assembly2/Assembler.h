#ifndef SCATHA_ASSEMBLY2_ASSEMBLER_H_
#define SCATHA_ASSEMBLY2_ASSEMBLER_H_

#include <string>

#include "Basic/Basic.h"

namespace scatha::vm {

class Program;

} // namespace scatha::vm

namespace scatha::asm2 {

class AssemblyStream;

struct AssemblerOptions {
    std::string startFunction;
};

SCATHA(API) vm::Program assemble(AssemblyStream const& assemblyStream, AssemblerOptions options = {});

} // namespace scatha::asm2

#endif // SCATHA_ASSEMBLY2_ASSEMBLER_H_
