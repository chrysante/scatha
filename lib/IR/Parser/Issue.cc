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
        [&](SemanticIssue const& issue) {
            auto reasonStr = UTL_SERIALIZE_ENUM(issue.reason(), {
                { SemanticIssue::TypeMismatch,              "Type mismatch" },
                { SemanticIssue::InvalidType,               "Invalid type" },
                { SemanticIssue::InvalidEntity,             "Invalid entity" },
                { SemanticIssue::UseOfUndeclaredIdentifier, "Use of undeclared identifier" },
                { SemanticIssue::Redeclaration,             "Redeclaration" },
                { SemanticIssue::UnexpectedID,              "Unexpected ID" },
                { SemanticIssue::ExpectedType,              "Expected type" },
            });
            auto sl = issue.sourceLocation();
            str << "Semantic issue: " << sl << ": " << reasonStr << "\n";
        },
    }, issue); // clang-format on
}

void ir::print(ParseIssue const& issue) { ir::print(issue, std::cout); }
