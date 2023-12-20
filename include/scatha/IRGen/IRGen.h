#ifndef SCATHA_IRGEN_IRGEN_H_
#define SCATHA_IRGEN_IRGEN_H_

#include <span>
#include <utility>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/Common/SourceFile.h>
#include <scatha/IR/Fwd.h>
#include <scatha/Sema/Fwd.h>

namespace scatha::irgen {

/// Config options used by `generateIR()`
struct SCATHA_API Config {
    /// The source files. Only used to generate debug symbols
    std::span<SourceFile const> sourceFiles;

    /// Set this to `true` to associate source locations with IR instructions
    bool generateDebugSymbols = false;
};

/// Lower the front-end representation of the program to IR
SCATHA_API void generateIR(ir::Context& ctx,
                           ir::Module& mod,
                           ast::ASTNode const& ast,
                           sema::SymbolTable const& symbolTable,
                           sema::AnalysisResult const& analysisResult,
                           Config config);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_IRGEN_H_
