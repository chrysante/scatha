#include "Lexer/LexerUtil.h"

#include <string_view>

namespace scatha::lex {

static bool isAnyOf(char c, std::string_view data) {
    for (auto i: data) {
        if (c == i) {
            return true;
        }
    }
    return false;
}

SCATHA(PURE) bool isPunctuation(char c) {
    return isAnyOf(c, "{}()[],;:");
}

SCATHA(PURE) bool isOperatorLetter(char c) {
    return isAnyOf(c, "+-*/%&|^.=><?!~");
}

SCATHA(PURE) bool isOperator(std::string_view id) {
    // clang-format off
    std::string_view constexpr operators[]{
        "+",  "-",  "*",  "/",  "%",  "&",  "|",   "^",   "!",  "~",
        "++", "--", "<<", ">>", "&&", "||",
        "=",  "+=", "-=", "*=", "/=", "%=", "<<=", ">>=", "&=", "|=", "^=",
        "==", "!=", "<",  "<=", ">",  ">=",
        ".",  "->", "?"
    };
    // clang-format on
    for (auto o: operators) {
        if (id == o) {
            return true;
        }
    }
    return false;
}

} // namespace scatha::lex
