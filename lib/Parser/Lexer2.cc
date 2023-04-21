#include "Parser/Lexer.h"

#include <optional>

#include "AST/SourceLocation.h"
#include "Common/Expected.h"
#include "Parser/LexerUtil.h"
#include "Parser/LexicalIssue2.h"

using namespace scatha;
using namespace parse;
using namespace lex;

namespace {

struct Context {
    utl::vector<Token> run();

    std::optional<Token> getToken();

    void ignoreSpaces();
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

    bool advance();
    bool advance(size_t count);

    void advanceToNextWhitespace();

    ssize_t textSize() const { return utl::narrow_cast<ssize_t>(text.size()); }

    TokenData beginToken(TokenType type) const;
    char current() const;
    std::optional<char> next(ssize_t offset = 1) const;

    std::string_view text;
    IssueHandler& issues;
    SourceLocation currentLocation{ 0, 0, 0 };
};

} // namespace

utl::vector<Token> parse::lex(std::string_view text, IssueHandler& issues) {
    Context ctx{ text, issues };
    return ctx.run();
}

utl::vector<Token> Context::run() {
    utl::vector<Token> result;
    while (currentLocation.index < textSize()) {
        auto token = getToken();
        if (!token) {
            advanceToNextWhitespace();
        }
        else {
            result.push_back(std::move(*token));
        }
    }
    SC_ASSERT(currentLocation.index == textSize(), "How is this possible?");
    Token eof = Token(beginToken(TokenType::EndOfFile));
    result.push_back(eof);
    return result;
}

std::optional<Token> Context::getToken() {
    SC_ASSERT(currentLocation.index < textSize(), "");
    ignoreSpaces();
    if (auto comment = getOneLineComment()) {
        return *comment;
    }
    if (auto comment = getMultiLineComment()) {
        return *comment;
    }
    if (auto punctuation = getPunctuation()) {
        return *punctuation;
    }
    if (auto op = getOperator()) {
        return *op;
    }
    if (auto integerLiteral = getIntegerLiteral()) {
        return *integerLiteral;
    }
    if (auto integerLiteralHex = getIntegerLiteralHex()) {
        return *integerLiteralHex;
    }
    if (auto floatingPointLiteral = getFloatingPointLiteral()) {
        return *floatingPointLiteral;
    }
    if (auto stringLiteral = getStringLiteral()) {
        return *stringLiteral;
    }
    if (auto booleanLiteral = getBooleanLiteral()) {
        return *booleanLiteral;
    }
    if (auto identifier = getIdentifier()) {
        return *identifier;
    }
    return std::nullopt;
}

void Context::ignoreSpaces() {
    if (!isSpace(current())) {
        return;
    }
    while (isSpace(current())) {
        if (!advance()) {
            break;
        }
    }
}

std::optional<Token> Context::getOneLineComment() {
    if (current() != '/') {
        return std::nullopt;
    }
    if (auto const next = this->next(); !next || *next != '/') {
        return std::nullopt;
    }
    TokenData result = beginToken(TokenType::Whitespace);
    result.id += current();
    advance();
    while (true) {
        result.id += current();
        if (current() == '\n' || !advance()) {
            return Token(result);
        }
    }
}

std::optional<Token> Context::getMultiLineComment() {
    if (current() != '/') {
        return std::nullopt;
    }
    if (auto const next = this->next(); !next || *next != '*') {
        return std::nullopt;
    }
    TokenData result = beginToken(TokenType::Whitespace);
    result.id += current();
    advance();
    /// Now we are at the next character after `/*`
    while (true) {
        result.id += current();
        if (!advance()) {
            issues.push<UnterminatedMultiLineComment>(Token(result));
            return std::nullopt;
        }
        if (result.id.back() == '*' && current() == '/') {
            result.id += current();
            advance();
            return Token(result);
        }
    }
}

std::optional<Token> Context::getPunctuation() {
    if (!isPunctuation(current())) {
        return std::nullopt;
    }
    TokenData result = beginToken(TokenType::Punctuation);
    result.id += current();
    advance();
    return Token(result);
}

std::optional<Token> Context::getOperator() {
    TokenData result = beginToken(TokenType::Operator);
    result.id += current();
    if (!isOperator(result.id)) {
        return std::nullopt;
    }
    while (true) {
        if (!advance()) {
            return Token(result);
        }
        result.id += current();
        if (!isOperator(result.id)) {
            result.id.pop_back();
            return Token(result);
        }
    }
}

std::optional<Token> Context::getIntegerLiteral() {
    if (!isDigitDec(current())) {
        return std::nullopt;
    }
    if (current() == '0' && next() && *next() == 'x') {
        return std::nullopt; // We are a hex literal, not our job
    }
    TokenData result = beginToken(TokenType::IntegerLiteral);
    result.id += current();
    ssize_t offset     = 1;
    std::optional next = this->next(offset);
    while (next && isDigitDec(*next)) {
        result.id += *next;
        ++offset;
        next = this->next(offset);
    }
    if (!next || isDelimiter(*next)) {
        while (offset-- > 0) {
            advance();
        }
        return Token(result);
    }
    if (*next == '.') {
        // we are a floating point literal
        return std::nullopt;
    }
    issues.push<InvalidNumericLiteral>(Token(result),
                                       InvalidNumericLiteral::Kind::Integer);
    return std::nullopt;
}

std::optional<Token> Context::getIntegerLiteralHex() {
    if (current() != '0' || !next() || *next() != 'x') {
        return std::nullopt;
    }
    TokenData result = beginToken(TokenType::IntegerLiteral);
    result.id += current();
    advance();
    result.id += current();
    // Now result.id == "0x"
    while (advance() && isDigitHex(current())) {
        result.id += current();
    }
    if (next() && !isLetter(*next())) {
        return Token(result);
    }
    issues.push<InvalidNumericLiteral>(Token(result),
                                       InvalidNumericLiteral::Kind::Integer);
    return std::nullopt;
}

std::optional<Token> Context::getFloatingPointLiteral() {
    if (!isFloatDigitDec(current())) {
        return std::nullopt;
    }
    TokenData result = beginToken(TokenType::FloatingPointLiteral);
    result.id += current();
    ssize_t offset     = 1;
    std::optional next = this->next(offset);
    while (next && isFloatDigitDec(*next)) {
        result.id += *next;
        next = this->next(++offset);
    }
    if (result.id == ".") { // This is not a floating point literal
        return std::nullopt;
    }
    if (!next || isDelimiter(*next)) {
        while (offset-- > 0) {
            advance();
        }
        return Token(result);
    }
    issues.push<InvalidNumericLiteral>(
        Token(result),
        InvalidNumericLiteral::Kind::FloatingPoint);
    return std::nullopt;
}

std::optional<Token> Context::getStringLiteral() {
    if (current() != '"') {
        return std::nullopt;
    }
    TokenData result = beginToken(TokenType::StringLiteral);
    if (!advance()) {
        issues.push<UnterminatedStringLiteral>(Token(result));
        return std::nullopt;
    }
    while (true) {
        if (current() == '"') {
            advance();
            return Token(result);
        }
        result.id += current();
        if (!advance() || current() == '\n') {
            issues.push<UnterminatedStringLiteral>(Token(result));
            return std::nullopt;
        }
    }
}

std::optional<Token> Context::getBooleanLiteral() {
    if (currentLocation.index + 3 < textSize() &&
        text.substr(utl::narrow_cast<size_t>(currentLocation.index), 4) ==
            "true")
    {
        if (auto const n = next(4); n && isLetterEx(*n)) {
            return std::nullopt;
        }
        TokenData result = beginToken(TokenType::BooleanLiteral);
        result.id        = "true";
        advance(4);
        return Token(result);
    }
    if (currentLocation.index + 4 < textSize() &&
        text.substr(utl::narrow_cast<size_t>(currentLocation.index), 5) ==
            "false")
    {
        if (auto const n = next(5); n && isLetterEx(*n)) {
            return std::nullopt;
        }
        TokenData result = beginToken(TokenType::BooleanLiteral);
        result.id        = "false";
        advance(5);
        return Token(result);
    }
    return std::nullopt;
}

std::optional<Token> Context::getIdentifier() {
    if (!isLetter(current())) {
        return std::nullopt;
    }
    TokenData result = beginToken(TokenType::Identifier);
    result.id += current();
    while (advance() && isLetterEx(current())) {
        result.id += current();
    }
    return Token(result);
}

bool Context::advance() {
    if (text[utl::narrow_cast<size_t>(currentLocation.index)] == '\n') {
        currentLocation.column = 0;
        ++currentLocation.line;
    }
    ++currentLocation.index;
    ++currentLocation.column;
    if (currentLocation.index == textSize()) {
        return false;
    }
    return true;
}

bool Context::advance(size_t count) {
    while (count-- > 0) {
        if (!advance()) {
            return false;
        }
    }
    return true;
}

void Context::advanceToNextWhitespace() {
    SC_ASSERT(currentLocation.index <= textSize(), "");
    if (currentLocation.index == textSize()) {
        return;
    }
    while (true) {
        if (!advance()) {
            return;
        }
        if (isSpace(current())) {
            return;
        }
    }
}

TokenData Context::beginToken(TokenType type) const {
    return TokenData{ .id             = std::string{},
                      .type           = type,
                      .sourceLocation = currentLocation };
}

char Context::current() const {
    SC_ASSERT(currentLocation.index < textSize(), "");
    return text[utl::narrow_cast<size_t>(currentLocation.index)];
}

std::optional<char> Context::next(ssize_t offset) const {
    if (currentLocation.index + offset >= textSize()) {
        return std::nullopt;
    }
    return text[utl::narrow_cast<size_t>(currentLocation.index + offset)];
}
