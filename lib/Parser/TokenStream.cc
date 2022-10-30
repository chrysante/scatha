#include "Parser/TokenStream.h"

namespace scatha::parse {

TokenStream::TokenStream(utl::vector<Token> tokens): tokens(std::move(tokens)) {}

Token const& TokenStream::eat() {
    return eatImpl(&index);
}

void TokenStream::advancePastSeparator() { SC_DEBUGFAIL(); }

void TokenStream::advanceUntilStable() {
    SC_ASSERT(index < (ssize_t)tokens.size(), "");
    if (index == (ssize_t)tokens.size() - 1) { return; }
    for (ssize_t numOpenBrace = 0, numOpenParan = 0, numOpenBracket = 0; index < (ssize_t)tokens.size(); ) {
        Token const& next = peek();
        if (next.id == "{") { ++numOpenBrace; }
        if (next.id == "}") { --numOpenBrace; }
        if (next.id == "(") { ++numOpenParan; }
        if (next.id == ")") { --numOpenParan; }
        if (next.id == "[") { ++numOpenBracket; }
        if (next.id == "]") { --numOpenBracket; }
        if (numOpenBrace <= 0 && numOpenParan == 0 && numOpenBracket == 0) {
            /// Here we can potentially find a stable point to continue parsing
            if (next.isSeparator) {
//                if (next.type != TokenType::EndOfFile) { eat(); }
                return;
            }
            if (next.isDeclarator) {
                return;
            }
            if (next.id == "}" && numOpenBrace == -1) {
                return;
            }
        }
        eat();
    }
}

bool TokenStream::advanceTo(std::string_view id) {
    while (true) {
        Token const& next = peek();
        if (next.isSeparator) { eat(); return false; }
        if (next.id == id) { return true; }
        eat();
    }
}

void TokenStream::insert(Token const& token) {
    tokens.insert(tokens.begin() + index, token);
}

Token const& TokenStream::peek() {
    ssize_t i = index;
    return eatImpl(&i);
}

Token const& TokenStream::current() {
    SC_ASSERT(index >= 0, "");
    SC_ASSERT(index < (ssize_t)tokens.size(), "");
    return tokens[(size_t)index];
}

Token const& TokenStream::eatImpl(ssize_t* i) {
    SC_ASSERT(*i >= -1, "");
    if (*i >= (ssize_t)tokens.size() - 1) {
        index = (ssize_t)tokens.size();
        return tokens.back();
    }
//    SC_ASSERT(*i < (ssize_t)tokens.size() - 1, "");
    return tokens[(size_t)++*i];
}

} // namespace scatha::parse
