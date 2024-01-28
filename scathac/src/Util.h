#ifndef SCATHAC_UTIL_H_
#define SCATHAC_UTIL_H_

#include <optional>
#include <span>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/Common/UniquePtr.h>
#include <scatha/IR/Fwd.h>
#include <scatha/IRGen/IRGen.h>
#include <scatha/Sema/AnalysisResult.h>
#include <scatha/Sema/Fwd.h>
#include <scatha/Sema/SymbolTable.h>
#include <utl/streammanip.hpp>

#include "Options.h"

namespace scatha::Asm {

class AssemblyStream;
class LinkerError;

} // namespace scatha::Asm

namespace scatha {

/// Result structure for `parseScatha()`
struct ScathaData {
    UniquePtr<ast::ASTNode> ast;
    sema::SymbolTable sym;
    sema::AnalysisResult analysisResult;
};

/// Parses input files into an AST
std::optional<ScathaData> parseScatha(
    std::span<SourceFile const> sourceFiles,
    std::span<std::filesystem::path const> libSearchPaths);

/// \overload that opens the source files from \p options
std::optional<ScathaData> parseScatha(OptionsBase const& options);

/// Parses single input file into an IR module
std::pair<ir::Context, ir::Module> parseIR(OptionsBase const& options);

std::pair<ir::Context, ir::Module> genIR(
    ast::ASTNode const& ast, sema::SymbolTable const& symbolTable,
    sema::AnalysisResult const& analysisResult, irgen::Config config);

/// Apply the specfied optimization level or pipeline to \p mod
void optimize(ir::Context& ctx, ir::Module& mod, OptionsBase const& options);

/// Print errors of linker phase
void printLinkerError(Asm::LinkerError const& error);

///
extern utl::vstreammanip<> const Warning;

///
extern utl::vstreammanip<> const Error;

} // namespace scatha

#endif // SCATHAC_UTIL_H_
