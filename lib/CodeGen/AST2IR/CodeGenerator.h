// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_CODEGEN_AST2IR_CODEGEN_H_
#define SCATHA_CODEGEN_AST2IR_CODEGEN_H_

#include <scatha/Basic/Basic.h>

namespace scatha::sema {

class SymbolTable;

} // namespace scatha::sema

namespace scatha::ir {

class Module;
class Context;

} // namespace scatha::ir

namespace scatha::ast {

class AbstractSyntaxTree;

[[nodiscard]] SCATHA_API ir::Module codegen(
    AbstractSyntaxTree const& ast,
    sema::SymbolTable const& symbolTable,
    ir::Context& context);

} // namespace scatha::ast

#endif // SCATHA_CODEGEN_AST2IR_CODEGEN_H_
