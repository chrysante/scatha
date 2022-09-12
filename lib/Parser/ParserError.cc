#include "Parser/ParserError.h"

#include "Lexer/LexerError.h"

namespace scatha::parse {
	
	ParserError::ParserError(TokenEx const& token, std::string const& what):
		std::runtime_error(what),
		_what(internal::makeWhatArg(what, token.sourceLocation)),
		token(token)
	{
		
	}
	
	const char* ParserError::what() const noexcept {
		return _what.data();
	}
	
	
}
