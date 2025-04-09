#ifndef SCATHA_IR_PARSER_ISSUE_H_
#define SCATHA_IR_PARSER_ISSUE_H_

#include <iosfwd>
#include <variant>

#include <scatha/IR/Parser/IRSourceLocation.h>
#include <scatha/IR/Parser/IRToken.h>

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
        InvalidFFIType,
        InvalidEntity,
        UseOfUndeclaredIdentifier,
        Redeclaration,
        UnexpectedID,
        ExpectedType,
        ExpectedConstantValue,
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

/// Prints \p issue to \p ostream
SCATHA_API void print(ParseIssue const& issue, std::ostream& ostream);

/// Prints \p issue to `std::cout`
SCATHA_API void print(ParseIssue const& issue);

/// Formats \p issue as a string
SCATHA_API std::string toString(ParseIssue const& issue);

} // namespace scatha::ir

#endif // SCATHA_IR_PARSER_ISSUE_H_
