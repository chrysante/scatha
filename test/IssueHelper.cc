#include "test/IssueHelper.h"

#include "Lexer/Lexer.h"
#include "test/Sema/SimpleAnalzyer.h"

using namespace scatha;

test::IssueHelper<lex::LexicalIssue> test::getLexicalIssues(std::string_view text) {
    issue::LexicalIssueHandler iss;
    (void)lex::lex(text, iss);
    return { sema::SymbolTable(), std::move(iss) };
}

test::IssueHelper<sema::SemanticIssue> test::getSemaIssues(std::string_view text) {
    auto [ast, sym, iss] = produceDecoratedASTAndSymTable(text);
    return { std::move(sym), std::move(iss) };
    
}

