#include "Lexer/LexerError.h"

#include <sstream>

std::string scatha::internal::makeWhatArg(std::string const& what, SourceLocation const& sc) {
	std::stringstream sstr;
	sstr << what << " at line " << sc.line << " column " << sc.column << "\n";
	return sstr.str();
}

namespace scatha::lex {
	
	LexerError::LexerError(SourceLocation sc, std::string const& what):
		std::runtime_error(what),
		_what(internal::makeWhatArg(what, sc)),
		sourceLocation(sc)
	{
		
	}
	
	const char* LexerError::what() const noexcept {
		return _what.data();
	}
	
}
