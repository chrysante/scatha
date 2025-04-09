#ifndef SCATHA_ASSEMBLY_ASSEMBLER_H_
#define SCATHA_ASSEMBLY_ASSEMBLER_H_

#include <filesystem>
#include <span>
#include <string>
#include <unordered_map>
#include <vector>

#include <scatha/Assembly/Options.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/DebugInfo.h>
#include <scatha/Common/Expected.h>
#include <scatha/Common/FFI.h>

namespace scatha::Asm {

class AssemblyStream;

struct AssemblerResult {
    /// The assembled program. Must be linked before execution
    std::vector<uint8_t> program;

    /// Symbol table of exported functions
    std::unordered_map<std::string, size_t> symbolTable;

    /// Symbols that still need to be linked. These are written as mangled names
    /// in the binary and need to be replaced by the linker
    std::vector<std::pair<size_t, ForeignFunctionInterface>> unresolvedSymbols;

    ///
    dbi::DebugInfoMap debugInfo;
};

/// Argument structure for `assemble()`
struct AssemblerOptions {
    bool generateDebugInfo = false;
};

/// Create binary executable file from the assembly stream \p program
/// References to functions in other libraries will not be resolved and written
/// into `unresolvedSymbols`
SCATHA_API AssemblerResult assemble(AssemblyStream const& program,
                                    AssemblerOptions const& options = {});

/// Error type returned by `link()`
struct SCATHA_API LinkerError {
    /// List of unresolved symbol references
    std::vector<std::string> missingSymbols;
};

/// Resolves unresolved symbols from other libraries
[[nodiscard]] SCATHA_API Expected<void, LinkerError> link(
    LinkerOptions options, std::vector<uint8_t>& program,
    std::span<ForeignLibraryDecl const> foreignLibraries,
    std::span<std::pair<size_t, ForeignFunctionInterface> const>
        unresolvedSymbols);

} // namespace scatha::Asm

#endif // SCATHA_ASSEMBLY_ASSEMBLER_H_
