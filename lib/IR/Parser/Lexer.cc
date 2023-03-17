#include "IR/Parser/Lexer.h"

#include <cctype>
#include <array>
#include <string_view>
#include <optional>

#include <range/v3/algorithm.hpp>
#include <utl/utility.hpp>

#include "Basic/Basic.h"
#include "IR/Parser/Token.h"

using namespace scatha;
using namespace ir;

bool isPunctuation(char c) {
    static constexpr std::array punctuations = {
        '(', ')', '{', '}', '=', ':', ','
    };
    return ranges::find(punctuations, c) != ranges::end(punctuations);
};

Expected<Token, InvalidToken> Lexer::next() {
    while (i != end && std::isspace(*i)) {
        inc();
    }
    if (i == end) {
        return Token(std::string_view{}, loc, TokenKind::EndOfFile);
    }
    // Identifier
    if (*i == '@' || *i == '%') {
        SourceLocation const beginSL = loc;
        char const* const first = i;
        while (i != end && !std::isspace(*i) && !isPunctuation(*i)) {
            inc();
        }
        TokenKind const kind = *first == '@' ? TokenKind::GlobalIdentifier : TokenKind::LocalIdentifier;
        return Token(first + 1, i, beginSL, kind);
    }
    // Punctuation
    if (isPunctuation(*i)) {
        SourceLocation const beginSL = loc;
        auto first = i;
        inc();
        return Token(first, i, beginSL, TokenKind::Punctuation);
    }
    // Keyword
    if (std::isalpha(*i)) {
        SourceLocation const beginSL = loc;
        char const* const first = i;
        do {
            inc();
        } while (i != end && std::isalnum(*i));
        std::string_view const id(first, static_cast<size_t>(i - first));
        using namespace std::string_view_literals;
        static constexpr std::array keywords = {
            "function"sv,
            "structure"sv,
            "load"sv,
            "store"sv,
            "return"sv,
            "neg"sv,
            "add"sv,
            "sub"sv,
            "mul"sv,
            "div"sv,
            "rem"sv,
        };
        auto itr = ranges::find(keywords, id);
        if (itr != ranges::end(keywords)) {
            return Token(id, beginSL, TokenKind::Keyword);
        }
        if ((*first == 'i' || *first == 'f') &&
            ranges::all_of(first + 1, i, [](char c) { return std::isdigit(c); }))
        {
            char* strEnd = nullptr;
            long const width = std::strtol(first + 1, &strEnd, 10);
            SC_ASSERT(first + 1 != strEnd, "Failed to pass width");
            auto const kind = *first == 'i' ? TokenKind::IntType :
                                              TokenKind::FloatType;
            return Token(id,
                         beginSL,
                         kind,
                         utl::narrow_cast<unsigned>(width));
        }
        return InvalidToken(beginSL);
    }
    return InvalidToken(loc);
}

void Lexer::inc() {
    ++i;
    ++loc.column;
    if (i != end && *i == '\n') {
        loc.column = 0;
        ++loc.line;
    }
}
