#ifndef SCATHA_PARSER_TOKENSTREAM_H_
#define SCATHA_PARSER_TOKENSTREAM_H_

#include <utl/vector.hpp>

#include "Basic/Basic.h"
#include "Common/Token.h"

namespace scatha::parse {


/// A stream  of tokens to be used by the parser.
/// Expands a range of Token's into TokenEx's.
///
/// \details
/// 1. Expects the last token in the stream to be of type EndOfFile.
class SCATHA(API) TokenStream {
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
    /// 1.  Always returns a valid reference, however it must not be called after
    /// end of file has been returned.
    /// 2. When called in a loop, the stream will be iterated.
    Token const& eat();
    
    
    /// Advance the token stream past the next separator.
    /// After calling this function a call to \p current() will return a separator token or the next closing brace '}'.
    [[deprecated]] void advancePastSeparator();
    
    /// Call on syntax errors.  Advances the token stream to a position where we can continue parsing.
    /// Advances past matching pairs of braces, parantheses and brackets.
    /// After calling this function a call to \p peek() will return  a separator token or a declarator or a closing brace '}'.
    void advanceUntilStable();
    
    /// Advance the token stream to the next token with \code token.id == id \endcode or past the next separator.
    /// After calling this function a call to \p current() will return the next token with specified id or a  separator token.
    ///
    /// \returns \p true iff \p id was found before a separator.
    bool advanceTo(std::string_view id);
    
    void insert(Token const& token);
    
    /// Look ahead one token into the stream.
    ///
    /// \param ignoreEOL If true, end of line tokens will be skipped.
    /// \returns A reference to the next token in the stream.
    ///
    /// \details
    /// 1.  Always returns a valid reference, however it must not be called after
    /// end of file has been returned by \p eat.
    /// 2. Always returns the same token when called in a loop.
    Token const& peek();

    
    /// \returns A reference to the current token in the stream aka the token
    /// returned by the last call to \p eat().
    ///
    /// \details
    /// 1.  Always returns a valid reference.
    /// 2. Always returns the same token when called in a loop.
    Token const& current();

private:
    Token const& eatImpl(ssize_t*);

private:
    utl::vector<Token> tokens;
    ssize_t index = -1;
};

} // namespace scatha::parse

#endif // SCATHA_PARSER_TOKENSTREAM_H_
