#pragma once

#ifndef SCATHA_LEXER_LEXER_H_
#define SCATHA_LEXER_LEXER_H_

#include <optional>
#include <string>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/SourceLocation.h"
#include "Common/Token.h"

namespace scatha::lex {

class SCATHA(API) Lexer {
  public:
    explicit Lexer(std::string_view text);

    [[nodiscard]] utl::vector<Token> lex();

  private:
    std::optional<Token> getToken();

    std::optional<Token> getSpaces();
    std::optional<Token> getOneLineComment();
    std::optional<Token> getMultiLineComment();
    std::optional<Token> getPunctuation();
    std::optional<Token> getOperator();
    std::optional<Token> getIntegerLiteral();
    std::optional<Token> getIntegerLiteralHex();
    std::optional<Token> getFloatingPointLiteral();
    std::optional<Token> getStringLiteral();
    std::optional<Token> getBooleanLiteral();
    std::optional<Token> getIdentifier();

    bool                 advance();
    bool                 advance(size_t count);

    Token                beginToken(TokenType type) const;
    char                 current() const;
    std::optional<char>  next(size_t offset = 1) const;

  private:
    std::string_view text;
    SourceLocation   currentLocation{0, 1, 1};
};

} // namespace scatha::lex

#endif // SCATHA_LEXER_LEXER_H_
