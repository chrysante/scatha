// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY_ASSEMBLER_H_
#define SCATHA_ASSEMBLY_ASSEMBLER_H_

#include <string>

#include <utl/vector.hpp>

#include <scatha/Basic/Basic.h>

namespace scatha::Asm {

class AssemblyStream;

struct AssemblerOptions {
    std::string startFunction;
};

[[nodiscard]] SCATHA(API) utl::vector<u8> assemble(AssemblyStream const& assemblyStream, AssemblerOptions options = {});

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLER_H_
