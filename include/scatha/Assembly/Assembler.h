#ifndef SCATHA_ASSEMBLY_ASSEMBLER_H_
#define SCATHA_ASSEMBLY_ASSEMBLER_H_

#include <string>
#include <unordered_map>
#include <vector>

#include <scatha/Common/Base.h>

namespace scatha::Asm {

class AssemblyStream;

struct AssemblerResult {
    std::vector<uint8_t> program;
    std::unordered_map<std::string, size_t> symbolTable;
};

SCATHA_API AssemblerResult assemble(AssemblyStream const& assemblyStream);

SCATHA_API std::string generateDebugSymbols(
    AssemblyStream const& assemblyStream);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLER_H_
