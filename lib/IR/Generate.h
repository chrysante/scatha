#ifndef SCATHA_IR_GENERATE_H_
#define SCATHA_IR_GENERATE_H_

#include "Basic/Basic.h"
#include "IR/Program.h"
#include "IR/SymbolTable.h"

namespace scatha::sema {

class SymbolTable;

} // namespace scatha::sema

namespace scatha::ast {

class AbstractSyntaxTree;

} // namespace scatha::ast

namespace scatha::ir {

struct GeneratorResult {
    Program program;
    SymbolTable symbolTable;
};

SCATHA(API) GeneratorResult generate(ast::AbstractSyntaxTree const& ast, sema::SymbolTable const& symbolTable);

} // namespace scatha::ir

#endif // SCATHA_IR_GENERATE_H_

