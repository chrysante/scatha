#include "Parser/ParserError.h"

#include <sstream>

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
	
	void expectIdentifier(TokenEx const& token, [[maybe_unused]] std::string_view message) {
		if (!token.isIdentifier) {
			throw ParserError(token, "Expected Identifier");
		}
	}
	
	void expectKeyword(TokenEx const& token, [[maybe_unused]] std::string_view message) {
		if (!token.isKeyword) {
			throw ParserError(token, "Expected Keyword");
		}
	}
	
	void expectDeclarator(TokenEx const& token, [[maybe_unused]] std::string_view message) {
		if (!token.isKeyword || token.keywordCategory != KeywordCategory::Declarators) {
			throw ParserError(token, "Expected Declarator");
		}
	}
	
	void expectSeparator(TokenEx const& token, [[maybe_unused]] std::string_view message) {
		if (!token.isSeparator) {
			throw ParserError(token, "Unqualified ID. Expected ';'");
		}
	}
	
	void expectID(TokenEx const& token, std::string_view id, std::string_view message) {
		if (token.id != id) {
			std::stringstream sstr;
			sstr << "Unqualified ID. Expected '" << id << "'";
			if (!message.empty()) {
				sstr << "\n" << message;
			}
			throw ParserError(token, sstr.str());
		}
	}
	
}
