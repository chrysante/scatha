#ifndef SCATHA_IR_PARSER_ISSUE_H_
#define SCATHA_IR_PARSER_ISSUE_H_

#include <iosfwd>
#include <variant>

#include "IR/Parser/SourceLocation.h"
#include "IR/Parser/Token.h"

namespace scatha::ir {

class SCATHA_API LexicalIssue {
public:
    explicit LexicalIssue(SourceLocation loc): _loc(loc) {}

    SourceLocation sourceLocation() const { return _loc; }

private:
    SourceLocation _loc;
};

class SCATHA_API SyntaxIssue {
public:
    explicit SyntaxIssue(Token token): _token(token) {}

    Token token() const { return _token; }

    SourceLocation sourceLocation() const { return token().sourceLocation(); }

private:
    Token _token;
};

class SCATHA_API SemanticIssue {
public:
    enum Reason {
        TypeMismatch,
        InvalidType,
        InvalidEntity,
        UseOfUndeclaredIdentifier,
        Redeclaration,
        UnexpectedID,
        ExpectedType,
        _count
    };

    explicit SemanticIssue(Token token, Reason reason):
        _token(token), _reason(reason) {}

    Token token() const { return _token; }

    SourceLocation sourceLocation() const { return token().sourceLocation(); }

    Reason reason() const { return _reason; }

private:
    Token _token;
    Reason _reason;
};

using ParseIssue = std::variant<LexicalIssue, SyntaxIssue, SemanticIssue>;

SCATHA_API void print(ParseIssue const& issue, std::ostream& ostream);

SCATHA_API void print(ParseIssue const& issue);

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_ISSUE_H_
