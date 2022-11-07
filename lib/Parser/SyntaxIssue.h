#ifndef SCATHA_PARSER_PARSERERROR_H_
#define SCATHA_PARSER_PARSERERROR_H_

#include <iosfwd>
#include <string>

#include "Basic/Basic.h"
#include "Common/ProgramIssue.h"
#include "Issue/ProgramIssue.h"

namespace scatha::issue {

class SyntaxIssueHandler;

}

namespace scatha::parse {

class SCATHA(API) SyntaxIssue: public issue::ProgramIssueBase {
public:
    enum class Reason {
        ExpectedIdentifier,
        ExpectedDeclarator,
        ExpectedSeparator,
        ExpectedExpression,
        UnqualifiedID,
        _count
    };
    
    explicit SyntaxIssue(Token const& token, Reason reason):
        issue::ProgramIssueBase(token), _reason(reason) {}
    
    explicit SyntaxIssue(SourceLocation location, Reason reason):
        issue::ProgramIssueBase(location), _reason(reason) {}
    
    SyntaxIssue static expectedID(Token const& token, std::string id) {
        return SyntaxIssue(token, std::move(id));
    }
    
    Reason reason() const { return _reason; }
    
    std::string_view expectedID() const {
        SC_EXPECT(_reason == Reason::UnqualifiedID, "Invalid");
        return _expectedID;
    }
    
private:
    SyntaxIssue(Token const& token, std::string id):
        issue::ProgramIssueBase(token), _reason(Reason::UnqualifiedID), _expectedID(std::move(id)) {}
    
private:
    Reason _reason;
    std::string _expectedID;
};

SCATHA(API) std::string_view toString(SyntaxIssue::Reason);
SCATHA(API) std::ostream& operator<<(std::ostream&, SyntaxIssue::Reason);

bool expectIdentifier(issue::SyntaxIssueHandler&, Token const&);
bool expectDeclarator(issue::SyntaxIssueHandler&, Token const&);
bool expectSeparator(issue::SyntaxIssueHandler&, Token const&);
bool expectID(issue::SyntaxIssueHandler&, Token const&, std::string id);

} // namespace scatha::parse

#endif // SCATHA_PARSER_PARSERERROR_H_
