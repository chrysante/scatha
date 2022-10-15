#ifndef SCATHA_PARSER_TOKENSTREAM_H_
#define SCATHA_PARSER_TOKENSTREAM_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"

namespace scatha::parse {

/**
 A stream  of tokens to be used by the parser.
 Expands a range of Token's into TokenEx's.

 # Notes: #
 1. Expects the last token in the stream to be of type EndOfFile.
 */
class SCATHA(API) TokenStream {
public:
    /**
     Constructs an empty TokenStream.
     */
    TokenStream() = default;
    /**
     Constructs a TokenStream from the given Range of Token's.

     - parameter tokens: Vector of tokens.

     */
    explicit TokenStream(utl::vector<Token> tokens);

    /**
     Extract one token from the stream.

     - parameter ignoreEOL: If true, end of line tokens will be skipped.
     - returns: A reference to the next token in the stream.

     # Notes: #
     1.  Always returns a valid reference, however it must not be called after
     end of file has been returned.
     2. When called in a loop, the stream will be iterated.
     */
    Token const& eat();
    /**
     Look ahead one token into the stream.

     - parameter ignoreEOL: If true, end of line tokens will be skipped.
     - returns: A reference to the next token in the stream.

     # Notes: #
     1.  Always returns a valid reference, however it must not be called after
     end of file has been returned by \p eatToken.
     2. Always returns the same token when called in a loop.
     */
    Token const& peek();

    /**
     - returns: A reference to the current token in the stream aka the token
     returned by the last call to \p eat().

     # Notes: #
     1.  Always returns a valid reference.
     2. Always returns the same token when called in a loop.
     */
    Token const& current();

private:
    Token const& eatImpl(size_t*);

private:
    utl::vector<Token> tokens;
    size_t index = 0;
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_TOKENSTREAM_H_
