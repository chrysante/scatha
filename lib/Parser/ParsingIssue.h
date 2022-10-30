#ifndef SCATHA_PARSER_PARSERERROR_H_
#define SCATHA_PARSER_PARSERERROR_H_

#include <iosfwd>
#include <string>

#include "Basic/Basic.h"
#include "Common/ProgramIssue.h"
#include "Issue/ProgramIssue.h"

namespace scatha::issue {

class ParsingIssueHandler;

}

namespace scatha::parse {

class SCATHA(API) ParsingIssue: public issue::ProgramIssueBase {
public:
    enum class Reason {
        ExpectedIdentifier,
        ExpectedDeclarator,
        ExpectedSeparator,
        ExpectedExpression,
        ExpectedSpecificID,
        UnqualifiedID,
        _count
    };
    
    explicit ParsingIssue(Token const& token, Reason reason):
        issue::ProgramIssueBase(token), _reason(reason) {}
    
    ParsingIssue static expectedID(Token const& token, std::string id) {
        return ParsingIssue(token, std::move(id));
    }
    
    Reason reason() const { return _reason; }
    
    std::string_view expectedID() const {
        SC_EXPECT(_reason == Reason::ExpectedSpecificID, "Invalid");
        return _expectedID;
    }
    
private:
    ParsingIssue(Token const& token, std::string id):
        issue::ProgramIssueBase(token), _reason(Reason::ExpectedSpecificID), _expectedID(std::move(id)) {}
    
private:
    Reason _reason;
    std::string _expectedID;
};

SCATHA(API) std::string_view toString(ParsingIssue::Reason);
SCATHA(API) std::ostream& operator<<(std::ostream&, ParsingIssue::Reason);

bool expectIdentifier(issue::ParsingIssueHandler&, Token const&);
bool expectDeclarator(issue::ParsingIssueHandler&, Token const&);
bool expectSeparator(issue::ParsingIssueHandler&, Token const&);
bool expectID(issue::ParsingIssueHandler&, Token const&, std::string id);


//void expectIdentifier(Token const&, std::string_view message = {});
//void expectKeyword(Token const&, std::string_view message = {});
//void expectDeclarator(Token const&, std::string_view message = {});
//void expectSeparator(Token const&, std::string_view message = {});
//void expectID(Token const&, std::string_view, std::string_view message = {});

} // namespace scatha::parse

#endif // SCATHA_PARSER_PARSERERROR_H_
