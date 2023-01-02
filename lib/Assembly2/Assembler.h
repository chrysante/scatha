#ifndef SCATHA_CODEGEN_ASSEMBLER_H_
#define SCATHA_CODEGEN_ASSEMBLER_H_

#include "Basic/Basic.h"

namespace scatha::vm {

class Program;

} // namespace scatha::vm

namespace scatha::asm2 {

class AssemblyStream;

struct AssemblerOptions {
    u64 mainID = u64(-1);
};

SCATHA(API) vm::Program assemble(AssemblyStream const& assemblyStream, AssemblerOptions options = {});

} // namespace scatha::asm2

#endif // SCATHA_CODEGEN_ASSEMBLER_H_
