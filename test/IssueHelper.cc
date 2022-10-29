#include "test/IssueHelper.h"

#include "Lexer/Lexer.h"
#include "test/Sema/SimpleAnalzyer.h"


scatha::test::IssueHelper scatha::test::getSemaIssues(std::string_view text) {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(text);
    return { std::move(sym), std::move(iss) };
    
}

scatha::test::IssueHelper scatha::test::getLexicalIssues(std::string_view text) {
    issue::IssueHandler iss;
    lex::lex(text, iss);
    return { sema::SymbolTable(), std::move(iss) };
}
