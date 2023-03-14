// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_ISSUE_PROGRAMISSUE_H_
#define SCATHA_ISSUE_PROGRAMISSUE_H_

#include <scatha/Basic/Basic.h>
#include <scatha/Common/Token.h>

namespace scatha::issue::internal {

/// So we can accept this as a parameter for default case in visitors.
class SCATHA(API) ProgramIssuePrivateBase {};

} // namespace scatha::issue::internal

namespace scatha::issue {

class SCATHA(API) ProgramIssueBase: public internal::ProgramIssuePrivateBase {
public:
    explicit ProgramIssueBase(Token token): _token(std::move(token)) {}
    explicit ProgramIssueBase(SourceLocation location) {
        _token.sourceLocation = location;
    }

    Token const& token() const { return _token; }
    SourceLocation sourceLocation() const { return _token.sourceLocation; }

    void setToken(Token token) { _token = std::move(token); }

private:
    Token _token;
};

} // namespace scatha::issue

#endif // SCATHA_ISSUE_PROGRAMISSUE_H_
