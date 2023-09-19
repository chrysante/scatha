#include "Parser/TokenStream.h"

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
    SC_ASSERT(_index >= 0, "");
    SC_ASSERT(_index < utl::narrow_cast<ssize_t>(tokens.size()), "");
    return tokens[utl::narrow_cast<size_t>(_index)];
}

Token const& TokenStream::eatImpl(ssize_t* i) {
    SC_ASSERT(*i >= -1, "");
    if (*i >= utl::narrow_cast<ssize_t>(tokens.size()) - 1) {
        _index = utl::narrow_cast<ssize_t>(tokens.size());
        return tokens.back();
    }
    //    SC_ASSERT(*i < (ssize_t)tokens.size() - 1, "");
    return tokens[utl::narrow_cast<size_t>(++*i)];
}

} // namespace scatha::parser
