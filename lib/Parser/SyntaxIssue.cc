#include "Parser/SyntaxIssue.h"

#include <ostream>

#include "Issue/IssueHandler.h"

using namespace scatha;
using namespace parse;

void ExpectedIdentifier::format(std::ostream& str) const {
    str << "Expected identifier";
}

void ExpectedDeclarator::format(std::ostream& str) const {
    str << "Expected declarator";
}

void UnexpectedDeclarator::format(std::ostream& str) const {
    str << "Unexpected declarator";
}

void ExpectedDelimiter::format(std::ostream& str) const {
    str << "Expected delimiter";
}

void ExpectedExpression::format(std::ostream& str) const {
    str << "Expected expression";
}

void ExpectedClosingBracket::format(std::ostream& str) const {
    str << "Expected closing bracket";
}

void UnexpectedClosingBracket::format(std::ostream& str) const {
    str << "Unexpected closing bracket";
}

void UnqualifiedID::format(std::ostream& str) const { str << "Unqualified ID"; }
