#ifndef SCATHA_AST_LOWERTOIR_H_
#define SCATHA_AST_LOWERTOIR_H_

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

namespace scatha::ast {

class ASTNode;

[[nodiscard]] SCATHA_API std::pair<ir::Context, ir::Module> lowerToIR(
    ASTNode const& ast,
    sema::SymbolTable const& symbolTable,
    sema::AnalysisResult const&);

} // namespace scatha::ast

#endif // SCATHA_AST_LOWERTOIR_H_
