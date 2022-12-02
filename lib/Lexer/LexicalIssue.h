#pragma once

#ifndef SCATHA_LEXER_LEXERERROR_H_
#define SCATHA_LEXER_LEXERERROR_H_

#include <variant>

#include <utl/utility.hpp>

#include "Basic/Basic.h"
#include "Issue/ProgramIssue.h"
#include "Issue/VariantIssueBase.h"

namespace scatha::lex {

class SCATHA(API) IssueBase: public issue::ProgramIssueBase {
protected:
    using issue::ProgramIssueBase::ProgramIssueBase;
};

class SCATHA(API) UnexpectedID: public IssueBase {
public:
    explicit UnexpectedID(Token const& token): IssueBase(token) {}
};

class SCATHA(API) InvalidNumericLiteral: public IssueBase {
public:
    enum class Kind { Integer, FloatingPoint };
    explicit InvalidNumericLiteral(Token const& token, Kind kind): IssueBase(token), _kind(kind) {}

    Kind kind() const { return _kind; }

private:
    Kind _kind;
};

class SCATHA(API) UnterminatedStringLiteral: public IssueBase {
public:
    explicit UnterminatedStringLiteral(Token const& token): IssueBase(token) {}
};

class SCATHA(API) UnterminatedMultiLineComment: public IssueBase {
public:
    explicit UnterminatedMultiLineComment(Token const& token): IssueBase(token) {}
};

/// MARK: Common class LexicalIssue

namespace internal {

using IssueVariant = std::variant<UnexpectedID,
                                  InvalidNumericLiteral,
                                  UnterminatedStringLiteral,
                                  UnterminatedMultiLineComment>;

} // namespace internal

class SCATHA(API) LexicalIssue: public issue::internal::VariantIssueBase<internal::IssueVariant> {
public:
    using issue::internal::VariantIssueBase<internal::IssueVariant>::VariantIssueBase;
};

} // namespace scatha::lex

#endif // SCATHA_LEXER_LEXERERROR_H_
