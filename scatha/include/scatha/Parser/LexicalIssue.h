#ifndef SCATHA_PARSER_LEXICALISSUE_H_
#define SCATHA_PARSER_LEXICALISSUE_H_

#include <variant>

#include <scatha/Common/Base.h>
#include <scatha/Issue/Issue.h>

namespace scatha::parser {

/// Base class of all lexical errors
class SCATHA_API LexicalIssue: public Issue {
public:
protected:
    using Issue::Issue;
};

/// Unexpected character encountered
class SCATHA_API UnexpectedCharacter: public LexicalIssue {
public:
    explicit UnexpectedCharacter(SourceRange sourceRange):
        LexicalIssue(sourceRange, IssueSeverity::Error) {}

private:
    void format(std::ostream&) const override;
};

/// Invalid numeric literal
class SCATHA_API InvalidNumericLiteral: public LexicalIssue {
public:
    enum class Kind { Integer, FloatingPoint };

    explicit InvalidNumericLiteral(SourceRange sourceRange, Kind kind):
        LexicalIssue(sourceRange, IssueSeverity::Error), _kind(kind) {}

    Kind kind() const { return _kind; }

private:
    void format(std::ostream&) const;

    Kind _kind;
};

/// Unterminated string literal
class SCATHA_API UnterminatedStringLiteral: public LexicalIssue {
public:
    explicit UnterminatedStringLiteral(SourceRange sourceRange):
        LexicalIssue(sourceRange, IssueSeverity::Error) {}

private:
    void format(std::ostream&) const override;
};

/// Unterminated char literal
class SCATHA_API UnterminatedCharLiteral: public LexicalIssue {
public:
    explicit UnterminatedCharLiteral(SourceRange sourceRange):
        LexicalIssue(sourceRange, IssueSeverity::Error) {}

private:
    void format(std::ostream&) const override;
};

class SCATHA_API InvalidCharLiteral: public LexicalIssue {
public:
    explicit InvalidCharLiteral(SourceRange sourceRange):
        LexicalIssue(sourceRange, IssueSeverity::Error) {}

private:
    void format(std::ostream&) const override;
};

class SCATHA_API InvalidEscapeSequence: public LexicalIssue {
public:
    explicit InvalidEscapeSequence(char litValue, SourceRange sourceRange):
        LexicalIssue(sourceRange, IssueSeverity::Warning), litValue(litValue) {}

private:
    void format(std::ostream&) const override;

    char litValue;
};

/// Unterminated comment
class SCATHA_API UnterminatedMultiLineComment: public LexicalIssue {
public:
    explicit UnterminatedMultiLineComment(SourceRange sourceRange):
        LexicalIssue(sourceRange, IssueSeverity::Error) {}

private:
    void format(std::ostream&) const override;
};

} // namespace scatha::parser

#endif // SCATHA_PARSER_LEXICALISSUE_H_
