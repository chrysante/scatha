#include "Parser/TokenStream.h"

#include <iostream>

namespace scatha::parser {

TokenStream::TokenStream(utl::vector<Token> tokens):
    tokens(std::move(tokens)) {}

Token const& TokenStream::eat() { return eatImpl(&_index); }

bool TokenStream::advanceTo(std::string_view id) {
    while (true) {
        Token const& next = peek();
        if (next.kind() == TokenKind::Semicolon ||
            next.kind() == TokenKind::EndOfFile)
        {
            eat();
            return false;
        }
        if (next.id() == id) {
            return true;
        }
        eat();
    }
}

Token const& TokenStream::peek() {
    ssize_t i = _index;
    return eatImpl(&i);
}

Token const& TokenStream::current() {
    SC_EXPECT(_index >= 0);
    SC_EXPECT(_index < utl::narrow_cast<ssize_t>(tokens.size()));
    return tokens[utl::narrow_cast<size_t>(_index)];
}

Token const& TokenStream::eatImpl(ssize_t* i) {
    SC_EXPECT(*i >= -1);
    if (*i >= utl::narrow_cast<ssize_t>(tokens.size()) - 1) {
        _index = utl::narrow_cast<ssize_t>(tokens.size());
        return tokens.back();
    }
    return tokens[utl::narrow_cast<size_t>(++*i)];
}

void print(TokenStream const& tokens) { print(tokens, std::cout); }

void print(TokenStream const& tokStr, std::ostream& str) {
    for (auto& tok: tokStr.tokens) {
        str << tok.kind() << " (" << tok.id() << ")\n";
    }
}

} // namespace scatha::parser
