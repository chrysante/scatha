// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_PARSER_SYNTAXISSUE2_H_
#define SCATHA_PARSER_SYNTAXISSUE2_H_

#include <iosfwd>
#include <string>
#include <string_view>

#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>
#include <scatha/Parser/Bracket.h>
#include <scatha/Parser/Token.h>

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
    Token tok;
};

class SCATHA_API ExpectedIdentifier: public SyntaxIssue {
public:
    explicit ExpectedIdentifier(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}

private:
    std::string message() const override { return "Expected identifier"; }
};

class SCATHA_API ExpectedDeclarator: public SyntaxIssue {
public:
    explicit ExpectedDeclarator(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}

private:
    std::string message() const override { return "Expected declarator"; }
};

class SCATHA_API ExpectedDelimiter: public SyntaxIssue {
public:
    explicit ExpectedDelimiter(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}

private:
    std::string message() const override { return "Expected delimiter"; }
};

class SCATHA_API ExpectedExpression: public SyntaxIssue {
public:
    explicit ExpectedExpression(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}

private:
    std::string message() const override { return "Expected expression"; }
};

class SCATHA_API ExpectedClosingBracket: public SyntaxIssue {
public:
    explicit ExpectedClosingBracket(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}

private:
    std::string message() const override { return "Expected closing bracket"; }
};

class SCATHA_API UnexpectedClosingBracket: public SyntaxIssue {
public:
    explicit UnexpectedClosingBracket(Token token):
        SyntaxIssue(std::move(token), IssueSeverity::Error) {}

private:
    std::string message() const override {
        return "Unexpected closing bracket";
    }
};

class SCATHA_API UnqualifiedID: public SyntaxIssue {
public:
    explicit UnqualifiedID(Token token, TokenKind expected):
        SyntaxIssue(std::move(token), IssueSeverity::Error), exp(expected) {}

    TokenKind expected() const { return exp; }

private:
    std::string message() const override { return "Unqualified ID"; }

    TokenKind exp;
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_SYNTAXISSUE_H_
