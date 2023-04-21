// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ASSEMBLY_ASSEMBLER_H_
#define SCATHA_ASSEMBLY_ASSEMBLER_H_

#include <string>

#include <utl/hashtable.hpp>
#include <utl/vector.hpp>

#include <scatha/Common/Base.h>

namespace scatha::Asm {

class AssemblyStream;

struct AssemblerResult {
    utl::vector<u8> program;
    utl::hashmap<std::string, size_t> symbolTable;
};

[[nodiscard]] SCATHA_API AssemblerResult
    assemble(AssemblyStream const& assemblyStream);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLER_H_
