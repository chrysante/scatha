// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_PARSER_SYNTAXISSUE2_H_
#define SCATHA_PARSER_SYNTAXISSUE2_H_

#include <iosfwd>
#include <string>
#include <string_view>

#include <scatha/AST/Token.h>
#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue2.h>
#include <scatha/Parser/Bracket.h>

namespace scatha::parse {

/// Base class of all syntax errors
class SCATHA_API SyntaxError: public Error {
protected:
    using Error::Error;

private:
    void doPrint(std::ostream&) const override;
    std::string doToString() const override;
};

class SCATHA_API ExpectedIdentifier: public SyntaxError {
public:
    explicit ExpectedIdentifier(Token const& token): SyntaxError(token) {}
};

class SCATHA_API ExpectedDeclarator: public SyntaxError {
public:
    explicit ExpectedDeclarator(Token const& token): SyntaxError(token) {}
};

class SCATHA_API ExpectedDelimiter: public SyntaxError {
public:
    explicit ExpectedDelimiter(Token const& token): SyntaxError(token) {}
};

class SCATHA_API ExpectedExpression: public SyntaxError {
public:
    explicit ExpectedExpression(Token const& token): SyntaxError(token) {}
};

class SCATHA_API ExpectedClosingBracket: public SyntaxError {
public:
    explicit ExpectedClosingBracket(Token const& token): SyntaxError(token) {}
};

class SCATHA_API UnexpectedClosingBracket: public SyntaxError {
public:
    explicit UnexpectedClosingBracket(Token const& token): SyntaxError(token) {}
};

class SCATHA_API UnqualifiedID: public SyntaxError {
public:
    explicit UnqualifiedID(Token const& token, TokenKind expected):
        SyntaxError(token), exp(expected) {}

    TokenKind expected() const { return exp; }

private:
    TokenKind exp;
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_SYNTAXISSUE_H_
