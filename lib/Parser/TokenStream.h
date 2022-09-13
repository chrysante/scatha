#ifndef SCATHA_PARSER_TOKENSTREAM_H_
#define SCATHA_PARSER_TOKENSTREAM_H_

#include <span>

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"
#include "Parser/TokenEx.h"

namespace scatha::parse {
	
	/**
	 A stream  of tokens to be used by the parser.
	 Expands a range of Token's into TokenEx's.
	 
	 # Notes: #
	 1. Expects the last token in the stream to be of type EndOfFile.
	 */
	class TokenStream {
	public:
		/**
		 Constructs an empty TokenStream.
		 */
		TokenStream() = default;
		/**
		 Constructs a TokenStream from the given Range of Token's.
		 
		 - parameter tokens: Range of tokens.
		 
		 # Notes: #
		 1. Expands the Token's into TokenEx's. Copies the Range into a buffer. The given range can be freed after constructing the TokenStream.
		 */
		explicit TokenStream(std::span<Token const> tokens);
		
		/**
		 Extract one token from the stream.
		 
		 - parameter ignoreEOL: If true, end of line tokens will be skipped.
		 - returns: A reference to the next token in the stream.
		 
		 # Notes: #
		 1.  Always returns a valid reference, however it must not be called after end of file has been returned.
		 2. When called in a loop, the stream will be iterated.
		 */
		TokenEx const& eat(bool ignoreEOL = true);
		/**
		 Look ahead one token into the stream.
		 
		 - parameter ignoreEOL: If true, end of line tokens will be skipped.
		 - returns: A reference to the next token in the stream.
		 
		 # Notes: #
		 1.  Always returns a valid reference, however it must not be called after end of file has been returned by \p eatToken.
		 2. Always returns the same token when called in a loop.
		 */
		TokenEx const& peek(bool ignoreEOL = true);
		
	private:
		TokenEx const& eatImpl(bool ignoreEOL, size_t*);
		
	private:
		utl::vector<TokenEx> tokens;
		size_t index = 0;
	};
	
}

#endif // SCATHA_PARSER_TOKENSTREAM_H_

