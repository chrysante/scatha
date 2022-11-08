#include "Lexer/Lexer.h"

#include <optional>

#include "Common/Expected.h"
#include "Common/SourceLocation.h"
#include "Lexer/LexerUtil.h"
#include "Lexer/LexicalIssue.h"

using namespace scatha;
using namespace lex;

namespace {

struct Context {
    utl::vector<Token> run();

    std::optional<Expected<Token, LexicalIssue>> getToken();

    std::optional<Expected<Token, LexicalIssue>> getSpaces();
    std::optional<Expected<Token, LexicalIssue>> getOneLineComment();
    std::optional<Expected<Token, LexicalIssue>> getMultiLineComment();
    std::optional<Expected<Token, LexicalIssue>> getPunctuation();
    std::optional<Expected<Token, LexicalIssue>> getOperator();
    std::optional<Expected<Token, LexicalIssue>> getIntegerLiteral();
    std::optional<Expected<Token, LexicalIssue>> getIntegerLiteralHex();
    std::optional<Expected<Token, LexicalIssue>> getFloatingPointLiteral();
    std::optional<Expected<Token, LexicalIssue>> getStringLiteral();
    std::optional<Expected<Token, LexicalIssue>> getBooleanLiteral();
    std::optional<Expected<Token, LexicalIssue>> getIdentifier();

    bool advance();
    bool advance(size_t count);

    void advanceToNextWhitespace();

    ssize_t textSize() const { return (ssize_t)text.size(); }

    Token beginToken(TokenType type) const;
    char current() const;
    std::optional<char> next(ssize_t offset = 1) const;

    std::string_view text;
    issue::LexicalIssueHandler& iss;
    SourceLocation currentLocation{ 0, 1, 1 };
};

} // namespace

utl::vector<Token> lex::lex(std::string_view text, issue::LexicalIssueHandler& iss) {
    Context ctx{ text, iss };
    return ctx.run();
}

utl::vector<Token> Context::run() {
    utl::vector<Token> result;
    while (currentLocation.index < textSize()) {
        if (auto optToken = getToken()) {
            auto& expToken = *optToken;
            if (!expToken) {
                iss.push(expToken.error());
                advanceToNextWhitespace();
                continue;
            }
            auto& token = *expToken;
            if (token.type != TokenType::Whitespace) {
                result.push_back(std::move(token));
            }
            continue;
        }
        iss.push(UnexpectedID(beginToken(TokenType::Other)));
        advanceToNextWhitespace();
    }
    SC_ASSERT(currentLocation.index == textSize(), "How is this possible?");
    Token eof = beginToken(TokenType::EndOfFile);
    result.push_back(eof);
    for (auto& token: result) {
        finalize(token);
    }
    return result;
}

std::optional<Expected<Token, LexicalIssue>> Context::getToken() {
    SC_ASSERT_AUDIT(currentLocation.index != textSize(), "");
    if (auto spaces = getSpaces()) {
        return *spaces;
    }
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

std::optional<Expected<Token, LexicalIssue>> Context::getSpaces() {
    if (!isSpace(current())) {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::Whitespace);
    while (isSpace(current())) {
        result.id += current();
        if (!advance()) {
            break;
        }
    }
    return result;
}

std::optional<Expected<Token, LexicalIssue>> Context::getOneLineComment() {
    if (current() != '/') {
        return std::nullopt;
    }
    if (auto const next = this->next(); !next || *next != '/') {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::Whitespace);
    result.id += current();
    advance();
    while (true) {
        result.id += current();
        if (current() == '\n' || !advance()) {
            return result;
        }
    }
}

std::optional<Expected<Token, LexicalIssue>> Context::getMultiLineComment() {
    if (current() != '/') {
        return std::nullopt;
    }
    if (auto const next = this->next(); !next || *next != '*') {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::Whitespace);
    result.id += current();
    advance();
    // now we are at the next character after "/*"
    while (true) {
        result.id += current();

        if (!advance()) {
            return UnterminatedMultiLineComment(result);
        }

        if (result.id.back() == '*' && current() == '/') {
            result.id += current();
            advance();
            return result;
        }
    }
}

std::optional<Expected<Token, LexicalIssue>> Context::getPunctuation() {
    if (!isPunctuation(current())) {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::Punctuation);
    result.id += current();
    advance();
    return result;
}

std::optional<Expected<Token, LexicalIssue>> Context::getOperator() {
    Token result = beginToken(TokenType::Operator);
    result.id += current();

    if (!isOperator(result.id)) {
        return std::nullopt;
    }

    while (true) {
        if (!advance()) {
            return result;
        }
        result.id += current();
        if (!isOperator(result.id)) {
            result.id.pop_back();
            return result;
        }
    }
}

std::optional<Expected<Token, LexicalIssue>> Context::getIntegerLiteral() {
    if (!isDigitDec(current())) {
        return std::nullopt;
    }
    if (current() == '0' && next() && *next() == 'x') {
        return std::nullopt; // We are a hex literal, not our job
    }
    Token result = beginToken(TokenType::IntegerLiteral);
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
        return result;
    }
    if (*next == '.') {
        // we are a floating point literal
        return std::nullopt;
    }
    return InvalidNumericLiteral(result, InvalidNumericLiteral::Kind::Integer);
}

std::optional<Expected<Token, LexicalIssue>> Context::getIntegerLiteralHex() {
    if (current() != '0' || !next() || *next() != 'x') {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::IntegerLiteral);
    result.id += current();
    advance();
    result.id += current();
    // Now result.id == "0x"
    while (advance() && isDigitHex(current())) {
        result.id += current();
    }
    if (next() && !isLetter(*next())) {
        return result;
    }
    return InvalidNumericLiteral(result, InvalidNumericLiteral::Kind::Integer);
}

std::optional<Expected<Token, LexicalIssue>> Context::getFloatingPointLiteral() {
    if (!isFloatDigitDec(current())) {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::FloatingPointLiteral);
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
        return result;
    }
    return InvalidNumericLiteral(result, InvalidNumericLiteral::Kind::FloatingPoint);
}

std::optional<Expected<Token, LexicalIssue>> Context::getStringLiteral() {
    if (current() != '"') {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::StringLiteral);
    if (!advance()) {
        return UnterminatedStringLiteral(result);
    }
    while (true) {
        if (current() == '"') {
            advance();
            return result;
        }
        result.id += current();
        if (!advance() || current() == '\n') {
            return UnterminatedStringLiteral(result);
        }
    }
}

std::optional<Expected<Token, LexicalIssue>> Context::getBooleanLiteral() {
    if (currentLocation.index + 3 < textSize() &&
        text.substr(utl::narrow_cast<size_t>(currentLocation.index), 4) == "true") {
        if (auto const n = next(4); n && isLetterEx(*n)) {
            return std::nullopt;
        }
        Token result = beginToken(TokenType::BooleanLiteral);
        result.id    = "true";
        advance(4);
        return result;
    }
    if (currentLocation.index + 4 < textSize() &&
        text.substr(utl::narrow_cast<size_t>(currentLocation.index), 5) == "false")
    {
        if (auto const n = next(5); n && isLetterEx(*n)) {
            return std::nullopt;
        }
        Token result = beginToken(TokenType::BooleanLiteral);
        result.id    = "false";
        advance(5);
        return result;
    }
    return std::nullopt;
}

std::optional<Expected<Token, LexicalIssue>> Context::getIdentifier() {
    if (!isLetter(current())) {
        return std::nullopt;
    }
    Token result = beginToken(TokenType::Identifier);
    result.id += current();
    while (advance() && isLetterEx(current())) {
        result.id += current();
    }
    return result;
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

Token Context::beginToken(TokenType type) const {
    Token result;
    result.sourceLocation = currentLocation;
    result.type           = type;
    return result;
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
