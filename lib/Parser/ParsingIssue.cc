#include "Parser/ParsingIssue.h"

#include <utl/utility.hpp>

#include "Issue/IssueHandler.h"

using namespace scatha;

using enum parse::ParsingIssue::Reason;

SCATHA(API) std::string_view parse::toString(ParsingIssue::Reason reason) {
    return UTL_SERIALIZE_ENUM(reason, {
        { ExpectedIdentifier, "Expected Identifier" },
        { ExpectedDeclarator, "Expected Declarator" },
        { ExpectedSeparator,  "Expected Separator" },
        { ExpectedExpression, "Expected Expression" },
        { ExpectedSpecificID, "Expected SpecificID" },
        { UnqualifiedID,      "Unqualified ID" }
    });
}

SCATHA(API) std::ostream& parse::operator<<(std::ostream& str, ParsingIssue::Reason reason) {
    return str << toString(reason);
}

bool parse::expectIdentifier(issue::ParsingIssueHandler& iss, Token const& token) {
    if (!token.isIdentifier) {
        iss.push(ParsingIssue(token, ExpectedIdentifier));
        return false;
    }
    return true;
}

bool parse::expectDeclarator(issue::ParsingIssueHandler& iss, Token const& token) {
    if (!token.isDeclarator) {
        iss.push(ParsingIssue(token, ExpectedDeclarator));
        return false;
    }
    return true;
}

bool parse::expectSeparator(issue::ParsingIssueHandler& iss, Token const& token) {
    if (!token.isSeparator) {
        iss.push(ParsingIssue(token, ExpectedSeparator));
        return false;
    }
    return true;
}

bool parse::expectID(issue::ParsingIssueHandler& iss, Token const& token, std::string id) {
    if (token.id != id) {
        iss.push(ParsingIssue::expectedID(token, std::move(id)));
        return false;
    }
    return true;
}


