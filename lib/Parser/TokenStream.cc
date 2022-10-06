#include "Parser/TokenStream.h"

namespace scatha::parse {

TokenStream::TokenStream(std::span<Token const> tokens) {
    this->tokens.reserve(tokens.size());
    for (auto const &t : tokens) {
#warning Change this
        this->tokens.push_back(t);
    }
}

TokenEx const &TokenStream::eat(bool ignoreEOL) { return eatImpl(ignoreEOL, &index); }

TokenEx const &TokenStream::peek(bool ignoreEOL) {
    size_t i = index;
    return eatImpl(ignoreEOL, &i);
}

TokenEx const &TokenStream::current() {
    SC_ASSERT(index > 0, "");
    SC_ASSERT(index < tokens.size() - 1, "");
    return tokens[index - 1];
}

TokenEx const &TokenStream::eatImpl(bool ignoreEOL, size_t *i) {
    if (ignoreEOL) {
        while (*i < tokens.size() && tokens[*i].isEOL) {
            ++*i;
        }
    }
    SC_ASSERT(*i != tokens.size(), "");
    return tokens[(*i)++];
}

} // namespace scatha::parse
