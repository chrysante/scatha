#include "Parser/Lexer.h"

#include <optional>

#include <ctre.hpp>
#include <range/v3/algorithm.hpp>
#include <utl/hashtable.hpp>

#include "Common/EscapeSequence.h"
#include "Common/Expected.h"
#include "Common/SourceLocation.h"
#include "Parser/LexerUtil.h"
#include "Parser/LexicalIssue.h"

using namespace scatha;
using namespace parser;
using namespace lex;

namespace {

struct Context {
    std::vector<Token> run();

    std::optional<Token> getToken();

    bool ignoreSpaces();
    bool ignoreLineComment();
    bool ignoreMultiLineComment();
    std::optional<Token> getPunctuation();
    std::optional<Token> getOperator();
    std::optional<Token> getFloatingPointLiteral();
    std::optional<Token> getIntegerLiteral();
    template <ctll::fixed_string BeginDelim, ctll::fixed_string EndDelim,
              typename Error>
    std::optional<Token> getStringLiteralImpl(TokenKind kind);
    char getCurrentEscapeSequence();
    std::optional<Token> getStringLiteral();
    std::optional<Token> getCharLiteral();
    std::optional<Token> getBooleanLiteral();
    std::optional<Token> getIdentifier();

    bool advance();
    bool advance(size_t count);

    std::pair<std::string, SourceLocation> beginToken() const;
    Token extractToken(size_t count, TokenKind kind);
    Token extractToken(size_t count, auto tokenKindFactory);
    char current() const;

    std::string_view text;
    size_t fileIndex;
    IssueHandler& issues;
    SourceLocation currentLocation{ 0, fileIndex, 1, 1 };
};

} // namespace

std::vector<Token> parser::lex(std::string_view text, IssueHandler& issues,
                               size_t fileIndex) {
    Context ctx{ text, fileIndex, issues };
    return ctx.run();
}

std::vector<Token> Context::run() {
    std::vector<Token> result;
    while (!text.empty()) {
        if (auto token = getToken()) {
            result.push_back(std::move(*token));
        }
    }
    SC_ASSERT(text.empty(), "The entire string shall be tokenized");
    result.push_back(
        Token({}, TokenKind::EndOfFile, { currentLocation, currentLocation }));
    return result;
}

std::optional<Token> Context::getToken() {
    if (text.empty()) {
        return std::nullopt;
    }
    if (ignoreSpaces() || ignoreLineComment() || ignoreMultiLineComment()) {
        if (issues.haveErrors()) {
            return std::nullopt;
        }
        return getToken();
    }
    if (auto floatingPointLiteral = getFloatingPointLiteral()) {
        return *floatingPointLiteral;
    }
    if (auto integerLiteral = getIntegerLiteral()) {
        return *integerLiteral;
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
    if (auto punctuation = getPunctuation()) {
        return *punctuation;
    }
    if (auto op = getOperator()) {
        return *op;
    }
    return std::nullopt;
}

bool Context::ignoreSpaces() {
    auto m = ctre::starts_with<"\\s+">(text);
    if (!m) {
        return false;
    }
    return advance(m.size());
}

bool Context::ignoreLineComment() {
    auto m = ctre::starts_with<"\\/\\/.*?\\n">(text);
    if (!m) {
        return false;
    }
    return advance(m.size());
}

bool Context::ignoreMultiLineComment() {
    auto m = ctre::starts_with<"\\/\\*(.|\\n)*?\\*\\/">(text);
    if (!m) {
        return false;
    }
    return advance(m.size());
}

static TokenKind toPunctuation(std::string_view ID) {
    SC_EXPECT(ID.size() == 1);
    using enum TokenKind;
    switch (ID.front()) {
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
}

std::optional<Token> Context::getPunctuation() {
    auto m = ctre::starts_with<"\\[|\\]|\\(|\\)|\\{|\\}|,|:|;">(text);
    if (!m) {
        return std::nullopt;
    }
    std::string ID = m.to_string();
    SC_ASSERT(ID.size() == 1, "");
    auto beginLoc = currentLocation;
    advance();
    return Token(ID, toPunctuation(ID), { beginLoc, currentLocation });
}

static TokenKind toOperator(std::string_view str) {
    if ((false)) (void)0; /// To make following chained `if/else` cases work
#define SC_OPERATOR_TOKEN_DEF(Token, op)                                       \
    else if (str == op) { return TokenKind::Token; }
#include "Parser/Token.def.h"
    SC_UNREACHABLE();
}

std::optional<Token> Context::getOperator() {
    auto [id, beginLoc] = beginToken();
    if (!isOperatorLetter(current())) {
        return std::nullopt;
    }
    id += current();
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

std::optional<Token> Context::getFloatingPointLiteral() {
    auto m = ctre::starts_with<"([\\d\\.]+)([A-Za-z_][A-Za-z0-9_]*)?">(text);
    if (!m || m.get<1>().size() == 1) {
        return std::nullopt;
    }
    size_t numDots = ranges::count(m.view(), '.');
    switch (numDots) {
    case 0:
        return std::nullopt;
    case 1:
        return extractToken(m.size(), TokenKind::FloatLiteral);
    default:
        auto beginLoc = currentLocation;
        advance(m.size());
        issues.push<InvalidNumericLiteral>(
            SourceRange{ beginLoc, currentLocation },
            InvalidNumericLiteral::Kind::FloatingPoint);
        return std::nullopt;
    }
}

std::optional<Token> Context::getIntegerLiteral() {
    if (auto m =
            ctre::starts_with<"0x[\\da-fA-F]+([A-Za-z_][A-Za-z0-9_]*)?">(text))
    {
        return extractToken(m.size(), TokenKind::IntegerLiteral);
    }
    if (auto m = ctre::starts_with<"\\d+([A-Za-z_][A-Za-z0-9_]*)?">(text)) {
        return extractToken(m.size(), TokenKind::IntegerLiteral);
    }
    return std::nullopt;
}

char Context::getCurrentEscapeSequence() {
    if (auto seq = toEscapeSequence(current())) {
        return *seq;
    }
    else {
        auto begin = currentLocation;
        auto end = currentLocation;
        --begin.index, --begin.column, ++end.index, ++end.column;
        issues.push<InvalidEscapeSequence>(current(),
                                           SourceRange{ begin, end });
        return current();
    }
}

template <size_t N, size_t M>
static constexpr auto operator+(ctll::fixed_string<N> a,
                                ctll::fixed_string<M> b) {
    std::array<char32_t, N + M> buffer{};
    ranges::copy(a, buffer.data());
    ranges::copy(b, buffer.data() + N);
    return ctll::fixed_string<N + M>(ctll::construct_from_pointer,
                                     buffer.data());
}

template <ctll::fixed_string BeginDelim, ctll::fixed_string EndDelim,
          typename Error>
std::optional<Token> Context::getStringLiteralImpl(TokenKind kind) {
    if (auto m = ctre::starts_with<BeginDelim>(text)) {
        advance(m.size());
    }
    else {
        return std::nullopt;
    }
    auto [id, beginLoc] = beginToken();
    auto pushError = [&] {
        issues.push<Error>(SourceRange{ beginLoc, currentLocation });
    };
    auto advanceChecked = [&] {
        if (!text.empty() && advance() && current() != '\n') {
            return true;
        }
        pushError();
        return false;
    };
    if (text.empty()) {
        pushError();
    }
    do {
        if (auto m = ctre::starts_with<EndDelim>(text)) {
            advance(m.size());
            return Token(id, kind, { beginLoc, currentLocation });
        }
        if (current() == '\\') {
            if (!advanceChecked()) {
                break;
            }
            id += getCurrentEscapeSequence();
        }
        else {
            id += current();
        }
    } while (advanceChecked());
    return std::nullopt;
}

std::optional<Token> Context::getStringLiteral() {
    return getStringLiteralImpl<"\"", "\"", UnterminatedStringLiteral>(
        TokenKind::StringLiteral);
}

std::optional<Token> Context::getCharLiteral() {
    auto token = getStringLiteralImpl<"\'", "\'", UnterminatedCharLiteral>(
        TokenKind::CharLiteral);
    if (!token) {
        return std::nullopt;
    }
    if (token->id().size() != 1) {
        issues.push<InvalidCharLiteral>(token->sourceRange());
    }
    return token;
}

std::optional<Token> Context::getBooleanLiteral() {
    if (auto m = ctre::starts_with<"true">(text)) {
        return extractToken(m.size(), TokenKind::True);
    }
    if (auto m = ctre::starts_with<"false">(text)) {
        return extractToken(m.size(), TokenKind::False);
    }
    return std::nullopt;
}

static TokenKind idToTokenKind(std::string_view ID) {
    static utl::hashmap<std::string_view, TokenKind> const map = {
#define SC_KEYWORD_TOKEN_DEF(Token, Spelling) { Spelling, TokenKind::Token },
#include "Parser/Token.def.h"
    };
    auto itr = map.find(ID);
    return itr != map.end() ? itr->second : TokenKind::Identifier;
}

std::optional<Token> Context::getIdentifier() {
    if (auto m = ctre::starts_with<"[a-zA-Z_][a-zA-Z0-9_]*">(text)) {
        return extractToken(m.size(), [](std::string_view ID) {
            return idToTokenKind(ID);
        });
    }
    return std::nullopt;
}

bool Context::advance() {
    SC_EXPECT(text.size() > 0);
    if (text.front() == '\n') {
        currentLocation.column = 0;
        ++currentLocation.line;
    }
    ++currentLocation.index;
    ++currentLocation.column;
    text = text.substr(1);
    return !text.empty();
}

bool Context::advance(size_t count) {
    for (size_t i = 0; i < count; ++i) {
        if (!advance()) {
            return false;
        }
    }
    return true;
}

std::pair<std::string, SourceLocation> Context::beginToken() const {
    return { std::string{}, currentLocation };
}

Token Context::extractToken(size_t count, TokenKind kind) {
    return extractToken(count, [&](auto&&) { return kind; });
}

Token Context::extractToken(size_t count, auto tokenKindFactory) {
    auto beginLoc = currentLocation;
    std::string_view ID = text.substr(0, count);
    advance(count);
    return Token(std::string(ID), tokenKindFactory(ID),
                 { beginLoc, currentLocation });
}

char Context::current() const {
    if (text.empty()) {
        return '\0';
    }
    return text.front();
}
