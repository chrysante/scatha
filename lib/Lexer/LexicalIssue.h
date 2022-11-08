#pragma once

#ifndef SCATHA_LEXER_LEXERERROR_H_
#define SCATHA_LEXER_LEXERERROR_H_

#include <variant>

#include <utl/utility.hpp>

#include "Basic/Basic.h"
#include "Issue/ProgramIssue.h"

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
using LexIssueVariant =
    std::variant<UnexpectedID, InvalidNumericLiteral, UnterminatedStringLiteral, UnterminatedMultiLineComment>;
}

class SCATHA(API) LexicalIssue: private internal::LexIssueVariant {
private:
    template <typename T>
    static decltype(auto) visitImpl(auto&& f, auto&& v) {
        auto const vis = utl::visitor{ f, [](issue::internal::ProgramIssuePrivateBase&) -> T {
                                          if constexpr (!std::is_same_v<T, void>) {
                                              SC_DEBUGFAIL();
                                          }
                                      } };
        return std::visit(vis, v);
    }

public:
    using internal::LexIssueVariant::LexIssueVariant;

    template <typename T = void>
    decltype(auto) visit(auto&& f) {
        return visitImpl<T>(f, asBase());
    }

    template <typename T = void>
    decltype(auto) visit(auto&& f) const {
        return visitImpl<T>(f, asBase());
    }

    template <typename T>
    auto& get() {
        return std::get<T>(asBase());
    }

    template <typename T>
    auto const& get() const {
        return std::get<T>(asBase());
    }

    template <typename T>
    bool is() const {
        return std::holds_alternative<T>(asBase());
    }

    Token const& token() const {
        return visit([](issue::ProgramIssueBase const& base) -> auto& { return base.token(); });
    }

private:
    internal::LexIssueVariant& asBase() { return *this; }
    internal::LexIssueVariant const& asBase() const { return *this; }
};

} // namespace scatha::lex

#endif // SCATHA_LEXER_LEXERERROR_H_
