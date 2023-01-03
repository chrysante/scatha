#ifndef SCATHA_ASTCODEGEN_CODEGEN_H_
#define SCATHA_ASTCODEGEN_CODEGEN_H_

#include "AST/Base.h"
#include "Basic/Basic.h"

namespace scatha::sema {

class SymbolTable;

} // namespace scatha::sema

namespace scatha::ir {

class Module;
class Context;

} // namespace scatha::ir

namespace scatha::ast {

SCATHA(API)
ir::Module codegen(AbstractSyntaxTree const& ast, sema::SymbolTable const& symbolTable, ir::Context& context);

} // namespace scatha::ast

#endif // SCATHA_ASTCODEGEN_CODEGEN_H_
