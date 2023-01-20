// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY_ASSEMBLER_H_
#define SCATHA_ASSEMBLY_ASSEMBLER_H_

#include <string>

#include <scatha/Basic/Basic.h>

namespace svm {

class Program;

} // namespace svm

namespace scatha::Asm {

class AssemblyStream;

struct AssemblerOptions {
    std::string startFunction;
};

[[nodiscard]] SCATHA(API) svm::Program assemble(AssemblyStream const& assemblyStream, AssemblerOptions options = {});

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLER_H_
