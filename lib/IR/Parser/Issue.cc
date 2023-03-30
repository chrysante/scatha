#include "IR/Parser/Issue.h"

#include <iostream>

using namespace scatha;
using namespace ir;

static std::ostream& operator<<(std::ostream& str, SourceLocation sl) {
    ++sl.line;
    ++sl.column;
    return str << "L:" << sl.line << " C:" << sl.column;
}

void ir::print(ParseIssue const& issue, std::ostream& str) {
    // clang-format off
    utl::visit(utl::overload{
        [&](LexicalIssue const& issue) {
            auto sl = issue.sourceLocation();
            str << "Lexical issue: " << sl << "\n";
        },
        [&](SyntaxIssue const& issue) {
            auto sl = issue.sourceLocation();
            str << "Syntax issue: " << sl << "\n";
        },
        [&](SemanticIssue const& issue) { str << "Semantic issue.\n"; },
    }, issue); // clang-format on
}

void ir::print(ParseIssue const& issue) { ir::print(issue, std::cout); }
