#include "Parser/Lexer.h"

#include <optional>

#include "Common/EscapeSequence.h"
#include "Common/Expected.h"
#include "Common/SourceLocation.h"
#include "Parser/LexerUtil.h"
#include "Parser/LexicalIssue.h"

using namespace scatha;
using namespace parse;
using namespace lex;

namespace {

struct Context {
    utl::vector<Token> run();

    std::optional<Token> getToken();

    bool ignoreSpaces();
    bool ignoreLineComment();
    bool ignoreMultiLineComment();
    std::optional<Token> getPunctuation();
    std::optional<Token> getOperator();
    std::optional<Token> getIntegerLiteral();
    std::optional<Token> getIntegerLiteralHex();
    std::optional<Token> getFloatingPointLiteral();
    std::optional<Token> getStringLiteral();
    std::optional<Token> getCharLiteral();
    std::optional<Token> getBooleanLiteral();
    std::optional<Token> getIdentifier();

    bool advance();
    bool advance(size_t count);

    void advanceToNextWhitespace();

    ssize_t textSize() const { return utl::narrow_cast<ssize_t>(text.size()); }

    std::pair<std::string, SourceLocation> beginToken() const;
    char current() const;
    std::optional<char> next(ssize_t offset = 1) const;

    std::string_view text;
    IssueHandler& issues;
    SourceLocation currentLocation{ 0, 1, 1 };
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
    result.push_back(
        Token({}, TokenKind::EndOfFile, { currentLocation, currentLocation }));
    return result;
}

std::optional<Token> Context::getToken() {
    if (currentLocation.index >= textSize()) {
        return std::nullopt;
    }
    if (ignoreSpaces() || ignoreLineComment() || ignoreMultiLineComment()) {
        if (issues.haveErrors()) {
            return std::nullopt;
        }
        return getToken();
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
    if (auto charLiteral = getCharLiteral()) {
        return *charLiteral;
    }
    if (auto booleanLiteral = getBooleanLiteral()) {
        return *booleanLiteral;
    }
    if (auto identifier = getIdentifier()) {
        return *identifier;
    }
    return std::nullopt;
}

bool Context::ignoreSpaces() {
    if (!isSpace(current())) {
        return false;
    }
    while (isSpace(current())) {
        if (!advance()) {
            break;
        }
    }
    return true;
}

bool Context::ignoreLineComment() {
    if (current() != '/') {
        return false;
    }
    if (auto const next = this->next(); !next || *next != '/') {
        return false;
    }
    advance();
    while (true) {
        if (current() == '\n' || !advance()) {
            return true;
        }
    }
}

bool Context::ignoreMultiLineComment() {
    if (current() != '/') {
        return false;
    }
    if (auto const next = this->next(); !next || *next != '*') {
        return false;
    }
    SourceLocation const beginLoc = currentLocation;
    advance();
    /// Now we are at the next character after `/*`
    char last = current();
    while (true) {
        if (!advance()) {
            issues.push<UnterminatedMultiLineComment>(
                SourceRange{ beginLoc, currentLocation });
            return true;
        }
        if (last == '*' && current() == '/') {
            advance();
            return true;
        }
        last = current();
    }
}

std::optional<Token> Context::getPunctuation() {
    if (!isPunctuation(current())) {
        return std::nullopt;
    }
    auto [id, beginLoc] = beginToken();
    id += current();
    auto punctuation = [&] {
        using enum TokenKind;
        switch (current()) {
        case '(':
            return OpenParan;
        case ')':
            return CloseParan;
        case '{':
            return OpenBrace;
        case '}':
            return CloseBrace;
        case '[':
            return OpenBracket;
        case ']':
            return CloseBracket;
        case ',':
            return Comma;
        case ';':
            return Semicolon;
        case ':':
            return Colon;
        default:
            SC_UNREACHABLE();
        }
    }();
    advance();
    return Token(id, punctuation, { beginLoc, currentLocation });
}

static TokenKind toOperator(std::string_view str) {
    if (false) (void)0; /// To make following chained `if/else` cases work
#define SC_OPERATOR_TOKEN_DEF(Token, op)                                       \
    else if (str == op) { return TokenKind::Token; }
#include "Parser/Token.def"
    SC_UNREACHABLE();
}

std::optional<Token> Context::getOperator() {
    auto [id, beginLoc] = beginToken();
    id += current();
    if (!isOperator(id)) {
        return std::nullopt;
    }
    while (true) {
        if (!advance()) {
            break;
        }
        id += current();
        if (!isOperator(id)) {
            id.pop_back();
            break;
        }
    }
    return Token(id, toOperator(id), { beginLoc, currentLocation });
}

std::optional<Token> Context::getIntegerLiteral() {
    if (!isDigitDec(current())) {
        return std::nullopt;
    }
    if (current() == '0' && next() && *next() == 'x') {
        return std::nullopt; // We are a hex literal, not our job
    }
    auto [id, beginLoc] = beginToken();
    id += current();
    ssize_t offset     = 1;
    std::optional next = this->next(offset);
    while (next && isDigitDec(*next)) {
        id += *next;
        ++offset;
        next = this->next(offset);
    }
    if (!next || isDelimiter(*next)) {
        while (offset-- > 0) {
            advance();
        }
        return Token(id,
                     TokenKind::IntegerLiteral,
                     { beginLoc, currentLocation });
    }
    if (*next == '.') {
        // We are a float literal
        return std::nullopt;
    }
    issues.push<InvalidNumericLiteral>(SourceRange{ beginLoc, currentLocation },
                                       InvalidNumericLiteral::Kind::Integer);
    return std::nullopt;
}

std::optional<Token> Context::getIntegerLiteralHex() {
    if (current() != '0' || !next() || *next() != 'x') {
        return std::nullopt;
    }
    auto [id, beginLoc] = beginToken();
    id += current();
    advance();
    id += current();
    // Now result.id == "0x"
    while (advance() && isDigitHex(current())) {
        id += current();
    }
    if (next() && !isLetter(*next())) {
        return Token(id,
                     TokenKind::IntegerLiteral,
                     { beginLoc, currentLocation });
    }
    issues.push<InvalidNumericLiteral>(SourceRange{ beginLoc, currentLocation },
                                       InvalidNumericLiteral::Kind::Integer);
    return std::nullopt;
}

std::optional<Token> Context::getFloatingPointLiteral() {
    if (!isFloatDigitDec(current())) {
        return std::nullopt;
    }
    auto [id, beginLoc] = beginToken();
    id += current();
    ssize_t offset     = 1;
    std::optional next = this->next(offset);
    while (next && isFloatDigitDec(*next)) {
        id += *next;
        next = this->next(++offset);
    }
    if (id == ".") { // This is not a floating point literal
        return std::nullopt;
    }
    if (!next || isDelimiter(*next)) {
        while (offset-- > 0) {
            advance();
        }
        return Token(id,
                     TokenKind::FloatLiteral,
                     { beginLoc, currentLocation });
    }
    issues.push<InvalidNumericLiteral>(
        SourceRange{ beginLoc, currentLocation },
        InvalidNumericLiteral::Kind::FloatingPoint);
    return std::nullopt;
}

std::optional<Token> Context::getStringLiteral() {
    if (current() != '"') {
        return std::nullopt;
    }
    auto [id, beginLoc] = beginToken();
    if (!advance()) {
        issues.push<UnterminatedStringLiteral>(
            SourceRange{ beginLoc, currentLocation });
        return std::nullopt;
    }
    bool haveEscape = false;
    while (true) {
        if (haveEscape) {
            haveEscape = false;
            if (auto seq = toEscapeSequence(current())) {
                id += *seq;
                advance();
            }
            else {
                auto begin = currentLocation;
                auto end   = currentLocation;
                --begin.index, --begin.column, ++end.index, ++end.column;
                issues.push<InvalidEscapeSequence>(current(),
                                                   SourceRange{ begin, end });
            }
        }
        if (current() == '"') {
            advance();
            return Token(id,
                         TokenKind::StringLiteral,
                         { beginLoc, currentLocation });
        }
        auto c = current();
        if (c == '\\') {
            haveEscape = true;
        }
        else {
            id += c;
        }
        if (!advance() || current() == '\n') {
            issues.push<UnterminatedStringLiteral>(
                SourceRange{ beginLoc, currentLocation });
            return std::nullopt;
        }
    }
}

std::optional<Token> Context::getCharLiteral() {
    if (current() != '\'') {
        return std::nullopt;
    }
    auto [id, beginLoc] = beginToken();
    auto advance        = [&, beginLoc = beginLoc] {
        if (this->advance()) {
            return true;
        }
        issues.push<UnterminatedCharLiteral>(
            SourceRange{ beginLoc, currentLocation });
        return false;
    };
    if (!advance()) {
        return std::nullopt;
    }
    if (current() == '\\') {
        if (!advance()) {
            return std::nullopt;
        }
        if (auto seq = toEscapeSequence(current())) {
            id += *seq;
        }
        else {
            auto begin = currentLocation;
            auto end   = currentLocation;
            --begin.index, --begin.column, ++end.index, ++end.column;
            issues.push<InvalidEscapeSequence>(current(),
                                               SourceRange{ begin, end });
            id += current();
        }
    }
    else {
        id += current();
    }
    if (!advance()) {
        return std::nullopt;
    }
    if (current() == '\'') {
        this->advance();
        return Token(id, TokenKind::CharLiteral, { beginLoc, currentLocation });
    }
    while (true) {
        if (current() == '\'') {
            this->advance();
            issues.push<InvalidCharLiteral>(
                SourceRange{ beginLoc, currentLocation });
            return std::nullopt;
        }
        if (current() == '\n' || !this->advance()) {
            issues.push<UnterminatedCharLiteral>(
                SourceRange{ beginLoc, currentLocation });
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
        auto [id, beginLoc] = beginToken();
        id                  = "true";
        advance(4);
        return Token(id, TokenKind::True, { beginLoc, currentLocation });
    }
    if (currentLocation.index + 4 < textSize() &&
        text.substr(utl::narrow_cast<size_t>(currentLocation.index), 5) ==
            "false")
    {
        if (auto const n = next(5); n && isLetterEx(*n)) {
            return std::nullopt;
        }
        auto [id, beginLoc] = beginToken();
        id                  = "false";
        advance(5);
        return Token(id, TokenKind::False, { beginLoc, currentLocation });
    }
    return std::nullopt;
}

static TokenKind idToTokenKind(std::string_view id) {
    if (false) (void)0;
#define SC_KEYWORD_TOKEN_DEF(Token, str)                                       \
    else if (id == str) { return TokenKind::Token; }
#include "Parser/Token.def"
    return TokenKind::Identifier;
}

std::optional<Token> Context::getIdentifier() {
    if (!isLetter(current())) {
        return std::nullopt;
    }
    auto [id, beginLoc] = beginToken();
    id += current();
    while (advance() && isLetterEx(current())) {
        id += current();
    }
    return Token(id, idToTokenKind(id), { beginLoc, currentLocation });
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

std::pair<std::string, SourceLocation> Context::beginToken() const {
    return { std::string{}, currentLocation };
}

char Context::current() const {
    auto index = utl::narrow_cast<size_t>(currentLocation.index);
    if (index >= text.size()) {
        return '\0';
    }
    return text[index];
}

std::optional<char> Context::next(ssize_t offset) const {
    if (currentLocation.index + offset >= textSize()) {
        return std::nullopt;
    }
    return text[utl::narrow_cast<size_t>(currentLocation.index + offset)];
}
