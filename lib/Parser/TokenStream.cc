#include "TokenStream.h"

namespace scatha::parse {
	
	TokenStream::TokenStream(std::span<Token const> tokens) {
		this->tokens.reserve(tokens.size());
		for (auto const& t: tokens) {
			this->tokens.push_back(expand(t));
		}
	}
	
	TokenEx const& TokenStream::eat(bool ignoreEOL) {
		return eatImpl(ignoreEOL, &index);
	}
	
	TokenEx const& TokenStream::peek(bool ignoreEOL) {
		size_t i = index;
		return eatImpl(ignoreEOL, &i);
	}
	
	TokenEx const& TokenStream::current() {
		assert(index > 0);
		assert(index < tokens.size() - 1);
		return tokens[index - 1];
	}
	
	TokenEx const& TokenStream::eatImpl(bool ignoreEOL, size_t* i) {
		if (ignoreEOL) {
			while (*i < tokens.size() && tokens[*i].isEOL) { ++*i; }
		}
		assert(*i != tokens.size());
		return tokens[(*i)++];
	}
	
}
