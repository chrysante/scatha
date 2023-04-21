// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_PARSER_LEXICALISSUE_H_
#define SCATHA_PARSER_LEXICALISSUE_H_

#include <variant>

#include <utl/utility.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue2.h>

namespace scatha::parse {

/// Base class of all lexical errors
class SCATHA_API LexicalError: public Error {
public:
protected:
    using Error::Error;

private:
    void doPrint(std::ostream&) const override;
    std::string doToString() const override;
};

/// Unexpected character encountered
class SCATHA_API UnexpectedCharacter: public LexicalError {
public:
    explicit UnexpectedCharacter(Token const& token): LexicalError(token) {}
};

/// Invalid numeric literal
class SCATHA_API InvalidNumericLiteral: public LexicalError {
public:
    enum class Kind { Integer, FloatingPoint };

    explicit InvalidNumericLiteral(Token const& token, Kind kind):
        LexicalError(token), _kind(kind) {}

    Kind kind() const { return _kind; }

private:
    Kind _kind;
};

/// Unterminated string literal
class SCATHA_API UnterminatedStringLiteral: public LexicalError {
public:
    explicit UnterminatedStringLiteral(Token const& token):
        LexicalError(token) {}
};

/// Unterminated comment
class SCATHA_API UnterminatedMultiLineComment: public LexicalError {
public:
    explicit UnterminatedMultiLineComment(Token const& token):
        LexicalError(token) {}
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_LEXICALISSUE_H_
