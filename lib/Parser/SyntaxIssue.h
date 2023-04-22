// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_PARSER_SYNTAXISSUE2_H_
#define SCATHA_PARSER_SYNTAXISSUE2_H_

#include <iosfwd>
#include <string>
#include <string_view>

#include <scatha/AST/Token.h>
#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>
#include <scatha/Parser/Bracket.h>

namespace scatha::parse {

/// Base class of all syntax errors
class SCATHA_API SyntaxIssue: public Issue {
public:
    Token const& token() const { return tok; }

protected:
    explicit SyntaxIssue(Token token, IssueSeverity severity):
        Issue(token.sourceLocation(), severity), tok(std::move(token)) {}
    using Issue::Issue;

private:
    void doPrint(std::ostream&) const override;
    std::string doToString() const override;

    Token tok;
};

class SCATHA_API ExpectedIdentifier: public SyntaxIssue {
public:
    explicit ExpectedIdentifier(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}
};

class SCATHA_API ExpectedDeclarator: public SyntaxIssue {
public:
    explicit ExpectedDeclarator(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}
};

class SCATHA_API ExpectedDelimiter: public SyntaxIssue {
public:
    explicit ExpectedDelimiter(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}
};

class SCATHA_API ExpectedExpression: public SyntaxIssue {
public:
    explicit ExpectedExpression(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}
};

class SCATHA_API ExpectedClosingBracket: public SyntaxIssue {
public:
    explicit ExpectedClosingBracket(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}
};

class SCATHA_API UnexpectedClosingBracket: public SyntaxIssue {
public:
    explicit UnexpectedClosingBracket(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}
};

class SCATHA_API UnqualifiedID: public SyntaxIssue {
public:
    explicit UnqualifiedID(Token token, TokenKind expected):
        SyntaxIssue(std::move(token), IssueSeverity::Error), exp(expected) {}

    TokenKind expected() const { return exp; }

private:
    TokenKind exp;
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_SYNTAXISSUE_H_
