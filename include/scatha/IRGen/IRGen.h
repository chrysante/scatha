#ifndef SCATHA_IRGEN_IRGEN_H_
#define SCATHA_IRGEN_IRGEN_H_

#include <utility>

#include <scatha/AST/Fwd.h>
#include <scatha/Common/Base.h>
#include <scatha/IR/Fwd.h>
#include <scatha/Sema/Fwd.h>

namespace scatha::irgen {

SCATHA_API std::pair<ir::Context, ir::Module> generateIR(
    ast::ASTNode const& ast,
    sema::SymbolTable const& symbolTable,
    sema::AnalysisResult const& analysisResult);

} // namespace scatha::irgen

#endif // SCATHA_IRGEN_IRGEN_H_
