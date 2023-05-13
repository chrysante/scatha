#include "CompileToIR.h"

#include "AST/AST.h"
#include "AST/LowerToIR.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Issue/IssueHandler.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"

using namespace scatha;

ir::Module test::compileToIR(std::string_view text) {
    IssueHandler issues;
    auto ast = parse::parse(text, issues);
    if (!issues.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    sema::SymbolTable sym;
    sema::analyze(*ast, sym, issues);
    if (!issues.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    ir::Context ctx;
    return ast::lowerToIR(*ast, sym, ctx);
}
