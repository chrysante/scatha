// SCATHA-PUBLIC-HEADER

#ifndef SCATHA_PARSER_LEXICALISSUE_H_
#define SCATHA_PARSER_LEXICALISSUE_H_

#include <variant>

#include <utl/utility.hpp>

#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>

namespace scatha::parse {

/// Base class of all lexical errors
class SCATHA_API LexicalIssue: public Issue {
public:
protected:
    using Issue::Issue;
};

/// Unexpected character encountered
class SCATHA_API UnexpectedCharacter: public LexicalIssue {
public:
    explicit UnexpectedCharacter(SourceLocation sourceLoc):
        LexicalIssue(sourceLoc, IssueSeverity::Error) {}

private:
    std::string message() const override { return "Unexpected character"; }
};

/// Invalid numeric literal
class SCATHA_API InvalidNumericLiteral: public LexicalIssue {
public:
    enum class Kind { Integer, FloatingPoint };

    explicit InvalidNumericLiteral(SourceLocation sourceLoc, Kind kind):
        LexicalIssue(sourceLoc, IssueSeverity::Error), _kind(kind) {}

    Kind kind() const { return _kind; }

private:
    std::string message() const override { return "Invalid numeric literal"; }

    Kind _kind;
};

/// Unterminated string literal
class SCATHA_API UnterminatedStringLiteral: public LexicalIssue {
public:
    explicit UnterminatedStringLiteral(SourceLocation sourceLoc):
        LexicalIssue(sourceLoc, IssueSeverity::Error) {}

private:
    std::string message() const override {
        return "Unterminated string literal";
    }
};

/// Unterminated comment
class SCATHA_API UnterminatedMultiLineComment: public LexicalIssue {
public:
    explicit UnterminatedMultiLineComment(SourceLocation sourceLoc):
        LexicalIssue(sourceLoc, IssueSeverity::Error) {}

private:
    std::string message() const override {
        return "Unterminated multiline comment";
    }
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_LEXICALISSUE_H_
