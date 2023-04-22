#include "CompileToIR.h"

#include "AST/AST.h"
#include "AST/LowerToIR.h"
#include "IR/Context.h"
#include "IR/Module.h"
#include "Issue/IssueHandler.h"
#include "Issue/IssueHandler2.h"
#include "Parser/Lexer.h"
#include "Parser/Parser.h"
#include "Sema/Analyze.h"

using namespace scatha;

ir::Module test::compileToIR(std::string_view text) {
    IssueHandler issues;
    auto tokens = parse::lex(text, issues);
    if (!issues.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    auto ast = parse::parse(tokens, issues);
    if (!issues.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    sema::SymbolTable sym;
    issue::SemaIssueHandler semaIss;
    sema::analyze(*ast, sym, semaIss);
    if (!semaIss.empty()) {
        throw std::runtime_error("Compilation failed");
    }
    ir::Context ctx;
    return ast::lowerToIR(*ast, sym, ctx);
}
