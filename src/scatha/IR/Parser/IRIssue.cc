#include "IR/Parser/IRIssue.h"

#include <iostream>
#include <sstream>

#include <utl/overload.hpp>

using namespace scatha;
using namespace ir;

static std::ostream& operator<<(std::ostream& str, SourceLocation sl) {
    ++sl.line;
    ++sl.column;
    return str << "L:" << sl.line << " C:" << sl.column;
}

void ir::print(ParseIssue const& issue, std::ostream& str) {
    // clang-format off
    std::visit(utl::overload{
        [&](LexicalIssue const& issue) {
            auto sl = issue.sourceLocation();
            str << "Lexical issue: " << sl << "\n";
        },
        [&](SyntaxIssue const& issue) {
            auto sl = issue.sourceLocation();
            str << "Syntax issue: " << sl << "\n";
        },
        [&](SemanticIssue const& issue) {
            auto reasonStr = [&] {
                switch(issue.reason()) {
                case SemanticIssue::TypeMismatch:
                    return "Type mismatch";
                case SemanticIssue::InvalidType:
                    return "Invalid type";
                case SemanticIssue::InvalidFFIType:
                    return "Invalid type for foreign function interface";
                case SemanticIssue::InvalidEntity:
                    return "Invalid entity";
                case SemanticIssue::UseOfUndeclaredIdentifier:
                    return "Use of undeclared identifier";
                case SemanticIssue::Redeclaration:
                    return "Redeclaration";
                case SemanticIssue::UnexpectedID:
                    return "Unexpected ID";
                case SemanticIssue::ExpectedType:
                    return "Expected type";
                case SemanticIssue::ExpectedConstantValue:
                    return "Expected value constant";
               }
            }();
            auto sl = issue.sourceLocation();
            str << "Semantic issue: " << sl << ": " << reasonStr << "\n";
        },
    }, issue); // clang-format on
}

void ir::print(ParseIssue const& issue) { ir::print(issue, std::cout); }

std::string ir::toString(ParseIssue const& issue) {
    std::stringstream sstr;
    print(issue, sstr);
    return std::move(sstr).str();
}
