#include "IR/Parser/IRIssue.h"

#include <iostream>
#include <sstream>

#include <utl/utility.hpp>

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
            auto reasonStr = UTL_SERIALIZE_ENUM(issue.reason(), {
                { SemanticIssue::TypeMismatch,              "Type mismatch" },
                { SemanticIssue::InvalidType,               "Invalid type" },
                { SemanticIssue::InvalidFFIType,            
                    "Invalid type for foreign function interface" },
                { SemanticIssue::InvalidEntity,             "Invalid entity" },
                { SemanticIssue::UseOfUndeclaredIdentifier, 
                    "Use of undeclared identifier" },
                { SemanticIssue::Redeclaration,             "Redeclaration" },
                { SemanticIssue::UnexpectedID,              "Unexpected ID" },
                { SemanticIssue::ExpectedType,              "Expected type" },
                { SemanticIssue::ExpectedConstantValue,     
                    "Expected value constant" },
                
            });
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
