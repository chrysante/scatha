#include "Parser/TokenStream.h"

namespace scatha::parse {

TokenStream::TokenStream(utl::vector<Token> tokens): tokens(std::move(tokens)) {}

TokenEx const& TokenStream::eat() {
    return eatImpl(&index);
}

TokenEx const& TokenStream::peek() {
    size_t i = index;
    return eatImpl(&i);
}

TokenEx const& TokenStream::current() {
    SC_ASSERT(index > 0, "");
    SC_ASSERT(index < tokens.size() - 1, "");
    return tokens[index - 1];
}

TokenEx const& TokenStream::eatImpl(size_t* i) {
    SC_ASSERT(*i != tokens.size(), "");
    return tokens[(*i)++];
}

} // namespace scatha::parse
