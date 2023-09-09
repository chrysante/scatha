#ifndef SCATHA_IRGEN_IRGEN_H_
#define SCATHA_IRGEN_IRGEN_H_

#include <utility>

#include <scatha/Common/Base.h>

namespace scatha::sema {

struct AnalysisResult;
class SymbolTable;

} // namespace scatha::sema

namespace scatha::ir {

class Module;
class Context;

} // namespace scatha::ir

/// For now this is in `namespace ast`, we will change this later
namespace scatha::ast {

class ASTNode;

[[nodiscard]] SCATHA_API std::pair<ir::Context, ir::Module> generateIR(
    ASTNode const& ast,
    sema::SymbolTable const& symbolTable,
    sema::AnalysisResult const&);

} // namespace scatha::ast

#endif // SCATHA_IRGEN_IRGEN_H_
