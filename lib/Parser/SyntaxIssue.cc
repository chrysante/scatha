#include "Parser/SyntaxIssue.h"

#include <ostream>

#include <utl/utility.hpp>

#include "Issue/IssueHandler.h"

using namespace scatha;

using enum parse::SyntaxIssue::Reason;

SCATHA(API) std::string_view parse::toString(SyntaxIssue::Reason reason) {
    // clang-format off
    return UTL_SERIALIZE_ENUM(reason, {
        { ExpectedIdentifier,       "Expected Identifier" },
        { ExpectedDeclarator,       "Expected Declarator" },
        { ExpectedSeparator,        "Expected Separator" },
        { ExpectedExpression,       "Expected Expression" },
        { ExpectedClosingBracket,   "Expected Closing Bracket" },
        { UnexpectedClosingBracket, "Unexpected Closing Bracket" },
        { UnqualifiedID,            "Unqualified ID" }
    });
    // clang-format on
}

SCATHA(API) std::ostream& parse::operator<<(std::ostream& str, SyntaxIssue::Reason reason) {
    return str << toString(reason);
}

bool parse::expectIdentifier(issue::SyntaxIssueHandler& iss, Token const& token) {
    if (!token.isIdentifier) {
        iss.push(SyntaxIssue(token, ExpectedIdentifier));
        return false;
    }
    return true;
}

bool parse::expectDeclarator(issue::SyntaxIssueHandler& iss, Token const& token) {
    if (!token.isDeclarator) {
        iss.push(SyntaxIssue(token, ExpectedDeclarator));
        return false;
    }
    return true;
}

bool parse::expectSeparator(issue::SyntaxIssueHandler& iss, Token const& token) {
    if (!token.isSeparator) {
        iss.push(SyntaxIssue(token, ExpectedSeparator));
        return false;
    }
    return true;
}

bool parse::expectID(issue::SyntaxIssueHandler& iss, Token const& token, std::string id) {
    if (token.id != id) {
        iss.push(SyntaxIssue::expectedID(token, std::move(id)));
        return false;
    }
    return true;
}
