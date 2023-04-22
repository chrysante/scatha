#ifndef SCATHA_PARSER_TOKENSTREAM_H_
#define SCATHA_PARSER_TOKENSTREAM_H_

#include <utl/vector.hpp>

#include "AST/Token.h"
#include "Common/Base.h"

namespace scatha::parse {

/// A stream  of tokens to be used by the parser.
/// Expands a range of Token's into TokenEx's.
///
/// \details
/// 1. Expects the last token in the stream to be of type EndOfFile.
class SCATHA_API TokenStream {
public:
    /// Constructs an empty TokenStream.
    TokenStream() = default;

    /// Constructs a TokenStream from the given Range of Token's.
    ///
    /// \param tokens Vector of tokens.
    explicit TokenStream(utl::vector<Token> tokens);

    /// Extract one token from the stream.
    ///
    /// \returns A reference to the next token in the stream.
    ///
    /// \details
    /// 1.  Always returns a valid reference, however it must not be called
    /// after end of file has been returned.
    /// 2. When called in a loop, the stream will be iterated.
    Token const& eat();

    /// Advance the token stream to the next token with `token.id == id` or past
    /// the next separator. After calling this function a call to `current()`
    /// will return the next token with specified id or a  separator token.
    ///
    /// \returns `true` iff \p id was found before a separator.
    bool advanceTo(std::string_view id);

    /// Look ahead one token into the stream.
    ///
    /// \param ignoreEOL If true, end of line tokens will be skipped.
    /// \returns A reference to the next token in the stream.
    ///
    /// \details
    /// 1.  Always returns a valid reference, however it must not be called
    /// after end of file has been returned by `eat()`.
    /// 2. Always returns the same token when called in a loop.
    Token const& peek();

    /// \returns A reference to the current token in the stream aka the token
    /// returned by the last call to `eat()`.
    ///
    /// \details
    /// 1.  Always returns a valid reference.
    /// 2. Always returns the same token when called in a loop.
    Token const& current();

    ssize_t index() const { return _index; }

    ssize_t size() const { return utl::narrow_cast<ssize_t>(tokens.size()); }

private:
    Token const& eatImpl(ssize_t*);

private:
    utl::vector<Token> tokens;
    ssize_t _index = -1;
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_TOKENSTREAM_H_
