#include "IR/Generate.h"

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "AST/AST.h"
#include "Sema/SymbolTable.h"
#include "IR/Module.h"

using namespace scatha;
using namespace ir;

namespace {

struct GenCtx {
    explicit GenCtx(Module* result, sema::SymbolTable const& sym): result(result), symTable(sym) {}
    
    void dispatch(ast::AbstractSyntaxTree const& node);
    
    void generate(ast::TranslationUnit const&);
    
    Module* result;
    sema::SymbolTable const& symTable;
};

} // namespace

Module* ir::generate(ast::AbstractSyntaxTree const& ast, sema::SymbolTable const& symbolTable) {
    Module* result = new Module();
    GenCtx ctx(result, symbolTable);
    ctx.dispatch(ast);
    return result;
}

void GenCtx::dispatch(ast::AbstractSyntaxTree const& node) {
    
}
