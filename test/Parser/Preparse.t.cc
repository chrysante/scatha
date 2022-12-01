#include <Catch/Catch2.hpp>

#include "Lexer/Lexer.h"
#include "Parser/Preparse.h"

using namespace scatha;

TEST_CASE("Preparse", "[parse][preparse]") {
    char const* const text = "((  )";
    auto tokens = [&]{
        issue::LexicalIssueHandler iss;
        return lex::lex(text, iss);
    }();
    
    issue::SyntaxIssueHandler iss;
    parse::preparse(tokens, iss);
    
}
